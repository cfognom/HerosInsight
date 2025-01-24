#ifdef _DEBUG

#include <Windows.h>

#include <bitset>
#include <format>
#include <map>
#include <string>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GadgetContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/GameplayContext.h>
#include <GWCA/Context/ItemContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/List.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_internal.h>

#include <behaviours.h>
#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <damage_display.h>
#include <debug_display.h>
#include <effect_tracking.h>
#include <energy_display.h>
#include <packet_reader.h>
#include <party_data.h>
#include <texture_module.h>
#include <utils.h>

namespace HerosInsight::Debug
{
    void DisplayHeroInfo(int hero_index)
    {
        const auto hero_agent_id = GW::Agents::GetHeroAgentID(hero_index);
        const auto map_agent = GW::Agents::GetMapAgentByID(hero_agent_id);
        if (map_agent == nullptr)
            return;
        const auto living_agent = Utils::GetAgentLivingByID(hero_agent_id);
        if (living_agent == nullptr)
            return;
        // DebugDisplay::PushToDisplay("hero" + std::to_string(hero_index) + " casting:", Utils::UInt32ToBinaryStr(bar->casting));
        const auto header = "hero" + std::to_string(hero_index);
        DebugDisplay::PushToDisplay(header + " cur_energy:", std::to_string(map_agent->cur_energy));
        DebugDisplay::PushToDisplay(header + " cur_health:", std::to_string(map_agent->cur_health));
        DebugDisplay::PushToDisplay(header + " effects:", std::to_string(map_agent->effects));
        DebugDisplay::PushToDisplay(header + " energy_regen:", std::to_string(map_agent->energy_regen));
        DebugDisplay::PushToDisplay(header + " health_regen:", std::to_string(map_agent->health_regen));
        DebugDisplay::PushToDisplay(header + " max_energy2:", std::to_string(map_agent->max_energy2));
        DebugDisplay::PushToDisplay(header + " max_energy:", std::to_string(map_agent->max_energy));
        DebugDisplay::PushToDisplay(header + " max_health:", std::to_string(map_agent->max_health));
        DebugDisplay::PushToDisplay(header + " skill_timestamp:", std::to_string(map_agent->skill_timestamp));
        DebugDisplay::PushToDisplay(header + " l_energy:", std::to_string(living_agent->energy));
        DebugDisplay::PushToDisplay(header + " l_energy_regen:", std::to_string(living_agent->energy_regen));
        DebugDisplay::PushToDisplay(header + " l_max_energy:", std::to_string(living_agent->max_energy));
        DebugDisplay::PushToDisplay(header + " distance_to_player:", std::to_string(Utils::DistanceToPlayer(hero_agent_id)));
    }

    GW::Constants::SkillID manual_hovered_skill_id = GW::Constants::SkillID::No_Skill;
    void SetHoveredSkill(GW::Constants::SkillID skill_id)
    {
        manual_hovered_skill_id = skill_id;
    }

    void DebugHoveredSkill()
    {
        auto skill_id = manual_hovered_skill_id;
        manual_hovered_skill_id = GW::Constants::SkillID::No_Skill;
        auto agent_id = GW::Agents::GetControlledCharacterId();

        auto tooltip = GW::UI::GetCurrentTooltip();
        if (tooltip && tooltip->payload > (uint32_t *)0x100)
        {
            const auto payload = tooltip->payload;
            skill_id = (GW::Constants::SkillID)payload[0];
            agent_id = payload[2] == 0 ? payload[3] : 0;
        }

        if (skill_id == GW::Constants::SkillID::No_Skill ||
            skill_id >= GW::Constants::SkillID::Count)
            return;

        const auto &cskill = CustomSkillDataModule::GetCustomSkillData(skill_id);
        auto &skill = *cskill.skill;
        const auto str = Utils::SkillConstantDataToString(skill);
        DebugDisplay::PushToDisplay("Hovered skill data", str);

        const auto custom_str = cskill.ToString();
        DebugDisplay::PushToDisplay("Hovered skill custom_sd", custom_str);

        DebugDisplay::PushToDisplay("Hovered skill", Utils::StrIDToStr(skill.name));
        DebugDisplay::PushToDisplay("Hovered skill agent_id", std::to_string(agent_id));
        DebugDisplay::PushToDisplay("Hovered skill init_effect count", cskill.init_effects.size());
        DebugDisplay::PushToDisplay("Hovered skill end_effect count", cskill.end_effects.size());
        for (uint32_t i = 0; i < cskill.init_effects.size(); i++)
        {
            auto &effect = cskill.init_effects[i];
            DebugDisplay::PushToDisplay(L"Hovered skill init_effect[{}] = {}", i, effect.ToWString());
        }
        for (uint32_t i = 0; i < cskill.end_effects.size(); i++)
        {
            auto &effect = cskill.end_effects[i];
            DebugDisplay::PushToDisplay(L"Hovered skill end_effect[{}] = {}", i, effect.ToWString());
        }

        const auto living_agent = Utils::GetAgentLivingByID(agent_id);
        if (living_agent == nullptr)
            return;

        const auto maintainability = Utils::CalculateMaintainability(living_agent, skill_id);
        DebugDisplay::PushToDisplay("Hovered skill maintainability", std::to_string(maintainability.value));
        DebugDisplay::PushToDisplay("Hovered skill effective maintainability", std::to_string(maintainability.avg_value));
    }

    void DebugTooltip()
    {
        DebugDisplay::ClearDisplay("Tooltip");
        const auto tooltip = GW::UI::GetCurrentTooltip();
        if (tooltip == nullptr)
            return;

        DebugDisplay::PushToDisplay("Tooltip bit_field", std::to_string(tooltip->bit_field));
        DebugDisplay::PushToDisplay("Tooltip unk1", std::to_string(tooltip->unk1));
        DebugDisplay::PushToDisplay("Tooltip unk2", std::to_string(tooltip->unk2));
        DebugDisplay::PushToDisplay("Tooltip unk3", std::to_string(tooltip->unk3));
        DebugDisplay::PushToDisplay("Tooltip unk4", std::to_string(tooltip->unk4));

        const auto payload = tooltip->payload;
        if (payload == nullptr)
            return;

        DebugDisplay::PushToDisplay("Tooltip payload_len", std::to_string(tooltip->payload_len));
        const auto len = tooltip->payload_len / sizeof(uint32_t);
        for (uint32_t i = 0; i < len; i++)
        {
            DebugDisplay::PushToDisplay("Tooltip payload[" + std::to_string(i) + "]", std::to_string(payload[i]));
        }

        auto decoded = Utils::DecodeString((wchar_t *)payload);
        DebugDisplay::PushToDisplay("Tooltip decoded", Utils::WStrToStr(decoded.data()));
    }

    void ListTargetEffects()
    {
        DebugDisplay::ClearDisplay("target effect");
        const auto target_id = GW::Agents::GetTargetId();
        const auto agent = GW::Agents::GetAgentByID(target_id);
        if (agent == nullptr)
            return;
        const auto living_agent = agent->GetAsAgentLiving();
        if (living_agent == nullptr)
            return;

        auto effects = GW::Effects::GetAgentEffects(target_id);
        if (effects == nullptr)
            return;

        auto n_effects = effects->size();
        DebugDisplay::PushToDisplay("target effect count", std::to_string(n_effects));

        for (uint32_t i = 0; i < n_effects; i++)
        {
            const auto &effect = effects->at(i);
            DebugDisplay::PushToDisplay("target effect " + std::to_string(i), Utils::EffectToString(&effect));
        }
    }

    void ListTargetVisibleEffects()
    {
        DebugDisplay::ClearDisplay("target visible effect");
        const auto target_id = GW::Agents::GetTargetId();
        const auto living_agent = Utils::GetAgentLivingByID(target_id);
        if (living_agent == nullptr)
            return;

        auto link = living_agent->visible_effects.Get();

        uint32_t i = 0;
        for (auto link = living_agent->visible_effects.Get(); link != nullptr; link = link->NextLink())
        {
            auto node = link->Next();
            if (!node)
                break;

            const auto &effect = *node;
            std::string str = std::format("unk: {}, id: {}, has_ended: {}", effect.unk, Utils::GetEffectIDStr(effect.id), effect.has_ended);
            DebugDisplay::PushToDisplay("target visible effect " + std::to_string(i), str);
            i++;
        }
        DebugDisplay::PushToDisplay("target visible effect count", std::to_string(i));
    }

    void ListTargetBuffs()
    {
        DebugDisplay::ClearDisplay("target buff");
        const auto target_id = GW::Agents::GetTargetId();
        const auto agent = GW::Agents::GetAgentByID(target_id);
        if (agent == nullptr)
            return;
        const auto living_agent = agent->GetAsAgentLiving();
        if (living_agent == nullptr)
            return;

        auto buffs = GW::Effects::GetAgentBuffs(target_id);
        if (buffs == nullptr)
            return;

        auto n_buffs = buffs->size();
        DebugDisplay::PushToDisplay("target buff count", std::to_string(n_buffs));

        for (uint32_t i = 0; i < n_buffs; i++)
        {
            const auto &buff = buffs->at(i);
            DebugDisplay::PushToDisplay("target buff " + std::to_string(i), Utils::BuffToString(&buff));
        }
    }

    void SearchForEncodedString()
    {
        for (uint32_t i = 0; i < 32000; i++)
        {
            auto OnDecoded = [](void *param, const wchar_t *s)
            {
                // if (wcscmp(s, L"!!!") != 0)
                // if (s[0] == L'(' && s[wcslen(s) - 1] == L')')
                if (wcsstr(s, L"...") != nullptr)
                {
                    auto i = (uint32_t)param;
                    DebugDisplay::PushToDisplay("_DECODED_" + std::to_string(i), s);
                }
            };
            GW::UI::AsyncDecodeStr((wchar_t *)(&i), OnDecoded, (void *)i);
        }
    }

    void DisplayTargetModifiers(uint32_t item_index)
    {
        auto key_prefix = "item " + std::to_string(item_index) + " mod_id ";
        DebugDisplay::ClearDisplay(key_prefix);

        const auto agent_ptr = GW::Agents::GetTargetAsAgentLiving();
        if (agent_ptr == nullptr)
            return;
        auto &agent = *agent_ptr;

        const auto target_item = Utils::GetAgentItem(agent, item_index);
        if (target_item == nullptr)
            return;

        const auto mods = Utils::GetItemModifiers(*target_item);
        for (const auto &mod : mods)
        {
            auto key = key_prefix + std::to_string(mod.identifier());
            // clang-format off
            switch ((Utils::ModifierID)mod.identifier())
            {
                case Utils::ModifierID::Damage_Range:
                case Utils::ModifierID::Damage_Range2:            key += "(Damage Range)"; break;
                case Utils::ModifierID::DamageType:               key += "(Damage Type)"; break;
                case Utils::ModifierID::WeaponRequirement:        key += "(Weapon requirement)"; break;
                case Utils::ModifierID::Customized:               key += "(Customized)"; break;

                case Utils::ModifierID::EnergyBonus:
                case Utils::ModifierID::EnergyBonus2:             key += "(Energy +)"; break;
                case Utils::ModifierID::OfEnchanting:             key += "(Of Enchanting)"; break;
                case Utils::ModifierID::Sundering:                key += "(Sundering)"; break;
                case Utils::ModifierID::OfFortitude:
                case Utils::ModifierID::OfFortitude2:             key += "(Of Fortitude)"; break;
                case Utils::ModifierID::OfAttribute:              key += "(Of Attribute)"; break;

                case Utils::ModifierID::HCT_Generic:              key += "(HCT generic)"; break;
                case Utils::ModifierID::HCT_OfAttribute:          key += "(HCT of attribute)"; break;
                case Utils::ModifierID::HCT_OfItemsAttribute:     key += "(HCT of item's attribute)"; break;

                case Utils::ModifierID::HSR_Generic:              key += "(HSR generic)"; break;
                case Utils::ModifierID::HSR_OfAttribute:          key += "(HSR of attribute)"; break;
                case Utils::ModifierID::HSR_OfItemsAttribute:     key += "(HSR of item's attribute)"; break;

                case Utils::ModifierID::StrengthAndHonor:         key += "(Strength and Honor)"; break;
                case Utils::ModifierID::VengeanceIsMine:          key += "(Vengeance is Mine)"; break;
                case Utils::ModifierID::DanceWithDeath:           key += "(Dance with Death)"; break;
                case Utils::ModifierID::TooMuchInformation:       key += "(Too Much Information)"; break;
                case Utils::ModifierID::GuidedByFate:             key += "(Guided by Fate)"; break;

                case Utils::ModifierID::DamageVsUndead:           key += "(Damage vs Undead)"; break;
                case Utils::ModifierID::ArmorVsElemental:         key += "(Armor vs Elemental)"; break;
                case Utils::ModifierID::ArmorVsDmgType:           key += "(Armor vs Dmg Type)"; break;
                case Utils::ModifierID::Armor:                    key += "(Armor)"; break;
                case Utils::ModifierID::ArmorValue:               key += "(Armor Value)"; break;
                case Utils::ModifierID::DoubleAdrenalineGain:     key += "(Double Adrenaline Gain)"; break;
                case Utils::ModifierID::HealthWhileEnchanted:     key += "(Health While Enchanted)"; break;
                case Utils::ModifierID::ConditionDecrease20:
                    key += "(";
                    key += Utils::WStrToStr(Utils::GetSkillName((GW::Constants::SkillID)((uint32_t)GW::Constants::SkillID::Bleeding + mod.arg1())).data());
                    key += " Duration -20%)";
                    break;
                case Utils::ModifierID::DurationIncrease33:
                    key += "(";
                    key += Utils::WStrToStr(Utils::GetSkillName((GW::Constants::SkillID)mod.arg()).data());
                    key += " Duration +33%)";
                    break;
            }
            // clang-format on
            DebugDisplay::PushToDisplay(key, "arg: " + std::to_string(mod.arg()) +
                                                 "\targ1: " + std::to_string(mod.arg1()) +
                                                 "\targ2: " + std::to_string(mod.arg2()));
        }
    }

    std::pair<std::wstring, int32_t> GetSkillFieldName(uint32_t offset)
    {
#define RETURN_FIELD_NAME_AND_OFFSET(field)                         \
    {                                                               \
        int32_t field_offset = offset - offsetof(GW::Skill, field); \
        if (field_offset >= 0)                                      \
            return {std::wstring(L#field), field_offset};           \
    }
        RETURN_FIELD_NAME_AND_OFFSET(description);
        RETURN_FIELD_NAME_AND_OFFSET(concise);
        RETURN_FIELD_NAME_AND_OFFSET(name);
        RETURN_FIELD_NAME_AND_OFFSET(icon_file_id_2);
        RETURN_FIELD_NAME_AND_OFFSET(icon_file_id);
        RETURN_FIELD_NAME_AND_OFFSET(projectile_animation_2_id);
        RETURN_FIELD_NAME_AND_OFFSET(projectile_animation_1_id);
        RETURN_FIELD_NAME_AND_OFFSET(target_overhead_animation_id);
        RETURN_FIELD_NAME_AND_OFFSET(target_body_animation_id);
        RETURN_FIELD_NAME_AND_OFFSET(caster_body_animation_id);
        RETURN_FIELD_NAME_AND_OFFSET(caster_overhead_animation_id);
        RETURN_FIELD_NAME_AND_OFFSET(const_effect);
        RETURN_FIELD_NAME_AND_OFFSET(aoe_range);
        RETURN_FIELD_NAME_AND_OFFSET(bonusScale15);
        RETURN_FIELD_NAME_AND_OFFSET(bonusScale0);
        RETURN_FIELD_NAME_AND_OFFSET(scale15);
        RETURN_FIELD_NAME_AND_OFFSET(scale0);
        RETURN_FIELD_NAME_AND_OFFSET(skill_arguments);
        RETURN_FIELD_NAME_AND_OFFSET(h0050);
        RETURN_FIELD_NAME_AND_OFFSET(recharge);
        RETURN_FIELD_NAME_AND_OFFSET(duration15);
        RETURN_FIELD_NAME_AND_OFFSET(duration0);
        RETURN_FIELD_NAME_AND_OFFSET(aftercast);
        RETURN_FIELD_NAME_AND_OFFSET(activation);
        RETURN_FIELD_NAME_AND_OFFSET(adrenaline);
        RETURN_FIELD_NAME_AND_OFFSET(h0037);
        RETURN_FIELD_NAME_AND_OFFSET(health_cost);
        RETURN_FIELD_NAME_AND_OFFSET(energy_cost);
        RETURN_FIELD_NAME_AND_OFFSET(overcast);
        RETURN_FIELD_NAME_AND_OFFSET(skill_equip_type);
        RETURN_FIELD_NAME_AND_OFFSET(h0032);
        RETURN_FIELD_NAME_AND_OFFSET(target);
        RETURN_FIELD_NAME_AND_OFFSET(combo);
        RETURN_FIELD_NAME_AND_OFFSET(skill_id_pvp);
        RETURN_FIELD_NAME_AND_OFFSET(title);
        RETURN_FIELD_NAME_AND_OFFSET(attribute);
        RETURN_FIELD_NAME_AND_OFFSET(profession);
        RETURN_FIELD_NAME_AND_OFFSET(weapon_req);
        RETURN_FIELD_NAME_AND_OFFSET(effect2);
        RETURN_FIELD_NAME_AND_OFFSET(condition);
        RETURN_FIELD_NAME_AND_OFFSET(effect1);
        RETURN_FIELD_NAME_AND_OFFSET(combo_req);
        RETURN_FIELD_NAME_AND_OFFSET(special);
        RETURN_FIELD_NAME_AND_OFFSET(type);
        RETURN_FIELD_NAME_AND_OFFSET(campaign);
        RETURN_FIELD_NAME_AND_OFFSET(h0004);
        RETURN_FIELD_NAME_AND_OFFSET(skill_id);

        return {L"Unknown", 0};
    }

    void DisplayTargetUnknownFields()
    {
        const auto living_target = GW::Agents::GetTargetAsAgentLiving();
        if (living_target == nullptr)
            return;

        const auto agent_id = living_target->agent_id;
        std::string key = "living_target.";

        DebugDisplay::PushToDisplay(key + "h0004", std::to_string(living_target->h0004));
        DebugDisplay::PushToDisplay(key + "h0008", std::to_string(living_target->h0008));

        DebugDisplay::PushToDisplay(key + "h000C[0]", std::to_string(living_target->h000C[0]));
        DebugDisplay::PushToDisplay(key + "h000C[1]", std::to_string(living_target->h000C[1]));

        DebugDisplay::PushToDisplay(key + "h0060", std::to_string(living_target->h0060));

        DebugDisplay::PushToDisplay(key + "h0070[0]", std::to_string(living_target->h0070[0]));
        DebugDisplay::PushToDisplay(key + "h0070[1]", std::to_string(living_target->h0070[1]));
        DebugDisplay::PushToDisplay(key + "h0070[2]", std::to_string(living_target->h0070[2]));
        DebugDisplay::PushToDisplay(key + "h0070[3]", std::to_string(living_target->h0070[3]));
        DebugDisplay::PushToDisplay(key + "h0080[0]", std::to_string(living_target->h0080[0]));
        DebugDisplay::PushToDisplay(key + "h0080[1]", std::to_string(living_target->h0080[1]));
        DebugDisplay::PushToDisplay(key + "h0080[2]", std::to_string(living_target->h0080[2]));
        DebugDisplay::PushToDisplay(key + "h0080[3]", std::to_string(living_target->h0080[3]));

        DebugDisplay::PushToDisplay(key + "h0092", std::to_string(living_target->h0092));

        DebugDisplay::PushToDisplay(key + "h0094[0]", std::to_string(*(float *)&living_target->h0094[0]));
        DebugDisplay::PushToDisplay(key + "h0094[1]", std::to_string(*(float *)&living_target->h0094[1]));

        DebugDisplay::PushToDisplay(key + "h00A8", std::to_string(living_target->h00A8));

        DebugDisplay::PushToDisplay(key + "h00B4[0]", std::to_string(living_target->h00B4[0]));
        DebugDisplay::PushToDisplay(key + "h00B4[1]", std::to_string(living_target->h00B4[1]));
        DebugDisplay::PushToDisplay(key + "h00B4[2]", std::to_string(living_target->h00B4[2]));
        DebugDisplay::PushToDisplay(key + "h00B4[3]", std::to_string(living_target->h00B4[3]));

        DebugDisplay::PushToDisplay(key + "h00C8", std::to_string(living_target->h00C8));
        DebugDisplay::PushToDisplay(key + "h00CC", std::to_string(living_target->h00CC));
        DebugDisplay::PushToDisplay(key + "h00D0", std::to_string(living_target->h00D0));

        DebugDisplay::PushToDisplay(key + "h00D4[0]", std::to_string(living_target->h00D4[0]));
        DebugDisplay::PushToDisplay(key + "h00D4[1]", std::to_string(living_target->h00D4[1]));
        DebugDisplay::PushToDisplay(key + "h00D4[2]", std::to_string(living_target->h00D4[2]));

        DebugDisplay::PushToDisplay(key + "h00E4[0]", std::to_string(living_target->h00E4[0]));
        DebugDisplay::PushToDisplay(key + "h00E4[1]", std::to_string(living_target->h00E4[1]));

        DebugDisplay::PushToDisplay(key + "h0100", std::to_string(living_target->h0100));
        DebugDisplay::PushToDisplay(key + "h0108", std::to_string(living_target->h0108));

        DebugDisplay::PushToDisplay(key + "h010E[0]", std::to_string(living_target->h010E[0]));
        DebugDisplay::PushToDisplay(key + "h010E[1]", std::to_string(living_target->h010E[1]));

        DebugDisplay::PushToDisplay(key + "h0110", std::to_string(*(float *)&living_target->h0110));
        DebugDisplay::PushToDisplay(key + "h0118 (overcast)", std::to_string(*(float *)&living_target->h0118));
        DebugDisplay::PushToDisplay(key + "h0124", std::to_string(living_target->h0124));
        DebugDisplay::PushToDisplay(key + "h012C", std::to_string(*(float *)&living_target->h012C));
        DebugDisplay::PushToDisplay(key + "h013C", std::to_string(living_target->h013C));

        DebugDisplay::PushToDisplay(key + "h0141[0]", std::to_string(living_target->h0141[0]));
        DebugDisplay::PushToDisplay(key + "h0141[1]", std::to_string(living_target->h0141[1]));
        DebugDisplay::PushToDisplay(key + "h0141[2]", std::to_string(living_target->h0141[2]));
        DebugDisplay::PushToDisplay(key + "h0141[3]", std::to_string(living_target->h0141[3]));
        DebugDisplay::PushToDisplay(key + "h0141[4]", std::to_string(living_target->h0141[4]));
        DebugDisplay::PushToDisplay(key + "h0141[5]", std::to_string(living_target->h0141[5]));
        DebugDisplay::PushToDisplay(key + "h0141[6]", std::to_string(living_target->h0141[6]));
        DebugDisplay::PushToDisplay(key + "h0141[7]", std::to_string(living_target->h0141[7]));
        DebugDisplay::PushToDisplay(key + "h0141[8]", std::to_string(living_target->h0141[8]));
        DebugDisplay::PushToDisplay(key + "h0141[9]", std::to_string(living_target->h0141[9]));
        DebugDisplay::PushToDisplay(key + "h0141[10]", std::to_string(living_target->h0141[10]));
        DebugDisplay::PushToDisplay(key + "h0141[11]", std::to_string(living_target->h0141[11]));
        DebugDisplay::PushToDisplay(key + "h0141[12]", std::to_string(living_target->h0141[12]));
        DebugDisplay::PushToDisplay(key + "h0141[13]", std::to_string(living_target->h0141[13]));
        DebugDisplay::PushToDisplay(key + "h0141[14]", std::to_string(living_target->h0141[14]));
        DebugDisplay::PushToDisplay(key + "h0141[15]", std::to_string(living_target->h0141[15]));
        DebugDisplay::PushToDisplay(key + "h0141[16]", std::to_string(living_target->h0141[16]));
        DebugDisplay::PushToDisplay(key + "h0141[17]", std::to_string(living_target->h0141[17]));
        DebugDisplay::PushToDisplay(key + "h0141[18]", std::to_string(living_target->h0141[18]));

        DebugDisplay::PushToDisplay(key + "h015C[0]", std::to_string(living_target->h015C[0]));
        DebugDisplay::PushToDisplay(key + "h015C[1]", std::to_string(living_target->h015C[1]));
        DebugDisplay::PushToDisplay(key + "h015C[2]", std::to_string(living_target->h015C[2]));
        DebugDisplay::PushToDisplay(key + "h015C[3]", std::to_string(living_target->h015C[3]));

        DebugDisplay::PushToDisplay(key + "h017C", std::to_string(living_target->h017C));

        DebugDisplay::PushToDisplay(key + "h0190[0]", std::to_string(living_target->h0190[0]));
        DebugDisplay::PushToDisplay(key + "h0190[1]", std::to_string(living_target->h0190[1]));
        DebugDisplay::PushToDisplay(key + "h0190[2]", std::to_string(living_target->h0190[2]));
        DebugDisplay::PushToDisplay(key + "h0190[3]", std::to_string(living_target->h0190[3]));
        DebugDisplay::PushToDisplay(key + "h0190[4]", std::to_string(living_target->h0190[4]));
        DebugDisplay::PushToDisplay(key + "h0190[5]", std::to_string(living_target->h0190[5]));
        DebugDisplay::PushToDisplay(key + "h0190[6]", std::to_string(living_target->h0190[6]));
        DebugDisplay::PushToDisplay(key + "h0190[7]", std::to_string(living_target->h0190[7]));
        DebugDisplay::PushToDisplay(key + "h0190[8]", std::to_string(living_target->h0190[8]));
        DebugDisplay::PushToDisplay(key + "h0190[9]", std::to_string(living_target->h0190[9]));
        DebugDisplay::PushToDisplay(key + "h0190[10]", std::to_string(living_target->h0190[10]));
        DebugDisplay::PushToDisplay(key + "h0190[11]", std::to_string(living_target->h0190[11]));
        DebugDisplay::PushToDisplay(key + "h0190[12]", std::to_string(living_target->h0190[12]));
        DebugDisplay::PushToDisplay(key + "h0190[13]", std::to_string(living_target->h0190[13]));
        DebugDisplay::PushToDisplay(key + "h0190[14]", std::to_string(living_target->h0190[14]));
        DebugDisplay::PushToDisplay(key + "h0190[15]", std::to_string(living_target->h0190[15]));
        DebugDisplay::PushToDisplay(key + "h0190[16]", std::to_string(living_target->h0190[16]));
        DebugDisplay::PushToDisplay(key + "h0190[17]", std::to_string(living_target->h0190[17]));
        DebugDisplay::PushToDisplay(key + "h0190[18]", std::to_string(living_target->h0190[18]));
        DebugDisplay::PushToDisplay(key + "h0190[19]", std::to_string(living_target->h0190[19]));
        DebugDisplay::PushToDisplay(key + "h0190[20]", std::to_string(living_target->h0190[20]));
        DebugDisplay::PushToDisplay(key + "h0190[21]", std::to_string(living_target->h0190[21]));
        DebugDisplay::PushToDisplay(key + "h0190[22]", std::to_string(living_target->h0190[22]));
        DebugDisplay::PushToDisplay(key + "h0190[23]", std::to_string(living_target->h0190[23]));
        DebugDisplay::PushToDisplay(key + "h0190[24]", std::to_string(living_target->h0190[24]));
        DebugDisplay::PushToDisplay(key + "h0190[25]", std::to_string(living_target->h0190[25]));
        DebugDisplay::PushToDisplay(key + "h0190[26]", std::to_string(living_target->h0190[26]));
        DebugDisplay::PushToDisplay(key + "h0190[27]", std::to_string(living_target->h0190[27]));
        DebugDisplay::PushToDisplay(key + "h0190[28]", std::to_string(living_target->h0190[28]));
        DebugDisplay::PushToDisplay(key + "h0190[29]", std::to_string(living_target->h0190[29]));
        DebugDisplay::PushToDisplay(key + "h0190[30]", std::to_string(living_target->h0190[30]));
        DebugDisplay::PushToDisplay(key + "h0190[31]", std::to_string(living_target->h0190[31]));

        DebugDisplay::PushToDisplay(key + "h01B6", std::to_string(living_target->h01B6));

        const auto *agent_movement = Utils::GetAgentMovementByID(agent_id);
        if (agent_movement == nullptr)
            return;

        auto move_key = key + "agent_movement.";

        DebugDisplay::PushToDisplay(move_key + "h0000[0]", std::to_string(agent_movement->h0000[0]));
        DebugDisplay::PushToDisplay(move_key + "h0000[1]", std::to_string(agent_movement->h0000[1]));
        DebugDisplay::PushToDisplay(move_key + "h0000[2]", std::to_string(agent_movement->h0000[2]));

        DebugDisplay::PushToDisplay(move_key + "h0010[0]", std::to_string(agent_movement->h0010[0]));
        DebugDisplay::PushToDisplay(move_key + "h0010[1]", std::to_string(agent_movement->h0010[1]));
        DebugDisplay::PushToDisplay(move_key + "h0010[2]", std::to_string(agent_movement->h0010[2]));

        DebugDisplay::PushToDisplay(move_key + "h0020[0]", std::to_string(agent_movement->h0020[0]));
        DebugDisplay::PushToDisplay(move_key + "h0020[1]", std::to_string(agent_movement->h0020[1]));
        DebugDisplay::PushToDisplay(move_key + "h0020[2]", std::to_string(agent_movement->h0020[2]));
        DebugDisplay::PushToDisplay(move_key + "h0020[3]", std::to_string(agent_movement->h0020[3]));
        DebugDisplay::PushToDisplay(move_key + "h0020[4]", std::to_string(agent_movement->h0020[4]));
        DebugDisplay::PushToDisplay(move_key + "h0020[5]", std::to_string(agent_movement->h0020[5]));

        DebugDisplay::PushToDisplay(move_key + "h003C[0]", std::to_string(agent_movement->h003C[0]));
        DebugDisplay::PushToDisplay(move_key + "h003C[1]", std::to_string(agent_movement->h003C[1]));

        DebugDisplay::PushToDisplay(move_key + "h0048[0]", std::to_string(agent_movement->h0048[0]));
        DebugDisplay::PushToDisplay(move_key + "h0048[1]", std::to_string(agent_movement->h0048[1]));
        DebugDisplay::PushToDisplay(move_key + "h0048[2]", std::to_string(agent_movement->h0048[2]));
        DebugDisplay::PushToDisplay(move_key + "h0048[3]", std::to_string(agent_movement->h0048[3]));
        DebugDisplay::PushToDisplay(move_key + "h0048[4]", std::to_string(agent_movement->h0048[4]));
        DebugDisplay::PushToDisplay(move_key + "h0048[5]", std::to_string(agent_movement->h0048[5]));
        DebugDisplay::PushToDisplay(move_key + "h0048[6]", std::to_string(agent_movement->h0048[6]));

        DebugDisplay::PushToDisplay(move_key + "h0070", std::to_string(agent_movement->h0070));

        const auto *agent_info = Utils::GetAgentInfoByID(agent_id);
        if (agent_info == nullptr)
            return;

        auto info_key = key + "agent_info.";

        DebugDisplay::PushToDisplay(info_key + "h0000[0]", std::to_string(agent_info->h0000[0]));
        DebugDisplay::PushToDisplay(info_key + "h0000[1]", std::to_string(agent_info->h0000[1]));
        DebugDisplay::PushToDisplay(info_key + "h0000[2]", std::to_string(agent_info->h0000[2]));
        DebugDisplay::PushToDisplay(info_key + "h0000[3]", std::to_string(agent_info->h0000[3]));
        DebugDisplay::PushToDisplay(info_key + "h0000[4]", std::to_string(agent_info->h0000[4]));
        DebugDisplay::PushToDisplay(info_key + "h0000[5]", std::to_string(agent_info->h0000[5]));
        DebugDisplay::PushToDisplay(info_key + "h0000[6]", std::to_string(agent_info->h0000[6]));
        DebugDisplay::PushToDisplay(info_key + "h0000[7]", std::to_string(agent_info->h0000[7]));
        DebugDisplay::PushToDisplay(info_key + "h0000[8]", std::to_string(agent_info->h0000[8]));
        DebugDisplay::PushToDisplay(info_key + "h0000[9]", std::to_string(agent_info->h0000[9]));
        DebugDisplay::PushToDisplay(info_key + "h0000[10]", std::to_string(agent_info->h0000[10]));
        DebugDisplay::PushToDisplay(info_key + "h0000[11]", std::to_string(agent_info->h0000[11]));
        DebugDisplay::PushToDisplay(info_key + "h0000[12]", std::to_string(agent_info->h0000[12]));

        const auto *agent_summary_info = Utils::GetAgentSummaryInfoByID(agent_id);
        if (agent_summary_info == nullptr)
            return;

        auto summary_key = key + "agent_summary_info.";

        DebugDisplay::PushToDisplay(summary_key + "h0000", std::to_string(agent_summary_info->h0000));
        DebugDisplay::PushToDisplay(summary_key + "h0004", std::to_string(agent_summary_info->h0004));

        const auto *agent_summary_info_sub = agent_summary_info->extra_info_sub;
        if (agent_summary_info_sub == nullptr)
            return;

        DebugDisplay::PushToDisplay(summary_key + "extra_info_sub.h0000", std::to_string(agent_summary_info_sub->h0000));
        DebugDisplay::PushToDisplay(summary_key + "extra_info_sub.h0004", std::to_string(agent_summary_info_sub->h0004));
        DebugDisplay::PushToDisplay(summary_key + "extra_info_sub.gadget_id", std::to_string(agent_summary_info_sub->gadget_id));
        DebugDisplay::PushToDisplay(summary_key + "extra_info_sub.h000C", std::to_string(agent_summary_info_sub->h000C));
        DebugDisplay::PushToDisplay(summary_key + "extra_info_sub.gadget_name", Utils::DecodeString(agent_summary_info_sub->gadget_name_enc));
        DebugDisplay::PushToDisplay(summary_key + "extra_info_sub.h0014", std::to_string(agent_summary_info_sub->h0014));
        DebugDisplay::PushToDisplay(summary_key + "extra_info_sub.composite_agent_id", std::to_string(agent_summary_info_sub->composite_agent_id));
    }

    void DebugTarget()
    {
        DebugDisplay::ClearDisplay("target");
        ListTargetEffects();
        ListTargetBuffs();
        ListTargetVisibleEffects();
        DisplayTargetModifiers(0);
        DisplayTargetModifiers(1);
        DisplayTargetUnknownFields();

        const auto target_id = GW::Agents::GetTargetId();
        DebugDisplay::PushToDisplay("target_id", std::to_string(target_id));

        const auto agent = GW::Agents::GetAgentByID(target_id);
        if (agent == nullptr)
            return;

        const auto living_agent = agent->GetAsAgentLiving();
        if (living_agent == nullptr)
            return;

        DebugDisplay::PushToDisplay("target weapon type", living_agent->weapon_type);
        DebugDisplay::PushToDisplay("target weapon item type", living_agent->weapon_item_type);
        DebugDisplay::PushToDisplay("target offhand item type", living_agent->offhand_item_type);
        DebugDisplay::PushToDisplay("target pet agent_id", Utils::GetPetOfAgent(target_id));
        DebugDisplay::PushToDisplay("target owner id", living_agent->owner);
        DebugDisplay::PushToDisplay("target owner", Utils::GetAgentName(living_agent->owner));
        DebugDisplay::PushToDisplay("target visual effects count", std::to_string(living_agent->visual_effects));
        DebugDisplay::PushToDisplay("target hp", std::to_string(living_agent->hp));
        auto current_hp = (uint32_t)std::round(living_agent->hp * (float)living_agent->max_hp);
        auto missing_hp = living_agent->max_hp - current_hp;
        DebugDisplay::PushToDisplay("target current_hp", std::to_string(current_hp));
        DebugDisplay::PushToDisplay("target missing_hp", std::to_string(missing_hp));
        DebugDisplay::PushToDisplay("target max_hp", std::to_string(living_agent->max_hp));
        DebugDisplay::PushToDisplay("target energy", std::to_string(living_agent->energy));
        DebugDisplay::PushToDisplay("target energy_regen", std::to_string(living_agent->energy_regen));
        DebugDisplay::PushToDisplay("target max_energy", std::to_string(living_agent->max_energy));
        DebugDisplay::PushToDisplay("target effect bitmap", Utils::UInt32ToBinaryStr(living_agent->effects));
        DebugDisplay::PushToDisplay("target type_map", Utils::UInt32ToBinaryStr(living_agent->type_map));
        DebugDisplay::PushToDisplay("target hex bitmap", Utils::UInt32ToBinaryStr(uint32_t(living_agent->hex) & 0xFF));

        if (living_agent->tags)
        {
            DebugDisplay::PushToDisplay("target tags guild id", living_agent->tags->guild_id);
            DebugDisplay::PushToDisplay("target tags level", living_agent->tags->level);
            DebugDisplay::PushToDisplay("target tags primary", living_agent->tags->primary);
            DebugDisplay::PushToDisplay("target tags secondary", living_agent->tags->secondary);
        }

        auto map_agent = GW::Agents::GetMapAgentByID(target_id);
        if (map_agent)
        {
            DebugDisplay::PushToDisplay("target map agent effect bitmap", Utils::UInt32ToBinaryStr(map_agent->effects));
            DebugDisplay::PushToDisplay("target map agent skill timestamp", map_agent->skill_timestamp);
            DebugDisplay::PushToDisplay("target map agent h0010", map_agent->h0010);
            DebugDisplay::PushToDisplay("target map agent h0018", map_agent->h0018);
            DebugDisplay::PushToDisplay("target map agent h001C", map_agent->h001C);
            DebugDisplay::PushToDisplay("target map agent h002C", map_agent->h002C);
        }

        std::string allegiance_str;
        // clang-format off
        switch (living_agent->allegiance) {
            case GW::Constants::Allegiance::Ally_NonAttackable: allegiance_str = "Ally_NonAttackable"; break;
            case GW::Constants::Allegiance::Neutral: allegiance_str = "Neutral"; break;
            case GW::Constants::Allegiance::Enemy: allegiance_str = "Enemy"; break;
            case GW::Constants::Allegiance::Spirit_Pet: allegiance_str = "Spirit_Pet"; break;
            case GW::Constants::Allegiance::Minion: allegiance_str = "Minion"; break;
            case GW::Constants::Allegiance::Npc_Minipet: allegiance_str = "Npc_Minipet"; break;
            default: allegiance_str = "Unknown"; break;
        }
        // clang-format on
        DebugDisplay::PushToDisplay("target allegiance", allegiance_str);

        DebugDisplay::PushToDisplay("target is bleeding", std::to_string(living_agent->GetIsBleeding()));
    }

    void DebugPreGameContext()
    {
        DebugDisplay::ClearDisplay("PreGame context");
        auto *pgc = GW::GetPreGameContext();
        if (pgc == nullptr)
            return;

        DebugDisplay::PushToDisplay(L"PreGame context chosen_character_index: {}", pgc->chosen_character_index);
    }

    void DebugGameplayContext()
    {
        DebugDisplay::ClearDisplay("Gameplay context");
        auto *gpc = GW::GetGameplayContext();
        if (gpc == nullptr)
            return;

        DebugDisplay::PushToDisplay(L"Gameplay context mission_map_zoom: {}", gpc->mission_map_zoom);

        for (uint32_t i = 0; i < std::size(gpc->h0000); i++)
            DebugDisplay::PushToDisplay(L"Gameplay context h0000[{}]: {}", i, gpc->h0000[i]);
        for (uint32_t i = 0; i < std::size(gpc->unk); i++)
            DebugDisplay::PushToDisplay(L"Gameplay context unk[{}]: {}", i, gpc->unk[i]);
    }

    void DebugItemContext()
    {
        DebugDisplay::ClearDisplay("Item context");
        auto *ic = GW::GetItemContext();
        if (ic == nullptr)
            return;

        DebugDisplay::PushToDisplay(L"Item context h0020: {}", ic->h0020);

        for (uint32_t i = 0; i < std::size(ic->h0034); i++)
            DebugDisplay::PushToDisplay(L"Item context h0034[{}]: {}", i, ic->h0034[i]);
        for (uint32_t i = 0; i < std::size(ic->h0060); i++)
            DebugDisplay::PushToDisplay(L"Item context h0060[{}]: {}", i, ic->h0060[i]);
        for (uint32_t i = 0; i < std::size(ic->h00C8); i++)
            DebugDisplay::PushToDisplay(L"Item context h00C8[{}]: {}", i, ic->h00C8[i]);

        DebugDisplay::PushToDisplay(L"Item context inventory->h0060: {}", ic->inventory->h0060);
        DebugDisplay::PushToDisplay(L"Item context inventory->h0088[0]: {}", ic->inventory->h0088[0]);
        DebugDisplay::PushToDisplay(L"Item context inventory->h0088[1]: {}", ic->inventory->h0088[1]);
    }

    void DebugGameContext()
    {
        DebugDisplay::ClearDisplay("Game context");
        auto *gc = GW::GetGameContext();
        if (gc == nullptr)
            return;

        DebugDisplay::PushToDisplay(L"Game context some_number: {}", gc->some_number);
    }

    void DebugPartyContext()
    {
        DebugDisplay::ClearDisplay("Party context");
        auto *pc = GW::GetPartyContext();

        DebugDisplay::PushToDisplay(L"Party context h0000: {}", pc->h0000);
        DebugDisplay::PushToDisplay(L"Party context flag: {}", pc->flag);
        DebugDisplay::PushToDisplay(L"Party context h0018: {}", pc->h0018);
        DebugDisplay::PushToDisplay(L"Party context h003C: {}", pc->h003C);
        DebugDisplay::PushToDisplay(L"Party context h0050: {}", pc->h0050);

        for (uint32_t i = 0; i < std::size(pc->h0058); i++)
            DebugDisplay::PushToDisplay(L"Party context h0058[{}]: {}", i, pc->h0058[i]);
    }

    void DebugCharContext()
    {
        DebugDisplay::ClearDisplay("Char context");
        auto *cc = GW::GetCharContext();

        DebugDisplay::PushToDisplay(L"Char context token1: {}", cc->token1);
        DebugDisplay::PushToDisplay(L"Char context token2: {}", cc->token2);

        DebugDisplay::PushToDisplay(L"Char context h0010: {}", cc->h0010);
        DebugDisplay::PushToDisplay(L"Char context h0024[0]: {}", cc->h0024[0]);
        DebugDisplay::PushToDisplay(L"Char context h0024[1]: {}", cc->h0024[1]);
        DebugDisplay::PushToDisplay(L"Char context h0024[2]: {}", cc->h0024[2]);
        DebugDisplay::PushToDisplay(L"Char context h0024[3]: {}", cc->h0024[3]);
        DebugDisplay::PushToDisplay(L"Char context h0054[0]: {}", cc->h0054[0]);
        DebugDisplay::PushToDisplay(L"Char context h0054[1]: {}", cc->h0054[1]);
        DebugDisplay::PushToDisplay(L"Char context h0054[2]: {}", cc->h0054[2]);
        DebugDisplay::PushToDisplay(L"Char context h0054[3]: {}", cc->h0054[3]);

        for (uint32_t i = 0; i < std::size(cc->h009C); i++)
            DebugDisplay::PushToDisplay(L"Char context h009C[{}]: {}", i, cc->h009C[i]);

        for (uint32_t i = 0; i < std::size(cc->h00FC); i++)
            DebugDisplay::PushToDisplay(L"Char context h00FC[{}]: {}", i, cc->h00FC[i]);

        DebugDisplay::PushToDisplay(L"Char context world_flags: {}", cc->world_flags);

        for (uint32_t i = 0; i < std::size(cc->h01BC); i++)
            DebugDisplay::PushToDisplay(L"Char context h01BC[{}]: {}", i, cc->h01BC[i]);

        DebugDisplay::PushToDisplay(L"Char context player_flags: {}", cc->player_flags);
        DebugDisplay::PushToDisplay(L"Char context player_number: {}", cc->player_number);

        for (uint32_t i = 0; i < std::size(cc->h0248); i++)
            DebugDisplay::PushToDisplay(L"Char context h0248[{}]: {}", i, cc->h0248[i]);
        for (uint32_t i = 0; i < std::size(cc->h02A8); i++)
            DebugDisplay::PushToDisplay(L"Char context h02A8[{}]: {}", i, cc->h02A8[i]);
        for (uint32_t i = 0; i < std::size(cc->h034C); i++)
            DebugDisplay::PushToDisplay(L"Char context h034C[{}]: {}", i, cc->h034C[i]);
    }

    void DebugWorldContext()
    {
        DebugDisplay::ClearDisplay("World context");
        const auto *wc = GW::GetWorldContext();
        if (wc == nullptr)
            return;

        DebugDisplay::PushToDisplay(L"World context h0054: {}", wc->h0054);
        DebugDisplay::PushToDisplay(L"World context h005C[0]: {}", wc->h005C[0]);
        DebugDisplay::PushToDisplay(L"World context h005C[1]: {}", wc->h005C[1]);
        DebugDisplay::PushToDisplay(L"World context h005C[2]: {}", wc->h005C[2]);
        DebugDisplay::PushToDisplay(L"World context h005C[3]: {}", wc->h005C[3]);
        DebugDisplay::PushToDisplay(L"World context h005C[4]: {}", wc->h005C[4]);
        DebugDisplay::PushToDisplay(L"World context h005C[5]: {}", wc->h005C[5]);
        DebugDisplay::PushToDisplay(L"World context h005C[6]: {}", wc->h005C[6]);
        DebugDisplay::PushToDisplay(L"World context h005C[7]: {}", wc->h005C[7]);
        DebugDisplay::PushToDisplay(L"World context h061C[0]: {}", wc->h061C[0]);
        DebugDisplay::PushToDisplay(L"World context h061C[1]: {}", wc->h061C[1]);
        DebugDisplay::PushToDisplay(L"World context h00A8: {}", wc->h00A8);
        for (uint32_t i = 0; i < 255; i++)
        {
            DebugDisplay::PushToDisplay(L"World context h00BC[{}]: {}", i, wc->h00BC[i]);
        }
        DebugDisplay::PushToDisplay(L"World context h04D8: {}", wc->h04D8);
        DebugDisplay::PushToDisplay(L"World context h04EC[0]: {}", wc->h04EC[0]);
        DebugDisplay::PushToDisplay(L"World context h04EC[1]: {}", wc->h04EC[1]);
        DebugDisplay::PushToDisplay(L"World context h04EC[2]: {}", wc->h04EC[2]);
        DebugDisplay::PushToDisplay(L"World context h04EC[3]: {}", wc->h04EC[3]);
        DebugDisplay::PushToDisplay(L"World context h04EC[4]: {}", wc->h04EC[4]);
        DebugDisplay::PushToDisplay(L"World context h04EC[5]: {}", wc->h04EC[5]);
        DebugDisplay::PushToDisplay(L"World context h04EC[6]: {}", wc->h04EC[6]);

        DebugDisplay::PushToDisplay(L"World context h053C[0]: {}", wc->h053C[0]);
        DebugDisplay::PushToDisplay(L"World context h053C[1]: {}", wc->h053C[1]);
        DebugDisplay::PushToDisplay(L"World context h053C[2]: {}", wc->h053C[2]);
        DebugDisplay::PushToDisplay(L"World context h053C[3]: {}", wc->h053C[3]);
        DebugDisplay::PushToDisplay(L"World context h053C[4]: {}", wc->h053C[4]);
        DebugDisplay::PushToDisplay(L"World context h053C[5]: {}", wc->h053C[5]);
        DebugDisplay::PushToDisplay(L"World context h053C[6]: {}", wc->h053C[6]);
        DebugDisplay::PushToDisplay(L"World context h053C[7]: {}", wc->h053C[7]);
        DebugDisplay::PushToDisplay(L"World context h053C[8]: {}", wc->h053C[8]);
        DebugDisplay::PushToDisplay(L"World context h053C[9]: {}", wc->h053C[9]);

        DebugDisplay::PushToDisplay(L"World context h05B4[0]: {}", wc->h05B4[0]);
        DebugDisplay::PushToDisplay(L"World context h05B4[1]: {}", wc->h05B4[1]);
        DebugDisplay::PushToDisplay(L"World context h061C[0]: {}", wc->h061C[0]);
        DebugDisplay::PushToDisplay(L"World context h061C[1]: {}", wc->h061C[1]);

        DebugDisplay::PushToDisplay(L"World context h028C: {}", wc->h028C);

        DebugDisplay::PushToDisplay(L"World context h063C[0]: {}", wc->h063C[0]);
        DebugDisplay::PushToDisplay(L"World context h063C[1]: {}", wc->h063C[1]);
        DebugDisplay::PushToDisplay(L"World context h063C[2]: {}", wc->h063C[2]);
        DebugDisplay::PushToDisplay(L"World context h063C[3]: {}", wc->h063C[3]);
        DebugDisplay::PushToDisplay(L"World context h063C[4]: {}", wc->h063C[4]);
        DebugDisplay::PushToDisplay(L"World context h063C[5]: {}", wc->h063C[5]);
        DebugDisplay::PushToDisplay(L"World context h063C[6]: {}", wc->h063C[6]);
        DebugDisplay::PushToDisplay(L"World context h063C[7]: {}", wc->h063C[7]);
        DebugDisplay::PushToDisplay(L"World context h063C[8]: {}", wc->h063C[8]);
        DebugDisplay::PushToDisplay(L"World context h063C[9]: {}", wc->h063C[9]);
        DebugDisplay::PushToDisplay(L"World context h063C[10]: {}", wc->h063C[10]);
        DebugDisplay::PushToDisplay(L"World context h063C[11]: {}", wc->h063C[11]);
        DebugDisplay::PushToDisplay(L"World context h063C[12]: {}", wc->h063C[12]);
        DebugDisplay::PushToDisplay(L"World context h063C[13]: {}", wc->h063C[13]);
        DebugDisplay::PushToDisplay(L"World context h063C[14]: {}", wc->h063C[14]);
        DebugDisplay::PushToDisplay(L"World context h063C[15]: {}", wc->h063C[15]);

        DebugDisplay::PushToDisplay(L"World context h0688[0]: {}", wc->h0688[0]);
        DebugDisplay::PushToDisplay(L"World context h0688[1]: {}", wc->h0688[1]);
        DebugDisplay::PushToDisplay(L"World context h0694[0]: {}", wc->h0694[0]);
        DebugDisplay::PushToDisplay(L"World context h0694[1]: {}", wc->h0694[1]);
        DebugDisplay::PushToDisplay(L"World context h0694[2]: {}", wc->h0694[2]);
        DebugDisplay::PushToDisplay(L"World context h0694[3]: {}", wc->h0694[3]);
        DebugDisplay::PushToDisplay(L"World context h0694[4]: {}", wc->h0694[4]);
        DebugDisplay::PushToDisplay(L"World context h06DC: {}", wc->h06DC);
        DebugDisplay::PushToDisplay(L"World context player_number: {}", wc->player_number);

        for (auto &ps : wc->party_profession_states)
        {
            auto agent_id = ps.agent_id;
            auto unk = ps.unk;
            DebugDisplay::PushToDisplay(L"World context prof state, agent_id: {}, unk: {}", agent_id, unk);
        }

        for (auto &hf : wc->hero_flags)
        {
            auto agent_id = hf.agent_id;
            auto h0018 = hf.h0018;
            auto h0020 = hf.h0020;
            DebugDisplay::PushToDisplay(L"World context prof state, agent_id: {}, unk1: {}, unk2: {}", agent_id, h0018, h0020);
        }

        for (auto &hi : wc->hero_info)
        {
            auto agent_id = hi.agent_id;

            uint32_t i = 0;
            for (auto unk : hi.h001C)
            {
                DebugDisplay::PushToDisplay(L"World context hero info, agent_id: {}, unk[{}]: {}", agent_id, i, unk);
                i++;
            }
        }
    }

    struct ArrayScanResult
    {
        bool success;
        uint32_t element_index;
        uint32_t element_offset;
    };

    ArrayScanResult ScanInLivingAgents(const char *pattern, uint32_t align, std::function<bool(GW::AgentLiving *)> predicate)
    {
        const auto *agents = GW::Agents::GetAgentArray();
        if (agents == nullptr)
            return {false, 0, 0};

        for (size_t i = 0; i < agents->size(); i++)
        {
            auto agent_ptr = agents->at(i);
            if (agent_ptr == nullptr)
                continue;

            auto living_agent = agent_ptr->GetAsAgentLiving();
            if (living_agent == nullptr)
                continue;

            if (predicate && !predicate(living_agent))
                continue;

            uintptr_t start_address = reinterpret_cast<uintptr_t>(living_agent);
            uintptr_t end_address = start_address + sizeof(GW::AgentLiving);
            uintptr_t address = GW::Scanner::FindInRange(pattern, nullptr, 0, start_address, end_address);

            if (address == 0)
                continue;

            if (align != 0 && address % align != 0)
                continue;

            auto offset = address - start_address;

            ArrayScanResult result = {true, i, offset};

            return result;
        }

        return {false, 0, 0};
    }

    ArrayScanResult ScanInParties(const char *pattern, uint32_t align, std::function<bool(GW::PartyInfo *)> predicate)
    {
        const auto *pc = GW::GetPartyContext();
        if (pc == nullptr)
            return {false, 0, 0};

        const GW::Array<GW::PartyInfo *> *parties = &pc->parties;
        if (parties == nullptr)
            return {false, 0, 0};

        for (size_t i = 0; i < parties->size(); i++)
        {
            auto party_info = parties->at(i);

            if (party_info == nullptr)
                continue;

            if (predicate && !predicate(party_info))
                continue;

            uintptr_t start_address = reinterpret_cast<uintptr_t>(party_info);
            uintptr_t end_address = start_address + sizeof(GW::PartyInfo);
            uintptr_t address = GW::Scanner::FindInRange(pattern, nullptr, 0, start_address, end_address);

            if (address == 0)
                continue;

            if (align != 0 && address % align != 0)
                continue;

            auto offset = address - start_address;

            ArrayScanResult result = {true, i, offset};

            return result;
        }

        return {false, 0, 0};
    }

    void DebugMarkedTarget()
    {
        auto p_eff = GW::Effects::GetPartyEffectsArray();
        if (p_eff == nullptr)
            return;

        for (auto &agent_effects : *p_eff)
        {
            for (auto &effect : agent_effects.effects)
            {
                if (effect.skill_id == GW::Constants::SkillID::Marked_For_Death)
                {
                    Utils::FormatToChat(L"Marked target: {}", Utils::GetAgentName(agent_effects.agent_id));
                }
            }
        }
    }

    void DisplaySoICharges()
    {
        auto player_id = GW::Agents::GetControlledCharacterId();

        auto &cad = CustomAgentDataModule::GetCustomAgentData(player_id);
        DebugDisplay::PushToDisplay("SoI charges", std::to_string(cad.signet_of_illusions_charges));
    }

    std::map<uint32_t, std::wstring> frame_id_to_label;
    std::map<uint32_t, std::wstring> frame_id_to_name;
    void OnCreateUIComponent(GW::UI::CreateUIComponentPacket *packet)
    {
        if (!packet)
            return;

        auto str = packet->component_label != nullptr ? packet->component_label : L"";
        // if (!str)
        //     return;

        auto frame_id = packet->frame_id;
        auto enc_name = packet->name_enc;

        frame_id_to_label[frame_id] = str;
        // if (enc_name > (wchar_t *)0x100000)
        //     frame_id_to_name[frame_id] = Utils::DecodeString(enc_name);

        OutputDebugStringW(L"Frame created: ");
        OutputDebugStringW(std::to_wstring(frame_id).c_str());
        OutputDebugStringW(L", label: ");
        OutputDebugStringW(str);
        OutputDebugStringW(L", component_flags: ");
        OutputDebugStringW(std::to_wstring(packet->component_flags).c_str());
        OutputDebugStringW(L"\n");
    }

    void OnFrameClick(GW::HookStatus *, const GW::UI::Frame *frame, GW::UI::UIMessage msg, void *wParam, void *lParam)
    {
        if (frame)
        {
            auto frame_id = frame->frame_id;
            Utils::FormatToChat(L"Frame clicked: {}, label: {}, name: {}, msg: {}, type: {}, child_offset_id: {}",
                frame_id,
                frame_id_to_label[frame_id],
                frame_id_to_name[frame_id],
                static_cast<uint32_t>(msg),
                frame->type,
                frame->child_offset_id);
        }
    }

    static GW::HookEntry OnUIMessage_Entry;
    GW::HookEntry debug_hook_entry;
    void Initialize()
    {
        // GW::UI::RegisterCreateUIComponentCallback(&debug_hook_entry, OnCreateUIComponent);
        // GW::UI::RegisterFrameUIMessageCallback(&debug_hook_entry, GW::UI::UIMessage::kMouseClick, OnFrameClick);

        // SearchForEncodedString();
    }

    void Terminate()
    {
        GW::UI::RemoveCreateUIComponentCallback(&debug_hook_entry);
        GW::UI::RemoveFrameUIMessageCallback(&debug_hook_entry);
    }

    static std::vector<std::wstring> decoded;
    void Update()
    {
        DebugTooltip();
        DisplaySoICharges();
        DebugMarkedTarget();
        DebugHoveredSkill();
        DebugTarget();

        // DebugGameplayContext();
        // DebugItemContext();
        // DebugPreGameContext();
        // DebugGameContext();
        // DebugPartyContext();
        // DebugCharContext();
        // DebugWorldContext();

        DisplayHeroInfo(0);
        DisplayHeroInfo(1);
        DisplayHeroInfo(2);
        DisplayHeroInfo(3);
        DisplayHeroInfo(4);
        DisplayHeroInfo(5);

        const auto seconds_til_contact = Utils::SecondsTilContact(0);
        DebugDisplay::PushToDisplay("seconds_til_contact", std::to_string(seconds_til_contact));
    }
}

#endif // _DEBUG
