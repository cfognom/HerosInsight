#pragma once

#include <imgui.h>

#include <constants.h>

namespace HerosInsight::ImGuiExt
{
    struct WindowScope
    {
        bool begun = false;
        WindowScope(const char *name, bool *p_open = nullptr, ImGuiWindowFlags flags = 0)
        {
            auto current_font = ImGui::GetFont();
            ImGui::PushFont(Constants::Fonts::window_name_font);
            begun = ImGui::Begin(name, p_open, flags);
            ImGui::PushFont(current_font);
        }
        ~WindowScope()
        {
            ImGui::PopFont();
            ImGui::End();
            ImGui::PopFont();
        }
    };

    inline bool Button(const char *label, const ImVec2 &size = ImVec2(0, 0))
    {
        ImGui::PushFont(Constants::Fonts::button_font);
        bool pressed = ImGui::Button(label, size);
        ImGui::PopFont();
        return pressed;
    }
}