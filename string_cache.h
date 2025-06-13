#pragma once

#include <array>
#include <functional>
#include <string>
#include <unordered_map>

namespace HerosInsight
{
    template <size_t N>
    class StringCache
    {
        struct StringView
        {
            uint16_t offset;
            uint16_t length;

            std::string_view view(std::string &data) const
            {
                return std::string_view(&data[offset], length);
            }
        };

        struct Hasher
        {
            size_t operator()(const StringView &sv) const
            {
                return std::hash<std::string_view>{}(sv.view(data));
            }

            const std::string &data;
        };

        struct Eq
        {
            bool operator()(const StringView &a, const StringView &b) const
            {
                return a.view(data) == b.view(data);
            }

            const std::string &data;
        };

        using builderFN = std::function<void(size_t id, std::string &data)>;

        builderFN builder;
        std::string data;
        std::unordered_map<StringView, uint16_t, Hasher, Eq> deduper;
        std::array<StringView, N> ids_to_strings;

        StringCache(builderFN builder) : builder(builder), deduper(Hasher{data}, Eq{data})
        {
            deduper.reserve(N);
        }

        void Build(size_t id)
        {
            auto &sv = ids_to_strings[id];
            auto size_before = data.size();
            builder(id, data);
            sv.offset = size_before;
            sv.length = data.size() - size_before;
            auto it = deduper.find(sv);
            if (it == deduper.end())
            {
                deduper.emplace(sv, size_before);
                data.append('\0');
            }
            else
            {
                sv.offset = it->second;
                data.resize(size_before);
            }
        }

        void Rebuild()
        {
            data.clear();
            deduper.clear();
            for (size_t i = 0; i < N; ++i)
            {
                Build(i);
            }
        }

        std::string_view Get(size_t id)
        {
            auto &sv = ids_to_strings[id];
            return sv.view(data);
        }
    };
}