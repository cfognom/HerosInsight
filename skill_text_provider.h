#pragma once

#include <GWCA/Constants/Constants.h>
#include <rich_text.h>
#include <string_arena.h>

namespace HerosInsight
{
    struct SkillTextProvider
    {
        static SkillTextProvider &GetInstance(GW::Constants::Language language = GW::Constants::Language::English);

        bool IsReady() const;
        std::string_view GetName(GW::Constants::SkillID skill_id);
        IndexedStringArena<char> &GetNames();
        std::string_view GetGenericDescription(GW::Constants::SkillID skill_id, bool is_concise);
        void MakeDescription(GW::Constants::SkillID skill_id, bool is_concise, int8_t attr_lvl, std::span<char> &dst);
        static void SkillDescriptionToEncStr(const GW::Skill &skill, bool concise, int32_t attr_lvl, std::span<wchar_t> &dst);

        SkillTextProvider(GW::Constants::Language language);

    private:
        struct Modification
        {
            uint16_t position;
            uint8_t size;
            uint8_t replacement_size;
            char replacement[4];
            Modification(uint16_t position, uint8_t size, std::string_view replacement)
                : position(position), size(size), replacement_size(replacement.size())
            {
                assert(replacement.size() <= sizeof(this->replacement));
                replacement.copy(this->replacement, replacement.size());

                // Zero the remaining bytes (if any)
                std::memset(this->replacement + replacement_size, 0, sizeof(this->replacement) - replacement_size);
            }
        };

        GW::Constants::Language language;
        IndexedStringArena<char> names;
        // IndexedStringArena<char> generic_descriptions;
        RichText::RichTextArena generic_descriptions;
        IndexedStringArena<Modification> spec_kits;

        struct DecodeProcess;
        std::atomic<bool> ready = false;
        friend struct DecodeProcess;
    };
}