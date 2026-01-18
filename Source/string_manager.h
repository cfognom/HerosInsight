#pragma once

#include <bitview.h>
#include <string_arena.h>

namespace HerosInsight::Text
{
    namespace AssembleMode
    {
        // clang-format off
        struct Renderable{};
        struct Searchable{};
        // clang-format on
    }

    enum struct Plurality
    {
        Null,
        Unknown,
        Singular,
        Plural,
    };

    static void EncodeSearchableNumber(OutBuf<char> out, float num)
    {
        out.push_back(1);
        uint32_t value = std::bit_cast<uint32_t>(num);
        if (num < 0)
        {
            value = ~value;
        }
        else if (num > 0)
        {
            value &= 0x7FFFFFFF;
        }
        else
        {
            value = 0;
        }
        for (int i = 0; i < 6; ++i)
        {
            out.push_back(((value >> (6 * i)) & 0x3F) | 0x80);
        }
    }

    static float DecodeSearchableNumber(char (*src)[7])
    {
        auto &cs = *src;
        assert(cs[0] == '\x1');
        uint32_t value = 0;
        for (int i = 0; i < 6; i++)
        {
            value |= (cs[i + 1] & 0x3F) << (6 * i);
        }
        return std::bit_cast<float>(value);
    }

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
            LookupSequence,
            ExplicitSequence,
            Substitution,
            Color,
            Number,
            Fraction,
            LookupString,
            ExplicitString,
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
            Type type : 4;
            Constraint constraint : 4;
        };
        static_assert(sizeof(Header) == 1);

        static StringTemplateAtom MakeLookupSequence(uint32_t seqId, OutBuf<StringTemplateAtom> dst, std::span<const StringTemplateAtom> substitutions)
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
        static StringTemplateAtom MakeLookupSequence(uint32_t seqId, OutBuf<StringTemplateAtom> dst, std::initializer_list<StringTemplateAtom> substitutions)
        {
            return MakeLookupSequence(seqId, dst, std::span<const StringTemplateAtom>(substitutions.begin(), substitutions.size()));
        }
        static StringTemplateAtom MakeExplicitSequence(OutBuf<StringTemplateAtom> dst, std::span<const StringTemplateAtom> atoms)
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
        static StringTemplateAtom MakeExplicitSequence(OutBuf<StringTemplateAtom> dst, std::initializer_list<StringTemplateAtom> atoms)
        {
            return MakeExplicitSequence(dst, std::span<const StringTemplateAtom>(atoms.begin(), atoms.size()));
        }
        static StringTemplateAtom MakeLookupString(Constraint constraint, uint32_t strId)
        {
            StringTemplateAtom atom;
            auto &obj = atom.parent;
            obj.header = Header{Type::LookupString, constraint};
            obj.child_count = 0;
            obj.child_offset = 0;
            obj.pieceId = strId;
            return atom;
        }
        static StringTemplateAtom MakeColor(uint32_t color)
        {
            StringTemplateAtom atom;
            auto &obj = atom.parent;
            obj.header = Header{Type::Color, Constraint::RenderableOnly};
            obj.child_count = 0;
            obj.child_offset = 0;
            obj.color = color;
            return atom;
        }
        static StringTemplateAtom MakeSubstitution(uint32_t index)
        {
            StringTemplateAtom atom;
            auto &obj = atom.parent;
            obj.header = Header{Type::Substitution, Constraint::None};
            obj.child_count = 0;
            obj.child_offset = 0;
            obj.subsIndex = index;
            return atom;
        }
        static StringTemplateAtom MakeNumber(float number)
        {
            StringTemplateAtom atom;
            auto &obj = atom.num;
            obj.header = Header{Type::Number, Constraint::None};
            obj.sign = false;
            obj.den = 1;
            obj.num = number; // TODO: bitcast to float since its not allowed to store real floats and have unique object representations
            return atom;
        }
        static StringTemplateAtom MakeFraction(float num, uint16_t den)
        {
            StringTemplateAtom atom;
            auto &obj = atom.num;
            obj.header = Header{Type::Fraction, Constraint::None};
            obj.sign = false;
            obj.den = den;
            obj.num = num;
            return atom;
        }
        static StringTemplateAtom MakeExplicitString(std::string_view str)
        {
            StringTemplateAtom atom;
            auto &obj = atom.str;
            obj.header = Header{Type::ExplicitString, Constraint::None};
            obj.padding = 0;
            obj.len = str.size();
            obj.ptr = str.data();
            return atom;
        }
        static StringTemplateAtom MakeChar(char ch, Constraint constraint = Constraint::None)
        {
            StringTemplateAtom atom;
            auto &obj = atom.chars;
            obj.header = Header{Type::InlineChars1, constraint};
            obj.chars[0] = ch;
            std::memset(obj.chars + 1, 0, sizeof(obj.chars) - 1);
            return atom;
        }
        static StringTemplateAtom MakeChars(std::string_view chars, Constraint constraint = Constraint::None)
        {
            assert(!chars.empty());
            StringTemplateAtom atom;
            auto &obj = atom.chars;
            obj.header = Header{(Type)((size_t)Type::InlineChars1 + chars.size() - 1), constraint};
            std::memcpy(obj.chars, chars.data(), chars.size());
            std::memset(obj.chars + chars.size(), 0, sizeof(obj.chars) - chars.size());
            return atom;
        }

        struct Parent
        {
            Header header;
            uint8_t child_count;
            uint16_t child_offset;
            union
            {
                uint32_t pieceId;
                uint32_t strId;
                uint32_t color;
                uint32_t subsIndex;
            };
            std::span<StringTemplateAtom> GetChildren(std::span<StringTemplateAtom> rest) { return rest.subspan(child_offset, child_count); }
        };
        static_assert(sizeof(Parent) == 8);
        struct Num
        {
            Header header;
            uint8_t sign;
            uint16_t den;
            uint32_t num;
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
            Parent parent;
            Num num;
            Str str;
            InlineChars chars;
        };
    };
    static_assert(sizeof(StringTemplateAtom) == 8);
    static_assert(std::has_unique_object_representations_v<StringTemplateAtom>);

    struct StringTemplate
    {
        StringTemplateAtom root;
        std::span<StringTemplateAtom> rest;
    };

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

        StringArena<char> strings;
        StringArena<StringTemplateAtom> sequences;

        StringArena<char>::deduper strings_deduper = strings.CreateDeduper(0);
        StringArena<StringTemplateAtom>::deduper sequences_deduper = sequences.CreateDeduper(0);

        size_t AssimilateString(std::string_view str);

        void AssembleSearchableString(StringTemplate t, OutBuf<char> dst);
        void AssembleRenderableString(StringTemplate t, OutBuf<char> dst);
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