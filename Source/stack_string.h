#pragma once

#include <cstring>
#include <string_view>
#include <cstdarg>

namespace HerosInsight::Utils
{
    bool AppendFormattedV(char *buf, size_t buf_size, size_t &len, const char *fmt, va_list args);

    template <size_t N>
    struct StackString
    {
        char data[N + 1];
        size_t len = 0;

        constexpr StackString()
        {
            data[0] = '\0';
        }

        StackString(const char *str)
        {
            append(str);
        }

        bool append(const char *str, const char *str_end = nullptr)
        {
            if (str_end == nullptr)
                str_end = str + strlen(str);
            auto str_len = str_end - str;
            size_t available = N - len;
            bool truncated = str_len > available;
            auto copy_len = std::min(str_len, available);
            memcpy(data + len, str, copy_len);
            len += copy_len;
            data[len] = '\0';
            return truncated;
        }

        bool append_formatted(const char *fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            bool result = Utils::AppendFormattedV(data, N, len, fmt, args);
            data[len] = '\0';
            va_end(args);
            return result;
        }

        constexpr size_t capacity() const
        {
            return N;
        }

        size_t size() const
        {
            return len;
        }

        const char *c_str() const
        {
            return data;
        }

        std::string_view view() const
        {
            return std::string_view(data, len);
        }

        operator std::string_view() const
        {
            return view();
        }
    };
}