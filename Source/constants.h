#pragma once

#include <filesystem>
#include <imgui.h>
#include <iostream>

namespace Constants
{
    struct Paths : public std::array<std::filesystem::path, 4>
    {
        // clang-format off
        std::filesystem::path &root()      { return (*this)[0]; };
        std::filesystem::path &resources() { return (*this)[1]; };
        std::filesystem::path &cache()     { return (*this)[2]; };
        std::filesystem::path &crash()     { return (*this)[3]; };
        // clang-format on

        void Init(HMODULE hModule)
        {
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(hModule, buffer, MAX_PATH);
            auto dll_dir_path = std::filesystem::path(buffer).parent_path();

            this->root() = dll_dir_path;
            this->resources() = dll_dir_path / "resources";
            this->cache() = dll_dir_path / "cache";
            this->crash() = dll_dir_path / "crash";
            for (auto &path : *this)
            {
                if (!std::filesystem::exists(path))
                {
                    std::filesystem::create_directory(path);
                }
            }
        }
    };
    inline Paths paths;

    namespace GWColors
    {
        constexpr inline ImU32 damage_yellow = IM_COL32(255, 255, 120, 255);
        constexpr inline ImU32 header_beige = IM_COL32(228, 214, 171, 255);
        constexpr inline ImU32 skill_dynamic_green = IM_COL32(143, 255, 143, 255);
        constexpr inline ImU32 heal_blue = IM_COL32(106, 195, 219, 255);
        constexpr inline ImU32 hp_red = IM_COL32(212, 48, 48, 255);
        constexpr inline ImU32 energy_blue = IM_COL32(65, 150, 215, 255);
        constexpr inline ImU32 elite_gold = IM_COL32(202, 164, 75, 255);
        constexpr inline ImU32 skill_dull_gray = IM_COL32(178, 178, 178, 255);
        constexpr inline ImU32 effect_border_colors[6] = {
            0x00000000, // null
            0xFF46834a, // default green
            0xFF32a8d4, // condition yellow
            0xFF3ad1a6, // enchantment green
            0xFFa729df, // hex purple
            0xFF977930  // dervish blue
        };
    }

    namespace Fonts
    {
        inline ImFont *gw_font_16;
        inline ImFont *skill_name_font;
        inline ImFont *big_font;
        inline ImFont *skill_thick_font_15;
        inline ImFont *skill_thick_font_12;
        inline ImFont *skill_thick_font_9;
    }
}