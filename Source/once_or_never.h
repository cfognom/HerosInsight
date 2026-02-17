#pragma once

#include <atomic>
#include <thread>

// Threadsafe wrapper for T, ensuring that it may be constructed only once before its destruction
template <typename T>
struct OnceOrNever
{
    struct construction_failed // Throw this in the constructor to indicate a failed construction. Then state will be State::Terminal afterwards.
    {
    };

    enum struct State
    {
        Initial,
        Constructing,
        Constructed,
        Terminal,
    };

    std::atomic<State> state = State::Initial;
    alignas(alignof(T)) char obj_storage[sizeof(T)];

    template <typename... Args>
    bool ConstructOnce(Args &&...args)
    {
        State expected = State::Initial;
        if (state.compare_exchange_strong(expected, State::Constructing))
        {
            try
            {
                new (obj_storage) T(std::forward<Args>(args)...);
                state.store(State::Constructed);
                return true;
            }
            catch (const construction_failed &e)
            {
                state.store(State::Terminal);
            }
        }
        return false;
    }

    bool IsConstructed() const { return state.load() == State::Constructed; }

    bool Terminate()
    {
        while (true)
        {
            auto actual = state.load();
            switch (actual)
            {
                case State::Constructing:
                    // The object is being constructed, yield and try again
                    std::this_thread::yield();
                    continue;

                case State::Constructed:
                    if (state.compare_exchange_strong(actual, State::Terminal))
                    {
                        reinterpret_cast<T *>(obj_storage)->~T();
                        return true;
                    }
                    continue;

                case State::Terminal:
                    return false;

                default:
                    if (state.compare_exchange_strong(actual, State::Terminal))
                    {
                        return false;
                    }
                    continue;
            }
        }
    }
    ~OnceOrNever()
    {
        Terminate();
    }
};