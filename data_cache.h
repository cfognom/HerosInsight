#pragma once

#include <array>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>

namespace HerosInsight
{
    template <typename T, size_t N>
    class DataCache
    {
        constexpr static bool is_char = std::is_same_v<T, char>;
        using storage_t = std::conditional_t<is_char, std::string, std::vector<T>>;
        using span_t = std::conditional_t<is_char, std::string_view, std::span<T>>;
        using offset_int_t = uint32_t;
        using size_int_t = uint16_t;

        struct LocalSpan
        {
            offset_int_t offset;
            size_int_t size;

            auto resolve(const storage_t &data) const
            {
                return span_t(
                    (T *)(data.data() + offset),
                    static_cast<size_t>(size));
            }
        };

        struct Hasher
        {
            size_t operator()(const LocalSpan &ls) const
            {
                return std::hash<std::string_view>{}(std::string_view((const char *)(data.data() + ls.offset), ls.size * sizeof(T)));
            }

            const storage_t &data;
        };

        struct Eq
        {
            bool operator()(const LocalSpan &a, const LocalSpan &b) const
            {
                if constexpr (is_char)
                {
                    return a.resolve(data) == b.resolve(data);
                }
                else
                {
                    if (a.size != b.size)
                        return false;
                    auto as = a.resolve(data);
                    auto bs = b.resolve(data);
                    return std::equal(as.begin(), as.end(), bs.begin());
                }
            }

            const storage_t &data;
        };

        using builderFN_t = std::function<void(size_t id, storage_t &data)>;

        storage_t data;
        std::unordered_map<LocalSpan, offset_int_t, Hasher, Eq> deduper;
        std::array<LocalSpan, N> ids_to_span;

    public:
        DataCache(size_t storage_size = 1024) : deduper(N, Hasher{data}, Eq{data})
        {
            data.reserve(storage_size);
        }

        void TryUndoBuild(size_t id)
        {
            auto &ls = ids_to_span[id];
            if (ls.size == 0)
                return true;
            if (ls.offset + ls.size < data.size())
                return false;
            auto restore_offset = ls.offset;
            if constexpr (is_char)
            {
                --restore_offset;
            }
            data.resize(restore_offset);
            ls.size = 0;
            ls.offset = 0;
            return true;
        }

        void Build(size_t id, builderFN_t builder)
        {
            auto &ls = ids_to_span[id];
            const bool is_extension = ls.size > 0;
            if (is_extension)
            {
                deduper.erase(ls);
                assert(ls.offset + ls.size == data.size() && "Bad DataCache build order");
            }
            else
            {
                if constexpr (is_char)
                {
                    data.push_back('\0');
                }
                ls.offset = data.size();
            }
            auto rollback_offset = ls.offset;
            if constexpr (is_char)
            {
                --rollback_offset;
            }
#ifdef _DEBUG
            assert(data.size() <= std::numeric_limits<offset_int_t>::max() && "DataCache overflow.");
#endif
            auto append_offset = static_cast<offset_int_t>(data.size());
            auto data_ptr = data.data();
            builder(id, data);
#ifdef _DEBUG
            // assert(data.data() == data_ptr && "DataCache reallocated, increase storage size.");
#endif
            auto append_size = data.size() - append_offset;
            ls.size += append_size;

            auto it = deduper.find(ls);
            if (it == deduper.end())
            {
                deduper.emplace(ls, ls.offset);
            }
            else
            {
                ls.offset = it->second;
                data.resize(rollback_offset);
            }
        }

        void Clear()
        {
            data.clear();
            deduper.clear();
        }

        void Rebuild(builderFN_t builder)
        {
            Clear();
            for (size_t id = 0; id < N; ++id)
            {
                Build(id, builder);
            }
        }

        span_t Get(size_t id)
        {
            auto &ls = ids_to_span[id];
            return ls.resolve(data);
        }
    };
}