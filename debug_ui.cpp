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
    GW::HookEntry entry;

    void DebugFrameCallback(GW::HookStatus *status, const GW::UI::Frame *frame, GW::UI::UIMessage msg, void *p1, void *p2)
    {
        Utils::FormatToChat(L"Frame: {}, UIMessage: {}", frame->frame_id, static_cast<uint32_t>(msg));
    }

    void DebugCallback(GW::HookStatus *status, GW::UI::UIMessage msg, void *wparam, void *lparam)
    {
        switch (msg)
        {
            case GW::UI::UIMessage::kWriteToChatLog:
            case GW::UI::UIMessage::kWriteToChatLogWithSender:
            case (GW::UI::UIMessage)(0x30000000 | 24):
            case (GW::UI::UIMessage)(0x30000000 | 25):
            case (GW::UI::UIMessage)(0x30000000 | 29):
            case (GW::UI::UIMessage)(0x30000000 | 31):
                return; // Prevent recursive stack overflow
        }
        Utils::FormatToChat(L"UIMessage: {}, wparam: {}, lparam: {}", Utils::UIMessageToWString(msg), wparam, lparam);
    }

    void ForAllUIMessages(std::function<void(GW::UI::UIMessage)> action)
    {
        for (uint32_t i = 1; i < 1000; ++i)
        {
            action((GW::UI::UIMessage)(i));
            action((GW::UI::UIMessage)(0x10000000 | i));
            action((GW::UI::UIMessage)(0x30000000 | i)); // Client to server
        }
    }

    void EnableUIMessageLogging()
    {
        Utils::FormatToChat(L"Enabling UI message logging");
        // GW::UI::RegisterFrameUIMessageCallback(&entry, GW::UI::UIMessage::kMouseClick, DebugFrameCallback, 0x8000);
        ForAllUIMessages(
            [](GW::UI::UIMessage msg)
            {
                GW::UI::RegisterUIMessageCallback(&entry, msg, DebugCallback);
            });
    }

    void DisableUIMessageLogging()
    {
        Utils::FormatToChat(L"Disabling UI message logging");
        // GW::UI::RemoveFrameUIMessageCallback(&entry);
        ForAllUIMessages(
            [](GW::UI::UIMessage msg)
            {
                GW::UI::RemoveUIMessageCallback(&entry, msg);
            });
    }

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

            for (auto frame : *all_frames)
            {
                if (!Utils::IsFrameValid(frame))
                    continue;

                auto parent = GW::UI::GetParentFrame(frame);
                if (parent == hovered_frame)
                {
                    label.clear();
                    label.PushFormat("ID: %u, (child %u of hovered)", frame->frame_id, frame->child_offset_id);
                    auto magenta = ImColor(255, 0, 255);
                    Utils::DrawOutlineOnFrame(*frame, magenta, label, ImVec2(1.f, 1.f));
                }
            }
        }
    }
}

#endif
