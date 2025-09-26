#pragma once

#include <variant>

#include <imgui.h>
#include <string_arena.h>

namespace HerosInsight::RichText
{
    struct ColorTag
    {
        ImU32 color; // 0 = pop color
    };

    struct TooltipTag
    {
        int32_t id; // -1 = close tooltip
    };

    struct ImageTag
    {
        uint32_t id;
    };

    struct FracTag
    {
        uint16_t num;
        uint16_t den;

        constexpr uint32_t GetValue() const
        {
            return (static_cast<uint32_t>(num) << 16) | den;
        }

        static bool TryRead(std::string_view &remaining, FracTag &out);
    };

    struct TextTag
    {
        constexpr static char TAG_MARKER_CHAR = 0x1a; // Used to mark the positions of non-zero-width tags in the text

        enum struct Type : uint8_t
        {
            Color,
            Tooltip,
            Image,
            Frac,
        };

        Type type;
        uint8_t _padding{};
        uint16_t offset;

        union
        {
            ColorTag color_tag;
            TooltipTag tooltip_tag;
            ImageTag image_tag;
            FracTag frac_tag;
        };

        bool IsZeroWidth() const
        {
            return type == Type::Color ||
                   type == Type::Tooltip;
        }
    };

    // using TextTag = std::variant<ColorTag, TooltipTag, ImageTag, FracTag>;
    bool TryReadTextTag(std::string_view &remaining, TextTag &out);
    std::string_view FindTextTag(std::string_view text, TextTag &out);

    class TextImageProvider
    {
    public:
        virtual ~TextImageProvider() = default;

        virtual void DrawImage(ImVec2 pos, ImageTag tag) = 0;
        virtual float CalcWidth(ImageTag tag) = 0;
    };

    class DefaultTextImageProvider : public TextImageProvider
    {
    public:
        enum Id : uint32_t
        {
            Upkeep,
            EnergyOrb,
            Activation,
            Aftercast,
            Recharge,
            Adrenaline,
            MonsterSkull,
            Sacrifice,
            Overcast,

            COUNT,
        };

        static DefaultTextImageProvider &Instance();
        void DrawImage(ImVec2 pos, ImageTag tag) override;
        float CalcWidth(ImageTag tag) override;
    };

    class TextTooltipProvider
    {
    public:
        virtual ~TextTooltipProvider() = default;
        virtual void DrawTooltip(uint32_t tooltip_id) = 0;
    };

    class TextFracProvider
    {
    public:
        virtual ~TextFracProvider() = default;
        virtual void DrawFraction(ImVec2 pos, FracTag tag) = 0;
        virtual float CalcWidth(FracTag tag) = 0;
    };

    struct TextSegment
    {
        enum struct WrapMode
        {
            Null,
            Disallow,
            Allow,
            Force,
        };

        std::string_view text;
        std::variant<std::monostate, ImageTag, FracTag> tag;
        float width;
        std::optional<ImU32> color = std::nullopt;
        std::optional<uint16_t> tooltip_id = std::nullopt;
        bool is_highlighted = false;
        WrapMode wrap_mode = WrapMode::Null;

        float CalcWidth(TextImageProvider *image_provider, TextFracProvider *frac_provider) const;
    };

    struct RichText
    {
        std::span<char> text;
        std::span<TextTag> tags;

        std::string_view Text() const { return std::string_view(text); }
    };

    void ExtractTags(std::string_view text_with_tags, std::span<char> &only_text, std::span<TextTag> &only_tags);

    struct RichTextArena
    {
        IndexedStringArena<char> text;
        IndexedStringArena<TextTag> tags;

        void Reset()
        {
            text.Reset();
            tags.Reset();
        }

        void ReserveFromHint(const std::string &id)
        {
            text.ReserveFromHint(id + "_text");
            tags.ReserveFromHint(id + "_tags");
        }

        void StoreCapacityHint(const std::string &id)
        {
            text.StoreCapacityHint(id + "_text");
            tags.StoreCapacityHint(id + "_tags");
        }

        void BeginWrite()
        {
            text.BeginWrite();
            tags.BeginWrite();
        }

        void EndWrite(size_t index)
        {
            text.EndWrite(index);
            tags.EndWrite(index);
        }

        RichText GetIndexed(size_t index)
        {
            return {
                text.GetIndexed(index),
                tags.GetIndexed(index)
            };
        }

        template <class Writer>
            requires std::invocable<Writer, std::span<char> &, std::span<TextTag> &>
        void AppendWriteBuffers(Writer &&writer)
        {
            this->text.AppendWriteBuffer(
                1024,
                [&](std::span<char> &text_span)
                {
                    this->tags.AppendWriteBuffer(
                        64,
                        [&](std::span<TextTag> &tags_span)
                        {
                            writer(text_span, tags_span);
                        }
                    );
                }
            );
        }

        void PushRichText(std::string_view text_with_tags)
        {
            this->AppendWriteBuffers(
                [&](std::span<char> &text_span, std::span<TextTag> &tags_span)
                {
                    ExtractTags(text_with_tags, text_span, tags_span);
                }
            );
        }

        void PushText(std::string_view text)
        {
            this->text.append_range(text);
        }

        void PushTag(TextTag tag)
        {
            tag.offset = this->text.GetWrittenSize();
            if (!tag.IsZeroWidth())
            {
                this->text.push_back(TextTag::TAG_MARKER_CHAR);
            }
            this->tags.push_back(tag);
        }

        void PushColorTag(ImU32 color)
        {
            this->PushTag(
                TextTag{
                    .type = TextTag::Type::Color,
                    .color_tag = {color}
                }
            );
        }

        void PushImageTag(uint32_t image_id)
        {
            PushTag(
                TextTag{
                    .type = TextTag::Type::Image,
                    .image_tag = {image_id}
                }
            );
        }

        void PushColoredText(ImU32 color, std::string_view text)
        {
            TextTag tag;
            tag.type = TextTag::Type::Color;
            tag.color_tag = {color};
            this->PushTag(tag);
            this->PushText(text);
            tag.color_tag = {0};
            this->PushTag(tag);
        }
    };

    struct Drawer
    {
        TextTooltipProvider *tooltip_provider = nullptr;
        TextImageProvider *image_provider = nullptr;
        TextFracProvider *frac_provider = nullptr;

        void MakeTextSegments(std::string_view text, std::span<TextSegment> &result, std::span<uint16_t> highlighting = {}, TextSegment::WrapMode first_segment_wrap_mode = TextSegment::WrapMode::Disallow);
        void DrawTextSegments(std::span<TextSegment> segments, float wrapping_min = 0, float wrapping_max = -1);
        void DrawRichText(std::string_view text, float wrapping_min = 0, float wrapping_max = -1, std::span<uint16_t> highlighting = {});
    };

    float CalcTextSegmentsWidth(std::span<TextSegment> segments);
}