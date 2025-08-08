#pragma once

#include <GWCA/Constants/Constants.h>
#include <string_arena.h>

namespace HerosInsight
{
    struct SkillTextProvider
    {
        static SkillTextProvider &GetInstance(GW::Constants::Language language);

        bool IsReady() const;
        std::string_view GetName(GW::Constants::SkillID skill_id);
        IndexedStringArena<char> &GetNames();
        std::string_view GetGenericDescription(GW::Constants::SkillID skill_id, bool is_concise);
        void GetDescriptionCopy(GW::Constants::SkillID skill_id, bool is_concise, int8_t attr_lvl, std::span<char> &dst);
        static void SkillDescriptionToEncStr(const GW::Skill &skill, bool concise, int32_t attr_lvl, std::span<wchar_t> &dst);

    private:
        SkillTextProvider(GW::Constants::Language language);
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
                std::memset(this->replacement + replacement_size, 0,
                    sizeof(this->replacement) - replacement_size);
            }

            template <typename Writer>
            Modification(uint16_t position, uint8_t size, Writer &&f)
                : position(position), size(size)
            {
                std::span<char> span = this->replacement;
                f(span);
                replacement_size = span.size();

                // Zero the remaining bytes (if any)
                std::memset(this->replacement + replacement_size, 0,
                    sizeof(this->replacement) - replacement_size);
            }
        };

        GW::Constants::Language language;
        IndexedStringArena<char> names{GW::Constants::SkillMax};
        IndexedStringArena<char> generic_descriptions{GW::Constants::SkillMax * 2};
        IndexedStringArena<Modification> spec_kits{GW::Constants::SkillMax * 2 * 21};

        struct DecodeProcess;
        DecodeProcess *decode_proc;
        friend struct DecodeProcess;
    };
}