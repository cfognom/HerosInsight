#include <string_view>

#include <parsing.h>
#include <rich_text.h>
#include <string_arena.h>

#include "string_manager.h"

namespace HerosInsight::Text
{
    struct Assimilator
    {
        StringManager &mgr;
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
            auto last = (std::string_view)mgr.strings.back();
            auto embedded_plurality = GetEmbeddedPlurality(last);
            if (embedded_plurality != Plurality::Null)
                return embedded_plurality;
            return plurality;
        }

        constexpr static size_t MaxInlineChars = sizeof(StringTemplateAtom::InlineChars::chars);

        void CommitStr(StringTemplateAtom::Constraint constraint, std::string_view str)
        {
            assert(!str.empty());
            if (str.size() <= MaxInlineChars)
            {
                mgr.sequences.Elements().emplace_back(StringTemplateAtom::Builder::Chars(str, constraint));
            }
            else
            {
                assert(mgr.strings.GetPendingSize() == 0);
                auto id = mgr.strings.push_back(str, &mgr.strings_deduper);
                mgr.sequences.Elements().emplace_back(StringTemplateAtom::Builder::LookupString(constraint, id));
            }
        }

        void CommitPending(StringTemplateAtom::Constraint constraint)
        {
            auto pending = (std::string_view)mgr.strings.GetPendingSpan();
            assert(!pending.empty());
            if (pending.size() <= MaxInlineChars)
            {
                mgr.sequences.Elements().emplace_back(StringTemplateAtom::Builder::Chars(pending, constraint));
                mgr.strings.DiscardWritten();
            }
            else
            {
                auto id = mgr.strings.CommitWritten(&mgr.strings_deduper);
                mgr.sequences.Elements().emplace_back(StringTemplateAtom::Builder::LookupString(constraint, id));
            }
        }

        template <typename Handler, typename... Rest>
        constexpr void FixCommon(std::string_view rem)
        {
            assert(!rem.empty());
            std::string_view current = rem;
            auto end = rem.data() + rem.size();
            auto handler = Handler();
            while (true)
            {
                auto found = handler.Locate(current);
                bool success = !found.empty();
                if (success)
                {
                    auto before_count = found.data() - current.data();
                    current = std::string_view(current.data(), before_count);
                }

                if (!current.empty())
                {
                    if constexpr (sizeof...(Rest) > 0)
                    {
                        FixCommon<Rest...>(current);
                    }
                    else
                    {
                        mgr.strings.Elements().append_range(current);
                        CommitPending(StringTemplateAtom::Constraint::None);
                    }
                }

                if (!success)
                    break;

                handler.Handle(*this);

                auto next = found.data() + found.size();
                current = std::string_view(next, end - next);
            }
        }

        struct Substitution
        {
            uint32_t id;
            std::string_view Locate(std::string_view rem)
            {
                using Pattern = std::tuple<Lit<"%str">, Int<10, &Substitution::id>, Lit<"%">>;
                return Parsing::find_pattern<Pattern>(rem, this);
            }
            void Handle(Assimilator &a)
            {
                a.mgr.sequences.Elements().emplace_back(StringTemplateAtom::Builder::Substitution(id - 1));
                a.plurality = Plurality::Unknown;
            }
        };

        struct ReplacePlural
        {
            std::string_view plural_str;
            std::string_view singular_str;
            std::string_view Locate(std::string_view rem)
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
                        a.mgr.strings.Elements().append_range(plural_str);
                        break;
                    case Plurality::Singular:
                        a.mgr.strings.Elements().append_range(singular_str);
                        break;
                    case Plurality::Unknown:
                    {
                        a.CommitStr(StringTemplateAtom::Constraint::SingularOnly, singular_str);
                        a.CommitStr(StringTemplateAtom::Constraint::PluralOnly, plural_str);
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
            std::string_view Locate(std::string_view rem)
            {
                using Pattern = std::tuple<Lit<"[s]">>;
                return Parsing::find_pattern<Pattern>(rem, this);
            }
            void Handle(Assimilator &a)
            {
                switch (a.plurality)
                {
                    case Plurality::Plural:
                        a.mgr.strings.Elements().push_back('s');
                        break;
                    case Plurality::Singular:
                        break;
                    case Plurality::Unknown:
                        a.mgr.sequences.Elements().emplace_back(StringTemplateAtom::Builder::Char('s', StringTemplateAtom::Constraint::PluralOnly));
                        break;
                    case Plurality::Null:
                    default:
                        assert(false);
                }
            }
        };

        struct TagFixer
        {
            RichText::TextTag tag;
            std::string_view tag_str;
            std::string_view Locate(std::string_view rem)
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
                    a.mgr.sequences.Elements().emplace_back(StringTemplateAtom::Builder::Fraction(num, den));
                }
                else
                {
                    a.CommitStr(StringTemplateAtom::Constraint::RenderableOnly, tag_str);
                }
            }
        };

        struct NumberFixer
        {
            int num;
            std::string_view Locate(std::string_view rem)
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
                a.mgr.sequences.Elements().emplace_back(StringTemplateAtom::Builder::Number(num));
                a.plurality = num == 1 ? Plurality::Singular : Plurality::Plural;
            }
        };

        struct PercentFixer
        {
            std::string_view Locate(std::string_view rem)
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

    size_t StringManager::AssimilateString(std::string_view str)
    {
        Assimilator a{
            .mgr = *this,
        };
        // clang-format off
        a.FixCommon<
            Assimilator::Substitution,
            Assimilator::ReplacePlural,
            Assimilator::OptionalS,
            Assimilator::TagFixer,
            Assimilator::NumberFixer,
            Assimilator::PercentFixer
        >(str);
        // clang-format on
        auto id = this->sequences.CommitWritten(&this->sequences_deduper);
        return id;
    }

    template <typename Mode>
    struct StringTemplateAssembler
    {
        constexpr static bool is_renderable = std::is_same_v<Mode, AssembleMode::Renderable>;
        constexpr static bool is_searchable = std::is_same_v<Mode, AssembleMode::Searchable>;

        StringManager &mgr;
        std::span<StringTemplateAtom> nodes;
        OutBuf<char> dst;
        Plurality plurality = Plurality::Null;

        void Assemble(std::span<StringTemplateAtom> atoms, std::span<StringTemplateAtom> subs)
        {
            std::array<std::string_view, 3> cached_subs;
            std::array<Plurality, 3> subs_pluralities;

            for (auto &atom : atoms)
            {
                std::string_view str_to_append;
#ifdef _DEBUG
                if (plurality == Plurality::Null)
                {
                    assert(atom.header.constraint != StringTemplateAtom::Constraint::SingularOnly);
                    assert(atom.header.constraint != StringTemplateAtom::Constraint::PluralOnly);
                }
#endif
                if ((atom.header.constraint == StringTemplateAtom::Constraint::SingularOnly && plurality != Plurality::Singular) ||
                    (atom.header.constraint == StringTemplateAtom::Constraint::PluralOnly && plurality != Plurality::Plural))
                    return;

                if constexpr (!is_renderable)
                {
                    if (atom.header.constraint == StringTemplateAtom::Constraint::RenderableOnly)
                        continue;
                }

                assert(dst.AvailableCapacity() >= 7);

                switch (atom.header.type)
                {
                    case StringTemplateAtom::Type::InlineChars7:
                        *(dst.data() + dst.size() + 6) = (atom.chars.chars[6]);
                    case StringTemplateAtom::Type::InlineChars6:
                        *(dst.data() + dst.size() + 5) = (atom.chars.chars[5]);
                    case StringTemplateAtom::Type::InlineChars5:
                        *(dst.data() + dst.size() + 4) = (atom.chars.chars[4]);
                    case StringTemplateAtom::Type::InlineChars4:
                        *(dst.data() + dst.size() + 3) = (atom.chars.chars[3]);
                    case StringTemplateAtom::Type::InlineChars3:
                        *(dst.data() + dst.size() + 2) = (atom.chars.chars[2]);
                    case StringTemplateAtom::Type::InlineChars2:
                        *(dst.data() + dst.size() + 1) = (atom.chars.chars[1]);
                    case StringTemplateAtom::Type::InlineChars1:
                        *(dst.data() + dst.size() + 0) = (atom.chars.chars[0]);
                        dst.Len() += ((size_t)atom.header.type - (size_t)StringTemplateAtom::Type::InlineChars1 + 1);
                        break;

                    case StringTemplateAtom::Type::Substitution:
                    {
                        auto subs_index = atom.parent.subsIndex;
                        str_to_append = cached_subs[subs_index];
                        if (str_to_append.empty())
                        {
                            auto start_idx = dst.size();
#ifdef _DEBUG
                            assert(subs_index < subs.size());
#else
                            if (subs_index >= subs.size())
                            {
                                if constexpr (is_renderable)
                                    dst.AppendString("<c=#FFFF0000>");
                                dst.AppendString("[Missing Text]");
                                if constexpr (is_renderable)
                                    dst.AppendString("</c>");
                                plurality = Plurality::Singular;
                                continue;
                            }
#endif
                            Assemble(subs.subspan(subs_index, 1), {});
                            subs_pluralities[subs_index] = plurality;
                            auto written_sub = std::string_view(dst.data() + start_idx, dst.size() - start_idx);
                            assert(!written_sub.empty());
                            cached_subs[subs_index] = written_sub;
                            continue;
                        }
                        dst.AppendRange(str_to_append);
                        plurality = subs_pluralities[subs_index];
                        break;
                    }

                    case StringTemplateAtom::Type::LookupSequence:
                    {
                        auto atoms = this->mgr.sequences.Get(atom.parent.strId);
                        auto subs = atom.parent.GetChildren(nodes);
                        Assemble(atoms, subs);
                        break;
                    }

                    case StringTemplateAtom::Type::ExplicitSequence:
                    {
                        auto atoms = atom.parent.GetChildren(nodes);
                        Assemble(atoms, {});
                        break;
                    }

                    case StringTemplateAtom::Type::ExplicitString:
                        str_to_append = atom.str.GetStr();
                        dst.AppendRange(str_to_append);
                        break;

                    case StringTemplateAtom::Type::LookupString:
                        str_to_append = mgr.strings.CGet(atom.parent.pieceId);
                        dst.AppendRange(str_to_append);
                        break;

                    case StringTemplateAtom::Type::Fraction:
                    {
                        auto num = atom.num.num;
                        auto den = atom.num.den;
                        if constexpr (is_renderable)
                        {
                            // clang-format off
                            if (num == 1 && den == 2) dst.AppendString("½"); else
                            if (num == 1 && den == 4) dst.AppendString("¼"); else
                            if (num == 3 && den == 4) dst.AppendString("¾"); else
                            // clang-format on
                            {
                                dst.AppendIntToChars(num);
                                dst.push_back('/');
                                dst.AppendIntToChars(den);
                            }
                        }
                        else if constexpr (is_searchable)
                        {
                            auto value = (float)num / (float)den;
                            EncodeSearchableNumber(dst, value);
                        }
                        plurality = Plurality::Plural;
                        break;
                    }

                    case StringTemplateAtom::Type::Number:
                    {
                        auto num = atom.num.num;
                        if constexpr (is_renderable)
                        {
                            dst.AppendIntToChars(num);
                        }
                        else if constexpr (is_searchable)
                        {
                            EncodeSearchableNumber(dst, (float)num);
                        }
                        plurality = num == 1 ? Plurality::Singular : Plurality::Plural;
                        break;
                    }

                    case StringTemplateAtom::Type::Color:
                        RichText::ColorTag{atom.parent.color}.ToChars(dst);
                        break;

#ifdef _DEBUG
                    default:
                        assert(false);
#endif
                }
            }
        }
    };

    void StringManager::AssembleSearchableString(StringTemplate t, OutBuf<char> dst)
    {
        auto a = StringTemplateAssembler<AssembleMode::Searchable>{
            .mgr = *this,
            .nodes = t.rest,
            .dst = dst,
        };
        a.Assemble(std::span<StringTemplateAtom>(&t.root, 1), {});
    }
    void StringManager::AssembleRenderableString(StringTemplate t, OutBuf<char> dst)
    {
        auto a = StringTemplateAssembler<AssembleMode::Renderable>{
            .mgr = *this,
            .nodes = t.rest,
            .dst = dst,
        };
        a.Assemble(std::span<StringTemplateAtom>(&t.root, 1), {});
    }
}