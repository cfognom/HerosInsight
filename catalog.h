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
            std::vector<InstanceHLData> bundles;

            HLData(size_t prop_count, size_t bundle_count, size_t instance_count) : props(prop_count), bundles(bundle_count)
            {
                for (auto &prop : props)
                {
                    prop = PropHLData(instance_count);
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

                for (auto &hl : bundles)
                {
                    hl.clear();
                }
            }

            InstanceHLData &GetPropHL(size_t prop_id, size_t span_id)
            {
                return props[prop_id][span_id];
            }

            InstanceHLData &GetBundleHL(size_t bundle_id)
            {
                return bundles[bundle_id];
            }
        };

        std::optional<size_t> BestMatch(std::string_view subject, StringArena<char> &targets);

        struct Filter
        {
            std::optional<size_t> bundle_id;
            std::string_view text;
            Matcher matcher;
        };

        bool ParseFilter(std::string_view source, StringArena<char> &prop_bundles, Filter &filter);

        struct SortCommand
        {
            struct Arg
            {
                size_t target_bundle;
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

        void ParseQuery(std::string_view source, StringArena<char> &prop_bundles, Query &query);

        void GetFeedback(Query &query, StringArena<char> &prop_bundle_names, std::string &out);
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

        IndexedStringArena<char> *props[PROP_COUNT]{};
        StringArena<char> &prop_bundle_names;
        std::span<T_propset> prop_bundles;

        Catalog(StringArena<char> &prop_bundle_names, std::span<T_propset> prop_bundles)
            : prop_bundle_names(prop_bundle_names), prop_bundles(prop_bundles) {}

        IndexedStringArena<char> *GetPropertyPtr(T_prop_id prop_id)
        {
            auto ptr = props[static_cast<size_t>(prop_id)];
            assert(ptr != nullptr);
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
                auto prop_mask = filter.bundle_id.has_value() ? prop_bundles[filter.bundle_id.value()] : ALL_PROPS;
                collision_mask |= footprint_mask & prop_mask;
                footprint_mask |= prop_mask;

                std::bitset<N_entries> found_by_filter;

                for (auto m = prop_mask; m.any(); Utils::ClearLowestSetBit(m))
                {
                    auto i_prop = Utils::CountTrailingZeros(m); // Get index of lowest set bit

                    bool is_meta = i_prop == PROP_COUNT;
                    if (is_meta) // Special case for meta properties
                    {
                        for (size_t i_bundle = 0; i_bundle < prop_bundle_names.SpanCount(); ++i_bundle)
                        {
                            auto str = prop_bundle_names.Get(i_bundle);
                            auto &hl = hl_data.GetBundleHL(i_bundle);
                            bool is_match = filter.matcher.Matches(str, &hl);
                            if (is_match)
                            {
                                // Add any entry that has properties in this bundle to the found_by_filter set
                                for (auto index : indices)
                                {
                                    for (auto m = prop_bundles[i_bundle]; m.any(); Utils::ClearLowestSetBit(m))
                                    {
                                        auto i_prop = Utils::CountTrailingZeros(m);
                                        auto prop_ptr = props[i_prop];
                                        if (prop_ptr == nullptr)
                                            continue;
                                        auto &prop = *prop_ptr;
                                        if (prop.GetSpanId(index) != prop.NULL_SPAN_ID)
                                        {
                                            found_by_filter.set(index);
                                            break;
                                        }
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
                        auto span_id = prop.GetSpanId(index);
                        span_ids[span_id] = true;
                    }
                    const auto unique_count = prop.SpanCount();
                    for (size_t span_id = 0; span_id < unique_count; ++span_id)
                    {
                        if (!span_ids[span_id])
                            continue;

                        auto str = prop.Get(span_id);
                        auto &hl = hl_data.GetPropHL(i_prop, span_id);
                        bool is_match = filter.matcher.Matches(str, &hl);

                        if (!is_match)
                            span_ids[span_id] = false;
                    }
                    for (auto index : indices)
                    {
                        auto span_id = prop.GetSpanId(index);
                        bool is_match = span_ids[span_id];
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

            // For cases where when multiple filters ran on same property, the hl spans may not be in order, so we need to sort them.
            for (auto m = collision_mask; m.any(); Utils::ClearLowestSetBit(m))
            {
                auto i_prop = Utils::CountTrailingZeros(m); // Get index of lowest set bit
                auto &prop = *props[i_prop];

                std::bitset<N_entries> span_ids; // Worst case size

                for (auto index : indices)
                {
                    auto span_id = prop.GetSpanId(index);
                    span_ids[span_id] = true;
                }
                const auto unique_count = prop.SpanCount();
                for (size_t span_id = 0; span_id < unique_count; ++span_id)
                {
                    if (!span_ids[span_id])
                        continue;

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