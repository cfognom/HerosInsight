#pragma once

#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <constants.h>
#include <make_color.h>

struct IDirect3DDevice9;

inline std::istream &operator>>(std::istream &is, ImVec4 &col)
{
    return is >> col.x >> col.y >> col.z >> col.w;
}

inline std::ostream &operator<<(std::ostream &os, const ImVec4 &col)
{
    os << col.x << ' ' << col.y << ' ' << col.z << ' ' << col.w;
    return os;
}

namespace HerosInsight
{
    struct SettingsManager
    {
        static SettingsManager &Get();

        SettingsManager();
        ~SettingsManager();

        // Disable copy
        SettingsManager(const SettingsManager &) = delete;
        SettingsManager &operator=(const SettingsManager &) = delete;

        inline thread_local static bool force_defaults;
        struct ForceDefaultScope
        {
            ForceDefaultScope() { force_defaults = true; }
            ~ForceDefaultScope() { force_defaults = false; }
        };

        template <typename T>
        T LoadSettingOrDefault(const std::string &key, T default_value)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            ++m_active_settings;
            if (force_defaults)
                return default_value;
            auto it = m_settings.find(key);
            if (it != m_settings.end())
            {
                std::istringstream ss(it->second);
                T value{};
                if ((ss >> value) && !ss.fail())
                {
                    return value;
                }
            }
            return default_value;
        }

        template <typename T>
        void StoreSetting(const std::string &key, T value)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            assert(m_active_settings > 0 && "StoreSetting called without LoadSettingOrDefault");
            --m_active_settings;

            std::ostringstream oss;
            oss << value;
            m_settings[key] = oss.str();
        }

        std::unordered_map<std::string, std::string> m_settings;
        size_t m_active_settings = 0;
        mutable std::recursive_mutex m_mutex;
    };

    template <typename T>
    struct Setting
    {
        Setting(const std::string &key, const T &default_value)
            : m_key(key), value(SettingsManager::Get().LoadSettingOrDefault(key, default_value)) {}

        ~Setting()
        {
            SettingsManager::Get().StoreSetting(m_key, value);
        }

        // Disable copy
        Setting(const Setting &) = delete;
        Setting &operator=(const Setting &) = delete;

        struct ChangeChecker
        {
            T last_value;
            bool Changed(const Setting<T> &setting)
            {
                if (setting.value != last_value)
                {
                    last_value = setting.value;
                    return true;
                }
                return false;
            }
        };

        ChangeChecker GetChangeChecker() { return ChangeChecker{value}; }

        T value;

    private:
        std::string m_key;

        friend struct Subscription;

        std::string SerializeValue() const
        {
            std::ostringstream ss;
            ss << value;
            return ss.str();
        }
    };

    struct Settings
    {
        struct General
        {
            Setting<bool> scroll_snap_to_item{"general.scroll_snap_to_item", true};
            Setting<float> main_menu_fadeout_seconds{"general.main_menu_fadeout_seconds", 0.33f};
        };
        struct Style
        {
            enum ColorTheme : int
            {
                ImGuiDefault,
                ImGuiRedshifted,
                GuildWars,
                COUNT,
            };
            Setting<std::underlying_type_t<ColorTheme>> color_theme{"style.color_theme", (std::underlying_type_t<ColorTheme>)ColorTheme::GuildWars};
            Setting<float> hue_shift{"style.hue_shift", 0.0f};
            Setting<float> saturation_shift{"style.saturation_shift", 0.f};
            Setting<float> lightness_shift{"style.lightness_shift", 0.f};
            Setting<int> roundness{"style.roundness", 4};
            // clang-format off
            // Setting<ImVec4> base_tint       {"style.base_tint"       , ImGui::ColorConvertU32ToFloat4(Constants::GWColors::window_grey    )};
            // Setting<ImVec4> button_tint     {"style.button_tint"     , ImGui::ColorConvertU32ToFloat4(Constants::GWColors::button_blue    )};
            // Setting<ImVec4> tab_tint        {"style.tab_tint"        , ImGui::ColorConvertU32ToFloat4(Constants::GWColors::tabs_blue      )};
            // Setting<ImVec4> header_tint     {"style.header_tint"     , ImGui::ColorConvertU32ToFloat4(Constants::GWColors::header_beige   )};
            // Setting<ImVec4> checkmark_color {"style.checkmark_color" , ImGui::ColorConvertU32ToFloat4(Constants::GWColors::checkmark_beige)};
            // Setting<ImVec4> checkbox_color  {"style.checkbox_color"  , ImGui::ColorConvertU32ToFloat4(Constants::GWColors::checkbox_blue  )};
            // Setting<ImVec4> background_color{"style.background_color", ImVec4(0.06f, 0.06f, 0.06f, 0.92f)};
            // clang-format on
        };
        struct SkillBook
        {
            Setting<bool> show_help_button{"skill_book.show_help_button", true};
            Setting<bool> show_focused_character{"skill_book.show_focused_character", true};
            enum struct FeedbackSetting : int
            {
                Hidden,
                Concise,
                Detailed,
            };
            Setting<int> feedback{"skill_book.feedback", (int)FeedbackSetting::Concise};
        };
        General general;
        Style style;
        SkillBook skill_book;

        static void Draw(IDirect3DDevice9 *device);
    };
    struct SettingsGuard
    {
        SettingsGuard();
        ~SettingsGuard();
        Settings &Access();
    };
}