#include <Windows.h>
#include <bitset>
#include <codecvt>
#include <d3d9.h>
#include <filesystem>
#include <future>
#include <iostream>
#include <regex>
#include <span>
#include <string>

#include <GWCA/GWCA.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Export.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/GWCA.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/List.h>

#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/EventMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <attribute_or_title.h>
#include <constants.h>
#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <debug_display.h>
#include <effect_tracking.h>
#include <party_data.h>
#include <update_manager.h>

#include "utils.h"

#define FIELD_SIZE(type, field) sizeof(((type *)0)->field)
#define OFFSET_IS_IN_FIELD(type, field, offset) (offset >= offsetof(type, field) && offset < offsetof(type, field) + FIELD_SIZE(type, field))

namespace HerosInsight::Utils
{
    std::string SkillSpecialToString(SkillSpecialFlags single_flag)
    {
        // clang-format off
        switch (single_flag)
        {
            case SkillSpecialFlags::Overcast: return "Overcast";
            case SkillSpecialFlags::Touch: return "Touch";
            case SkillSpecialFlags::Elite: return "Elite";
            case SkillSpecialFlags::HalfRange: return "HalfRange";
            case SkillSpecialFlags::SignetOfCapture: return "SignetOfCapture";
            case SkillSpecialFlags::EasilyInterruptible: return "EasilyInterruptible";
            case SkillSpecialFlags::FailureChance: return "FailureChance";
            case SkillSpecialFlags::Minus40ArmorDuringActivation: return "Minus40ArmorDuringActivation";
            case SkillSpecialFlags::Unknown1: return "Unknown1";
            case SkillSpecialFlags::Degen: return "Degen";
            case SkillSpecialFlags::SiegeAttack: return "SiegeAttack";
            case SkillSpecialFlags::Resurrection: return "Resurrection";
            case SkillSpecialFlags::OathShot: return "OathShot";
            case SkillSpecialFlags::Condition: return "Condition";
            case SkillSpecialFlags::MonsterSkill: return "MonsterSkill";
            case SkillSpecialFlags::DualAttack: return "DualAttack";
            case SkillSpecialFlags::IsStacking: return "IsStacking";
            case SkillSpecialFlags::IsNonStacking: return "IsNonStacking";
            case SkillSpecialFlags::ExploitsCorpse: return "ExploitsCorpse";
            case SkillSpecialFlags::PvESkill: return "PvESkill";
            case SkillSpecialFlags::Effect: return "Effect";
            case SkillSpecialFlags::NaturalResistance: return "NaturalResistance";
            case SkillSpecialFlags::PvP: return "PvP";
            case SkillSpecialFlags::FlashEnchantment: return "FlashEnchantment";
            case SkillSpecialFlags::SummoningSickness: return "SummoningSickness";
            case SkillSpecialFlags::IsUnplayable: return "IsUnplayable";
            
            default: return std::to_string((uint32_t)single_flag);
        }
        // clang-format on
    }

    std::optional<std::string_view> GetRangeStr(Range range)
    {
        // clang-format off
        switch (range)
        {
            case Range::AdjacentToTarget: return "Adjacent to Target";
            case Range::Adjacent:         return "Adjacent";
            case Range::Nearby:           return "Nearby";
            case Range::InTheArea:        return "In the Area";
            case Range::Earshot:          return "Earshot";
            case Range::SpiritRange:      return "Spirit Range";
            case Range::CompassRange:     return "Compass Range";

            default:   return std::nullopt;
        }
        // clang-format on
    }

    bool IsSpace(char c)
    {
        return c == ' ';
        // return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }

    bool IsAlpha(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    bool IsDigit(char c)
    {
        return (c >= '0' && c <= '9');
    }

    bool IsAlphaNum(char c)
    {
        return IsAlpha(c) || IsDigit(c);
    }

    // Returns the number of characters that are equal between the two strings.
    // The comparison is case-insensitive if a is uppercase OR b is lowercase.
    uint32_t StrCountEqual(std::string_view a, std::string_view b)
    {
        uint32_t max_len = std::min(a.size(), b.size());
        uint32_t i = 0;
        while (i < max_len && (a[i] == b[i] || std::tolower(a[i]) == b[i]))
        {
            i++;
        }
        return i;
    }

    bool StrFind(const char *&cursor, const char *end, std::string_view substr)
    {
        const auto substr_len = substr.size();
        if (cursor < end && substr_len == 0)
            return true;

        const auto search_end = end - substr_len + 1;
        while (cursor < search_end)
        {
            const auto test = std::string_view(cursor, substr_len);
            if (StrCountEqual(test, substr) == substr_len)
                return true;
            cursor++;
        }
        return false;
    }

    std::string_view PopWord(std::string_view &str)
    {
        char *p = (char *)str.data();
        char *end = p + str.size();
        while (p < end && Utils::IsSpace(*p))
            p++;

        char *word_start = p;
        while (p < end && !Utils::IsSpace(*p))
            p++;

        str = std::string_view(p, end - p);
        return std::string_view(word_start, p - word_start);
    }

    bool TryReadSpaces(char *&p, char *end)
    {
        if (p < end && Utils::IsSpace(*p))
        {
            do
            {
                ++p;
            } while (p < end && Utils::IsSpace(*p));
            return true;
        }
        return false;
    }
    bool TryRead(const char c, char *&p, char *end)
    {
        if (p < end && c == *p || std::tolower(c) == *p)
        {
            p++;
            return true;
        }
        return false;
    }
    bool TryRead(const std::string_view str, char *&p, char *end)
    {
        const auto len = str.size();
        if (StrCountEqual(str, std::string_view(p, end - p)) == len)
        {
            p += len;
            return true;
        }
        return false;
    }
    uint32_t TryReadPartial(const std::string_view str, char *&p, char *end)
    {
        auto eq_len = 0;
        if ((eq_len = StrCountEqual(str, std::string_view(p, end - p))) > 0)
        {
            p += eq_len;
            return eq_len;
        }
        return 0;
    }

    bool TryReadBefore(std::string_view str, char *start, char *&p)
    {
        auto p_init = p;
        const auto size = str.size();
        p -= size;
        while (p >= start)
        {
            if (Utils::TryRead(str, p, p_init))
            {
                return true;
            }
            p--;
        }
        p = p_init;
        return false;
    }

    bool TryReadAfter(std::string_view str, char *&p, char *end)
    {
        auto p_init = p;
        const auto size = str.size();
        auto eff_end = end - size + 1;
        while (p < eff_end)
        {
            if (Utils::TryRead(str, p, end))
            {
                return true;
            }
            p++;
        }
        p = p_init;
        return false;
    }

    std::string UInt32ToBinaryStr(uint32_t value)
    {
        std::bitset<32> bits(value);
        return bits.to_string();
    }

    std::wstring StrToWStr(const std::string_view str)
    {
        std::wstring wstr(str.begin(), str.end());
        return wstr;
    }

    std::string WStrToStr(const wchar_t *wstr, const wchar_t *end)
    {
        uint32_t len = end ? end - wstr : wcslen(wstr);
        // std::string str;
        // for (wchar_t wc : wstr)
        // {
        //     if (wc < 128)
        //         str.push_back(static_cast<char>(wc));
        //     else
        //         str.push_back('?'); // replace non-ASCII characters with '?'
        // }
        // return str;

        if (len == 0)
            return std::string();

        // Calculate the size needed for the destination string
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, len, NULL, 0, NULL, NULL);
        if (size_needed <= 0)
            return std::string();

        // Allocate the destination string
        std::string str(size_needed, 0);

        // Perform the conversion
        WideCharToMultiByte(CP_UTF8, 0, wstr, len, &str[0], size_needed, NULL, NULL);

        // // Replace single '%' with '%%' to escape them
        // std::string::size_type pos = 0;
        // while ((pos = str.find('%', pos)) != std::string::npos)
        // {
        //     str.insert(pos, "%");
        //     pos += 2; // Move past the inserted '%%'
        // }

        return str;
    }

    uint32_t last_message_frame_id = 0;
    DWORD last_message_timestamp = 0;

    void WriteDebugMessageRaw(const wchar_t *message, GW::Chat::Color color)
    {
        const auto channel = GW::Chat::Channel::CHANNEL_MODERATOR;

        if (last_message_frame_id != UpdateManager::frame_id)
        {
            auto delta_frames = UpdateManager::frame_id - last_message_frame_id;
            last_message_frame_id = UpdateManager::frame_id;
            auto delta_seconds = (float)(UpdateManager::elapsed_ms - last_message_timestamp) / 1000.f;
            last_message_timestamp = UpdateManager::elapsed_ms;

            GW::Chat::SetMessageColor(channel, 0xFF00FF00);

            const auto sep = std::format(L"------------ {} frames, {} seconds ------------", delta_frames, delta_seconds);
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_MODERATOR, sep.c_str());

            GW::Chat::Color c;
            GW::Chat::GetDefaultColors(channel, nullptr, &c);
            GW::Chat::SetMessageColor(channel, c);
        }

        if (color)
        {
            GW::Chat::SetMessageColor(channel, color);
        }

        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_MODERATOR, message);

        if (color)
        {
            GW::Chat::GetDefaultColors(channel, nullptr, &color);
            GW::Chat::SetMessageColor(channel, color);
        }
    }

    void WriteStringToChatV(GW::Chat::Color color, const wchar_t *format, va_list args)
    {
        const size_t bufferSize = 256; // Adjust size as needed
        wchar_t buffer[bufferSize];
        const auto last_index = bufferSize - 1;

        int result = vswprintf(buffer, last_index, format, args);

        if (result < 0)
        {
            WriteDebugMessageRaw((L"Error formatting string " + std::to_wstring(result) + L", format: " + format).c_str());
        }
        else if (result >= last_index)
        {
            WriteDebugMessageRaw(L"String too long for buffer");
        }
        else
        {
            buffer[last_index] = L'\0';
            // Now buffer contains the formatted string.
            // You can write it to the chat here.
            WriteDebugMessageRaw(buffer, color);
        }
    }
    void WriteStringToChat(GW::Chat::Color color, const wchar_t *format, ...)
    {
        va_list args;
        va_start(args, format);
        WriteStringToChatV(color, format, args);
        va_end(args);
    }
    void WriteStringToChat(const wchar_t *format, ...)
    {
        va_list args;
        va_start(args, format);
        WriteStringToChatV(NULL, format, args);
        va_end(args);
    }

    std::unordered_map<std::wstring, long> debug_message_last_date;
    void WriteDebugMessage(const wchar_t *message)
    {
        const auto period = 1000;
        const auto key = std::wstring(message);
        const auto last_date = debug_message_last_date[key];
        const auto now = GetTickCount();
        if (now - last_date >= period)
        {
            std::wstring time_message = L"(" + std::to_wstring(now) + L") " + message;
            WriteDebugMessageRaw(time_message.c_str());
            debug_message_last_date[key] = now;
        }
    }

    void WorkingDirectoryToChat()
    {
        // Get the current working directory
        auto p = std::filesystem::current_path();
        std::string path_str = p.string(); // Convert path to string

        // Convert the path to a wide string and display it
        std::wstring wpath_str = Utils::StrToWStr(path_str);
        HerosInsight::Utils::WriteStringToChat(L"Current working directory: '%s'", wpath_str.c_str());
    }

    // Divides nom by den, rounding to nearest. If two values are equally close, rounds to even.
    uint32_t DivHalfToEven(uint32_t nom, uint32_t den)
    {
        const auto div = nom / den;
        const auto rem = nom % den;
        const auto rem2 = 2 * rem;
        if (rem2 > den || (rem2 == den && div & 1))
        {
            return div + 1;
        }
        return div;
    }

    uint32_t RoundHalfToEven(double num)
    {
        double intpart;
        double fractpart = std::modf(num, &intpart);
        if (fractpart == 0.5)
        {
            if (static_cast<int>(intpart) % 2 == 0)
            {
                return (uint32_t)intpart;
            }
            else
            {
                return (uint32_t)intpart + 1;
            }
        }
        else
        {
            return (uint32_t)std::round(num);
        }
    }

    uint32_t RoundHalfToEven(float num)
    {
        float intpart;
        float fractpart = std::modf(num, &intpart);
        if (fractpart == 0.5f)
        {
            if (static_cast<int>(intpart) % 2 == 0)
            {
                return (uint32_t)intpart;
            }
            else
            {
                return (uint32_t)intpart + 1;
            }
        }
        else
        {
            return (uint32_t)std::round(num);
        }
    }

    GW::UI::ControlAction GetSkillControlAction(uint32_t hero_index, uint32_t slot_index)
    {
        assert(slot_index < 8);
        assert(hero_index < 8);

        uint32_t command_id;
        if (hero_index == 0)
        {
            command_id = (uint32_t)GW::UI::ControlAction_UseSkill1;
        }
        else if (hero_index < 4)
        {
            command_id = (uint32_t)GW::UI::ControlAction_Hero1Skill1 + (hero_index - 1) * 8;
        }
        else if (hero_index < 8)
        {
            command_id = (uint32_t)GW::UI::ControlAction_Hero4Skill1 + (hero_index - 4) * 8;
        }

        return (GW::UI::ControlAction)(command_id + slot_index);
    }

    bool HasPendingCast(GW::Skillbar *bar)
    {
        return bar->h00B4[1] != 8;
    }

    bool CanUseSkill(uint32_t hero_index, uint32_t slot_index)
    {
        const auto *skill_bar = GW::SkillbarMgr::GetHeroSkillbar(hero_index);
        if (skill_bar->casting)
        {
            return false;
        }

        const auto &skill_state = skill_bar->skills[slot_index];

        if (skill_state.recharge)
        {
            return false;
        }

        return true;
    }

    const static auto ControlAction_Suppress = (GW::UI::ControlAction)208;
    bool ToggleSkill(uint32_t hero_index, uint32_t slot_index)
    {
        const auto control_action = GetSkillControlAction(hero_index, slot_index);
        if (!GW::UI::Keydown(ControlAction_Suppress))
        {
            return false;
        }
        bool success = GW::UI::Keypress(control_action);
        GW::UI::Keyup(ControlAction_Suppress);
        return true;
    }

    bool EnableSkill(uint32_t hero_index, uint32_t slot_index)
    {
        const auto disabled = GW::SkillbarMgr::GetHeroSkillbar(hero_index)->disabled;
        if (!(disabled & (1 << slot_index)))
        {
            return true;
        }

        return ToggleSkill(hero_index, slot_index);
    }

    bool DisableSkill(uint32_t hero_index, uint32_t slot_index)
    {
        const auto disabled = GW::SkillbarMgr::GetHeroSkillbar(hero_index)->disabled;
        if (disabled & (1 << slot_index))
        {
            return true;
        }

        return ToggleSkill(hero_index, slot_index);
    }

    uint32_t LinearAttributeScale(uint32_t value0, uint32_t value15, uint32_t attribute_level)
    {
        assert(attribute_level <= 21);
        int32_t diff = (int32_t)value15 - (int32_t)value0;
        int32_t rounder = diff < 0 ? -15 : 15;
        int32_t result = (int32_t)value0 + (diff * (int32_t)attribute_level * 2 + rounder) / 30;

        // Only for Withdraw Hexes and Signet of Binding might the result be negative if attribute_level is 21.
        // It is unclear what the game does in that case. We assume it is clamped to 0.
        uint32_t uresult = (uint32_t)std::max(result, 0);
        return uresult;
    }

    uint8_t ReverseLinearAttributeScale(uint32_t value0, uint32_t value15, uint32_t value)
    {
        // Currently computes the highest attribute that could produce this value.
        int32_t diffX_0 = (int32_t)value - (int32_t)value0;
        int32_t diff15_0 = (int32_t)value15 - (int32_t)value0;
        int32_t rounder = diffX_0 < 0 ? -15 : 15;
        auto attribute_level = std::clamp((diffX_0 * 30 + rounder) / (2 * diff15_0), 0, 20);
        return attribute_level;
    }

    std::span<GW::Attribute> GetAgentAttributeSpan(uint32_t agent_id)
    {
        auto attributes = GW::PartyMgr::GetAgentAttributes(agent_id);
        if (!attributes)
            return {};

        return std::span(attributes, 52); // 54 doesn't seem to be the right size
    }

    std::pair<GW::Effect *, size_t> SortEffects(GW::EffectArray *effects, std::pair<GW::Effect *, size_t> stackalloc = std::pair<GW::Effect *, size_t>(nullptr, 0))
    {
        const auto n_effects = effects->size();
        GW::Effect *effectBuffer;
        std::unique_ptr<GW::Effect[]> heapBuffer;

        if (n_effects <= stackalloc.second)
        {
            std::copy(effects->begin(), effects->end(), stackalloc.first);
            effectBuffer = stackalloc.first;
        }
        else
        {
            heapBuffer = std::make_unique<GW::Effect[]>(n_effects);
            std::copy(effects->begin(), effects->end(), heapBuffer.get());
            effectBuffer = heapBuffer.get();
        }

        // Sort effects by timestamp
        // clang-format off
        std::sort(effectBuffer, effectBuffer + n_effects, [](const auto &a, const auto &b) { 
            if (a.skill_id == b.skill_id && a.attribute_level != b.attribute_level)
                return a.attribute_level > b.attribute_level;
            return a.timestamp < b.timestamp;
        });
        // clang-format on

        return std::make_pair(effectBuffer, n_effects);
    }

    uint8_t CalculateEnergyCost(uint32_t agent_id, GW::Constants::SkillID skill_id)
    {
        const auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);
        const auto &skill = *custom_sd.skill;

        const auto base_energy_cost = skill.GetEnergyCost();
        auto energy_cost = (float)base_energy_cost;

        const auto effects = GW::Effects::GetAgentEffects(agent_id);
        if (effects)
        {
            std::array<GW::Effect, 50> stackBuffer;
            auto result = SortEffects(effects, {stackBuffer.data(), stackBuffer.size()});
            const auto effectBuffer = result.first;
            const auto n_effects = result.second;

            bool has_energizing_wind = false;
            bool has_quickening_zephyr = false;
            uint32_t cultists_fervor_discount = 0;

            auto effects = EffectTracking::GetTrackerSpan(agent_id);
            for (auto &effect : effects)
            {
                // Agents may have multiple instances of the same effect, typically the one with greatest effect is the one that matters
                auto attr_lvl = effect.attribute_level;

                if (effect.skill_id == GW::Constants::SkillID::Cultists_Fervor &&
                    skill.profession == GW::Constants::ProfessionByte::Necromancer &&
                    custom_sd.tags.Spell)
                {
                    const auto effect_csd = GW::SkillbarMgr::GetSkillConstantData(effect.skill_id);
                    const auto energy_discount = LinearAttributeScale(effect_csd->scale0, effect_csd->scale15, attr_lvl);

                    cultists_fervor_discount = std::max(cultists_fervor_discount, energy_discount);
                }

                if (effect.skill_id == GW::Constants::SkillID::Quickening_Zephyr &&
                    has_quickening_zephyr == false)
                {
                    has_quickening_zephyr = true;
                    energy_cost *= 1.3f;
                }

                if (effect.skill_id == GW::Constants::SkillID::Energizing_Wind &&
                    has_energizing_wind == false)
                {
                    has_energizing_wind = true;
                    if (energy_cost > 10)
                    {
                        // energy_cost -= 15.f;
                        energy_cost = std::min(energy_cost, 10.f);
                    }
                    // break;
                }
            }

            // if (has_quickening_zephyr)
            // {
            //     int32_t cost_increase;
            //     // clang-format off
            //         switch (base_energy_cost) {
            //         case 0:
            //         case 1: cost_increase = 0; break;
            //         case 5: cost_increase = 1; break;
            //         case 10: cost_increase = 3; break;
            //         case 15: cost_increase = 5; break;
            //         case 25: cost_increase = 7; break;
            //         default:
            //             DebugDisplay::PushToDisplay("Unknown base energy cost", std::to_string(base_energy_cost));
            //             cost_increase = 0;
            //             break;
            //         }
            //     // clang-format on

            //     energy_cost = energy_cost + cost_increase;
            // }

            if (cultists_fervor_discount)
            {
                // energy_cost = energy_cost > cultists_fervor_discount ? energy_cost - cultists_fervor_discount : 0;
                energy_cost -= cultists_fervor_discount;
                energy_cost = std::max(energy_cost, 0.f);
            }

            // if (has_energizing_wind)
            // {
            //     // This is not how the skill is described, but it matches empirical testing
            //     if (energy_cost > 10)
            //     {
            //         energy_cost = 10;
            //     }
            // }
        }

        const auto attributes = GetAgentAttributeSpan(agent_id);
        if (!attributes.empty())
        {
            for (auto &attribute : attributes)
            {
                if (attribute.id == GW::Constants::Attribute::Mysticism &&
                    skill.type == GW::Constants::SkillType::Enchantment &&
                    skill.profession == GW::Constants::ProfessionByte::Dervish)
                {
                    const auto multiplier = (float)(100 - 4 * attribute.level) / 100.f;
                    energy_cost *= multiplier;
                }

                if (attribute.id == GW::Constants::Attribute::Expertise &&
                    (skill.type == GW::Constants::SkillType::Attack ||
                        skill.type == GW::Constants::SkillType::Ritual ||
                        skill.profession == GW::Constants::ProfessionByte::Ranger ||
                        skill.IsTouchRange()))
                {
                    const auto multiplier = (float)(100 - 4 * attribute.level) / 100.f;
                    energy_cost *= multiplier;
                }
            }
        }

        return (uint8_t)RoundHalfToEven(energy_cost);
    }

    std::span<uint16_t> GetAgentWeaponAndOffhandItemIds(GW::AgentLiving &agent)
    {
        return {&agent.weapon_item_id, 2};
    }

    GW::Item *GetAgentItem(GW::AgentLiving &agent, uint32_t item_index)
    {
        const auto weapon_id = (*(agent.equip))->item_ids[item_index];
        if (weapon_id == 0)
            return nullptr;

        return GW::Items::GetItemById(weapon_id);
    }

    std::span<GW::ItemModifier> GetItemModifiers(const GW::Item &item)
    {
        return {item.mod_struct, item.mod_struct_size};
    }

    GW::ItemModifier GetItemModifier(const GW::Item &item, ModifierID mod_id)
    {
        const auto modifiers = GetItemModifiers(item);
        for (auto mod : modifiers)
        {
            if (mod.identifier() == (uint32_t)mod_id)
            {
                return mod;
            }
        }

        return {};
    }

    uint32_t GetHCTChance(GW::AgentLiving &agent, GW::Constants::AttributeByte attribute)
    {
        FixedArray<uint32_t, 2> hct_sources_salloc;
        auto hct_sources = hct_sources_salloc.ref();

        for (const auto item_id : Utils::GetAgentWeaponAndOffhandItemIds(agent))
        {
            const auto item_ptr = GetAgentItem(agent, item_id);
            if (item_ptr)
            {
                const auto &item = *item_ptr;
                const auto mods = GetItemModifiers(item);
                for (const auto mod : mods)
                {
                    switch ((ModifierID)mod.identifier())
                    {
                        case ModifierID::HCT_Generic:
                            hct_sources.try_push(mod.arg1());
                            break;

                        case ModifierID::HCT_OfAttribute:
                            if (attribute == (GW::Constants::AttributeByte)mod.arg2())
                                hct_sources.try_push(mod.arg1());
                            break;

                        case ModifierID::HCT_OfItemsAttribute:
                            const auto req_mod = GetItemModifier(item, ModifierID::WeaponRequirement);
                            if (req_mod && attribute == (GW::Constants::AttributeByte)req_mod.arg1())
                                hct_sources.try_push(mod.arg1());
                            break;
                    }
                }
            }
        }

        switch (hct_sources.size())
        {
            case 0:
                return 0;
            case 1:
                return hct_sources[0];
            default:
                SOFT_ASSERT(false, L"Too many HCT sources");
                // FALLTHROUGH
            case 2:
                return hct_sources[0] + hct_sources[1] - (hct_sources[0] * hct_sources[1]) / 100; // 20 + 20 = 36
        }
    }

    uint32_t GetHSRChance(GW::AgentLiving &agent, GW::Constants::AttributeByte attribute)
    {
        uint32_t hsr_sources[2] = {0, 0};
        auto source_index = 0;

        auto push_to_buffer = [&](uint32_t value)
        {
            if (source_index < 2)
                hsr_sources[source_index++] = value;
            else
                WriteDebugMessageRaw(L"ERROR: Too many HSR sources");
        };

        for (const auto item_id : Utils::GetAgentWeaponAndOffhandItemIds(agent))
        {
            const auto item_ptr = GetAgentItem(agent, item_id);
            if (item_ptr)
            {
                const auto &item = *item_ptr;
                const auto mods = GetItemModifiers(item);
                for (const auto mod : mods)
                {
                    switch ((ModifierID)mod.identifier())
                    {
                        case ModifierID::HSR_Generic:
                            push_to_buffer(mod.arg1());
                            break;

                        case ModifierID::HSR_OfAttribute:
                            if (attribute == (GW::Constants::AttributeByte)mod.arg2())
                                push_to_buffer(mod.arg1());
                            break;

                        case ModifierID::HSR_OfItemsAttribute:

                            const auto req_mod = GetItemModifier(item, ModifierID::WeaponRequirement);
                            if (req_mod && attribute == (GW::Constants::AttributeByte)req_mod.arg1())
                                push_to_buffer(mod.arg1());
                            break;
                    }
                }
            }
        }

        const auto hsr = hsr_sources[0] + hsr_sources[1] - (hsr_sources[0] * hsr_sources[1]) / 100; // 20 + 20 = 36
        return hsr;
    }

    uint32_t CalculateDuration(GW::Skill &skill, uint32_t base_duration, uint32_t caster_id)
    {
        if (base_duration == 0)
            return 0;

        uint32_t duration = base_duration;

        auto caster_ptr = Utils::GetAgentLivingByID(caster_id);
        if (caster_ptr)
        {
            auto &caster = *caster_ptr;

            auto skill_id = skill.skill_id;

            for (auto item_id : Utils::GetAgentWeaponAndOffhandItemIds(caster))
            {
                auto item_ptr = GW::Items::GetItemById(item_id);
                if (!item_ptr)
                    continue;
                auto &item = *item_ptr;

                const auto mods = GetItemModifiers(item);
                for (const auto mod : mods)
                {
                    switch ((ModifierID)mod.identifier())
                    {
                        case ModifierID::DurationIncrease33:
                        {
                            auto condition_skill_id = (GW::Constants::SkillID)mod.arg();
                            bool is_matching_condition = skill_id == condition_skill_id;
                            if (is_matching_condition ||
                                skill_id == GW::Constants::SkillID::Bleeding) // A bug makes bleeding affected any +33% mod in a weird way
                            {
                                uint32_t min_duration = duration + 1;
                                if (is_matching_condition)
                                    duration = Utils::DivHalfToEven(duration * 133, 100);
                                duration = std::max(duration, min_duration);
                            }

                            break;
                        }

                        case ModifierID::ConditionDecrease20:
                        {
                            if (skill_id == GW::Constants::SkillID::Bleeding)
                            {
                                if (skill.type == GW::Constants::SkillType::Condition)
                                    duration -= 1; // Weird, but true
                            }

                            // auto condition_skill_id = GW::Constants::SkillID((uint32_t)GW::Constants::SkillID::Bleeding + mod.arg1());
                            // if (skill_id == condition_skill_id)
                            //     duration *= 0.8f;
                            break;
                        }

                        case ModifierID::OfEnchanting:
                        {
                            if (skill.type == GW::Constants::SkillType::Enchantment)
                            {
                                const auto percent_increase = mod.arg2();
                                duration = Utils::DivHalfToEven(duration * (100 + percent_increase), 100);
                            }
                            break;
                        }
                    }
                }
            }
        }

        auto caster_effects = EffectTracking::GetTrackerSpan(caster_id);
        for (auto &effect : caster_effects)
        {
            if (effect.skill_id == GW::Constants::SkillID::Archers_Signet &&
                caster_ptr)
            {
                auto weapon = GetAgentItem(*caster_ptr, 0);
                if (weapon->type == GW::Constants::ItemType::Bow)
                    duration *= 2;
            }
        }

        return duration;
    }

    std::optional<ChancyValue> CalculateRecharge(uint32_t agent_id, GW::Constants::SkillID skill_id)
    {
        const auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);
        const auto &skill = *custom_sd.skill;
        const auto &custom_ad = CustomAgentDataModule::GetCustomAgentData(agent_id);
        const auto agent_ptr = Utils::GetAgentLivingByID(agent_id);
        if (agent_ptr == nullptr)
            return std::nullopt;
        auto &agent = *agent_ptr;

        auto base_recharge = (float)skill.recharge;

        if (skill.profession == GW::Constants::ProfessionByte::Mesmer && custom_sd.tags.Spell)
        {
            const auto fast_casting = custom_ad.GetOrEstimateAttribute(AttributeOrTitle(GW::Constants::AttributeByte::FastCasting));
            if (fast_casting)
            {
                const auto multiplier = (float)(100 - 3 * fast_casting) / 100.f;
                base_recharge *= multiplier;
            }
        }

        auto recharge = base_recharge;

        auto multiplier = 1.f;
        auto lowest_multiplier = 1.f;
        auto add_multiplier = [&](float value)
        {
            multiplier *= value;
            lowest_multiplier = std::min(lowest_multiplier, value);
        };

        const auto effects = GW::Effects::GetAgentEffects(agent_id);
        if (effects)
        {
            std::array<GW::Effect, 50> stackBuffer;
            auto result = SortEffects(effects, {stackBuffer.data(), stackBuffer.size()});
            const auto effectBuffer = result.first;
            const auto n_effects = result.second;

            bool has_quickening_zephyr = false;
            bool has_energizing_wind = false;
            for (size_t i = 0; i < n_effects; i++) // May contain duplicates
            {
                auto &effect = effectBuffer[i];

                if (effect.skill_id == GW::Constants::SkillID::Energizing_Wind &&
                    has_energizing_wind == false)
                {
                    has_energizing_wind = true;
                    multiplier *= 1.25f;
                }

                if (effect.skill_id == GW::Constants::SkillID::Quickening_Zephyr &&
                    has_quickening_zephyr == false)
                {
                    has_quickening_zephyr = true;
                    multiplier *= 0.5f;
                    break; // Weird but true: If QZ was applied before EW, the 1.25 multiplier is skipped.
                }
            }
        }

        multiplier = std::max(multiplier, 0.5f);
        recharge *= multiplier;

        auto hsr_chance = GetHSRChance(agent, skill.attribute);
        float hsr_recharge;
        if (hsr_chance)
            hsr_recharge = 0.5f * base_recharge;
        else
            hsr_recharge = recharge;

        const auto chance = (float)hsr_chance / 100.0f;
        const auto avg_recharge = recharge * (1.f - chance) + hsr_recharge * chance;

        return ChancyValue{
            recharge,
            hsr_recharge,
            hsr_chance,
            avg_recharge,
            RoundHalfToEven(recharge),
            RoundHalfToEven(hsr_recharge),
        };
    }

    std::optional<ChancyValue> CalculateCasttime(uint32_t agent_id, GW::Constants::SkillID skill_id)
    {
        const auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);
        const auto &skill = *custom_sd.skill;
        const auto &custom_ad = CustomAgentDataModule::GetCustomAgentData(agent_id);
        const auto agent_ptr = Utils::GetAgentLivingByID(agent_id);
        if (agent_ptr == nullptr)
            return std::nullopt;
        auto &agent = *agent_ptr;

        auto base_casttime = skill.activation;

        if ((base_casttime >= 2.f || skill.profession == GW::Constants::ProfessionByte::Mesmer) &&
            (custom_sd.tags.Spell || skill.type == GW::Constants::SkillType::Signet))
        {
            const auto fast_casting = custom_ad.GetOrEstimateAttribute(AttributeOrTitle(GW::Constants::AttributeByte::FastCasting));
            if (fast_casting)
            {
                const auto multiplier = std::pow(0.5f, (float)fast_casting / 15.f);
                base_casttime *= multiplier;
            }
        }

        auto casttime = base_casttime;

        const auto effects = GW::Effects::GetAgentEffects(agent_id);
        if (effects)
        {
            // std::array<GW::Effect, 50> stackBuffer;
            // auto result = SortEffectsByTimestamp(effects, {stackBuffer.data(), stackBuffer.size()});
            // const auto effectBuffer = result.first;
            // const auto n_effects = result.second;

            for (const auto &effect : *effects)
            {
                if (effect.skill_id == GW::Constants::SkillID::Signet_of_Mystic_Speed &&
                    skill.type == GW::Constants::SkillType::Enchantment &&
                    skill.target == 0)
                {
                    casttime = 0.f;
                    break;
                }
            }
        }

        // auto multiplier = 1.f;
        // auto lowest_multiplier = 1.f;
        // auto add_multiplier = [&](float value)
        // {
        //     multiplier *= value;
        //     lowest_multiplier = std::min(lowest_multiplier, value);
        // };

        // auto hct_casttime = casttime;

        // if (lowest_multiplier <= 0.5f)
        //     casttime *= lowest_multiplier;
        // else
        //     casttime *= multiplier;

        auto hct_chance = GetHCTChance(agent, skill.attribute);
        float hct_casttime;
        if (hct_chance)
            hct_casttime = std::min(casttime, 0.5f * base_casttime);
        else
            hct_casttime = casttime;

        const auto chance = (float)hct_chance / 100.0f;
        const auto avg_casttime = casttime * (1.f - chance) + hct_casttime * chance;

        return ChancyValue{
            casttime,
            hct_casttime,
            hct_chance,
            avg_casttime,
            RoundHalfToEven(casttime),
            RoundHalfToEven(hct_casttime),
        };
    }

    Maintainability CalculateMaintainability(GW::AgentLiving *agent, GW::Constants::SkillID skill_id)
    {
        auto agent_id = agent->agent_id;
        auto &custom_ad = CustomAgentDataModule::GetCustomAgentData(agent_id);
        auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);
        auto base_duration = custom_sd.ResolveBaseDuration(custom_ad);
        const auto duration = (float)CalculateDuration(*custom_sd.skill, base_duration, agent_id);
        if (duration == 0)
            return {};

        const auto recharge = CalculateRecharge(agent_id, skill_id).value_or(ChancyValue{});
        const auto casttime = CalculateCasttime(agent_id, skill_id).value_or(ChancyValue{});
        const auto energy_cost = CalculateEnergyCost(agent_id, skill_id);

        const auto normal_cycle_time = (float)recharge.rounded + casttime.value;
        const auto maintainability = duration / normal_cycle_time;
        const auto avg_cycle_time = recharge.avg + casttime.avg;
        const auto avg_maintainability = duration / avg_cycle_time;
        const auto avg_energy_per_sec = energy_cost / avg_cycle_time;

        return Maintainability{
            maintainability,
            avg_maintainability,
            avg_energy_per_sec,
            true,
        };
    }

    std::optional<EffectHPPerSec> CalculateEffectHPPerSec(GW::Constants::SkillID skill_id, uint32_t attribute_level)
    {
        auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);

        auto csd = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        if (csd == nullptr)
            return std::nullopt;

        FixedArray<ParsedSkillData, 4> reg_salloc, deg_salloc;
        auto regs = reg_salloc.ref();
        auto degs = deg_salloc.ref();

        int32_t regen_hp_per_second = 0;
        int32_t degen_hp_per_second = 0;
        custom_sd.GetParsedSkillParams(ParsedSkillData::Type::HealthRegen, regs);
        custom_sd.GetParsedSkillParams(ParsedSkillData::Type::HealthDegen, degs);
        for (auto &reg : regs)
        {
            regen_hp_per_second += 2 * reg.param.Resolve(attribute_level);
        }
        for (auto &deg : degs)
        {
            degen_hp_per_second -= 2 * deg.param.Resolve(attribute_level);
        }

        if (!regen_hp_per_second && !degen_hp_per_second)
            return std::nullopt;

        EffectHPPerSec result = {};
        if (regen_hp_per_second && !degen_hp_per_second) // Only regen
        {
            result.target_hp_sec = (float)regen_hp_per_second;
        }
        else if (!regen_hp_per_second && degen_hp_per_second) // Only degen
        {
            if (skill_id == GW::Constants::SkillID::Malaise)
            {
                result.caster_hp_sec = (float)degen_hp_per_second;
            }
            else
            {
                result.target_hp_sec = (float)degen_hp_per_second;
            }
        }
        else if (regen_hp_per_second && degen_hp_per_second) // Both regen and degen
        {
            result.caster_hp_sec = (float)regen_hp_per_second;
            result.target_hp_sec = (float)degen_hp_per_second;
        }
        return result;
    }

    bool TryGetDecodedString(std::wstring_view enc_str, std::wstring_view &out)
    {
        static std::mutex bookkeeping_mutex;
        static std::vector<std::wstring> enc_keys;
        static std::unordered_map<std::wstring_view, std::optional<std::wstring>> enc_to_dec;

        size_t index;
        {
            std::lock_guard<std::mutex> lock(bookkeeping_mutex);
            auto it = enc_to_dec.find(enc_str);
            if (it != enc_to_dec.end())
            {
                auto opt = it->second;
                if (opt.has_value())
                {
                    out = opt.value();
                    return true;
                }
                else
                {
                    return false;
                }
            }

            index = enc_keys.size();
            enc_keys.push_back(std::wstring(enc_str));
            enc_to_dec[enc_str] = std::nullopt;
        }

        auto DecodeCallback = [](void *param, const wchar_t *s)
        {
            auto index = reinterpret_cast<size_t>(param);
            std::lock_guard<std::mutex> lock(bookkeeping_mutex);
            enc_to_dec[enc_keys[index]] = s;
        };
        GW::UI::AsyncDecodeStr(enc_str.data(), DecodeCallback, reinterpret_cast<void *>(index));

        std::lock_guard<std::mutex> lock(bookkeeping_mutex);
        auto &opt = enc_to_dec[enc_str];
        if (opt.has_value())
        {
            out = opt.value();
            return true;
        }

        return false;
    }

    std::wstring DecodeString(const wchar_t *enc_str, std::chrono::microseconds timeout)
    {
        // We need this mutex to guard against GW::UI::AsyncDecodeStr setting the
        // stack allocated promise after it has been destroyed (we returned early)
        static std::mutex decode_mutex;
        static size_t n_timeouts = 0;
        static std::promise<std::wstring> promise;

        std::future<std::wstring> future;
        {
            std::lock_guard<std::mutex> lock(decode_mutex);
            promise = std::promise<std::wstring>();
            future = promise.get_future();
        }

        auto DecodeCallback = [](void *param, const wchar_t *s)
        {
            auto n_timeouts_when_started = reinterpret_cast<size_t>(param);
            std::lock_guard<std::mutex> lock(decode_mutex);
            if (n_timeouts_when_started == n_timeouts)
            {
                promise.set_value(s);
            }
        };
        GW::UI::AsyncDecodeStr(enc_str, DecodeCallback, reinterpret_cast<void *>(n_timeouts));

        if (future.wait_for(timeout) != std::future_status::ready)
        {
            std::lock_guard<std::mutex> lock(decode_mutex);
            n_timeouts++;
            return L"DECODE_TIMEOUT";
        }

        return future.get();
    }

    std::wstring StrIDToEncStr(uint32_t id)
    {
        const size_t bufferSize = 256; // Adjust the buffer size as needed
        wchar_t buffer[bufferSize];
        if (!GW::UI::UInt32ToEncStr(id, buffer, bufferSize - 1))
        {
            return L"FAILED_TO_CONVERT";
        }

        buffer[bufferSize - 1] = L'\0';

        return std::wstring(buffer);
    }

    std::wstring StrIDToWStr(uint32_t id)
    {
        const size_t bufferSize = 256; // Adjust the buffer size as needed
        wchar_t buffer[bufferSize];
        if (!GW::UI::UInt32ToEncStr(id, buffer, bufferSize - 1))
        {
            return L"FAILED_TO_CONVERT";
        }

        buffer[bufferSize - 1] = L'\0';

        return DecodeString(buffer);
    }

    std::wstring GetAgentName(uint32_t agent_id)
    {
        const auto agent_name = GW::Agents::GetAgentEncName(agent_id);
        if (agent_name == nullptr)
            return L"UNKNOWN_AGENT";

        return DecodeString(agent_name);
    }

    std::wstring GetSkillName(GW::Constants::SkillID skill_id)
    {
        if (skill_id == GW::Constants::SkillID::No_Skill)
            return L"NO_SKILL";

        const auto csd = GW::SkillbarMgr::GetSkillConstantData(skill_id);
        if (csd == nullptr)
            return L"UNKNOWN_SKILL";

        return StrIDToWStr(csd->name);
    }

    std::wstring EffectToString(const GW::Effect *effect)
    {
        if (effect == nullptr)
            return L"NULL_EFFECT";

        const auto effect_name = GetSkillName(effect->skill_id);
        return std::format(L"Effect '{}' with effect_id '{}'", effect_name, effect->effect_id);
    }

    std::wstring BuffToString(const GW::Buff *buff)
    {
        if (buff == nullptr)
            return L"NULL_BUFF";

        const auto buff_name = GetSkillName(buff->skill_id);
        return std::format(L"Buff '{}' with buff_id '{}' and h0004 '{}'", buff_name, buff->buff_id, buff->h0004);
    }

    std::string StrIDToStr(uint32_t id)
    {
        return WStrToStr(StrIDToWStr(id).c_str());
    }

    uint32_t GetHeroIndex(uint32_t agent_id)
    {
        const auto info = GW::PartyMgr::GetPartyInfo();
        if (info == nullptr)
            return 0;

        for (auto &hero : info->heroes)
        {
            if (hero.agent_id == agent_id)
                return hero.hero_id;
        }

        return 0;
    }

    bool UseSkill(uint32_t hero_index, GW::Constants::SkillID skill_id, uint32_t target)
    {
        const auto bar = GW::SkillbarMgr::GetHeroSkillbar(hero_index);
        if (bar == nullptr)
            return false;

        size_t slot;
        const auto skill = bar->GetSkillById(skill_id, &slot);
        if (skill == nullptr)
            return false;

        return UseSkill(hero_index, slot, target);
    }

    bool UseSkill(uint32_t hero_index, uint32_t slot_index, uint32_t target)
    {
        auto hero_data = PartyDataModule::GetHeroState(hero_index);
        if (hero_data == nullptr ||
            hero_data->n_used_skills_this_frame >= 1)
            return false;

        const auto bar = GW::SkillbarMgr::GetHeroSkillbar(hero_index);
        if (bar == nullptr ||
            HasPendingCast(bar) ||
            bar->casting)
            return false;

        const auto bar_skill = bar->skills[slot_index];
        if (bar_skill.recharge > 0)
            return false;

        const auto csd = GW::SkillbarMgr::GetSkillConstantData(bar_skill.skill_id);
        if (csd == nullptr)
            return false;

        const auto agent_id = GW::Agents::GetHeroAgentID(hero_index);
        if (agent_id == 0)
            return false;

        const auto living_agent = Utils::GetAgentLivingByID(agent_id);
        if (living_agent == nullptr)
            return false;

        const auto current_energy = living_agent->energy * living_agent->max_energy;
        const auto energy_cost = CalculateEnergyCost(agent_id, bar_skill.skill_id);
        if (current_energy < energy_cost)
            return false;

        bool skill_requires_target = csd->target;
        bool changed_target = false;
        uint32_t current_target;
        if (skill_requires_target && target)
        {
            current_target = GW::Agents::GetTargetId();
            const bool needs_target_change = target != current_target;
            if (needs_target_change)
            {
                if (!GW::Agents::ChangeTarget(target))
                    return false;

                changed_target = true;
            }
        }

        const auto control_action = Utils::GetSkillControlAction(hero_index, slot_index);
        const auto success = GW::UI::Keypress(control_action);
        if (success)
        {
            hero_data->n_used_skills_this_frame++;
        }

        if (changed_target)
        {
            GW::Agents::ChangeTarget(current_target);
        }

        return success;
    }

    D3DVIEWPORT9 GetViewport()
    {
        D3DVIEWPORT9 viewport;
        viewport.X = 0;
        viewport.Y = 0;
        viewport.Width = GW::Render::GetViewportWidth();
        viewport.Height = GW::Render::GetViewportHeight();
        viewport.MinZ = 0.0f;
        viewport.MaxZ = 1.0f;

        return viewport;
    }

    D3DXMATRIX GetViewMatrix()
    {
        const auto camera = GW::CameraMgr::GetCamera();
        if (!camera)
            return {};
        // For some reason, up is down. *Pirates of the Caribbean track plays*
        D3DXVECTOR3 eye(camera->position.x, camera->position.y, -camera->position.z);
        D3DXVECTOR3 at(camera->look_at_target.x, camera->look_at_target.y, -camera->look_at_target.z);
        D3DXVECTOR3 up(0, 0, 1);
        D3DXMATRIX view;
        D3DXMatrixLookAtRH(&view, &eye, &at, &up);
        return view;
    }

    D3DXMATRIX GetProjectionMatrix()
    {
        float fov = GW::Render::GetFieldOfView();
        float aspectRatio = static_cast<float>(GW::Render::GetViewportWidth()) /
                            static_cast<float>(GW::Render::GetViewportHeight());
        float nearPlane = 48000.0f / 1024; // ty tedy
        float farPlane = 48000.0f;
        D3DXMATRIX projection;
        D3DXMatrixPerspectiveFovRH(&projection, fov, aspectRatio, nearPlane, farPlane);
        return projection;
    }

    ImVec2 WorldSpaceToScreenSpace(GW::Vec3f world_pos)
    {
        const auto view = GetViewMatrix();
        const auto projection = GetProjectionMatrix();
        const auto viewport = GetViewport();

        // Project the world position to screen space
        D3DXVECTOR3 screen_pos;
        D3DXVECTOR3 world_pos_dx(world_pos.x, world_pos.y, world_pos.z);
        D3DXVec3Project(&screen_pos, &world_pos_dx, &viewport, &projection, &view, nullptr);

        return ImVec2(screen_pos.x, screen_pos.y);
    }

    IDirect3DStateBlock9 *PrepareWorldSpaceRendering(IDirect3DDevice9 *device)
    {
        // Save current state
        IDirect3DStateBlock9 *changes = nullptr;
        device->CreateStateBlock(D3DSBT_ALL, &changes);

        // Disable lighting (so it doesn't interfere with color)
        device->SetRenderState(D3DRS_LIGHTING, FALSE);

        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);     // Enable depth testing
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);      // Enable depth writing (maybe not needed)
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL); // Standard depth comparison

        // Set up the view transformation (camera position and target)
        D3DXMATRIX viewMatrix = Utils::GetViewMatrix();
        device->SetTransform(D3DTS_VIEW, &viewMatrix);

        // Set the projection transformation (field of view, near/far planes)
        D3DXMATRIX projMatrix = Utils::GetProjectionMatrix();
        device->SetTransform(D3DTS_PROJECTION, &projMatrix);

        // Set the world transformation to identity
        D3DXMATRIX identity = D3DXMATRIX(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
        device->SetTransform(D3DTS_WORLD, &identity);

        return changes;
    }

    template <typename T>
    struct Buffer
    {
        T *ptr;
        size_t len;
        size_t cap;

        bool Push(T element)
        {
            if (len == cap)
                return false;

            ptr[len++] = element;
            return true;
        }
    };

    float DistanceSqrd(GW::Vec3f a, GW::Vec3f b)
    {
        const auto dx = a.x - b.x;
        const auto dy = a.y - b.y;
        const auto dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    float Dot(GW::Vec2f a, GW::Vec2f b)
    {
        return a.x * b.x + a.y * b.y;
    }

    float Dot(GW::Vec3f a, GW::Vec3f b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    float Cross(GW::Vec2f a, GW::Vec2f b)
    {
        return a.x * b.y - a.y * b.x;
    }

    GW::Vec2f Cross(GW::Vec2f a, float b)
    {
        return {a.y * b, -a.x * b};
    }

    GW::Vec2f Cross(float a, GW::Vec2f b)
    {
        return {-a * b.y, a * b.x};
    }

    GW::Vec2f Cross(GW::Vec3f a, GW::Vec3f b)
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z};
    }

    bool TryNormalize(GW::Vec2f &a)
    {
        const auto length_sqrd = Dot(a, a);
        if (length_sqrd == 0)
            return false;

        const auto length = std::sqrt(length_sqrd);
        a /= length;
        return true;
    }

    bool TryNormalize(GW::Vec3f &a)
    {
        const auto length_sqrd = Dot(a, a);
        if (length_sqrd == 0)
            return false;

        const auto length = std::sqrt(length_sqrd);
        a /= length;
        return true;
    }

    float CircleCast(GW::Vec2f start, GW::Vec2f direction, float radius, Buffer<GW::Vec2f> positions, float max = std::numeric_limits<float>::max())
    {
        const auto perp = Cross(1.f, direction);
        const auto radius_sqrd = radius * radius;

        float closest_hit_distance = max;
        for (size_t i = 0; i < positions.len; i++)
        {
            const auto position = positions.ptr[i];
            const auto position_local = position - start;

            if (Dot(position_local, position_local) <= radius_sqrd)
                return 0.f;

            const auto position_along = Dot(position_local, direction);

            if (position_along < 0)
                continue;

            if (position_along - radius > closest_hit_distance) // Not needed for correctness, but can be used to optimize
                continue;

            const auto position_perp = Dot(position_local, perp);
            const auto distance_perp = std::abs(position_perp);

            if (distance_perp > radius)
                continue;

            const auto perp_distance_sqrd = distance_perp * distance_perp;

            const auto x = std::sqrt(radius_sqrd - perp_distance_sqrd); // Distance along direction from the hit position to the position we hit
            const auto hit_distance = position_along - x;

            if (hit_distance < closest_hit_distance)
                closest_hit_distance = hit_distance;
        }

        return closest_hit_distance;
    }

    bool IsFoe(GW::AgentLiving *agent)
    {
        return agent->allegiance == GW::Constants::Allegiance::Enemy;
    }

    void CollectEnemyPositions(Buffer<GW::Vec2f> *positions)
    {
        const auto *aa = GW::Agents::GetAgentArray();
        if (aa == nullptr)
            return;

        for (auto agent : *aa)
        {
            if (agent == nullptr)
                continue;
            const auto living_agent = agent->GetAsAgentLiving();
            if (living_agent == nullptr)
                continue;

            if (IsFoe(living_agent))
            {
                if (!positions->Push({living_agent->pos.x, living_agent->pos.y}))
                    break;
            }
        }
    }

    bool CircleOverlap(GW::Vec2f center, float radius, Buffer<GW::Vec2f> positions)
    {
        const auto radius_sqrd = radius * radius;
        for (size_t i = 0; i < positions.len; i++)
        {
            const auto position = positions.ptr[i];
            const auto position_local = position - center;
            const auto distance_sqrd = Dot(position_local, position_local);
            if (distance_sqrd <= radius_sqrd)
                return true;
        }
        return false;
    }

    // Estimated time until this hero will be in aggro range of an enemy
    float SecondsTilContact(uint32_t hero_index)
    {
        const auto hero_id = GW::Agents::GetHeroAgentID(hero_index);
        if (hero_id == 0)
            return 0.f;
        const auto hero_agent = GW::Agents::GetAgentByID(hero_id);
        if (hero_agent == nullptr)
            return 0.f;

        GW::Vec2f positions[64];
        Buffer<GW::Vec2f> positions_buffer = {positions, 0, 64};
        CollectEnemyPositions(&positions_buffer);

        if (positions_buffer.len > 0)
        {
            const auto aggro_range = GW::Constants::Range::Earshot;

            const GW::Vec2f hero_position = {hero_agent->pos.x, hero_agent->pos.y};
            const auto hero_vel = hero_agent->velocity;
            const auto hero_speed_sqrd = Dot(hero_vel, hero_vel);

            if (hero_speed_sqrd > 0.f)
            {
                const auto hero_speed = std::sqrt(hero_speed_sqrd);
                auto direction = hero_vel / hero_speed;
                const auto closest_hit_distance = CircleCast(hero_position, direction, aggro_range, positions_buffer, 5000 - aggro_range);
                const auto seconds = closest_hit_distance / hero_speed;
                return seconds;
            }
            else
            {
                if (CircleOverlap(hero_position, aggro_range, positions_buffer))
                    return 0.f;
            }
        }

        return 60.f; // 1 minute
    }

    float DistanceToPlayer(uint32_t agent_id)
    {
        const auto agent = GW::Agents::GetAgentByID(agent_id);
        if (agent == nullptr)
            return -1;

        const auto player = GW::Agents::GetControlledCharacter();
        if (player == nullptr)
            return -1;

        const auto position_local = agent->pos - player->pos;
        const auto distance_sqrd = Dot(position_local, position_local);
        const auto distance = std::sqrt(distance_sqrd);

        return distance;
    }

    uint32_t GetClosestAgentID(GW::Vec2f pos)
    {
        const auto agents = GW::Agents::GetAgentArray();
        if (agents == nullptr)
            return 0;

        uint32_t closest_agent_id = 0;
        float closest_distance_sqrd = std::numeric_limits<float>::max();
        for (auto agent : *agents)
        {
            if (agent == nullptr)
                continue;

            const auto agent_pos = GW::Vec2f(agent->pos.x, agent->pos.y);
            const auto position_local = agent_pos - pos;
            const auto distance_sqrd = Dot(position_local, position_local);
            if (distance_sqrd < closest_distance_sqrd)
            {
                closest_agent_id = agent->agent_id;
                closest_distance_sqrd = distance_sqrd;
            }
        }

        return closest_agent_id;
    }

    void ForEnemiesInCircle(GW::Vec2f center, float radius, GW::Constants::Allegiance allegiance, std::function<void(GW::AgentLiving &)> op)
    {
        auto Op = [&](GW::AgentLiving &other)
        {
            if (Utils::GetAgentRelations(allegiance, other.allegiance) == Utils::AgentRelations::Hostile)
                op(other);
        };
        ForAgentsInCircle(center, radius, Op);
    }

    void ForAlliesInCircle(GW::Vec2f center, float radius, GW::Constants::Allegiance allegiance, std::function<void(GW::AgentLiving &)> op)
    {
        auto Op = [&](GW::AgentLiving &other)
        {
            if (Utils::GetAgentRelations(allegiance, other.allegiance) == Utils::AgentRelations::Friendly)
                op(other);
        };
        ForAgentsInCircle(center, radius, Op);
    }

    void ForPartyMembersInCircle(GW::Vec2f center, float radius, uint32_t agent_id, std::function<void(GW::AgentLiving &)> op)
    {
        auto Op = [&](GW::AgentLiving &other)
        {
            if (Utils::IsPartyMember(agent_id))
                op(other);
        };
        ForAgentsInCircle(center, radius, Op);
    }

    void ForAgentsInCircle(GW::Vec2f center, float radius, std::function<void(GW::AgentLiving &)> op)
    {
        std::vector<uint32_t> in_circle;
        const auto agents = GW::Agents::GetAgentArray();
        const auto radius_sqrd = radius * radius;
        for (auto agent : *agents)
        {
            if (agent == nullptr)
                continue;

            auto living_agent = agent->GetAsAgentLiving();
            if (living_agent == nullptr)
                continue;

            GW::Vec2f agent_pos = agent->pos;
            const auto position_local = agent_pos - center;
            const auto distance_sqrd = Dot(position_local, position_local);
            if (distance_sqrd > radius_sqrd)
                continue;

            op(*living_agent);
        }
    }

    GW::AgentLiving *GetHeroAsAgentLiving(uint32_t hero_index)
    {
        const auto hero_id = GW::Agents::GetHeroAgentID(hero_index);
        if (hero_id == 0)
            return nullptr;

        const auto hero_agent = GW::Agents::GetAgentByID(hero_id);
        if (hero_agent == nullptr)
            return nullptr;

        const auto living_agent = hero_agent->GetAsAgentLiving();
        return living_agent;
    }

    float SecondsTilFullEnergy(GW::AgentLiving *agent)
    {
        const auto missing_energy = 1 - agent->energy;
        const auto seconds = missing_energy / agent->energy_regen;
        return seconds;
    }

    float SecondsTilEnergyRecovered(GW::AgentLiving *agent, uint32_t energy)
    {
        const auto energy_percentage = (float)energy / (float)agent->max_energy;
        const auto seconds = energy_percentage / agent->energy_regen;
        return seconds;
    }

    GW::AgentInfoArray *GetAgentInfoArray()
    {
        auto *w = GW::GetWorldContext();
        return w ? &w->agent_infos : nullptr;
    }

    GW::AgentInfo *GetAgentInfoByID(uint32_t agent_id)
    {
        auto *agent_info_array = agent_id ? GetAgentInfoArray() : nullptr;
        return agent_info_array && agent_id < agent_info_array->size() ? &agent_info_array->at(agent_id) : nullptr;
    }

    GW::AgentMovementArray *GetAgentMovementArray()
    {
        auto *w = GW::GetAgentContext();
        return w ? &w->agent_movement : nullptr;
    }

    GW::AgentMovement *GetAgentMovementByID(uint32_t agent_id)
    {
        auto *movement_array = agent_id ? GetAgentMovementArray() : nullptr;
        return movement_array && agent_id < movement_array->size() ? movement_array->at(agent_id) : nullptr;
    }

    GW::Array<GW::AgentSummaryInfo> *GetAgentSummaryInfoArray()
    {
        auto *w = GW::GetAgentContext();
        return w ? &w->agent_summary_info : nullptr;
    }

    GW::AgentSummaryInfo *GetAgentSummaryInfoByID(uint32_t agent_id)
    {
        auto *summary_info_array = agent_id ? GetAgentSummaryInfoArray() : nullptr;
        return summary_info_array && agent_id < summary_info_array->size() ? &summary_info_array->at(agent_id) : nullptr;
    }

    std::wstring GenericValueIDToString(uint32_t id)
    {
        // clang-format off
        switch (id) {
            case 1:  return L"melee_attack_finished";
            case 3:  return L"attack_stopped";
            case 4:  return L"attack_started";
            case 6:  return L"add_effect";
            case 7:  return L"remove_effect";
            case 8:  return L"disabled";
            case 10: return L"skill_damage";
            case 11: return L"apply_marker";
            case 12: return L"remove_marker";
            case 16: return L"damage";
            case 17: return L"critical";
            case 20: return L"effect_on_target";
            case 21: return L"effect_on_agent";
            case 22: return L"animation";
            case 23: return L"animation_special";
            case 28: return L"animation_loop";
            case 29: return L"boss_glow?";
            case 32: return L"max_hp_reached";
            case 34: return L"health";
            case 35: return L"interrupted";
            case 36: return L"level_update";
            case 38: return L"missed";
            case 42: return L"max_hp_update";
            case 43: return L"change_energy_regen";
            case 44: return L"change_health_regen";
            case 46: return L"attack_skill_finished";
            case 48: return L"instant_skill_activated";
            case 49: return L"attack_skill_stopped";
            case 50: return L"attack_skill_activated";
            case 52: return L"energygain";
            case 55: return L"armorignoring";
            case 58: return L"skill_finished";
            case 59: return L"skill_stopped";
            case 60: return L"skill_activated";
            case 61: return L"casttime";
            case 62: return L"energy_spent";
            case 63: return L"knocked_down";
            default: return std::to_wstring(id);
        }
        // clang-format on
    }

    // Returns size of the bounding box
    ImVec2 CalculateTextBoundingBox(ImFont *font, const char *text, ImVec2 &out_min, ImVec2 &out_max)
    {
        float x = 0.0f; // Current x position for the next character
        bool first_glyph = true;

        for (const char *p = text; *p; p++)
        {
            if (*p == '\n')
            {
                // Handle new lines if necessary
                continue;
            }

            const ImFontGlyph *glyph = font->FindGlyph(*p);
            if (!glyph)
            {
                continue; // Ignore missing glyphs
            }

            // Calculate the bounding box of the glyph in screen space
            auto glyph_min = ImVec2(x + glyph->X0, glyph->Y0);
            auto glyph_max = ImVec2(x + glyph->X1, glyph->Y1);

            if (first_glyph)
            {
                out_min = glyph_min;
                out_max = glyph_max;
                first_glyph = false;
            }
            else
            {
                if (glyph_min.x < out_min.x)
                    out_min.x = glyph_min.x;
                if (glyph_min.y < out_min.y)
                    out_min.y = glyph_min.y;
                if (glyph_max.x > out_max.x)
                    out_max.x = glyph_max.x;
                if (glyph_max.y > out_max.y)
                    out_max.y = glyph_max.y;
            }

            x += glyph->AdvanceX; // Move to the next character position
        }

        return out_max - out_min;
    }

    std::string SkillConstantDataToString(GW::Skill &skill)
    {
        std::string result;

        result += "\n";
        result += std::format("\tskill_id: {}\n", static_cast<uint32_t>(skill.skill_id));
        result += std::format("\th0004: {}\n", skill.h0004);
        result += std::format("\tcampaign: {}\n", static_cast<uint32_t>(skill.campaign));
        result += std::format("\ttype: {}\n", static_cast<uint32_t>(skill.type));
        result += std::format("\tspecial: {}\n", Utils::UInt32ToBinaryStr(skill.special));
        for (uint32_t i = 0; i < 32; i++)
        {
            auto flag = skill.special & (1 << i);
            if (!flag)
                continue;

            result += std::format("\tspecial flag {}: {}\n", i, SkillSpecialToString(static_cast<SkillSpecialFlags>(flag)));
        }
        result += std::format("\tcombo_req: {}\n", skill.combo_req);
        result += std::format("\teffect1: {}\n", skill.effect1);
        result += std::format("\tcondition: {}\n", skill.condition);
        result += std::format("\teffect2: {}\n", skill.effect2);
        result += std::format("\tweapon_req: {}\n", skill.weapon_req);
        result += std::format("\tprofession: {}\n", static_cast<uint32_t>(skill.profession));
        result += std::format("\tattribute: {}\n", static_cast<uint32_t>(skill.attribute));
        result += std::format("\ttitle: {}\n", skill.title);
        result += std::format("\tskill_id_pvp: {}\n", static_cast<uint32_t>(skill.skill_id_pvp));
        result += std::format("\tcombo: {}\n", skill.combo);
        result += std::format("\ttarget: {}\n", skill.target);
        result += std::format("\th0032: {}\n", skill.h0032);
        result += std::format("\tskill_equip_type: {}\n", skill.skill_equip_type);
        result += std::format("\tovercast: {}\n", skill.overcast);
        result += std::format("\tenergy_cost: {}\n", skill.energy_cost);
        result += std::format("\thealth_cost: {}\n", skill.health_cost);
        result += std::format("\th0037: {}\n", skill.h0037);
        result += std::format("\tadrenaline: {}\n", skill.adrenaline);
        result += std::format("\tactivation: {}\n", skill.activation);
        result += std::format("\taftercast: {}\n", skill.aftercast);
        result += std::format("\tduration0: {}\n", skill.duration0);
        result += std::format("\tduration15: {}\n", skill.duration15);
        result += std::format("\trecharge: {}\n", skill.recharge);
        result += std::format("\th0050[0]: {}\n", skill.h0050[0]);
        result += std::format("\th0050[1]: {}\n", skill.h0050[1]);
        result += std::format("\th0050[2]: {}\n", skill.h0050[2]);
        result += std::format("\th0050[3]: {}\n", skill.h0050[3]);
        result += std::format("\tskill_arguments: {}\n", skill.skill_arguments);
        result += std::format("\tscale0: {}\n", skill.scale0);
        result += std::format("\tscale15: {}\n", skill.scale15);
        result += std::format("\tbonusScale0: {}\n", skill.bonusScale0);
        result += std::format("\tbonusScale15: {}\n", skill.bonusScale15);
        result += std::format("\taoe_range: {}\n", skill.aoe_range);
        result += std::format("\tconst_effect: {}\n", skill.const_effect);
        result += std::format("\tcaster_overhead_animation_id: {}\n", skill.caster_overhead_animation_id);
        result += std::format("\tcaster_body_animation_id: {}\n", skill.caster_body_animation_id);
        result += std::format("\ttarget_body_animation_id: {}\n", skill.target_body_animation_id);
        result += std::format("\ttarget_overhead_animation_id: {}\n", skill.target_overhead_animation_id);
        result += std::format("\tprojectile_animation_1_id: {}\n", skill.projectile_animation_1_id);
        result += std::format("\tprojectile_animation_2_id: {}\n", skill.projectile_animation_2_id);
        result += std::format("\ticon_file_id: {}\n", skill.icon_file_id);
        result += std::format("\ticon_file_id_2: {}\n", skill.icon_file_id_2);
        result += std::format("\tname: {}\n", skill.name);
        result += std::format("\tconcise: {}\n", skill.concise);
        result += std::format("\tdescription: {}\n", skill.description);

        char hex[128] = {0};
        wchar_t enc[32] = {0};
        auto ToHex = [&](uint32_t str_id)
        {
            std::memset(hex, 0, sizeof(hex));
            std::memset(enc, 0, sizeof(enc));
            GW::UI::UInt32ToEncStr(str_id, enc, std::size(enc));
            auto enc_len = std::wcslen(enc);
            for (size_t i = 0; i < enc_len; i++)
            {
                std::snprintf(hex + i * 4, 5, "%04X", enc[i]);
            }
        };
        ToHex(skill.name);
        result += std::format("\tname ENCODED: 0x{}\n", hex);
        ToHex(skill.concise);
        result += std::format("\tconcise ENCODED: 0x{}\n", hex);
        ToHex(skill.description);
        result += std::format("\tdescription ENCODED: 0x{}\n", hex);

        result += std::format("\tname DECODED: {}\n", Utils::StrIDToStr(skill.name));
        result += std::format("\tconcise DECODED: {}\n", Utils::StrIDToStr(skill.concise));
        result += std::format("\tdescription DECODED: {}\n", Utils::StrIDToStr(skill.description));

        return result;
    }

    std::span<GW::Skill> GetSkillSpan()
    {
        const auto ptr = GW::SkillbarMgr::GetSkillConstantData((GW::Constants::SkillID)0);
        if (ptr == nullptr)
        {
            return {}; // Return an empty span if ptr is nullptr
        }

        const auto len = static_cast<uint32_t>(GW::Constants::SkillID::Count);

        return {ptr, len};
    }

    // Returns a vector of unique effects sorted by timestamp
    std::vector<GW::Effect *> GetAgentUniqueEffects(uint32_t agent_id)
    {
        auto *effects = GW::Effects::GetAgentEffects(agent_id);
        if (effects == nullptr)
            return {};

        std::vector<GW::Effect *> sorted_effects;

        for (auto &effect : *effects)
        {
            const auto skill_id = (GW::Constants::SkillID)effect.skill_id;
            auto it = std::find_if(sorted_effects.begin(), sorted_effects.end(),
                [skill_id](const GW::Effect *e)
                { return e->skill_id == skill_id; });

            if (it != sorted_effects.end()) // We already have an effect with the same skill_id
            {
                if ((*it)->attribute_level < effect.attribute_level ||
                    (*it)->timestamp < effect.timestamp)
                {
                    // New effect is better; remove the old effect and later insert the new effect
                    sorted_effects.erase(it);
                }
                else
                {
                    // Old effect is better; skip the new effect
                    continue;
                }
            }
            auto insert_pos = std::upper_bound(sorted_effects.begin(), sorted_effects.end(), &effect,
                [](const GW::Effect *a, const GW::Effect *b)
                { return a->timestamp < b->timestamp; });
            sorted_effects.insert(insert_pos, &effect);
        }

        return sorted_effects;
    }

    GW::Effect *GetEffectByID(uint32_t agent_id, uint32_t effect_id)
    {
        auto effects = GW::Effects::GetAgentEffects(agent_id);
        if (effects == nullptr)
            return nullptr;

        for (auto &effect : *effects)
        {
            if (effect.effect_id == effect_id)
                return &effect;
        }

        return nullptr;
    }

    GW::Effect *GetUniqueEffectByID(uint32_t agent_id, uint32_t effect_id)
    {
        auto effects = Utils::GetAgentUniqueEffects(agent_id);
        if (effects.empty())
            return nullptr;

        for (auto effect : effects)
        {
            if (effect->effect_id == effect_id)
                return effect;
        }

        return nullptr;
    }

    GW::AgentLiving *GetAgentLivingByID(uint32_t agent_id)
    {
        auto agent = GW::Agents::GetAgentByID(agent_id);
        if (agent == nullptr)
            return nullptr;

        auto living_agent = agent->GetAsAgentLiving();
        if (living_agent == nullptr)
            return nullptr;

        return living_agent;
    }

    GW::AgentLiving &GetAgentLivingOrThrow(uint32_t agent_id)
    {
        auto agent = GW::Agents::GetAgentByID(agent_id);
        if (agent == nullptr)
        {
            throw std::runtime_error("Agent not found");
        }

        auto living_agent = agent->GetAsAgentLiving();
        if (living_agent == nullptr)
        {
            throw std::runtime_error("Agent is not living");
        }

        return *living_agent;
    }

    // If the agent receives effect added/removed events
    bool ReceivesStoCEffects(uint32_t agent_id)
    {
        auto wc = GW::GetWorldContext();
        if (wc == nullptr)
            return false;

        for (auto &pc : wc->party_profession_states)
        {
            if (pc.agent_id == agent_id)
                return true;
        }

        return false;
    }

    bool IsPartyMember(uint32_t agent_id, uint32_t party_id)
    {
        auto party = GW::PartyMgr::GetPartyInfo(party_id);
        SOFT_ASSERT(party);
        if (!party)
            return false;

        return IsPartyMember(*party, agent_id);
    }

    bool IsPartyMember(GW::PartyInfo &party, uint32_t agent_id)
    {
        for (const auto &hero : party.heroes)
        {
            if (hero.agent_id == agent_id)
                return true;
        }

        for (const auto &henchman : party.henchmen)
        {
            if (henchman.agent_id == agent_id)
                return true;
        }

        const auto agent_living = Utils::GetAgentLivingByID(agent_id);
        if (agent_living == nullptr)
            return false;

        const auto login_number = agent_living->login_number;
        if (login_number == 0)
            return false;

        for (const auto &player : party.players)
        {
            if (player.login_number == login_number)
                return true;
        }

        return false;
    }

    std::string_view GetEffectIDStr(GW::Constants::EffectID effect_id)
    {
        // clang-format off
        switch (effect_id) {
            case GW::Constants::EffectID::black_cloud: return "Black Cloud";
            case GW::Constants::EffectID::mesmer_symbol: return "Mesmer Symbol";
            case GW::Constants::EffectID::green_cloud: return "Green Cloud";
            case GW::Constants::EffectID::green_sparks: return "Green Sparks";
            case GW::Constants::EffectID::necro_symbol: return "Necro Symbol";
            case GW::Constants::EffectID::ele_symbol: return "Elementalist Symbol";
            case GW::Constants::EffectID::white_clouds: return "White Clouds";
            case GW::Constants::EffectID::monk_symbol: return "Monk Symbol";
            case GW::Constants::EffectID::bleeding: return "Bleeding";
            case GW::Constants::EffectID::blind: return "Blind";
            case GW::Constants::EffectID::burning: return "Burning";
            case GW::Constants::EffectID::disease: return "Disease";
            case GW::Constants::EffectID::poison: return "Poison";
            case GW::Constants::EffectID::dazed: return "Dazed";
            case GW::Constants::EffectID::weakness: return "Weakness"; // cracked_armor has same EffectID
            case GW::Constants::EffectID::assasin_symbol: return "Assassin Symbol";
            case GW::Constants::EffectID::ritualist_symbol: return "Ritualist Symbol";
            case GW::Constants::EffectID::dervish_symbol: return "Dervish Symbol";
            default: return {};
        }
        // clang-format on
    }

    bool HasVisibleEffect(GW::AgentLiving &agent_living, GW::Constants::EffectID visible_effect_id)
    {
        auto &visible_effects = agent_living.visible_effects;

        for (auto link = agent_living.visible_effects.Get(); link != nullptr; link = link->NextLink())
        {
            auto node = link->Next();
            if (!node)
                break;

            if (node->id == visible_effect_id &&
                !node->has_ended)
                return true;
        }

        return false;
    }

    AgentRelations GetAgentRelations(GW::Constants::Allegiance agent1_allegiance, GW::Constants::Allegiance agent2_allegiance)
    {
        if (agent1_allegiance == agent2_allegiance)
            return AgentRelations::Friendly;

        auto IsFriendlyToPlayer = [&](GW::Constants::Allegiance allegiance)
        {
            return allegiance == GW::Constants::Allegiance::Ally_NonAttackable ||
                   allegiance == GW::Constants::Allegiance::Minion ||
                   allegiance == GW::Constants::Allegiance::Npc_Minipet ||
                   allegiance == GW::Constants::Allegiance::Spirit_Pet;
        };

        if (IsFriendlyToPlayer(agent1_allegiance) && IsFriendlyToPlayer(agent2_allegiance))
            return AgentRelations::Friendly;

        return AgentRelations::Hostile;
    }

    AgentRelations GetAgentRelations(uint32_t agent1, uint32_t agent2)
    {
        const auto agent1_living = Utils::GetAgentLivingByID(agent1);
        const auto agent2_living = Utils::GetAgentLivingByID(agent2);
        if (agent1_living == nullptr || agent2_living == nullptr)
            return AgentRelations::Null;

        return GetAgentRelations(agent1_living->allegiance, agent2_living->allegiance);
    }

    float Remap(float input_min, float input_max, float output_min, float output_max, float value)
    {
        return output_min + (output_max - output_min) * ((value - input_min) / (input_max - input_min));
    }

    float ParabolicRemap(float input_min, float input_max, float output_min, float output_max, float value)
    {
        const auto value_local = value - input_min;
        const auto output_max_local = output_max - output_min;
        const auto input_max_local = input_max - input_min;
        return output_min + ((output_max_local * value_local) / input_max_local) * (2 - value_local / input_max_local);
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

    bool SkillDescriptionToEncStr(wchar_t *buffer, size_t buffer_size, const GW::Skill &skill, bool concise, int32_t attribute_level)
    {
        auto buffer_end = buffer + buffer_size;
        auto str_id = concise ? skill.concise : skill.description;
        bool success = GW::UI::UInt32ToEncStr(str_id, buffer, buffer_size);
        if (!success)
            return false;

        buffer += wcslen(buffer);

#define SWPRINTF_OR_RETURN(buffer, buffer_end, ...)                   \
    written_len = swprintf(buffer, buffer_end - buffer, __VA_ARGS__); \
    if (written_len == -1)                                            \
        return false;                                                 \
    buffer += written_len;

        bool has_attribute = attribute_level != -1;

        constexpr uint32_t MAX_VALUE = 0x8000 - 0x100 - 1;

        int written_len;
        auto Write = [&](uint32_t i, uint32_t value0, uint32_t value15) -> bool
        {
            constexpr auto enc_params = L"\x10A\x10B\x10C";
            if (buffer == buffer_end)
                return false;
            *buffer++ = enc_params[i];

            bool is_dynamic = value0 != value15;
            if (is_dynamic)
            {
                // If dynamic, start green color
                SWPRINTF_OR_RETURN(buffer, buffer_end, L"\x108\x107<c=#8fff8f><tip=%d><rep=%d>\x1\x2", i, i);
            }

            bool is_single_value = has_attribute || !is_dynamic;
            if (is_single_value)
            {
                // If one value, add it as literal
                uint32_t value;
                if (is_dynamic)
                    value = LinearAttributeScale(value0, value15, (uint32_t)attribute_level);
                else
                    value = value0;
                value = std::min(value, MAX_VALUE) + 0x100;
                SWPRINTF_OR_RETURN(buffer, buffer_end, L"\x104\x101%c", value);
            }
            else
            {
                // If two values, add range with two literals
                value0 = std::min(value0, MAX_VALUE) + 0x100;
                value15 = std::min(value15, MAX_VALUE) + 0x100;
                SWPRINTF_OR_RETURN(buffer, buffer_end, L"\x45B\x101%c\x102%c", value0, value15);
            }

            if (is_dynamic)
            {
                // If dynamic, end green color
                SWPRINTF_OR_RETURN(buffer, buffer_end, L"\x2\x108\x107</rep></tip></c>\x1");
            }

            if (buffer == buffer_end)
                return false;
            *buffer++ = '\x1';

            return true;
        };

        if (!Write(0, skill.scale0, skill.scale15))
            return false;
        if (!Write(1, skill.bonusScale0, skill.bonusScale15))
            return false;
        if (!Write(2, skill.duration0, skill.duration15))
            return false;

        if (buffer == buffer_end)
            return false;

        *buffer++ = L'\0';

        return true;
    }

    // Function to convert hex string (0xAARRGGBB) to ImU32 color
    ImU32 HexToColor(std::string &hex)
    {
        SOFT_ASSERT(hex.size() == 6);
        auto val = std::stoul(hex, nullptr, 16);
        // Swap red and blue channels because in written form red is of higher significance
        val = ((val & 0xFF00FF00) | ((val & 0xFF) << 16) | ((val & 0xFF0000) >> 16));
        auto alpha = val >> IM_COL32_A_SHIFT;
        if (alpha == 0)
            val |= IM_COL32_A_MASK;
        auto value = static_cast<ImU32>(val);
        return value;
    }

    void UnrichText(std::string &rich_text, std::vector<ColorChange> &color_changes, std::vector<TextTooltip> &tooltips, std::span<std::string_view> replacements)
    {
        size_t pos = 0;
        int color_level = 0;

        int current_replacement_id = -1;
        int replacement_start_pos = -1;

        int current_tooltip_id = -1;
        int tooltip_start_pos = -1;

        while ((pos = rich_text.find('<', pos)) != std::string::npos)
        {
            size_t end_pos = rich_text.find('>', pos);
            if (end_pos == std::string::npos)
            {
                break; // Malformed tag, exit loop
            }

            auto tag_pos = pos;
            auto tag_len = end_pos - tag_pos + 1;
            char *tag_start = &rich_text[tag_pos];
            char *tag_end = tag_start + tag_len;
            char *p = tag_start;
            p++; // Skip the '<'

            // Check if it's a color tag
            if (TryRead("c=", p, tag_end))
            {
                if (TryRead('#', p, tag_end))
                {
                    // Literal color tag
                    auto color_str = std::string(p, tag_end - 1);
                    ImU32 color = HexToColor(color_str);
                    color_changes.push_back({pos, color});
                }
                else if (TryRead('@', p, tag_end))
                {
                    // Variable color tag
                    ImU32 color;
                    if (TryRead("SKILLDULL", p, tag_end))
                        color = Constants::GWColors::skill_dull_gray;
                    else if (TryRead("SKILLDYN", p, tag_end))
                        color = Constants::GWColors::skill_dynamic_green;
                    else
                        throw std::runtime_error("Unknown color variable");
                    color_changes.push_back({pos, color});
                }
                else
                {
                    // Unknown color tag, mark with red color
                    color_changes.push_back({pos, Constants::GWColors::hp_red});
                }
                color_level++;
            }
            else if (TryRead("/c", p, tag_end))
            {
                if (color_level > 0) // Needed because some descriptions tries to close non-existing color tags
                {
                    color_changes.push_back({pos, 0});
                    color_level--;
                }
            }
            else if (TryRead("rep=", p, tag_end))
            {
                if (current_replacement_id == -1)
                {
                    auto rep_id = std::stoul(p, nullptr, 10);
                    if (rep_id < replacements.size())
                    {
                        current_replacement_id = rep_id;
                        replacement_start_pos = pos;
                    }
                }
            }
            else if (TryRead("/rep", p, tag_end))
            {
                if (current_replacement_id != -1)
                {
                    auto &replacement = replacements[current_replacement_id];
                    const auto replaced_len = pos - replacement_start_pos;
                    rich_text.replace(replacement_start_pos, replaced_len, replacement);
                    pos += replacement.size() - replaced_len;
                    current_replacement_id = -1;
                }
            }
            else if (TryRead("tip=", p, tag_end))
            {
                if (current_tooltip_id == -1)
                {
                    auto rep_id = std::stoul(p, nullptr, 10);
                    current_tooltip_id = rep_id;
                    tooltip_start_pos = pos;
                }
            }
            else if (TryRead("/tip", p, tag_end))
            {
                if (current_tooltip_id != -1)
                {
                    tooltips.push_back({(uint32_t)tooltip_start_pos, pos, (uint32_t)current_tooltip_id});
                    current_tooltip_id = -1;
                }
            }
            else
            {
                // Unknown tag, color it and leave it
                color_changes.push_back({pos, Constants::GWColors::skill_dynamic_green});
                color_changes.push_back({pos + tag_len, 0});

                pos = end_pos + 1;
                continue;
            }

            rich_text.erase(pos, tag_len);
        }

        while (color_level) // Some descriptions do not close the color tag
        {
            color_changes.push_back({rich_text.size(), 0});
            color_level--;
        }

        assert(color_level == 0);
    }

    float CalcColorBrightness(ImVec4 color)
    {
        const float r = color.x;
        const float g = color.y;
        const float b = color.z;
        const float a = color.w;
        return (0.299f * r + 0.587f * g + 0.114f * b) * a;
    }

    ImU32 InvertColor(ImU32 color)
    {
        const ImU32 r = (color >> IM_COL32_R_SHIFT) & 0xFF;
        const ImU32 g = (color >> IM_COL32_G_SHIFT) & 0xFF;
        const ImU32 b = (color >> IM_COL32_B_SHIFT) & 0xFF;
        const ImU32 a = (color >> IM_COL32_A_SHIFT) & 0xFF;
        return (255 - r) << IM_COL32_R_SHIFT | (255 - g) << IM_COL32_G_SHIFT | (255 - b) << IM_COL32_B_SHIFT | a << IM_COL32_A_SHIFT;
    }

    void DrawMultiColoredText(
        const char *text, const char *end,
        float wrapping_min, float wrapping_max,
        std::span<ColorChange> color_changes,
        std::span<uint16_t> highlighting,
        std::span<TextTooltip> tooltips,
        std::function<void(uint32_t)> draw_tooltip)
    {
        auto text_len = end ? end - text : strlen(text);

        const auto initial_cursor = ImGui::GetCursorScreenPos();

        if (text_len == 0)
        {
            auto rect = ImRect(initial_cursor, initial_cursor);
            ImGui::ItemSize(rect);
            ImGui::ItemAdd(rect, 0);
            return;
        }

        auto window = ImGui::GetCurrentWindow();
        auto draw_list = window->DrawList;

        auto ss_cursor = initial_cursor;
        const auto text_height = ImGui::GetTextLineHeight();

        float max_width;
        if (wrapping_max < 0)
            max_width = std::numeric_limits<float>::max();
        else
            max_width = wrapping_max - wrapping_min;

        float used_width = ImGui::GetCursorPosX() - wrapping_min;

        uint32_t i_color = 0;
        uint32_t i_color_change = color_changes.empty() ? -1 : color_changes[0].pos;

        uint32_t i_tooltip = 0;
        uint32_t i_tooltip_update = tooltips.empty() || draw_tooltip == nullptr ? -1 : tooltips[0].start;
        float tooltip_start_x = -1;

        uint32_t i_highlight = 0;
        uint32_t i_highlight_change = highlighting.empty() ? -1 : highlighting[0];
        float highlight_start_x = -1;

        // screen space bounding box
        ImRect bb = ImRect(ss_cursor, ss_cursor);

        const auto highlight_color = ImGui::GetColorU32(IM_COL32(250, 148, 54, 255));
        const auto highlight_text_color = ImGui::GetColorU32(IM_COL32_BLACK);

        auto DrawLine = [&](uint32_t i_start, uint32_t i_end)
        {
            bb.Min.x = ss_cursor.x;
            uint32_t i;
            do
            {
                i = std::min(i_end, i_color_change);
                i = std::min(i, i_tooltip_update);
                i = std::min(i, i_highlight_change);

                if (i_start < i)
                {
                    auto ptr_start = &text[i_start];
                    auto ptr_end = &text[i];
                    auto width = ImGui::CalcTextSize(ptr_start, ptr_end).x;
                    auto text_color = ImGui::GetColorU32(ImGuiCol_Text);
                    float new_ss_cursor_x = ss_cursor.x + width;
                    if (highlight_start_x != -1)
                    {
                        // text_color = InvertColor(text_color);
                        text_color = highlight_text_color;
                        auto min = ImVec2(highlight_start_x, ss_cursor.y);
                        auto max = ImVec2(new_ss_cursor_x, ss_cursor.y + text_height);
                        draw_list->AddRectFilled(min, max, highlight_color);
                        highlight_start_x = new_ss_cursor_x;
                    }
                    draw_list->AddText(ss_cursor, text_color, ptr_start, ptr_end);
                    ss_cursor.x = new_ss_cursor_x;
                    i_start = i;
                }

                if (i == i_highlight_change)
                {
                    highlight_start_x = highlight_start_x == -1 ? ss_cursor.x : -1;
                    i_highlight++;
                    i_highlight_change = i_highlight < highlighting.size() ? highlighting[i_highlight] : -1;
                }

                if (i == i_color_change)
                {
                    auto new_color = color_changes[i_color++].color;
                    if (new_color)
                        ImGui::PushStyleColor(ImGuiCol_Text, new_color);
                    else
                        ImGui::PopStyleColor();

                    i_color_change = i_color < color_changes.size() ? color_changes[i_color].pos : -1;
                }

                if (i == i_tooltip_update)
                {
                    if (tooltip_start_x == -1)
                    {
                        // No active tooltip
                        tooltip_start_x = ss_cursor.x;
                        i_tooltip_update = tooltips[i_tooltip].end;
                    }
                    else
                    {
                        // Active tooltip
                        auto min = ImVec2(tooltip_start_x, ss_cursor.y);
                        auto max = ImVec2(ss_cursor.x, ss_cursor.y + text_height);
                        if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(min, max))
                        {
                            draw_tooltip(tooltips[i_tooltip].id);
                        }
                        tooltip_start_x = -1;
                        i_tooltip++;
                        i_tooltip_update = i_tooltip < tooltips.size() ? tooltips[i_tooltip].start : -1;
                    }
                }
            } while (i < i_end);

            bb.Max.x = std::max(bb.Max.x, ss_cursor.x);
            ss_cursor.x = window->ContentRegionRect.Min.x + wrapping_min;
            ss_cursor.y += text_height;
            if (highlight_start_x != -1)
                highlight_start_x = ss_cursor.x;
            bb.Max.y = ss_cursor.y;
        };

        uint32_t i_line_start = 0;
        uint32_t i_line_end = 0;
        char prev_c = '\0';
        bool pending_newline = false;
        for (uint32_t i = 0; i <= text_len; i++)
        {
            char c = i < text_len ? text[i] : '\0';

            // bool is_word_start = (!prev_c || prev_c == ' ') && c != ' ';
            bool is_new_line = c == '\n';
            bool is_word_end = is_new_line || prev_c && prev_c != ' ' && (!c || c == ' ');

            if (is_word_end)
            {
                auto new_i_line_end = i;

                auto ptr_end = &text[i_line_end];
                auto ptr_new_end = &text[new_i_line_end];
                auto word_width = ImGui::CalcTextSize(ptr_end, ptr_new_end).x;

                auto new_used_width = used_width + word_width;

                if (new_used_width > max_width && used_width > 0)
                {
                    DrawLine(i_line_start, i_line_end);
                    i_line_start = i_line_end;
                    while (std::isspace(text[i_line_start]))
                        i_line_start++;
                    i_line_end = new_i_line_end;

                    auto ptr_start = &text[i_line_start];
                    auto ptr_end = &text[i_line_end];
                    used_width = ImGui::CalcTextSize(ptr_start, ptr_end).x;
                }
                else
                {
                    i_line_end = new_i_line_end;
                    used_width = new_used_width;
                }

                if (is_new_line)
                {
                    DrawLine(i_line_start, i_line_end);
                    i_line_start = i_line_end + 1;
                    i_line_end = i_line_start;
                    used_width = 0;
                }
            }

            prev_c = c;
        }

        if (i_line_start < i_line_end)
        {
            DrawLine(i_line_start, i_line_end);
        }

        ImGui::ItemSize(bb);
        ImGui::ItemAdd(bb, 0);
    }

    const std::string_view GetAttributeString(GW::Constants::AttributeByte attribute)
    {
        // clang-format off
        switch (attribute) {
            case GW::Constants::AttributeByte::FastCasting:        return "Fast Casting";
            case GW::Constants::AttributeByte::IllusionMagic:      return "Illusion Magic";
            case GW::Constants::AttributeByte::DominationMagic:    return "Domination Magic";
            case GW::Constants::AttributeByte::InspirationMagic:   return "Inspiration Magic";
            case GW::Constants::AttributeByte::BloodMagic:         return "Blood Magic";
            case GW::Constants::AttributeByte::DeathMagic:         return "Death Magic";
            case GW::Constants::AttributeByte::SoulReaping:        return "Soul Reaping";
            case GW::Constants::AttributeByte::Curses:             return "Curses";
            case GW::Constants::AttributeByte::AirMagic:           return "Air Magic";
            case GW::Constants::AttributeByte::EarthMagic:         return "Earth Magic";
            case GW::Constants::AttributeByte::FireMagic:          return "Fire Magic";
            case GW::Constants::AttributeByte::WaterMagic:         return "Water Magic";
            case GW::Constants::AttributeByte::EnergyStorage:      return "Energy Storage";
            case GW::Constants::AttributeByte::HealingPrayers:     return "Healing Prayers";
            case GW::Constants::AttributeByte::SmitingPrayers:     return "Smiting Prayers";
            case GW::Constants::AttributeByte::ProtectionPrayers:  return "Protection Prayers";
            case GW::Constants::AttributeByte::DivineFavor:        return "Divine Favor";
            case GW::Constants::AttributeByte::Strength:           return "Strength";
            case GW::Constants::AttributeByte::AxeMastery:         return "Axe Mastery";
            case GW::Constants::AttributeByte::HammerMastery:      return "Hammer Mastery";
            case GW::Constants::AttributeByte::Swordsmanship:      return "Swordsmanship";
            case GW::Constants::AttributeByte::Tactics:            return "Tactics";
            case GW::Constants::AttributeByte::BeastMastery:       return "Beast Mastery";
            case GW::Constants::AttributeByte::Expertise:          return "Expertise";
            case GW::Constants::AttributeByte::WildernessSurvival: return "Wilderness Survival";
            case GW::Constants::AttributeByte::Marksmanship:       return "Marksmanship";
            case GW::Constants::AttributeByte::DaggerMastery:      return "Dagger Mastery";
            case GW::Constants::AttributeByte::DeadlyArts:         return "Deadly Arts";
            case GW::Constants::AttributeByte::ShadowArts:         return "Shadow Arts";
            case GW::Constants::AttributeByte::Communing:          return "Communing";
            case GW::Constants::AttributeByte::RestorationMagic:   return "Restoration Magic";
            case GW::Constants::AttributeByte::ChannelingMagic:    return "Channeling Magic";
            case GW::Constants::AttributeByte::CriticalStrikes:    return "Critical Strikes";
            case GW::Constants::AttributeByte::SpawningPower:      return "Spawning Power";
            case GW::Constants::AttributeByte::SpearMastery:       return "Spear Mastery";
            case GW::Constants::AttributeByte::Command:            return "Command";
            case GW::Constants::AttributeByte::Motivation:         return "Motivation";
            case GW::Constants::AttributeByte::Leadership:         return "Leadership";
            case GW::Constants::AttributeByte::ScytheMastery:      return "Scythe Mastery";
            case GW::Constants::AttributeByte::WindPrayers:        return "Wind Prayers";
            case GW::Constants::AttributeByte::EarthPrayers:       return "Earth Prayers";
            case GW::Constants::AttributeByte::Mysticism:          return "Mysticism";
            
            case (GW::Constants::AttributeByte)51:
            case GW::Constants::AttributeByte::None:               return "";
            default:                                               return "ERROR";
        }
        // clang-format on
    }

    const std::string_view GetSkillContextString(SkillContext context)
    {
        // clang-format off
        switch (context)
        {
            case SkillContext::Rollerbeetle:      return "Rollerbeetle";
            case SkillContext::Yuletide:          return "Yuletide";
            case SkillContext::Polymock:          return "Polymock";
            case SkillContext::DragonArena:       return "Dragon Arena";
            case SkillContext::AgentOfTheMadKing: return "Agent of the Mad King";
            case SkillContext::CandyCornInfantry: return "Candy Corn Infantry";
            case SkillContext::Brawling:          return "Brawling";
            case SkillContext::Commando:          return "Commando";
            case SkillContext::Golem:             return "Golem";
            case SkillContext::UrsanBlessing:     return "Ursan Blessing";
            case SkillContext::RavenBlessing:     return "Raven Blessing";
            case SkillContext::VolfenBlessing:    return "Volfen Blessing";
            case SkillContext::KeiranThackeray:   return "Keiran Thackeray";
            case SkillContext::TuraiOssa:         return "Turai Ossa";
            case SkillContext::SaulDAlessio:      return "Saul D'Alessio";
            case SkillContext::Togo:              return "Togo";
            case SkillContext::Gwen:              return "Gwen";
            case SkillContext::SpiritForm:        return "Spirit Form";
            case SkillContext::SiegeDevourer:     return "Siege Devourer";
            case SkillContext::Junundu:           return "Junundu";

            case SkillContext::Null:              return "";
            default:                              return "ERROR";
        }
        // clang-format on
    }

    const std::string_view GetTitleString(GW::Constants::TitleID title_id)
    {
        // clang-format off
        switch (title_id) {
            case GW::Constants::TitleID::Hero:                 return "Hero Title";
            case GW::Constants::TitleID::TyrianCarto:          return "Tyrian Cartographer Title";
            case GW::Constants::TitleID::CanthanCarto:         return "Canthan Cartographer Title";
            case GW::Constants::TitleID::Gladiator:            return "Gladiator Title";
            case GW::Constants::TitleID::Champion:             return "Champion Title";
            case GW::Constants::TitleID::Kurzick:              return "Kurzick Title";
            case GW::Constants::TitleID::Luxon:                return "Luxon Title";
            case GW::Constants::TitleID::Drunkard:             return "Drunkard Title";
            case GW::Constants::TitleID::Survivor:             return "Survivor Title";
            case GW::Constants::TitleID::KoaBD:                return "Kind of a Big Deal Title";
            case GW::Constants::TitleID::ProtectorTyria:       return "Protector of Tyria Title";
            case GW::Constants::TitleID::ProtectorCantha:      return "Protector of Cantha Title";
            case GW::Constants::TitleID::Lucky:                return "Lucky Title";
            case GW::Constants::TitleID::Unlucky:              return "Unlucky Title";
            case GW::Constants::TitleID::Sunspear:             return "Sunspear Title";
            case GW::Constants::TitleID::ElonianCarto:         return "Elonian Cartographer Title";
            case GW::Constants::TitleID::ProtectorElona:       return "Protector of Elona Title";
            case GW::Constants::TitleID::Lightbringer:         return "Lightbringer Title";
            case GW::Constants::TitleID::LDoA:                 return "Legendary Defender of Ascalon Title";
            case GW::Constants::TitleID::Commander:            return "Commander Title";
            case GW::Constants::TitleID::Gamer:                return "Gamer Title";
            case GW::Constants::TitleID::SkillHunterTyria:     return "Skill Hunter of Tyria Title";
            case GW::Constants::TitleID::VanquisherTyria:      return "Vanquisher of Tyria Title";
            case GW::Constants::TitleID::SkillHunterCantha:    return "Skill Hunter of Cantha Title";
            case GW::Constants::TitleID::VanquisherCantha:     return "Vanquisher of Cantha Title";
            case GW::Constants::TitleID::SkillHunterElona:     return "Skill Hunter of Elona Title";
            case GW::Constants::TitleID::VanquisherElona:      return "Vanquisher of Elona Title";
            case GW::Constants::TitleID::LegendaryCarto:       return "Legendary Cartographer Title";
            case GW::Constants::TitleID::LegendaryGuardian:    return "Legendary Guardian Title";
            case GW::Constants::TitleID::LegendarySkillHunter: return "Legendary Skill Hunter Title";
            case GW::Constants::TitleID::LegendaryVanquisher:  return "Legendary Vanquisher Title";
            case GW::Constants::TitleID::Sweets:               return "Sweets Title";
            case GW::Constants::TitleID::GuardianTyria:        return "Guardian of Tyria Title";
            case GW::Constants::TitleID::GuardianCantha:       return "Guardian of Cantha Title";
            case GW::Constants::TitleID::GuardianElona:        return "Guardian of Elona Title";
            case GW::Constants::TitleID::Asuran:               return "Asuran Title";
            case GW::Constants::TitleID::Deldrimor:            return "Deldrimor Title";
            case GW::Constants::TitleID::Vanguard:             return "Ebon Vanguard Title";
            case GW::Constants::TitleID::Norn:                 return "Norn Title";
            case GW::Constants::TitleID::MasterOfTheNorth:     return "Master of the North Title";
            case GW::Constants::TitleID::Party:                return "Party Title";
            case GW::Constants::TitleID::Zaishen:              return "Zaishen Title";
            case GW::Constants::TitleID::TreasureHunter:       return "Treasure Hunter Title";
            case GW::Constants::TitleID::Wisdom:               return "Wisdom Title";
            case GW::Constants::TitleID::Codex:                return "Codex Title";

            case (GW::Constants::TitleID)48:
            case GW::Constants::TitleID::None:                 return "";
            default:                                           return "ERROR";
        }
        // clang-format on
    }

    const std::string_view GetProfessionString(GW::Constants::ProfessionByte profession)
    {
        // clang-format off
        switch (profession) {
            case GW::Constants::ProfessionByte::Warrior:      return "Warrior";
            case GW::Constants::ProfessionByte::Ranger:       return "Ranger";
            case GW::Constants::ProfessionByte::Monk:         return "Monk";
            case GW::Constants::ProfessionByte::Necromancer:  return "Necromancer";
            case GW::Constants::ProfessionByte::Mesmer:       return "Mesmer";
            case GW::Constants::ProfessionByte::Elementalist: return "Elementalist";
            case GW::Constants::ProfessionByte::Assassin:     return "Assassin";
            case GW::Constants::ProfessionByte::Ritualist:    return "Ritualist";
            case GW::Constants::ProfessionByte::Paragon:      return "Paragon";
            case GW::Constants::ProfessionByte::Dervish:      return "Dervish";

            case GW::Constants::ProfessionByte::None:         return "";
            default:                                          return "ERROR";
        }
        // clang-format on
    }

    const std::string_view GetCampaignString(GW::Constants::Campaign campaign)
    {
        // clang-format off
        switch (campaign) {
            case GW::Constants::Campaign::Core:             return "Core";
            case GW::Constants::Campaign::Prophecies:       return "Prophecies";
            case GW::Constants::Campaign::Factions:         return "Factions";
            case GW::Constants::Campaign::Nightfall:        return "Nightfall";
            case GW::Constants::Campaign::EyeOfTheNorth:    return "Eye of the North";
            case GW::Constants::Campaign::BonusMissionPack: return "Bonus Mission Pack";

            default:                                        return "ERROR";
        }
        // clang-format on
    }

    // Multiply the result by 2 to get index in bg atlas, add 1 if elite
    uint32_t GetSkillEffectBorderIndex(const GW::Skill &skill)
    {
        if (skill.type == GW::Constants::SkillType::Condition)
        {
            return 2;
        }
        else if (skill.type == GW::Constants::SkillType::Enchantment)
        {
            if (skill.profession == GW::Constants::ProfessionByte::Dervish)
                return 5;
            else
                return 3;
        }
        else if (skill.type == GW::Constants::SkillType::Hex)
        {
            return 4;
        }
        else
        {
            return 1;
        }
    }

    bool IsControllableAgentOfPlayer(uint32_t agent_id, uint32_t player_number)
    {
        FixedArray<uint32_t, 8> player_agents_salloc;
        auto player_agents = player_agents_salloc.ref();

        GetControllableAgentsOfPlayer(player_agents, player_number);

        for (uint32_t i = 0; i < player_agents.size(); i++)
        {
            if (player_agents[i] == agent_id)
                return true;
        }

        return false;
    }

    void GetControllableAgentsOfPlayer(FixedArrayRef<uint32_t> out, uint32_t player_number)
    {
        if (!player_number)
            player_number = GW::PlayerMgr::GetPlayerNumber();

        if (!player_number)
        {
            SOFT_ASSERT(false, L"Player number is null");
            return;
        }

        auto info = GW::PartyMgr::GetPartyInfo();
        if (!info)
        {
            SOFT_ASSERT(false, L"Party info is null");
            return;
        }

        bool success = true;
        success &= out.try_push(GW::PlayerMgr::GetPlayerAgentId(player_number));

        for (auto hero : info->heroes)
        {
            if (hero.owner_player_id == player_number)
            {
                success &= out.try_push(hero.agent_id);
            }
        }
        SOFT_ASSERT(success, L"Failed to get controllable agents of player");
    }

    void OpenWikiPage(std::string_view page)
    {
        std::string url = "https://wiki.guildwars.com/wiki/";
        const auto base_len = url.size();
        url += page;
        for (uint32_t i = base_len; i < url.size(); i++)
            if (url[i] == ' ')
                url[i] = '_';
        // // clang-format off
        // GW::GameThread::Enqueue([url]() {
        //     GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, (void*)url.data());
        // });
        // // clang-format on
        GW::UI::SendUIMessage(GW::UI::UIMessage::kOpenWikiUrl, url.data());
    }

    void ImGuiCenterAlignCursorX(float size_x)
    {
        const auto avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - size_x) / 2);
    }

    void ImGuiThickText(const char *text)
    {
        const auto cursor = ImGui::GetCursorPos();

        ImGui::TextUnformatted(text);

        ImGui::SetCursorPos(cursor + ImVec2(1, -1));
        ImGui::TextUnformatted(text);

        ImGui::SetCursorPos(cursor + ImVec2(0, -1));
        ImGui::TextUnformatted(text);

        ImGui::SetCursorPos(cursor + ImVec2(1, 0));
        ImGui::TextUnformatted(text);
    }

    bool AppendFormattedV(char *buf, size_t buf_size, size_t &len, const char *fmt, va_list args)
    {
        auto avail = buf_size - len;
        auto count = vsnprintf(buf + len, avail, fmt, args);
        assert(count >= 0);
        bool success = count < avail;
        len += count;
        return true;
    }

    bool AppendFormattedV(std::string &str, size_t reservation_size, const char *fmt, va_list args)
    {
        auto old_size = str.size();
        auto new_size = std::max(old_size + reservation_size, str.capacity());
        bool success = false;
        auto F = [&](char *buf, std::size_t buf_size)
        {
            auto size = old_size;
            success = AppendFormattedV(buf, buf_size, size, fmt, args);
            return size;
        };
        str.resize_and_overwrite(new_size, F);
        return success;
    }

    // Append formatted string to string buffer, by first reserving additional space and then appending.
    // If the appended content is larger than the unused space in the string,
    // the string is truncated and the function returns false.
    bool AppendFormatted(std::string &str, size_t reservation_size, const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        auto success = AppendFormattedV(str, reservation_size, fmt, args);
        va_end(args);
        return success;
    }

    // Splits on space, ignores leading and trailing spaces
    void Split(std::string_view str, FixedArrayRef<std::string_view> out)
    {
        uint32_t i = 0;
        while (i < str.size())
        {
            while (i < str.size() && str[i] == ' ')
                i++;
            if (i == str.size())
                break;
            uint32_t start = i;
            while (i < str.size() && str[i] != ' ')
                i++;
            uint32_t end = i;

            out.try_push(str.substr(start, end - start));
        }
    }

    void CamelSplit(std::string_view str, FixedArrayRef<std::string_view> out)
    {
        uint32_t i = 0;
        while (i < str.size() && str[i] == ' ')
            i++;
        while (i < str.size())
        {
            uint32_t start = i;
            i++;
            while (i < str.size() && str[i] >= 'a' && str[i] <= 'z')
                i++;
            uint32_t end = i;

            out.try_push(str.substr(start, end - start));
        }
    }

    bool IsFrameValid(GW::UI::Frame *frame)
    {
        return frame && (int)frame != -1;
    }

    uint32_t GetPetOfAgent(uint32_t agent_id)
    {
        auto pet_info = GW::PartyMgr::GetPetInfo(agent_id);
        if (pet_info)
        {
            return pet_info->agent_id;
        }

        // TODO: Figure out way to get pet of non-party members

        return 0;
    }

    bool InSameParty(uint32_t agent1_id, uint32_t agent2_id)
    {
        auto party_context = GW::GetPartyContext();
        if (!party_context)
            return false;

        for (auto party : party_context->parties)
        {
            if (!party)
                continue;

            if (Utils::IsPartyMember(*party, agent1_id))
            {
                return Utils::IsPartyMember(*party, agent2_id);
            }
        }

        return false;
    }

    bool IsRangeValue(float value)
    {
        return value == 72.f ||
               value == 156.f ||
               value == 240.f ||
               value == 312.f ||
               value == 1000.f ||
               value == 2000.f ||
               value == 2500.f ||
               value == 5000.f;
    }

    bool GetIsPet(uint32_t agent_id)
    {
        bool is_pet = false;
        auto agent = Utils::GetAgentLivingByID(agent_id);
        if (agent && agent->IsNPC())
        {
            auto npc = GW::Agents::GetNPCByID(agent->player_number);
            if (npc && npc->IsPet())
                is_pet = true;
        }
        return is_pet;
    }

    bool IsOvercast(GW::AgentLiving &agent)
    {
        auto overcast_lower_bound = *(float *)&agent.h0118;
        bool is_overcast = overcast_lower_bound < 1.f;
        return is_overcast;
    }
}