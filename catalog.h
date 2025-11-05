#pragma once

#include <bitset>
#include <front_back_pair.h>
#include <matcher.h>
#include <optional>
#include <set>
#include <string_arena.h>
#include <utils.h>

namespace HerosInsight
{
    namespace CatalogUtils
    {
        // Takes a span of highlight starts and stops, sorts them and removes any bad openings and closings
        // Modifies the size of the span to account for the removed values
        void SortHighlighting(std::span<uint16_t> &hl);
        void SortHighlighting(std::vector<uint16_t> &hl);

        struct HLData
        {
            using InstanceHLData = std::vector<uint16_t>;
            using PropHLData = std::vector<InstanceHLData>;

            std::vector<PropHLData> props;
            std::vector<InstanceHLData> meta_props;

            HLData(size_t prop_count, size_t meta_prop_count, size_t instance_count) : props(prop_count), meta_props(meta_prop_count)
            {
                for (auto &prop_hl : props)
                {
                    prop_hl.resize(instance_count);
                }
            }

            void Reset()
            {
                for (auto &prop : props)
                {
                    for (auto &hl : prop)
                    {
                        hl.clear();
                    }
                }

                for (auto &hl : meta_props)
                {
                    hl.clear();
                }
            }

            InstanceHLData &GetPropHL(size_t prop_id, size_t span_id)
            {
                return props[prop_id][span_id];
            }

            InstanceHLData &GetMetaPropHL(size_t meta_prop_id)
            {
                return meta_props[meta_prop_id];
            }
        };

        std::optional<size_t> BestMatch(std::string_view subject, StringArena<char> &targets);

        struct Filter
        {
            std::optional<size_t> meta_prop_id;
            std::string_view text;
            Matcher matcher;
        };

        bool ParseFilter(std::string_view source, StringArena<char> &meta_prop_names, Filter &filter);

        struct SortCommand
        {
            struct Arg
            {
                size_t target_meta_prop_id;
                bool is_negated;
            };

            std::vector<Arg> args;
        };

        using Command = std::variant<SortCommand>;

        bool TryReadCommand(std::string_view &remaining, Command &command);

        struct Query
        {
            std::vector<Filter> filters;
            std::vector<Command> commands;

            void Clear()
            {
                filters.clear();
                commands.clear();
            }
        };

        void ParseQuery(std::string_view source, StringArena<char> &meta_prop_names, Query &query);

        void GetFeedback(Query &query, StringArena<char> &meta_prop_names, std::string &out);
    }

    template <typename T_prop_id, size_t N_entries>
    class Catalog
    {
    public:
        using T_index = uint16_t;
        constexpr static size_t PROP_COUNT = static_cast<size_t>(T_prop_id::COUNT);
        using T_propset = std::bitset<PROP_COUNT + 1>; // + 1 for meta properties
        constexpr static T_propset ALL_PROPS = ~T_propset(0);

        static T_propset MakePropset(std::initializer_list<T_prop_id> props)
        {
            T_propset result;
            for (auto prop : props)
            {
                result.set(static_cast<size_t>(prop));
            }
            return result;
        }

        std::array<IndexedStringArena<char> *, PROP_COUNT> props{nullptr};
        StringArena<char> &meta_prop_names;
        std::span<T_propset> meta_propsets;

        Catalog(StringArena<char> &meta_prop_names, std::span<T_propset> meta_props)
            : meta_prop_names(meta_prop_names), meta_propsets(meta_props) {}

        IndexedStringArena<char> *GetPropertyPtr(T_prop_id prop_id)
        {
            auto ptr = props[static_cast<size_t>(prop_id)];
            return ptr;
        }

        void SetPropertyPtr(T_prop_id prop_id, IndexedStringArena<char> *ptr)
        {
            props[static_cast<size_t>(prop_id)] = ptr;
        }

        void RunFilters(std::span<CatalogUtils::Filter> filters, std::vector<T_index> &indices, CatalogUtils::HLData &hl_data)
        {
            T_propset collision_mask;
            T_propset footprint_mask;
            for (auto &filter : filters)
            {
                auto prop_mask = filter.meta_prop_id.has_value() ? meta_propsets[filter.meta_prop_id.value()] : ALL_PROPS;
                collision_mask |= footprint_mask & prop_mask;
                footprint_mask |= prop_mask;

                std::bitset<N_entries> found_by_filter;

                for (Utils::BitsetIterator it(prop_mask); !it.IsDone(); it.Next())
                {
                    auto i_prop = it.index;

                    bool is_meta = i_prop == PROP_COUNT;
                    if (is_meta) // Special case for meta properties
                    {
                        // Iterate the meta properties and check for name matches
                        for (size_t i_meta = 0; i_meta < meta_propsets.size(); ++i_meta)
                        {
                            auto str = std::string_view(meta_prop_names.Get(i_meta));
                            auto &hl = hl_data.GetMetaPropHL(i_meta);
                            bool is_match = filter.matcher.Matches(str, &hl);
                            if (!is_match)
                                continue;

                            auto &meta_propset = meta_propsets[i_meta];
                            // Iterate the indices and check which ones "has values" in this meta property
                            for (auto index : indices)
                            {
                                for (Utils::BitsetIterator it(meta_propset); !it.IsDone(); it.Next())
                                {
                                    auto i_prop = it.index;
                                    auto prop_ptr = props[i_prop];
                                    if (prop_ptr == nullptr)
                                        continue;
                                    auto &prop = *prop_ptr;
                                    auto span_id_opt = prop.GetSpanId(index);
                                    if (span_id_opt.has_value())
                                    {
                                        found_by_filter[index] = true;
                                        break;
                                    }
                                }
                            }
                        }
                        continue;
                    }

                    auto prop_ptr = props[i_prop];
                    if (prop_ptr == nullptr)
                        continue;
                    auto &prop = *prop_ptr;

                    std::bitset<N_entries> span_ids; // Worst case size

                    for (auto index : indices)
                    {
                        auto span_id_opt = prop.GetSpanId(index);
                        if (!span_id_opt.has_value())
                            continue;
                        span_ids[span_id_opt.value()] = true;
                    }
                    const auto unique_count = prop.SpanCount();
                    span_ids[unique_count] = true; // Prevents it.Next() from running all the way to the end.
                    for (Utils::BitsetIterator it(span_ids); it.index < unique_count; it.Next())
                    {
                        auto span_id = it.index;
                        auto str = std::string_view(prop.Get(span_id));
                        auto &hl = hl_data.GetPropHL(i_prop, span_id);
                        bool is_match = filter.matcher.Matches(str, &hl);

                        if (!is_match)
                            span_ids[span_id] = false;
                    }
                    for (auto index : indices)
                    {
                        auto span_id_opt = prop.GetSpanId(index);
                        if (!span_id_opt.has_value())
                            continue;
                        bool is_match = span_ids[span_id_opt.value()];
                        if (!is_match)
                            continue;

                        found_by_filter[index] = true;
                    }
                }

                size_t i = 0;
                for (auto index : indices)
                {
                    if (found_by_filter[index])
                        indices[i++] = index;
                }
                indices.resize(i);
            }

            // For cases when multiple filters ran on same property, the hl spans may not be in order, so we need to sort them.
            for (Utils::BitsetIterator it(collision_mask); !it.IsDone(); it.Next())
            {
                auto i_prop = it.index;
                auto prop_ptr = props[i_prop];
                if (prop_ptr == nullptr)
                    continue;
                auto &prop = *prop_ptr;

                std::bitset<N_entries> span_ids; // Worst case size

                for (auto index : indices)
                {
                    auto span_id_opt = prop.GetSpanId(index);
                    if (!span_id_opt.has_value())
                        continue;
                    span_ids[span_id_opt.value()] = true;
                }
                const auto unique_count = prop.SpanCount();
                span_ids[unique_count] = true; // Prevents it.Next() from running all the way to the end.
                for (Utils::BitsetIterator it(span_ids); it.index < unique_count; it.Next())
                {
                    auto span_id = it.index;
                    auto &hl = hl_data.GetPropHL(i_prop, span_id);
                    CatalogUtils::SortHighlighting(hl);
                }
            }
        }

        void RunQuery(CatalogUtils::Query &query, std::vector<T_index> &indices, CatalogUtils::HLData &hl_data)
        {
            RunFilters(query.filters, indices, hl_data);
        }
    };
}