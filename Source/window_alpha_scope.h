#pragma once

#include <unordered_map>

#include <imgui.h>
#include <imgui_internal.h>

namespace HerosInsight
{
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