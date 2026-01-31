#include <texture_module.h>
#include <utils.h>

#include <parsing.h>

#include "rich_text.h"

namespace HerosInsight::RichText
{
    using namespace Parsing;

    using ColorOpenPattern = std::tuple<Lit<"<c=">, Color<&ColorTag::color>, Lit<">">>;
    using ColorClosePattern = std::tuple<Lit<"</c>">>;

    using TooltipOpenPattern = std::tuple<Lit<"<tip=">, Int<10, &TooltipTag::id>, Lit<">">>;
    using TooltipClosePattern = std::tuple<Lit<"</tip>">>;

    using FracPattern = std::tuple<Lit<"<frac=">, Int<10, &FracTag::num>, Lit<"/">, Int<10, &FracTag::den>, Lit<">">>;

    void ColorTag::ToChars(OutBuf<char> output) const
    {
        if (color)
            Parsing::write_pattern<ColorOpenPattern>(output, this);
        else
            Parsing::write_pattern<ColorClosePattern>(output, this);
    }

    bool ColorTag::TryRead(std::string_view &remaining, ColorTag &out)
    {
        if (Parsing::read_pattern<ColorOpenPattern>(remaining, &out))
            return true;

        if (Parsing::read_pattern<ColorClosePattern>(remaining, &out))
        {
            out.color = NULL;
            return true;
        }
        return false;
    }

    void TooltipTag::ToChars(OutBuf<char> output) const
    {
        if (id != -1)
            Parsing::write_pattern<TooltipOpenPattern>(output, this);
        else
            Parsing::write_pattern<TooltipClosePattern>(output, this);
    }

    bool TooltipTag::TryRead(std::string_view &remaining, TooltipTag &out)
    {
        if (Parsing::read_pattern<TooltipOpenPattern>(remaining, &out))
            return true;

        if (Parsing::read_pattern<TooltipClosePattern>(remaining, &out))
        {
            out.id = -1;
            return true;
        }
        return false;
    }

    bool FracTag::TryRead(std::string_view &remaining, FracTag &out) { return Parsing::read_pattern<FracPattern>(remaining, &out); }
    std::string_view FracTag::Find(std::string_view text, FracTag &out) { return Parsing::find_pattern<FracPattern>(text, &out); }

    float TextSegment::CalcWidth(TextFracProvider *frac_provider) const
    {
        if (auto frac_tag = std::get_if<FracTag>(&tag))
        {
            assert(frac_provider != nullptr);
            return frac_provider->CalcWidth(*frac_tag);
        }
        else
        {
            return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
        }

        assert(false && "Unknown text segment type");
        return 0;
    }

    void ExtractTags(std::string_view text_with_tags, std::span<char> &only_text, std::span<TextTag> &only_tags)
    {
        size_t tag_count = 0;
        size_t write_offset = 0;
        auto rem = text_with_tags;
        TextTag dummy;
        bool write_text = only_text.data() != nullptr;
        bool write_tags = only_tags.data() != nullptr;
        while (!rem.empty())
        {
            TextTag &tag = write_tags ? only_tags[tag_count] : dummy;
            auto tag_str = TextTag::Find(rem, tag);
            bool has_tag = tag_str.data() != nullptr;

            size_t copy_count = has_tag ? tag_str.data() - rem.data() : rem.size();

            if (write_text)
            {
                assert(write_offset + copy_count <= only_text.size());
                std::memmove(&only_text[write_offset], rem.data(), copy_count); // We use memmove in case text_with_tags and only_text overlap
                write_offset += copy_count;
            }

            if (has_tag)
            {
                if (write_tags)
                {
                    ++tag_count;
                    tag.offset = write_offset;
                }

                // if (write_text && !tag.IsZeroWidth())
                // {
                //     // Add placeholder char to mark a visible tag
                //     assert(write_offset < only_text.size());
                //     only_text[write_offset] = TextTag::TAG_MARKER_CHAR;
                //     ++write_offset;
                // }
            }

            rem.remove_prefix(copy_count + tag_str.size());
        }
        only_text = std::span<char>{only_text.data(), write_offset};
        only_tags = std::span<TextTag>{only_tags.data(), tag_count};
    }

    void Drawer::MakeTextSegments(std::string_view text, OutBuf<TextSegment> result, std::span<uint16_t> highlighting, TextSegment::WrapMode first_segment_wrap_mode)
    {
        FixedVector<ImU32, 32> color_stack;
        FixedVector<uint32_t, 32> tooltip_stack;

        bool is_currently_highlighted = false;
        bool has_hidden_hl = false;
        size_t i_hl = 0;
        size_t i_hl_change = highlighting.empty() ? std::numeric_limits<size_t>::max() : highlighting[0];

        auto AddSegment = [&](size_t i_start, size_t i_end, std::optional<TextTag> tag = std::nullopt)
        {
            auto index = result.size();
            auto &seg = result.emplace_back();

            seg.text = text.substr(i_start, i_end - i_start);
            if (!color_stack.empty())
                seg.color = color_stack.back();
            if (!tooltip_stack.empty())
                seg.tooltip_id = tooltip_stack.back();
            seg.is_highlighted = is_currently_highlighted;
            seg.has_hidden_hl = has_hidden_hl;
            has_hidden_hl = false;

            if (index == 0)
            {
                seg.wrap_mode = first_segment_wrap_mode;
            }
            else if (seg.text.empty())
            {
                seg.wrap_mode = TextSegment::WrapMode::Disallow;
            }
            else if (text[i_start] == '\n')
            {
                seg.wrap_mode = TextSegment::WrapMode::Force;
            }
            else
            {
                auto &prev_seg = result[index - 1];
                bool prev_is_visible = std::holds_alternative<FracTag>(prev_seg.tag) ||
                                       (prev_seg.text.back() != ' ');
                bool is_visible = std::holds_alternative<FracTag>(seg.tag) ||
                                  (seg.text.front() != ' ');
                bool can_wrap = !prev_is_visible && is_visible;
                seg.wrap_mode = can_wrap ? TextSegment::WrapMode::Allow : TextSegment::WrapMode::Disallow;
            }

            if (tag.has_value())
            {
                auto tag_val = tag.value();
                switch (tag_val.type)
                {
                    case TextTagType::Frac:
                    {
                        seg.tag = tag_val.tag.frac_tag;
                        seg.width = frac_provider->CalcWidth(tag_val.tag.frac_tag);
                        break;
                    }
                    default:
                    {
                        assert(false);
                    }
                }
            }
            else
            {
                seg.tag = std::monostate{};
                seg.width = Utils::CalcExactTextSize(seg.text.data(), seg.text.data() + seg.text.size()).x;
                // seg.width = ImGui::CalcTextSize(seg.text.data(), seg.text.data() + seg.text.size()).x;
            }
        };

        size_t i_start = 0;
        for (size_t i = 0; i <= text.size();)
        {
            TextTag tag;
            auto rem = text.substr(i);
            bool has_tag = i < text.size() && text[i] == '<' && TextTag::TryRead(rem, tag);
            bool is_hl_change = i >= i_hl_change;
            bool is_new_line = i < text.size() && text[i] == '\n';
            bool is_new_word = i > 0 && i < text.size() && text[i - 1] == ' ' && text[i] != ' ';

            bool flush = has_tag || is_hl_change || is_new_line || is_new_word || i == text.size();
            if (!flush)
            {
                ++i;
                continue;
            }

            if (i > i_start)
            {
                AddSegment(i_start, i);
                i_start = i;
            }

            if (is_hl_change)
            {
                bool was_highlighted = is_currently_highlighted;
                do
                {
                    ++i_hl;
                    i_hl_change = i_hl < highlighting.size() ? highlighting[i_hl] : std::numeric_limits<size_t>::max();
                } while (i_hl_change <= i);
                is_currently_highlighted = (i_hl & 1) == 1;
                has_hidden_hl = !was_highlighted && !is_currently_highlighted;
            }

            if (has_tag)
            {
                i = rem.data() - text.data();
                switch (tag.type)
                {
                    case TextTagType::Frac:
                    {
                        AddSegment(i_start, i, tag);
                        break;
                    }
                    case TextTagType::Color:
                    {
                        auto &color_tag = tag.tag.color_tag;
                        if (color_tag.color == 0)
                        {
                            if (!color_stack.empty()) // GW sometimes closes colors without opening
                                color_stack.pop_back();
                        }
                        else
                        {
                            color_stack.push_back(color_tag.color);
                        }
                        break;
                    }
                    case TextTagType::Tooltip:
                    {
                        auto &tooltip_tag = tag.tag.tooltip_tag;
                        if (tooltip_tag.id == -1)
                        {
                            tooltip_stack.pop_back();
                        }
                        else
                        {
                            tooltip_stack.push_back(tooltip_tag.id);
                        }
                        break;
                    }
                    default:
                    {
                        assert(false);
                    }
                }
                i_start = i;
            }
            else if (is_new_line)
            {
                ++i; // Skip \n
                AddSegment(i_start, i);
                i_start = i;
            }
            else
            {
                ++i;
            }
        }
        if (!has_hidden_hl)
        {
            has_hidden_hl = i_hl < highlighting.size();
        }
        if (has_hidden_hl)
        {
            AddSegment(text.size(), text.size());
        }
        assert(tooltip_stack.empty());
        assert(color_stack.empty());
    }

    void Drawer::DrawTextSegments(std::span<TextSegment> segments, float wrapping_min, float wrapping_max)
    {
        if (segments.empty())
            return;

        float max_width = wrapping_max < 0 ? std::numeric_limits<float>::max() : wrapping_max - wrapping_min;
        float used_width = ImGui::GetCursorPosX() - wrapping_min;
        auto ss_cursor = ImGui::GetCursorScreenPos();
        const auto style = ImGui::GetStyle();
        const auto text_height = ImGui::GetTextLineHeight();

        auto window = ImGui::GetCurrentWindow();
        auto draw_list = window->DrawList;

        const auto highlight_color = ImGui::GetColorU32(Constants::Colors::highlight);
        const auto highlight_text_color = ImGui::GetColorU32(IM_COL32_BLACK);

        // Screen-space bounding box
        ImRect bb = ImRect(
            std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
            std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()
        );

        size_t n_segments = segments.size();
        // i_rem is the start of remaining segments or end of done segments
        for (size_t i_rem = 0; i_rem < n_segments;)
        {
            // Discover how much we can draw before wrapping (i_wrap).
            size_t i_wrap = n_segments;
            std::optional<size_t> i_wrap_allowed = std::nullopt;
            for (size_t i = i_rem; i < n_segments; ++i)
            {
                auto &seg = segments[i];
                if (used_width > 0.f) // We only allow wrapping if we are not directly at the start
                {
                    if (seg.wrap_mode == TextSegment::WrapMode::Force)
                    {
                        i_wrap = i;
                        break;
                    }

                    if (seg.wrap_mode == TextSegment::WrapMode::Allow)
                    {
                        i_wrap_allowed = i;
                    }
                }

                used_width += seg.width;
                if (used_width >= max_width && i_wrap_allowed.has_value())
                {
                    i_wrap = i_wrap_allowed.value();
                    break;
                }
            }

            // Draw segments in the range [i_rem, i_wrap)
            for (; i_rem < i_wrap; ++i_rem)
            {
                auto &seg = segments[i_rem];
                auto segment_size = ImVec2(seg.width, text_height);
                auto min = ss_cursor;
                auto max = min + segment_size;

                bb.Add(ImRect(min, max));

                if (seg.color.has_value())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, seg.color.value());
                }

                if (seg.is_highlighted)
                {
                    auto min_aligned = ImFloor(ImVec2(min.x, min.y + 1));
                    auto max_aligned = ImFloor(max);
                    draw_list->AddRectFilled(min_aligned, max_aligned, highlight_color);
                    ImGui::PushStyleColor(ImGuiCol_Text, highlight_text_color);
                }

                if (seg.has_hidden_hl)
                {
                    auto min_aligned = ImFloor(ImVec2(min.x, min.y + 1));
                    auto max_aligned = ImFloor(ImVec2(min.x + 1, max.y));
                    draw_list->AddRectFilled(min_aligned, max_aligned, highlight_color);
                }

                // Draw segment content
                if (std::holds_alternative<std::monostate>(seg.tag))
                {
                    ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
                    draw_list->AddText(min, text_color, seg.text.data(), seg.text.data() + seg.text.size());
                }
                else if (auto frac_tag = std::get_if<FracTag>(&seg.tag))
                {
                    assert(frac_provider != nullptr);
                    frac_provider->DrawFraction(min, *frac_tag);
                }
                else
                {
                    assert(false && "Unknown TextSegment type");
                }

                if (seg.tooltip_id.has_value())
                {
                    assert(tooltip_provider != nullptr);
                    if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(min, max))
                    {
                        tooltip_provider->DrawTooltip(seg.tooltip_id.value());
                    }
                }

                if (seg.is_highlighted)
                {
                    ImGui::PopStyleColor();
                }

                if (seg.color.has_value())
                {
                    ImGui::PopStyleColor();
                }

                ss_cursor.x += seg.width;
            }

            // Make newline
            used_width = 0.f;
            ss_cursor.x = window->ContentRegionRect.Min.x + wrapping_min;
            ss_cursor.y += text_height;
        }

        assert(!bb.IsInverted());

        ImGui::ItemSize(bb);
        ImGui::ItemAdd(bb, 0);
        ss_cursor.y += style.ItemSpacing.y;
        ImGui::SetCursorScreenPos(ss_cursor);
    }

    void Drawer::DrawRichText(std::string_view text, float wrapping_min, float wrapping_max, std::span<uint16_t> highlighting, TextSegment::WrapMode first_segment_wrap_mode)
    {
        FixedVector<TextSegment, 512> segments;
        MakeTextSegments(text, segments, highlighting, first_segment_wrap_mode);
        DrawTextSegments(segments, wrapping_min, wrapping_max);
    }

    float CalcTextSegmentsWidth(std::span<TextSegment> segments)
    {
        float width = 0.f;
        for (auto &seg : segments)
        {
            width += seg.width;
        }
        return width;
    }

    void WriteTag(TextTagType type, UntypedTextTag tag, OutBuf<char> output)
    {
        switch (type)
        {
            case TextTagType::Color:
                tag.color_tag.ToChars(output);
                break;

            case TextTagType::Tooltip:
                tag.tooltip_tag.ToChars(output);
                break;

            case TextTagType::Frac:
                Parsing::write_pattern<FracPattern>(output, &tag.frac_tag);
                break;

            default:
                assert(false);
        }
    }

    bool TextTag::TryRead(std::string_view &remaining, TextTag &out)
    {
        if (remaining.empty() || remaining[0] != '<')
            return false;

        if (TooltipTag::TryRead(remaining, out.tag.tooltip_tag))
        {
            out.type = TextTagType::Tooltip;
            return true;
        }
        else if (ColorTag::TryRead(remaining, out.tag.color_tag))
        {
            out.type = TextTagType::Color;
            return true;
        }
        else if (FracTag::TryRead(remaining, out.tag.frac_tag))
        {
            out.type = TextTagType::Frac;
            return true;
        }

        return false;
    }
    std::string_view TextTag::Find(std::string_view text, TextTag &out)
    {
        while (true)
        {
            auto potential_tag = text.find('<');
            if (potential_tag == std::string::npos)
                return {};

            text = text.substr(potential_tag);
            auto start = text.data();
            if (TextTag::TryRead(text, out))
            {
                return std::string_view(start, text.data() - start);
            }
            text = text.substr(1); // Skip '<'
        }
    }
}
