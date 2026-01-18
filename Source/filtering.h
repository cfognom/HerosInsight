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
#include <string_cache.h>
#include <text_provider.h>
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
        FixedVector<char, 512> text;
        std::vector<uint16_t> hl; // Highlight
    };

    struct IncrementalProp
    {
        using BuildTemplateFn =
            Text::StringTemplateAtom (*)(
                OutBuf<Text::StringTemplateAtom>,
                size_t,
                void *
            );

        Text::StringManager &mgr = Text::s_Manager;
        LoweredTextVector searchable_text;
        StringArena<Text::StringTemplateAtom> stringTemplates;
        StringArena<Text::StringTemplateAtom>::deduper stringTemplates_deduper;
        std::vector<uint16_t> item_to_str;
        void *buildTemplate_data;
        BuildTemplateFn BuildTemplate_fn;

        size_t GetUniqueCount() const { return searchable_text.arena.SpanCount(); }

        void SetupIncremental(void *data, BuildTemplateFn fn)
        {
            buildTemplate_data = data;
            BuildTemplate_fn = fn;
            stringTemplates_deduper = stringTemplates.CreateDeduper(0);
        }

        void PopulateItems(IndexedStringArena<char> &src)
        {
            searchable_text = LoweredTextVector(src);
            item_to_str = src.index_to_id;
        }

        void PopulateItems(std::string_view hint_key, size_t count, auto &&itemPropertyGetter, bool dedupe = true)
        {
            auto &arena = searchable_text.arena;
            arena.clear();
            arena.ReserveFromHint(hint_key);
            item_to_str.clear();
            item_to_str.reserve(count);

            std::string deduper_hint_key;
            StringArena<char>::deduper deduper;
            if (dedupe)
            {
                deduper_hint_key = std::format("{}_deduper", hint_key);
                auto bucket_count = CapacityHints::GetHint(deduper_hint_key);
                deduper = arena.CreateDeduper(bucket_count);
            }
            auto p_deduper = dedupe ? &deduper : nullptr;

            auto skills = GW::SkillbarMgr::GetSkills();
            for (size_t i = 0; i < count; ++i)
            {
                itemPropertyGetter(arena, i);
                auto strId = arena.CommitWritten(p_deduper);
                item_to_str.push_back(strId);
            }
            searchable_text.LowercaseFold();

            if (dedupe)
            {
                CapacityHints::UpdateHint(deduper_hint_key, deduper.bucket_count());
            }
            arena.StoreCapacityHint(hint_key);
        }

        void MarkDirty()
        {
            searchable_text.arena.clear();
            stringTemplates.clear();
            stringTemplates_deduper.clear();
            item_to_str.clear();
        }

        Text::StringTemplate GetStringTemplate(size_t strId)
        {
            auto span = stringTemplates[strId];
            auto root = span.back();
            return Text::StringTemplate{
                .root = root,
                .rest = span
            };
        }

        size_t GetStrId(size_t itemId)
        {
            if (itemId >= item_to_str.size())
                item_to_str.resize(itemId + 1, std::numeric_limits<uint16_t>::max());
            auto &strId = item_to_str[itemId];
            if (strId == std::numeric_limits<uint16_t>::max())
            {
                stringTemplates.AppendWriteBuffer(
                    32,
                    [&](std::span<Text::StringTemplateAtom> &buffer)
                    {
                        SpanWriter<Text::StringTemplateAtom> writer(buffer);
                        OutBuf<Text::StringTemplateAtom> out(writer);
                        out.push_back(BuildTemplate_fn(out, itemId, buildTemplate_data));
                        buffer = writer.WrittenSpan();
                    }
                );
                strId = stringTemplates.CommitWritten(&stringTemplates_deduper);
                if (strId == searchable_text.arena.SpanCount())
                {
                    auto t = GetStringTemplate(strId);
                    if (t.root.header.type != Text::StringTemplateAtom::Type::Null)
                    {
                        searchable_text.arena.AppendWriteBuffer(
                            512,
                            [&](std::span<char> &buffer)
                            {
                                SpanWriter<char> writer(buffer);
                                OutBuf<char> out(writer);
                                mgr.AssembleSearchableString(t, out);
                                buffer = writer.WrittenSpan();
                            }
                        );
                    }
                    auto strId2 = searchable_text.CommitAndFold();
                    assert(strId == strId2);
                }
            }
            return strId;
        }

        LoweredText GetSearchableStr(size_t strId)
        {
            return searchable_text[strId];
        }

        void GetRenderableString(size_t strId, OutBuf<char> out)
        {
            if (stringTemplates.SpanCount() == 0) // Temp hack
            {
                auto text = GetSearchableStr(strId);
                text.GetRenderableString(out);
                return;
            }

            auto t = GetStringTemplate(strId);
            if (t.root.header.type != Text::StringTemplateAtom::Type::Null)
            {
                mgr.AssembleRenderableString(t, out);
                return;
            }
        }
    };

    template <typename P>
    concept Property =
        requires(P &p, size_t itemId, size_t strId) {
            { p.GetStrId(itemId) } -> std::same_as<size_t>;
            { p.GetStr(strId) } -> std::same_as<LoweredText>;
        };

    template <typename I>
    concept DeviceImpl =
        requires {
            typename I::index_type;
        } &&
        requires(I &d, size_t metaId, size_t propId) {
            // dataset shape
            { d.MaxSpanCount() } -> std::convertible_to<size_t>;
            { d.PropCount() } -> std::convertible_to<size_t>;
            { d.MetaCount() } -> std::convertible_to<size_t>;

            // meta
            { d.GetMetaName(metaId) } -> std::same_as<LoweredText>;
            { d.GetMetaPropset(metaId) } -> std::same_as<BitView>;

            // props
            { d.GetProperty(propId) } -> std::same_as<IncrementalProp &>;
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
            Stopwatch stopwatch("RunFilters");

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

                            // The filter matched against this meta name, now we need to find out which items "have it"

                            BitView meta_propset = impl.GetMetaPropset(i_meta);
                            for (auto prop_id : meta_propset.IterSetBits())
                            {
                                bool is_meta = prop_id == n_props;
                                if (is_meta) // Meta properties cannot be "had"
                                    continue;
                                auto &prop = impl.GetProperty(prop_id);
                                // Iterate the items and check which ones "has values" in this property
                                bool all_confirmed = true;
                                for (auto index : unconfirmed_match.IterSetBits())
                                {
                                    auto item = items[index];
                                    auto str_id = prop.GetStrId(item);
                                    auto str = prop.GetSearchableStr(str_id);
                                    if (!str.text.empty())
                                    {
                                        unconfirmed_match[index] = false;
                                        continue;
                                    }
                                    all_confirmed = false;
                                }
                                if (all_confirmed)
                                    goto next_filter;
                            }
                        }
                    }
                    else // Non-meta
                    {
                        auto &prop = impl.GetProperty(prop_id);

                        // if (unique_count == 0) // Todo: remove
                        //     continue;
                        BitView marked_spans(span_bits.ptr, n_spans, false);

                        // Iterate the unconfirmed items and mark which spans we use
                        for (auto index : unconfirmed_match.IterSetBits())
                        {
                            auto item = items[index];
                            auto span_id = prop.GetStrId(item);
                            marked_spans[span_id] = true;
                        }
                        const auto unique_count = prop.GetUniqueCount();
                        marked_spans = marked_spans.Subview(0, unique_count);
                        stopwatch.Checkpoint(std::format("prop_{} marking", prop_id));

                        // Iterate marked spans and unmark those that don't match
                        for (auto span_id : marked_spans.IterSetBits())
                        {
                            auto str = prop.GetSearchableStr(span_id);

                            if (!filter.matcher.Match(str, 0))
                                marked_spans[span_id] = false;
                        }
                        stopwatch.Checkpoint(std::format("prop_{} matching", prop_id));

                        // Iterate the items and record which ones' spans matched
                        bool all_confirmed = true;
                        for (auto index : unconfirmed_match.IterSetBits())
                        {
                            auto item = items[index];
                            auto span_id = prop.GetStrId(item);
                            bool is_match = marked_spans[span_id];
                            if (is_match)
                            {
                                unconfirmed_match[index] = false;
                                continue;
                            }
                            all_confirmed = false;
                        }
                        stopwatch.Checkpoint(std::format("prop_{} checking", prop_id));
                        if (all_confirmed)
                            goto next_filter;
                    }
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
                FixedVector<char, 128> meta_name;
                impl.GetMetaName(filter.meta_prop_id).GetRenderableString(meta_name);
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
            auto &prop = impl.GetProperty(prop_id);
            auto str_id = prop.GetStrId(item_id);
            LoweredText lowered = prop.GetSearchableStr(str_id);
            ResultItem result;
            CalcHL(q, prop_id, lowered, result.hl);
            prop.GetRenderableString(str_id, result.text);
            return result;
        }

        ResultItem CalcMetaResult(Query &q, size_t meta)
        {
            LoweredText lowered = impl.GetMetaName(meta);
            auto meta_prop_id = impl.PropCount();
            ResultItem result;
            CalcHL(q, meta_prop_id, lowered, result.hl);
            lowered.GetRenderableString(result.text);
            return result;
        }

    private:
        inline void CalcHL(Query &q, size_t prop_id, LoweredText lowered, std::vector<uint16_t> &hl)
        {
            if (!lowered.text.empty())
            {
                for (auto &filter : q.filters)
                {
                    auto propset = impl.GetMetaPropset(filter.meta_prop_id);
                    if (propset[prop_id])
                    {
                        bool match = filter.matcher.Matches(lowered, hl);
                    }
                }
                SortHighlighting(hl);
            }
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