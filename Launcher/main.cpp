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

#define MOD_NAME "Hero's Insight"
#define MOD_MACHINE_NAME "HerosInsight"
#define MOD_DLL_NAME MOD_MACHINE_NAME ".dll"
#define MOD_EXE_NAME "Launch_" MOD_MACHINE_NAME ".exe"

constexpr std::string_view mod_dll_name = MOD_DLL_NAME;
constexpr std::string_view github_agent_name = MOD_NAME " Updater";
constexpr std::string_view installerArchiveName = MOD_MACHINE_NAME "_Installer.zip";
constexpr std::string_view installerName = MOD_MACHINE_NAME "_Installer.exe";
bool check_for_updates = true;
std::filesystem::path curExePath;
std::filesystem::path rootDirPath;
std::filesystem::path installerArchivePath;
std::filesystem::path installerExePath;
std::filesystem::path logFilePath;
std::string curExeName;
DWORD gwProcessId = 0;
HWND gwHwnd = nullptr;

struct StaticInitailizer
{
    StaticInitailizer()
    {
        wchar_t buffer[MAX_PATH];
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        curExePath = std::filesystem::path(buffer);
        rootDirPath = curExePath.parent_path();
        curExeName = curExePath.filename().string();
        installerArchivePath = rootDirPath / installerArchiveName;
        installerExePath = rootDirPath / installerName;
        logFilePath = rootDirPath / MOD_EXE_NAME, logFilePath.replace_extension("log");
    }
};
StaticInitailizer dummy;

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

    bool IsEmpty() { return file.tellp() == 0; }

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

    void DownloadFile(std::string_view url, const std::filesystem::path &dst)
    {
        ScopedFile file(dst);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // REQUIRED for GitHub
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteToFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, github_agent_name);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            throw std::runtime_error(std::string("Failed to download file: ") + curl_easy_strerror(res));
        if (!std::filesystem::exists(dst))
            throw std::runtime_error("Failed to download file: File does not exist");

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
        auto data_path = rootDirPath / "data";
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

struct ZipRAII
{
    std::filesystem::path path;
    zip *zip;
    ZipRAII(const std::filesystem::path &p) : path(p)
    {
        int err = 0;
        zip = zip_open(p.string().c_str(), 0, nullptr);
        if (!zip || err)
            throw std::runtime_error(std::format("Failed to open zip file: {}", p.string()));
    }
    ~ZipRAII() { zip_close(zip); }
};

struct ZipFileRAII
{
    zip_file *file;
    ZipFileRAII(zip *z, zip_uint64_t index) : file(zip_fopen_index(z, index, 0)) {}
    ~ZipFileRAII() { zip_fclose(file); }
};

std::filesystem::path InstallFromArchive(const std::filesystem::path &zipPath, const std::filesystem::path &dstPath)
{
    Log(std::format(L"Installing from archive {} to {}", zipPath.wstring(), dstPath.wstring()));
    ZipRAII zip(zipPath);
    auto z = zip.zip;

    zip_int64_t num_entries = zip_get_num_entries(z, 0);

    std::string topDir;             // detected top-level dir in zip
    std::filesystem::path foundExe; // path to exe we want to return

    Log(std::format(L"Found {} entries in archive.", num_entries));
    for (zip_uint64_t i = 0; i < num_entries; ++i)
    {
        const char *name = zip_get_name(z, i, 0);
        Log(std::format(L"Entry {}: {}", i, StrToWStr(name)));
        if (!name)
            continue;

        std::string entryName = name;

        // Detect top-level directory (first component before '/')
        if (topDir.empty())
        {
            auto pos = entryName.find('/');
            if (pos != std::string::npos)
            {
                topDir = entryName.substr(0, pos + 1);
                Log(std::format(L"Detecting archive top-level directory: {}", StrToWStr(topDir)));
            }
        }

        if (topDir.empty())
            throw std::runtime_error("Top-level directory not found");

        if (!entryName.starts_with(topDir))
            throw std::runtime_error(std::format("Entry {} is not in top-level directory", entryName));

        // Remove top-level folder from path
        std::string relativePath = entryName.substr(topDir.size());

        if (relativePath.empty())
            continue;

        std::filesystem::path outFile = dstPath / relativePath;
        std::filesystem::create_directories(outFile.parent_path());

        ZipFileRAII zf_raii(z, i);
        zip_file *zf = zf_raii.file;
        if (!zf)
            continue;

        std::ofstream ofs(outFile, std::ios::binary | std::ios::out | std::ios::trunc);
        char buf[4096];
        zip_int64_t n;
        while ((n = zip_fread(zf, buf, sizeof(buf))) > 0)
            ofs.write(buf, n);

        // Detect the exe
        if (foundExe.empty() && outFile.extension() == ".exe")
            foundExe = outFile;
    }

    if (foundExe.empty())
        throw std::runtime_error("No exe found inside zip");

    return foundExe;
}

void RunExe(const std::filesystem::path &appPath, std::wstring cmdLine)
{
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);

    auto appPathStr = appPath.wstring();
    Log(std::format(L"Running exe {} with command line {}", appPathStr, cmdLine));
    cmdLine = std::format(L"\"{}\" {}", appPathStr, cmdLine);
    if (!CreateProcessW(
            appPathStr.c_str(), // Application name
            cmdLine.data(),     // Command line arguments (can be nullptr)
            nullptr,            // Process handle not inheritable
            nullptr,            // Thread handle not inheritable
            FALSE,              // Inherit handles
            0,                  // No special creation flags
            nullptr,            // Use parent's environment
            nullptr,            // Use parent's current directory
            &si,
            &pi
        ))
    {
        DWORD err = GetLastError();
        throw std::runtime_error("Failed to run exe. Error code: " + std::to_string(err));
    }

    // Close handles to avoid leaks
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

enum struct UpdateFailedChoice
{
    UseOld,
    Cancel,
};

UpdateFailedChoice UpdateFailedPromt(const std::exception &e)
{
    std::wcerr << "Error updating: " << e.what() << std::endl;

    auto msg = std::format(
        L"Update failed."
        "\n\n"
        "Reason: {}"
        "\n\n"
        "Do you want to attempt to use the old version instead?",
        StrToWStr(e.what())
    );

    auto result = MessageBoxW(gwHwnd, msg.c_str(), L"Update Failed", MB_ICONERROR | MB_OKCANCEL);

    return result == IDOK ? UpdateFailedChoice::UseOld : UpdateFailedChoice::Cancel;
}

[[noreturn]] void InstallAndLaunch(const std::filesystem::path &oldExePath)
{
    std::wstring cmdLine = std::format(
        L"--prev_pid {} --delete \"{}\"",
        GetCurrentProcessId(),
        std::filesystem::relative(installerExePath, rootDirPath).wstring()
    );
    try
    {
        if (!std::filesystem::exists(installerArchivePath))
            throw std::runtime_error("Installation archive does not exist.");

        struct ArchiveGuard
        {
            std::filesystem::path installerArchivePath;
            ~ArchiveGuard()
            {
                std::filesystem::remove(installerArchivePath);
            }
        } archiveGuard{installerArchivePath};

        auto newExePath = InstallFromArchive(installerArchivePath, rootDirPath);
        Log(L"Install complete.");

        if (newExePath != oldExePath)
        {
            Log(L"Removing old exe...");
            std::filesystem::remove(oldExePath);
        }

        RunExe(newExePath, cmdLine);
    }
    catch (const std::exception &e)
    {
        switch (UpdateFailedPromt(e))
        {
            case UpdateFailedChoice::UseOld:
                std::format_to(std::back_inserter(cmdLine), L" --no_update");
                break;
            case UpdateFailedChoice::Cancel:
                std::format_to(std::back_inserter(cmdLine), L" --cleanup");
                break;
        }
        RunExe(oldExePath, cmdLine);
    }

    // Exit current instance
    ExitProcess(0);
}

void DownloadAndLaunchInstaller(GitHubRelease &release)
{
    Log(L"Downloading release...");
    CurlEasy().DownloadFile(release.download_url, installerArchivePath);

    try
    {
        if (std::filesystem::is_empty(installerArchivePath))
        {
            throw std::runtime_error("The installation archive is empty.");
        }

        // Launch installer/updater from a temp copy so the original exe can be replaced.
        Log(std::format(L"Copying exe from {} to {}", curExePath.wstring(), installerExePath.wstring()));
        std::filesystem::copy_file(curExePath, installerExePath, std::filesystem::copy_options::overwrite_existing);

        try
        {
            auto cmdLine = std::format(L"--prev_pid {} --install \"{}\"", GetCurrentProcessId(), curExePath.c_str());
            RunExe(installerExePath, cmdLine);

            // Exit current instance so updater can overwrite files
            ExitProcess(0);
        }
        catch (const std::exception &e)
        {
            std::filesystem::remove(installerExePath);
            throw;
        }
    }
    catch (const std::exception &e)
    {
        std::filesystem::remove(installerArchivePath);
        throw;
    }
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
                    "\n\n"
                    "Release notes:"
                    "\n\n"
                    "{}"
                    "\n\n\n"
                    "Press OK to download and install.",
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
                switch (UpdateFailedPromt(e))
                {
                    case UpdateFailedChoice::UseOld:
                        Log(L"Using old version...");
                        break;
                    case UpdateFailedChoice::Cancel:
                        Log(L"Terminating...");
                        ExitProcess(0);
                }
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
            std::wcerr << "(Maybe try adding " MOD_DLL_NAME " and " << curExePath.filename() << " to your antivirus whitelist/exlusion list)" << std::endl;
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

void FocusWindow(HWND hwnd)
{
    if (!hwnd)
        return;

    if (IsIconic(hwnd))
    {
        // If minimized, restore it
        ShowWindow(hwnd, SW_RESTORE);
    }
    else
    {
        // Otherwise, give it the focus
        SetForegroundWindow(hwnd);
    }
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
        MessageBoxW(gwHwnd, L"Failed to find Guild Wars process.\n\n(Guild wars must be running when launching Hero's Insight.)", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (IsDllLoaded(gwProcessId, installation->mod_dll))
    {
        MessageBoxW(gwHwnd, L"Hero's Insight is already running.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (!TryInjectDLL(gwProcessId, installation->mod_dll))
    {
        MessageBoxW(gwHwnd, L"Failed to attach mod, check output file for more details.", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    std::cout << "Mod DLL attached successfully." << std::endl;

    FocusWindow(gwHwnd);

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

struct Args
{
    int count = 0;
    LPWSTR *ptr;
    std::unordered_map<std::wstring_view, int> positions;

    Args()
    {
        auto commandLine = GetCommandLineW();
        ptr = CommandLineToArgvW(commandLine, &count);
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
            std::wcout << ptr[i] << L" ";
        }
        std::wcout << std::endl;
        std::wcout << L"Version: " << HEROSINSIGHT_VERSION_STRING << std::endl;
    }

    bool Has(std::wstring_view name)
    {
        auto &pos = positions[name];
        pos = find(name, pos);
        if (pos >= count)
            return false;
        ++pos;
        return true;
    }

    bool GetInt(std::wstring_view name, int &out)
    {
        auto &pos = positions[name];
        pos = find(name, pos);
        if (pos + 1 >= count)
            return false;

        if (!parse_int(ptr[pos + 1], out))
            return false;

        pos += 2;
        return true;
    }

    bool GetPath(std::wstring_view name, std::filesystem::path &out)
    {
        auto &pos = positions[name];
        pos = find(name, pos);
        if (pos + 1 >= count)
            return false;

        out = ptr[pos + 1];
        pos += 2;
        return true;
    }

private:
    int find(std::wstring_view name, int pos) const
    {
        for (int i = pos; i < count; ++i)
            if (wcscmp(ptr[i], name.data()) == 0)
                return i;
        return count;
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

void DeleteRequestedFiles(Args &args)
{
    // "--delete [local path]" must be supported by every version of the launcher.
    std::filesystem::path local_path;
    while (args.GetPath(L"--delete", local_path))
    {
        if (local_path.is_absolute())
            continue; // Ignore absolute paths

        auto abs_path = rootDirPath / local_path;
        std::wcout << "Deleting " << abs_path << std::endl;
        std::filesystem::remove(abs_path);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
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
        flags |= std::ios::app; // Append to file
    }

    // Redirect std::cout and std::wcerr to a file
    std::wofstream logFile(logFilePath, flags);
    std::wcout.rdbuf(logFile.rdbuf());
    std::wcerr.rdbuf(logFile.rdbuf());

    args.Log();

    DeleteRequestedFiles(args);
    if (args.Has(L"--cleanup")) // cleanup means just delete files and exit
    {
        return 0;
    }

    gwProcessId = GetProcessIdByName("Gw.exe");
    gwHwnd = FindWindowByPID(gwProcessId);

    int result = 0;

    std::filesystem::path oldExePath;
    if (args.GetPath(L"--install", oldExePath))
    {
        assert(curExePath == installerExePath);
        InstallAndLaunch(oldExePath);
    }

    if (args.Has(L"--no_update"))
    {
        check_for_updates = false;
    }
    result = RunNormalApp();

    return result;
}
