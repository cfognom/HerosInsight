#include <GWCA/Constants/Skills.h>
#include <GWCA/Context/TextParser.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/AssetMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <coroutine>
#include <ranges>
#include <variant>

#include <bitview.h>
#include <custom_skill_data.h>
#include <string_arena.h>
#include <utils.h>

#include "text_provider.h"

namespace HerosInsight::Text
{
    Provider &GetTextProvider(GW::Constants::Language language)
    {
        static std::unique_ptr<Provider> providers[GW::Constants::LangMax] = {};

        auto &provider = providers[static_cast<size_t>(language)];
        if (!provider)
        {
            provider = std::make_unique<Provider>(language);
        }
        return *provider;
    }

    void AppendWStrToStrArena(StringArena<char> &arena, std::wstring_view wstr)
    {
        assert(!wstr.empty());
        arena.AppendWriteBuffer(
            wstr.size(),
            [wstr](std::span<char> &buffer)
            {
                // auto written_count = simdutf::convert_utf16_to_utf8_safe(
                //     (const char16_t *)wstr.data(),
                //     wstr.size(),
                //     buffer.data(),
                //     buffer.size()
                // );
                Utils::WStrToStr(wstr.data(), buffer);
                // assert(written_count > 0);
                // buffer = buffer.subspan(0, written_count);
            }
        );
    };

    struct FetchContext
    {
        uint16_t alignas(uint32_t) skill_id;
        Provider::SkillTextType::Enum type;
    };
    static_assert(sizeof(FetchContext) == sizeof(uint32_t));
    static_assert(alignof(FetchContext) == alignof(uint32_t));

    Provider::Provider(GW::Constants::Language language)
        : language(language)
    {
        std::vector<Text::GWStringDescriptor> str_ids_to_get;
        auto skills = GW::SkillbarMgr::GetSkills();
        str_ids_to_get.reserve(skills.size() * 3);
        for (auto &skill : skills)
        {
            uint16_t skill_id = static_cast<uint16_t>(skill.skill_id);
            str_ids_to_get.emplace_back(skill.name, std::bit_cast<uint32_t>(FetchContext{skill_id, Provider::SkillTextType::Name}));
            str_ids_to_get.emplace_back(skill.description, std::bit_cast<uint32_t>(FetchContext{skill_id, Provider::SkillTextType::Description}));
            str_ids_to_get.emplace_back(skill.concise, std::bit_cast<uint32_t>(FetchContext{skill_id, Provider::SkillTextType::Concise}));
        }
        auto error = IterGWStrings(
            this->language,
            str_ids_to_get,
            [this](Text::GWStringDescriptor &fetch_desc, GW::StringFileInfo &info)
            {
                auto wstr = info.GetStr();
                auto context = std::bit_cast<FetchContext>(fetch_desc.user_context);
                auto type = context.type;

                auto &arena = this->skill_raw[type];
                AppendWStrToStrArena(arena, wstr);

                uint32_t span_id = arena.CommitWrittenToIndex(context.skill_id);
            }
        );
        if (error != IterGWStringsError::None)
        {
            throw std::runtime_error("Failed to get skill texts");
        }
        for (bool is_concise : {false, true})
        {
            const auto &raw_descs = *this->GetRawDescriptions(is_concise);
            auto &cache = this->description_cache[is_concise];
            auto deduper = cache.pieces.CreateDeduper(0);
            std::vector<uint16_t> remapper(raw_descs.SpanCount(), std::numeric_limits<uint16_t>::max());
            for (size_t skill_id = 0; skill_id < GW::Constants::SkillMax; ++skill_id)
            {
                auto src_span_id = raw_descs.GetSpanId(skill_id).value();
                auto &dst_span_id = remapper[src_span_id];
                if (dst_span_id == std::numeric_limits<uint16_t>::max())
                {
                    auto raw_desc = raw_descs.Get(src_span_id);
                    cache.AssimilateGWString(raw_desc, &deduper);
                    dst_span_id = cache.strings.CommitWrittenToIndex(skill_id);
                }
                else
                {
                    cache.strings.SetSpanId(skill_id, dst_span_id);
                }
            }
        }
    }

    enum struct Plurality
    {
        Null,
        Unknown,
        Singular,
        Plural,
    };

    Plurality GetPlurality(std::string_view str)
    {
        auto pos = str.find_last_of("0123456789");
        if (pos == std::string_view::npos)
            return Plurality::Null;
        bool is_singular = str[pos] == '1' && (pos == 0 || !Utils::IsDigit(str[pos - 1]));
        return is_singular ? Plurality::Singular : Plurality::Plural;
    }

    void Provider::GWStringCache::AssimilateGWString(std::string_view src_gw_str, StringArena<char>::deduper *deduper)
    {
        enum FixType
        {
            Substitution,
            OptionalS,
            PluralReplace,
            FixCount,
        };

        enum Kind
        {
            Prefix,
            Postfix,
            KindCount,
        };

        static constexpr std::string_view patterns[KindCount][FixCount]{
            {"%str",
             "[s]",
             "[pl:\""},
            {"%",
             "",
             "\"]"}
        };

        size_t fix_positions[FixCount];
        std::memset(fix_positions, -1, sizeof(fix_positions));

        auto FindFix = [&fix_positions, src_gw_str](size_t fix)
        {
            auto &pos = fix_positions[fix];
            auto &pattern = patterns[Prefix][fix];
            pos = src_gw_str.find(pattern, pos + 1);
        };

        // static auto AppendToArena = [this, deduper](std::wstring_view wstr)
        // {
        //     AppendWStrToStrArena(this->pieces, wstr);
        //     return this->pieces.CommitWritten(deduper);
        // };

        for (auto i = 0; i < FixCount; ++i)
        {
            assert(fix_positions[i] == -1);
            FindFix(i);
        }

        size_t piece_begin_idx = 0;
        Plurality replacement_plurality = Plurality::Null;
        FixType fix_id;
        for (; true; FindFix(fix_id))
        {
            auto min_pos_it = std::ranges::min_element(fix_positions);
            fix_id = (FixType)std::distance(fix_positions, min_pos_it);
            auto fix_pos = *min_pos_it;
            bool is_last = fix_pos == std::string_view::npos;
            fix_pos = is_last ? src_gw_str.size() : fix_pos;
            assert(fix_pos <= src_gw_str.size());
            size_t piece_end_idx;
            if (!is_last && fix_id == PluralReplace)
            {
                piece_end_idx = src_gw_str.find_last_of(' ', fix_pos - 1);
                assert(piece_end_idx != std::string_view::npos);
                piece_end_idx++;
            }
            else
            {
                piece_end_idx = fix_pos;
            }

            auto piece = src_gw_str.substr(piece_begin_idx, piece_end_idx - piece_begin_idx);
            size_t subpiece_begin = 0;
            size_t subpiece_end;
            do
            {
                // GWs strings use %% for %, we need to fix that.
                subpiece_end = piece.find("%%", subpiece_begin);
                auto sub_piece = piece.substr(subpiece_begin, std::min(subpiece_end, piece.size() - 1) + 1 - subpiece_begin);
                pieces.Elements().append_range(sub_piece);
                subpiece_begin = subpiece_end + 2;
            } while (subpiece_end != std::string_view::npos);

            if (fix_id == OptionalS || fix_id == PluralReplace)
            {
                // Use embedded plurality if it exists, otherwise fall back to the replacement plurality
                auto embedded_plurality = GetPlurality(piece);
                if (embedded_plurality != Plurality::Null)
                {
                    assert(fix_id != PluralReplace); // PluralReplace can't have embedded plurality (Not implemented)

                    if (embedded_plurality == Plurality::Plural)
                        pieces.Elements().push_back('s');
                    piece_begin_idx = piece_end_idx + patterns[Prefix][OptionalS].size();
                    assert(!is_last);
                    continue;
                }
                assert(replacement_plurality != Plurality::Null);
            }
            auto str_id = pieces.CommitWritten(deduper);
            this->strings.Elements().emplace_back(GWStringPiece::Type::StringAlways, str_id);

            if (is_last)
                break;

            switch (fix_id)
            {
                case Substitution:
                {
                    constexpr auto prefix_size = patterns[Prefix][Substitution].size();
                    constexpr auto postfix_size = patterns[Postfix][Substitution].size();

                    piece_begin_idx = fix_pos + prefix_size;
                    uint16_t subs_id = src_gw_str[piece_begin_idx++] - L'1'; // Their indexing starts at 1 but we want 0-indexed
                    piece_begin_idx += postfix_size;

                    this->strings.Elements().emplace_back(GWStringPiece::Type::Substitution, subs_id);
                    replacement_plurality = Plurality::Unknown; // We assume every replacement contains a number
                    break;
                }
                case OptionalS:
                {
                    piece_begin_idx = piece_end_idx + patterns[Prefix][OptionalS].size();

                    this->strings.Elements().emplace_back(GWStringPiece::Type::CharPluralOnly, (uint16_t)'s');
                    break;
                }
                case PluralReplace:
                {
                    auto singular_piece = src_gw_str.substr(piece_end_idx, fix_pos - piece_end_idx);
                    auto singular_str_id = pieces.push_back(singular_piece, deduper);
                    this->strings.Elements().emplace_back(GWStringPiece::Type::StringSingularOnly, singular_str_id);

                    constexpr auto &prefix = patterns[Prefix][PluralReplace];
                    constexpr auto &postfix = patterns[Postfix][PluralReplace];
                    auto plural_begin_idx = fix_pos + prefix.size();
                    auto plural_end_idx = src_gw_str.find(postfix, plural_begin_idx);
                    assert(plural_end_idx != std::string_view::npos);
                    auto plural_piece = src_gw_str.substr(plural_begin_idx, plural_end_idx - plural_begin_idx);
                    auto plural_str_id = pieces.push_back(plural_piece, deduper);
                    this->strings.Elements().emplace_back(GWStringPiece::Type::StringPluralOnly, plural_str_id);

                    piece_begin_idx = plural_end_idx + postfix.size();
                    break;
                }
            }
        }
    }

    std::string_view Provider::GetName(GW::Constants::SkillID skill_id)
    {
        return (std::string_view)this->skill_raw[SkillTextType::Name].GetIndexed((size_t)skill_id);
    }

    IndexedStringArena<char> *Provider::GetNames()
    {
        return &this->skill_raw[SkillTextType::Name];
    }

    std::string_view Provider::GetRawDescription(GW::Constants::SkillID skill_id, bool is_concise)
    {
        return (std::string_view)this->skill_raw[is_concise ? SkillTextType::Concise : SkillTextType::Description].GetIndexed((size_t)skill_id);
    }

    IndexedStringArena<char> *Provider::GetRawDescriptions(bool is_concise)
    {
        return &this->skill_raw[is_concise ? SkillTextType::Concise : SkillTextType::Description];
    }

    bool WriteSkillParam(const GW::Constants::SkillID skill_id, size_t param_id, int8_t attr_lvl, OutBuf<char> out)
    {
        auto &skill = GW::SkillbarMgr::GetSkills()[static_cast<size_t>(skill_id)];
        bool has_attribute = attr_lvl != -1;

        auto param = GetSkillParam(skill, param_id);

        bool is_constant = param.IsConstant();
        bool is_green = !is_constant;
        if (is_green)
        {
            // Start green color
            out.AppendFormat("<c=#8fff8f><tip={}>", param_id);
        }

        bool is_single_value = has_attribute || is_constant;
        uint32_t value;
        if (is_single_value)
        {
            // If one value, add it as literal
            value = param.Resolve((uint32_t)attr_lvl);
            out.AppendIntToChars(value);
        }
        else
        {
            // If two values, add range with two literals
            out.AppendFormat("({}...{})", param.val0, param.val15);
            value = param.val15;
        }

        bool is_plural = value != 1;

        if (is_green)
        {
            // End green color
            out.AppendRange(std::string_view("</tip></c>"));
        }

        return is_plural;
    }

    void Provider::MakeDescription(GW::Constants::SkillID skill_id, bool is_concise, int8_t attr_lvl, OutBuf<char> dst)
    {
        auto &cache = this->description_cache[is_concise];
        auto string = cache.strings.GetIndexed((size_t)skill_id);
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
                case GWStringPiece::Type::StringSingularOnly:
                case GWStringPiece::Type::StringPluralOnly:
                case GWStringPiece::Type::CharPluralOnly:
                    assert(plurality != Plurality::Null);
            }
#endif
            switch (piece.type)
            {
                case GWStringPiece::Type::StringAlways:
                    str_piece = pieces.Get(piece.value);
                    break;

                case GWStringPiece::Type::StringSingularOnly:
                    if (plurality != Plurality::Singular)
                        continue;
                    str_piece = pieces.Get(piece.value);
                    break;

                case GWStringPiece::Type::StringPluralOnly:
                    if (plurality != Plurality::Plural)
                        continue;
                    str_piece = pieces.Get(piece.value);
                    break;

                case GWStringPiece::Type::CharPluralOnly:
                    if (plurality != Plurality::Plural)
                        continue;
                    dst.push_back((char)piece.value);
                    continue;

                case GWStringPiece::Type::Substitution:
                {
                    auto param_id = piece.value;
                    str_piece = replacements[param_id];
                    if (str_piece.empty())
                    {
                        auto start_idx = dst.size();
                        bool plural = WriteSkillParam(skill_id, param_id, attr_lvl, dst);
                        is_plural[param_id] = plural;
                        replacements[param_id] = std::string_view(&dst[start_idx], dst.size() - start_idx);
                        plurality = plural ? Plurality::Plural : Plurality::Singular;
                        continue;
                    }
                    plurality = is_plural[param_id] ? Plurality::Plural : Plurality::Singular;
                    break;
                }
            }
            dst.AppendRange(str_piece);
        }
    }

    /*
    --------- Information about GW's string encoding ---------

    From GWCA/TB++ discord
    {
        User 'Jon [SNOW]':
        {
            \x2 means "append the following",
            \x107 means "new section, unencoded text",
            \x108 means "the next section is already formatted how I want it",
            \x1 means "close section"
        }

        User 'AdituV':
        {
            Control characters:
            \x0000 - final string terminator
            \x0001 - intermediate string terminator (for nested encstring substitution parameters, or literal unencoded strings)
            \x0002 - append an encoded string.  does not use a \x0001 terminator.
            \x0003 - append a literal unencoded string.  does use a \x0001 terminator.

            Other special ones:
            \x0101-\x0106, \x010d-\x010f - signifies a numeric substitution parameter
            \x0107-\x0109 - signifies a literal unencoded string substitution parameter, which is terminated with \x0001
            \x010a-\x010c - signifies a nested encoded string substitution parameter, which is terminated with \x0001

            These parameter signifiers can also just represent a lookup table entry.
            For example the lookup table entry corresponding to '\x0102' is a new line.
            What they mean depends on their context within the encoded string.

            --------- EXAMPLE ---------
            {
                Agony concise skill description.  I don't know how the LUT text represents
                substitutions yet but it's both positional and type-based.  For now, I'm
                representing it as a hybrid {position:type}.

                0x46C   LUT INDEX ==> "{1:s}. {2:s} {3:s}"
                |---0x10A   ENC PARAM 1
                |   |---0x4A6  LUT INDEX ==> "Binding Ritual"
                |   0x1     END ENC PARAM 1
                |
                |---0x10B   ENC PARAM 2
                |   |---0x8102 0x2F2D   LUT INDEX ==>
                |   |   | "Creates a level {1:s} spirit ({3:s} second lifespan). Causes {2:s} Health loss each second to foes in earshot."
                |   |   | "<c=@SkillDull>This spirit loses {2:s} Health for each foe that loses Health.</c>"
                |   |   |
                |   |   `---0x10A  ENC PARAM 1
                |   |       |---0x108  LUT INDEX ==> "{1:s}"
                |   |       |   `---0x107   LIT PARAM 1
                |   |       |       |---0x3C 0x63 0x3D 0x23 0x38 0x66 0x66 0x66 0x38 0x66 0x3E "<c=#8fff8f>"
                |   |       |       0x1     END LIT PARAM 1
                |   |       |---0x2  APPEND ENCODED
                |   |       |---0x104  LUT INDEX ==> "{1:u}"
                |   |       |   `---0x101   NUM PARAM 1
                |   |       |       `---0x101  VALUE 1   (WORD_VALUE_BASE + 1)
                |   |       |---0x2  APPEND ENCODED
                |   |       |---0x108  LUT INDEX ==> "{1:s}"
                |   |       |   `---0x107 LIT PARAM 1
                |   |       |       |---0x3C 0x2F 0x63 0x3E  "</c>"
                |   |       |       0x1 END LIT PARAM 1
                |   |       0x1   END ENC PARAM 1
                |   |
                |   |-------0x10B ENC PARAM 2
                |   |       |---0x108  LUT INDEX ==> "{1:s}"
                |   |       |   `---0x107   LIT PARAM 1
                |   |       |       |---0x3C 0x63 0x3D 0x23 0x38 0x66 0x66 0x66 0x38 0x66 0x3E "<c=#8fff8f>"
                |   |       |       0x1  END LIT PARAM 1
                |   |       |---0x2  APPEND ENCODED
                |   |       |---0x104  LUT INDEX ==> "{1:u}"
                |   |       |   `---0x101   NUM PARAM 1
                |   |       |       `---0x103   VALUE 3   (WORD_VALUE_BASE + 3)
                |   |       |---0x2  APPEND ENCODED
                |   |       |---0x108  LUT INDEX ==> "{1:s}"
                |   |       |   `---0x107  LIT PARAM 1
                |   |       |       |---0x3C 0x2F 0x63 0x3E  "</c>"
                |   |       |       0x1  END LIT PARAM 1
                |   |       0x1  END ENC PARAM 2
                |   |
                |   |-------0x10C  ENC PARAM 3
                |   |       |---0x108  LUT INDEX ==> "{1:s}"
                |   |       |   `---0x107  LIT PARAM 1
                |   |       |       |---0x3C 0x63 0x3D 0x23 0x38 0x66 0x66 0x66 0x38 0x66 0x3E  "<c=#8fff8f>"
                |   |       |       0x1  END LIT PARAM 1
                |   |       |---0x2  APPEND ENCODED
                |   |       |---0x104  LUT INDEX ==> "{1:u}"
                |   |       |   `---0x101  NUM PARAM 1
                |   |       |       `---0x11E  VALUE 30  (WORD_VALUE_BASE + 30)
                |   |       |---0x2  APPEND ENCODED
                |   |       |---0x108  LUT INDEX ==> "{1:s}"
                |   |       |   `---0x107  LIT PARAM 1
                |   |       |       |---0x3C 0x2F 0x63 0x3E  "</c>"
                |   |       |       0x1  END LIT PARAM 1
                |   |       0x1  END ENC PARAM 3
                |   0x1  END ENC PARAM 2
                |
                `---0x10C  ENC PARAM 3
                    |---0x46E LUT INDEX  ==> "(Attrib: {1:s})"
                    |   `---0x10A  ENC PARAM 1
                    |       |---0x966  LUT INDEX ==> "Channeling Magic"
                    |       0x1    END ENC PARAM 1
                    0x1  END ENC PARAM 3
            }
        }
    }
    */

#define LUT_SKILL_DESC L'\x46C' // "{1:s}. {2:s} {3:s}"
#define LUT_RANGE L'\x45B'      // "({1:u}...{2:u})"
#define LUT_ATTRIB L'\x46E';    // "(Attrib: {1:s})"

    void SkillDescriptionToEncStr(const GW::Skill &skill, bool concise, int32_t attr_lvl, std::span<wchar_t> dst)
    {
        SpanWriter<wchar_t> buffer(dst);
        auto str_id = concise ? skill.concise : skill.description;

        bool success = GW::UI::UInt32ToEncStr(str_id, dst.data(), dst.size());
        assert(success);
        buffer.Len() = wcslen(dst.data());

        bool has_attribute = attr_lvl != -1;
        constexpr uint32_t MAX_VALUE = 0x8000 - 0x100 - 1;
        for (size_t i = 0; i < 3; ++i)
        {
            auto param = GetSkillParam(skill, i);

            buffer.push_back(L"\x10A\x10B\x10C"[i]);

            bool is_constant = param.IsConstant();
            bool is_green = !is_constant;
            if (is_green)
            {
                // Start green color
                buffer.PushFormat(L"\x108\x107<c=#8fff8f><tip=%d><param=%d>\x1\x2", i, i);
            }

            bool is_single_value = has_attribute || is_constant;
            if (is_single_value)
            {
                // If one value, add it as literal
                auto value = param.Resolve((uint32_t)attr_lvl);
                value = std::min(value, MAX_VALUE) + 0x100;
                buffer.PushFormat(L"\x104\x101%c", value);
            }
            else
            {
                // If two values, add range with two literals
                auto value0 = std::min(param.val0, MAX_VALUE) + 0x100;
                auto value15 = std::min(param.val15, MAX_VALUE) + 0x100;
                buffer.PushFormat(L"\x45B\x101%c\x102%c", value0, value15);
            }

            if (is_green)
            {
                // End green color
                buffer.PushFormat(L"\x2\x108\x107</param></tip></c>\x1");
            }

            buffer.push_back('\x1');
        }

        buffer.push_back('\0');
    }
}