#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/AssetMgr.h>
#include <rich_text.h>
#include <span_vector.h>
#include <string_manager.h>

namespace HerosInsight::Text
{
    struct GWStringDescriptor
    {
        uint32_t gw_string_id;
        uint32_t user_context;
    };

    enum struct IterGWStringsError
    {
        None,
        NoInput,
        FileNotFound,
        FileNotReadable,
    };

    template <typename Op>
        requires std::invocable<Op, GWStringDescriptor &, GW::StringFileInfo &>
    IterGWStringsError IterGWStrings(GW::Constants::Language language, std::span<GWStringDescriptor> descriptors, Op &&op)
    {
        if (descriptors.empty())
            return IterGWStringsError::NoInput;

        std::sort(
            descriptors.begin(),
            descriptors.end(),
            [](auto &a, auto &b)
            {
                return a.gw_string_id < b.gw_string_id;
            }
        );

        auto cache = GW::AssetMgr::GetTextCache();
        auto strings_per_file = cache->m_stringsPerFile;
        auto files = cache->GetFiles(language);

        auto descriptor = descriptors.begin();
        auto end = descriptors.end();
        while (true)
        {
            auto file_index = descriptor->gw_string_id / strings_per_file;
            if (file_index >= files.size())
                return IterGWStringsError::FileNotFound;
            auto &file = files[file_index];
            // auto fileId = GW::AssetMgr::FileHashToFileId(file.path);
            // if (fileId && !file.is_downloading)
            // {
            //     file.is_downloading = 1;
            //     assert(cache->m_downloadFunc);
            //     cache->m_downloadFunc(fileId);
            // }
            auto reading_str_id = file.str_id_begin;

            auto readable = GW::AssetMgr::ReadableFile(file.path);
            if (!readable)
                return IterGWStringsError::FileNotReadable;
            auto data_end = readable.data + readable.size;
            auto file_header = readable.GetFileHeader();

            for (auto &info : readable.Strings())
            {
                // assert(reading_str_id < file.str_id_end);
                // assert((uintptr_t)&info < (uintptr_t)data_end);
                // assert(info.wType == 0x10);
                if (descriptor->gw_string_id == reading_str_id)
                {
                    // auto wstr = info.GetStr();
                    do
                    {
                        op(*descriptor, info);
                        ++descriptor;
                        if (descriptor == end)
                            return IterGWStringsError::None;
                    } while (descriptor->gw_string_id == reading_str_id);
                    if (descriptor->gw_string_id >= file.str_id_end)
                        break;
                }
                ++reading_str_id;
            }
        }
    }

    void SkillDescriptionToEncStr(const GW::Skill &skill, bool concise, int32_t attr_rank, std::span<wchar_t> dst);

    inline StringManager s_Manager;

#define AssimilateIntoVar(var, str) HerosInsight::Text::StringManager::StrId var = HerosInsight::Text::s_Manager.AssimilateString(str);
    struct CommonStrings
    {
        AssimilateIntoVar(dyn_strId, "<c=#8fff8f>%str1%</c>");
        AssimilateIntoVar(dyn_range_strId, "<c=#8fff8f>(%str1%...%str2%)</c>");
        AssimilateIntoVar(comma, ", ");
    };
    inline CommonStrings s_CommonStrings;

    struct Provider
    {
        std::string_view GetName(GW::Constants::SkillID skill_id);
        std::string_view GetRawDescription(GW::Constants::SkillID skill_id, bool is_concise);
        SlotSpanVector<char> *GetNames();
        SlotSpanVector<char> *GetRawDescriptions(bool is_concise);
        StringTemplateAtom MakeSkillParam(StringTemplateAtom::Builder &b, GW::Constants::SkillID skill_id, int8_t attr_rank, size_t param_id);
        StringTemplateAtom MakeSkillDescription(StringTemplateAtom::Builder &b, GW::Constants::SkillID skill_id, bool is_concise, int8_t attr_rank);
        StringTemplateAtom MakeSkillName(StringTemplateAtom::Builder &b, GW::Constants::SkillID skill_id);

        Provider(GW::Constants::Language language);

        struct SkillTextType
        {
            enum Enum : uint16_t
            {
                Name,
                Description,
                Concise,

                COUNT,
            };
        };

        GW::Constants::Language language;

        std::array<StringManager::StrId, GW::Constants::SkillMax> skill_strIds[SkillTextType::COUNT];
        SlotSpanVector<char> skill_raw[SkillTextType::COUNT];
    };

    Provider &GetTextProvider(GW::Constants::Language language = GW::Constants::Language::English);
}