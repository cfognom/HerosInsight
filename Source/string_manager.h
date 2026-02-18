#pragma once

#include <bitview.h>
#include <rich_text.h>
#include <span_vector.h>

namespace HerosInsight::Text
{
    namespace AssembleMode
    {
        // clang-format off
        struct Renderable{};
        struct Searchable{};
        struct Measure{};
        // clang-format on
    }

    enum struct Plurality
    {
        Null,
        Unknown,
        Singular,
        Plural,
    };

    using EncodedNumber = std::array<char, 7>;

    constexpr EncodedNumber EncodeSearchableNumber(float num)
    {
        EncodedNumber out;

        out[0] = '\x1';

        uint32_t value = std::bit_cast<uint32_t>(num);

        // Manipulate values so that sort correctly during lexical sort
        if (num < 0) // Negative values become positive and all bits are inverted
            value = ~value;
        else if (num >= 0) // Positive values and negative zero become negative
            value |= 0x80000000;

        for (int i = 0; i < 6; ++i)
        {
            size_t bits_idx;
            if constexpr (std::endian::native == std::endian::little)
            {
                bits_idx = 5 - i;
            }
            else
            {
                bits_idx = i;
            }
            out[1 + i] = char(((value >> (6 * bits_idx)) & 0x3F) | 0x80);
        }

        return out;
    }
    float DecodeSearchableNumber(EncodedNumber &enc_num);

    // We add padding manually to get unique object representations
    struct StringTemplateAtom
    {
        enum struct Type : uint8_t
        {
            Null,
            InlineChars1,
            InlineChars2,
            InlineChars3,
            InlineChars4,
            InlineChars5,
            InlineChars6,
            InlineChars7,
            Tag,
            Number,
            LookupString,
            ExplicitString,
            __LEAF_END__,
            LookupSequence,
            ExplicitSequence,
            Substitution,
        };
        enum struct Constraint : uint8_t
        {
            None,
            SingularOnly,
            PluralOnly,
            RenderableOnly,
        };
        struct Header
        {
            Type type : 5;
            Constraint constraint : 3;
        };
        static_assert(sizeof(Header) == 1);

        struct Parent
        {
            Header header;
            uint8_t child_count;
            uint16_t child_offset;
            union
            {
                uint32_t pieceId;
                uint32_t strId;
                uint32_t subsIndex;
            };
            std::span<StringTemplateAtom> GetChildren(std::span<StringTemplateAtom> rest) { return rest.subspan(child_offset, child_count); }
        };
        static_assert(sizeof(Parent) == 8);
        struct Tag
        {
            Header header;
            RichText::TextTagType tag_type;
            uint16_t padding;
            RichText::UntypedTextTag raw_tag;
        };
        static_assert(sizeof(Tag) == 8);
        struct Num
        {
            Header header;
            uint8_t den;
            int16_t num;
            int32_t value; // For non-fractional numbers this holds a bit-casted float (bit cast is a workaround since we need the object to have unique object representation)

            float GetValue()
            {
                if (den == 0)
                {
                    return std::bit_cast<float>(value);
                }
                else
                {
                    return (float)value + (float)num / (float)den;
                }
            }
        };
        static_assert(sizeof(Num) == 8);
        struct Str
        {
            Header header;
            uint8_t padding;
            uint16_t len;
            const char *ptr;
            std::string_view GetStr() const { return std::string_view(ptr, len); }
        };
        static_assert(sizeof(Str) == 8);
        struct InlineChars
        {
            Header header;
            char chars[7];
        };
        static_assert(sizeof(InlineChars) == 8);

        union
        {
            struct
            {
                Header header;
                uint8_t padding[7];
            };
            Tag tag;
            Parent parent;
            Num number;
            Str str;
            InlineChars chars;
        };

        struct Builder
        {
            OutBuf<StringTemplateAtom> dst;

            StringTemplateAtom LookupSequence(uint32_t seqId, std::span<const StringTemplateAtom> substitutions)
            {
                StringTemplateAtom atom;
                auto &obj = atom.parent;
                obj.header = Header{Type::LookupSequence, Constraint::None};
                obj.child_count = substitutions.size();
                obj.child_offset = dst.size();
                obj.strId = seqId;
                dst.AppendRange(substitutions);
                return atom;
            }
            StringTemplateAtom LookupSequence(uint32_t seqId, std::initializer_list<StringTemplateAtom> substitutions)
            {
                return this->LookupSequence(seqId, std::span<const StringTemplateAtom>(substitutions.begin(), substitutions.size()));
            }
            StringTemplateAtom LookupSequence(uint32_t seqId)
            {
                return this->LookupSequence(seqId, std::span<const StringTemplateAtom>{});
            }
            StringTemplateAtom ExplicitSequence(std::span<const StringTemplateAtom> atoms)
            {
                StringTemplateAtom atom;
                auto &obj = atom.parent;
                obj.header = Header{Type::ExplicitSequence, Constraint::None};
                obj.child_count = atoms.size();
                obj.child_offset = dst.size();
                obj.strId = 0;
                dst.AppendRange(atoms);
                return atom;
            }
            StringTemplateAtom ExplicitSequence(std::initializer_list<StringTemplateAtom> atoms)
            {
                return this->ExplicitSequence(std::span<const StringTemplateAtom>(atoms.begin(), atoms.size()));
            }
            static StringTemplateAtom LookupString(Constraint constraint, uint32_t strId)
            {
                StringTemplateAtom atom;
                auto &obj = atom.parent;
                obj.header = Header{Type::LookupString, constraint};
                obj.child_count = 0;
                obj.child_offset = 0;
                obj.pieceId = strId;
                return atom;
            }
            static StringTemplateAtom Tag(RichText::TextTag tag)
            {
                StringTemplateAtom atom;
                auto &obj = atom.tag;
                obj.header = Header{Type::Tag, Constraint::RenderableOnly};
                obj.tag_type = tag.type;
                obj.padding = 0;
                obj.raw_tag = tag.tag;
                return atom;
            }
            static StringTemplateAtom Substitution(uint32_t index)
            {
                StringTemplateAtom atom;
                auto &obj = atom.parent;
                obj.header = Header{Type::Substitution, Constraint::None};
                obj.child_count = 0;
                obj.child_offset = 0;
                obj.subsIndex = index;
                return atom;
            }
            static StringTemplateAtom Number(float number)
            {
                StringTemplateAtom atom;
                auto &obj = atom.number;
                obj.header = Header{Type::Number, Constraint::None};
                obj.den = 0;
                obj.num = 0;
                obj.value = std::bit_cast<decltype(obj.value)>(number);
                return atom;
            }
            static StringTemplateAtom MixedNumber(int32_t wholes, int16_t num, uint8_t den)
            {
                StringTemplateAtom atom;
                auto &obj = atom.number;
                obj.header = Header{Type::Number, Constraint::None};
                obj.den = den;
                obj.num = num;
                obj.value = wholes;
                return atom;
            }
            static StringTemplateAtom MixedNumber(float number) // Auto decomposes
            {
                float value_int;
                float value_fract = std::modf(number, &value_int);

                int16_t num = 0;
                uint8_t den = 0;
                if (value_fract == 0.25f)
                {
                    num = 1;
                    den = 4;
                }
                else if (value_fract == 0.5f)
                {
                    num = 1;
                    den = 2;
                }
                else if (value_fract == 0.75f)
                {
                    num = 3;
                    den = 4;
                }

                if (num)
                {
                    int32_t wholes = (int32_t)value_int;
                    return MixedNumber(wholes, num, den);
                }
                else
                {
                    return Number(number);
                }
            }
            static StringTemplateAtom Fraction(int16_t num, uint8_t den)
            {
                return MixedNumber(0, num, den);
            }
            static StringTemplateAtom ExplicitString(std::string_view str)
            {
                StringTemplateAtom atom;
                auto &obj = atom.str;
                obj.header = Header{Type::ExplicitString, Constraint::None};
                obj.padding = 0;
                obj.len = str.size();
                obj.ptr = str.data();
                return atom;
            }
            static StringTemplateAtom Char(char ch, Constraint constraint = Constraint::None)
            {
                StringTemplateAtom atom;
                auto &obj = atom.chars;
                obj.header = Header{Type::InlineChars1, constraint};
                obj.chars[0] = ch;
                std::memset(obj.chars + 1, 0, sizeof(obj.chars) - 1);
                return atom;
            }
            static StringTemplateAtom Chars(std::string_view chars, Constraint constraint = Constraint::None)
            {
                assert(!chars.empty());
                StringTemplateAtom atom;
                auto &obj = atom.chars;
                assert(chars.size() <= sizeof(obj.chars));
                obj.header = Header{(Type)((size_t)Type::InlineChars1 + chars.size() - 1), constraint};
                std::memcpy(obj.chars, chars.data(), chars.size());
                std::memset(obj.chars + chars.size(), 0, sizeof(obj.chars) - chars.size());
                return atom;
            }
        };
    };
    static_assert(sizeof(StringTemplateAtom) == 8);
    static_assert(std::has_unique_object_representations_v<StringTemplateAtom>);

    struct StringTemplate
    {
        StringTemplateAtom root;
        std::span<StringTemplateAtom> rest;
    };

    struct PosDelta
    {
        uint16_t pos;
        int16_t delta;
    };

    void PatchPositions(std::span<uint16_t> positions, std::span<uint16_t> out_positions, std::span<PosDelta> deltas);

    struct StringManager
    {
        using StrId = uint16_t;
        // struct StringPiece
        // {
        //     enum struct Type : uint8_t
        //     {
        //         LookupString,
        //         Char,
        //         Char2,
        //         Substitution,
        //         Number,
        //         Fraction,
        //         Tag,
        //     };
        //     enum struct When : uint8_t
        //     {
        //         Always,
        //         SingularOnly,
        //         PluralOnly,
        //     };
        //     Type type;
        //     When when;
        //     uint16_t value;
        // };

        SpanVector<char> strings;
        SpanVector<StringTemplateAtom> sequences;

        SpanVector<char>::Deduper strings_deduper = strings.CreateDeduper(0);
        SpanVector<StringTemplateAtom>::Deduper sequences_deduper = sequences.CreateDeduper(0);

        size_t AssimilateString(std::string_view str);

        void AssembleSearchableString(StringTemplate t, OutBuf<char> dst);
        void AssembleRenderableString(StringTemplate t, OutBuf<char> dst, OutBuf<PosDelta> *searchable_to_renderable = nullptr);
    };

    struct StringMapper
    {
        Text::StringManager &mgr;
        std::vector<uint16_t> itemId_to_strId;

        StringMapper(size_t size, Text::StringManager &assembler) : mgr(assembler)
        {
            itemId_to_strId.resize(size, std::numeric_limits<uint16_t>::max());
        }

        size_t AssimilateString(size_t itemId, std::string_view str)
        {
            auto strId = mgr.AssimilateString(str);
            itemId_to_strId[itemId] = strId;
            return strId;
        }

        void SetStrId(size_t itemId, size_t strId)
        {
            itemId_to_strId[itemId] = strId;
        }

        size_t GetStrId(size_t itemId)
        {
            return itemId_to_strId[itemId];
        }
    };
}