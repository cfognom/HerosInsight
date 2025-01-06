#include <format>

#include <GWCA/GWCA.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/List.h>

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
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>

#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <debug_display.h>
#include <hero_ai.h>
#include <party_data.h>
#include <update_manager.h>
#include <utils.h>

#include "effect_tracking.h"

#ifdef _DEBUG
// #define DEBUG_EFFECT_LIFETIME
#endif

namespace HerosInsight::EffectTracking
{
    std::unordered_map<uint32_t, AgentEffectTrackers> agent_trackers;

    // effect_ids to remove after update
    std::unordered_map<uint32_t, std::vector<uint32_t>> scheduled_removals;

    uint32_t effect_added_counter = 0;

    void ApplySkillEffect(uint32_t target_id, uint32_t cause_id, SkillEffect effect)
    {
        if (Utils::ReceivesStoCEffects(target_id))
        {
            return;
        }

        SOFT_ASSERT(effect.base_duration);

        auto duration = Utils::CalculateDuration(
            *GW::SkillbarMgr::GetSkillConstantData(effect.effect_skill_id),
            effect.base_duration,
            cause_id //
        );

        EffectTracking::EffectTracker new_tracker = {};
        new_tracker.cause_agent_id = cause_id;
        new_tracker.skill_id = effect.effect_skill_id;
        new_tracker.duration_sec = duration;
        EffectTracking::AddTracker(target_id, new_tracker);
    }

    void ApplySkillEffects(uint32_t target_id, uint32_t cause_id, std::span<SkillEffect> effects)
    {
        for (const auto &effect : effects)
        {
            ApplySkillEffect(target_id, cause_id, effect);
        }
    }

    void AddTracker(uint32_t agent_id, EffectTracker new_effect)
    {
#ifdef DEBUG_EFFECT_LIFETIME
        // new_effect.duration_sec += 1; // Add a small buffer so that it looses signatures before it times out (helps with debugging)
        Utils::FormatToChat(0xFFFFFF00, L"Adding effect for agent {}, caster {}, duration: {}, skill {}", agent_id, new_effect.cause_agent_id, new_effect.duration_sec, Utils::GetSkillName(new_effect.skill_id));
#endif

#ifdef _DEBUG
        if (Utils::ReceivesStoCEffects(agent_id))
        {
            assert(new_effect.effect_id);
        }
#endif

        const auto &skill = *GW::SkillbarMgr::GetSkillConstantData(new_effect.skill_id);
        const auto skill_type = skill.type;

        if (skill_type == GW::Constants::SkillType::Preparation ||
            skill_type == GW::Constants::SkillType::Stance ||
            skill_type == GW::Constants::SkillType::PetAttack ||
            skill_type == GW::Constants::SkillType::WeaponSpell ||
            skill_type == GW::Constants::SkillType::ItemSpell ||
            skill_type == GW::Constants::SkillType::Form ||
            skill_type == GW::Constants::SkillType::Disguise)
        {
            // There can only be one of this type active at a time

            // clang-format off
            EffectTracking::RemoveTrackers(agent_id, [=](EffectTracking::EffectTracker &tracker)
            {
                return GW::SkillbarMgr::GetSkillConstantData(tracker.skill_id)->type == skill_type;
            });
            // clang-format on
        }

        if (!new_effect.begin_timestamp)
        {
            auto timestamp_now = GW::MemoryMgr::GetSkillTimer();
            new_effect.begin_timestamp = timestamp_now;
        }

        auto &effects = agent_trackers[agent_id].effects;

        new_effect.unique_id = effect_added_counter++;
        new_effect.is_active = true;

        for (auto &existing_effect : effects)
        {
            if (existing_effect.skill_id == new_effect.skill_id && existing_effect.is_active)
            {
                if (existing_effect.attribute_level < new_effect.attribute_level ||
                    (existing_effect.attribute_level == new_effect.attribute_level &&
                        existing_effect.cause_agent_id == new_effect.cause_agent_id))
                {
                    existing_effect.is_active = false;
                }
                else
                {
                    new_effect.is_active = false;
                }
            }
        }

        effects.push_back(new_effect);
    }

    std::vector<EffectTracker>::iterator RemoveAndElectNew(std::vector<EffectTracker> &effects, std::vector<EffectTracker>::iterator effect_it)
    {
#ifdef DEBUG_EFFECT_LIFETIME
        Utils::FormatToChat(0xFFFFFF00, L"Removing effect for agent {}, skill {}", effect_it->cause_agent_id, Utils::GetSkillName(effect_it->skill_id));
#endif

        const auto skill_id = effect_it->skill_id;
        const bool was_active = effect_it->is_active;

        // Erase the effect using its pointer directly
        effect_it = effects.erase(effect_it);

        if (was_active) // Removed active effect
        {
            EffectTracker *candidate_effect = nullptr;

            for (auto &check_effect : effects)
            {
                if (check_effect.skill_id == skill_id &&
                    // Because the effects are sorted by timestap; in case of ties, we get the oldest effect
                    (!candidate_effect || check_effect.attribute_level > candidate_effect->attribute_level))
                {
                    candidate_effect = &check_effect;
                }
            }

            if (candidate_effect)
            {
                SOFT_ASSERT(!candidate_effect->is_active, L"Unexpected multiple active effects for the same skill");
                candidate_effect->is_active = true;
            }
        }

        return effect_it;
    }

    void RemoveTrackers(uint32_t agent_id, std::function<bool(EffectTracker &)> predicate)
    {
        auto &agent_tracking = agent_trackers[agent_id];
        auto &effects = agent_tracking.effects;

        for (auto effect_it = effects.begin(); effect_it < effects.end();)
        {
            if (predicate(*effect_it))
            {
                effect_it = RemoveAndElectNew(effects, effect_it);
            }
            else
            {
                ++effect_it;
            }
        }
    }

    void SpendCharge(uint32_t agent_id, GW::Constants::SkillID effect_skill_id)
    {
        auto &agent_tracking = agent_trackers[agent_id];
        auto &effects = agent_tracking.effects;

        for (auto effect_it = effects.begin(); effect_it < effects.end();)
        {
            if (effect_it->skill_id == effect_skill_id)
            {
                if (effect_it->charges > 0)
                    effect_it->charges--;

                if (effect_it->charges == 0)
                {
                    effect_it = RemoveAndElectNew(effects, effect_it);
                }
                else
                {
                    ++effect_it;
                }
            }
        }
    }

    void ScheduleTrackerRemoval(uint32_t agent_id, uint32_t effect_id)
    {
        auto effects = GetTrackerSpan(agent_id);
        for (auto &effect : effects)
        {
            if (effect.effect_id == effect_id)
            {
                scheduled_removals[agent_id].push_back(effect.unique_id);
            }
        }
    }

    std::span<EffectTracker> GetTrackerSpan(uint32_t agent_id)
    {
        auto it = agent_trackers.find(agent_id);
        if (it == agent_trackers.end())
        {
            return {};
        }
        return it->second.effects;
    }

    AgentEffectTrackers &GetTrackers(uint32_t agent_id)
    {
        return agent_trackers[agent_id];
    }

    EffectTracker *GetEffectBySkillID(uint32_t agent_id, GW::Constants::SkillID skill_id)
    {
        auto &agent_tracking = agent_trackers[agent_id];
        auto &effects = agent_tracking.effects;

        for (auto &effect : effects)
        {
            if (effect.skill_id == skill_id)
            {
                return &effect;
            }
        }

        return nullptr;
    }

    void Reset()
    {
        agent_trackers.clear();
        scheduled_removals.clear();
    }

    bool EffectTimedOut(GW::AgentLiving *agent, EffectTracker &effect, DWORD timestamp_now)
    {
        const auto skill_id = effect.skill_id;
        const auto end_timestamp = effect.GetEndTimestamp();
        if (end_timestamp && timestamp_now >= end_timestamp)
        {
            if (skill_id == GW::Constants::SkillID::Enduring_Toxin &&
                agent && (agent->velocity.x != 0.f || agent->velocity.y != 0.f))
            {
                effect.begin_timestamp += effect.duration_sec * 1000;
                return false;
            }

            return true;
        }

        return false;
    }

    bool AgentHasEffectSignatures(GW::AgentLiving &agent, EffectTracker &effect)
    {
        const auto skill_id = effect.skill_id;
        const auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);
        const auto &skill = *custom_sd.skill;

        // clang-format off
        switch (skill.type) {
            case GW::Constants::SkillType::Condition: {
                if (!agent.GetIsConditioned())
                    return false;
                
                switch (skill_id) {
                    case GW::Constants::SkillID::Bleeding:      if (!agent.GetIsBleeding())                                                                      return false; break;
                    case GW::Constants::SkillID::Burning:       if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::burning))                           return false; break;
                    case GW::Constants::SkillID::Poison:        if (!agent.GetIsPoisoned() || !Utils::HasVisibleEffect(agent, GW::Constants::EffectID::poison))  return false; break;
                    case GW::Constants::SkillID::Disease:       if (!agent.GetIsPoisoned() || !Utils::HasVisibleEffect(agent, GW::Constants::EffectID::disease)) return false; break;
                    case GW::Constants::SkillID::Dazed:         if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::dazed))                             return false; break;
                    case GW::Constants::SkillID::Blind:         if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::blind))                             return false; break;
                    case GW::Constants::SkillID::Weakness:
                    case GW::Constants::SkillID::Cracked_Armor: if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::weakness))                          return false; break;
                    case GW::Constants::SkillID::Crippled:      if (!agent.GetIsCrippled())                                                                      return false; break;
                    case GW::Constants::SkillID::Deep_Wound:    if (!agent.GetIsDeepWounded())                                                                   return false; break;
                }
                break;
            }

            case GW::Constants::SkillType::Enchantment: if (!agent.GetIsEnchanted())     return false; break;
            case GW::Constants::SkillType::Hex:         if (!agent.GetIsHexed())         return false; break;
            case GW::Constants::SkillType::WeaponSpell: if (!agent.GetIsWeaponSpelled()) return false; break;
            case GW::Constants::SkillType::Stance:                                       return true; // There is no way to check for stances
        }
        // clang-format on

        if ((skill.special & (uint32_t)Utils::SkillSpecialFlags::Degen) && !agent.GetIsDegenHexed())
            return true;

        // clang-format off
        switch (skill.profession) {
            case GW::Constants::ProfessionByte::Elementalist: if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::ele_symbol))       return false; break;
            case GW::Constants::ProfessionByte::Monk:         if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::monk_symbol))      return false; break;
            case GW::Constants::ProfessionByte::Necromancer:  if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::necro_symbol))     return false; break;
            case GW::Constants::ProfessionByte::Mesmer:       if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::mesmer_symbol))    return false; break;
            case GW::Constants::ProfessionByte::Assassin:     if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::assasin_symbol))   return false; break;
            case GW::Constants::ProfessionByte::Dervish:      if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::dervish_symbol))   return false; break;
            case GW::Constants::ProfessionByte::Ritualist:    if (!Utils::HasVisibleEffect(agent, GW::Constants::EffectID::ritualist_symbol)) return false; break;
        }
        // clang-format on

        return true;
    }

#ifdef _DEBUG
    void DebugTrackedEffects()
    {
        DebugDisplay::ClearDisplay("Tracked effects");
        for (const auto &[agent_id, agent_tracker] : agent_trackers)
        {
            uint32_t i = 0;
            for (const auto &effect : agent_tracker.effects)
            {
                DebugDisplay::PushToDisplay(L"Tracked effects for Agent {} [{}], effect {}, skill {}", agent_id, i++, effect.effect_id, Utils::GetSkillName(effect.skill_id));
            }
        }
    }
#endif

    void Update()
    {
#ifdef _DEBUG
        DebugTrackedEffects();
#endif

        auto timestamp_now = GW::MemoryMgr::GetSkillTimer();

        for (auto it = agent_trackers.begin(); it != agent_trackers.end();)
        {
            const auto agent_id = it->first;
            auto &agent_tracker = it->second;
            auto &effects = agent_tracker.effects;

            const auto agent = Utils::GetAgentLivingByID(agent_id);

            if (agent && (!agent->GetIsAlive() || agent->GetIsDeadByTypeMap()))
            {
                // Utils::FormatToChat(L"Removing dead agent {}", agent_id);
                it = agent_trackers.erase(it);
                continue;
            }

            if (Utils::ReceivesStoCEffects(agent_id)) // We receive effect added/removed events for this agent and don't need to manually track effects
            {
                it++;
                continue;
            }

            auto EffectIsValid = [&](EffectTracker &effect)
            {
                if (EffectTimedOut(agent, effect, timestamp_now))
                {
#ifdef DEBUG_EFFECT_LIFETIME
                    Utils::FormatToChat(0xFFFFFF00, L"Effect timed out: skill {}", Utils::GetSkillName(effect.skill_id));
#endif
                    return false;
                }

                if (agent)
                {
                    bool has_signatures = AgentHasEffectSignatures(*agent, effect);

                    if (!effect.observed_timestamp && has_signatures)
                    {
#ifdef DEBUG_EFFECT_LIFETIME
                        Utils::FormatToChat(0xFFFFFF00, L"Effect gained signatures: skill {}", Utils::GetSkillName(effect.skill_id));
#endif
                        effect.observed_timestamp = timestamp_now;
                    }

                    if (effect.observed_timestamp && !has_signatures)
                    {
#ifdef DEBUG_EFFECT_LIFETIME
                        auto timestamp_now = GW::MemoryMgr::GetSkillTimer();
                        auto rem_sec = (float)(effect.GetEndTimestamp() - timestamp_now) / 1000.f;
                        Utils::FormatToChat(0xFFFFFF00, L"Effect lost signatures: skill {}, rem_sec {}", Utils::GetSkillName(effect.skill_id), rem_sec);
                        if (rem_sec > 0.2f)
                        {
                            Utils::FormatToChat(0xFFFFFFFF, L"WRONG CALCULATED EFFECT DURATION!");
                        }
#endif
                        return false;
                    }
                }
                // We assume the effect stays on until it times out when the mob is outside compass range
                return true;
            };

            // Check if we can remove any effects
            for (auto effect_it = effects.begin(); effect_it < effects.end();)
            {
                if (!EffectIsValid(*effect_it))
                {
                    effect_it = RemoveAndElectNew(effects, effect_it);
                }
                else
                {
                    ++effect_it;
                }
            }

            it++;
        }

        // Remove effects scheduled for removal
        for (auto &[agent_id, unique_ids_to_remove] : scheduled_removals)
        {
            for (const auto unique_id_to_remove : unique_ids_to_remove)
            {
                RemoveTrackers(agent_id, [=](EffectTracker &effect)
                    { return effect.unique_id == unique_id_to_remove; });
            }
            unique_ids_to_remove.clear();
        }
    }
}
