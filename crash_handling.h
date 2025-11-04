#pragma once

#include <cstdio>
#include <ctime>
#include <dbghelp.h>
#include <string>
#include <windows.h>

#include <GWCA/Managers/MemoryMgr.h>
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

    std::optional<std::filesystem::path> WriteCrashDump(EXCEPTION_POINTERS *info)
    {
        auto dump_path = GetTimestampedPath("crash", "dmp");
        HANDLE hFile = CreateFileW(dump_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return std::nullopt;

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

        return dump_path;
    }

    LONG WINAPI CrashHandler(EXCEPTION_POINTERS *info)
    {
#ifdef _DEBUG
        HWND hWnd = GW::MemoryMgr::GetGWWindowHandle();
    retry:
        int result = MessageBoxA(
            hWnd,
            "HerosInsight has crashed.\n\n"
            "Abort: Attempt to unhook the mod\n"
            "Retry: Attempt to break the debugger\n"
            "Ignore: Pass to GW crash handler",
            "HerosInsight Crash",
            MB_ABORTRETRYIGNORE | MB_ICONERROR
        );

        switch (result)
        {
            case IDABORT:
                return EXCEPTION_EXECUTE_HANDLER;

            case IDRETRY:
                if (IsDebuggerPresent())
                    __debugbreak();
                else
                    goto retry;
                return EXCEPTION_EXECUTE_HANDLER;

            case IDIGNORE:
            default:
                return EXCEPTION_CONTINUE_SEARCH;
        }
#else

        wchar_t msg[1024];
        SpanWriter<wchar_t> writer(msg);
        writer.AppendString(L"HerosInsight encountered an error. Game state might be unstable, please restart the game as soon as possible.");

        auto path = WriteCrashDump(info);
        if (path.has_value())
        {
            writer.AppendString(L"\n\nA crash report has been saved to:\n");
            writer.AppendString(path.value().c_str());
        }
        writer.push_back(L'\0');

        MessageBoxW(
            nullptr,
            msg,
            L"HerosInsight Crash",
            MB_OK | MB_ICONERROR
        );

        return EXCEPTION_EXECUTE_HANDLER;
#endif
    }

    bool SafeCall(void (*fn)(void *data), void *data, void (*termination_fn)())
    {
        bool success = true;
#ifdef DISABLE_SAFECALL
        fn(data);
#else
        __try
        {
            fn(data);
        }
        __except (CrashHandler(GetExceptionInformation()))
        {
            termination_fn();
            success = false;
        }
#endif
        return success;
    }
}