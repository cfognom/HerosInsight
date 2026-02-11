#pragma once

#include <cstdio>
#include <ctime>
#include <dbghelp.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <windows.h>

#include <GWCA/Managers/MemoryMgr.h>
#include <constants.h>
#include <utils.h>

namespace HerosInsight::CrashHandling
{
    inline std::filesystem::path GetTimestampedPath(const char *prefix, const char *ext)
    {
        char buf[128];
        std::time_t t = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &t);
        sprintf(buf, "%s_%04d%02d%02d_%02d%02d%02d.%s", prefix, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec, ext);
        return Constants::paths.crash() / std::filesystem::path(buf);
    }

    inline std::optional<std::filesystem::path> WriteCrashDump(EXCEPTION_POINTERS *info)
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

    inline static std::atomic<std::wstring_view> msg_overload{};

    inline std::optional<std::wstring> ErrorCodeToString(DWORD error_code)
    {
        std::string_view str{reinterpret_cast<const char *>(&error_code), sizeof(decltype(error_code))};
        bool any_printable = false;
        for (auto &c : str)
        {
            if (c == '\0')
                return std::nullopt;
            if (!any_printable && std::isprint(c))
                any_printable = true;
        }
        if (!any_printable)
            return std::nullopt;
        if constexpr (std::endian::native == std::endian::little)
        {
            return std::wstring{str.rbegin(), str.rend()};
        }
        else
        {
            return std::wstring{str.begin(), str.end()};
        }
    }

    struct ExceptionRecord
    {
        std::optional<std::filesystem::path> report_path = std::nullopt;
        std::optional<DWORD> code = std::nullopt;
        const std::exception *exception = nullptr;

        inline LONG ErrorPromt()
        {
            wchar_t msg[1024];
            SpanWriter<wchar_t> writer(msg);

            auto msg_ov = msg_overload.load();
            writer.AppendString(!msg_ov.empty() ? msg_ov : L"Hero's Insight encountered an error.");
            writer.push_back(L'\n');
            if (exception)
            {
                writer.AppendString(L"\nError message: \"");
                auto what = exception->what();
                std::wstring error_msg(what, what + strlen(what));
                writer.AppendString(error_msg);
                writer.push_back(L'\"');
            }

            if (code.has_value())
            {
                // auto val = code.value();
                auto val = 0xE06D7363;
                writer.AppendFormat(L"\nError code: 0x{:X}", val);
                if (auto code_str = ErrorCodeToString(val); code_str.has_value())
                {
                    writer.AppendFormat(L" (\"{}\")", code_str.value());
                }
            }
            else
            {
                writer.AppendString(L"\nNo error code available.");
            }

            HWND hWnd = GW::IsInitialized() ? GW::MemoryMgr::GetGWWindowHandle() : nullptr;

#ifdef _DEBUG
            writer.push_back(L'\n');
            writer.AppendString(L"\nAbort: Attempt to unhook the mod"
                                L"\nRetry: Attempt to break the debugger"
                                L"\nIgnore: Pass to GW crash handler");
            writer.push_back(L'\0');

        retry:
            int result = MessageBoxW(hWnd, msg, L"Hero's Insight Error", MB_ABORTRETRYIGNORE | MB_ICONERROR);

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
            if (report_path.has_value())
            {
                auto &path_value = report_path.value();
                writer.AppendString(L"\n\nA crash report has been saved to:\n");
                writer.AppendString(path_value.c_str());
            }
            else
            {
                writer.AppendString(L"\n\nAttempted to write a crash report but failed.");
            }

            writer.AppendString(L"\n\nGame state might be unstable, please restart the game as soon as possible.");
            writer.push_back(L'\0');

            MessageBoxW(hWnd, msg, L"Hero's Insight Crash", MB_OK | MB_ICONERROR);

            return EXCEPTION_EXECUTE_HANDLER;
#endif
        }
    };

    inline LONG WINAPI CrashHandler(EXCEPTION_POINTERS *info, ExceptionRecord &record)
    {
        record.code = info->ExceptionRecord->ExceptionCode;
        bool is_cpp_exception = record.code == 0xE06D7363;
        bool is_SEH_exception = !is_cpp_exception;

        record.report_path = WriteCrashDump(info);

        if (is_cpp_exception)
        {
            return EXCEPTION_CONTINUE_SEARCH; // We pass it to the C++ exception handler, where we can get additional info
        }
        else
        {
            return record.ErrorPromt();
        }
    }

    inline bool SafeCallInner(void (*fn)(void *data), void *data, ExceptionRecord &record)
    {
        bool success = true;
        __try
        {
            fn(data);
        }
        __except (CrashHandler(GetExceptionInformation(), record))
        {
            success = false;
        }
        return success;
    }

    inline bool HandleCPPException(ExceptionRecord &record)
    {
        bool success = true;
        auto result = record.ErrorPromt();
        switch (result)
        {
            case EXCEPTION_CONTINUE_SEARCH:
                success = true;
                break;
            case EXCEPTION_EXECUTE_HANDLER:
                success = false;
                break;
        }
        return success;
    }

    inline bool SafeCall(void (*fn)(void *data), void *data = nullptr)
    {
        bool success = true;
#ifdef ENABLE_SAFECALL
        ExceptionRecord record;
        try
        {
            success = SafeCallInner(fn, data, record);
        }
        catch (const std::exception &e)
        {
            record.exception = &e;
            success = HandleCPPException(record);
        }
        catch (...)
        {
            success = HandleCPPException(record);
        }
#else
        fn(data);
#endif
        return success;
    }
}