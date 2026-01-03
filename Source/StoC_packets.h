#pragma once

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/Utilities/Hook.h>
#include <custom_skill_data.h>

namespace StoC = GW::Packet::StoC;

namespace GW::Packet::StoC
{
    namespace GenericValueID
    {
        const uint32_t attack_missed = 38;       // GenericValueTarget
        const uint32_t max_hp_update = 42;       // GenericValue
        const uint32_t change_energy_regen = 43; // GenericFloat
    }

    struct ProjectileCreated : StoC::Packet<ProjectileCreated>
    {
        uint32_t agent_id;
        Vec2f destination;
        uint32_t unk1; // always 0?
        float air_time;
        uint32_t model_id;
        uint32_t projectile_id;
        uint32_t is_physical;
    };
    constexpr uint32_t StoC::Packet<ProjectileCreated>::STATIC_HEADER = GAME_SMSG_AGENT_PROJECTILE_LAUNCHED;

    struct ProjectileDestroyed : StoC::Packet<ProjectileDestroyed>
    {
        uint32_t caster_id;
        uint32_t projectile_id;
        HerosInsight::DamageType damage_type;
    };
    constexpr uint32_t StoC::Packet<ProjectileDestroyed>::STATIC_HEADER = 166;

    // Only for heroes and player
    struct ActivationDone : StoC::Packet<ActivationDone>
    {
        uint32_t agent_id;
        uint32_t skill_id;
    };
    constexpr uint32_t StoC::Packet<ActivationDone>::STATIC_HEADER = GAME_SMSG_SKILL_ACTIVATED;

    struct Overcast : StoC::Packet<Overcast>
    {
        uint32_t agent_id;
        float overcast_amount;
        float overcast_loss_rate;
    };
    constexpr uint32_t StoC::Packet<Overcast>::STATIC_HEADER = 156;

    struct AttributeUpdatePacket : StoC::Packet<AttributeUpdatePacket>
    {
        uint32_t agent_id;
        GW::Constants::Attribute attribute;
        uint32_t old_value;
        uint32_t new_value;
    };
    constexpr uint32_t StoC::Packet<AttributeUpdatePacket>::STATIC_HEADER = GAME_SMSG_AGENT_UPDATE_ATTRIBUTE;

    struct PingReply : StoC::Packet<PingReply>
    {
        uint32_t ping;
    };
    constexpr uint32_t StoC::Packet<PingReply>::STATIC_HEADER = GAME_SMSG_PING_REPLY;

    struct PingRequest : StoC::Packet<PingRequest>
    {
        uint32_t unk;
    };
    constexpr uint32_t StoC::Packet<PingRequest>::STATIC_HEADER = GAME_SMSG_PING_REQUEST;

    struct AdrenalineGain : StoC::Packet<AdrenalineGain>
    {
        uint32_t agent_id;
        uint32_t amount;
    };
    constexpr uint32_t StoC::Packet<AdrenalineGain>::STATIC_HEADER = 206;

    struct MovementTick : StoC::Packet<MovementTick>
    {
        uint32_t unk1;
        //...
    };
    constexpr uint32_t StoC::Packet<MovementTick>::STATIC_HEADER = GAME_SMSG_AGENT_MOVEMENT_TICK;

    struct MoveToPoint : StoC::Packet<MoveToPoint>
    {
        uint32_t agent_id;
        GW::Vec2f destination;
        uint32_t unk1;
        uint32_t unk2;
        uint32_t unk3;
    };
    constexpr uint32_t StoC::Packet<MoveToPoint>::STATIC_HEADER = GAME_SMSG_AGENT_MOVE_TO_POINT;

    struct UnkOnMapLoad : StoC::Packet<UnkOnMapLoad>
    {
        uint32_t unk1;
        uint32_t unk2;
        uint32_t unk3;
        uint32_t unk4;
    };
    constexpr uint32_t StoC::Packet<UnkOnMapLoad>::STATIC_HEADER = 238;

    struct InitialEffect : StoC::Packet<InitialEffect>
    {
        uint32_t agent_id;
        uint32_t unk_bitwise;
        uint32_t padding1;
        uint32_t effect_id;
        float duration;
    };
    constexpr uint32_t StoC::Packet<InitialEffect>::STATIC_HEADER = GAME_SMSG_AGENT_INITIAL_EFFECTS;

    struct CreateNPC : StoC::Packet<CreateNPC>
    {
        uint32_t agent_id;
        uint32_t allegiance_bits;
        uint32_t agent_type; // Bitwise field. 0x20000000 = NPC | PlayerNumber, 0x30000000 = Player | PlayerNumber, 0x00000000 = Signpost
        uint32_t effect_id;
        float lifetime;
    };
    constexpr uint32_t StoC::Packet<CreateNPC>::STATIC_HEADER = GAME_SMSG_AGENT_CREATE_NPC;
}