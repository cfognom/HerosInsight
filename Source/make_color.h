#pragma once

#include <imgui.h>

// Helpers to create colors exploiting vscode's css color picker
namespace MakeColor
{
    namespace U32
    {
        inline constexpr uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
        {
            return 0xff000000 |
                   (uint32_t(r) << 16) |
                   (uint32_t(g) << 8) |
                   uint32_t(b);
        }

        inline constexpr uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, double a)
        {
            return uint32_t(a * 255.0 + 0.5) << 24 |
                   (uint32_t(r) << 16) |
                   (uint32_t(g) << 8) |
                   uint32_t(b);
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