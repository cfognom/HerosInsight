#include <Windows.h>

#include <map>
#include <string>
#include <bitset>
#include <format>

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

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_internal.h>

#include <utils.h>
#include <behaviours.h>
#include <party_data.h>
#include <debug_display.h>
#include <energy_display.h>
#include <packet_reader.h>
#include <damage_display.h>
#include <effect_tracking.h>

namespace HerosInsight
{
    static bool should_update = false;

    void ApplyBehaviours()
    {
        for (auto &party_member : PartyDataModule::party_members)
        {
            if (!party_member.is_controllable)
            {
                continue;
            }

            for (const auto &behaviour : HerosInsight::Behaviours::behaviours)
            {
                if (behaviour.prereq(&party_member))
                {
                    behaviour.exec(&party_member);
                }
            }
        }
    }

    void Update()
    {
        ApplyBehaviours();
    }
}