#ifdef _DEBUG

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Hero.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GadgetContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/GameplayContext.h>
#include <GWCA/Context/ItemContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/List.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
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

#include "debug_ui.h"
#include <update_manager.h>

namespace HerosInsight::DebugUI
{
    void Draw(IDirect3DDevice9 *device)
    {
        auto all_frames = UpdateManager::s_FrameArray;

        float hovered_area = std::numeric_limits<float>::max();
        GW::UI::Frame *hovered_frame = nullptr;
        for (auto frame : *all_frames)
        {
            if (!Utils::IsFrameValid(frame))
                continue;

            if (Utils::IsHoveringFrame(*frame))
            {
                auto size = Utils::GetFrameRect(*frame).GetSize();
                auto area = size.x * size.y;
                if (area < hovered_area)
                {
                    hovered_area = area;
                    hovered_frame = frame;
                }
                else if (area == hovered_area)
                {
                    assert(hovered_frame);
                    GW::UI::Frame *parent = frame;
                    while (parent != hovered_frame)
                    {
                        parent = GW::UI::GetParentFrame(parent);
                        if (!parent)
                        {
                            hovered_frame = frame;
                            break;
                        }
                    }
                }
            }
        }

        if (hovered_frame)
        {
            FixedArray<char, 64> label_salloc;
            auto label = label_salloc.ref();

            auto parent = GW::UI::GetParentFrame(hovered_frame);
            if (parent)
            {
                label.PushFormat("ID: %u (parent)", parent->frame_id);
                auto yellow = ImColor(255, 255, 0);
                Utils::DrawOutlineOnFrame(*parent, yellow, label);
            }

            label.clear();
            label.PushFormat("ID: %u, (child %u of parent)", hovered_frame->frame_id, hovered_frame->child_offset_id);
            auto cyan = ImColor(0, 255, 255);
            Utils::DrawOutlineOnFrame(*hovered_frame, cyan, label, ImVec2(0.5f, 0.5f));

            size_t child_index = 0;
            while (true)
            {
                auto child_ptr = GW::UI::GetChildFrame(hovered_frame, child_index);
                if (!child_ptr)
                    break;

                auto &child_frame = *child_ptr;

                assert(child_frame.child_offset_id == child_index);
                label.clear();
                label.PushFormat("ID: %u, (child %u of hovered)", child_frame.frame_id, child_index);
                auto magenta = ImColor(255, 0, 255);
                Utils::DrawOutlineOnFrame(child_frame, magenta, label, ImVec2(1.f, 1.f));

                ++child_index;
            }
        }
    }
}

#endif
