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

    // The text formatting of descriptions changes slightly depending on the skill parameters.
    // For example "1 strike" or "2 strikes". Note the added 's' at the end.
    // This function calculates the format ID.
    // Returns a number in the range [0, 7].
    uint8_t CalcDescFormatID(size_t skill_id, bool is_concise, int32_t attr_lvl)
    {
        auto &skill = *GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
        uint8_t format_id = 0;
        for (size_t i = 0; i < 3; ++i)
        {
            auto is_singular = GetSkillParam(skill, i).IsSingular(attr_lvl);
            format_id |= is_singular << i;
        }
        return format_id;
    }

#ifdef _DEBUG
    // If we are always in gamethread, we dont need any synchronization
#define CHECK_THREAD assert(GW::Render::GetIsInRenderLoop())
#else
#define CHECK_THREAD ((void)0)
#endif

    struct SkillTextProvider::DecodeProcess
    {
        struct ConciseCatcher
        {
            struct SkillCatcher
            {
                struct FormatCatcher
                {
                    uint8_t format_id; // Specific decode processes target these values for their param ptr. From it they can know the format_id, skill_id, and conciseness by walking up the hierarchy.

                    SkillCatcher &GetParent()
                    {
                        auto format_catchers_ptr = (uintptr_t)(this - format_id);
                        return *(SkillCatcher *)(format_catchers_ptr - offsetof(SkillCatcher, format_catchers));
                    }
                };
                uint16_t skill_id_16;
                FormatCatcher format_catchers[8];

                ConciseCatcher &GetParent()
                {
                    auto skill_catchers_ptr = (uintptr_t)(this - skill_id_16);
                    return *(ConciseCatcher *)(skill_catchers_ptr - offsetof(ConciseCatcher, skill_catchers));
                }
            };
            bool is_concise;
            SkillCatcher skill_catchers[GW::Constants::SkillMax];

            DecodeProcess &GetParent()
            {
                auto data_ptr = (uintptr_t)(this - is_concise);
                return *(DecodeProcess *)(data_ptr - offsetof(DecodeProcess, conc_catchers));
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
        IndexedStringArena<char> build_site;
        IndexedStringArena<ParamSpan> param_spans;
        ConciseCatcher conc_catchers[2];
        bool is_initializing = false;

        static void SpecializeGenericDescription(RichText::RichText generic_desc, std::span<SkillTextProvider::Modification> spec_kit, RichText::RichText &build_site)
        {
            // Copy over all tags, we adjust the offsets later
            assert(generic_desc.tags.size() <= build_site.tags.size());
            std::memcpy(build_site.tags.data(), generic_desc.tags.data(), generic_desc.tags.size());
            build_site.tags = std::span<RichText::TextTag>(build_site.tags.data(), generic_desc.tags.size());

            size_t i_tag = 0;
            size_t i_src = 0;
            size_t i_dst = 0;
            auto CopyOverUntil = [&](size_t until)
            {
                size_t copy_size = until - i_src;
                assert(i_dst + copy_size <= build_site.text.size());
                std::memcpy(&build_site.text[i_dst], &generic_desc.text[i_src], copy_size);
                i_src += copy_size;
                i_dst += copy_size;

                // Adjust tag offsets
                while (i_tag < build_site.tags.size())
                {
                    auto &tag = build_site.tags[i_tag];
                    if (tag.offset >= until)
                        break;

                    auto unalignment = i_dst - i_src;
                    tag.offset += unalignment;
                    i_tag++;
                }
            };
            for (auto &mod : spec_kit)
            {
                CopyOverUntil(mod.position);
                i_src += mod.size;
                if (mod.replacement_size > 0)
                {
                    std::memcpy(&build_site.text[i_dst], mod.replacement, mod.replacement_size);
                    i_dst += mod.replacement_size;
                }
            }
            CopyOverUntil(generic_desc.text.size());
            build_site.text = std::span<char>(build_site.text.data(), i_dst);
        }

        static void ExtractParams(std::span<char> &desc, bool is_generic, IndexedStringArena<ParamSpan> &param_spans, std::span<RichText::TextTag> tags)
        {
            bool is_param = false;
            double param_id;
            uint16_t param_str_pos;
            size_t i_dst = 0;
            size_t i_src = 0;
            auto tag_it = tags.begin();
            while (i_src < desc.size())
            {
                auto rem = std::string_view(desc.subspan(i_src));
                if (Utils::TryRead("</param>", rem))
                {
                    is_param = false;
                    i_src = rem.data() - desc.data();
                    if (is_generic)
                    {
                        uint8_t param_str_size = i_dst - param_str_pos;
                        param_spans.emplace_back(param_str_pos, param_str_size, (uint8_t)param_id);
                    }
                }
                else if (Utils::TryRead("<param=", rem) &&
                         Utils::TryReadNumber(rem, param_id) &&
                         Utils::TryRead('>', rem))
                {
                    is_param = true;
                    param_str_pos = i_dst;
                    i_src = rem.data() - desc.data();
                }
                else
                {
                    bool skip = !is_generic && is_param;
                    if (!skip)
                    {
                        desc[i_dst] = desc[i_src];
                    }

                    ++i_dst;
                    ++i_src;
                }

                if (tag_it != tags.end() && tag_it->offset == i_src)
                {
                    // When we extract a param, we need to adjust the tag offsets
                    tag_it->offset = i_dst;
                    ++tag_it;
                }
            }
            desc = std::span<char>(desc.data(), i_dst);
        }

        static void OnDescDecoded(void *param, const wchar_t *wstr)
        {
            CHECK_THREAD;
            auto &format_catcher = *reinterpret_cast<ConciseCatcher::SkillCatcher::FormatCatcher *>(param);
            auto &skill_catcher = format_catcher.GetParent();
            auto skill_id_16 = skill_catcher.skill_id_16;
            auto &concise_catcher = skill_catcher.GetParent();
            bool is_concise = concise_catcher.is_concise;
            auto &decode = concise_catcher.GetParent();
            auto &provider = *decode.provider;

            bool is_generic = false;

            std::optional<size_t> span_id = std::nullopt;

            std::bitset<23> affected_attr_lvls;
            for (size_t attr_index = 0; attr_index <= 22; ++attr_index)
            {
                auto attr_lvl = static_cast<int8_t>(attr_index) - 1;
                auto format_id = CalcDescFormatID(skill_id_16, is_concise, attr_lvl);

                if (format_id != format_catcher.format_id)
                    continue;

                is_generic |= attr_lvl == -1;
                affected_attr_lvls[attr_index] = true;
            }

            auto text_dst_ptr = is_generic ? &provider.generic_descriptions.text : &decode.build_site;
            auto tags_dst_ptr = is_generic ? &provider.generic_descriptions.tags : nullptr;

            text_dst_ptr->BeginWrite();
            if (is_generic)
            {
                decode.param_spans.BeginWrite();
                tags_dst_ptr->BeginWrite();
            }

            text_dst_ptr->AppendWriteBuffer(1024,
                [&](std::span<char> &text_buf)
                {
                    if (skill_id_16 == (uint16_t)GW::Constants::SkillID::Power_Block)
                    {
                        assert(true);
                    }
                    Utils::WStrToStr(wstr, text_buf);
                    auto ExtractTags = [&](std::span<RichText::TextTag> &tags_buf)
                    {
                        RichText::ExtractTags(std::string_view(text_buf), text_buf, tags_buf);
                        ExtractParams(text_buf, is_generic, decode.param_spans, tags_buf);
                    };
                    if (is_generic)
                    {
                        tags_dst_ptr->AppendWriteBuffer(64, ExtractTags);
                    }
                    else
                    {
                        std::span<RichText::TextTag> tags_span{};
                        ExtractTags(tags_span);
                    }
                });

            if (is_generic)
            {
                auto dst_index = GetGenericIndex(skill_id_16, is_concise);
                decode.param_spans.EndWrite(dst_index);
                text_dst_ptr->EndWrite(dst_index);
                tags_dst_ptr->EndWrite(dst_index);
            }
            else
            {
                std::optional<size_t> span_id = std::nullopt;
                for (auto m = affected_attr_lvls; m.any(); Utils::ClearLowestSetBit(m))
                {
                    auto attr_index = Utils::CountTrailingZeros(m);
                    auto attr_lvl = attr_index - 1;
                    auto dst_index = GetGenericAttrIndex(skill_id_16, is_concise, attr_lvl);
                    if (!span_id.has_value())
                    {
                        span_id = text_dst_ptr->EndWrite(dst_index);
                    }
                    else
                    {
                        text_dst_ptr->SetSpanId(dst_index, span_id.value());
                    }
                }
            }

            decode.DecrementDecodingCount();
        };

        static constexpr auto N_DESCRIPTIONS = GW::Constants::SkillMax * 2;
        void StartDecodingDescriptions()
        {
            auto provider = this->provider;

            provider->generic_descriptions.ReserveFromHint("generic_descriptions");
            this->build_site.ReserveFromHint("build_site");
            this->param_spans.ReserveFromHint("param_spans");

            wchar_t enc_str[256];
            for (bool is_concise : {false, true})
            {
                auto &conc_catcher = conc_catchers[is_concise];
                conc_catcher.is_concise = is_concise;
                for (size_t skill_id = 0; skill_id < GW::Constants::SkillMax; ++skill_id)
                {
                    auto &skill_catcher = conc_catcher.skill_catchers[skill_id];
                    skill_catcher.skill_id_16 = static_cast<uint16_t>(skill_id);

                    auto &skill = *GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
                    std::bitset<8> visited;
                    for (size_t attr_index = 0; attr_index <= 22; ++attr_index)
                    {
                        int8_t attr_lvl = static_cast<int8_t>(attr_index) - 1;
                        auto format_id = CalcDescFormatID(skill_id, is_concise, attr_lvl);
                        if (!visited[format_id])
                        {
                            visited[format_id] = true;

                            auto &format_catcher = skill_catcher.format_catchers[format_id];
                            format_catcher.format_id = format_id;

                            std::span<wchar_t> enc_str_span = enc_str;
                            SkillDescriptionToEncStr(skill, is_concise, attr_lvl, enc_str_span);
                            ++this->decoding_count;
                            GW::UI::AsyncDecodeStr(enc_str, OnDescDecoded, (void *)&format_catcher, provider->language);
                        }
                    }
                }
            }
        }

        static void OnNameDecoded(void *param, const wchar_t *wstr)
        {
            CHECK_THREAD;
            auto &skill_catcher = *reinterpret_cast<ConciseCatcher::SkillCatcher *>(param);
            auto skill_id = skill_catcher.skill_id_16;
            auto &decode_proc = skill_catcher.GetParent().GetParent();
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
            provider->names.ReserveFromHint("names");
            this->decoding_count += GW::Constants::SkillMax;

            wchar_t enc_str[32];
            for (size_t skill_id = 0; skill_id < GW::Constants::SkillMax; ++skill_id)
            {
                auto &skill = *GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
                bool success = GW::UI::UInt32ToEncStr(skill.name, enc_str, std::size(enc_str));
                assert(success);
                GW::UI::AsyncDecodeStr(enc_str, OnNameDecoded, &conc_catchers[0].skill_catchers[skill_id], provider->language);
            }
        }

        DecodeProcess(SkillTextProvider *provider)
            : provider(provider)
        {
            CHECK_THREAD;
            this->is_initializing = true;
            assert(this->decoding_count == 0);
            StartDecodingDescriptions();
            StartDecodingNames();
            this->is_initializing = false;

            if (this->decoding_count == 0)
            {
                FinishDecodeProcess();
            }
        }

        // Expects first chars to mismatch
        std::pair<size_t, size_t> MinPrefixDiffLength(std::string_view a, std::string_view b)
        {
            size_t iend = std::max(a.size(), b.size());
            for (size_t imax = 1; imax < iend; ++imax)
            {
                size_t len_a = std::numeric_limits<size_t>::max();
                size_t len_b = std::numeric_limits<size_t>::max();
                if (imax < a.size())
                {
                    size_t iend = std::min(b.size(), imax + 1);
                    for (size_t i = 0; i < iend; ++i)
                    {
                        if (b[i] == a[imax])
                        {
                            len_b = i;
                            break;
                        }
                    }
                }
                if (imax < b.size())
                {
                    size_t iend = std::min(a.size(), imax + 1);
                    for (size_t i = 0; i < iend; ++i)
                    {
                        if (a[i] == b[imax])
                        {
                            len_a = i;
                            break;
                        }
                    }
                }
                if (len_b < len_a)
                {
                    return {imax, len_b};
                }
                else if (len_a < len_b ||
                         len_a != std::numeric_limits<size_t>::max())
                {
                    return {len_a, imax};
                }
            }
            return {a.size(), b.size()};
        }

        void FinalizeDescription(size_t skill_id, bool is_concise)
        {
            auto gen_index = GetGenericIndex(skill_id, is_concise);
            auto &skill = *GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(skill_id));
            auto generic_str = std::string_view(provider->generic_descriptions.text.GetIndexed(gen_index));
            for (int8_t attr_lvl = 0; attr_lvl <= 21; ++attr_lvl)
            {
                auto index = GetGenericAttrIndex(skill_id, is_concise, attr_lvl);
                auto specific_str = std::string_view(this->build_site.GetIndexed(index));

                provider->spec_kits.BeginWrite();
                uint16_t i_gen = 0;
                size_t i_spe = 0;
                size_t i_param_span = 0;
                auto gen_param_spans = this->param_spans.GetIndexed(gen_index);
                while (i_gen < generic_str.size())
                {
                    if (i_param_span < gen_param_spans.size())
                    {
                        auto &param_span = gen_param_spans[i_param_span];
                        if (i_gen == param_span.position)
                        {
                            auto param = GetSkillParam(skill, param_span.param_id);
                            char buffer[4];
                            std::span<char> str{buffer, sizeof(buffer)};
                            param.Print(attr_lvl, str);
                            provider->spec_kits.emplace_back(
                                param_span.position,
                                param_span.size,
                                std::string_view(buffer, str.size()));
                            i_gen += param_span.size;
                            ++i_param_span;
                            continue;
                        }
                    }
                    if (specific_str.data() != nullptr)
                    {
                        if (generic_str[i_gen] != specific_str[i_spe])
                        {
                            auto [gen_len, spe_len] = MinPrefixDiffLength(generic_str.substr(i_gen), specific_str.substr(i_spe));
                            auto replacement = specific_str.substr(i_spe, spe_len);
                            provider->spec_kits.emplace_back(i_gen, gen_len, replacement);
                            i_gen += gen_len;
                            i_spe += spe_len;
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

        void FinishDecodeProcess()
        {
            provider->spec_kits.ReserveFromHint("spec_kits");
            for (auto is_concise : {false, true})
            {
                for (uint16_t skill_id_16 = 0; skill_id_16 < GW::Constants::SkillMax; ++skill_id_16)
                {
                    this->FinalizeDescription(skill_id_16, is_concise);
                }
            }
            provider->spec_kits.StoreCapacityHint("spec_kits");

            provider->generic_descriptions.StoreCapacityHint("generic_descriptions");
            provider->names.StoreCapacityHint("names");
            this->build_site.StoreCapacityHint("build_site");
            this->param_spans.StoreCapacityHint("param_spans");

            this->provider->ready.store(true);
            delete this;
        }

        void DecrementDecodingCount()
        {
            assert(this->decoding_count > 0);
            --this->decoding_count;
            if (this->decoding_count == 0 && !this->is_initializing)
            {
                FinishDecodeProcess();
            }
        }
    };
    // static constexpr size_t DecodeProcessSize = sizeof(SkillTextProvider::DecodeProcess);

    SkillTextProvider::SkillTextProvider(GW::Constants::Language language)
        : language(language)
    {
        new DecodeProcess(this);
    }

    SkillTextProvider &SkillTextProvider::GetInstance(GW::Constants::Language language)
    {
        static std::unique_ptr<SkillTextProvider> providers[GW::Constants::LangMax] = {};

        auto &provider = providers[static_cast<size_t>(language)];
        if (!provider)
        {
            provider = std::make_unique<SkillTextProvider>(language);
        }
        return *provider;
    }

    bool SkillTextProvider::IsReady() const
    {
        return this->ready.load();
    }

    std::string_view SkillTextProvider::GetName(GW::Constants::SkillID skill_id)
    {
        assert(this->IsReady());
        return std::string_view(this->names.GetIndexed(static_cast<size_t>(skill_id)));
    }

    IndexedStringArena<char> &SkillTextProvider::GetNames()
    {
        assert(this->IsReady());
        return this->names;
    }

    RichText::RichText SkillTextProvider::GetGenericDescription(GW::Constants::SkillID skill_id, bool is_concise)
    {
        assert(this->IsReady());
        auto index = GetGenericIndex(static_cast<size_t>(skill_id), is_concise);
        return this->generic_descriptions.GetIndexed(index);
    }

    void SkillTextProvider::MakeDescription(GW::Constants::SkillID skill_id, bool is_concise, int8_t attr_lvl, RichText::RichText &dst)
    {
        auto generic_desc = GetGenericDescription(skill_id, is_concise);
        if (attr_lvl == -1)
        {
            size_t copy_size = generic_desc.text.size();
            assert(copy_size <= dst.text.size());
            std::memcpy(dst.text.data(), generic_desc.text.data(), copy_size);
            dst.text = std::span<char>(dst.text.data(), copy_size);

            copy_size = generic_desc.tags.size();
            assert(copy_size <= dst.tags.size());
            std::memcpy(dst.tags.data(), generic_desc.tags.data(), copy_size);
            dst.tags = std::span<RichText::TextTag>(dst.tags.data(), copy_size);
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
        buffer.AppendWith(
            [=](auto &dst)
            {
                bool success = GW::UI::UInt32ToEncStr(str_id, dst.data(), dst.size());
                assert(success);
                auto len = wcslen(dst.data());
                dst = dst.subspan(0, len);
            });

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