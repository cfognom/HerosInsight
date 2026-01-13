#pragma once

#include <bitview.h>
#include <string_arena.h>

namespace HerosInsight::Text
{
    namespace BuildMode
    {
        // clang-format off
        struct Readable{};
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

    struct StringCache
    {
        struct StringPiece
        {
            enum struct Type : uint16_t
            {
                StringAlways,
                StringSingularOnly,
                StringPluralOnly,
                CharPluralOnly,
                Substitution,
                Number,
                Fraction,
                Tag,
            };
            Type type;
            uint16_t value;
        };

        StringArena<char> pieces;
        StringArena<StringPiece> strings;
        size_t AssimilateString(std::string_view str, StringArena<char>::deduper *pieces_deduper, StringArena<StringPiece>::deduper *strings_deduper);

        template <typename SubsProvider>
        void BuildSearchableString(size_t id, OutBuf<char> dst, SubsProvider &&subs_provider);
        template <typename SubsProvider>
        void BuildReadableString(size_t id, OutBuf<char> dst, SubsProvider &&subs_provider);
    };

    namespace
    {
        template <typename SubsProvider, typename Mode>
        void BuildString(StringCache &cache, size_t id, OutBuf<char> dst, SubsProvider &&subs_provider)
        {
            using StringPiece = StringCache::StringPiece;

            auto string = cache.strings.Get(id);
            const auto &pieces = cache.pieces;

            std::array<std::string_view, 3> replacements;
            BitArray<3> is_plural;

            Plurality plurality = Plurality::Null;

            for (auto &piece : string)
            {
                std::string_view str_piece;
#ifdef _DEBUG
                switch (piece.type)
                {
                    case StringPiece::Type::StringSingularOnly:
                    case StringPiece::Type::StringPluralOnly:
                    case StringPiece::Type::CharPluralOnly:
                        assert(plurality != Plurality::Null);
                }
#endif

                switch (piece.type)
                {
                    case StringPiece::Type::Tag:
                    case StringPiece::Type::StringAlways:
                        str_piece = pieces.Get(piece.value);
                        break;

                    case StringPiece::Type::StringSingularOnly:
                        if (plurality != Plurality::Singular)
                            continue;
                        str_piece = pieces.Get(piece.value);
                        break;

                    case StringPiece::Type::StringPluralOnly:
                        if (plurality != Plurality::Plural)
                            continue;
                        str_piece = pieces.Get(piece.value);
                        break;

                    case StringPiece::Type::CharPluralOnly:
                        if (plurality != Plurality::Plural)
                            continue;
                        dst.push_back((char)piece.value);
                        continue;

                    case StringPiece::Type::Substitution:
                    {
                        auto param_id = piece.value;
                        str_piece = replacements[param_id];
                        if (str_piece.empty())
                        {
                            auto start_idx = dst.size();
                            // bool plural = WriteSkillParam(skill_id, param_id, attr_lvl, dst);
                            bool plural = subs_provider(param_id, dst);
                            is_plural[param_id] = plural;
                            replacements[param_id] = std::string_view(&dst[start_idx], dst.size() - start_idx);
                            plurality = plural ? Plurality::Plural : Plurality::Singular;
                            continue;
                        }
                        plurality = is_plural[param_id] ? Plurality::Plural : Plurality::Singular;
                        break;
                    }

                    case StringPiece::Type::Fraction:
                    {
                        auto num = piece.value >> 8;
                        auto den = piece.value & 0xFF;
                        if constexpr (std::is_same_v<Mode, BuildMode::Readable>)
                        {
                            switch (piece.value)
                            {
                                    // clang-format off
                            case ((1 << 8) | 2): dst.push_back('½'); break;
                            case ((1 << 8) | 4): dst.push_back('¼'); break;
                            case ((3 << 8) | 4): dst.push_back('¾'); break;
                                    // clang-format on
                                default:
                                {
                                    dst.AppendIntToChars(num);
                                    dst.push_back('/');
                                    dst.AppendIntToChars(den);
                                }
                            }
                        }
                        else if constexpr (std::is_same_v<Mode, BuildMode::Searchable>)
                        {
                            auto value = (float)num / (float)den;
                            EncodeSearchableNumber(dst, value);
                        }
                        continue;
                    }

                    case StringPiece::Type::Number:
                    {
                        if constexpr (std::is_same_v<Mode, BuildMode::Readable>)
                        {
                            dst.AppendIntToChars(piece.value);
                        }
                        else if constexpr (std::is_same_v<Mode, BuildMode::Searchable>)
                        {
                            EncodeSearchableNumber(dst, (float)piece.value);
                        }
                        continue;
                    }

#ifdef _DEBUG
                    default:
                        assert(false);
#endif
                }
                dst.AppendRange(str_piece);
            }
        }
    }

    template <typename SubsProvider>
    inline void StringCache::BuildSearchableString(size_t id, OutBuf<char> dst, SubsProvider &&subs_provider)
    {
        BuildString<SubsProvider, BuildMode::Searchable>(*this, id, dst, std::forward<SubsProvider>(subs_provider));
    }
    template <typename SubsProvider>
    inline void StringCache::BuildReadableString(size_t id, OutBuf<char> dst, SubsProvider &&subs_provider)
    {
        BuildString<SubsProvider, BuildMode::Readable>(*this, id, dst, std::forward<SubsProvider>(subs_provider));
    }
}