#pragma once

#include <utility>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <cstdint>

#include <utils.h>

namespace HerosInsight
{
    class UpdateManager
    {
    public:
        static uint32_t frame_id;
        static DWORD start_gw_ms;
        static DWORD elapsed_ms;
        static float delta_seconds;
        static float elapsed_seconds;

        static DWORD render_elapsed_ms;
        static float render_delta_seconds;

#ifdef _DEBUG
        static bool enable_ui_debug;
        static bool open_debug;
        static bool open_texture_viewer;
        static bool open_encstr_debugger;
#endif
        static bool open_main_menu;
        static bool open_skill_book;
        static bool open_settings;
#ifdef EXPERIMENTAL_FEATURES
        static bool open_damage;
#endif

        inline static std::tuple<bool *, char, char> key_bindings[] = {
            {&open_skill_book, VK_CONTROL, 'K'},
        };

        static void Initialize();
        static void Terminate();
        static void Update();
        static void Draw(IDirect3DDevice9 *device);
    };
}