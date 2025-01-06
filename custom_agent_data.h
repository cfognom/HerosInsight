#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <custom_skill_data.h>
#include <attribute_or_title.h>
#include <attribute_store.h>

namespace HerosInsight
{
    struct CustomAgentData
    {
        uint32_t agent_id = 0;

        uint8_t signet_of_illusions_charges = 0;

        void SetAttribute(AttributeOrTitle id, uint8_t level);
        std::optional<uint8_t> GetAttribute(AttributeOrTitle id) const;
        uint8_t GetOrEstimateAttribute(AttributeOrTitle id) const;
        uint8_t GetAttrLvlForSkill(const CustomSkillData &custom_sd) const;

        GW::AgentLiving *TryGetAgentLiving() const;

        AttributeStore GetAttributes() const;

    private:
        AttributeStore attribute_cache;
    };

    namespace CustomAgentDataModule
    {
        void Initialize();
        void Terminate();

        CustomAgentData *TryGetCustomAgentData(uint32_t agent_id);
        CustomAgentData &GetCustomAgentData(uint32_t agent_id);
    }
}