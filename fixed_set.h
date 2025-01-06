#pragma once

namespace HerosInsight
{
    // FixedSet is a set with a fixed number of elements (N)
    // It uses linear probing to resolve collisions
    // The set is implemented as a buffer of size N
    // The buffer is initialized with zeros
    template <typename T, size_t N>
    class FixedSet
    {
    public:
        constexpr FixedSet()
        {
        }

        // Variadic template constructor
        template <typename... Args>
        constexpr FixedSet(Args... args)
        {
            static_assert(sizeof...(args) <= N, "Too many arguments");
            constexpr auto load_factor = static_cast<float>(sizeof...(args)) / N;
            static_assert(load_factor <= 0.75f, "Bad load factor");
            for (const auto &arg : {args...})
            {
                insert(arg);
            }
        }

        constexpr bool insert(const T &element)
        {
            auto id = custom_hash(element);
            size_t index = id % N;
            size_t start_index = index;
            do
            {
                bool slot_is_empty = buffer[index] == (T)0;
#ifdef _DEBUG
                if (!slot_is_empty)
                    collision_count++;
#endif
                if (slot_is_empty || buffer[index] == element)
                {
                    buffer[index] = element;
                    return true;
                }
                index = (index + 1) % N;
            } while (index != start_index);
            return false; // Set is full
        }

        constexpr bool has(const T &element) const
        {
            auto id = custom_hash(element);
            size_t index = id % N;
            size_t start_index = index;
            do
            {
                if (buffer[index] == (T)0)
                {
                    return false;
                }
                if (buffer[index] == element)
                {
                    return true;
                }
                index = (index + 1) % N;
            } while (index != start_index);
            return false;
        }

    private:
        using IntType = typename std::conditional<
            sizeof(T) <= sizeof(uint8_t), uint8_t,
            typename std::conditional<
                sizeof(T) <= sizeof(uint16_t), uint16_t,
                typename std::conditional<
                    sizeof(T) <= sizeof(uint32_t), uint32_t,
                    uint64_t>::type>::type>::type;

        constexpr std::size_t custom_hash(const T &element) const
        {
            auto value = (IntType &)element;
            // A simple hash function (FNV-1a hash)
            std::size_t hash = 14695981039346656037ull;
            for (std::size_t i = 0; i < sizeof(value); ++i)
            {
                hash ^= (value >> (i * 8)) & 0xff;
                hash *= 1099511628211ull;
            }
            return hash;
        }

        T buffer[N] = {(T)0};
#ifdef _DEBUG
        uint32_t collision_count = 0;
#endif
    };

    // Factory function to deduce the size (N) from the number of arguments
    template <typename T, typename... Args>
    constexpr auto MakeFixedSet(Args... args)
    {
        // sizeof...(args) deduces the number of elements passed in the argument pack
        constexpr auto count = sizeof...(args);
        constexpr auto n = static_cast<size_t>(4 * count / 3 + 1);
        return FixedSet<T, n>(args...);
    }
}