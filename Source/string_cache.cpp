#include <string_view>

#include <parsing.h>
#include <rich_text.h>
#include <string_arena.h>

#include "string_cache.h"

namespace HerosInsight::Text
{
    struct Assimilator
    {
        using StringPieceType = StringCache::StringPiece::Type;
        StringCache &cache;
        StringArena<char>::deduper *deduper;
        Plurality plurality = Plurality::Null;

        static Plurality GetEmbeddedPlurality(std::string_view str)
        {
            auto pos = str.find_last_of("0123456789");
            if (pos == std::string_view::npos)
                return Plurality::Null;
            bool is_singular = str[pos] == '1' && (pos == 0 || !Utils::IsDigit(str[pos - 1]));
            return is_singular ? Plurality::Singular : Plurality::Plural;
        }

        Plurality GetPlurality()
        {
            // Use embedded plurality if it exists, otherwise fall back to the replacement plurality
            auto last = (std::string_view)cache.pieces.back();
            auto embedded_plurality = GetEmbeddedPlurality(last);
            if (embedded_plurality != Plurality::Null)
                return embedded_plurality;
            return plurality;
        }

        template <typename Handler, typename... Rest>
        void FixGeneric(std::string_view rem)
        {
            std::string_view current = rem;
            auto end = rem.data() + rem.size();
            auto handler = Handler();
            while (true)
            {
                auto found = handler.Find(current);
                bool success = !found.empty();
                if (success)
                    current = std::string_view(current.data(), found.data() - current.data());

                if constexpr (sizeof...(Rest) > 0)
                {
                    FixGeneric<Rest...>(current);
                }
                else
                {
                    cache.pieces.Elements().append_range(current);
                    auto id = cache.pieces.CommitWritten(deduper);
                    cache.strings.Elements().emplace_back(StringPieceType::StringAlways, id);
                }

                if (!success)
                    break;

                handler.Handle(*this);

                auto next = found.data() + found.size();
                current = std::string_view(next, end - next);
            }
        }

        struct Subsitution
        {
            uint16_t id;
            std::string_view Find(std::string_view rem)
            {
                using Pattern = std::tuple<Lit<"%str">, Int<10, &Subsitution::id>, Lit<"%">>;
                return Parsing::find_pattern<Pattern>(rem, this);
            }
            void Handle(Assimilator &a)
            {
                a.cache.strings.Elements().emplace_back(StringPieceType::Substitution, id - 1);
                a.plurality = Plurality::Unknown;
            }
        };

        struct ReplacePlural
        {
            std::string_view plural_str;
            std::string_view singular_str;
            std::string_view Find(std::string_view rem)
            {
                using Pattern = std::tuple<Lit<"[pl:\"">, CaptureUntil<"\"]", &ReplacePlural::plural_str>>;
                auto found = Parsing::find_pattern<Pattern>(rem, this);
                if (found.empty() || found.data() == rem.data())
                    return {};
                auto sing_end = found.data() - rem.data();
                auto sing_begin = rem.find_last_of(' ', sing_end - 1);
                assert(sing_begin != std::string_view::npos);
                sing_begin++;
                auto sing_len = sing_end - sing_begin;
                singular_str = rem.substr(sing_begin, sing_len);
                return rem.substr(sing_begin, sing_len + found.size());
            }
            void Handle(Assimilator &a)
            {
                switch (a.plurality)
                {
                    case Plurality::Plural:
                        a.cache.pieces.Elements().append_range(plural_str);
                        break;
                    case Plurality::Singular:
                        a.cache.pieces.Elements().append_range(singular_str);
                        break;
                    case Plurality::Unknown:
                    {
                        auto singular_str_id = a.cache.pieces.push_back(singular_str, a.deduper);
                        a.cache.strings.Elements().emplace_back(StringPieceType::StringSingularOnly, singular_str_id);

                        auto plural_str_id = a.cache.pieces.push_back(plural_str, a.deduper);
                        a.cache.strings.Elements().emplace_back(StringPieceType::StringPluralOnly, plural_str_id);
                        break;
                    }
                    case Plurality::Null:
                    default:
                        assert(false);
                }
            }
        };

        struct OptionalS
        {
            std::string_view Find(std::string_view rem)
            {
                using Pattern = std::tuple<Lit<"[s]">>;
                return Parsing::find_pattern<Pattern>(rem, this);
            }
            void Handle(Assimilator &a)
            {
                switch (a.plurality)
                {
                    case Plurality::Plural:
                        a.cache.pieces.Elements().push_back('s');
                        break;
                    case Plurality::Singular:
                        break;
                    case Plurality::Unknown:
                        a.cache.strings.Elements().emplace_back(StringPieceType::CharPluralOnly, (uint16_t)'s');
                        break;
                    case Plurality::Null:
                    default:
                        assert(false);
                }
            }
        };

        struct Tags
        {
            RichText::TextTag tag;
            std::string_view tag_str;
            std::string_view Find(std::string_view rem)
            {
                tag_str = RichText::TextTag::Find(rem, tag);
                return tag_str;
            }
            void Handle(Assimilator &a)
            {
                if (tag.type == RichText::TextTag::Type::Frac)
                {
                    auto num = tag.frac_tag.num;
                    auto den = tag.frac_tag.den;
                    assert(num <= 255);
                    assert(den <= 255);
                    a.cache.strings.Elements().emplace_back(StringPieceType::Fraction, (num << 8) | den);
                }
                else
                {
                    auto tag_str_id = a.cache.pieces.push_back(tag_str, a.deduper);
                    a.cache.strings.Elements().emplace_back(StringPieceType::Tag, tag_str_id);
                }
            }
        };

        struct Numbers
        {
            int num;
            std::string_view Find(std::string_view rem)
            {
                auto pos = rem.find_first_of("0123456789");
                if (pos == std::string_view::npos)
                    return {};
                auto begin = rem.data() + pos;
                auto [ptr, ec] = std::from_chars(begin, rem.data() + rem.size(), num, 10);
                assert(ec == std::errc());
                return std::string_view(begin, ptr - begin);
            }
            void Handle(Assimilator &a)
            {
                a.cache.strings.Elements().emplace_back(StringPieceType::Number, num);
                a.plurality = num == 1 ? Plurality::Singular : Plurality::Plural;
            }
        };

        struct PercentFix
        {
            std::string_view Find(std::string_view rem)
            {
                auto pos = rem.find("%%");
                if (pos == std::string_view::npos)
                    return {};
                return rem.substr(pos, 1);
            }

            void Handle(Assimilator &a)
            {
            }
        };
    };

    size_t StringCache::AssimilateString(std::string_view str, StringArena<char>::deduper *pieces_deduper, StringArena<StringPiece>::deduper *strings_deduper)
    {
        Assimilator a{
            .cache = *this,
            .deduper = pieces_deduper,
        };
        // clang-format off
        a.FixGeneric<
            Assimilator::Subsitution,
            Assimilator::ReplacePlural,
            Assimilator::OptionalS,
            Assimilator::Tags,
            Assimilator::Numbers,
            Assimilator::PercentFix
        >(str);
        // clang-format on
        auto id = this->strings.CommitWritten(strings_deduper);
        return id;
    }
}