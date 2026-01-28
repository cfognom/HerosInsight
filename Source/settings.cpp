#include <fstream>
#include <imgui.h>
#include <string>

#include <constants.h>
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

    void Settings::Draw(IDirect3DDevice9 *device)
    {
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Settings", &HerosInsight::UpdateManager::open_settings))
        {
            // Make tabs: General, Skillbook
            if (ImGui::BeginTabBar("SettingsTabs"))
            {
                general.Draw(device);
                skill_book.Draw(device);
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    void CheckboxSetting(const char *label, ObservableSetting<bool> &setting)
    {
        bool value = setting.Get();
        if (ImGui::Checkbox(label, &value))
        {
            setting.Set(value);
        }
    }

    void ComboSetting(const char *label, ObservableSetting<int> &setting, std::span<const char *> items)
    {
        int value = setting.Get();
        if (ImGui::Combo(label, &value, items.data(), items.size()))
        {
            setting.Set(value);
        }
    }

    void Settings::General::Draw(IDirect3DDevice9 *device)
    {
        if (ImGui::BeginTabItem("General"))
        {
            CheckboxSetting("Scroll snap to item", g_settings.general.scroll_snap_to_item);

            ImGui::EndTabItem();
        }
    }

    void Settings::SkillBook::Draw(IDirect3DDevice9 *device)
    {
        if (ImGui::BeginTabItem("Skill Book"))
        {
            const char *feedback_items[] = {"Hidden", "Concise", "Detailed"};
            ComboSetting("Feedback", g_settings.skill_book.feedback, feedback_items);

            ImGui::EndTabItem();
        }
    }
}
