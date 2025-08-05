#include <GWCA/Constants/Skills.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <coroutine>
#include <variant>

#include <custom_skill_data.h>
#include <string_arena.h>
#include <utils.h>

#include "skill_text_provider.h"

namespace HerosInsight
{
    size_t GetGenericIndex(size_t skill_id, bool is_concise)
    {
        return GW::Constants::SkillMax * is_concise + skill_id;
    }

    size_t GetGenericAttrIndex(size_t skill_id, bool is_concise, int32_t attr_lvl)
    {
        assert(attr_lvl >= 0);
        return GW::Constants::SkillMax * (attr_lvl * 2 + is_concise) + skill_id;
    }

#ifdef _DEBUG
    // If we are always in gamethread, we dont need any synchronization
#define CHECK_THREAD assert(GW::GameThread::IsInGameThread())
#else
#define CHECK_THREAD ((void)0)
#endif

    struct SkillTextProvider::DecodeProcess
    {
        struct PerConc
        {
            struct PerSkill
            {
                struct PerAttr
                {
                    int8_t attr_lvl;
                    int8_t array_index;

                    PerAttr(int8_t array_index) : attr_lvl(-2), array_index(array_index) {}

                    PerSkill &GetParent()
                    {
                        auto data_ptr = (uintptr_t)(this - array_index);
                        return *(PerSkill *)(data_ptr - offsetof(PerSkill, data));
                    }
                };
                uint16_t skill_id_16;
                PerAttr data[8]{0, 1, 2, 3, 4, 5, 6, 7};

                PerConc &GetParent()
                {
                    auto data_ptr = (uintptr_t)(this - skill_id_16);
                    return *(PerConc *)(data_ptr - offsetof(PerConc, data));
                }
            };
            bool is_concise;
            PerSkill data[GW::Constants::SkillMax];

            DecodeProcess &GetParent()
            {
                auto data_ptr = (uintptr_t)(this - is_concise);
                return *(DecodeProcess *)(data_ptr - offsetof(DecodeProcess, data));
            }
        };

        struct ParamSpan
        {
            uint16_t position;
            uint8_t size;
            uint8_t param_id;
        };

        SkillTextProvider *provider;
        size_t decoding_count = 0;
        IndexedStringArena<char> build_site{GW::Constants::SkillMax * 2 * 21};
        IndexedStringArena<ParamSpan> param_spans{GW::Constants::SkillMax * 2};
        PerConc data[2]; // Holds ptr destinations for all combinations of is_concise, skill_id, and attr_lvl

        static void SpecializeGenericDescription(std::string_view generic_desc, std::span<SkillTextProvider::Modification> spec_kit, std::span<char> &build_site)
        {
            size_t i_src = 0;
            size_t i_dst = 0;
            auto CopyOverUntil = [&](size_t until)
            {
                size_t copy_size = until - i_src;
                assert(i_dst + copy_size <= build_site.size());
                generic_desc.copy(&build_site[i_dst], copy_size, i_src);
                i_src += copy_size;
                i_dst += copy_size;
            };
            for (auto &mod : spec_kit)
            {
                CopyOverUntil(mod.position);
                i_src += mod.size;
                if (mod.replacement_size > 0)
                {
                    std::memcpy(&build_site[i_dst], mod.replacement, mod.replacement_size);
                    i_dst += mod.replacement_size;
                }
            }
            CopyOverUntil(generic_desc.size());
            build_site = build_site.subspan(0, i_dst);
        }

        static void OnDescDecoded(void *param, const wchar_t *wstr)
        {
            CHECK_THREAD;
            auto &per_attr = *reinterpret_cast<PerConc::PerSkill::PerAttr *>(param);
            auto attr_lvl = per_attr.attr_lvl;
            auto &per_skill = per_attr.GetParent();
            auto skill_id_16 = per_skill.skill_id_16;
            auto &per_conc = per_skill.GetParent();
            bool is_concise = per_conc.is_concise;
            auto &decode = per_conc.GetParent();
            auto &manager = *decode.provider;
            bool is_generic = attr_lvl == -1;
            auto dst_ptr = is_generic ? &manager.generic_descriptions : &decode.build_site;
            auto &dst = *dst_ptr;
            auto dst_index = is_generic ? GetGenericIndex(skill_id_16, is_concise) : GetGenericAttrIndex(skill_id_16, is_concise, attr_lvl);
            dst.BeginWrite();
            if (is_generic)
                decode.param_spans.BeginWrite();
            auto init_size = dst.size();
            auto len = wcslen(wstr);
            auto p = (wchar_t *)wstr;
            auto end = p + len;
            while (p < end)
            {
                {
                    double param_id;
                    auto p_tmp = p;
                    if (Utils::TryRead(L"<param=", p_tmp, end) &&
                        Utils::TryReadNumber(p_tmp, end, param_id) &&
                        Utils::TryRead(L'>', p_tmp, end))
                    {
                        while (p_tmp < end && *p_tmp != L'<')
                            ++p_tmp;
                        if (Utils::TryRead(L"</param>", p_tmp, end))
                        {
                            if (is_generic)
                            {
                                auto &skill = *GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id_16));
                                auto param = GetSkillParam(skill, (uint32_t)param_id);
                                auto param_str_pos = dst.size() - init_size;
                                uint8_t param_str_size;
                                dst.AppendBufferAndOverwrite(16,
                                    [param, &param_str_size](std::span<char> &dst)
                                    {
                                        param.Print(-1, dst);
                                        param_str_size = dst.size();
                                    });
                                decode.param_spans.emplace_back(param_str_pos, param_str_size, (uint8_t)param_id);
                            }

                            p = p_tmp;
                            continue;
                        }
                    }
                }

                auto wc = *p++;
                auto c = wc <= 0x7F ? static_cast<char>(wc) : '?';
                dst.push_back(c);
            }
            dst.EndWrite(dst_index);
            if (is_generic)
                decode.param_spans.EndWrite(dst_index);

            decode.DecrementDecodingCount();
        };

        void StartDecodingDescriptions()
        {
            for (bool is_concise : {false, true})
            {
                auto &per_conc = data[is_concise];
                per_conc.is_concise = is_concise;
                for (size_t skill_id = 0; skill_id < GW::Constants::SkillMax; ++skill_id)
                {
                    auto &per_skill = per_conc.data[skill_id];
                    per_skill.skill_id_16 = static_cast<uint16_t>(skill_id);

                    auto &skill = *GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
                    std::bitset<8> visited;
                    for (size_t attr_index = 0; attr_index < 22; ++attr_index)
                    {
                        int8_t attr_lvl = static_cast<int8_t>(attr_index) - 1;
                        size_t combination = 0;
                        for (size_t i = 0; i < 3; ++i)
                        {
                            auto is_singular = GetSkillParam(skill, i).IsSingular(attr_lvl);
                            combination |= is_singular << i;
                        }
                        if (!visited[combination])
                        {
                            visited[combination] = true;
                            per_skill.data[combination].attr_lvl = attr_lvl;
                        }
                    }
                    this->decoding_count += visited.count();
                }
            }

            static constexpr auto N_DESCRIPTIONS = GW::Constants::SkillMax * 2;
            assert(this->decoding_count >= N_DESCRIPTIONS);
            static constexpr auto avg_chars_per_desc = 300;
            auto result_decode_count = N_DESCRIPTIONS;
            auto build_site_decode_count = this->decoding_count - result_decode_count;

            provider->generic_descriptions.ReserveElements(result_decode_count * avg_chars_per_desc);
            this->build_site.ReserveElements(build_site_decode_count * avg_chars_per_desc);
            static constexpr auto avg_params_per_desc = 2;
            this->param_spans.ReserveElements(N_DESCRIPTIONS * avg_params_per_desc);

            wchar_t enc_str[256];
            for (bool is_concise : {false, true})
            {
                auto &per_conc = data[is_concise];
                for (size_t skill_id = 0; skill_id < GW::Constants::SkillMax; ++skill_id)
                {
                    auto &per_skill = per_conc.data[skill_id];
                    auto &skill = *GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));

                    for (size_t attr_index = 0; attr_index < 8; ++attr_index)
                    {
                        auto attr_lvl = per_skill.data[attr_index].attr_lvl;
                        if (attr_lvl == -2)
                            continue;
                        assert(attr_lvl >= -1 && attr_lvl <= 20);

                        std::span<wchar_t> enc_str_span = enc_str;
                        SkillDescriptionToEncStr(skill, is_concise, attr_lvl, enc_str_span);
                        GW::UI::AsyncDecodeStr(enc_str, OnDescDecoded, (void *)&per_skill.data[attr_index], provider->language);
                    }
                }
            }
        }

        static void OnNameDecoded(void *param, const wchar_t *wstr)
        {
            CHECK_THREAD;
            auto &per_skill = *reinterpret_cast<PerConc::PerSkill *>(param);
            auto skill_id = per_skill.skill_id_16;
            auto &decode_proc = per_skill.GetParent().GetParent();
            auto &provider = *decode_proc.provider;
            provider.names.BeginWrite();
            for (const wchar_t *pwc = wstr; *pwc; ++pwc)
            {
                auto wc = *pwc;
                auto c = wc <= 0x7F ? static_cast<char>(wc) : '?';
                provider.names.push_back(c);
            }
            provider.names.EndWrite(skill_id);

            decode_proc.DecrementDecodingCount();
        };

        void StartDecodingNames()
        {
            static constexpr auto N_NAMES = GW::Constants::SkillMax;
            static constexpr auto avg_chars_per_name = 16;
            provider->names.ReserveElements(N_NAMES * avg_chars_per_name);

            wchar_t enc_str[32];
            for (size_t skill_id = 0; skill_id < GW::Constants::SkillMax; ++skill_id)
            {
                auto &skill = *GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
                bool success = GW::UI::UInt32ToEncStr(skill.name, enc_str, std::size(enc_str));
                assert(success);
                GW::UI::AsyncDecodeStr(enc_str, OnNameDecoded, &data[0].data[skill_id], provider->language);
            }
        }

        DecodeProcess(SkillTextProvider *manager)
            : provider(manager)
        {
            StartDecodingDescriptions();
            StartDecodingNames();
        }

        void FinalizeDescription(size_t skill_id, bool is_concise)
        {
            auto gen_index = GetGenericIndex(skill_id, is_concise);
            auto &skill = *GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
            auto generic_str = provider->generic_descriptions.GetIndexed(gen_index);
            std::string_view specific_str{};
            for (int8_t attr_lvl = 0; attr_lvl <= 20; ++attr_lvl)
            {
                auto new_specific_str = this->build_site.GetIndexed(attr_lvl);
                if (new_specific_str.data() != nullptr)
                    specific_str = new_specific_str;

                provider->spec_kits.BeginWrite();
                size_t i_gen = 0;
                size_t i_spe = 0;
                size_t i_param = 0;
                while (i_gen < generic_str.size())
                {
                    if (i_param < param_spans.size())
                    {
                        auto &param_span = param_spans[i_param];
                        if (i_gen == param_span.position)
                        {
                            auto param = GetSkillParam(skill, param_span.param_id);
                            provider->spec_kits.emplace_back(
                                param_span.position,
                                param_span.size,
                                [param, attr_lvl](auto &dst)
                                {
                                    param.Print(attr_lvl, dst);
                                });
                            i_gen += param_span.size;
                            ++i_param;
                            continue;
                        }
                    }
                    if (specific_str.data() != nullptr)
                    {
                        if (generic_str[i_gen] != specific_str[i_spe])
                        {
                            if (generic_str[i_gen] == 's')
                            {
                                provider->spec_kits.emplace_back(i_gen, 1, "");
                                ++i_gen;
                                continue;
                            }
                            else if (specific_str[i_spe] == 's')
                            {
                                provider->spec_kits.emplace_back(i_gen, 0, "s");
                                ++i_spe;
                                continue;
                            }
                            else
                            {
                                assert(false);
                            }
                        }
                        ++i_spe;
                    }
                    ++i_gen;
                }
                assert(i_gen == generic_str.size());
                assert(i_spe == specific_str.size());
                auto spec_kit_index = GetGenericAttrIndex(skill_id, is_concise, attr_lvl);
                provider->spec_kits.EndWrite(spec_kit_index);
            }
        }

        void DecrementDecodingCount()
        {
            --this->decoding_count;
            if (this->decoding_count != 0)
                return;

            for (auto is_concise : {false, true})
            {
                for (uint16_t skill_id_16 = 0; skill_id_16 < GW::Constants::SkillMax; ++skill_id_16)
                {
                    this->FinalizeDescription(skill_id_16, is_concise);
                }
            }
            this->provider->decode_proc = nullptr;
            delete this;
        }
    };
    // static constexpr size_t DecodeProcessSize = sizeof(SkillTextProvider::DecodeProcess);

    SkillTextProvider::SkillTextProvider(GW::Constants::Language language)
        : language(language), decode_proc(new DecodeProcess(this)) {}

    SkillTextProvider &SkillTextProvider::GetInstance(GW::Constants::Language language)
    {
        static std::optional<SkillTextProvider> providers[GW::Constants::LangMax];

        auto &provider = providers[static_cast<size_t>(language)];
        if (!provider)
            provider.emplace(language);
        return provider.value();
    }

    bool SkillTextProvider::IsReady() const
    {
        CHECK_THREAD;
        return this->decode_proc == nullptr;
    }

    std::string_view SkillTextProvider::GetName(GW::Constants::SkillID skill_id)
    {
        assert(this->IsReady());
        return this->names.GetIndexed(static_cast<size_t>(skill_id));
    }

    std::string_view SkillTextProvider::GetGenericDescription(GW::Constants::SkillID skill_id, bool is_concise)
    {
        assert(this->IsReady());
        auto index = GetGenericIndex(static_cast<size_t>(skill_id), is_concise);
        return this->generic_descriptions.GetIndexed(index);
    }

    void SkillTextProvider::GetDescriptionCopy(GW::Constants::SkillID skill_id, bool is_concise, int8_t attr_lvl, std::span<char> &dst)
    {
        auto generic_desc = GetGenericDescription(skill_id, is_concise);
        if (attr_lvl == -1)
        {
            assert(generic_desc.size() <= dst.size());
            generic_desc.copy(dst.data(), generic_desc.size());
            dst = dst.subspan(0, generic_desc.size());
            return;
        }

        auto kit_index = GetGenericAttrIndex(static_cast<size_t>(skill_id), is_concise, attr_lvl);
        auto kit = this->spec_kits.GetIndexed(kit_index);
        DecodeProcess::SpecializeGenericDescription(generic_desc, kit, dst);
    }

    /*
--------- Information about GW's string encoding ---------

From GWCA/TB++ discord
{
    User "Jon [SNOW]":
    {
        \x2 means "append the following",
        \x107 means "new section, unencoded text",
        \x108 means "the next section is already formatted how I want it",
        \x1 means "close section"
    }

    User "AdituV":
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

    void SkillTextProvider::SkillDescriptionToEncStr(const GW::Skill &skill, bool concise, int32_t attr_lvl, std::span<wchar_t> &dst)
    {
        size_t written_len = 0;
        FixedArrayRef<wchar_t> buffer{dst, written_len};
        auto str_id = concise ? skill.concise : skill.description;
        bool success = GW::UI::UInt32ToEncStr(str_id, buffer.data(), buffer.capacity());
        assert(success);
        written_len = wcslen(buffer.data());

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

        dst = dst.subspan(0, written_len);
        buffer.push_back('\0');
    }
}