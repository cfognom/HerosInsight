// Reordering these causes compile errors...
// clang-format off
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <assert.h>
// clang-format on

#include <fstream>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <zip.h>

#include "version.h"

std::string_view mod_dll_name = "HerosInsight.dll";
std::string_view version_file_name = "HerosInsight.version";
std::string_view github_agent_name = "Hero's Insight Updater";
bool check_for_updates = true;
std::string launcher_exe_name;
std::filesystem::path exePath;
std::filesystem::path exeDirPath;
DWORD gwProcessId = 0;
HWND gwHwnd = nullptr;

std::wstring StrToWStr(const std::string &str)
{
    return std::wstring(str.begin(), str.end());
}
std::wstring StrToWStr(std::string_view str)
{
    return std::wstring(str.begin(), str.end());
}
std::wstring StrToWStr(const char *str)
{
    return std::wstring(str, str + strlen(str));
}

void Log(std::wstring_view msg)
{
    std::wcout << msg << std::endl;
}

std::wstring IndentMultiline(std::wstring_view text, std::wstring_view indent)
{
    std::wostringstream out;
    bool first = true;

    for (size_t pos = 0, next; pos < text.size(); pos = next + 1)
    {
        next = text.find(L'\n', pos);
        if (next == std::string_view::npos)
            next = text.size();

        if (!first)
            out << L'\n';
        first = false;

        out << indent << text.substr(pos, next - pos);
    }

    return out.str();
}

HWND FindWindowByPID(DWORD pid)
{
    struct Param
    {
        DWORD pid;
        HWND out_hwnd;
    };
    Param param = {pid, nullptr};

    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL
        {
            auto param = (Param *)lParam;
            DWORD windowPid;
            GetWindowThreadProcessId(hwnd, &windowPid);

            if (windowPid == param->pid)
            {
                // Likely a "main" window
                param->out_hwnd = hwnd;
                return FALSE; // stop enumeration
            }

            return TRUE;
        },
        (LPARAM)&param
    );

    return param.out_hwnd;
}

struct ScopedFile
{
    std::filesystem::path path;
    std::ofstream file;
    bool committed = false;

    explicit ScopedFile(const std::filesystem::path &p)
        : path(p), file(p, std::ios::binary)
    {
        if (!file)
            throw std::runtime_error("Failed to open file");
    }

    void Commit()
    {
        committed = true;
    }

    ~ScopedFile()
    {
        file.close();
        if (!committed)
            std::filesystem::remove(path);
    }
};

struct CurlEasy
{
    CURL *curl = nullptr;
    CurlEasy() : curl(curl_easy_init())
    {
        if (!curl)
            throw std::runtime_error("Failed to init curl");
    }
    ~CurlEasy()
    {
        if (curl)
            curl_easy_cleanup(curl);
    }

    static size_t CurlWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        size_t total = size * nmemb;
        static_cast<std::string *>(userp)->append(static_cast<char *>(contents), total);
        return total;
    }

    std::string HttpGet(std::string_view url)
    {
        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // GitHub requires a User-Agent
        curl_easy_setopt(curl, CURLOPT_USERAGENT, github_agent_name);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            throw std::runtime_error(std::format("CURL failed: {}", curl_easy_strerror(res)));

        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

        if (httpCode != 200)
            throw std::runtime_error(std::format("Http request failed with code {}\nresponse:\n{}", httpCode, response));

        return response;
    }

    static size_t CurlWriteToFile(void *ptr, size_t size, size_t nmemb, void *userdata)
    {
        auto out = static_cast<ScopedFile *>(userdata);
        out->file.write(static_cast<char *>(ptr), size * nmemb);
        return size * nmemb;
    }

    void DownloadFile(std::string_view url, std::filesystem::path &dst)
    {
        ScopedFile file(dst);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteToFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, github_agent_name);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            throw std::runtime_error(std::string("Failed to download file: ") + curl_easy_strerror(res));

        file.Commit();
    }
};

struct Version
{
    int major = 0;
    int minor = 0;
    int patch = 0;

    // Comparison operators
    bool operator==(const Version &other) const
    {
        return std::tie(major, minor, patch) == std::tie(other.major, other.minor, other.patch);
    }
    bool operator<(const Version &other) const
    {
        return std::tie(major, minor, patch) < std::tie(other.major, other.minor, other.patch);
    }

    std::wstring ToWString() const
    {
        return std::format(L"{}.{}.{}", major, minor, patch);
    }

    static Version FromString(std::string_view str)
    {
        if (!str.empty() && (str[0] == 'v' || str[0] == 'V'))
            str = str.substr(1);

        Version v;
        char dot1, dot2;
        std::istringstream ss(str.data());
        ss >> v.major >> dot1 >> v.minor >> dot2 >> v.patch;
        if (ss.fail() || dot1 != '.' || dot2 != '.')
        {
            throw std::runtime_error(std::format("Invalid version format: {}", str));
        }
        return v;
    }
};

struct GitHubRelease
{
    Version version;
    std::string tag;
    std::string name;
    std::string body;
    std::string download_url;

    GitHubRelease(std::string_view release_json)
    {
        auto j = nlohmann::json::parse(release_json);

        this->tag = j.at("tag_name").get<std::string>();
        this->name = j.at("name").get<std::string>();
        this->body = j.at("body").get<std::string>();
        this->version = Version::FromString(this->tag);

        // Pick first asset (you can improve this later)
        if (!j["assets"].empty())
        {
            this->download_url = j["assets"][0]["browser_download_url"].get<std::string>();
        }
    }
};

template <typename T, typename... Args>
std::optional<T> TryConstruct(Args &&...args)
{
    try
    {
        return T(std::forward<Args>(args)...);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<GitHubRelease> TryGetLatestRelease()
{
    try
    {
        std::string jsonText = CurlEasy().HttpGet(
            "https://api.github.com/repos/cfognom/HerosInsight/releases/latest"
        );

        return GitHubRelease(jsonText);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

struct LocalInstallation
{
    Version version;
    std::filesystem::path mod_dll;

    LocalInstallation()
    {
        auto data_path = exeDirPath / "data";
        if (!std::filesystem::exists(data_path))
            throw std::runtime_error("'data' directory does not exist");

        this->version = Version(HEROSINSIGHT_VERSION_MAJOR, HEROSINSIGHT_VERSION_MINOR, HEROSINSIGHT_VERSION_PATCH);

        this->mod_dll = data_path / mod_dll_name;
        if (!std::filesystem::exists(this->mod_dll))
            throw std::runtime_error("Mod DLL does not exist");
    }
};

std::optional<LocalInstallation> TryGetLocalInstallation()
{
    try
    {
        return LocalInstallation();
    }
    catch (const std::exception &e)
    {
        return std::nullopt;
    }
}

void ExtractZip(const std::filesystem::path &zipPath, const std::filesystem::path &dstPath)
{
    int err = 0;
    zip *z = zip_open(zipPath.string().c_str(), 0, &err);
    if (!z)
    {
        throw std::runtime_error(std::format("Failed to open zip file: {}", zipPath.string()));
    }

    zip_int64_t num_entries = zip_get_num_entries(z, 0);
    for (zip_uint64_t i = 0; i < num_entries; ++i)
    {
        const char *name = zip_get_name(z, i, 0);
        if (!name) continue;

        zip_file *zf = zip_fopen_index(z, i, 0);
        if (!zf) continue;

        std::filesystem::path outFile = dstPath / name;
        std::filesystem::create_directories(outFile.parent_path());

        std::ofstream ofs(outFile, std::ios::binary);
        char buf[4096];
        zip_int64_t n;
        while ((n = zip_fread(zf, buf, sizeof(buf))) > 0)
        {
            ofs.write(buf, n);
        }

        zip_fclose(zf);
    }

    zip_close(z);
}

void RunLauncher(wchar_t *cmdLine = nullptr)
{
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    if (!CreateProcessW(
            exePath.c_str(), // Application name
            cmdLine,         // Command line arguments (can be nullptr)
            nullptr,         // Process handle not inheritable
            nullptr,         // Thread handle not inheritable
            FALSE,           // Inherit handles
            0,               // No special creation flags
            nullptr,         // Use parent's environment
            nullptr,         // Use parent's current directory
            &si,
            &pi
        ))
    {
        DWORD err = GetLastError();
        throw std::runtime_error("Failed to launch exe. Error code: " + std::to_string(err));
    }

    // Close handles to avoid leaks
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

void InstallAndRelaunch(const std::filesystem::path &install_path)
{
    auto package_path = install_path / "new_version.zip";
    ExtractZip(package_path, install_path);
    std::filesystem::remove(package_path);
    Log(L"Install complete.");

    // Relaunch
    auto cmdLine = std::format(L"--prev_pid {} --no_update", GetCurrentProcessId());
    Log(L"Relaunching...");
    RunLauncher(cmdLine.data());

    // Exit current instance
    ExitProcess(0);
}

void DownloadAndLaunchInstaller(GitHubRelease &release)
{
    Log(L"Downloading release...");
    auto download_path = exeDirPath / "new_version.zip";
    CurlEasy().DownloadFile(release.download_url, download_path);

    // Launch installer/updater
    auto cmdLine = std::format(L"--prev_pid {} --install \"{}\"", GetCurrentProcessId(), exeDirPath.c_str());
    Log(L"Launching installer...");
    RunLauncher(cmdLine.data());

    // Exit current instance so updater can overwrite files
    ExitProcess(0);
}

std::optional<LocalInstallation> TryGetOrCreateLocalInstallation()
{
    auto local_installation = TryGetLocalInstallation();

    if (check_for_updates)
    {
        Log(L"Checking for updates...");
        auto latest_release = TryGetLatestRelease();

        bool should_download = false;
        if (latest_release)
        {
            Log(L"Found latest release.");
            if (!local_installation)
            {
                Log(L"No local installation found.");
                should_download = true;
            }
            else if (local_installation->version < latest_release->version)
            {
                Log(L"Latest release is newer than local installation.");
                auto cur_version_str = L"" HEROSINSIGHT_VERSION_STRING;
                auto new_version_str = latest_release->version.ToWString();
                auto wbody = StrToWStr(latest_release->body);
                // wbody = IndentMultiline(wbody, L"    ");
                auto message = std::format(
                    L"There is a new version of Hero's Insight available: {}"
                    "\n(Current version: {})"
                    "\n\nO>======<Release notes>======<O"
                    "\n\n{}"
                    "\n\nO>=======================<O"
                    "\n\nPress OK to download and install.",
                    new_version_str,
                    cur_version_str,
                    wbody
                );
                auto name = std::format(
                    L"New version available: {}",
                    new_version_str
                );
                auto result = MessageBoxW(
                    gwHwnd,
                    message.c_str(),
                    name.c_str(),
                    MB_OKCANCEL | MB_ICONINFORMATION
                );

                if (result == IDOK)
                {
                    Log(L"User accepted update. Downloading and installing...");
                    should_download = true;
                }
                else
                {
                    Log(L"User declined update. Terminating.");
                    ExitProcess(0);
                }
            }
            else
            {
                Log(L"Local installation is up to date.");
            }
        }

        if (should_download)
        {
            try
            {
                DownloadAndLaunchInstaller(latest_release.value());
            }
            catch (const std::exception &e)
            {
                std::wcerr << "Error downloading and launching installer: " << e.what() << std::endl;
            }
        }
    }

    return local_installation;
}

DWORD GetProcessIdByName(const char *processName)
{
    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        std::wcerr << "Failed to create snapshot. Error: " << GetLastError() << std::endl;
        return 0;
    }

    if (Process32First(snapshot, &processEntry))
    {
        do
        {
            if (strcmp(processEntry.szExeFile, processName) == 0)
            {
                CloseHandle(snapshot);
                return processEntry.th32ProcessID;
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return 0;
}

bool IsDllLoaded(DWORD processID, const std::filesystem::path &dllPath)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (!hProcess)
    {
        std::wcerr << "Failed to open process. Error: " << GetLastError() << "\n";
        return false;
    }

    HMODULE hModules[1024];
    DWORD cbNeeded;

    auto dllPathStr = dllPath.c_str();

    if (EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded))
    {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); ++i)
        {
            wchar_t moduleName[MAX_PATH];
            if (GetModuleFileNameExW(hProcess, hModules[i], moduleName, std::size(moduleName)))
            {
                if (wcscmp(moduleName, dllPathStr) == 0)
                {
                    CloseHandle(hProcess);
                    return true;
                }
            }
        }
    }
    else
    {
        std::wcerr << "Failed to enumerate process modules. Error: " << GetLastError() << "\n";
    }

    CloseHandle(hProcess);
    return false;
}

bool TryInjectDLL(DWORD processId, const std::filesystem::path &dllPath)
{
    assert(processId);

    bool success = false;
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (process == NULL)
    {
        auto last_error = GetLastError();
        std::wcerr << "Failed to open target process. Error: " << last_error << std::endl;
        if (last_error == 5)
        {
            std::wcerr << "(Maybe try adding HerosInsight.dll and Launch_HerosInsight.exe to your antivirus whitelist/exlusion list)" << std::endl;
        }
    }
    else
    {
        auto dllPathStr = dllPath.wstring();
        assert(dllPathStr.size() > mod_dll_name.size()); // We want full path for security reasons
        size_t allocSize = (dllPathStr.size() + 1) * sizeof(wchar_t);
        LPVOID allocPtr = VirtualAllocEx(process, NULL, allocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (allocPtr == NULL)
        {
            std::wcerr << "Failed to allocate memory in target process." << std::endl;
        }
        else
        {
            if (!WriteProcessMemory(process, allocPtr, dllPathStr.data(), allocSize, NULL))
            {
                std::wcerr << "Failed to write DLL path to target process memory." << std::endl;
            }
            else
            {
                HANDLE remoteThread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryW, allocPtr, 0, NULL);
                if (remoteThread == NULL)
                {
                    std::wcerr << "Failed to create remote thread in target process." << std::endl;
                }
                else
                {
                    WaitForSingleObject(remoteThread, INFINITE);
                    success = true;

                    CloseHandle(remoteThread);
                }
            }
            VirtualFreeEx(process, allocPtr, 0, MEM_RELEASE);
        }
        CloseHandle(process);
    }
    return success;
}

int RunNormalApp()
{
    auto installation = TryGetOrCreateLocalInstallation();
    if (!installation)
    {
        auto message = std::format(L"Failed to get or create local installation.");
        MessageBoxW(gwHwnd, message.c_str(), L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (gwProcessId == 0)
    {
        MessageBoxW(gwHwnd, L"Failed to find target process.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (IsDllLoaded(gwProcessId, installation->mod_dll))
    {
        MessageBoxW(gwHwnd, L"Hero's Insight is already running.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (!TryInjectDLL(gwProcessId, installation->mod_dll))
    {
        MessageBoxW(gwHwnd, L"Failed to inject DLL, check output file for more details.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    std::cout << "DLL injected successfully." << std::endl;

    return 0;
}

void WaitForProcessExit(DWORD pid)
{
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!hProcess)
        return; // Process may already be gone

    WaitForSingleObject(hProcess, INFINITE);
    CloseHandle(hProcess);
}

void InitPaths()
{
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    exePath = std::filesystem::path(buffer);
    launcher_exe_name = exePath.filename().string();
    exeDirPath = exePath.parent_path();
}

struct Args
{
    int count = 0;
    LPWSTR *ptr;

    Args()
    {
        ptr = CommandLineToArgvW(GetCommandLineW(), &count);
    }
    ~Args()
    {
        LocalFree(ptr);
    }

    void Log()
    {
        std::wcout << L"Running executable: ";
        for (int i = 0; i < count; i++)
        {
            std::wcout << ptr[i];
        }
        std::wcout << std::endl;
    }

    bool Has(std::wstring_view name) const { return find(name) >= 0; }

    bool GetInt(std::wstring_view name, int &out) const
    {
        int i = find(name);
        return i >= 0 &&
               i + 1 < count &&
               parse_int(ptr[i + 1], out);
    }

    bool GetPath(std::wstring_view name, std::filesystem::path &out) const
    {
        int i = find(name);
        if (i < 0 || i + 1 >= count)
            return false;

        out = ptr[i + 1];
        return true;
    }

private:
    int find(std::wstring_view name) const
    {
        for (int i = 0; i < count; ++i)
            if (wcscmp(ptr[i], name.data()) == 0)
                return i;
        return -1;
    }

    static bool parse_int(std::wstring_view s, int &out)
    {
        wchar_t *end = nullptr;
        long v = wcstol(s.data(), &end, 10);

        if (end == s || *end != L'\0')
            return false;

        out = static_cast<int>(v);
        return true;
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    InitPaths();

    // Convert full command line to argv (Unicode-safe)
    Args args{};
    if (args.ptr == nullptr)
        return -1;

    int flags = std::ios::out;
    int prev_pid;
    if (args.GetInt(L"--prev_pid", prev_pid))
    {
        // Wait for previous process to exit
        WaitForProcessExit(prev_pid);
        flags |= std::ios::app;
    }

    // Redirect std::cout and std::wcerr to a file
    auto logPath = exePath.replace_extension("log");
    std::wofstream logFile(logPath, flags);
    std::wcout.rdbuf(logFile.rdbuf());
    std::wcerr.rdbuf(logFile.rdbuf());

    gwProcessId = GetProcessIdByName("Gw.exe");
    gwHwnd = FindWindowByPID(gwProcessId);

    args.Log();

    int result = 0;

    std::filesystem::path installDir;
    if (args.GetPath(L"--install", installDir))
    {
        InstallAndRelaunch(installDir);
    }
    else
    {
        if (args.Has(L"--no_update"))
        {
            check_for_updates = false;
        }
        result = RunNormalApp();
    }

    return result;
}
