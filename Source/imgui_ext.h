#pragma once

#include <optional>

#include <imgui.h>

#include <GWCA/Managers/TextMgr.h>

#include <constants.h>
#include <imgui_custom.h>

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
        explicit operator bool() const { return begun; }
    };

    struct TabBarScope
    {
        bool result;
        TabBarScope(const char *str_id, ImGuiTabBarFlags flags = 0)
            : result(ImGui::BeginTabBar(str_id, flags)) {}
        ~TabBarScope() { ImGui::EndTabBar(); }
        explicit operator bool() const { return result; }
    };

    struct TabItemScope
    {
        bool opened;
        TabItemScope(const char *label, bool *p_open = nullptr, ImGuiTabItemFlags flags = 0)
        {
            ImGui::PushFont(Constants::Fonts::button_font);
            opened = ImGui::BeginTabItem(label, p_open, flags);
            ImGui::PopFont();
        }
        ~TabItemScope()
        {
            if (opened) ImGui::EndTabItem();
        }
        explicit operator bool() const { return opened; }
    };

    inline bool Button(const char *label, const ImVec2 &size = ImVec2(0, 0))
    {
        ImGui::PushFont(Constants::Fonts::button_font);
        bool pressed = ImGui::Button(label, size);
        ImGui::PopFont();
        return pressed;
    }

    inline bool Combo(const char *label, int *current_item, const char *const items[], int items_count, int popup_max_height_in_items = -1)
    {
        ImGui::PushFont(Constants::Fonts::button_font);
        bool opened = ImGui::Combo(label, current_item, items, items_count, popup_max_height_in_items);
        ImGui::PopFont();
        return opened;
    }

    inline bool RadioArray(const char *label, int *current_item, const char *const items[], int items_count)
    {
        ImGui::PushFont(Constants::Fonts::button_font);
        auto size = ImGui::CalcTextSize(label);
        auto init_cursor = ImGui::GetCursorScreenPos();

        auto new_cursor = init_cursor;
        new_cursor.x += size.x + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorScreenPos(new_cursor);
        bool pressed_any = false;
        for (int i = 0; i < items_count; i++)
        {
            bool pressed = ImGui::RadioButton(items[i], *current_item == i);
            if (pressed)
            {
                *current_item = i;
                pressed_any = true;
            }
            if (i < items_count - 1)
                ImGui::SameLine();
        }

        auto radio_size = ImGui::GetItemRectSize();
        auto y_offset = (radio_size.y - size.y) / 2;
        new_cursor = init_cursor;
        new_cursor.y += y_offset;
        auto draw_list = ImGui::GetWindowDrawList();
        draw_list->AddText(new_cursor, ImGui::GetColorU32(ImGuiCol_Text), label);

        ImGui::PopFont();
        return pressed_any;
    }

    struct TextColor
    {
        TextColor(uint32_t color) { ImGui::PushStyleColor(ImGuiCol_Text, color); }
        ~TextColor() { ImGui::PopStyleColor(); }
    };

    struct TextSize
    {
        TextSize() { ImGuiCustom::PushFontSizeDefault(); }
        TextSize(GW::Constants::InterfaceSize size) { ImGuiCustom::PushFontSize(size); }
        TextSize(int32_t interfaceSizeChange) { ImGuiCustom::PushFontSize(interfaceSizeChange); }
        TextSize(float size) { ImGui::PushFont(NULL, size); }
        ~TextSize() { ImGui::PopFont(); }
    };

    struct TextFont
    {
        TextFont() { ImGuiCustom::PushFont(GW::TextMgr::EngFont::Default); }
        TextFont(GW::TextMgr::EngFont fontId) { ImGuiCustom::PushFont(fontId); }
        TextFont(ImFont *font) { ImGuiCustom::PushFont(font); }
        ~TextFont() { ImGui::PopFont(); }
    };

    struct TextEffect
    {
        TextEffect() { ImGuiCustom::PushFontFlags(GW::TextMgr::BlitFontFlags::None); }
        TextEffect(GW::TextMgr::BlitFontFlags flags) { ImGuiCustom::PushFontFlags(flags); }
        ~TextEffect() { ImGui::PopFont(); }
    };
}