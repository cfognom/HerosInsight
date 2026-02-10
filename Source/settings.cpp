#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <string>

#include <constants.h>
#include <imgui_ext.h>
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

    void DrawGeneral(Settings &settings)
    {
        if (ImGuiExt::BeginTabItem("General"))
        {
            ImGui::Checkbox("Scroll snap to item", &settings.general.scroll_snap_to_item.value);

            auto &io = ImGui::GetIO();
            auto ini_filename = io.IniFilename;
            auto cache_dir_string = Constants::paths.cache().string();
            std::string_view imgui_ini_filename_str{ini_filename};
            if (imgui_ini_filename_str.starts_with(cache_dir_string.c_str()) &&
                std::filesystem::path(ini_filename).is_absolute()) // Safety
            {
                if (ImGuiExt::Button("Clear window cache"))
                {
                    std::remove(ini_filename);
                }
            }
            if (ImGuiExt::Button("Reset to default"))
            {
                SettingsManager::ForceDefaultScope guard{};
                Utils::Reconstuct(settings.general);
            }
            if (ImGuiExt::Button("Reset ALL settings to default"))
            {
                SettingsManager::ForceDefaultScope guard{};
                Utils::Reconstuct(settings);
            }

            ImGui::EndTabItem();
        }
    }

    void DrawSkillBook(Settings &settings)
    {
        if (ImGuiExt::BeginTabItem("Skill Book"))
        {
            ImGui::Checkbox("Show help button", &settings.skill_book.show_help_button.value);
            ImGui::Checkbox("Show focused character", &settings.skill_book.show_focused_character.value);

            const char *feedback_items[] = {"Hidden", "Concise", "Detailed"};
            ImGuiExt::Combo("Feedback", &settings.skill_book.feedback.value, feedback_items, IM_ARRAYSIZE(feedback_items));

            if (ImGuiExt::Button("Reset to default"))
            {
                SettingsManager::ForceDefaultScope guard{};
                Utils::Reconstuct(settings.skill_book);
            }

            ImGui::EndTabItem();
        }
    }

    void Settings::Draw(IDirect3DDevice9 *device)
    {
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
        if (ImGuiExt::WindowScope scope{"Settings", &HerosInsight::UpdateManager::open_settings}; scope.begun)
        {
            // Make tabs: General, Skillbook
            if (ImGui::BeginTabBar("SettingsTabs"))
            {
                SettingsGuard g{};
                auto &settings = g.Access();
                DrawGeneral(settings);
                DrawSkillBook(settings);
            }
            ImGui::EndTabBar();
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
