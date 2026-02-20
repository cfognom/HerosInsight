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
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>

#include <attribute_or_title.h>
#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <debug_display.h>
#include <effect_tracking.h>
#include <party_data.h>
#include <update_manager.h>
#include <utils.h>

namespace StoC = GW::Packet::StoC;

static GW::HookEntry packet_reader_entry;
static GW::HookEntry packet_reader_debug_entry;

const uint32_t headers_to_debug[] = {
    StoC::GenericFloat::STATIC_HEADER,
    StoC::GenericModifier::STATIC_HEADER,
    StoC::GenericValue::STATIC_HEADER,
    StoC::GenericValueTarget::STATIC_HEADER,
    StoC::ProjectileCreated::STATIC_HEADER,
    StoC::ProjectileDestroyed::STATIC_HEADER,
    StoC::AddEffect::STATIC_HEADER,
    (uint32_t)GAME_SMSG::EFFECT_RENEWED,
    StoC::RemoveEffect::STATIC_HEADER,
    // StoC::SkillActivate::STATIC_HEADER,
    // StoC::SkillRecharge::STATIC_HEADER,
    // StoC::SkillRecharged::STATIC_HEADER,
    // StoC::PlayEffect::STATIC_HEADER,
    // StoC::AgentState::STATIC_HEADER,
    // StoC::AgentAdd::STATIC_HEADER,
    // StoC::InstanceLoadInfo::STATIC_HEADER,
    // (uint32_t)GAME_SMSG::AGENT_UPDATE_ATTRIBUTE,
    // StoC::UpdateTitle::STATIC_HEADER,
};

#ifdef _DEBUG
#define DEBUG_PACKETS 0 // 0 = none, 1 = only related to player, 2 = only related to player or same frame, 3 = all
#define DEBUG_CALC 1    // 0 = none, 1 = only wrong, 2 = all
// #else
// #define DEBUG_PACKETS 0
// #define DEBUG_CALC 0
#else
#define DEBUG_PACKETS 0
#define DEBUG_CALC 0
#endif

namespace HerosInsight::PacketReader
{
    struct LastSkillCast
    {
        uint32_t caster;
        uint32_t target_id;
        GW::Constants::SkillID skill_id;
        uint32_t activated_frame;
        uint32_t finished_frame;
        uint32_t interrupted_frame;

        uint8_t energy_spent;
        uint32_t energy_spent_frame;

        float casttime;
        uint32_t casttime_frame;
    };

    static uint32_t last_debug_frame_id = 0;
    static float last_debug_date = 0;
    static std::unordered_map<uint32_t, LastSkillCast> last_skill_cast; // key = agent_id of caster
    static std::vector<OwnedProjectile> projectiles;

    void TrimProjectiles()
    {
        // Remove any trailing invalid projectiles
        while (!projectiles.empty() && !projectiles.back().IsValid())
            projectiles.pop_back();
    }

    void WriteDebugCheckMessage(uint32_t agent_id, GW::Constants::SkillID skill_id, bool success, const wchar_t *property_name, float calc, float actual)
    {
        if (DEBUG_CALC == 0 ||
            (DEBUG_CALC == 1 && success))
            return;

        const auto skill_name = Utils::GetSkillName(skill_id);
        const auto agent_name = Utils::GetAgentName(agent_id);
        const auto color = success ? 0xFF00FF00 : 0xFFFF0000; // green : red
        const auto message = success ? L"CORRECT" : L"WRONG";
        Utils::FormatToChat(
            color,
            L"Calculated {} of '{}'/'{}' was {}. Calculated '{}', Actual '{}'",
            property_name, agent_name.c_str(), skill_name.c_str(), message, calc, actual
        );
    }

    void CheckEnergyCostCalculation(uint32_t agent_id, uint8_t energy_spent, GW::Constants::SkillID skill_id)
    {
        const auto calc_energy_spent = Utils::CalculateEnergyCost(agent_id, skill_id);

        const auto energy_diff = std::abs(energy_spent - (float)calc_energy_spent);
        const auto success = energy_diff < 0.001f;

        WriteDebugCheckMessage(agent_id, skill_id, success, L"energy cost", calc_energy_spent, energy_spent);
    }

    void CheckCasttimeCalculation(uint32_t agent_id, float casttime, GW::Constants::SkillID skill_id)
    {
        const auto opt_calc = Utils::CalculateCasttime(agent_id, skill_id);
        if (opt_calc)
        {
            const auto &calc = *opt_calc;

            const auto calc_casttime = calc.half_chance && std::abs(casttime - calc.half) < 0.001f ? calc.half : calc.value;

            const auto casttime_diff = std::abs(casttime - calc_casttime);
            const auto success = casttime_diff < 0.001f;

            WriteDebugCheckMessage(agent_id, skill_id, success, L"casttime", calc_casttime, casttime);
        }
    }

    void OnSkillActivationStart(uint32_t caster_id, uint32_t target_id, GW::Constants::SkillID skill_id)
    {
        auto &data = last_skill_cast[caster_id];

        data = {};

        data.caster = caster_id;
        data.target_id = target_id;
        data.skill_id = skill_id;
        data.activated_frame = UpdateManager::frame_id;

        if ((uint32_t)skill_id)
        {
            if (data.energy_spent_frame && data.energy_spent_frame == UpdateManager::frame_id)
            {
                CheckEnergyCostCalculation(caster_id, data.energy_spent, skill_id);
            }

            if (data.casttime_frame && data.casttime_frame == UpdateManager::frame_id)
            {
                CheckCasttimeCalculation(caster_id, data.casttime, skill_id);
            }
        }
    }

    void OnSkillActivationFinish(uint32_t caster_id)
    {
        auto &data = last_skill_cast[caster_id];
        if (data.finished_frame)
            return;

        data.finished_frame = UpdateManager::frame_id;
        auto skill_id = data.skill_id;
        auto &skill = CustomSkillDataModule::GetCustomSkillData(skill_id);
        auto caster = CustomAgentDataModule::GetCustomAgentData(caster_id);
        skill.OnSkillActivation(caster, data.target_id);
    }

    void OnAttackHit(uint32_t cause_id, uint32_t target_id, bool is_projectile, GW::Constants::SkillID skill_id)
    {
        bool target_needs_manual_tracking = !Utils::ReceivesStoCEffects(target_id);

        auto &attack_skill = CustomSkillDataModule::GetCustomSkillData(skill_id);
        auto &caster = CustomAgentDataModule::GetCustomAgentData(cause_id);
        FixedVector<SkillEffect, 8> effects_to_apply;
        // attack_skill.GetOnHitEffects(caster, target_id, is_projectile, effects_to_apply);

        if (!Utils::ReceivesStoCEffects(target_id))
        {
            EffectTracking::ApplySkillEffects(target_id, cause_id, effects_to_apply);
        }

        EffectTracking::RemoveTrackers(cause_id, [](EffectTracking::EffectTracker &tracker)
                                       {
                                           return tracker.skill_id == GW::Constants::SkillID::Poison_Tip_Signet; //
                                       });
    }

    void OnProjectileHit(uint32_t target_id, uint32_t projectile_id)
    {
        auto &projectile = projectiles[projectile_id];
        auto projectile_skill_id = projectile.skill_id;
        projectile.impact_frame = UpdateManager::frame_id;

        if (!Utils::ReceivesStoCEffects(target_id))
        {
            EffectTracking::ApplySkillEffects(target_id, projectile.owner_agent_id, projectile.carried_effects);
        }

        OnAttackHit(projectile.owner_agent_id, target_id, true, projectile_skill_id);
    }

    void OnDamage(float hp_fract, uint32_t target_id, uint32_t cause_id, bool is_armor_ignoring)
    {
        const auto living_target = Utils::GetAgentLivingByID(target_id);
        if (living_target == nullptr)
            return;

        const auto min_hp_delta_normalized = -living_target->hp;
        const auto max_hp_delta_normalized = 1.0f - living_target->hp;
        const auto hp_delta_normalized = std::clamp(hp_fract, min_hp_delta_normalized, max_hp_delta_normalized);

        const auto hp_delta = hp_delta_normalized * living_target->max_hp;

        if (is_armor_ignoring && !Utils::ReceivesStoCEffects(cause_id))
        {
            // Try to deduce the required attribute rank for the observed damage and store that information

            auto &data = last_skill_cast[cause_id];
        }

        if (hp_fract <= 0 && Utils::GetAgentRelations(cause_id, target_id) == Utils::AgentRelations::Hostile)
        {
            // Damage

            auto Predicate = [](EffectTracking::EffectTracker &effect)
            {
                auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(effect.skill_id);
                return custom_sd.tags.EndsOnIncDamage;
            };
            EffectTracking::RemoveTrackers(target_id, Predicate);
        }

        PartyDataModule::RecordDamage(target_id, cause_id, hp_delta);

        // Utils::WriteStringToChat(L"%s damaged %s for %f", Utils::GetAgentName(cause).c_str(), Utils::GetAgentName(target).c_str(), hp_delta);
    }

    bool HasDebuggedThisFrame()
    {
        return last_debug_frame_id == UpdateManager::frame_id;
    }

    void WritePacketDebugInfo(GW::HookStatus *, const StoC::PacketBase *packet)
    {
        std::wstring message;
        std::vector<uint32_t> ids_to_check;

        switch (packet->header)
        {
            case StoC::GenericFloat::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::GenericFloat *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"GenericFloat: type={}, target={}, value={}",
                    Utils::GenericValueIDToString(p->type), p->agent_id, p->value
                );
                break;
            }
            case StoC::GenericModifier::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::GenericModifier *>(packet);
                ids_to_check.push_back(p->target_id);
                ids_to_check.push_back(p->cause_id);
                message = std::format(
                    L"GenericModifier: type={}, target={}, cause={}, value={}",
                    Utils::GenericValueIDToString(p->type), p->target_id, p->cause_id, p->value
                );
                break;
            }
            case StoC::GenericValue::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::GenericValue *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"GenericValue: type={}, target={}, value={}",
                    Utils::GenericValueIDToString(p->value_id), p->agent_id, p->value
                );
                break;
            }
            case StoC::GenericValueTarget::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::GenericValueTarget *>(packet);
                ids_to_check.push_back(p->target);
                ids_to_check.push_back(p->caster);
                message = std::format(
                    L"GenericValueTarget: type={}, target={}, caster={}, value={}",
                    Utils::GenericValueIDToString(p->Value_id), p->target, p->caster, p->value
                );
                break;
            }
            case StoC::AddEffect::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::AddEffect *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"AddEffect: agent={}, skill={}, attr_rank={}, effect_id={}, duration={}",
                    p->agent_id, p->skill_id, p->attribute_rank, p->effect_id, *(float *)&p->timestamp
                );
                break;
            }
            case (uint32_t)GAME_SMSG::EFFECT_RENEWED:
            {
                auto p = reinterpret_cast<const StoC::AddEffect *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"EffectRenewed: agent={}, skill={}, attr_rank={}, effect_id={}, duration={}",
                    p->agent_id, p->skill_id, p->attribute_rank, p->effect_id, *(float *)&p->timestamp
                );
                break;
            }
            case StoC::RemoveEffect::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::RemoveEffect *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"RemoveEffect: agent={}, effect_id={}",
                    p->agent_id, p->effect_id
                );
                break;
            }
            case StoC::SkillActivate::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::SkillActivate *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"SkillActivate: agent={}, skill={}, skill_instance={}",
                    p->agent_id, p->skill_id, p->skill_instance
                );
                break;
            }
            case StoC::SkillRecharge::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::SkillRecharge *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"SkillRecharge: agent={}, skill={}, skill_instance={}, recharge={}",
                    p->agent_id, p->skill_id, p->skill_instance, p->recharge
                );
                break;
            }
            case StoC::SkillRecharged::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::SkillRecharged *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"SkillRecharged: agent={}, skill={}, skill_instance={}",
                    p->agent_id, p->skill_id, p->skill_instance
                );
                break;
            }
            case StoC::PlayEffect::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::PlayEffect *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"PlayEffect: agent={}, effect_id={}, coords=({}, {}), plane={}, data5={}, data6={}",
                    p->agent_id, p->effect_id, p->coords.x, p->coords.y, p->plane, p->data5, p->data6
                );
                break;
            }
            case StoC::AgentState::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::AgentState *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"AgentState: agent={}, state={}",
                    p->agent_id, p->state
                );
                break;
            }
            case StoC::AgentAdd::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::AgentAdd *>(packet);
                // ids_to_check.push_back(p->agent_id);
                auto name = Utils::GetAgentName(p->agent_id);
                message = std::format(
                    LR"(AgentAdd:
                    agent_id={},
                    agent_type = {},
                    type = {},
                    unk3 = {},
                    position = ({}, {}),
                    unk4 = {},
                    unk5 = ({}, {}),
                    unk6 = {},
                    speed = {},
                    unk7 = {},
                    unknown_bitwise_1 = {},
                    allegiance_bits = {},
                    unk8[0] = {},
                    unk8[1] = {},
                    unk8[2] = {},
                    unk8[3] = {},
                    unk8[4] = {},
                    unk9 = ({}, {}),
                    unk10 = ({}, {}),
                    unk11[0] = {},
                    unk11[1] = {},
                    unk12 = ({}, {}),
                    unk13 = {}
                    )",
                    p->agent_id,
                    p->agent_type,
                    p->type,
                    p->unk3,
                    p->position.x, p->position.y,
                    p->unk4,
                    p->unk5.x, p->unk5.y,
                    p->unk6,
                    p->speed,
                    p->unk7,
                    p->unknown_bitwise_1,
                    p->allegiance_bits,
                    p->unk8[0], p->unk8[1], p->unk8[2], p->unk8[3], p->unk8[4],
                    p->unk9.x, p->unk9.y,
                    p->unk10.x, p->unk10.y,
                    p->unk11[0], p->unk11[1],
                    p->unk12.x, p->unk12.y,
                    p->unk13
                );
                break;
            }
            case StoC::InstanceLoadInfo::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::InstanceLoadInfo *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"InstanceLoadInfo: agent={}, map_id={}, is_explorable={}, is_observer={}",
                    p->agent_id, p->map_id, p->is_explorable, p->is_observer
                );
                break;
            }
            case StoC::AttributeUpdatePacket::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::AttributeUpdatePacket *>(packet);
                ids_to_check.push_back(p->agent_id);

                auto agent_name = Utils::GetAgentName(p->agent_id);
                auto attribute_name = Utils::StrToWStr(Utils::GetAttributeString((GW::Constants::AttributeByte)p->attribute));
                message = std::format(
                    L"AgentAttribute: agent={}({}), attribute={}({}), old_value={}, new_value={}",
                    p->agent_id, agent_name, (uint32_t)p->attribute, attribute_name, p->old_value, p->new_value
                );
                break;
            }
            case StoC::UpdateTitle::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::UpdateTitle *>(packet);
                auto agent_id = GW::Agents::GetControlledCharacterId();
                ids_to_check.push_back(agent_id);

                auto title_name = Utils::StrToWStr(Utils::GetTitleString((GW::Constants::TitleID)p->title_id));
                message = std::format(
                    L"TitleUpdate: title_id={}({}), new_value={}",
                    p->title_id, title_name, p->new_value
                );
                break;
            }
            case StoC::ProjectileCreated::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::ProjectileCreated *>(packet);
                ids_to_check.push_back(p->agent_id);
                message = std::format(
                    L"ProjectileCreated: agent={}, dest=({},{}), unk1={}, air_time={}, model_id={}, projectile_id={}, is_physical={}",
                    p->agent_id, (int32_t)p->destination.x, (int32_t)p->destination.y, p->unk1, p->air_time, p->model_id, p->projectile_id, p->is_physical
                );
                break;
            }
            case StoC::ProjectileDestroyed::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::ProjectileDestroyed *>(packet);
                message = std::format(
                    L"ProjectileDestroyed: caster={}, projectile_id={}, damage_type={}",
                    p->caster_id, p->projectile_id, (uint32_t)p->damage_type
                );
                break;
            }
            case StoC::ActivationDone::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::ActivationDone *>(packet);
                message = std::format(
                    L"ActivationDone: agent={}, skill={}",
                    p->agent_id, p->skill_id
                );
                break;
            }
            case StoC::Overcast::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::Overcast *>(packet);
                message = std::format(
                    L"Overcast: agent={}, overcast={}, loss_rate={}",
                    p->agent_id, p->overcast_amount, p->overcast_loss_rate
                );
                break;
            }
            case StoC::SpeechBubble::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::SpeechBubble *>(packet);
                message = std::format(
                    L"SpeechBubble: agent={}, message={}",
                    p->agent_id, Utils::DecodeString(p->message)
                );
                break;
            }
            case StoC::AdrenalineGain::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::AdrenalineGain *>(packet);
                message = std::format(
                    L"AdrenalineGain: agent_id={}, amount={}",
                    p->agent_id, p->amount
                );
                break;
            }
            case StoC::MovementTick::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::MovementTick *>(packet);
                message = std::format(
                    L"MovementTick: unk1={}",
                    p->unk1
                );
                break;
            }
            case StoC::MoveToPoint::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::MoveToPoint *>(packet);
                message = std::format(
                    L"MoveToPoint: agent={}, destination=({}, {}), unk1={}, unk2={}, unk3={}",
                    p->agent_id, p->destination.x, p->destination.y, p->unk1, p->unk2, p->unk3
                );
                break;
            }
            case StoC::UnkOnMapLoad::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::UnkOnMapLoad *>(packet);
                message = std::format(
                    L"UnkOnMapLoad: unk1={}, unk2={}, unk3={}, unk4={}",
                    p->unk1, p->unk2, p->unk3, p->unk4
                );
                break;
            }
            case StoC::AgentName::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::AgentName *>(packet);
                message = std::format(
                    L"AgentName: agent={}, name={}",
                    p->agent_id, Utils::DecodeString(p->name_enc)
                );
                break;
            }
            case StoC::InitialEffect::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::InitialEffect *>(packet);
                message = std::format(
                    L"InitialEffects: agent_id={}, unk_bitwise={}, padding1={}, effect_id={}, duration={}",
                    p->agent_id, p->unk_bitwise, p->padding1, p->effect_id, p->duration
                );
                break;
            }
            case StoC::AgentUnk2::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::AgentUnk2 *>(packet);
                message = std::format(
                    L"AgentUnk2: agent={}, unk1={}, unk2={}",
                    p->agent_id, p->unk1, p->unk2
                );
                break;
            }
            case StoC::AddExternalBond::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::AddExternalBond *>(packet);
                message = std::format(
                    L"AddExternalBond: caster_id={}, receiver_id={}, skill_id={}, effect_type={}, effect_id={}",
                    p->caster_id, p->receiver_id, p->skill_id, p->effect_type, p->effect_id
                );
                break;
            }
            case StoC::CreateNPC::STATIC_HEADER:
            {
                auto p = reinterpret_cast<const StoC::CreateNPC *>(packet);
                message = std::format(
                    L"CreateNPC: agent_id={}, allegiance_bits={}, agent_type={}, effect_id={}, lifetime={}",
                    p->agent_id, p->allegiance_bits, p->agent_type, p->effect_id, p->lifetime
                );
                break;
            }
            default:
            {
                message = std::format(
                    L"Unknown packet: header={}",
                    packet->header
                );
                break;
            }
        }

        bool is_related_to_player = ids_to_check.empty() ? true : false;
        for (const auto id : ids_to_check)
        {
            if (id == GW::Agents::GetControlledCharacterId() ||
                id == GW::Agents::GetTargetId())
            {
                is_related_to_player = true;
                break;
            }
        }

        bool should_write = DEBUG_PACKETS == 3 ||
                            (DEBUG_PACKETS == 2 && (is_related_to_player || HasDebuggedThisFrame())) ||
                            (DEBUG_PACKETS == 1 && is_related_to_player);

        if (!should_write)
            return;

        last_debug_frame_id = UpdateManager::frame_id;

        // if (!HasDebuggedThisFrame())
        // {
        //     auto delta_frames = UpdateManager::frame_id - last_debug_frame_id;
        //     last_debug_frame_id = UpdateManager::frame_id;
        //     auto delta_seconds = UpdateManager::elapsed_seconds - last_debug_date;
        //     last_debug_date = UpdateManager::elapsed_seconds;
        //     Utils::WriteStringToChat(0xFF00FF00, L"------------ %u frames, %f seconds ------------", delta_frames, delta_seconds);
        // }

        Utils::WriteToChat(message.c_str(), is_related_to_player ? NULL : 0xFFFF0000);
    }

    void GenericFloatCallback(GW::HookStatus *, const StoC::GenericFloat *packet)
    {
        if (packet->type == StoC::GenericValueID::energy_spent)
        {
            const auto agent_id = packet->agent_id;

            const auto living_agent = Utils::GetAgentLivingByID(agent_id);
            if (living_agent == nullptr)
                return;

            const auto energy_spent = std::round(-(living_agent->max_energy * packet->value));

            auto &data = last_skill_cast[agent_id];

            data.energy_spent = energy_spent;
            data.energy_spent_frame = UpdateManager::frame_id;
        }

        if (packet->type == StoC::GenericValueID::casttime)
        {
            const auto agent_id = packet->agent_id;
            const auto casttime = packet->value;

            auto &data = last_skill_cast[agent_id];

            data.casttime = casttime;
            data.casttime_frame = UpdateManager::frame_id;
        }

        if (packet->type == StoC::GenericValueID::change_health_regen)
        {
            // const auto agent_id = packet->agent_id;
            // const auto map_agent = GW::Agents::GetMapAgentByID(agent_id);
            // if (map_agent == nullptr)
            //     return;

            // const auto health_regen = map_agent->health_regen;
            // const auto new_helth_regen = packet->value;
            // const auto delta = new_helth_regen - health_regen;
            // const auto max_hp = map_agent->max_health;

            // const auto living_agent = Utils::GetAgentLivingByID(agent_id);
            // if (living_agent == nullptr)
            //     return;

            // const auto living_max_hp = living_agent->max_hp;
        }
    }

    void GenericModifierCallback(GW::HookStatus *, const StoC::GenericModifier *packet)
    {
        const auto target_id = packet->target_id;
        const auto cause_id = packet->cause_id;

        if (packet->type == StoC::GenericValueID::casttime)
        {
            const auto casttime = packet->value;
            const auto skill_id = last_skill_cast[target_id].skill_id;

            CheckCasttimeCalculation(target_id, casttime, skill_id);
        }

        bool is_damage = packet->type == StoC::GenericValueID::damage;
        bool is_armor_ignoring = packet->type == StoC::GenericValueID::armorignoring;

        //         if (is_damage)
        //         {
        //             TrimProjectiles();

        //             for (uint32_t i = 1; i < projectiles.size(); i++) // Skip the first projectile, that one is never valid
        //             {
        //                 auto &projectile = projectiles[i];

        //                 if (!projectile.IsValid())
        //                     continue;

        //                 if (projectile.owner_agent_id != cause_id)
        //                     continue;

        //                 if (projectile.calculated_target_id != target_id) // We assume a projectile may only ever hit the calculated target
        //                     continue;

        //                 auto timestamp_now = GW::MemoryMgr::GetSkillTimer();
        //                 auto timestamp_min = projectile.timestamp_impact - 100; // 100 ms before estimation
        //                 auto timestamp_max = projectile.timestamp_impact + 200; // 100 ms after estimation
        //                 if (timestamp_min <= timestamp_now && timestamp_now <= timestamp_max)
        //                 {
        // #ifdef _DEBUG
        //                     // auto time_error = (int32_t)projectile.timestamp_impact - (int32_t)timestamp_now;
        //                     // Utils::FormatToChat(0xffffff00, L"Projectile hit {} ms before expected", time_error);
        // #endif
        //                     OnProjectileHit(target_id, i);
        //                 }
        //             }
        //         }

        if (is_damage || is_armor_ignoring)
        {
            const auto hp_fract = packet->value;
            OnDamage(hp_fract, target_id, cause_id, is_armor_ignoring);
        }
    }

    void GenericValueCallback(GW::HookStatus *, const StoC::GenericValue *packet)
    {
        if (packet->value_id == StoC::GenericValueID::melee_attack_finished)
        {
            const auto cause_id = packet->agent_id;
            const auto &data = last_skill_cast[cause_id];
            const auto target_id = data.target_id;
            // OnAttackHit(cause_id, target_id, false, data.skill_id);
        }

        if (packet->value_id == StoC::GenericValueID::skill_activated)
        {
            const auto caster = packet->agent_id;
            const auto skill_id = (GW::Constants::SkillID)packet->value;

            OnSkillActivationStart(caster, caster, skill_id);
        }

        if (packet->value_id == StoC::GenericValueID::skill_finished ||
            packet->value_id == StoC::GenericValueID::attack_skill_finished)
        {
            const auto caster_id = packet->agent_id;

            OnSkillActivationFinish(caster_id);
        }

        if (packet->value_id == StoC::GenericValueID::instant_skill_activated)
        {
            const auto caster_id = packet->agent_id;
            const auto skill_id = (GW::Constants::SkillID)packet->value;

            auto &skill = *GW::SkillbarMgr::GetSkillConstantData(skill_id);
            OnSkillActivationStart(caster_id, caster_id, skill_id);
            if (!skill.target)
            {
                // We assume that skills without a target are meant for caster
                // and those with target get an effect_on_target packet later
                OnSkillActivationFinish(caster_id);
            }
        }
    }

    void GenericValueTargetCallback(GW::HookStatus *, const StoC::GenericValueTarget *packet)
    {
        if (packet->Value_id == StoC::GenericValueID::skill_activated ||
            packet->Value_id == StoC::GenericValueID::attack_skill_activated ||
            packet->Value_id == StoC::GenericValueID::attack_started) // We include this so that the last skill cast is cleared
        {
            const auto caster = packet->target; // For skill_activated, the target and caster are swapped
            const auto target = packet->caster;
            const auto skill_id = (GW::Constants::SkillID)packet->value; // For attack_started, the value is 0 a.k.a. no skill

            OnSkillActivationStart(caster, target, skill_id);
        }

        if (packet->Value_id == StoC::GenericValueID::effect_on_target)
        {
            const auto caster = packet->caster;
            const auto target = packet->target;

            auto &data = last_skill_cast[caster];
            data.target_id = target;

            OnSkillActivationFinish(caster);
        }

        if (packet->Value_id == StoC::GenericValueID::attack_missed)
        {
            Utils::FormatToChat(0xffffff00, L"Target dodged the attack, value: {}", packet->value);
        }
    }

    void ProjectileLaunchedCallback(GW::HookStatus *, const StoC::AgentProjectileLaunched *packet)
    {
        const auto caster_id = packet->agent_id;
        const auto dest = packet->destination;
        const auto air_time = *(float *)&packet->unk2;
        const auto projectile_id = packet->unk4;
        const auto projectile_model_id = packet->unk3;
        // const auto is_physical_dmg = packet->is_attack;

        auto &data = last_skill_cast[caster_id];

        auto timestamp_now = GW::MemoryMgr::GetSkillTimer();

        projectiles.resize(std::max((size_t)projectile_id + 1, projectiles.size()));
        auto &projectile = projectiles[projectile_id];

        auto target_id = Utils::GetClosestAgentID(dest);

        // #ifdef _DEBUG
        //         GW::Vec2f target_pos = GW::Agents::GetAgentByID(target_id)->pos;
        //         auto local = dest - target_pos;
        //         auto distance_sqrd = Utils::Dot(local, local);
        //         auto dest_error = std::sqrt(distance_sqrd);
        //         Utils::FormatToChat(0xffffff00, L"Projectile launched, dest_error: {}", dest_error);
        // #endif

        OnSkillActivationFinish(caster_id);

        projectile = {}; // Clear the data
        projectile.owner_agent_id = caster_id;
        projectile.calculated_target_id = target_id;
        projectile.original_target_id = data.target_id;
        projectile.skill_id = data.skill_id;
        projectile.timestamp_impact = timestamp_now + static_cast<DWORD>(air_time * 1000.f);
        projectile.destination = dest;

        auto &projectile_skill = CustomSkillDataModule::GetCustomSkillData(data.skill_id);
        auto &caster = CustomAgentDataModule::GetCustomAgentData(caster_id);

        auto carried_effects = projectile.carried_effects.WrittenSpan();
        // projectile_skill.GetProjectileEffects(caster, carried_effects);
    }

    // Only called for heroes
    void AddEffectCallback(GW::HookStatus *, const StoC::AddEffect *packet)
    {
        const auto target_id = packet->agent_id;
        const auto skill_id = (GW::Constants::SkillID)packet->skill_id;
        const auto current_frame = UpdateManager::frame_id;
        const auto attribute_rank = packet->attribute_rank;
        const auto effect_id = packet->effect_id;
        const auto duration = *(float *)&packet->timestamp;

        uint32_t caster_id = 0;

        auto effect = Utils::GetEffectByID(target_id, effect_id);
        SOFT_ASSERT(effect);
        if (effect)
        {
            caster_id = effect->agent_id;
        }

        if (caster_id == 0)
        {
            for (const auto &data : last_skill_cast) // Find the source of the effect
            {
                const auto cast_data = data.second;
                if (cast_data.finished_frame == current_frame &&
                    cast_data.skill_id == skill_id &&
                    (cast_data.caster == target_id ||
                     cast_data.target_id == target_id))
                {
                    caster_id = cast_data.caster;
                    break;
                }
            }
        }

        EffectTracking::EffectTracker new_tracker = {};
        new_tracker.cause_agent_id = caster_id;
        new_tracker.skill_id = skill_id;
        new_tracker.attribute_rank = attribute_rank;
        new_tracker.effect_id = effect_id;
        new_tracker.duration_sec = static_cast<uint32_t>(duration);

        // EffectTracking::AddTracker(target_id, new_tracker);

        if (caster_id)
        {
            auto &custom_ad = CustomAgentDataModule::GetCustomAgentData(caster_id);
            auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);
            auto base_duration = custom_sd.ResolveBaseDuration(custom_ad, attribute_rank);
            auto source_skill_id = last_skill_cast[caster_id].skill_id;
            const auto calc_duration = (float)Utils::CalculateDuration(*custom_sd.skill, base_duration, caster_id);

            const auto duration_diff = std::abs(duration - calc_duration);
            const auto success = duration_diff < 0.001f;

            WriteDebugCheckMessage(caster_id, skill_id, success, L"duration", calc_duration, duration);
        }
    }

    // Only called for heroes
    void RemoveEffectCallback(GW::HookStatus *, const StoC::RemoveEffect *packet)
    {
        const auto agent_id = packet->agent_id;
        const auto effect_id = packet->effect_id;

        EffectTracking::RemoveTrackers(agent_id, [&](EffectTracking::EffectTracker &effect)
                                       { return effect.effect_id == effect_id; });

        // if (effect_id > 0)
        // {
        //     // We schedule the removal of the effect to happen at the end of the frame.
        //     // Otherwise important effects, such as Poison Tip Signet, are lost before the arrow hit the target
        //     EffectTracking::ScheduleTrackerRemoval(agent_id, effect_id);
        // }
    }

    // Only called for heroes
    void SkillActivateCallback(GW::HookStatus *, const StoC::SkillActivate *packet)
    {
        // const auto caster = packet->agent_id;

        // auto data = last_skill_cast[caster];

        // data.caster = caster;
        // data.target = caster; // For now we assume the caster is the target, we change this later if a packet tells us otherwise
        // data.skill_id = (GW::Constants::SkillID)packet->skill_id;
        // data.finished_frame = 0;
        // data.interrupted_frame = 0;

        // last_skill_cast[caster] = data;
    }

    void SkillRechargeCallback(GW::HookStatus *, const StoC::SkillRecharge *packet)
    {
        if (packet->skill_id == 0)
            return;

        const auto agent_id = packet->agent_id;
        const auto skill_id = (GW::Constants::SkillID)packet->skill_id;

        const auto recharge = (float)packet->recharge;
        const auto opt_calc = Utils::CalculateRecharge(agent_id, skill_id);
        if (opt_calc)
        {
            const auto &calc = *opt_calc;
            const auto calc_recharge = (float)(calc.half_chance && recharge == calc.half_rounded ? calc.half_rounded : calc.rounded);
            const auto success = recharge == calc_recharge;

            WriteDebugCheckMessage(agent_id, skill_id, success, L"recharge", calc_recharge, recharge);
        }
    }

    void AgentAddCallback(GW::HookStatus *, const StoC::AgentAdd *packet)
    {
        const auto agent_id = packet->agent_id;

        if (!Utils::ReceivesStoCEffects(agent_id))
        {
            auto agent_tracker = EffectTracking::agent_trackers.find(agent_id);
            if (agent_tracker != EffectTracking::agent_trackers.end())
            {
                for (auto &effect : agent_tracker->second.effects)
                {
                    // Utils::FormatToChat(L"setting timestamp_removal_immunity to {}", GW::MemoryMgr::GetSkillTimer() + GetPing() * 2);
                    // effect.timestamp_removal_immunity = GW::MemoryMgr::GetSkillTimer() + GetPing() * 2; // * 2 is just a safe margin
                }
            }
        }

        if (packet->agent_type == 0x20000000) // NPC
        {
            // FoeData foe_data;
            // foe_data.agent_id = packet->agent_id;
            // // foe_data.hp_pips = 0;
            // // foe_data.energy_pips = 0;

            // const auto agent = GW::Agents::GetAgentByID(packet->agent_id);
            // if (agent == nullptr)
            //     return;

            // const auto living = agent->GetAsAgentLiving();
            // if (living == nullptr)
            //     return;

            // PartyDataModule::foes.insert({packet->agent_id, foe_data});
        }
    }

    void AgentRemoveCallback(GW::HookStatus *, const StoC::AgentRemove *packet)
    {
        // PartyDataModule::foes.erase(packet->agent_id);
    }

    void InstanceLoadInfoCallback(GW::HookStatus *, const StoC::InstanceLoadInfo *packet)
    {
        if (packet->is_explorable && !packet->is_observer)
        {
        }
        else
        {
        }
    }

    void AttributeUpdateCallback(GW::HookStatus *, const StoC::AttributeUpdatePacket *packet)
    {
        auto &agent_data = CustomAgentDataModule::GetCustomAgentData(packet->agent_id);
        agent_data.SetAttribute(AttributeOrTitle(packet->attribute), packet->new_value);
    }

    void TitleUpdateCallback(GW::HookStatus *, const StoC::UpdateTitle *packet)
    {
        auto id = AttributeOrTitle((GW::Constants::TitleID)packet->title_id);
        auto new_value = packet->new_value;

        FixedVector<uint32_t, 8> agent_ids;
        Utils::GetControllableAgentsOfPlayer(agent_ids);
        for (auto agent_id : agent_ids)
        {
            auto &agent_data = CustomAgentDataModule::GetCustomAgentData(agent_id);
            agent_data.SetAttribute(id, new_value);
        }
    }

    uint32_t current_ping = 0;
    void PingCallback(GW::HookStatus *, StoC::PingReply *packet)
    {
        const auto ping = packet->ping;

        if (current_ping < ping)
        {
            current_ping = ping;
        }
        else
        {
            static const float alpha = 0.1f; // Smoothing factor for running average
            current_ping = static_cast<uint32_t>(alpha * ping + (1 - alpha) * current_ping);
        }
    }

    uint32_t GetPing()
    {
        return current_ping;
    }

    void Initialize()
    {
        // return;
        GW::StoC::RegisterPacketCallback<StoC::GenericFloat>(&packet_reader_entry, &GenericFloatCallback);
        GW::StoC::RegisterPacketCallback<StoC::GenericModifier>(&packet_reader_entry, &GenericModifierCallback);
        GW::StoC::RegisterPacketCallback<StoC::GenericValue>(&packet_reader_entry, &GenericValueCallback);
        GW::StoC::RegisterPacketCallback<StoC::GenericValueTarget>(&packet_reader_entry, &GenericValueTargetCallback);
        // GW::StoC::RegisterPacketCallback<StoC::AgentProjectileLaunched>(&packet_reader_entry, &ProjectileLaunchedCallback);
        // GW::StoC::RegisterPacketCallback<StoC::AddEffect>(&packet_reader_entry, &AddEffectCallback, 1); // After because we want to read the effect data
        // GW::StoC::RegisterPacketCallback<StoC::RemoveEffect>(&packet_reader_entry, &RemoveEffectCallback, 0xffff);
        // GW::StoC::RegisterPacketCallback<StoC::SkillActivate>(&packet_reader_entry, &SkillActivateCallback);
        GW::StoC::RegisterPacketCallback<StoC::SkillRecharge>(&packet_reader_entry, &SkillRechargeCallback);
        GW::StoC::RegisterPacketCallback<StoC::AgentAdd>(&packet_reader_entry, &AgentAddCallback);
        GW::StoC::RegisterPacketCallback<StoC::AgentRemove>(&packet_reader_entry, &AgentRemoveCallback);
        // GW::StoC::RegisterPacketCallback<StoC::InstanceLoadInfo>(&packet_reader_entry, &InstanceLoadInfoCallback);
        GW::StoC::RegisterPacketCallback<StoC::AttributeUpdatePacket>(&packet_reader_entry, &AttributeUpdateCallback);
        GW::StoC::RegisterPacketCallback<StoC::UpdateTitle>(&packet_reader_entry, &TitleUpdateCallback);
        GW::StoC::RegisterPacketCallback<StoC::PingReply>(&packet_reader_entry, PingCallback, 0x800);

        if (DEBUG_PACKETS)
        {
            // for (auto &header : headers_to_debug)
            for (uint32_t header = 0; header < 500; header++)
            {
                if (header == (uint32_t)GAME_SMSG::AGENT_MOVEMENT_TICK ||
                    header == (uint32_t)GAME_SMSG::AGENT_MOVE_TO_POINT ||
                    header == (uint32_t)GAME_SMSG::AGENT_STOP_MOVING ||
                    header == (uint32_t)GAME_SMSG::AGENT_UPDATE_EFFECTS ||
                    header == (uint32_t)GAME_SMSG::AGENT_UPDATE_ROTATION ||
                    header == (uint32_t)GAME_SMSG::PING_REQUEST ||
                    header == (uint32_t)GAME_SMSG::PING_REPLY ||
                    header == StoC::UnkOnMapLoad::STATIC_HEADER ||
                    header == (uint32_t)GAME_SMSG::AGENT_UPDATE_DESTINATION)
                    continue;
                GW::StoC::RegisterPacketCallback(&packet_reader_debug_entry, header, &WritePacketDebugInfo, 0);
            }
        }
    }

    void Terminate()
    {
        GW::StoC::RemoveCallbacks(&packet_reader_entry);
        GW::StoC::RemoveCallbacks(&packet_reader_debug_entry);
    }
}
