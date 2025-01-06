#include <unordered_map>
#include <unordered_set>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/GadgetContext.h>
#include <GWCA/Context/GameContext.h>

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>
#include <GWCA/Constants/Constants.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/List.h>

#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <debug_display.h>
#include <update_manager.h>
#include <utils.h>
#include <effect_tracking.h>
#include <worldspaceUI.h>

#include "party_data.h"

#include <format>

namespace HerosInsight
{
    GW::AgentLiving *PartyMemberData::GetAgent() const
    {
        auto agent = GW::Agents::GetAgentByID(agent_id);
        if (agent == nullptr)
            return nullptr;
        return agent->GetAsAgentLiving();
    }

    GW::Skillbar *PartyMemberData::GetSkillbar() const
    {
        assert(is_controllable);
        return GW::SkillbarMgr::GetHeroSkillbar(party_index);
    }

    GW::SkillbarSkill (*PartyMemberData::GetSkills() const)[8]
    {
        assert(is_controllable);
        return &GetSkillbar()->skills;
    }

    GW::AgentMovement *PartyMemberData::GetMovement() const
    {
        return Utils::GetAgentMovementByID(agent_id);
    }

    GW::AgentSummaryInfo *PartyMemberData::GetSummaryInfo() const
    {
        return Utils::GetAgentSummaryInfoByID(agent_id);
    }

    GW::AgentInfo *PartyMemberData::GetInfo() const
    {
        return Utils::GetAgentInfoByID(agent_id);
    }

    bool PartyMemberData::HasSkill(GW::Constants::SkillID skill_id) const
    {
        const auto skills = GetSkills();
        if (skills == nullptr)
            return false;

        for (int i = 0; i < 8; ++i)
        {
            if ((*skills)[i].skill_id == skill_id)
            {
                return true;
            }
        }
        return false;
    }

    namespace PartyDataModule
    {
        std::vector<PartyMemberData> party_members;

        PartyMemberData *GetHeroData(uint32_t hero_index)
        {
            return &party_members[hero_index];
        }

        PartyMemberData *GetHeroDataByID(uint32_t agent_id)
        {
            for (auto &hero_data : party_members)
            {
                if (hero_data.agent_id == agent_id)
                    return &hero_data;
            }
            return nullptr;
        }

        PartyMemberState *GetHeroState(uint32_t hero_index)
        {
            return &GetHeroData(hero_index)->state;
        }

        void name_decoded_callback(void *param, const wchar_t *decoded_name)
        {
            auto party_index = reinterpret_cast<uint32_t>(param);

            if (party_index >= party_members.size())
                return;

            std::string name;
            if (decoded_name == nullptr || decoded_name[0] == 0)
                name = "Unknown";
            else
                name = Utils::WStrToStr(decoded_name);

            party_members[party_index].decoded_name = name;
        }

        void StartDecodingPartyMemberName(uint32_t party_index)
        {
            if (party_index >= party_members.size())
                return;

            const auto data = &party_members[party_index];

            data->decoded_name = "Decoding...";

            auto enc_name = GW::Agents::GetAgentEncName(data->agent_id);
            GW::UI::AsyncDecodeStr(enc_name, name_decoded_callback, reinterpret_cast<void *>(party_index));
        }

        void AddPartyMember(uint32_t agent_id)
        {
            if (agent_id == 0)
                return;

            const auto party_index = party_members.size();
            PartyMemberData hero_data = {};
            hero_data.party_index = party_index;
            hero_data.agent_id = agent_id;
            hero_data.is_controllable = GW::PartyMgr::GetAgentHeroID(agent_id) != 0;

            PartyMemberState state = {};
            state.agent_id = agent_id;
            state.n_used_skills_this_frame = 0;
            state.accum_incoming_damage = 0;
            state.accum_incoming_healing = 0;
            state.accum_outgoing_damage = 0;
            state.accum_outgoing_healing = 0;

            hero_data.state = state;

            party_members.push_back(hero_data);

            StartDecodingPartyMemberName(party_index);
        }

        void Initialize()
        {
            const auto info = GW::PartyMgr::GetPartyInfo();
            if (info == nullptr)
                return;

            const auto party_size = info->GetPartySize();

            for (const auto player_member : info->players)
            {
                const auto player = GW::PlayerMgr::GetPlayerByID(player_member.login_number);
                auto agent_id = player->agent_id;
                AddPartyMember(agent_id);

                for (const auto hero : info->heroes)
                {
                    if (hero.owner_player_id == player_member.login_number)
                    {
                        agent_id = hero.agent_id;
                        AddPartyMember(agent_id);
                    }
                }
            }

            for (const auto henchman : info->henchmen)
            {
                AddPartyMember(henchman.agent_id);
            }
        }

        void RecordDamage(uint32_t target, uint32_t cause, float hp_delta)
        {
            if (hp_delta == 0)
                return;

            const bool self_inflicted = cause == target;

            auto cause_hero = PartyDataModule::GetHeroDataByID(cause);
            if (cause_hero) // A hero caused it
            {
                if (hp_delta < 0)
                {
                    const auto damage = -hp_delta;
                    if (!self_inflicted)
                    {
                        cause_hero->state.accum_outgoing_damage += damage;
                    }
                }
                else
                {
                    const auto healing = hp_delta;
                    cause_hero->state.accum_outgoing_healing += healing;
                }
            }
            auto target_hero = PartyDataModule::GetHeroDataByID(target);
            if (target_hero) // A hero received it
            {
                if (hp_delta < 0)
                {
                    const auto damage = -hp_delta;
                    target_hero->state.accum_incoming_damage += damage;
                    if (!self_inflicted)
                    {
                        target_hero->state.damaged_date_seconds = UpdateManager::elapsed_seconds;
                    }
                }
                else
                {
                    const auto healing = hp_delta;
                    target_hero->state.accum_incoming_healing += healing;
                }
            }
        }

        void Update()
        {
            for (auto &hero : party_members)
            {
                hero.state.n_used_skills_this_frame = 0;
            }

            const auto the_player_id = GW::Agents::GetControlledCharacterId();

            for (auto &[agent_id, agent_trackers] : EffectTracking::agent_trackers)
            {
                auto handled_casters = FixedSet<uint64_t, 64>();
                for (auto &effect : agent_trackers.effects)
                {
                    auto skill_id = effect.skill_id;

                    const auto caster_id = effect.cause_agent_id;
                    const auto opt_hp_per_sec = Utils::CalculateEffectHPPerSec(skill_id, effect.attribute_level);

                    auto caster_hp_per_sec = opt_hp_per_sec ? opt_hp_per_sec.value().caster_hp_sec : 0;
                    auto target_hp_per_sec = opt_hp_per_sec ? opt_hp_per_sec.value().target_hp_sec : 0;

                    if (effect.is_active)
                    {
                        // The target is only affected by the active (non-dormant) effect
                        const float target_delta_hp = target_hp_per_sec * UpdateManager::delta_seconds;
                        RecordDamage(agent_id, caster_id, target_delta_hp);
                        // if (caster_id == the_player_id)
                        {
                            WorldSpaceUI::PushSkillForDraw(agent_id, skill_id, target_delta_hp, effect.begin_timestamp, effect.duration_sec);
                        }
                    }

                    if (caster_hp_per_sec)
                    {
                        const auto casterskill_key = ((uint64_t)(caster_id) << 32) | (uint64_t)skill_id;
                        if (!handled_casters.has(casterskill_key))
                        {
                            // Each caster is affected by its oldest effect (thats how the game does it, atleast with life siphon)
                            const auto success = handled_casters.insert(casterskill_key);
                            SOFT_ASSERT(success, L"The handled_caster_ids set is full, damage tracking may be inaccurate");
                            if (!success)
                                break;
                            const float caster_delta_hp = caster_hp_per_sec * UpdateManager::delta_seconds;
                            RecordDamage(caster_id, caster_id, caster_delta_hp);
                            if (caster_id == the_player_id)
                            {
                                WorldSpaceUI::PushSkillForDraw(caster_id, skill_id, caster_delta_hp, effect.begin_timestamp, effect.duration_sec);
                            }
                        }
                    }
                }
            }
        }

        void Terminate()
        {
            party_members.clear();
        }
    }
}