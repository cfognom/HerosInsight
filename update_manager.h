#pragma once

#include <unordered_map>
#include <utility>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>

#include "scanner_tool.h"
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
        static bool open_scanner_tool;
#endif
        static bool open_main_menu;
        static bool open_damage;
        static bool open_skill_book;

        // Global array of every frame drawn in the game atm
        inline static GW::Array<GW::UI::Frame *> *s_FrameArray = nullptr;

        inline static std::tuple<bool *, char, char> key_bindings[] = {
            {&open_skill_book, VK_CONTROL, 'K'},
        };

        static bool is_dragging_skill;
        static bool unlock_windows;

        static void Initialize();
        static void Terminate();
        static void Update(GW::HookStatus *);
        static void Draw(IDirect3DDevice9 *device);
        static ImGuiWindowFlags GetWindowFlags();
        static bool RequestSkillDragging(GW::Constants::SkillID skill_id);

    private:
#ifdef _DEBUG
        static ScannerTool scanner_tool;
#endif
    };

    // Hash function for compile-time string hashing
    constexpr unsigned int hash(const char *str, int h = 0)
    {
        return !str[h] ? 5381 : (hash(str, h + 1) * 33) ^ str[h];
    }

// Macro to generate a unique ID based on, __COUNTER__, and a hash of __FILE__
#define UNIQUE_ID (hash(__FILE__) ^ __COUNTER__)

// Macro to instantiate WindowAlphaScope with a unique ID
#define WINDOW_ALPHA_SCOPE() WindowAlphaScope(UNIQUE_ID)

    // Scope guard to change the alpha of a window when dragging a skill
    struct WindowAlphaScope
    {
        inline static std::unordered_map<int, ImRect> window_rects;
        int id;
        bool pushed = false;

        WindowAlphaScope(int unique_id)
        {
            id = unique_id;

            if (UpdateManager::is_dragging_skill)
            {
                auto it = window_rects.find(id);

                if (it != window_rects.end())
                {
                    const auto pad = 40.f;
                    const auto pad2 = ImVec2(pad, pad);
                    if (ImGui::IsMouseHoveringRect(it->second.Min - pad2, it->second.Max + pad2, false))
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f * ImGui::GetStyle().Alpha);
                        pushed = true;
                    }
                }
            }
        }

        ~WindowAlphaScope()
        {
            if (pushed)
                ImGui::PopStyleVar();

            const auto window = ImGui::GetCurrentContext()->CurrentWindow;

            if (!window)
                return;

            const auto rect = window->Rect();

            window_rects[id] = rect;
        }
    };
}