#pragma once

#include <atomic>
#include <functional>

namespace HerosInsight::Utils
{
    // Must be placed in a static memory location
    struct InitGuard
    {
        using Store = uint32_t;
        constexpr static uint32_t KEY_MAX = sizeof(Store) * 8 / 2 - 1;

        std::atomic<Store> status{0};
        static_assert(std::atomic<Store>::is_always_lock_free);

        bool IsInit(uint32_t key = 0)
        {
            assert(key <= KEY_MAX);
            return (status.load() >> key) & 1;
        }

        // Attempt to begin initialization, this can only ever happen once, even across threads
        bool TryBeginInit(uint32_t key = 0)
        {
            assert(key < KEY_MAX);
            auto begun_mask = 1 << (KEY_MAX + key);
            auto before = status.fetch_or(begun_mask);
            return (before & begun_mask) == 0;
        }

        // Mark the initialization as complete
        void FinishInit(uint32_t key = 0)
        {
            assert(key <= KEY_MAX);
            status.fetch_or(1 << key);
        }
    };
}
