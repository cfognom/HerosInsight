#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <streambuf>

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

bool InjectDLL(DWORD processId, const char *dllPath)
{
    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (process == NULL)
    {
        std::cerr << "Failed to open target process. Error: " << GetLastError() << std::endl;
        return false;
    }

    size_t dllPathLength = strlen(dllPath) + 1;
    LPVOID allocatedMemory = VirtualAllocEx(process, NULL, dllPathLength, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (allocatedMemory == NULL)
    {
        std::cerr << "Failed to allocate memory in target process." << std::endl;
        CloseHandle(process);
        return false;
    }

    if (!WriteProcessMemory(process, allocatedMemory, dllPath, dllPathLength, NULL))
    {
        std::cerr << "Failed to write DLL path to target process memory." << std::endl;
        VirtualFreeEx(process, allocatedMemory, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HANDLE remoteThread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, allocatedMemory, 0, NULL);
    if (remoteThread == NULL)
    {
        std::cerr << "Failed to create remote thread in target process." << std::endl;
        VirtualFreeEx(process, allocatedMemory, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    WaitForSingleObject(remoteThread, INFINITE);

    VirtualFreeEx(process, allocatedMemory, 0, MEM_RELEASE);
    CloseHandle(remoteThread);
    CloseHandle(process);

    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const char *processName = "Gw.exe"; // Replace with the actual Guild Wars executable name
#ifdef _DEBUG
    const char *dllPath = "C:/Users/carlh/Documents/Guild Wars/HerosInsight/build/Debug/HerosInsight.dll"; // Replace with the actual path to your DLL
#else
    const char *dllPath = "C:/Users/carlh/Documents/Guild Wars/HerosInsight/build/Release/HerosInsight.dll"; // Replace with the actual path to your DLL
#endif

    // Redirect std::cout and std::cerr to a file
    std::ofstream outFile("gw_hero_ai_output.log");
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

    if (InjectDLL(processId, dllPath))
    {
        std::cout << "DLL injected successfully." << std::endl;
    }
    else
    {
        MessageBox(NULL, "Failed to inject DLL.", "Error", MB_ICONERROR | MB_OK);
    }

    // Restore the original buffers
    std::cout.rdbuf(originalCoutBuffer);
    std::cerr.rdbuf(originalCerrBuffer);

    return 0;
}