#include <cassert>
#include <chrono>
#include <coroutine>
#include <format>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

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
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>

#include <StoC_packets.h>
#include <attribute_or_title.h>
#include <autovec.h>
#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <debug_display.h>
#include <effect_tracking.h>
#include <hero_ai.h>
#include <packet_stepper.h>
#include <party_data.h>
#include <update_manager.h>
#include <utils.h>

#include "effect_initiator.h"

#define DEBUG_ACTIVATIONS 1
#define DEBUG_ACTIVATIONS_COLOR 0xFFFF00FF

namespace StoC = GW::Packet::StoC;

namespace HerosInsight::EffectInitiator
{
    using namespace HerosInsight::PacketStepper;

    constexpr uint8_t MAIN_CHANNEL = 0;

    struct Attack
    {
        GW::Constants::SkillID skill_id;
        GW::Constants::SkillID prep_skill_id;
        uint8_t attr_lvl;
        uint8_t prep_attr_lvl;
        bool is_projectile;

        bool IsMelee() const { return !is_projectile; }
    };

    struct AgentActions
    {
        TrackedCoroutine main;
        TrackedCoroutine instant;
    };

    uint32_t n_postponers = 0;
    std::vector<GW::Packet::StoC::RemoveEffect> postponed;
    // When this exists, any RemoveEffect StoC packets will be postponed until it goes out of scope
    struct RemoveEffectPostponeScope
    {
        RemoveEffectPostponeScope()
        {
            n_postponers++;
        }
        ~RemoveEffectPostponeScope()
        {
            assert(n_postponers > 0);
            n_postponers--;

            if (n_postponers == 0)
            {
                for (auto &packet : postponed)
                {
                    EffectTracking::RemoveTrackers(packet.agent_id,
                        [&](EffectTracking::EffectTracker &effect)
                        {
                            return effect.effect_id == packet.effect_id;
                        });
                }
                postponed.clear();
            }
        }
    };

    AutoVec<AgentActions> agent_actions;

    void OnSkillActivated(uint32_t caster_id, uint32_t target_id, GW::Constants::SkillID skill_id)
    {
#ifdef DEBUG_ACTIVATIONS
        Utils::FormatToChat(DEBUG_ACTIVATIONS_COLOR,
            L"SkillActivated: skill_id={}, caster_id={}, target_id={}",
            (uint32_t)skill_id, caster_id, target_id);
#endif
        auto &cskill = CustomSkillDataModule::GetCustomSkillData(skill_id);
        auto &caster = CustomAgentDataModule::GetCustomAgentData(caster_id);
        const auto attr_lvl = caster.GetAttrLvlForSkill(cskill);
        // skill.OnSkillActivation(caster, target_id);

        auto &skill = *GW::SkillbarMgr::GetSkillConstantData(skill_id);

        { // SPECIAL SKILL "EFFECTS"
            if (cskill.tags.Spell)
            {
                if (caster.signet_of_illusions_charges > 0)
                    caster.signet_of_illusions_charges--;
            }

            switch (skill_id)
            {
                case GW::Constants::SkillID::Signet_of_Illusions:
                {
                    caster.signet_of_illusions_charges = cskill.GetSkillParam(0).Resolve(attr_lvl);
                    break;
                }

                case GW::Constants::SkillID::Barrage:
                case GW::Constants::SkillID::Volley:
                {
                    EffectTracking::RemoveTrackers(caster_id,
                        [](EffectTracking::EffectTracker &effect)
                        {
                            return GW::SkillbarMgr::GetSkillConstantData(effect.skill_id)->type == GW::Constants::SkillType::Preparation;
                        });
                    break;
                }
            }
        }

        { // ON ACTIVATION EFFECTS
            auto caster_agent = Utils::GetAgentLivingByID(caster_id);
            auto target_agent = Utils::GetAgentLivingByID(target_id);
            if (!target_agent || !caster_agent)
                return;

            if (cskill.effect_target != EffectTarget::None)
            {
                auto base_duration = cskill.ResolveBaseDuration(caster, attr_lvl);
                auto skill_effect = SkillEffect{skill_id, skill_id, base_duration};

                std::vector<uint32_t> targets;

                switch (cskill.effect_target)
                {
                    case EffectTarget::CasterAndPet:
                    case EffectTarget::Caster:
                        targets.push_back(caster_id);
                        if (cskill.effect_target == EffectTarget::Caster)
                            break;
                    case EffectTarget::Pet:
                    {
                        auto pet_id = Utils::GetPetOfAgent(caster_id);
                        if (pet_id)
                            targets.push_back(pet_id);
                        break;
                    }

                    case EffectTarget::Target:
                        targets.push_back(target_id);
                        break;

                    case EffectTarget::TargetAOE:
                    {
                        auto caster_target_relations = Utils::GetAgentRelations(caster_agent->allegiance, target_agent->allegiance);
                        Utils::ForAgentsInCircle(target_agent->pos, skill.aoe_range,
                            [&](GW::AgentLiving &agent)
                            {
                                auto caster_agent_relations = Utils::GetAgentRelations(caster_agent->allegiance, agent.allegiance);
                                if (caster_target_relations == caster_agent_relations)
                                    targets.push_back(agent.id);
                            });
                        break;
                    }
                }

                bool any_receives_stoc_effects = false;
                for (auto target : targets)
                {
                    if (Utils::ReceivesStoCEffects(target))
                    {
                        any_receives_stoc_effects = true;
                        break;
                    }
                }

                uint32_t duration = 0;

                auto AddTracker = [&](uint32_t target, uint32_t effect_id = 0)
                {
                    EffectTracking::EffectTracker tracker = {};
                    tracker.cause_agent_id = caster_id;
                    tracker.skill_id = skill_id;
                    tracker.attribute_level = attr_lvl;
                    tracker.effect_id = effect_id;
                    tracker.duration_sec = duration;

                    EffectTracking::AddTracker(target, skill_effect);
                };

                if (any_receives_stoc_effects)
                {
                    AfterEffectsAwaiter awaiter(caster_id, targets);
                    PacketListenerScope listener(
                        [&](const StoC::AddEffect &packet)
                        {
                            if (packet.skill_id != skill_id)
                                break;

                            for (auto &target : targets)
                            {
                                if (target == packet.agent_id)
                                {
                                    target |= 0x80000000; // Mark as "processed"
                                    goto found;
                                }
                            }
                            break;
                        found:

                            // TODO: Check for liutenant's and account for it

                            attr_lvl = packet.attribute_level;
                            duration = packet.timestamp;
                            AddTracker(packet.agent_id, packet.effect_id);
                        });
                    co_await awaiter;
                }

                if (duration == 0)
                {
                    duration = Utils::CalculateDuration(skill, base_duration, caster_id);
                }

                for (auto &target : targets)
                {
                    if (target & 0x80000000) // Already processed
                    {
                        target &= ~0x80000000; // Remove mark
                        continue;
                    }

                    AddTracker(target);
                }
            }

            FixedArray<SkillEffect, 18> conditions_salloc;
            auto conditions = conditions_salloc.ref();
            cskill.GetOnActivationEffects(caster, target_id, conditions);

            if (conditions.size() > 0)
            {
            }
        }
    }

    void OnAttackHit(uint32_t attacker_id, uint32_t victim_id, Attack &attack, bool is_crit, std::span<SkillEffect> effects)
    {
        if (attack.is_projectile)
        {
#ifdef DEBUG_ACTIVATIONS
            Utils::FormatToChat(DEBUG_ACTIVATIONS_COLOR,
                L"ProjectileHit: skill_id={}, attacker_id={}, victim_id={}",
                (uint32_t)attack.skill_id, attacker_id, victim_id);
#endif
        }
        else // is_melee
        {
#ifdef DEBUG_ACTIVATIONS
            Utils::FormatToChat(DEBUG_ACTIVATIONS_COLOR,
                L"MeleeHit: skill_id={}, attacker_id={}, victim_id={}",
                (uint32_t)attack.skill_id, attacker_id, victim_id);
#endif
        }

        EffectTracking::ApplySkillEffects(victim_id, attacker_id, effects);

        switch (attack.skill_id)
        {
            case GW::Constants::SkillID::Wild_Smash:
            case GW::Constants::SkillID::Wild_Throw:
            case GW::Constants::SkillID::Wild_Blow:
            case GW::Constants::SkillID::Wild_Strike:
            case GW::Constants::SkillID::Wild_Strike_PvP:
            case GW::Constants::SkillID::Forceful_Blow:
            case GW::Constants::SkillID::Whirling_Axe:
                EffectTracking::RemoveTrackers(victim_id,
                    [](EffectTracking::EffectTracker &effect)
                    {
                        return GW::SkillbarMgr::GetSkillConstantData(effect.skill_id)->type == GW::Constants::SkillType::Stance;
                    });
        }
    }

    void OnAttackBlocked(uint32_t attacker_id, uint32_t defender_id, Attack &attack)
    {
        if (attack.is_projectile)
        {
#ifdef DEBUG_ACTIVATIONS
            Utils::FormatToChat(DEBUG_ACTIVATIONS_COLOR,
                L"ProjectileBlock: attacker_id={}, defender_id={}",
                attacker_id, defender_id);
#endif
            switch (attack.prep_skill_id)
            {
                case GW::Constants::SkillID::Glass_Arrows:
                case GW::Constants::SkillID::Glass_Arrows_PvP:
                {
                    auto base_duration = Utils::LinearAttributeScale(10, 20, attack.prep_attr_lvl);
                    auto bleeding_effect = SkillEffect{GW::Constants::SkillID::Bleeding, GW::Constants::SkillID::Glass_Arrows, base_duration};
                    EffectTracking::ApplySkillEffect(defender_id, attacker_id, bleeding_effect);
                }
            }
        }
        else // is_melee
        {
#ifdef DEBUG_ACTIVATIONS
            Utils::FormatToChat(DEBUG_ACTIVATIONS_COLOR,
                L"MeleeBlock: attacker_id={}, defender_id={}",
                attacker_id, defender_id);
#endif
            // Swift_Chop
            auto condition = GW::Constants::SkillID::Deep_Wound;
            uint32_t duration = 20;
            switch (attack.skill_id)
            {
                case GW::Constants::SkillID::Seeking_Blade:
                    condition = GW::Constants::SkillID::Bleeding;
                    duration = 25;
                case GW::Constants::SkillID::Swift_Chop:
                {
                    auto skill_effect = SkillEffect{condition, attack.skill_id, duration};
                    EffectTracking::ApplySkillEffect(attacker_id, defender_id, skill_effect);
                }
            }
        }

        auto defender_effects = EffectTracking::GetTrackers(defender_id);
        std::vector<GW::Constants::SkillID> spent_charges;
        for (auto &defender_effect : defender_effects)
        {
            switch (defender_effect.skill_id)
            {
                case GW::Constants::SkillID::Deflect_Arrows:
                {
                    if (attack.is_projectile)
                    {
                        auto defender = Utils::GetAgentLivingByID(defender_id);
                        if (defender)
                        {
                            auto base_duration = Utils::LinearAttributeScale(5, 15, defender_effect.attribute_level);
                            auto bleeding_effect = SkillEffect{GW::Constants::SkillID::Bleeding, GW::Constants::SkillID::Deflect_Arrows, base_duration};
                            Utils::ForEnemiesInCircle(defender->pos, (float)Utils::Range::Adjacent, defender->allegiance,
                                [&](GW::AgentLiving &agent)
                                {
                                    EffectTracking::ApplySkillEffect(agent.agent_id, defender_id, bleeding_effect);
                                });
                        }
                    }
                    break;
                }
                case GW::Constants::SkillID::Shield_of_Force:
                {
                    auto defender = Utils::GetAgentLivingByID(defender_id);
                    if (defender)
                    {
                        auto base_duration = Utils::LinearAttributeScale(5, 20, defender_effect.attribute_level);
                        auto weakness_effect = SkillEffect{GW::Constants::SkillID::Weakness, GW::Constants::SkillID::Shield_of_Force, base_duration};
                        Utils::ForEnemiesInCircle(defender->pos, (float)Utils::Range::Adjacent, defender->allegiance,
                            [&](GW::AgentLiving &enemy)
                            {
                                if (enemy.GetIsAttacking())
                                {
                                    EffectTracking::ApplySkillEffect(enemy.agent_id, defender_id, weakness_effect);
                                }
                            });
                    }
                    spent_charges.push_back(defender_effect.skill_id);
                    break;
                }
                case GW::Constants::SkillID::Deadly_Riposte:
                {
                    if (attack.IsMelee())
                    {
                        auto defender = Utils::GetAgentLivingByID(defender_id);
                        if (defender && defender->weapon_type == 7) // Sword
                        {
                            auto base_duration = Utils::LinearAttributeScale(3, 25, defender_effect.attribute_level);
                            auto bleeding_effect = SkillEffect{GW::Constants::SkillID::Bleeding, GW::Constants::SkillID::Deadly_Riposte, base_duration};
                            EffectTracking::ApplySkillEffect(attacker_id, defender_id, bleeding_effect);
                            spent_charges.push_back(defender_effect.skill_id);
                        }
                    }
                    break;
                }
                case GW::Constants::SkillID::Burning_Shield:
                {
                    auto defender = Utils::GetAgentLivingByID(defender_id);
                    if (defender &&
                        defender->offhand_item_type == (uint8_t)Utils::OffhandItemType::Shield && // Wielding a shield
                        attack.skill_id != GW::Constants::SkillID::No_Skill)                      // An attack skill was used
                    {
                        if (attack.IsMelee())
                        {
                            auto base_duration = Utils::LinearAttributeScale(1, 6, defender_effect.attribute_level);
                            auto burning_effect = SkillEffect{GW::Constants::SkillID::Burning, GW::Constants::SkillID::Burning_Shield, base_duration};
                            EffectTracking::ApplySkillEffect(attacker_id, defender_id, burning_effect);
                        }
                        spent_charges.push_back(defender_effect.skill_id);
                    }
                    break;
                }
            }
        }
        for (const auto &skill_id : spent_charges)
        {
            EffectTracking::SpendCharge(defender_id, skill_id);
        }
    }

    void CollectAttackEffects(uint32_t attacker_id, GW::Constants::SkillID skill_id, bool is_melee, FixedArrayRef<SkillEffect> out)
    {
        auto attacker_effects = EffectTracking::GetTrackers(attacker_id);
        std::vector<GW::Constants::SkillID> spent_charges;
        for (const auto &attacker_effect : attacker_effects)
        {
            FixedArray<SkillEffect, 8> conditions_salloc;
            auto conditions = conditions_salloc.ref();

            auto effect_skill_id = attacker_effect.skill_id;
            switch (effect_skill_id)
            {
                case GW::Constants::SkillID::Apply_Poison:
                {
                    if (is_melee)
                    {
                        auto base_duration = Utils::LinearAttributeScale(3, 15, attacker_effect.attribute_level);
                        auto poison_effect = SkillEffect{GW::Constants::SkillID::Poison, GW::Constants::SkillID::Apply_Poison, base_duration};
                        out.try_push(poison_effect);
                    }
                }

                case GW::Constants::SkillID::Strike_as_One:
                {
                    bool is_pet = Utils::GetIsPet(attacker_id);
                    auto base_duration = Utils::LinearAttributeScale(5, 15, attacker_effect.attribute_level);
                    auto condition = is_pet ? GW::Constants::SkillID::Bleeding : GW::Constants::SkillID::Crippled;
                    out.try_push({condition, effect_skill_id, base_duration});
                    spent_charges.push_back(effect_skill_id);
                    break;
                }

                case GW::Constants::SkillID::Anthem_of_Weariness:
                case GW::Constants::SkillID::Anthem_of_Flame:
                case GW::Constants::SkillID::Crippling_Anthem:
                    if (skill_id == GW::Constants::SkillID::No_Skill) // These require an attack skill
                        break;
                case GW::Constants::SkillID::Hidden_Rock:
                case GW::Constants::SkillID::Yellow_Snow:
                case GW::Constants::SkillID::Poison_Tip_Signet:
                case GW::Constants::SkillID::Sundering_Weapon:
                case GW::Constants::SkillID::Weapon_of_Shadow:
                case GW::Constants::SkillID::Find_Their_Weakness:
                case GW::Constants::SkillID::Find_Their_Weakness_Thackeray:
                    spent_charges.push_back(effect_skill_id);
                case GW::Constants::SkillID::Find_Their_Weakness_PvP: // We dont spend charge yet. Needs special handling because it requires a critical hit.
                {
                    auto &cskill = CustomSkillDataModule::GetCustomSkillData(effect_skill_id);
                    cskill.GetInitConditions(attacker_effect.attribute_level, conditions);
#ifdef _DEBUG
                    SOFT_ASSERT(conditions.size() > 0, L"Missing conditions for skill {}", Utils::GetSkillName(effect_skill_id));
#endif
                    if (conditions.size() > 0)
                        out.try_push(conditions[0]);
                    break;
                }
            }
        }
        for (const auto &skill_id : spent_charges)
        {
            EffectTracking::SpendCharge(attacker_id, skill_id);
        }
    }

    Attack CreateMeleeAttack(uint32_t caster_id, GW::Constants::SkillID skill_id)
    {
        auto &caster_agent = CustomAgentDataModule::GetCustomAgentData(caster_id);
        auto &cskill = CustomSkillDataModule::GetCustomSkillData(skill_id);
        auto attr_lvl = caster_agent.GetAttrLvlForSkill(cskill);

        Attack attack = {};
        attack.skill_id = skill_id;
        attack.attr_lvl = attr_lvl;
        return attack;
    }

    Attack CreateRangedAttack(uint32_t caster_id, GW::Constants::SkillID skill_id)
    {
        auto &caster_agent = CustomAgentDataModule::GetCustomAgentData(caster_id);
        auto &cskill = CustomSkillDataModule::GetCustomSkillData(skill_id);
        auto attr_lvl = caster_agent.GetAttrLvlForSkill(cskill);

        Attack proj = {};
        proj.skill_id = skill_id;
        proj.attr_lvl = attr_lvl;
        proj.is_projectile = true;

        auto caster_living = Utils::GetAgentLivingByID(caster_id);
        bool is_arrow = false;
        if (caster_living && caster_living->weapon_type == 1) // Bow
            is_arrow = true;

        auto caster_effects = EffectTracking::GetTrackerSpan(caster_id);
        for (auto &effect : caster_effects)
        {
            if (!effect.is_active)
                continue;

            if (skill_id == GW::Constants::SkillID::Apply_Poison ||
                (is_arrow && GW::SkillbarMgr::GetSkillConstantData(effect.skill_id)->type == GW::Constants::SkillType::Preparation))
            {
                proj.prep_skill_id = effect.skill_id;
                proj.prep_attr_lvl = effect.attribute_level;
                break;
            }
        }

        return proj;
    }

    TrackedCoroutine TrackHit(uint32_t attacker_id, Attack attack)
    {
        FixedArray<SkillEffect, 18> effects_salloc;
        auto effects = effects_salloc.ref();
        CollectAttackEffects(attacker_id, attack.skill_id, false, effects);

        {
            FrameEndAwaiter awaiter;
            PacketListenerScope miss_listener(
                [&](const StoC::GenericValueTarget &packet)
                {
                    if (packet.caster != attacker_id)
                        return;

                    if (packet.Value_id == StoC::GenericValueID::attack_missed)
                    {
                        auto defender_id = packet.target;
                        switch (packet.value)
                        {
                            case (uint32_t)Utils::MissType::Block:
                                OnAttackBlocked(attacker_id, defender_id, attack);
                                break;
                        }
                        awaiter.stop();
                    }
                });
            PacketListenerScope hit_listener(
                [&](const StoC::GenericModifier &packet)
                {
                    if (packet.cause_id != attacker_id)
                        return;

                    bool is_crit = false;
                    switch (packet.type)
                    {
                        case StoC::GenericValueID::critical:
                            is_crit = true;
                        case StoC::GenericValueID::damage:
                            OnAttackHit(attacker_id, packet.target_id, attack, is_crit, effects);
                            awaiter.stop();
                    }
                });
            co_await awaiter;
        }

        co_return;
    }

    TrackedCoroutine TrackProjectile(uint32_t projectile_id, uint32_t caster_id, float air_time, Attack proj)
    {
#ifdef DEBUG_ACTIVATIONS
        Utils::FormatToChat(DEBUG_ACTIVATIONS_COLOR,
            L"ProjectileCreated: skill_id={}, caster_id={}",
            (uint32_t)proj.skill_id, caster_id);
#endif

        bool timed_out = true;
        {
            DelayAwaiter awaiter(uint32_t(air_time * 1000.f) + 3000);
            PacketListenerScope listener(
                [&](const StoC::ProjectileDestroyed &packet)
                {
                    if (projectile_id != packet.projectile_id)
                        return;

                    TrackHit(caster_id, proj);
                    timed_out = false;
                    awaiter.stop();
                });
            co_await awaiter;
        }

#ifdef DEBUG_ACTIVATIONS
        if (timed_out)
        {
            SOFT_ASSERT(false && "Projectile timed out");
        }
#endif

        co_return;
    }

    TrackedCoroutine TrackAttack(uint32_t attacker_id, uint32_t target_id, GW::Constants::SkillID skill_id)
    {
        {
            PermaAwaiter awaiter;
            PacketListenerScope melee_listener(
                [&](const StoC::GenericValue &packet)
                {
                    if (packet.agent_id != attacker_id)
                        return;

                    switch (packet.value_id)
                    {
                        case StoC::GenericValueID::melee_attack_finished:
                            auto attack = CreateMeleeAttack(attacker_id, skill_id);
                            TrackHit(attacker_id, attack);
                        case StoC::GenericValueID::attack_stopped:
                            awaiter.stop();
                    }
                });
            PacketListenerScope ranged_listener(
                [&](const StoC::ProjectileCreated &packet)
                {
                    if (packet.agent_id != attacker_id)
                        return;

                    auto proj = CreateRangedAttack(attacker_id, skill_id);
                    TrackProjectile(packet.projectile_id, attacker_id, packet.air_time, proj);
                    awaiter.stop();
                });
            co_await awaiter;
        }

        co_return;
    }

    TrackedCoroutine TrackInstantSkill(uint32_t caster_id, GW::Constants::SkillID skill_id)
    {
        uint32_t target_id = caster_id;
        // {
        //     // It seems the only way to deduce the target is by listening to the effect_on_target packet
        //     // And that only works for some targeted skills
        //     PacketListenerScope listener(
        //         [&](const StoC::GenericValueTarget &packet)
        //         {
        //             if (packet.caster == caster_id &&
        //                 packet.Value_id == StoC::GenericValueID::effect_on_target)
        //             {
        //                 target_id = packet.target;
        //             }
        //         });
        //     co_await FrameEndAwaiter();
        // }

        // // Attempt to deduce the target from the next packets (We assume no other packets comes inbetween)
        // while (true)
        // {
        //     auto next_packet = co_await NextPacketAwaiter();

        //     switch (next_packet->header)
        //     {
        //         // These are allowed to come before...
        //         case StoC::GenericValue::STATIC_HEADER:
        //         {
        //             auto p = (StoC::GenericValue *)next_packet;
        //             if (p->agent_id == caster_id)
        //                 switch (p->value_id)
        //                 {
        //                     case StoC::GenericValueID::effect_on_agent:
        //                         continue;
        //                 }
        //             break;
        //         }
        //         // ... this, and if we get this, we can deduce the target
        //         case StoC::GenericValueTarget::STATIC_HEADER:
        //         {
        //             auto p = (StoC::GenericValueTarget *)next_packet;
        //             if (p->caster == caster_id)
        //                 switch (p->Value_id)
        //                 {
        //                     case StoC::GenericValueID::effect_on_target:
        //                         target_id = p->target;
        //                 }
        //             break;
        //         }
        //     }
        //     break;
        // };

        {
            PacketListenerScope listener(
                [&](const StoC::GenericValueTarget &packet)
                {
                    if (packet.caster != caster_id)
                        return;

                    target_id = packet.target;
                });
            co_await AfterEffectsAwaiter(caster_id, std::span(&target_id, 1));
        }

        OnSkillActivated(caster_id, target_id, skill_id);

        co_return;
    }

    void ReverseEngineerAttribute(uint32_t caster_id, GW::Constants::SkillID source_skill_id, const StoC::AddEffect &packet)
    {
        auto &effect_skill = *GW::SkillbarMgr::GetSkillConstantData((GW::Constants::SkillID)packet.skill_id);

        auto SetCasterAttr = [&](GW::Constants::AttributeByte attribute, uint32_t level)
        {
#ifdef DEBUG_ACTIVATIONS
            auto attr_str = Utils::GetAttributeString(attribute);
            auto attr_str_w = Utils::StrToWStr(attr_str);
            Utils::FormatToChat(DEBUG_ACTIVATIONS_COLOR,
                L"Deduced attribute: caster_id={}, attribute={}, level={}",
                caster_id, level, attr_str_w);
#endif

            auto &caster = CustomAgentDataModule::GetCustomAgentData(caster_id);
            caster.SetAttribute(attribute, level);
        };

        if (effect_skill.type == GW::Constants::SkillType::Condition)
        {
            // Maybe do something here based on duration and skill params...
        }
        else
        {
            if (effect_skill.attribute != 51)
            {
                SetCasterAttr(effect_skill.attribute, packet.attribute_level);
            }
        }
    }

    TrackedCoroutine TrackSkill(uint32_t caster_id, uint32_t target_id, GW::Constants::SkillID skill_id)
    {
        bool fail = false;
        {
            PermaAwaiter awaiter;
            PacketListenerScope listener(
                [&](const StoC::GenericValue &packet)
                {
                    if (packet.agent_id != caster_id)
                        return;

                    switch (packet.value_id)
                    {
                        case StoC::GenericValueID::skill_stopped:
                            fail = true;
                        case StoC::GenericValueID::skill_finished:
                            awaiter.stop();
                    }
                });
            co_await awaiter;
        }

        if (fail)
            co_return;

        auto &cskill = CustomSkillDataModule::GetCustomSkillData(skill_id);
        auto &caster = CustomAgentDataModule::GetCustomAgentData(caster_id);
        const auto attr_lvl = caster.GetAttrLvlForSkill(cskill);
        OnSkillActivated(caster_id, target_id, skill_id);

        {
            PacketListenerScope listener(
                [&](const StoC::ProjectileCreated &packet)
                {
                    if (packet.agent_id != caster_id)
                        return;

                    auto proj = CreateRangedAttack(caster_id, skill_id);
                    TrackProjectile(packet.projectile_id, caster_id, packet.air_time, proj);
                });

            bool caster_has_unknown_attributes = Utils::GetAgentAttributeSpan(caster_id).empty();
            if (caster_has_unknown_attributes)
            {
                PacketListenerScope listener(
                    [&](const StoC::AddEffect &packet)
                    {
                        ReverseEngineerAttribute(caster_id, skill_id, packet);
                    });
                co_await AfterEffectsAwaiter(caster_id, std::span(&target_id, 1));
            }

            // We linger here, listening for any projectiles created, we will be forced out once a new action is assigned to the agent
            co_await std::suspend_always{};
        }

        co_return;
    }

    bool AssignAction(TrackedCoroutine &existing, TrackedCoroutine action)
    {
        bool was_overwritten = false;
        if (!existing.is_finished())
        {
            was_overwritten = true;
            existing.destroy();
        }
        existing = action;
        return was_overwritten;
    }

    void AssignMainAction(uint32_t agent_id, TrackedCoroutine action)
    {
        auto &existing = agent_actions[agent_id];
        bool was_overwritten = AssignAction(existing.main, action);
#ifdef _DEBUG
        if (was_overwritten)
            Utils::FormatToChat(0xFFFFFFFF, L"Main action for agent {} was overwritten", agent_id);
#endif
    }

    void AssignInstantAction(uint32_t agent_id, TrackedCoroutine action)
    {
        auto &existing = agent_actions[agent_id];
        bool was_overwritten = AssignAction(existing.instant, action);
#ifdef _DEBUG
        if (was_overwritten)
            Utils::FormatToChat(0xFFFFFFFF, L"Instant action for agent {} was overwritten", agent_id);
#endif
    }

    void GenericValueCallback(GW::HookStatus *, const StoC::GenericValue *packet)
    {
        if (packet->value_id == StoC::GenericValueID::skill_activated)
        {
            auto caster_id = packet->agent_id;
            TrackedCoroutine action = TrackSkill(caster_id, caster_id, (GW::Constants::SkillID)packet->value);
            AssignMainAction(caster_id, action);
        }

        if (packet->value_id == StoC::GenericValueID::instant_skill_activated)
        {
            auto caster_id = packet->agent_id;
            auto skill_id = (GW::Constants::SkillID)packet->value;
            TrackedCoroutine action = TrackInstantSkill(caster_id, skill_id);
            AssignInstantAction(caster_id, action);
        }
    }

    void GenericValueTargetCallback(GW::HookStatus *, const StoC::GenericValueTarget *packet)
    {
        auto caster_id = packet->target;
        auto target_id = packet->caster;

        if (packet->Value_id == StoC::GenericValueID::skill_activated)
        {
            auto skill_id = (GW::Constants::SkillID)packet->value;
            TrackedCoroutine action = TrackSkill(caster_id, target_id, skill_id);
            AssignMainAction(caster_id, action);
        }

        if (packet->Value_id == StoC::GenericValueID::attack_skill_activated ||
            packet->Value_id == StoC::GenericValueID::attack_started)
        {
            auto skill_id = (GW::Constants::SkillID)packet->value;
            TrackedCoroutine action = TrackAttack(caster_id, target_id, skill_id);
            AssignMainAction(caster_id, action);
        }
    }

    void AddEffectCallback(GW::HookStatus *, const StoC::AddEffect *packet)
    {
        // EffectTracking::AddTracker
    }

    // void RemoveEffectCallback(GW::HookStatus *, const StoC::RemoveEffect *packet)
    // {
    //     if (n_postponers > 0)
    //     {
    //         postponed.push_back(*packet);
    //     }
    //     else
    //     {
    //         EffectTracking::RemoveTrackers(packet->agent_id,
    //             [&](EffectTracking::EffectTracker &effect)
    //             {
    //                 return effect.effect_id == packet->effect_id;
    //             });
    //     }
    // }

    GW::HookEntry entry;
    void Initialize()
    {
        int altitude = -1;
        GW::StoC::RegisterPacketCallback<StoC::GenericValue>(&entry, &GenericValueCallback, altitude);
        GW::StoC::RegisterPacketCallback<StoC::GenericValueTarget>(&entry, &GenericValueTargetCallback, altitude);
        GW::StoC::RegisterPacketCallback<StoC::AddEffect>(&entry, &AddEffectCallback, altitude);
        // GW::StoC::RegisterPacketCallback<StoC::RemoveEffect>(&entry, &RemoveEffectCallback, altitude);
    }

    void Terminate()
    {
        GW::StoC::RemoveCallbacks(&entry);
    }

    void Update();
}
