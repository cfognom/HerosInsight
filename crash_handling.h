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

    LONG ErrorPromt(EXCEPTION_POINTERS *info, const std::exception *e)
    {
        char msg[1024];
        SpanWriter<char> writer(msg);

        writer.AppendString("HerosInsight encountered an error.");
        if (e)
        {
            writer.AppendString("\nError message: \"");
            writer.AppendString(e->what());
        }
        else
        {
            writer.AppendString("\nError code: \"");
            writer.AppendIntToChars(info->ExceptionRecord->ExceptionCode, 16);
        }
        writer.push_back('\"');

#ifdef _DEBUG
        writer.push_back('\n');
        writer.AppendString("\nAbort: Attempt to unhook the mod"
                            "\nRetry: Attempt to break the debugger"
                            "\nIgnore: Pass to GW crash handler");
        writer.push_back('\0');

        HWND hWnd = GW::MemoryMgr::GetGWWindowHandle();
    retry:
        int result = MessageBoxA(hWnd, msg, "HerosInsight Error", MB_ABORTRETRYIGNORE | MB_ICONERROR);

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
        auto path = WriteCrashDump(info);

        if (path.has_value())
        {
            auto &path_value = path.value();
            writer.AppendString("\n\nA crash report has been saved to:\n");
            writer.AppendString(path_value.string());
        }
        else
        {
            writer.AppendString("\n\nAttempted to write a crash report but failed.");
        }
        writer.AppendString("\n\nGame state might be unstable, please restart the game as soon as possible.");
        writer.push_back('\0');

        MessageBoxA(nullptr, msg, "HerosInsight Crash", MB_OK | MB_ICONERROR);

        return EXCEPTION_EXECUTE_HANDLER;
#endif
    }

    inline thread_local EXCEPTION_POINTERS *g_exceptionPointers = nullptr;
    LONG WINAPI CrashHandler(EXCEPTION_POINTERS *info)
    {
        DWORD code = info->ExceptionRecord->ExceptionCode;
        bool is_cpp_exception = code == 0xE06D7363;
        bool is_SEH_exception = !is_cpp_exception;

        if (is_cpp_exception)
        {
            g_exceptionPointers = info;
            return EXCEPTION_CONTINUE_SEARCH; // We pass it to the C++ exception handler, where we can get additional info
        }
        else
        {
            return ErrorPromt(info, nullptr);
        }
    }

    bool SafeCallInner(void (*fn)(void *data), void *data)
    {
        bool success = true;
        __try
        {
            fn(data);
        }
        __except (CrashHandler(GetExceptionInformation()))
        {
            success = false;
        }
        return success;
    }

    bool SafeCall(void (*fn)(void *data), void *data = nullptr)
    {
        bool success = true;
#ifdef DISABLE_SAFECALL
        fn(data);
#else
        try
        {
            success = SafeCallInner(fn, data);
        }
        catch (const std::exception &e)
        {
            if (g_exceptionPointers)
            {
                auto result = ErrorPromt(g_exceptionPointers, &e);
                switch (result)
                {
                    case EXCEPTION_CONTINUE_SEARCH:
                        success = true;
                        break;
                    case EXCEPTION_EXECUTE_HANDLER:
                        success = false;
                        break;
                }
            }
            else
            {
                success = false;
            }
        }
#endif
        return success;
    }
}