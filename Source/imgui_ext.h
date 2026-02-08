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

    inline bool BeginTabItem(const char *label, bool *p_open = nullptr, ImGuiTabItemFlags flags = 0)
    {
        ImGui::PushFont(Constants::Fonts::button_font);
        bool opened = ImGui::BeginTabItem(label, p_open, flags);
        ImGui::PopFont();
        return opened;
    }

    inline bool Combo(const char *label, int *current_item, const char *const items[], int items_count, int popup_max_height_in_items = -1)
    {
        ImGui::PushFont(Constants::Fonts::button_font);
        bool opened = ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);
        ImGui::PopFont();
        return opened;
    }
}