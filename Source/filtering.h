#pragma once

#include <optional>
#include <set>
#include <span>

#include <bitview.h>
#include <front_back_pair.h>
#include <matcher.h>
#include <multibuffer.h>
#include <span_vector.h>
#include <stopwatch.h>
#include <string_manager.h>
#include <text_provider.h>
#include <utils.h>

namespace HerosInsight::Filtering
{
    // Takes a span of highlight starts and stops, sorts them and removes any bad openings and closings
    // Modifies the size of the span to account for the removed values
    void SortHighlighting(std::span<uint16_t> &hl);
    void ConnectHighlighting(std::string_view text, std::span<uint16_t> &hl);
    std::strong_ordering CompareSubstrs(std::span<uint16_t> asubs, std::string_view astr, std::span<uint16_t> bsubs, std::string_view bstr);

    // Returns the index of the "best" match
    template <typename TextGetter>
    std::optional<size_t> BestMatch(std::string_view subject, TextGetter &&getter, size_t count)
    {
        size_t index;

        auto matcher = Matcher(subject);

        size_t best_match_cost = std::numeric_limits<size_t>::max();
        for (size_t i = 0; i < count; ++i)
        {
            auto target = getter(i);
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

    struct Filter
    {
        size_t meta_prop_id = 0;
        std::string_view source_text;
        std::string_view filter_text;
        std::vector<Matcher> matchers; // We have several because we can OR them
        bool inverted;
    };
    struct SortCommand
    {
        struct Arg
        {
            size_t meta_id;
            bool is_negated;
        };

        std::vector<Arg> args;
    };
    using Command = std::variant<SortCommand>;

    template <class... Ts>
    struct VisitHelper : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    VisitHelper(Ts...) -> VisitHelper<Ts...>;

    struct SortAtom
    {
        uint16_t prop_id = 0;
        bool is_negated = false;
    };

    struct Query
    {
        std::vector<Filter> filters;
        std::vector<size_t> filters_in_decl_order;
        std::span<Filter> exclusion_filters;
        std::span<Filter> inclusion_filters;
        bool reverse_output = false;
        std::vector<SortAtom> sort_atoms;
        std::vector<SortCommand::Arg> sort_args;

        Filter &GetFilterInDeclOrder(size_t index)
        {
            return filters[filters_in_decl_order[index]];
        }

        void clear()
        {
            reverse_output = false;
            filters.clear();
            filters_in_decl_order.clear();
            sort_atoms.clear();
            sort_args.clear();
        }
    };

    struct RelevantMeta
    {
        uint32_t meta_id;
        uint32_t popcount;
    };

    struct ResultItem
    {
        LoweredText searchable_text;
        FixedVector<char, 512> presentable_text;
        std::span<uint16_t> searchable_hl;
        std::span<uint16_t> presentable_hl;

        std::vector<uint16_t> _hl_storage; // Highlight
    };

    struct IncrementalProp
    {
        using BuildTemplateFn =
            Text::StringTemplateAtom (*)(
                Text::StringTemplateAtom::Builder &b,
                size_t itemId,
                void *data
            );

        Text::StringManager &mgr = Text::s_Manager;
        LoweredTextVector searchable_text;
        SpanVector<Text::StringTemplateAtom> stringTemplates;
        SpanVector<Text::StringTemplateAtom>::Deduper stringTemplates_deduper;
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

        void PopulateItems(SlotSpanVector<char> &src)
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
            SpanVector<char>::Deduper deduper;
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
                CapacityHints::UpdateHint(deduper_hint_key, deduper.uniques.bucket_count());
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
                        Text::StringTemplateAtom::Builder b(out);
                        out.push_back(BuildTemplate_fn(b, itemId, buildTemplate_data));
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

        void GetRenderableString(size_t strId, OutBuf<char> out, OutBuf<Text::PosDelta> deltas)
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
                mgr.AssembleRenderableString(t, out, &deltas);
                return;
            }
        }
    };

    struct Feedback
    {
        std::string filter_feedback;
        std::string command_feedback;
    };

    template <typename I>
    concept DeviceImpl =
        requires {
            typename I::index_type;
            // call static constexpr functions on the type itself
            { I::MaxSpanCount() } -> std::convertible_to<size_t>;
            { I::PropCount() } -> std::convertible_to<size_t>;
        } &&
        requires(I &d, size_t metaId, size_t propId) {
            // meta
            { d.MetaCount() } -> std::convertible_to<size_t>;
            { d.GetMetaName(metaId) } -> std::same_as<LoweredText>;
            { d.GetMetaPropset(metaId) } -> std::same_as<BitView>;

            // props
            { d.GetProperty(propId) } -> std::same_as<IncrementalProp *>;
        };

    template <DeviceImpl I>
    struct Device
    {
        I &impl;
        SpanVector<RelevantMeta> relevant_metas_per_prop; // For each prop this is sorted from most specific to least

        Device(I &impl) : impl(impl)
        {
            auto n_meta = impl.MetaCount();
            auto n_props = I::PropCount();
            auto deduper = relevant_metas_per_prop.CreateDeduper(n_props);
            relevant_metas_per_prop.Reserve(n_props, n_props * 2);
            for (size_t prop_id = 0; prop_id <= n_props; ++prop_id)
            {
                for (size_t meta_id = 0; meta_id < n_meta; ++meta_id)
                {
                    auto propset = impl.GetMetaPropset(meta_id);
                    if (propset[prop_id])
                    {
                        relevant_metas_per_prop.Elements().emplace_back(meta_id, propset.PopCount());
                    }
                }
                auto pending = relevant_metas_per_prop.GetPendingSpan();
                std::stable_sort(
                    pending.begin(),
                    pending.end(),
                    [](auto a, auto b)
                    {
                        return a.popcount < b.popcount;
                    }
                );
                relevant_metas_per_prop.CommitWritten(&deduper);
            }
        }

        std::optional<Filter> ParseFilter(std::string_view source)
        {
            Filter filter;
            filter.source_text = source;
            auto rem = source;
            Utils::ReadSpaces(rem);

            auto separator_pos = rem.find_first_of("!:=<>");

            filter.meta_prop_id = 0;
            if (separator_pos != std::string_view::npos)
            {
                char separator = rem[separator_pos];
                if (separator_pos != 0)
                {
                    auto target_str = rem.substr(0, separator_pos);
                    if (!target_str.empty())
                    {
                        auto opt_meta_prop_id = TryGetMetaIndexFromName(target_str);
                        if (opt_meta_prop_id.has_value())
                        {
                            filter.meta_prop_id = opt_meta_prop_id.value();
                            rem = rem.substr(separator_pos);
                        }
                    }
                }
            }

            filter.inverted = Utils::TryRead('!', rem);
            if (filter.meta_prop_id)
            {
                Utils::TryRead(':', rem) || Utils::TryRead('=', rem);
            }

            Utils::ReadSpaces(rem);
            filter.filter_text = rem;

            do
            {
                auto or_pos = rem.find('|');
                std::string_view content;
                if (or_pos == std::string_view::npos)
                {
                    content = rem;
                    rem = "";
                }
                else
                {
                    content = rem.substr(0, or_pos);
                    rem = rem.substr(or_pos + 1);
                }
                Utils::ReadSpaces(content);
                Utils::TrimTrailingSpaces(content);

                filter.matchers.emplace_back(Matcher(content));
            } while (!rem.empty());

            return filter;
        }
        bool ParseCommand(std::string_view source, Command &command)
        {
            auto rem = source;

            Utils::ReadSpaces(rem);

            if (!Utils::TryRead('/', rem))
                return false;

            LoweredTextOwned sort_command_name("SORT");

            auto command_name_end = std::min(rem.find(' '), rem.size());
            auto command_name = rem.substr(0, command_name_end);
            if (command_name.empty())
                return true;
            auto command_id_opt = BestMatch(
                command_name,
                [&](size_t index)
                {
                    return (LoweredText)sort_command_name;
                },
                1
            );
            if (!command_id_opt.has_value())
                return false;
            rem = rem.substr(command_name_end);

            auto &sort_com = command.emplace<SortCommand>();
            while (true)
            {
                auto arg_end = rem.find(',');
                auto target_text = rem.substr(0, arg_end);
                Utils::ReadSpaces(target_text);
                Utils::TrimTrailingSpaces(target_text);
                bool is_negated = !target_text.empty() && target_text.back() == '!';
                if (is_negated)
                    target_text = target_text.substr(0, target_text.size() - 1);
                Utils::TrimTrailingSpaces(target_text);
                auto index = target_text.empty() ? 0 : TryGetMetaIndexFromName(target_text);
                if (index.has_value())
                {
                    auto &arg = sort_com.args.emplace_back();
                    arg.is_negated = is_negated;
                    arg.meta_id = index.value();
                }

                if (arg_end == std::string_view::npos)
                    break;
                rem = rem.substr(arg_end + 1);
            }

            return true;
        }
        void ParseQuery(std::string_view source, Query &query)
        {
            query.clear();

            BitArray<I::PropCount() + 1> sorting_props;

            auto ParseTrailingCommands = [&](std::string_view &stmt)
            {
                FixedVector<Command, 32> commands;
                while (true) // Loop that eats commands from end
                {
                    auto com_start = stmt.find_last_of('/');
                    if (com_start == std::string_view::npos)
                        break;
                    auto com_src = stmt.substr(com_start);

                    auto &command = commands.emplace_back();
                    if (ParseCommand(com_src, command))
                    {
                        stmt = stmt.substr(0, com_start);
                        Utils::TrimTrailingSpaces(stmt);
                    }
                    else
                    {
                        commands.pop_back();
                        break;
                    }
                }
                for (auto &command : std::views::reverse(commands))
                {
                    std::visit(
                        VisitHelper{
                            [&](SortCommand &sort_command)
                            {
                                for (auto &arg : sort_command.args)
                                {
                                    query.sort_args.emplace_back(std::move(arg));
                                    if (arg.meta_id == 0) // Special case: Default sort
                                    {
                                        if (arg.is_negated)
                                            query.reverse_output = !query.reverse_output;
                                        continue;
                                    }
                                    auto propset = impl.GetMetaPropset(arg.meta_id);
                                    for (auto prop_id : propset.IterSetBits())
                                    {
                                        if (prop_id == I::PropCount())
                                            continue;
                                        if (sorting_props[prop_id])
                                            continue;
                                        sorting_props[prop_id] = true;
                                        query.sort_atoms.emplace_back((uint16_t)prop_id, arg.is_negated);
                                    }
                                }
                            },
                            [](auto &&)
                            {
                                assert(false);
                            },
                        },
                        command
                    );
                }
            };

            // inclusion filters go into temporary storage
            std::vector<Filter> inclusion_filters_temp;
            constexpr uint32_t InclusionMarkerBit = 0x80000000;

            auto rem = source;
            while (!rem.empty())
            {
                auto stmt_end = rem.find('&');
                std::string_view stmt;
                if (stmt_end == std::string_view::npos)
                {
                    stmt = rem;
                    rem = "";
                }
                else
                {
                    stmt = rem.substr(0, stmt_end);
                    rem = rem.substr(stmt_end + 1);
                }
                Utils::ReadSpaces(stmt);
                Utils::TrimTrailingSpaces(stmt);

                ParseTrailingCommands(stmt);

                if (stmt.empty())
                    continue;

                auto filter_opt = ParseFilter(stmt);
                if (filter_opt.has_value())
                {
                    auto &filter = filter_opt.value();
                    uint32_t index;
                    if (filter.inverted)
                    {
                        index = query.filters.size();
                        query.filters.emplace_back(std::move(filter));
                    }
                    else
                    {
                        index = inclusion_filters_temp.size();
                        index |= InclusionMarkerBit;
                        inclusion_filters_temp.emplace_back(std::move(filter));
                    }
                    query.filters_in_decl_order.push_back(index);
                }
            }

            auto offset = query.filters.size();
            // Copy in the inclusion filters
            for (auto &f : inclusion_filters_temp)
            {
                query.filters.emplace_back(std::move(f));
            }
            // Fix their indices
            for (auto &index : query.filters_in_decl_order)
            {
                if (index & InclusionMarkerBit)
                {
                    index &= ~InclusionMarkerBit;
                    index += offset;
                }
            }
            // Make the spans
            query.exclusion_filters = std::span<Filter>(query.filters.data(), offset);
            query.inclusion_filters = std::span<Filter>(query.filters.data() + offset, query.filters.size() - offset);
        }
        template <bool is_exclusion>
        void RunFilters(std::span<Filter> filters, std::vector<typename I::index_type> &items)
        {
            Stopwatch stopwatch("RunFilters");

            size_t n_filters = filters.size();
            size_t n_items = items.size();
            size_t n_spans = impl.MaxSpanCount();
            size_t n_props = I::PropCount();
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
                BitView propset = impl.GetMetaPropset(filter.meta_prop_id);

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
                            for (auto &matcher : filter.matchers)
                            {
                                if (!matcher.Match(str, 0))
                                    continue;

                                // The filter matched against this meta name, now we need to find out which items "have it"

                                BitView meta_propset = impl.GetMetaPropset(i_meta);
                                for (auto prop_id : meta_propset.IterSetBits())
                                {
                                    bool is_meta = prop_id == n_props;
                                    if (is_meta) // Meta properties cannot be "had"
                                        continue;
                                    auto &prop = *impl.GetProperty(prop_id);
                                    // Iterate the items and check which ones "has values" in this property
                                    bool all_matched = true;
                                    for (auto index : unconfirmed_match.IterSetBits())
                                    {
                                        auto item = items[index];
                                        auto str_id = prop.GetStrId(item);
                                        auto str = prop.GetSearchableStr(str_id);
                                        if (!str.text.empty())
                                        {
                                            // If they have a value, mark the item as confirmed
                                            unconfirmed_match[index] = false;
                                            continue;
                                        }
                                        all_matched = false;
                                    }
                                    if (all_matched)
                                    {
                                        if constexpr (is_exclusion)
                                        {
                                            items.clear();
                                            return;
                                        }
                                        else
                                        {
                                            goto next_filter;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else // Non-meta
                    {
                        auto &prop = *impl.GetProperty(prop_id);
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

                            for (auto &matcher : filter.matchers)
                            {
                                if (matcher.Match(str, 0))
                                    goto next;
                            }
                            marked_spans[span_id] = false;
                        next:;
                        }
                        stopwatch.Checkpoint(std::format("prop_{} matching", prop_id));

                        // Iterate the items and record which ones' spans matched
                        bool all_matched = true;
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
                            all_matched = false;
                        }
                        stopwatch.Checkpoint(std::format("prop_{} checking", prop_id));
                        if (all_matched)
                        {
                            if constexpr (is_exclusion)
                            {
                                items.clear();
                                return;
                            }
                            else
                            {
                                goto next_filter;
                            }
                        }
                    }
                }

                {
                    // Iterate the matched items and repopulate the items vector with them
                    size_t new_items_size = 0;
                    if constexpr (!is_exclusion)
                    {
                        unconfirmed_match.Flip(); // It becomes "confirmed match"
                    }
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
        void SortItems(std::span<SortAtom> atoms, std::span<typename I::index_type> items)
        {
            Stopwatch stopwatch("SortItems");

            std::stable_sort(
                items.begin(), items.end(),
                [&](size_t item_a, size_t item_b)
                {
                    for (auto &atom : atoms)
                    {
                        auto prop_id = atom.prop_id;
                        auto &prop = *impl.GetProperty(prop_id);
                        auto str_a = prop.GetSearchableStr(prop.GetStrId(item_a));
                        auto str_b = prop.GetSearchableStr(prop.GetStrId(item_b));
                        auto cmp = str_a.text <=> str_b.text;
                        if (cmp == 0)
                            continue;

                        return atom.is_negated ? cmp > 0 : cmp < 0;
                    }
                    return false;
                }
            );
        }
        void RunQuery(Query &query, std::vector<typename I::index_type> &items)
        {
            if (!query.exclusion_filters.empty())
            {
                RunFilters<true>(query.exclusion_filters, items);
            }

            if (!query.inclusion_filters.empty())
            {
                RunFilters<false>(query.inclusion_filters, items);
            }

            if (query.reverse_output)
            {
                std::reverse(items.begin(), items.end());
            }

            if (!query.sort_atoms.empty())
            {
                SortItems(query.sort_atoms, items);
            }
        }
        void GetFeedback(Query &query, Feedback &out, bool verbose = false)
        {
            static auto GetDispStr = [](const Matcher::Atom &atom) -> std::string_view
            {
                if (atom.type == Matcher::Atom::Type::AnyNumber)
                {
                    if (!Utils::HasAnyFlag(atom.post_check, Matcher::Atom::PostCheck::NumChecks))
                    {
                        return "any number";
                    }
                }
                else if (atom.type == Matcher::Atom::Type::ExactNumber)
                {
                    if (!atom.src_str.empty() && atom.src_str[0] == '=')
                        return atom.src_str.substr(1);
                }
                return atom.src_str;
            };

            static auto AppendJoin = [](std::string &s, size_t i, size_t count) -> void
            {
                if (i > 0)
                {
                    std::string_view str;
                    if (i + 1 < count)
                    {
                        str = ", then ";
                    }
                    else
                    {
                        str = " and then ";
                    }
                    s.append_range(str);
                }
            };

#define SkillDull "<c=@skilldull>"
#define ArgColor "<c=@skilldyn>"
#define ControlColor "<c=#64ffff>"
#define ModifierColor "<c=#f0f080>"
#define CloseColor "</c>"

            {
                auto &s = out.filter_feedback;
                s.clear();
                auto inserter = std::back_inserter(s);
                const auto n_filters = query.filters_in_decl_order.size();
                for (size_t f = 0; f < n_filters; ++f)
                {
                    auto &filter = query.GetFilterInDeclOrder(f);

                    FixedVector<char, 128> meta_name;
                    impl.GetMetaName(filter.meta_prop_id).GetRenderableString(meta_name);
                    auto meta_name_str = (std::string_view)meta_name;
                    auto cond = filter.inverted ? ControlColor "not " CloseColor : "";
                    auto musty = meta_name_str.empty() ? "Must" : " must";
                    std::format_to(inserter, ControlColor "{}</c>{} {}contain: ", meta_name_str, musty, cond);
                    for (size_t m = 0; m < filter.matchers.size(); ++m)
                    {
                        auto &matcher = filter.matchers[m];

                        if (!verbose)
                        {
                            std::format_to(inserter, ArgColor "'{}'</c>", matcher.src_str);
                        }
                        else
                        {
                            if (matcher.atoms.empty())
                            {
                                std::format_to(inserter, ArgColor "[Nothing]</c>");
                            }
                            else
                            {
                                for (size_t a = 0; a < matcher.atoms.size(); ++a)
                                {
                                    auto &atom = matcher.atoms[a];
                                    AppendJoin(s, a, matcher.atoms.size());

                                    bool is_leading = Utils::HasAnyFlag(atom.post_check, Matcher::Atom::PostCheck::Distinct);
                                    if (is_leading)
                                    {
                                        s.append_range((std::string_view)ModifierColor "leading</c> ");
                                    }
                                    auto src_str = GetDispStr(atom);
                                    switch (atom.type)
                                    {
                                        case Matcher::Atom::Type::String:
                                        {
                                            std::format_to(inserter, ArgColor "'{}'</c>", atom.src_str);
                                            break;
                                        }
                                        case Matcher::Atom::Type::AnyNumber:
                                        case Matcher::Atom::Type::ExactNumber:
                                        {
                                            std::string_view number_str = atom.src_str;
                                            if (atom.type == Matcher::Atom::Type::AnyNumber)
                                            {
                                                if (!Utils::HasAnyFlag(atom.post_check, Matcher::Atom::PostCheck::NumChecks))
                                                    number_str = "[AnyNumber]";
                                            }
                                            else if (atom.type == Matcher::Atom::Type::ExactNumber)
                                            {
                                                if (!atom.src_str.empty() && atom.src_str[0] == '=')
                                                    number_str = atom.src_str.substr(1);
                                            }

                                            std::format_to(inserter, ArgColor "{}</c>", number_str);
                                            break;
                                        }
                                    }
                                    switch (atom.search_bound)
                                    {
                                        case Matcher::Atom::SearchBound::Anywhere:
                                            // std::format_to(inserter, " <c=#ffff80>anywhere</c>");
                                            break;
                                        case Matcher::Atom::SearchBound::WithinXWords:
                                            switch (atom.within_count)
                                            {
                                                case 0:
                                                    std::format_to(inserter, ModifierColor " within same word</c>");
                                                    break;
                                                case 1:
                                                    std::format_to(inserter, ModifierColor " within 1 word</c>");
                                                    break;
                                                default:
                                                    std::format_to(inserter, ModifierColor " within {} words</c>", atom.within_count);
                                                    break;
                                            }
                                            break;
                                    }
                                }
                            }
                        }

                        if (m + 1 < filter.matchers.size()) // all but last
                        {
                            std::format_to(inserter, ", " ControlColor "or</c> ");
                        }
                        else // last
                        {
                            *inserter++ = '.';
                        }
                    }

                    if (f + 1 < n_filters) // all but last
                    {
                        *inserter++ = '\n';
                    }
                }
            }

            auto &s = out.command_feedback;
            s.clear();
            if (!query.sort_args.empty())
            {
                auto inserter = std::back_inserter(s);

                const auto n_args = query.sort_args.size();

                std::format_to(inserter, "Sort ");
                for (uint32_t a = 0; a < n_args; a++)
                {
                    const auto &arg = query.sort_args[a];

                    AppendJoin(s, a, n_args);
                    s.append_range((std::string_view) "by ");

                    std::string_view sort_target_name;
                    if (arg.meta_id == 0)
                    {
                        sort_target_name = "Default";
                    }
                    else
                    {
                        auto meta_name = impl.GetMetaName(arg.meta_id);
                        FixedVector<char, 128> name;
                        meta_name.GetRenderableString(name);
                        sort_target_name = name;
                    }

                    std::format_to(inserter, ArgColor "{}</c>", sort_target_name);

                    if (arg.is_negated)
                    {
                        std::format_to(inserter, ":" ModifierColor "descending</c>");
                    }
                    else
                    {
                        std::format_to(inserter, ":" ModifierColor "ascending</c>");
                    }
                }
            }
        }

        ResultItem CalcItemResult(Query &q, size_t prop_id, I::index_type item_id)
        {
            auto &prop = *impl.GetProperty(prop_id);
            auto str_id = prop.GetStrId(item_id);
            ResultItem result;
            result.searchable_text = prop.GetSearchableStr(str_id);
            CalcHL(q, prop_id, result.searchable_text, result._hl_storage);
            FixedVector<Text::PosDelta, 128> deltas;
            prop.GetRenderableString(str_id, result.presentable_text, deltas);
            if (deltas.empty())
            {
                result.searchable_hl = result.presentable_hl = result._hl_storage;
            }
            else
            {
                auto hl_count = result._hl_storage.size();
                result._hl_storage.resize(hl_count * 2);
                result.searchable_hl = {result._hl_storage.data(), hl_count};
                result.presentable_hl = {result._hl_storage.data() + hl_count, hl_count};
                Text::PatchPositions(result.searchable_hl, result.presentable_hl, deltas);
            }
            return result;
        }

        ResultItem CalcPropResult(Query &q, size_t prop_id)
        {
            ResultItem result;
            auto meta_prop_id = I::PropCount();
            auto relevant_metas = relevant_metas_per_prop.Get(prop_id);
            if (!relevant_metas.empty())
            {
                constexpr std::string_view join = SkillDull ", " CloseColor;
                for (size_t i = 0; i < relevant_metas.size(); ++i)
                {
                    auto &relevant_meta = relevant_metas[i];

                    size_t offset = result.presentable_text.size();
                    LoweredText lowered = impl.GetMetaName(relevant_meta.meta_id);
                    if (lowered.text.empty()) // We allow empty meta names; just skip
                        continue;
                    auto size = result._hl_storage.size();
                    CalcHL(q, meta_prop_id, lowered, result._hl_storage);
                    result.searchable_hl = result.presentable_hl = result._hl_storage;
                    lowered.GetRenderableString(result.presentable_text);
                    if (offset > 0)
                    {
                        for (size_t i = size; i < result._hl_storage.size(); i++)
                        {
                            result._hl_storage[i] += offset;
                        }
                    }

                    result.presentable_text.AppendString(join);
                }
                if (result.presentable_text.size() > join.size())
                {
                    result.presentable_text.resize(result.presentable_text.size() - join.size());
                }
            }

            return result;
        }

    private:
        inline void CalcHL(Query &q, size_t prop_id, LoweredText lowered, std::vector<uint16_t> &hl)
        {
            if (!lowered.text.empty())
            {
                auto hl_size_before = hl.size();
                for (auto &filter : q.inclusion_filters)
                {
                    auto propset = impl.GetMetaPropset(filter.meta_prop_id);
                    if (propset[prop_id])
                    {
                        for (auto &matcher : filter.matchers)
                        {
                            auto hl_size_before = hl.size();
                            bool is_match = matcher.Matches(lowered, hl);
                            std::span<uint16_t> hl_span = hl;
                            hl_span = hl_span.subspan(hl_size_before);
                            ConnectHighlighting(lowered.text, hl_span);
                            hl.resize(hl_size_before + hl_span.size());
                        }
                    }
                }
                std::span<uint16_t> hl_span = hl;
                hl_span = hl_span.subspan(hl_size_before);
                SortHighlighting(hl_span);
                hl.resize(hl_size_before + hl_span.size());
            }
        }

        inline std::optional<size_t> TryGetMetaIndexFromName(std::string_view subject)
        {
            return BestMatch(
                subject,
                [this](size_t metaId)
                {
                    return this->impl.GetMetaName(metaId);
                },
                impl.MetaCount()
            );
        }
    };
}