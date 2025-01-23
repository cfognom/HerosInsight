// Reordering these causes compile errors...
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <assert.h>
// clang-format on

DWORD GetProcessIdByName(const char *processName)
{
    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to create snapshot. Error: " << GetLastError() << std::endl;
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

bool TryInjectDLL(DWORD processId, const char *dllPath)
{
    assert(processId);
    assert(dllPath);

    bool success = false;
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (process == NULL)
    {
        auto last_error = GetLastError();
        std::cerr << "Failed to open target process. Error: " << last_error << std::endl;
        if (last_error == 5)
        {
            std::cerr << "(Maybe try adding the HerosInsight dll and launcher exe to your antivirus whitelist/exlusion list)" << std::endl;
        }
    }
    else
    {
        size_t dllPathLength = strlen(dllPath) + 1;
        LPVOID allocatedMemory = VirtualAllocEx(process, NULL, dllPathLength, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (allocatedMemory == NULL)
        {
            std::cerr << "Failed to allocate memory in target process." << std::endl;
        }
        else
        {
            if (!WriteProcessMemory(process, allocatedMemory, dllPath, dllPathLength, NULL))
            {
                std::cerr << "Failed to write DLL path to target process memory." << std::endl;
            }
            else
            {
                HANDLE remoteThread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, allocatedMemory, 0, NULL);
                if (remoteThread == NULL)
                {
                    std::cerr << "Failed to create remote thread in target process." << std::endl;
                }
                else
                {
                    WaitForSingleObject(remoteThread, INFINITE);
                    success = true;

                    CloseHandle(remoteThread);
                }
            }
            VirtualFreeEx(process, allocatedMemory, 0, MEM_RELEASE);
        }
        CloseHandle(process);
    }
    return success;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const char *processName = "Gw.exe";

    char dllPathBuffer[MAX_PATH];
    GetFullPathName("HerosInsight.dll", MAX_PATH, dllPathBuffer, NULL);
    const char *dllPath = dllPathBuffer;

    // Redirect std::cout and std::cerr to a file
    std::ofstream outFile("HerosInsight_output.log");
    std::streambuf *originalCoutBuffer = std::cout.rdbuf();
    std::streambuf *originalCerrBuffer = std::cerr.rdbuf();
    std::cout.rdbuf(outFile.rdbuf());
    std::cerr.rdbuf(outFile.rdbuf());

    DWORD processId = GetProcessIdByName(processName);
    if (processId == 0)
    {
        MessageBox(NULL, "Failed to find target process.", "Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (TryInjectDLL(processId, dllPath))
    {
        std::cout << "DLL injected successfully." << std::endl;
    }
    else
    {
        MessageBox(NULL, "Failed to inject DLL, check output file for more details.", "Error", MB_ICONERROR | MB_OK);
    }

    // Restore the original buffers
    std::cout.rdbuf(originalCoutBuffer);
    std::cerr.rdbuf(originalCerrBuffer);

    return 0;
}