#pragma once

#include <Windows.h>
#include <unordered_map>
#include <unordered_set>

#include <bitset>
#include <map>
#include <string>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GadgetContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/PreGameContext.h>

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

#include <debug_display.h>
#include <energy_display.h>
#include <packet_reader.h>
#include <utils.h>

namespace HerosInsight
{
    struct PartyMemberState
    {
        uint32_t agent_id;
        uint8_t n_used_skills_this_frame;
        float accum_incoming_damage;
        float accum_incoming_healing;
        float accum_outgoing_damage;
        float accum_outgoing_healing;
        float damaged_date_seconds;
    };

    struct PartyMemberData
    {
        uint32_t agent_id;
        uint32_t party_index;
        bool is_controllable;
        PartyMemberState state;
        std::string decoded_name;

        GW::AgentLiving *GetAgent() const;
        GW::Skillbar *GetSkillbar() const;
        GW::SkillbarSkill (*GetSkills() const)[8];
        GW::AgentMovement *GetMovement() const;
        GW::AgentSummaryInfo *GetSummaryInfo() const;
        GW::AgentInfo *GetInfo() const;

        bool HasSkill(GW::Constants::SkillID skill_id) const;
    };

    namespace PartyDataModule
    {
        extern std::vector<PartyMemberData> party_members;

        PartyMemberData *GetHeroData(uint32_t hero_id);
        PartyMemberData *GetHeroDataByID(uint32_t agent_id);
        PartyMemberState *GetHeroState(uint32_t hero_id);

        void Initialize();
        void RecordDamage(uint32_t target, uint32_t cause, float hp_delta);
        void Update();
        void Terminate();
    }
}