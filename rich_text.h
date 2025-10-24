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
        enum struct Type : uint8_t
        {
            Color,
            Tooltip,
            Image,
            Frac,
        };

        TextTag() = default;
        TextTag(ColorTag color_tag) : type(Type::Color), color_tag(color_tag) {};
        TextTag(TooltipTag tooltip_tag) : type(Type::Tooltip), tooltip_tag(tooltip_tag) {};
        TextTag(ImageTag image_tag) : type(Type::Image), image_tag(image_tag) {};
        TextTag(FracTag frac_tag) : type(Type::Frac), frac_tag(frac_tag) {};

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

        void ToChars(std::span<char> &out) const;
        static bool TryRead(std::string_view &remaining, TextTag &out);
        static std::string_view Find(std::string_view text, TextTag &out);
    };

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

    void ExtractTags(std::string_view text_with_tags, std::span<char> &only_text, std::span<TextTag> &only_tags);

    struct RichTextArena : public IndexedStringArena<char>
    {
        void PushText(std::string_view text)
        {
            this->append_range(text);
        }

        void PushTag(TextTag tag)
        {
            this->AppendWriteBuffer(
                64,
                [&](std::span<char> &out)
                {
                    tag.ToChars(out);
                }
            );
        }

        void PushColorTag(ImU32 color)
        {
            this->PushTag(TextTag(ColorTag{color}));
        }

        void PushImageTag(uint32_t image_id)
        {
            PushTag(TextTag(ImageTag{image_id}));
        }

        void PushColoredText(ImU32 color, std::string_view text)
        {
            this->PushTag(TextTag(ColorTag{color}));
            this->PushText(text);
            this->PushTag(TextTag(ColorTag{NULL}));
        }
    };

    struct Drawer
    {
        TextTooltipProvider *tooltip_provider = nullptr;
        TextImageProvider *image_provider = nullptr;
        TextFracProvider *frac_provider = nullptr;

        void MakeTextSegments(std::string_view text, std::span<TextSegment> &result, std::span<uint16_t> highlighting = {}, TextSegment::WrapMode first_segment_wrap_mode = TextSegment::WrapMode::Disallow);
        void DrawTextSegments(std::span<TextSegment> segments, float wrapping_min = 0, float wrapping_max = -1);
        void DrawRichText(std::string_view text, float wrapping_min = 0, float wrapping_max = -1, std::span<uint16_t> highlighting = {}, TextSegment::WrapMode first_segment_wrap_mode = TextSegment::WrapMode::Disallow);
    };

    float CalcTextSegmentsWidth(std::span<TextSegment> segments);
}