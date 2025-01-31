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

#ifdef _DEBUG
#define DEBUG_ACTIVATIONS 1
#endif
#define DEBUG_ACTIVATIONS_COLOR 0xFFFF00FF

namespace StoC = GW::Packet::StoC;

namespace HerosInsight::EffectInitiator
{
    using namespace HerosInsight::PacketStepper;

    struct EffectClaimingScope
    {
        static inline uint32_t current_cause_agent_id = 0;
        static inline std::function<void(const StoC::AddEffect &)> claiming_handler = nullptr;

        EffectClaimingScope(uint32_t cause_agent_id, std::function<void(const StoC::AddEffect &)> handler)
        {
            assert(cause_agent_id != 0);
            assert(current_cause_agent_id == 0);
            current_cause_agent_id = cause_agent_id;
            claiming_handler = handler;
        }
        ~EffectClaimingScope()
        {
            assert(current_cause_agent_id != 0);
            current_cause_agent_id = 0;
            claiming_handler = nullptr;
        }
    };

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

    void GetSpellEffects(
        uint32_t caster_id, uint32_t target_id,
        GW::Constants::SkillID skill_id, uint8_t attr_lvl,
        FixedArrayRef<StaticSkillEffect> result)
    {
        auto caster_ptr = Utils::GetAgentLivingByID(caster_id);
        // auto target_ptr = Utils::GetAgentLivingByID(target_id);
        if (!caster_ptr)
            return;
        auto &caster_living = *caster_ptr;
        // auto &target_living = *target_ptr;

        auto &cskill = CustomSkillDataModule::GetCustomSkillData(skill_id);
        auto &skill = *GW::SkillbarMgr::GetSkillConstantData(skill_id);

        for (auto init_effect : cskill.init_effects)
        {
            if (auto effect_skill_id = std::get_if<GW::Constants::SkillID>(&init_effect.skill_id_or_removal))
            {
                switch (*effect_skill_id)
                {
                    case GW::Constants::SkillID::Disease:
                    {
                        if (skill_id == GW::Constants::SkillID::Mystic_Corruption &&
                            caster_living.GetIsEnchanted())
                        {
                            init_effect.duration_or_count.val0 *= 2;
                            init_effect.duration_or_count.val15 *= 2;
                        }
                        break;
                    }

                    case GW::Constants::SkillID::Ice_Spear:
                    case GW::Constants::SkillID::Smoldering_Embers:
                    case GW::Constants::SkillID::Magnetic_Surge:
                    case GW::Constants::SkillID::Stone_Daggers:
                    {
                        // These skills only cause effects if the caster is overcast
                        if (Utils::IsOvercast(caster_living))
                            break;
                        continue;
                    }
                }
            }

            result.try_push(init_effect);
        }

        switch (skill_id)
        {
            case GW::Constants::SkillID::Epidemic:
            {
                auto timestamp_now = GW::MemoryMgr::GetSkillTimer();
                auto target_effects = EffectTracking::GetTrackerSpan(target_id);
                for (const auto &effect : target_effects)
                {
                    auto &effect_skill = *GW::SkillbarMgr::GetSkillConstantData(effect.skill_id);
                    if (effect_skill.type == GW::Constants::SkillType::Condition)
                    {
                        auto timestamp_end = effect.GetEndTimestamp();
                        if (timestamp_end > timestamp_now)
                        {
                            auto rem_ms = timestamp_end - timestamp_now;
                            auto rem_sec = (rem_ms + 999) / 1000; // Is wrong?

                            StaticSkillEffect effect = {};
                            effect.location = EffectLocation::Target;
                            effect.mask = EffectMask::OtherFoes;
                            effect.radius = Utils::Range::Adjacent;
                            effect.skill_id_or_removal = effect_skill.skill_id;
                            effect.duration_or_count = {rem_sec, rem_sec};
                            result.try_push(effect);
                        }
                    }
                }
                return;
            }
        }
    }

    TrackedCoroutine OnSkillActivated(uint32_t caster_id, uint32_t target_id, GW::Constants::SkillID skill_id)
    {
#ifdef DEBUG_ACTIVATIONS
        Utils::FormatToChat(DEBUG_ACTIVATIONS_COLOR,
            L"SkillActivated: skill_id={}, caster_id={}, target_id={}",
            (uint32_t)skill_id, caster_id, target_id);
#endif
        auto &cskill = CustomSkillDataModule::GetCustomSkillData(skill_id);
        auto &caster = CustomAgentDataModule::GetCustomAgentData(caster_id);
        auto attr_lvl = caster.GetAttrLvlForSkill(cskill);

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

        bool has_init_effects = !cskill.init_effects.empty();

        if (has_init_effects)
        {
            FixedArray<StaticSkillEffect, 18> effects_salloc;
            auto effects = effects_salloc.ref();
            GetSpellEffects(caster_id, target_id, skill_id, attr_lvl, effects);

            FixedSet<uint32_t, (size_t)(16 / 0.75)> handled_agents;
            {
                EffectClaimingScope scope(caster_id,
                    [&](const StoC::AddEffect &packet)
                    {
                        if (packet.skill_id == skill_id)
                        {
                            attr_lvl = packet.attribute_level;

                            // TODO: Check for liutenant's and account for it maybe?
                        }

                        handled_agents.insert(packet.agent_id);
                    });
                if (target_id == 0) // Some targeted instant skills don't properly disclose their target, so we attempt to deduce it
                {
                    PacketListenerScope listener(
                        [&](const StoC::GenericValueTarget &packet)
                        {
                            if (packet.caster == caster_id)
                                target_id = packet.target;
                        });
                }
                co_await AfterEffectsAwaiter(caster_id);
            }

            if (target_id == 0)
                target_id = caster_id; // If all else fails assume the caster is the target

            for (auto &effect : effects)
            {
                effect.Apply(caster_id, target_id, attr_lvl,
                    [&](GW::AgentLiving &agent)
                    {
                        return !handled_agents.has(agent.agent_id);
                    });
            }
        }

        switch (skill.type)
        {
            case GW::Constants::SkillType::Ritual:
            {
                auto npc_packet = co_await PacketAwaiter<StoC::CreateNPC>();
                co_await PacketAwaiter<StoC::AgentAdd>(Altitude::After); // The npc needs to be created before we can "attach" the effect to it
                EffectTracking::CreateAuraEffect(
                    npc_packet.agent_id,
                    (float)Utils::Range::SpiritRange,
                    skill_id,
                    npc_packet.effect_id,
                    caster_id,
                    npc_packet.lifetime);
                break;
            }

            case GW::Constants::SkillType::Well:
            {

                break;
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
                    break;
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
        CollectAttackEffects(attacker_id, attack.skill_id, attack.IsMelee(), effects);

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
        while (true)
        {
            NEXT_PACKET_SWITCH
            {
                NEXT_PACKET_CASE(StoC::GenericValue, packet, {
                    if (packet.agent_id != attacker_id)
                        continue;

                    switch (packet.value_id)
                    {
                        case StoC::GenericValueID::melee_attack_finished:
                            auto attack = CreateMeleeAttack(attacker_id, skill_id);
                            TrackHit(attacker_id, attack);
                        case StoC::GenericValueID::attack_stopped:
                            goto success;
                    }

                    break;
                })

                NEXT_PACKET_CASE(StoC::ProjectileCreated, packet, {
                    if (packet.agent_id != attacker_id)
                        continue;

                    auto proj = CreateRangedAttack(attacker_id, skill_id);
                    TrackProjectile(packet.projectile_id, attacker_id, packet.air_time, proj);
                    goto success;
                })
            }
        }
    success:
        // {

        //     PermaAwaiter awaiter;
        //     PacketListenerScope melee_listener(
        //         [=, &awaiter](const StoC::GenericValue &packet)
        //         {
        //             if (packet.agent_id != attacker_id)
        //                 return;

        //             switch (packet.value_id)
        //             {
        //                 case StoC::GenericValueID::melee_attack_finished:
        //                     auto attack = CreateMeleeAttack(attacker_id, skill_id);
        //                     TrackHit(attacker_id, attack);
        //                 case StoC::GenericValueID::attack_stopped:
        //                     awaiter.stop();
        //             }
        //         });
        //     PacketListenerScope ranged_listener(
        //         [=, &awaiter](const StoC::ProjectileCreated &packet)
        //         {
        //             if (packet.agent_id != attacker_id)
        //                 return;

        //             auto proj = CreateRangedAttack(attacker_id, skill_id);
        //             TrackProjectile(packet.projectile_id, attacker_id, packet.air_time, proj); // <- coroutine
        //             awaiter.stop();
        //         });
        //     co_await awaiter;
        // }

        co_return;
    }

    TrackedCoroutine TrackInstantSkill(uint32_t caster_id, GW::Constants::SkillID skill_id)
    {
        OnSkillActivated(caster_id, NULL, skill_id);

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
        float duration = *(float *)&packet->timestamp;

        if (EffectClaimingScope::claiming_handler)
            EffectClaimingScope::claiming_handler(*packet);

        EffectTracking::EffectTracker tracker = {};
        tracker.cause_agent_id = EffectClaimingScope::current_cause_agent_id;
        tracker.skill_id = (GW::Constants::SkillID)packet->skill_id;
        tracker.attribute_level = packet->attribute_level;
        tracker.effect_id = packet->effect_id;
        tracker.duration_sec = duration;

        EffectTracking::AddTracker(packet->agent_id, tracker);
    }

    void RemoveEffectCallback(GW::HookStatus *, const StoC::RemoveEffect *packet)
    {
        if (n_postponers > 0)
        {
            postponed.push_back(*packet);
        }
        else
        {
            EffectTracking::RemoveTrackers(packet->agent_id,
                [&](EffectTracking::EffectTracker &effect)
                {
                    return effect.effect_id == packet->effect_id;
                });
        }
    }

    GW::HookEntry entry;
    void Initialize()
    {
        constexpr int altitude = -1;
        GW::StoC::RegisterPacketCallback<StoC::GenericValue>(&entry, &GenericValueCallback, altitude);
        GW::StoC::RegisterPacketCallback<StoC::GenericValueTarget>(&entry, &GenericValueTargetCallback, altitude);
        GW::StoC::RegisterPacketCallback<StoC::AddEffect>(&entry, &AddEffectCallback, altitude);
        GW::StoC::RegisterPacketCallback<StoC::RemoveEffect>(&entry, &RemoveEffectCallback, altitude);
    }

    void Terminate()
    {
        GW::StoC::RemoveCallbacks(&entry);
    }

    void Update();
}
