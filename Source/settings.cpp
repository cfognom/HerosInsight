#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <string>

#include <constants.h>
#include <imgui_custom.h>
#include <update_manager.h>

#include "settings.h"

std::filesystem::path GetSettingsFilePath()
{
    return Constants::paths.root() / "settings.txt";
}

namespace HerosInsight
{
    SettingsManager &SettingsManager::Get()
    {
        static SettingsManager instance; // ONE global instance
        return instance;
    }

    SettingsManager::SettingsManager()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        assert(m_active_settings == 0);

        auto filepath = GetSettingsFilePath();

        m_settings.clear();
        std::ifstream file(filepath);
        if (!file.is_open()) return;

        std::string line;
        while (std::getline(file, line))
        {
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            m_settings[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }

    SettingsManager::~SettingsManager()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        assert(m_active_settings == 0 && "Destroying SettingsManager before all settings have been destroyed!");

        auto filepath = GetSettingsFilePath();
        std::ofstream file_out(filepath, std::ios::trunc);
        if (!file_out.is_open()) return;

        for (const auto &[key, value] : m_settings)
        {
            file_out << key << "=" << value << "\n";
        }
    }

    template <typename T>
    bool ResetButton(const char *label, T &value)
    {
        bool pressed = ImGuiCustom::Button(label);
        if (pressed)
        {
            SettingsManager::ForceDefaultScope guard{};
            Utils::Reconstuct(value);
        }
        return pressed;
    }

    void DrawGeneral(Settings &settings)
    {
        if (ImGuiCustom::TabItemScope item{"General"})
        {
            ImGui::Checkbox("Scroll snap to item", &settings.general.scroll_snap_to_item.value);
            ImGuiCustom::SliderFloat("Menu fadeout (s)", &settings.general.main_menu_fadeout_seconds.value, 0.f, 3.f, "%.2f");

            auto &io = ImGui::GetIO();
            {
                auto ini_filename = io.IniFilename;
                auto cache_dir_string = Constants::paths.cache().string();
                std::string_view imgui_ini_filename_str{ini_filename};
                if (imgui_ini_filename_str.starts_with(cache_dir_string.c_str()) &&
                    std::filesystem::path(ini_filename).is_absolute()) // Safety
                {
                    if (ImGuiCustom::Button("Clear window cache"))
                    {
                        std::remove(ini_filename);
                    }
                }
            }

            ImGui::Separator();

            ResetButton("Reset to default", settings.general);
            ResetButton("Reset ALL settings to default", settings);
        }
    }

    void DrawSkillBook(Settings &settings)
    {
        if (ImGuiCustom::TabItemScope item{"Skill Book"})
        {
            ImGui::Checkbox("Show help button", &settings.skill_book.show_help_button.value);
            ImGui::Checkbox("Show focused character", &settings.skill_book.show_focused_character.value);

            const char *feedback_items[] = {"Hidden", "Concise", "Detailed"};
            ImGuiCustom::Combo("Feedback", &settings.skill_book.feedback.value, feedback_items, IM_ARRAYSIZE(feedback_items));

            ImGui::Separator();

            ResetButton("Reset to default", settings.skill_book);
        }
    }

    void Settings::Draw(IDirect3DDevice9 *device)
    {
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
        if (ImGuiCustom::WindowScope wnd{"Settings", &HerosInsight::UpdateManager::open_settings})
        {
            // Make tabs: General, Skillbook
            if (ImGuiCustom::TabBarScope bar{"SettingsTabs"})
            {
                SettingsGuard g{};
                auto &settings = g.Access();
                DrawGeneral(settings);
                DrawSkillBook(settings);
            }
        }
    }

    static std::recursive_mutex settings_mutex;
    static Settings settings;
    SettingsGuard::SettingsGuard()
    {
        settings_mutex.lock();
    }
    SettingsGuard::~SettingsGuard()
    {
        settings_mutex.unlock();
    }
    Settings &SettingsGuard::Access()
    {
        return settings;
    }
}
