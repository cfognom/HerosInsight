#pragma once

#include <filesystem>
#include <imgui.h>
#include <iostream>

#include <make_color.h>

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

        static HMODULE GetCurrentModule()
        {
            HMODULE hModule = NULL;
            GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)&GetCurrentModule, &hModule);
            return hModule;
        }

        Paths()
        {
            auto hModule = GetCurrentModule();
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
        // clang-format off
        constexpr inline ImU32 damage_yellow       = MakeColor::U32::rgb(255, 255, 120);
        constexpr inline ImU32 header_beige        = MakeColor::U32::rgb(228, 214, 171);
        constexpr inline ImU32 skill_dynamic_green = MakeColor::U32::rgb(143, 255, 143);
        constexpr inline ImU32 heal_blue           = MakeColor::U32::rgb(106, 195, 219);
        constexpr inline ImU32 hp_red              = MakeColor::U32::rgb(212, 48, 48);
        constexpr inline ImU32 energy_blue         = MakeColor::U32::rgb(65, 150, 215);
        constexpr inline ImU32 elite_gold          = MakeColor::U32::rgb(202, 164, 75);
        constexpr inline ImU32 skill_dull_gray     = MakeColor::U32::rgb(178, 178, 178);
        constexpr inline ImU32 window_title_text   = MakeColor::U32::rgb(221, 221, 221);
        constexpr inline ImVec4 button_blue        = MakeColor::ImVec4::rgb(55, 85, 124);
        constexpr inline ImVec4 button_green       = MakeColor::ImVec4::rgb(31, 107, 57);
        constexpr inline ImVec4 button_red         = MakeColor::ImVec4::rgb(93, 37, 36);
        constexpr inline ImVec4 checkmark_beige    = MakeColor::ImVec4::rgb(225, 215, 178);
        constexpr inline ImVec4 checkbox_blue      = MakeColor::ImVec4::rgb(41, 50, 63);
        // clang-format on
        constexpr inline ImU32 effect_border_colors[6] = {
            MakeColor::U32::rgba_hex<"#00000000">(), // null
            MakeColor::U32::rgb_hex<"#4a8346">(),    // default green
            MakeColor::U32::rgb_hex<"#d4a832">(),    // condition yellow
            MakeColor::U32::rgb_hex<"#a6d13a">(),    // enchantment green
            MakeColor::U32::rgb_hex<"#df29a7">(),    // hex purple
            MakeColor::U32::rgb_hex<"#307997">()     // dervish blue
        };
    }

    namespace Colors
    {
        constexpr inline ImU32 notify = MakeColor::U32::rgb(240, 240, 128);
        constexpr inline ImU32 highlight = MakeColor::U32::rgb(250, 148, 54);
    };
}
