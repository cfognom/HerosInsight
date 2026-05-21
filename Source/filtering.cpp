#pragma once

#include "filtering.h"
#include <span>
#include <vector>

namespace HerosInsight::Filtering
{
    // Takes a span of highlight starts and stops, sorts them and removes any bad openings and closings
    // Modifies the size of the span to account for the removed values
    static void SortHighlighting(std::span<uint16_t> &hl)
    {
        assert(hl.size() % 2 == 0);

        // Add a least significant bit to each hl value and mark the closings (every other value)
        for (size_t i = 0; i < hl.size(); ++i)
        {
            hl[i] = (hl[i] << 1) | (i & 1);
        }

        // Sort the values
        std::sort(hl.begin(), hl.end());

        // Erase bad openings and closings
        size_t hl_level = 0;
        auto Remover = [&hl_level](uint16_t v)
        {
            bool is_closing = v & 1;
            hl_level += is_closing ? -1 : 1;
#ifdef _DEBUG
            assert(hl_level != -1);
#endif
            bool keep = is_closing ? hl_level == 0 : hl_level == 1;
            return !keep;
        };
        auto it = std::remove_if(hl.begin(), hl.end(), Remover);
        hl = std::span<uint16_t>(hl.data(), it - hl.begin());

        // Remove the least significant bit
        for (auto &v : hl)
        {
            v >>= 1;
        }
    }

    static void ConnectHighlighting(std::string_view text, std::span<uint16_t> &hl)
    {
        if (hl.size() < 4)
            return;

        size_t dst_idx = 1;
        size_t i = 1;
        for (; i + 1 < hl.size(); i += 2)
        {
            auto begin = hl[i];
            auto end = hl[i + 1];
            auto between = text.substr(begin, end - begin);
            for (auto c : between)
            {
                if (c != ' ')
                {
                    hl[dst_idx++] = begin;
                    hl[dst_idx++] = end;
                    break;
                }
            }
        }
        hl[dst_idx++] = hl[i];
        hl = hl.subspan(0, dst_idx);
    }

    std::strong_ordering CompareSubstrs(std::span<const uint16_t> asubs, std::string_view astr, std::span<const uint16_t> bsubs, std::string_view bstr)
    {
        size_t count = std::min(asubs.size(), bsubs.size());
        std::strong_ordering cmp;
        for (size_t i = 0; i < count; i += 2)
        {
            auto asub = astr.substr(asubs[i], asubs[i + 1] - asubs[i]);
            auto bsub = bstr.substr(bsubs[i], bsubs[i + 1] - bsubs[i]);
            cmp = asub <=> bsub;
            if (cmp != 0)
                return cmp;
        }
        cmp = asubs.size() <=> bsubs.size();
        return cmp;
    }

    // Returns the index of the "best" match
    template <typename TextGetter>
    static std::optional<size_t> BestMatch(std::string_view subject, TextGetter &&getter, size_t count)
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

    Device::Device(std::span<MetaProp> metas, std::span<IncrementalProp *> props)
        : metas(metas), props(props)
    {
        auto n_meta = metas.size();
        auto n_props = props.size();
        auto deduper = relevant_metas_per_prop.CreateDeduper(n_props);
        relevant_metas_per_prop.Reserve(n_props, n_props * 2);
        for (size_t prop_id = 0; prop_id <= n_props; ++prop_id)
        {
            for (size_t meta_id = 0; meta_id < n_meta; ++meta_id)
            {
                auto propset = metas[meta_id].propset;
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

    static std::optional<size_t> TryGetMetaIdFromName(std::span<const MetaProp> metas, std::string_view subject)
    {
        return BestMatch(
            subject,
            [metas](size_t metaId)
            {
                return metas[metaId].name;
            },
            metas.size()
        );
    }

    static std::optional<Filter> ParseFilter(std::span<const MetaProp> metas, std::string_view source)
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
                    auto opt_meta_prop_id = TryGetMetaIdFromName(metas, target_str);
                    if (opt_meta_prop_id.has_value())
                    {
                        filter.meta_prop_id = opt_meta_prop_id.value();
                        rem = rem.substr(separator_pos);
                    }
                }
            }
        }

        filter.is_exclusion = Utils::TryRead('!', rem);
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

    static LoweredString matched_target{"Matched"};
    static LoweredStringView GetSortTargetName(std::span<const MetaProp> metas, size_t sort_target_id)
    {
        auto n_meta = metas.size();
        if (sort_target_id < n_meta)
            return metas[sort_target_id].name;

        assert(sort_target_id == n_meta);
        return (LoweredStringView)matched_target;
    }

    static std::optional<size_t> TryGetSortTargetIdFromName(std::span<const MetaProp> metas, std::string_view subject)
    {
        return BestMatch(
            subject,
            [metas](size_t id)
            {
                return GetSortTargetName(metas, id);
            },
            metas.size() + 1
        );
    }

    static bool ParseCommand(std::span<const MetaProp> metas, std::string_view source, Command &command)
    {
        auto rem = source;

        Utils::ReadSpaces(rem);

        if (!Utils::TryRead('/', rem))
            return false;

        LoweredString sort_command_name("SORT");

        auto command_name_end = std::min(rem.find(' '), rem.size());
        auto command_name = rem.substr(0, command_name_end);
        if (command_name.empty())
            return true;
        auto command_id_opt = BestMatch(
            command_name,
            [&](size_t index)
            {
                return (LoweredStringView)sort_command_name;
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
            size_t pos;
            SortCommand::Arg arg;
            while (pos = target_text.find_last_of("!"), pos != std::string_view::npos)
            {
                arg.is_negated |= target_text[pos] == '!';
                target_text = target_text.substr(0, pos);
            }
            Utils::TrimTrailingSpaces(target_text);
            auto sort_target_id = target_text.empty() ? 0 : TryGetSortTargetIdFromName(metas, target_text);
            if (sort_target_id.has_value())
            {
                arg.sort_target_id = sort_target_id.value();
                sort_com.args.push_back(std::move(arg));
            }

            if (arg_end == std::string_view::npos)
                break;
            rem = rem.substr(arg_end + 1);
        }

        return true;
    }

    void Device::ParseQuery(std::string_view source, Query &query) const
    {
        query.clear();

        BitVector sorting_props{props.size(), false};

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
                if (ParseCommand(this->metas, com_src, command))
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
                                if (arg.sort_target_id == 0) // Special case: Default sort
                                {
                                    if (arg.is_negated)
                                        query.reverse_output = !query.reverse_output;
                                    continue;
                                }
                                if (GetSortTargetName(this->metas, arg.sort_target_id).text == (std::string_view)matched_target.text) // Special case: Matched sort
                                {
                                    query.sort_atoms.emplace_back(0, arg.is_negated, true);
                                    continue;
                                }
                                auto propset = metas[arg.sort_target_id].propset;
                                for (auto prop_id : propset.IterSetBits())
                                {
                                    if (prop_id == props.size())
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

            auto filter_opt = ParseFilter(this->metas, stmt);
            if (filter_opt.has_value())
            {
                auto &filter = filter_opt.value();
                query.filters.emplace_back(std::move(filter));
            }
        }

        // We now need to put exclusion filters before inclusion filters while also keeping track of their original order
        std::span<Filter> filters = query.filters;
        auto n_filters = filters.size();

        // Create mapping from original index to new index
        query.filters_in_decl_order.resize(n_filters);
        size_t dst_index = 0;
        for (size_t i = 0; i < n_filters; ++i)
        {
            if (filters[i].is_exclusion)
                query.filters_in_decl_order[i] = dst_index++;
        }
        size_t inclusion_start = dst_index;
        for (size_t i = 0; i < n_filters; ++i)
        {
            if (!filters[i].is_exclusion)
                query.filters_in_decl_order[i] = dst_index++;
        }

        // Partition the filters so that exclusion filters come first
        std::stable_partition(
            filters.begin(), filters.end(),
            [](const Filter &f)
            {
                return f.is_exclusion;
            }
        );

        // Make the spans
        query.exclusion_filters = filters.subspan(0, inclusion_start);
        query.inclusion_filters = filters.subspan(inclusion_start);
    }

    struct FilterUnit
    {
        Device::index_t item;
        Device::index_t str_id;
        Device::index_t index;
        bool is_match;
    };

    static void FilterUnits(const Filtering::Device &device, Filter &filter, std::span<FilterUnit> &units)
    {
        ProfilingScope profiler;

        size_t n_props = device.props.size();
        size_t n_meta = device.metas.size();

        BitView propset = device.metas[filter.meta_prop_id].propset;

        struct Partitioner
        {
            std::span<FilterUnit> unmatched;

            static bool Predicate(FilterUnit &unit) { return unit.is_match; }
            void PartitionMatches()
            {
                ProfilingScope profiler;
                auto unmatched_group_it = std::partition(unmatched.begin(), unmatched.end(), Predicate);
                unmatched = std::span{unmatched_group_it, unmatched.end()};
            }
        } partitioner{units};

        for (auto prop_id : propset.IterSetBits())
        {
            bool is_meta = prop_id == n_props;
            if (is_meta) // Special case for meta properties
            {
                // Iterate the meta properties and check for name matches
                for (size_t i_meta = 0; i_meta < n_meta; ++i_meta)
                {
                    LoweredStringView str = device.metas[i_meta].name;
                    for (auto &matcher : filter.matchers)
                    {
                        if (!matcher.Match(str, 0))
                            continue;

                        // The filter matched against this meta name, now we need to find out which items "have it"

                        BitView meta_propset = device.metas[i_meta].propset;
                        for (auto prop_id : meta_propset.IterSetBits())
                        {
                            bool is_meta = prop_id == n_props;
                            if (is_meta) // Meta properties cannot be "had"
                                continue;
                            auto &prop = *device.props[prop_id];

                            // Iterate the items and check which ones "has values" in this property
                            for (auto &package : partitioner.unmatched)
                            {
                                auto str_id = prop.GetStrId(package.item);
                                auto str = prop.GetSearchableStr(str_id);
                                bool is_match = !str.text.empty();
                                package.is_match = is_match;
                            }

                            partitioner.PartitionMatches();
                        }
                    }
                }
            }
            else // Non-meta
            {
                auto &prop = *device.props[prop_id];

                { // Tag the items with their string ids
                    ProfilingScope profiler{"tagging"};
                    for (auto &package : partitioner.unmatched)
                    {
                        package.str_id = prop.GetStrId(package.item);
                    }
                }

                { // Sort by string id so we can avoid processing duplicates
                    ProfilingScope profiler{"sorting"};
                    std::sort(
                        partitioner.unmatched.begin(), partitioner.unmatched.end(),
                        [](auto &a, auto &b)
                        {
                            return a.str_id < b.str_id;
                        }
                    );
                }

                { // Iterate the items and mark which ones match
                    ProfilingScope profiler{"matching"};

                    auto prev_str_id = std::numeric_limits<Device::index_t>::max();
                    bool is_match;
                    for (auto &package : partitioner.unmatched)
                    {
                        auto str_id = package.str_id;
                        if (str_id != prev_str_id)
                        {
                            prev_str_id = str_id;
                            auto str = prop.GetSearchableStr(str_id);
                            is_match = filter.Match(str);
                        }
                        package.is_match = is_match;
                    }
                }

                partitioner.PartitionMatches();
            }
        }

        // Only keep the items that passed this filter.
        if (filter.is_exclusion)
            units = partitioner.unmatched;
        else
            units = std::span{units.data(), partitioner.unmatched.data()};
        // 'units' now contain only the items that passed this filter
    }

    static void RunFilters(const Filtering::Device &device, std::span<Filter> filters, std::vector<Device::index_t> &items)
    {
        ProfilingScope profiler;

        size_t n_items = items.size();

        MultiBuffer::Spec<FilterUnit> units_spec{n_items};
        MultiBuffer::HeapAllocated allocation{units_spec};
        auto units = units_spec.Span();

        { // Initialize the units
            ProfilingScope profiler{"init"};
            for (size_t i = 0; i < n_items; ++i)
            {
                auto &package = units[i];
                package.item = items[i];
                package.index = i;
            }
        }

        for (auto &filter : filters)
        {
            FilterUnits(device, filter, units);
        }

        { // Restore the order of the items
            ProfilingScope profiler{"restore order"};

            std::sort(
                units.begin(), units.end(),
                [](auto &a, auto &b)
                {
                    return a.index < b.index;
                }
            );
        }

        { // Populate the output with the matched items
            ProfilingScope profiler{"populate output"};

            auto dst = items.begin();
            for (auto &package : units)
            {
                *dst++ = package.item;
            }
            items.resize(units.size());
        }
    }

    static void SortItems(const Filtering::Device &device, Query &query, std::span<Device::index_t> items)
    {
        ProfilingScope profiler;

        struct MatchedSortObject
        {
            const Filtering::Device &device;
            SlotSpanVector<char> &cache;
            Filter &filter;
            bool negated;

            size_t CalcMatched(size_t id)
            {
#ifdef _DEBUG
                assert(cache.GetPendingSpan().empty());
                assert(!cache.GetSpanId(id).has_value());
#endif
                std::vector<char> candidate;
                std::vector<uint16_t> matches;
                candidate.reserve(128);
                matches.reserve(64);
                auto propset = device.metas[filter.meta_prop_id].propset;
                for (auto prop_id : propset.IterSetBits())
                {
                    if (prop_id == device.props.size())
                        continue;

                    auto &prop = *device.props[prop_id];
                    auto str = prop.GetSearchableStr(prop.GetStrId(id));
                    for (auto &matcher : filter.matchers)
                    {
                        size_t offset = 0;
                        while (matcher.Match(str, offset, (matches.clear(), matches)))
                        {
                            if (matches.empty())
                                continue;

                            auto current = cache.GetPendingSpan();
                            bool no_current = current.empty();
                            std::vector<char> &dst_vec = no_current ? cache.Elements() : (candidate.clear(), candidate);
                            for (size_t i = 0; i < matches.size(); i += 2)
                            {
                                dst_vec.append_range(str.text.substr(matches[i], matches[i + 1] - matches[i]));
                            }
                            if (no_current)
                                continue;

                            auto cmp = (std::string_view)candidate <=> (std::string_view)current;
                            if (cmp == 0)
                                continue;

                            if ((cmp < 0) != negated)
                            {
                                cache.DiscardWritten();
                                cache.Elements().append_range(candidate);
                            }
                        }
                    }
                }
                return cache.CommitWrittenToIndex(id);
            };
            size_t GetOrCalcMatched(size_t id)
            {
                auto opt_span_id = cache.GetSpanId(id);
                return opt_span_id.has_value() ? opt_span_id.value() : CalcMatched(id);
            };
        };

        std::vector<SlotSpanVector<char>> cache;

        std::stable_sort(
            items.begin(), items.end(),
            [&](size_t item_a, size_t item_b)
            {
                for (auto &atom : query.sort_atoms)
                {
                    std::strong_ordering cmp;
                    if (atom.sort_by_matched)
                    {
                        for (size_t f = 0; f < query.inclusion_filters.size(); ++f)
                        {
                            auto &c = f < cache.size() ? cache[f] : cache.emplace_back();
                            MatchedSortObject obj{
                                .device = device,
                                .cache = c,
                                .filter = query.inclusion_filters[f],
                                .negated = atom.is_negated,
                            };
                            auto a_span_id = obj.GetOrCalcMatched(item_a);
                            auto b_span_id = obj.GetOrCalcMatched(item_b);
                            auto a_matched = c.CGet(a_span_id);
                            auto b_matched = c.CGet(b_span_id);
                            cmp = a_matched <=> b_matched;
                            if (cmp != 0)
                                return (cmp < 0) != atom.is_negated;
                        }
                    }
                    else
                    {
                        auto prop_id = atom.prop_id;
                        auto &prop = *device.props[prop_id];
                        auto str_a = prop.GetSearchableStr(prop.GetStrId(item_a));
                        auto str_b = prop.GetSearchableStr(prop.GetStrId(item_b));
                        cmp = str_a.text <=> str_b.text;
                        if (cmp != 0)
                            return (cmp < 0) != atom.is_negated;
                    }
                }
                return false;
            }
        );
    }

    void Device::RunQuery(Query &query, std::vector<index_t> &items) const
    {
        if (!query.filters.empty())
        {
            RunFilters(*this, query.filters, items);
        }

        if (query.reverse_output)
        {
            std::reverse(items.begin(), items.end());
        }

        if (!query.sort_atoms.empty())
        {
            SortItems(*this, query, items);
        }
    }

    void Device::GetFeedback(Query &query, Feedback &out, bool verbose) const
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
                metas[filter.meta_prop_id].name.GetRenderableString(meta_name);
                auto meta_name_str = (std::string_view)meta_name;
                auto cond = filter.is_exclusion ? ControlColor "not " CloseColor : "";
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
                if (arg.sort_target_id == 0)
                {
                    sort_target_name = "Default";
                }
                else
                {
                    FixedVector<char, 128> name;
                    GetSortTargetName(this->metas, arg.sort_target_id).GetRenderableString(name);
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

    static void CalcHL(const Filtering::Device &device, Query &q, size_t prop_id, LoweredStringView lowered, std::vector<uint16_t> &hl)
    {
        if (!lowered.text.empty())
        {
            auto hl_size_before = hl.size();
            for (auto &filter : q.inclusion_filters)
            {
                auto propset = device.metas[filter.meta_prop_id].propset;
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

    ResultItem Device::CalcItemResult(Query &q, size_t prop_id, index_t item_id) const
    {
        auto &prop = *props[prop_id];
        auto str_id = prop.GetStrId(item_id);
        ResultItem result;
        result.searchable_text = prop.GetSearchableStr(str_id);
        CalcHL(*this, q, prop_id, result.searchable_text, result._hl_storage);
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

    ResultItem Device::CalcPropResult(Query &q, size_t prop_id) const
    {
        ResultItem result;
        auto meta_prop_id = props.size();
        auto relevant_metas = relevant_metas_per_prop.CGet(prop_id);
        if (!relevant_metas.empty())
        {
            constexpr std::string_view join = SkillDull ", " CloseColor;
            for (size_t i = 0; i < relevant_metas.size(); ++i)
            {
                auto &relevant_meta = relevant_metas[i];

                size_t offset = result.presentable_text.size();
                LoweredStringView lowered = metas[relevant_meta.meta_id].name;
                if (lowered.text.empty()) // We allow empty meta names; just skip
                    continue;
                auto size = result._hl_storage.size();
                CalcHL(*this, q, meta_prop_id, lowered, result._hl_storage);
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
}
