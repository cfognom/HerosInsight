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
#include <parsing.h>
#include <string_arena.h>
#include <string_cache.h>
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
            auto &cont = this->descriptions[is_concise];
            auto &cache = cont.cache;
            auto deduper1 = cache.pieces.CreateDeduper(0);
            auto deduper2 = cache.strings.CreateDeduper(0);
            std::vector<uint16_t> remapper(raw_descs.SpanCount(), std::numeric_limits<uint16_t>::max());
            for (size_t skill_id = 0; skill_id < GW::Constants::SkillMax; ++skill_id)
            {
                auto src_span_id = raw_descs.GetSpanId(skill_id).value();
                auto &dst_span_id = remapper[src_span_id];
                if (dst_span_id == std::numeric_limits<uint16_t>::max())
                {
                    auto raw_desc = raw_descs.Get(src_span_id);
                    auto id = cache.AssimilateString(raw_desc, &deduper1, &deduper2);
                    dst_span_id = id;
                    cont.skill_id_to_str_id[skill_id] = id;
                }
                else
                {
                    cont.skill_id_to_str_id[skill_id] = dst_span_id;
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

    template <typename Mode>
    bool WriteSkillParam(const GW::Constants::SkillID skill_id, size_t param_id, int8_t attr_lvl, OutBuf<char> out)
    {
        auto &skill = GW::SkillbarMgr::GetSkills()[static_cast<size_t>(skill_id)];
        bool has_attribute = attr_lvl != -1;

        auto param = GetSkillParam(skill, param_id);

        auto AppendValue = [&](uint32_t value)
        {
            if constexpr (std::is_same_v<Mode, BuildMode::Readable>)
            {
                out.AppendIntToChars(value);
            }
            else if constexpr (std::is_same_v<Mode, BuildMode::Searchable>)
            {
                EncodeSearchableNumber(out, (float)value);
            }
        };

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
            AppendValue(value);
        }
        else
        {
            // If two values, add range with two literals
            out.push_back('(');
            AppendValue(param.val0);
            out.AppendRange(std::string_view("..."));
            AppendValue(param.val15);
            out.push_back(')');
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
        auto &cont = this->descriptions[is_concise];
        auto str_id = cont.skill_id_to_str_id[(size_t)skill_id];
        auto SubsProvider = [skill_id, attr_lvl](size_t param_id, OutBuf<char> out) -> bool
        {
            return WriteSkillParam<BuildMode::Readable>(skill_id, param_id, attr_lvl, out);
        };
        cont.cache.BuildReadableString(str_id, dst, SubsProvider);
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