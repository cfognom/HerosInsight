#pragma once

#include <compiletime_string.h>
#include <imgui.h>

// Helpers to create colors exploiting vscode's css color picker
namespace MakeColor
{
    template <char c>
    constexpr uint8_t hex_digit()
    {
        constexpr uint8_t value =
            (c >= '0' && c <= '9')   ? (c - '0')
            : (c >= 'a' && c <= 'f') ? (c - 'a' + 10)
            : (c >= 'A' && c <= 'F') ? (c - 'A' + 10)
                                     : 42;
        static_assert(value < 16, "Invalid hex digit");
        return value;
    };

    namespace U32
    {
        inline constexpr uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) { return IM_COL32(r, g, b, 255); }
        inline constexpr uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, double a) { return IM_COL32(r, g, b, a * 255.0 + 0.5); }

        // Example: rgb_hex<"#RRGGBB">()
        template <CompiletimeString S>
        constexpr uint32_t rgb_hex()
        {
            constexpr auto hex = S.str();
            static_assert(hex.size() == 7 || hex.size() == 9, "Hex string must be either 7 or 9 characters");
            static_assert(hex[0] == '#', "Hex string must start with '#'");

            auto r = hex_digit<hex[1]>() << 4 | hex_digit<hex[2]>();
            auto g = hex_digit<hex[3]>() << 4 | hex_digit<hex[4]>();
            auto b = hex_digit<hex[5]>() << 4 | hex_digit<hex[6]>();
            return IM_COL32(r, g, b, 255);
        }

        // Example: rgb_hex<"#RRGGBBAA">()
        template <CompiletimeString S>
        constexpr uint32_t rgba_hex()
        {
            constexpr auto hex = S.str();
            static_assert(hex.size() == 9, "Hex string must be 9 characters");
            static_assert(hex[0] == '#', "Hex string must start with '#'");

            auto r = hex_digit<hex[1]>() << 4 | hex_digit<hex[2]>();
            auto g = hex_digit<hex[3]>() << 4 | hex_digit<hex[4]>();
            auto b = hex_digit<hex[5]>() << 4 | hex_digit<hex[6]>();
            auto a = hex_digit<hex[7]>() << 4 | hex_digit<hex[8]>();
            return IM_COL32(r, g, b, a);
        }
    }

    namespace ImVec4
    {
        inline ::ImVec4 rgb(uint8_t r, uint8_t g, uint8_t b)
        {
            return ::ImVec4(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, 1.0f);
        }

        inline ::ImVec4 rgba(uint8_t r, uint8_t g, uint8_t b, double a)
        {
            return ::ImVec4(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, float(a));
        }
    }
}