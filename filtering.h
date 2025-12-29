#pragma once

#include <optional>
#include <set>
#include <span>

#include <bitview.h>
#include <front_back_pair.h>
#include <matcher.h>
#include <multibuffer.h>
#include <stopwatch.h>
#include <string_arena.h>
#include <utils.h>

namespace HerosInsight::Filtering
{
    // Takes a span of highlight starts and stops, sorts them and removes any bad openings and closings
    // Modifies the size of the span to account for the removed values
    void SortHighlighting(std::span<uint16_t> &hl);
    void SortHighlighting(std::vector<uint16_t> &hl);

    struct Filter
    {
        size_t meta_prop_id = 0;
        std::string_view source_text;
        std::string_view filter_text;
        Matcher matcher;
        bool inverted;
    };
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

    struct Query
    {
        std::vector<Filter> filters;
        std::vector<Command> commands;

        void clear()
        {
            filters.clear();
            commands.clear();
        }
    };

    struct ResultItem
    {
        std::string text;
        std::vector<uint16_t> hl; // Highlight
    };

    template <typename I>
    concept DeviceImpl =
        requires {
            typename I::index_type;
        } &&
        requires(I &p, size_t prop, size_t meta, size_t item, size_t span) {
            // dataset shape
            { p.MaxSpanCount() } -> std::convertible_to<size_t>;
            { p.PropCount() } -> std::convertible_to<size_t>;
            { p.MetaCount() } -> std::convertible_to<size_t>;

            // meta
            { p.GetMetaName(meta) } -> std::same_as<LoweredText>;
            { p.GetMetaPropset(meta) } -> std::same_as<BitView>;

            // props
            { p.GetItemToSpan(prop) } -> std::same_as<std::span<typename I::index_type>>;
            { p.GetProperty(prop) } -> std::same_as<LoweredTextVector &>;
        };

    template <DeviceImpl I>
    struct Device
    {
        I &impl;

        bool ParseFilter(std::string_view source, Filter &filter)
        {
            filter.source_text = source;
            Utils::TryReadSpaces(source);

            auto splitter_pos = source.find(':');
            if (splitter_pos != std::string_view::npos)
            {
                auto target_str = source.substr(0, splitter_pos);
                if (!target_str.empty())
                {
                    auto index = BestMatch(target_str);
                    if (!index.has_value())
                        return false;
                    filter.meta_prop_id = index.value();
                }

                size_t i = splitter_pos + 1;
                while (i < source.size() && Utils::IsSpace(source[i]))
                    ++i;

                source = source.substr(i);
            }

            Utils::TryReadSpaces(source);
            filter.inverted = Utils::TryRead('!', source);
            Utils::TryReadSpaces(source);

            filter.filter_text = source;
            filter.matcher = Matcher(source);

            return true;
        }
        bool TryReadCommand(std::string_view &remaining, Command &command)
        {
            auto rem = remaining;

            Utils::ReadWhitespace(rem);

            if (!Utils::TryRead('/', rem))
                return false;

            if (Utils::TryRead("SORT", rem))
            {
                auto &sort_com = command.emplace<SortCommand>();
                while (rem.size())
                {
                    if (!Utils::ReadWhitespace(rem))
                        break;
                    sort_com.args.emplace_back();
                    auto &sort_arg = sort_com.args.back();
                    bool is_negated = Utils::TryRead('-', rem);
                    auto comma_pos = rem.find(',');
                    auto target_text = rem.substr(0, comma_pos);
                    auto index = BestMatch(target_text);
                    if (!index.has_value())
                        return false;
                    sort_arg.target_meta_prop_id = index.value();

                    rem = rem.substr(comma_pos + 1);
                }
            }
            else
            {
                return false;
            }

            return true;
        }
        void ParseQuery(std::string_view source, Query &query)
        {
            query.clear();

            char *p = (char *)source.data();
            char *end = p + source.size();

            while (p < end)
            {
                auto stmt_start = p;
                while (p < end && *p != '&')
                    ++p;
                auto stmt_end = p;
                std::string_view stmt(stmt_start, stmt_end - stmt_start);

                if (*stmt_start == '/')
                {
                    query.commands.emplace_back();
                    auto &command = query.commands.back();
                    if (!TryReadCommand(stmt, command))
                        query.commands.pop_back();
                }
                else
                {
                    query.filters.emplace_back();
                    auto &filter = query.filters.back();
                    if (!ParseFilter(stmt, filter))
                        query.filters.pop_back();
                }
                ++p;
            }
        }
        void RunFilters(std::span<Filter> filters, std::vector<typename I::index_type> &items)
        {
#ifdef _TIMING
            Stopwatch stopwatch("RunFilters");
#endif
            size_t n_filters = filters.size();
            size_t n_items = items.size();
            size_t n_spans = impl.MaxSpanCount();
            size_t n_props = impl.PropCount();
            size_t n_meta = impl.MetaCount();

            // clang-format off
                //   NAME                            TYPE               COUNT
                auto items_bits = MultiBuffer::Spec< BitView::word_t >( BitView::CalcWordCount(n_items) );
                auto span_bits  = MultiBuffer::Spec< BitView::word_t >( BitView::CalcWordCount(n_spans) );
            // clang-format on
            auto allocation = MultiBuffer::HeapAllocated(
                items_bits,
                span_bits
            );

            stopwatch.Checkpoint("init");
            for (auto &filter : filters)
            {
                // std::string_view meta_prop_name = GetMetaName(filter.meta_prop_id);
                BitView propset = impl.GetMetaPropset(filter.meta_prop_id);

                // std::bitset<N_entries> matched_items;
                BitView unconfirmed_match(items_bits.ptr, items.size(), true);

                for (auto prop_id : propset.IterSetBits())
                {
                    bool is_meta = prop_id == n_props;
                    if (is_meta) // Special case for meta properties
                    {
                        // Iterate the meta properties and check for name matches
                        for (size_t i_meta = 0; i_meta < n_meta; ++i_meta)
                        {
                            LoweredText str = impl.GetMetaName(i_meta);
                            if (!filter.matcher.Match(str, 0))
                                continue;

                            BitView meta_propset = impl.GetMetaPropset(i_meta);
                            // Iterate the items and check which ones "has values" in this meta property
                            for (auto index : unconfirmed_match.IterSetBits())
                            {
                                auto item = items[index];
                                for (auto i_prop : meta_propset.IterSetBits())
                                {
                                    auto &prop = impl.GetProperty(i_prop);
                                    auto span_id = impl.GetItemToSpan(i_prop)[item];
                                    auto str = prop.Get(span_id);
                                    if (!str.text.empty())
                                    {
                                        unconfirmed_match[index] = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else // Non-meta
                    {
                        LoweredTextVector &prop = impl.GetProperty(prop_id);
                        auto item_to_span = impl.GetItemToSpan(prop_id);

                        // std::bitset<N_entries> span_ids; // Worst case size
                        const auto unique_count = prop.arena.SpanCount();
                        if (unique_count == 0) // Todo: remove
                            continue;
                        BitView marked_spans(span_bits.ptr, unique_count, false);

                        // Iterate the unconfirmed items and mark which spans we use
                        for (auto index : unconfirmed_match.IterSetBits())
                        {
                            auto span_id = item_to_span[items[index]];
                            marked_spans[span_id] = true;
                        }
                        stopwatch.Checkpoint(std::format("prop_{} marking", prop_id));

                        // Iterate marked spans and unmark those that don't match
                        for (auto span_id : marked_spans.IterSetBits())
                        {
                            LoweredText str = prop.Get(span_id);

                            if (!filter.matcher.Match(str, 0))
                                marked_spans[span_id] = false;
                        }
                        stopwatch.Checkpoint(std::format("prop_{} matching", prop_id));

                        // Iterate the items and record which ones' spans matched
                        for (auto index : unconfirmed_match.IterSetBits())
                        {
                            auto span_id = item_to_span[items[index]];
                            bool is_match = marked_spans[span_id];
                            if (is_match)
                                unconfirmed_match[index] = false;
                        }
                        stopwatch.Checkpoint(std::format("prop_{} checking", prop_id));
                    }
                    if (!unconfirmed_match.Any())
                        goto next_filter;
                }

                {
                    // Iterate the matched items and repopulate the items vector with them
                    size_t new_items_size = 0;
                    if (!filter.inverted)
                        unconfirmed_match.Flip(); // It becomes "confirmed match"
                    for (auto index : unconfirmed_match.IterSetBits())
                    {
                        items[new_items_size++] = items[index];
                    }
                    items.resize(new_items_size);
                }
            next_filter:
                stopwatch.Checkpoint("filter");
            }
            stopwatch.Checkpoint("filters");
        }
        void RunQuery(Query &query, std::vector<typename I::index_type> &items)
        {
            if (!query.filters.empty())
                RunFilters(query.filters, items);
        }
        void GetFeedback(Query &query, std::string &out)
        {
            out.clear();
            auto inserter = std::back_inserter(out);
            for (auto &filter : query.filters)
            {
                auto meta_name = impl.GetMetaName(filter.meta_prop_id).GetPresentableString();
                auto cond = filter.inverted ? "not " : "";
                std::format_to(inserter, "{} must {}contain '{}'\n", meta_name, cond, filter.filter_text);
            }

            // for (auto &command : query.commands)
            // {
            //     if (std::holds_alternative<SortCommand>(command))
            //     {
            //         auto &sort_command = std::get<SortCommand>(command);

            //         const auto n_values = sort_command.args.size();

            //         out += "Sort";
            //         for (uint32_t i = 0; i < n_values; i++)
            //         {
            //             const auto &arg = sort_command.args[i];
            //             auto kind = i == 0             ? 0
            //                         : i < n_values - 1 ? 1
            //                                            : 2;

            //             // clang-format off
            //                 if (kind == 1)      out += ", then";
            //                 else if (kind == 2) out += " and then";
            //                 if (arg.is_negated) out += " descending by ";
            //                 else                out += " ascending by ";
            //                                     out += (std::string_view)prop_bundle_names[arg.target_meta_prop_id];
            //             // clang-format on
            //         }
            //     }
            //     out += "\n";
            // }
        }

        ResultItem CalcItemResult(Query &q, size_t prop_id, I::index_type item_id)
        {
            LoweredTextVector &prop = impl.GetProperty(prop_id);
            LoweredText lowered = prop.Get(impl.GetItemToSpan(prop_id)[item_id]);
            return CalcResult(q, prop_id, lowered);
        }

        ResultItem CalcMetaResult(Query &q, size_t meta)
        {
            LoweredText lowered = impl.GetMetaName(meta);
            auto meta_prop_id = impl.PropCount();
            return CalcResult(q, meta_prop_id, lowered);
        }

    private:
        inline ResultItem CalcResult(Query &q, size_t prop_id, LoweredText lowered)
        {
            ResultItem result;
            if (!lowered.text.empty())
            {
                for (auto &filter : q.filters)
                {
                    auto propset = impl.GetMetaPropset(filter.meta_prop_id);
                    if (propset[prop_id])
                    {
                        bool match = filter.matcher.Matches(lowered, result.hl);
                    }
                }
                SortHighlighting(result.hl);
                result.text = lowered.GetPresentableString();
            }
            return result;
        }

        // Returns the index of the "best" match
        inline std::optional<size_t> BestMatch(std::string_view subject)
        {
            size_t index;

            auto matcher = Matcher(subject);

            size_t best_match_cost = std::numeric_limits<size_t>::max();
            for (size_t i = 0; i < impl.MetaCount(); ++i)
            {
                auto target = impl.GetMetaName(i);
                if (matcher.Match(target, 0))
                {
                    float match_cost = target.text.size();
                    if (match_cost < best_match_cost)
                    {
                        best_match_cost = match_cost;
                        index = i;
                    }
                }
            }

            if (best_match_cost == std::numeric_limits<size_t>::max())
                return std::nullopt;

            return index;
        }
    };

}