#include <texture_module.h>
#include <utils.h>

#include "rich_text.h"

namespace HerosInsight::RichText
{
    using DrawPacketArray = std::array<TextureModule::DrawPacket, DefaultTextImageProvider::Id::COUNT>;
    DrawPacketArray CreatePackets()
    {
        namespace FileIDs = TextureModule::KnownFileIDs;
        using Id = DefaultTextImageProvider::Id;

        DrawPacketArray packets;
        const auto size = ImVec2(16, 16);
        packets[Id::MonsterSkull] = TextureModule::GetPacket_ImageInAtlas(FileIDs::UI_SkillStatsIcons, size, size, 5);
        packets[Id::EnergyOrb] = TextureModule::GetPacket_ImageInAtlas(FileIDs::UI_SkillStatsIcons, size, size, 1);

        return packets;
    }
    static DrawPacketArray packets = CreatePackets();

    DefaultTextImageProvider &DefaultTextImageProvider::Instance()
    {
        static DefaultTextImageProvider instance;
        return instance;
    }
    void DefaultTextImageProvider::Draw(ImVec2 pos, size_t id)
    {
        auto &packet = packets[id];
        auto window = ImGui::GetCurrentWindow();
        auto draw_list = window->DrawList;
        packet.AddToDrawList(draw_list, pos);
    }
    float DefaultTextImageProvider::GetWidth(size_t id)
    {
        auto &packet = packets[id];
        return packet.size.x;
    }

    float TextSegment::GetWidth(TextImageProvider *image_provider) const
    {
        if (auto image_id = std::get_if<size_t>(&text_or_image_id))
        {
            assert(image_provider != nullptr);
            return image_provider->GetWidth(*image_id);
        }
        else if (auto text = std::get_if<std::string_view>(&text_or_image_id))
        {
            return ImGui::CalcTextSize(text->data(), text->data() + text->size()).x;
        }
        else
        {
            assert(false);
        }
    }

    bool TryReadTextTag(std::string_view &remaining, TextTag &out)
    {
        auto rem = remaining;
        if (!Utils::TryRead('<', rem))
            return false;

        bool is_closing = Utils::TryRead('/', rem);

        if (Utils::TryRead('c', rem))
        {
            auto &tag = out.emplace<ColorTag>();
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
            auto &tag = out.emplace<TooltipTag>();
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
                auto &tag = out.emplace<ImageTag>();
                tag.id = id;
                remaining = rem;
                return true;
            }
        }
        else if (Utils::TryRead("frac=", rem))
        {
            assert(!is_closing);
            int32_t num, denom;
            if (Utils::TryReadInt(rem, num) &&
                Utils::TryRead('/', rem) &&
                Utils::TryReadInt(rem, denom) &&
                Utils::TryRead('>', rem))
            {
                auto &tag = out.emplace<FracTag>();
                tag.num = num;
                tag.denom = denom;
                remaining = rem;
                return true;
            }
        }

        return false;
    }

    std::string_view FindTextTag(std::string_view text, TextTag &out)
    {
        while (true)
        {
            auto potential_tag = text.find('<');
            if (potential_tag == std::string::npos)
                return {};

            text = text.substr(potential_tag);
            auto start = text.data();
            if (TryReadTextTag(text, out))
            {
                return std::string_view(start, text.data() - start);
            }
            text = text.substr(1);
        }
    }

    void MakeTextSegments(std::string_view text, std::span<uint16_t> highlighting, std::span<TextSegment> &result)
    {
        if (text.empty())
        {
            result = {};
            return;
        }

        size_t result_len = 0;
        FixedArrayRef<TextSegment> result_builder{result, result_len};
        FixedArray<ImU32, 32> color_buffer;
        auto color_stack = color_buffer.ref();
        FixedArray<uint32_t, 32> tooltip_buffer;
        auto tooltip_stack = tooltip_buffer.ref();

        bool is_currently_highlighted = false;
        size_t i_hl = 0;
        int32_t i_hl_change = highlighting.empty() ? -1 : highlighting[0];

        auto AppendTextSegment = [&](size_t i_start, size_t i_end, bool can_wrap_after) -> void
        {
            auto &seg = result_builder.emplace_back();
            auto seg_text = std::string_view(text.data() + i_start, i_end - i_start);
            seg.text_or_image_id = seg_text;
            if (!color_stack.empty())
                seg.color = color_stack.back();
            if (!tooltip_stack.empty())
                seg.tooltip_id = tooltip_stack.back();
            seg.is_highlighted = is_currently_highlighted;
            seg.can_wrap_after = can_wrap_after;
        };

        char prev_visible_element = false;
        size_t i_start = 0;
        size_t i = 0;
        while (i <= text.size())
        {
            auto c = i < text.size() ? text[i] : '\0';
            bool is_end = i == text.size();
            bool is_hl_change = i >= i_hl_change;
            bool is_word_start = prev_visible_element && c != ' ';

            auto view = std::string_view(text.data() + i, text.size() - i);
            TextTag tag;
            if (c == '<' && TryReadTextTag(view, tag))
            {
                bool is_image_tag = std::holds_alternative<ImageTag>(tag);
                if (i != i_start)
                {
                    bool can_wrap_after = is_word_start && is_image_tag;
                    AppendTextSegment(i_start, i, can_wrap_after);
                }

                if (auto image_tag = std::get_if<ImageTag>(&tag))
                {
                    auto &seg = result_builder.emplace_back();
                    seg.text_or_image_id = image_tag->id;
                    seg.is_highlighted = is_currently_highlighted;
                    seg.can_wrap_after = false;
                }
                else if (auto color_tag = std::get_if<ColorTag>(&tag))
                {
                    if (color_tag->color == 0)
                        color_stack.pop();
                    else
                        color_stack.push_back(color_tag->color);
                }
                else if (auto tooltip_tag = std::get_if<TooltipTag>(&tag))
                {
                    if (tooltip_tag->id == -1)
                        tooltip_stack.pop();
                    else
                        tooltip_stack.push_back(tooltip_tag->id);
                }
                else
                {
                    assert(false);
                }

                if (is_image_tag)
                {
                    prev_visible_element = true;
                }

                i = view.data() - text.data();
            }
            else
            {
                if (i != i_start && (is_word_start || is_hl_change || is_end))
                {
                    AppendTextSegment(i_start, i, is_word_start);
                }

                prev_visible_element = c != ' ';
                ++i;
            }

            if (is_hl_change)
            {
                do
                {
                    ++i_hl;
                    i_hl_change = i_hl < highlighting.size() ? highlighting[i_hl] : -1;
                } while (i_hl_change <= i);
                is_currently_highlighted = i_hl & 1;
            }
        }

        result = result.subspan(0, result_len);
    }

    void DrawTextSegments(
        std::span<TextSegment> segments,
        float wrapping_min, float wrapping_max,
        TextTooltipProvider *tooltip_provider,
        TextImageProvider *image_provider)
    {
        if (segments.empty())
            return;

        float max_width = wrapping_max < 0 ? std::numeric_limits<float>::max() : wrapping_max - wrapping_min;
        float used_width = ImGui::GetCursorPosX() - wrapping_min;
        auto ss_cursor = ImGui::GetCursorScreenPos();
        const auto text_height = ImGui::GetTextLineHeight();

        bool can_wrap = used_width > 0.f;

        auto window = ImGui::GetCurrentWindow();
        auto draw_list = window->DrawList;

        const auto highlight_color = ImGui::GetColorU32(IM_COL32(250, 148, 54, 255));
        const auto highlight_text_color = ImGui::GetColorU32(IM_COL32_BLACK);

        // Screen-space bounding box
        ImRect bb = ImRect(
            std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
            std::numeric_limits<float>::min(), std::numeric_limits<float>::min());

        for (auto &seg : segments)
        {
            float seg_width = seg.GetWidth(image_provider);
            used_width += seg_width;
            if (can_wrap && used_width > max_width)
            {
                ss_cursor.x = window->ContentRegionRect.Min.x + wrapping_min;
                ss_cursor.y += text_height;
                used_width = seg_width;
            }

            auto segment_size = ImVec2(seg_width, text_height);
            auto min = ss_cursor;
            auto max = min + segment_size;

            bb.Add(ImRect(min, max));

            if (seg.is_highlighted)
            {
                draw_list->AddRectFilled(min, max, highlight_color);
            }

            // Draw segment content
            if (auto text = std::get_if<std::string_view>(&seg.text_or_image_id))
            {
                ImU32 text_color;
                if (seg.is_highlighted)
                    text_color = highlight_text_color;
                else if (seg.color.has_value())
                    text_color = seg.color.value();
                else
                    text_color = ImGui::GetColorU32(ImGuiCol_Text);

                draw_list->AddText(min, text_color, text->data(), text->data() + text->size());
            }
            else if (auto image_id = std::get_if<size_t>(&seg.text_or_image_id))
            {
                image_provider->Draw(min, *image_id);
            }
            else
            {
                assert(false && "Unknown TextSegment type");
            }

            if (seg.tooltip_id.has_value())
            {
                if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(min, max))
                {
                    tooltip_provider->DrawTooltip(seg.tooltip_id.value());
                }
            }

            ss_cursor.x += seg_width;
            can_wrap = seg.can_wrap_after;
        }

        assert(!bb.IsInverted());

        ImGui::ItemSize(bb);
        ImGui::ItemAdd(bb, 0);
    }

    void DrawRichText(
        std::string_view text,
        float wrapping_min, float wrapping_max,
        std::span<uint16_t> highlighting,
        TextTooltipProvider *tooltip_provider,
        TextImageProvider *image_provider)
    {
        TextSegment segments[512];
        std::span<TextSegment> seg_view = segments;
        MakeTextSegments(text, highlighting, seg_view);
        DrawTextSegments(seg_view, wrapping_min, wrapping_max, tooltip_provider, image_provider);
    }
}
