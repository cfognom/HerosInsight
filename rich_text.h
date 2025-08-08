#pragma once

#include <imgui.h>

namespace HerosInsight::RichText
{
    class TextImageProvider
    {
    public:
        virtual ~TextImageProvider() = default;

        virtual void Draw(ImVec2 pos, size_t id) = 0;
        virtual float GetWidth(size_t id) = 0;
    };

    class DefaultTextImageProvider : public TextImageProvider
    {
    public:
        enum Id : size_t
        {
            MonsterSkull,
            EnergyOrb,

            COUNT,
        };

        static DefaultTextImageProvider &Instance();
        void Draw(ImVec2 pos, size_t id) override;
        float GetWidth(size_t id) override;
    };

    class TextTooltipProvider
    {
    public:
        virtual ~TextTooltipProvider() = default;
        virtual void DrawTooltip(size_t id) = 0;
    };

    struct ColorTag
    {
        ImU32 color = 0;
    };

    struct TooltipTag
    {
        int32_t id = -1; // -1 = close tooltip
    };

    struct ImageTag
    {
        uint32_t id;
    };

    struct FracTag
    {
        uint16_t num = 0.0;
        uint16_t denom = 1.0;
    };

    using TextTag = std::variant<ColorTag, TooltipTag, ImageTag, FracTag>;
    bool TryReadTextTag(std::string_view &remaining, TextTag &out);
    std::string_view FindTextTag(std::string_view text, TextTag &out);

    struct TextSegment
    {
        std::variant<std::string_view, size_t> text_or_image_id;
        std::optional<ImU32> color = std::nullopt;
        std::optional<uint16_t> tooltip_id = std::nullopt;
        bool is_highlighted = false;
        bool can_wrap_after = false;

        float GetWidth(TextImageProvider *image_provider) const;
    };

    bool TryReadTextTag(std::string_view &remaining, TextTag &out);
    std::string_view FindTextTag(std::string_view text, TextTag &out);

    void MakeTextSegments(std::string_view text, std::span<uint16_t> highlighting, std::span<TextSegment> &result);
    void DrawTextSegments(
        std::span<TextSegment> segments,
        float wrapping_min, float wrapping_max,
        TextTooltipProvider *tooltip_provider,
        TextImageProvider *image_provider);
    void DrawRichText(
        std::string_view text,
        float wrapping_min, float wrapping_max,
        std::span<uint16_t> highlighting,
        TextTooltipProvider *tooltip_provider,
        TextImageProvider *image_provider);
}