#pragma once

#include "catalog.h"
#include <span>
#include <vector>

namespace HerosInsight
{
    namespace CatalogUtils
    {
        void SortHighlighting(std::span<uint16_t> &hl)
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

        void SortHighlighting(std::vector<uint16_t> &hl)
        {
            std::span<uint16_t> span = hl;
            SortHighlighting(span);
            hl.resize(span.size());
        }

        // Returns the index of the "best" match
        std::optional<size_t> BestMatch(std::string_view subject, std::span<std::string_view> targets)
        {
            size_t index;

            auto matcher = Matcher(subject, true);

            size_t best_match_cost = std::numeric_limits<size_t>::max();
            for (size_t i = 0; i < targets.size(); ++i)
            {
                auto target = targets[i];
                bool match = matcher.Matches(target, nullptr);
                if (match)
                {
                    float match_cost = target.size();
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

        bool ParseFilter(std::string_view source, std::span<std::string_view> prop_bundles, Filter &filter)
        {
            { // Skip leading whitespace
                size_t i = 0;

                while (i < source.size() && Utils::IsSpace(source[i]))
                    ++i;

                if (i == source.size())
                    return false;

                source = source.substr(i);
            }
            auto splitter_pos = source.find(':');
            if (splitter_pos != std::string_view::npos)
            {
                auto target_str = source.substr(0, splitter_pos);
                if (!target_str.empty())
                {
                    auto index = CatalogUtils::BestMatch(target_str, prop_bundles);
                    if (!index.has_value())
                        return false;
                    filter.bundle_id = index.value();
                }

                size_t i = splitter_pos + 1;
                while (i < source.size() && Utils::IsSpace(source[i]))
                    ++i;

                source = source.substr(i);
            }

            filter.matcher = Matcher(source, true);

            return true;
        }

        bool TryReadCommand(std::string_view &remaining, std::span<std::string_view> prop_bundles, Command &command)
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
                    auto index = BestMatch(target_text, prop_bundles);
                    if (!index.has_value())
                        return false;
                    sort_arg.target_bundle = index.value();

                    rem = rem.substr(comma_pos + 1);
                }
            }
            else
            {
                return false;
            }

            return true;
        }

        void ParseQuery(std::string_view source, std::span<std::string_view> prop_bundles, Query &query)
        {
            char *p = (char *)source.data();
            char *end = p + source.size();

            while (p < end)
            {
                auto stmt_start = p;
                while (p < end && *p != ';')
                    ++p;
                auto stmt_end = p;
                std::string_view stmt(stmt_start, stmt_end - stmt_start);

                if (*stmt_start == '/')
                {
                    query.commands.emplace_back();
                    auto &command = query.commands.back();
                    if (!TryReadCommand(stmt, prop_bundles, command))
                        query.commands.pop_back();
                }
                else
                {
                    query.filters.emplace_back();
                    auto &filter = query.filters.back();
                    if (!ParseFilter(stmt, prop_bundles, filter))
                        query.filters.pop_back();
                }
            }
        }

        void GetFeedback(Query &query, std::span<std::string_view> &prop_bundle_names, Utils::RichString &out)
        {
            out.str.clear();
            out.color_changes.clear();
            for (auto &filter : query.filters)
            {
                // if (!filter.IsValid())
                // {
                //     out.color_changes.push_back({out.str.size(), Constants::GWColors::skill_dull_gray});
                // }

#ifdef _DEBUG
                out.str += filter.matcher.ToString();
#endif

                // const auto target_name = filter.target.ToStr();
                // const auto op_desc = GetOpDescription(filter.target, filter.op).data();
                // bool is_string = filter.target.IsStringType();
                // bool is_number = filter.target.IsNumberType();

                // if (filter.join == FilterJoin::Or)
                // {
                //     out.str += "OR ";
                // }
                // else if (filter.join == FilterJoin::And)
                // {
                //     out.str += "AND ";
                // }

                // if (is_string)
                // {
                //     Utils::AppendFormatted(out.str, 64, "%s %s: ", target_name.data(), op_desc);
                // }
                // else if (is_number)
                // {
                //     Utils::AppendFormatted(out.str, 128, "%s %s ", target_name.data(), op_desc);
                // }
                // else
                // {
                //     out.str += "...";
                // }

                // auto value_str_len = filter.value_end - filter.value_start;

                // const auto n_values = filter.str_values.size();
                // for (uint32_t i = 0; i < n_values; i++)
                // {
                //     auto filt_str = filter.str_values[i];
                //     if (is_number && filt_str.size() == 0)
                //         filt_str = "...";
                //     auto kind = i == 0             ? 0
                //                 : i < n_values - 1 ? 1
                //                                    : 2;
                //     // clang-format off
                //     if (kind == 1)      out.str += ", ";
                //     else if (kind == 2) out.str += " or ";
                //     if (is_string)      out.str += "\'";
                //                         out.str += filt_str;
                //     if (is_string)      out.str += "\'";
                //     // if (kind = 2)       out.str += ".";
                //     // clang-format on
                // }

                // if (n_values == 0 && is_number)
                // {
                //     out.str += "...";
                // }

                // if (!filter.IsValid())
                // {
                //     out.color_changes.push_back({out.str.size(), 0});
                // }

                out.str += "\n";
            }

            for (auto &command : query.commands)
            {
                if (std::holds_alternative<SortCommand>(command))
                {
                    auto &sort_command = std::get<SortCommand>(command);

                    const auto n_values = sort_command.args.size();
                    bool is_incomplete = n_values == 0;
                    if (is_incomplete)
                    {
                        out.color_changes.push_back({out.str.size(), Constants::GWColors::skill_dull_gray});
                    }

                    out.str += is_incomplete ? "Sort by ..." : "Sort";
                    for (uint32_t i = 0; i < n_values; i++)
                    {
                        const auto &arg = sort_command.args[i];
                        auto kind = i == 0             ? 0
                                    : i < n_values - 1 ? 1
                                                       : 2;

                        // clang-format off
                    if (kind == 1)      out.str += ", then";
                    else if (kind == 2) out.str += " and then";
                    if (arg.is_negated) out.str += " descending by ";
                    else                out.str += " ascending by ";
                                        out.str += prop_bundle_names[arg.target_bundle];
                        // clang-format on
                    }

                    if (is_incomplete)
                    {
                        out.color_changes.push_back({out.str.size(), 0});
                    }
                }
                out.str += "\n";
            }
        }
    }
}
