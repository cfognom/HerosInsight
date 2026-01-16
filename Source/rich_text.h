#pragma once

#include <variant>

#include <imgui.h>
#include <string_arena.h>

namespace HerosInsight::RichText
{
    struct ColorTag
    {
        ImU32 color; // 0 = pop color

        static bool TryRead(std::string_view &remaining, ColorTag &out);
    };

    struct TooltipTag
    {
        int32_t id; // -1 = close tooltip

        static bool TryRead(std::string_view &remaining, TooltipTag &out);
        static std::string_view Find(std::string_view text, TooltipTag &out);
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
        static std::string_view Find(std::string_view text, FracTag &out);
    };

    struct TextTag
    {
        enum struct Type : uint8_t
        {
            Color,
            Tooltip,
            Frac,
        };

        TextTag() = default;
        TextTag(ColorTag color_tag) : type(Type::Color), color_tag(color_tag) {};
        TextTag(TooltipTag tooltip_tag) : type(Type::Tooltip), tooltip_tag(tooltip_tag) {};
        TextTag(FracTag frac_tag) : type(Type::Frac), frac_tag(frac_tag) {};

        Type type;
        uint8_t _padding{};
        uint16_t offset;

        union
        {
            ColorTag color_tag;
            TooltipTag tooltip_tag;
            FracTag frac_tag;
        };

        bool IsZeroWidth() const
        {
            return type == Type::Color ||
                   type == Type::Tooltip;
        }

        void ToChars(OutBuf<char> output) const;
        static bool TryRead(std::string_view &remaining, TextTag &out);
        static std::string_view Find(std::string_view text, TextTag &out);
    };

    namespace Icons
    {
        inline static std::string_view Upkeep = (const char *)u8"\uE000";
        inline static std::string_view EnergyOrb = (const char *)u8"\uE001";
        inline static std::string_view Activation = (const char *)u8"\uE002";
        inline static std::string_view Recharge = (const char *)u8"\uE003";
        inline static std::string_view Adrenaline = (const char *)u8"\uE004";
        inline static std::string_view MonsterSkull = (const char *)u8"\uE005";
        inline static std::string_view HealthUpkeep = (const char *)u8"\uE006";
        inline static std::string_view Sacrifice = (const char *)u8"\uE007";
        inline static std::string_view StarOrb = (const char *)u8"\uE009";
        inline static std::string_view Overcast = (const char *)u8"\uE00a";
        inline static std::string_view Aftercast = (const char *)u8"\uE00b";
    }

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
        enum struct WrapMode : uint8_t
        {
            Null,
            Disallow,
            Allow,
            Force,
        };

        std::string_view text;
        std::variant<std::monostate, FracTag> tag;
        float width;
        std::optional<ImU32> color = std::nullopt;
        std::optional<uint16_t> tooltip_id = std::nullopt;
        bool is_highlighted = false;
        bool has_hidden_hl = false;
        WrapMode wrap_mode = WrapMode::Null;

        float CalcWidth(TextFracProvider *frac_provider) const;
    };

    void ExtractTags(std::string_view text_with_tags, std::span<char> &only_text, std::span<TextTag> &only_tags);

    namespace Helpers
    {
        inline void PushText(StringArena<char> &dst, std::string_view text)
        {
            dst.Elements().append_range(text);
        }

        inline void PushTag(StringArena<char> &dst, TextTag tag)
        {
            dst.AppendWriteBuffer(
                64,
                [&](std::span<char> &out)
                {
                    SpanWriter<char> out_writer(out);
                    OutBuf<char> out_buf(out_writer);
                    tag.ToChars(out_buf);
                    out = out_writer.WrittenSpan();
                }
            );
        }

        inline void PushColorTag(StringArena<char> &dst, ImU32 color)
        {
            PushTag(dst, TextTag(ColorTag{color}));
        }

        inline void PushColoredText(StringArena<char> &dst, ImU32 color, std::string_view text)
        {
            PushTag(dst, TextTag(ColorTag{color}));
            PushText(dst, text);
            PushTag(dst, TextTag(ColorTag{NULL}));
        }
    };

    struct Drawer
    {
        TextTooltipProvider *tooltip_provider = nullptr;
        TextFracProvider *frac_provider = nullptr;

        void MakeTextSegments(std::string_view text, OutBuf<TextSegment> result, std::span<uint16_t> highlighting = {}, TextSegment::WrapMode first_segment_wrap_mode = TextSegment::WrapMode::Disallow);
        void DrawTextSegments(std::span<TextSegment> segments, float wrapping_min = 0, float wrapping_max = -1);
        void DrawRichText(std::string_view text, float wrapping_min = 0, float wrapping_max = -1, std::span<uint16_t> highlighting = {}, TextSegment::WrapMode first_segment_wrap_mode = TextSegment::WrapMode::Disallow);
    };

    float CalcTextSegmentsWidth(std::span<TextSegment> segments);
}