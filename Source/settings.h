#pragma once

#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

struct IDirect3DDevice9;

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
            m_settings[key] = std::to_string(value);
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
        SkillBook skill_book;

        static void Draw(IDirect3DDevice9 *device);
    };
    struct SettingsGuard
    {
        SettingsGuard() { mutex.lock(); }
        ~SettingsGuard() { mutex.unlock(); }
        Settings &Access() { return settings; }

    private:
        inline static std::recursive_mutex mutex;
        inline static Settings settings;
    };
}