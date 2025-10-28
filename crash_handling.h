#pragma once

#include <cstdio>
#include <ctime>
#include <dbghelp.h>
#include <string>
#include <windows.h>

#include <constants.h>
#include <utils.h>

// #define DISABLE_SAFECALL

namespace HerosInsight::CrashHandling
{
    std::filesystem::path GetTimestampedPath(const char *prefix, const char *ext)
    {
        char buf[128];
        std::time_t t = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &t);
        sprintf(buf, "%s_%04d%02d%02d_%02d%02d%02d.%s", prefix, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, ext);
        return Constants::paths.crash() / std::filesystem::path(buf);
    }

    void WriteCrashDump(EXCEPTION_POINTERS *info)
    {
        auto dump_path = GetTimestampedPath("crash", "dmp");
        HANDLE hFile = CreateFileW(dump_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return;

        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = info;
        mdei.ClientPointers = FALSE;

        MiniDumpWriteDump(
            GetCurrentProcess(), GetCurrentProcessId(), hFile,
            MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs,
            &mdei, nullptr, nullptr
        );

        CloseHandle(hFile);

        // Optional text summary
        auto log_path = GetTimestampedPath("crash", "txt");
        std::ofstream f(log_path);
        if (f)
        {
            f << "Exception code: 0x" << std::hex << info->ExceptionRecord->ExceptionCode << std::endl;
            f << "Fault address: " << std::hex << info->ExceptionRecord->ExceptionAddress << std::endl;
        }

        wchar_t msg[512];
        swprintf(msg, L"HerosInsight has crashed.\n\nA crash report has been saved to:\n%s\n\n"
                      L"Please send this file to the developer.",
                 dump_path.c_str());
        MessageBoxW(nullptr, msg, L"HerosInsight Crash Report", MB_OK | MB_ICONERROR);
    }

    LONG WINAPI CrashHandler(EXCEPTION_POINTERS *info)
    {
        WriteCrashDump(info);
        return EXCEPTION_EXECUTE_HANDLER; // swallow and exit gracefully
    }

    bool SafeCall(std::wstring_view context, void (*fn)(void *data), void *data)
    {
#ifdef DISABLE_SAFECALL
        fn(data);
#else
        __try
        {
            fn(data);
        }
        __except (CrashHandler(GetExceptionInformation()))
        {
            HerosInsight::Utils::FormatToChat(L"HerosInsight: {} exception", context);
            return false;
        }
#endif
        return true;
    }
}