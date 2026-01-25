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
            size_t target_meta_prop_id;
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
            { d.GetProperty(propId) } -> std::same_as<IncrementalProp *>;
        };

    template <DeviceImpl I>
    struct Device
    {
        I &impl;

        bool ParseFilter(std::string_view source, Filter &filter)
        {
            filter.source_text = source;
            auto rem = source;
            Utils::ReadSpaces(rem);

            enum struct SeparatorType
            {
                None,
                Colon,
                Equal,
                LessThan,
                GreaterThan,
                Not,
            };
            SeparatorType separator_type = SeparatorType::None;
            auto separator_pos = rem.find(':');
            if (separator_pos != std::string_view::npos)
            {
                separator_type = SeparatorType::Colon;
            }
            else
            {
                auto r = rem;
                size_t temp_pos;
                if (temp_pos = r.find('='), temp_pos != std::string_view::npos)
                {
                    separator_type = SeparatorType::Equal;
                    separator_pos = temp_pos;
                    r = r.substr(0, separator_pos);
                }
                if (temp_pos = r.find('<'), temp_pos != std::string_view::npos)
                {
                    separator_type = SeparatorType::LessThan;
                    separator_pos = temp_pos;
                    r = r.substr(0, separator_pos);
                }
                if (temp_pos = r.find('>'), temp_pos != std::string_view::npos)
                {
                    separator_type = SeparatorType::GreaterThan;
                    separator_pos = temp_pos;
                    r = r.substr(0, separator_pos);
                }
                if (temp_pos = r.find('!'), temp_pos != std::string_view::npos)
                {
                    separator_type = SeparatorType::Not;
                    separator_pos = temp_pos;
                    r = r.substr(0, separator_pos);
                }
            }

            std::optional<size_t> meta_prop_id = std::nullopt;
            if (separator_pos != std::string_view::npos)
            {
                auto target_str = rem.substr(0, separator_pos);
                if (!target_str.empty())
                {
                    meta_prop_id = BestMatch(target_str);
                }
            }

            if (meta_prop_id.has_value())
            {
                filter.meta_prop_id = meta_prop_id.value();
                rem = rem.substr(separator_pos);
                if (separator_type == SeparatorType::Colon)
                {
                    rem = rem.substr(1);
                }
            }
            else
            {
                if (separator_type == SeparatorType::Colon)
                    return false;
            }

            Utils::ReadSpaces(rem);
            filter.filter_text = rem;
            filter.inverted = Utils::TryRead('!', rem);

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

            return true;
        }
        bool ParseCommand(std::string_view source, Command &command)
        {
            auto rem = source;

            Utils::ReadSpaces(rem);

            if (!Utils::TryRead('/', rem))
                return false;

            if (Utils::TryRead("SORT", rem))
            {
                auto &sort_com = command.emplace<SortCommand>();
                while (rem.size())
                {
                    if (!Utils::ReadSpaces(rem))
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
                if (stmt.empty())
                    continue;

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
                while (!commands.empty()) // Reverse
                {
                    query.commands.emplace_back(std::move(commands.pop_back()));
                }

                auto &filter = query.filters.emplace_back();
                if (!ParseFilter(stmt, filter))
                    query.filters.pop_back();
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
                                    bool all_confirmed = true;
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
                                        all_confirmed = false;
                                    }
                                    if (all_confirmed)
                                        goto next_filter;
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
        void RunCommands(std::span<Command> commands, std::vector<typename I::index_type> &items)
        {
            Stopwatch stopwatch("RunCommands");

            for (auto &command : commands)
            {
                std::visit(
                    VisitHelper{
                        [&](SortCommand &sort_command)
                        {
                            for (auto &arg : sort_command.args)
                            {
                                auto propset = impl.GetMetaPropset(arg.target_meta_prop_id);
                                for (auto prop_id : propset.IterSetBits())
                                {
                                    if (prop_id == impl.PropCount())
                                        continue;
                                    auto &prop = *impl.GetProperty(prop_id);
                                    std::sort(
                                        items.begin(), items.end(),
                                        [&](size_t item_a, size_t item_b)
                                        {
                                            auto str_a = prop.GetSearchableStr(prop.GetStrId(item_a));
                                            auto str_b = prop.GetSearchableStr(prop.GetStrId(item_b));
                                            return str_a.text < str_b.text;
                                        }
                                    );
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
        }
        void RunQuery(Query &query, std::vector<typename I::index_type> &items)
        {
            if (!query.filters.empty())
            {
                RunFilters(query.filters, items);
            }

            if (!query.commands.empty())
            {
                RunCommands(query.commands, items);
            }
        }
        void GetFeedback(Query &query, std::string &out, bool verbose = false)
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
            out.clear();
            auto inserter = std::back_inserter(out);
            for (size_t f = 0; f < query.filters.size(); ++f)
            {
                auto &filter = query.filters[f];

                FixedVector<char, 128> meta_name;
                impl.GetMetaName(filter.meta_prop_id).GetRenderableString(meta_name);
                auto cond = filter.inverted ? "<c=#55ffdd>not</c> " : "";
                std::format_to(inserter, "<c=#55ffdd>{}</c> must {}contain<c=#55ffdd>:</c> ", (std::string_view)meta_name, cond);
                for (size_t m = 0; m < filter.matchers.size(); ++m)
                {
                    auto &matcher = filter.matchers[m];

                    if (!verbose)
                    {
                        std::format_to(inserter, "<c=@skilldyn>'{}'</c>", matcher.src_str);
                    }
                    else
                    {
                        if (matcher.atoms.empty())
                        {
                            std::format_to(inserter, "<c=@skilldyn>[Nothing]</c>");
                        }
                        else
                        {
                            for (size_t a = 0; a < matcher.atoms.size(); ++a)
                            {
                                auto &atom = matcher.atoms[a];

                                bool is_leading = Utils::HasAnyFlag(atom.post_check, Matcher::Atom::PostCheck::Distinct);
                                if (is_leading)
                                    std::format_to(inserter, "leading ");
                                auto src_str = GetDispStr(atom);
                                switch (atom.type)
                                {
                                    case Matcher::Atom::Type::String:
                                    {
                                        std::format_to(inserter, "<c=@skilldyn>'{}'</c>", atom.src_str);
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

                                        std::format_to(inserter, "<c=@skilldyn>{}</c>", number_str);
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
                                                std::format_to(inserter, " <c=#ffff80>within same word</c>");
                                                break;
                                            case 1:
                                                std::format_to(inserter, " <c=#ffff80>within 1 word</c>");
                                                break;
                                            default:
                                                std::format_to(inserter, " <c=#ffff80>within {} words</c>", atom.within_count);
                                                break;
                                        }
                                        break;
                                }

                                if (a + 2 < matcher.atoms.size())
                                {
                                    std::format_to(inserter, ", then ");
                                }
                                else if (a + 1 < matcher.atoms.size()) // second to last
                                {
                                    std::format_to(inserter, " and then ");
                                }
                            }
                        }
                    }

                    if (m + 1 < filter.matchers.size()) // all but last
                    {
                        std::format_to(inserter, ", <c=#55ffdd>or</c> ");
                    }
                    else // last
                    {
                        *inserter++ = '.';
                    }
                }

                if (f + 1 < query.filters.size()) // all but last
                {
                    *inserter++ = '\n';
                }
            }

            if (!query.filters.empty())
            {
                *inserter++ = '\n';
            }

            for (size_t c = 0; c < query.commands.size(); c++)
            {
                auto &command = query.commands[c];

                std::visit(
                    VisitHelper{
                        [&](SortCommand &sort_command)
                        {
                            const auto n_args = sort_command.args.size();

                            std::format_to(inserter, "<c=#55ffdd>Sort</c>");
                            for (uint32_t a = 0; a < n_args; a++)
                            {
                                const auto &arg = sort_command.args[a];

                                if (a > 0)
                                {
                                    if (a + 1 < n_args)
                                    {
                                        std::format_to(inserter, ", then");
                                    }
                                    else
                                    {
                                        std::format_to(inserter, " and then");
                                    }
                                }

                                if (arg.is_negated)
                                {
                                    std::format_to(inserter, " descending by");
                                }
                                else
                                {
                                    std::format_to(inserter, " ascending by");
                                }

                                auto meta_name = impl.GetMetaName(arg.target_meta_prop_id);
                                FixedVector<char, 64> name;
                                meta_name.GetRenderableString(name);

                                std::format_to(inserter, " <c=#55ffdd>{}</c>", (std::string_view)name);
                            }
                        },
                        [](auto &&)
                        {
                            assert(false);
                        },
                    },
                    command
                );

                if (c + 1 < query.commands.size()) // all but last
                {
                    *inserter++ = '\n';
                }
            }
        }

        ResultItem CalcItemResult(Query &q, size_t prop_id, I::index_type item_id)
        {
            auto &prop = *impl.GetProperty(prop_id);
            auto str_id = prop.GetStrId(item_id);
            LoweredText lowered = prop.GetSearchableStr(str_id);
            ResultItem result;
            CalcHL(q, prop_id, lowered, result.hl);
            FixedVector<Text::PosDelta, 64> deltas;
            prop.GetRenderableString(str_id, result.text, deltas);
            Text::PatchPositions(result.hl, deltas);
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
                SortHighlighting(hl_span);
                hl.resize(hl_span.size());
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