#include <texture_module.h>
#include <utils.h>

#include "rich_text.h"

namespace HerosInsight::RichText
{
    TextureModule::DrawPacket GetImagePacket(uint32_t id)
    {
        using Id = DefaultTextImageProvider::Id;

        std::optional<uint32_t> stats_atlas_index = std::nullopt;
        // clang-format off
        switch (id)
        {
            case Id::Upkeep:       stats_atlas_index = 0;  break;
            case Id::EnergyOrb:    stats_atlas_index = 1;  break;
            case Id::Aftercast:
            case Id::Activation:   stats_atlas_index = 2;  break;
            case Id::Recharge:     stats_atlas_index = 3;  break;
            case Id::Adrenaline:   stats_atlas_index = 4;  break;
            case Id::MonsterSkull: stats_atlas_index = 5;  break;
            case Id::Sacrifice:    stats_atlas_index = 7;  break;
            case Id::Overcast:     stats_atlas_index = 10; break;
        }
        // clang-format on

        if (stats_atlas_index.has_value())
        {
            const auto size = ImVec2(16, 16);
            auto packet = TextureModule::GetPacket_ImageInAtlas(TextureModule::KnownFileIDs::UI_SkillStatsIcons, size, size, stats_atlas_index.value());
            if (id == Id::Aftercast)
            {
                packet.tint_col = ImVec4(0.3f, 0.33f, 0.7f, 1.f);
            }
            return packet;
        }

        assert(false && "Unknown image id");
        return TextureModule::DrawPacket{};
    }

    DefaultTextImageProvider &DefaultTextImageProvider::Instance()
    {
        static DefaultTextImageProvider instance;
        return instance;
    }
    void DefaultTextImageProvider::DrawImage(ImVec2 pos, ImageTag tag)
    {
        auto packet = GetImagePacket(tag.id);
        auto window = ImGui::GetCurrentWindow();
        auto draw_list = window->DrawList;
        auto y_offset = -packet.size.y * 0.1f;
        pos = ImFloor(ImVec2(pos.x, pos.y + y_offset));
        packet.AddToDrawList(draw_list, pos);

// #define DEBUG_POS
#ifdef DEBUG_POS
        draw_list->AddRect(pos, ImVec2(pos.x + packet.size.x, pos.y + packet.size.y), 0xFF0000FF);
#endif
    }
    float DefaultTextImageProvider::CalcWidth(ImageTag tag)
    {
        auto packet = GetImagePacket(tag.id);
        return packet.size.x;
    }

    float TextSegment::CalcWidth(TextImageProvider *image_provider, TextFracProvider *frac_provider) const
    {
        if (auto image_tag = std::get_if<ImageTag>(&tag))
        {
            assert(image_provider != nullptr);
            return image_provider->CalcWidth(*image_tag);
        }
        else if (auto frac_tag = std::get_if<FracTag>(&tag))
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

    bool FracTag::TryRead(std::string_view &remaining, FracTag &out)
    {
        auto rem = remaining;
        if (Utils::TryRead("<frac=", rem))
        {
            int32_t num, denom;
            if (Utils::TryReadInt(rem, num) &&
                Utils::TryRead('/', rem) &&
                Utils::TryReadInt(rem, denom) &&
                Utils::TryRead('>', rem))
            {
                out.num = num;
                out.den = denom;
                remaining = rem;
                return true;
            }
        }
        return false;
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

    void Drawer::MakeTextSegments(std::string_view text, std::span<TextSegment> &result, std::span<uint16_t> highlighting, TextSegment::WrapMode first_segment_wrap_mode)
    {
        if (text.empty())
        {
            result = {};
            return;
        }

        SpanWriter<TextSegment> result_builder = result;
        FixedVector<ImU32, 32> color_stack;
        FixedVector<uint32_t, 32> tooltip_stack;

        bool is_currently_highlighted = false;
        size_t i_hl = 0;
        size_t i_hl_change = highlighting.empty() ? std::numeric_limits<size_t>::max() : highlighting[0];

        auto AddSegment = [&](size_t i_start, size_t i_end, std::optional<TextTag> tag = std::nullopt)
        {
            auto index = result_builder.size();
            auto &seg = result_builder.emplace_back();

            seg.text = text.substr(i_start, i_end - i_start);
            if (!color_stack.empty())
                seg.color = color_stack.back();
            if (!tooltip_stack.empty())
                seg.tooltip_id = tooltip_stack.back();
            seg.is_highlighted = is_currently_highlighted;

            if (index == 0)
            {
                seg.wrap_mode = first_segment_wrap_mode;
            }
            else
            {
                if (text[i_start] == '\n')
                {
                    seg.wrap_mode = TextSegment::WrapMode::Force;
                }
                else
                {
                    auto &prev_seg = result[index - 1];
                    bool prev_is_visible = std::holds_alternative<ImageTag>(prev_seg.tag) ||
                                           std::holds_alternative<FracTag>(prev_seg.tag) ||
                                           (prev_seg.text.back() != ' ');
                    bool is_visible = std::holds_alternative<ImageTag>(seg.tag) ||
                                      std::holds_alternative<FracTag>(seg.tag) ||
                                      (seg.text.front() != ' ');
                    bool can_wrap = !prev_is_visible && is_visible;
                    seg.wrap_mode = can_wrap ? TextSegment::WrapMode::Allow : TextSegment::WrapMode::Disallow;
                }
            }

            if (tag.has_value())
            {
                auto tag_val = tag.value();
                switch (tag_val.type)
                {
                    case TextTag::Type::Image:
                    {
                        seg.tag = tag_val.image_tag;
                        seg.width = image_provider->CalcWidth(tag_val.image_tag);
                        break;
                    }
                    case TextTag::Type::Frac:
                    {
                        seg.tag = tag_val.frac_tag;
                        seg.width = frac_provider->CalcWidth(tag_val.frac_tag);
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
                do
                {
                    ++i_hl;
                    i_hl_change = i_hl < highlighting.size() ? highlighting[i_hl] : std::numeric_limits<size_t>::max();
                } while (i_hl_change <= i);
                is_currently_highlighted = (i_hl & 1) == 1;
            }

            if (has_tag)
            {
                i = rem.data() - text.data();
                switch (tag.type)
                {
                    case TextTag::Type::Image:
                    case TextTag::Type::Frac:
                    {
                        AddSegment(i_start, i, tag);
                        break;
                    }
                    case TextTag::Type::Color:
                    {
                        auto &color_tag = tag.color_tag;
                        if (color_tag.color == 0)
                            color_stack.pop();
                        else
                            color_stack.push_back(color_tag.color);
                        break;
                    }
                    case TextTag::Type::Tooltip:
                    {
                        auto &tooltip_tag = tag.tooltip_tag;
                        if (tooltip_tag.id == -1)
                            tooltip_stack.pop();
                        else
                            tooltip_stack.push_back(tooltip_tag.id);
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

        result = result_builder.WrittenSpan();
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

        const auto highlight_color = ImGui::GetColorU32(IM_COL32(250, 148, 54, 255));
        const auto highlight_text_color = ImGui::GetColorU32(IM_COL32_BLACK);

        // Screen-space bounding box
        ImRect bb = ImRect(
            std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
            std::numeric_limits<float>::min(), std::numeric_limits<float>::min()
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
                if (used_width > 0.f) // We only allow wrapping if we are not direactly at the start
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
                    auto min_aligned = ImFloor(min);
                    auto max_aligned = ImFloor(max);
                    draw_list->AddRectFilled(min_aligned, max_aligned, highlight_color);
                    ImGui::PushStyleColor(ImGuiCol_Text, highlight_text_color);
                }

                // Draw segment content
                if (std::holds_alternative<std::monostate>(seg.tag))
                {
                    ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
                    draw_list->AddText(min, text_color, seg.text.data(), seg.text.data() + seg.text.size());
                }
                else if (auto image_tag = std::get_if<ImageTag>(&seg.tag))
                {
                    assert(image_provider != nullptr);
                    image_provider->DrawImage(min, *image_tag);
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
        TextSegment segments[512];
        std::span<TextSegment> seg_view = segments;
        MakeTextSegments(text, seg_view, highlighting, first_segment_wrap_mode);
        DrawTextSegments(seg_view, wrapping_min, wrapping_max);
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

    void TextTag::ToChars(std::span<char> &out) const
    {
        std::format_to_n_result<char *> result;
        std::string_view to_copy{};
        switch (type)
        {
            case Type::Color:
                if (color_tag.color == NULL)
                    to_copy = "</c>";
                else
                {
                    ImU32 color = color_tag.color;
                    // Swap red and blue channels because that's how GW formats them!
                    color = ((color & 0xFF00FF00) | ((color & 0xFF) << 16) | ((color & 0xFF0000) >> 16));
                    result = std::format_to_n(out.data(), out.size(), "<c=#{:x}>", color);
                }
                break;

            case Type::Tooltip:
                if (tooltip_tag.id == -1)
                    to_copy = "</t>";
                else
                    result = std::format_to_n(out.data(), out.size(), "<t={}>", tooltip_tag.id);
                break;

            case Type::Image:
                result = std::format_to_n(out.data(), out.size(), "<img={}>", image_tag.id);
                break;

            case Type::Frac:
                result = std::format_to_n(out.data(), out.size(), "<frac={}/{}>", frac_tag.num, frac_tag.den);
                break;

            default:
                assert(false);
        }

        if (to_copy.data() != nullptr)
        {
            assert(out.size() >= to_copy.size());
            to_copy.copy(out.data(), to_copy.size());
            out = std::span<char>(out.data(), to_copy.size());
        }
        else
        {
            out = std::span<char>(out.data(), result.size);
        }
    }

    bool TextTag::TryRead(std::string_view &remaining, TextTag &out)
    {
        auto rem = remaining;
        if (!Utils::TryRead('<', rem))
            return false;

        bool is_closing = Utils::TryRead('/', rem);

        if (Utils::TryRead('c', rem))
        {
            auto &tag = out.color_tag;
            out.type = TextTag::Type::Color;
            if (is_closing)
            {
                if (Utils::TryRead('>', rem))
                {
                    tag.color = 0;
                    remaining = rem;
                    return true;
                }
            }
            else if (Utils::TryRead('=', rem))
            {
                if (Utils::TryRead('#', rem))
                {
                    // Literal color tag
                    if (Utils::TryReadHexColor(rem, tag.color) &&
                        Utils::TryRead('>', rem))
                    {
                        remaining = rem;
                        return true;
                    }
                }
                else if (Utils::TryRead('@', rem))
                {
                    // Variable color tag
                    if (Utils::TryRead("SKILLDULL", rem))
                        tag.color = Constants::GWColors::skill_dull_gray;
                    else if (Utils::TryRead("SKILLDYN", rem))
                        tag.color = Constants::GWColors::skill_dynamic_green;
                    else
                        return false;
                    if (Utils::TryRead('>', rem))
                    {
                        remaining = rem;
                        return true;
                    }
                }
            }
        }
        else if (Utils::TryRead("tip", rem))
        {
            auto &tag = out.tooltip_tag;
            out.type = TextTag::Type::Tooltip;
            if (is_closing)
            {
                if (Utils::TryRead('>', rem))
                {
                    tag.id = -1;
                    remaining = rem;
                    return true;
                }
            }
            else if (Utils::TryRead('=', rem) &&
                     Utils::TryReadInt(rem, tag.id) &&
                     Utils::TryRead('>', rem))
            {
                remaining = rem;
                return true;
            }
        }
        else if (Utils::TryRead("img=", rem))
        {
            assert(!is_closing);
            int32_t id;
            if (Utils::TryReadInt(rem, id) &&
                Utils::TryRead('>', rem))
            {
                auto &tag = out.image_tag;
                out.type = TextTag::Type::Image;
                tag.id = id;
                remaining = rem;
                return true;
            }
        }

        {
            auto &tag = out.frac_tag;
            out.type = TextTag::Type::Frac;
            if (FracTag::TryRead(remaining, tag))
            {
                return true;
            }
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
