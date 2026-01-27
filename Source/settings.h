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

        template <typename T>
        T InitSetting(const std::string &key, T default_value)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            ++m_active_settings;
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
        void TermSetting(const std::string &key, T value)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            --m_active_settings;
            m_settings[key] = std::to_string(value);
        }

        std::unordered_map<std::string, std::string> m_settings;
        size_t m_active_settings = 0;
        mutable std::recursive_mutex m_mutex;
    };

    template <typename T>
    class ObservableSetting
    {
    public:
        using Callback = std::function<void(const T &)>;

        ObservableSetting(const std::string &key, const T &default_value)
            : m_key(key), m_value(SettingsManager::Get().InitSetting(key, default_value)) {}

        ~ObservableSetting()
        {
            SettingsManager::Get().TermSetting(m_key, m_value);
        }

        T Get() const
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            return m_value;
        }

        void Set(const T &new_value)
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            if (m_value != new_value)
            {
                m_value = new_value;
                Notify();
            }
        }

        struct Subscription
        {
            ObservableSetting &setting;
            size_t id;
            Subscription(ObservableSetting &setting, Callback cb)
                : setting(setting)
            {
                std::lock_guard<std::recursive_mutex> lock(setting.m_mutex);
                size_t id = setting.m_next_id++;
                setting.m_callbacks[id] = std::move(cb);
            }
            ~Subscription()
            {
                std::lock_guard<std::recursive_mutex> lock(setting.m_mutex);
                setting.m_callbacks.erase(id);
            }
        };

    private:
        std::string m_key;
        T m_value;
        std::unordered_map<size_t, Callback> m_callbacks;
        size_t m_next_id = 0;
        mutable std::recursive_mutex m_mutex;

        friend struct Subscription;

        void Notify()
        {
            for (auto &[_, cb] : m_callbacks)
            {
                cb(m_value);
            }
        }

        std::string SerializeValue() const
        {
            std::ostringstream ss;
            ss << m_value;
            return ss.str();
        }
    };

    struct Settings
    {
        struct General
        {
            ObservableSetting<bool> scroll_snap_to_item{"general.scroll_snap_to_item", true};

            void Draw(IDirect3DDevice9 *device);
        };
        struct SkillBook
        {
            enum struct FeedbackSetting : int
            {
                Hidden,
                Concise,
                Detailed,
            };
            ObservableSetting<int> feedback{"skill_book.feedback", (int)FeedbackSetting::Concise};

            void Draw(IDirect3DDevice9 *device);
        };
        General general;
        SkillBook skill_book;

        void Draw(IDirect3DDevice9 *device);
    };
    inline Settings g_settings;
}