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

#include <constants.h>
#include <debug_display.h>
#include <party_data.h>
#include <update_manager.h>

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
#include <GWCA/GameEntities/Title.h>

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
#include <custom_skill_data.h>
#include <utils.h>

#include "custom_agent_data.h"

namespace HerosInsight
{

    GW::AgentLiving *CustomAgentData::TryGetAgentLiving() const
    {
        return Utils::GetAgentLivingByID(agent_id);
    }

    AttributeStore CustomAgentData::GetAttributes() const
    {
        auto attribute_span = Utils::GetAgentAttributeSpan(agent_id);
        if (!attribute_span.empty())
        {
            return AttributeStore(attribute_span);
        }
        return attribute_cache;
    }

    uint8_t EstimateAttribute(uint32_t agent_id)
    {
#ifdef _DEBUG
        if (Utils::IsControllableAgentOfPlayer(agent_id))
        {
            SOFT_ASSERT(false, L"Do not EstimateAttribute for player agents, use exact value");
        }
#endif

        auto agent_living = Utils::GetAgentLivingByID(agent_id);
        if (agent_living == nullptr)
            return 6;

        return std::min((agent_living->level * 12) / 20, 20);
    }

    void CustomAgentData::SetAttribute(AttributeOrTitle id, uint8_t level)
    {
        attribute_cache.SetAttribute(id, level);
    }

    std::optional<uint8_t> CustomAgentData::GetAttribute(AttributeOrTitle id) const
    {
        if (id.IsAttribute())
        {
            auto attribute_span = Utils::GetAgentAttributeSpan(agent_id);
            if (!attribute_span.empty())
            {
                return {attribute_span[(size_t)id.GetAttribute()].level};
            }
        }
        else if (id.IsTitle())
        {
            if (Utils::IsControllableAgentOfPlayer(agent_id))
            {
                auto world_ctx_ptr = GW::GetWorldContext();
                if (world_ctx_ptr)
                {
                    auto &titles = world_ctx_ptr->titles;
                    auto &title_tiers = world_ctx_ptr->title_tiers;
                    if (titles.valid() && title_tiers.valid())
                    {
                        auto title_id = id.GetTitle();
                        auto &title_tier = title_tiers.at(titles.at(static_cast<size_t>(title_id)).current_title_tier_index);
                        return {(uint8_t)(std::min(title_tier.tier_number, 5u) * 3u)};
                    }
                }
            }
        }
        else if (id.IsNone())
        {
            return 0;
        }

        return attribute_cache.GetAttribute(id);
    }

    uint8_t CustomAgentData::GetOrEstimateAttribute(AttributeOrTitle id) const
    {
        auto value = GetAttribute(id);
        return value ? value.value() : EstimateAttribute(agent_id);
    }

    uint8_t CustomAgentData::GetAttrLvlForSkill(const CustomSkillData &custom_sd) const
    {
        auto attr = custom_sd.attribute;
        if (custom_sd.tags.Spell && signet_of_illusions_charges > 0)
        {
            attr = GW::Constants::AttributeByte::IllusionMagic;
        }

        return GetOrEstimateAttribute(attr);
    }

    // bool TryCollectAttributes()
    // {
    //     auto w = GW::GetWorldContext();
    //     if (!w)
    //         return false;

    //     auto &attributes = w->attributes;
    //     if (!attributes.valid())
    //         return false;

    //     for (auto &agent_attributes : attributes)
    //     {
    //         auto agent_id = agent_attributes.agent_id;
    //         auto &custom_ad = GetCustomAgentData(agent_id);

    //         for (uint32_t i = 0; i < 52; i++)
    //         {
    //             auto &attribute = agent_attributes.attribute[i];
    //             if (attribute.level != 0)
    //             {
    //                 auto id = AttributeOrTitle(attribute.id);
    //                 custom_ad.SetAttributeOrTitle(id, attribute.level);
    //             }
    //         }
    //     }

    //     return true;
    // }

    namespace CustomAgentDataModule
    {
        GW::HookEntry hook_entry;
        void Initialize()
        {
            // TryCollectAttributes();
        }

        void Terminate()
        {
        }

        std::unordered_map<uint32_t, CustomAgentData> custom_agent_datas;

        CustomAgentData *TryGetCustomAgentData(uint32_t agent_id)
        {
            auto it = custom_agent_datas.find(agent_id);
            if (it == custom_agent_datas.end())
            {
                return nullptr;
            }
            auto *ret = &it->second;
            ret->agent_id = agent_id;
            return ret;
        }

        CustomAgentData &GetCustomAgentData(uint32_t agent_id)
        {
            auto &ret = custom_agent_datas[agent_id];
            ret.agent_id = agent_id;
            return ret;
        }
    }
}