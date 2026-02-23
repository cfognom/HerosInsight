#pragma once

template <size_t N>
struct CompiletimeString
{
    char data[N] = {};

    constexpr CompiletimeString(const char (&str)[N])
    {
        for (size_t i = 0; i < N; ++i)
        {
            data[i] = str[i];
        }
    }

    // Conversion to string_view for easy use
    constexpr std::string_view str() const
    {
        return std::string_view(data, N - 1); // Exclude null terminator
    }
};