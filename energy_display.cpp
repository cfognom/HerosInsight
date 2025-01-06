#include <Windows.h>

#include <map>
#include <string>
#include <bitset>

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
#include <constants.h>
#include <party_data.h>
#include <debug_display.h>
#include <packet_reader.h>
#include <texture_module.h>

namespace HerosInsight::EnergyDisplay
{
    bool skillbar_pos_stale = true;
    GW::UI::FramePosition skillbar_skill_positions[8];
    ImVec2 skill_positions_calculated[8];
    // Overall settings
    enum class Layout
    {
        Row,
        Rows,
        Column,
        Columns
    };
    Layout layout = Layout::Row;
    float m_skill_width = 50.f;
    float m_skill_height = 50.f;

    GW::UI::Frame *skillbar_frame = nullptr;
    bool skillbar_position_dirty = true;
    GW::UI::UIInteractionCallback OnSkillbar_UICallback_Ret = nullptr;
    void __cdecl OnSkillbar_UICallback(GW::UI::InteractionMessage *message, void *wParam, void *lParam)
    {
        GW::Hook::EnterHook();
        OnSkillbar_UICallback_Ret(message, wParam, lParam);
        switch (static_cast<uint32_t>(message->message_id))
        {
        case 0xb:
            skillbar_frame = nullptr;
            skillbar_position_dirty = true;
            break;
        case 0x13:
        case 0x30:
        case 0x33:
            skillbar_position_dirty = true; // Forces a recalculation
            break;
        }
        GW::Hook::LeaveHook();
    }

    GW::UI::Frame *GetSkillbarFrame()
    {
        if (skillbar_frame)
            return skillbar_frame;
        skillbar_frame = GW::UI::GetFrameByLabel(L"Skillbar");
        if (skillbar_frame)
        {
            assert(skillbar_frame->frame_callbacks.size());
            if (skillbar_frame->frame_callbacks[0] != OnSkillbar_UICallback)
            {
                OnSkillbar_UICallback_Ret = skillbar_frame->frame_callbacks[0];
                skillbar_frame->frame_callbacks[0] = OnSkillbar_UICallback;
            }
        }
        return skillbar_frame;
    }

    bool UpdateSkillbarPos()
    {
        if (!skillbar_position_dirty)
            return true;
        const auto frame = GetSkillbarFrame();
        if (!(frame && frame->IsVisible() && frame->IsCreated()))
        {
            return false;
        }
        for (size_t i = 0; i < 8; i++)
        {
            const auto skillframe = GW::UI::GetChildFrame(frame, i);
            if (!skillframe)
                return false;
            skillbar_skill_positions[i] = skillframe->position;
            const auto tls = skillbar_skill_positions[i].GetTopLeftOnScreen();
            skill_positions_calculated[i] = ImVec2(tls.x, tls.y);
            if (i == 0)
            {
                m_skill_width = skillbar_skill_positions[0].GetSizeOnScreen().x;
                m_skill_height = skillbar_skill_positions[0].GetSizeOnScreen().y;
            }
        }

        skillbar_pos_stale = false;

        // Calculate columns/rows
        if (skillbar_skill_positions[0].screen_top == skillbar_skill_positions[7].screen_top)
        {
            layout = Layout::Row;
        }
        else if (skillbar_skill_positions[0].screen_left == skillbar_skill_positions[7].screen_left)
        {
            layout = Layout::Column;
        }
        else if (skillbar_skill_positions[0].screen_top == skillbar_skill_positions[3].screen_top)
        {
            layout = Layout::Rows;
        }
        else
        {
            layout = Layout::Columns;
        }
        skillbar_position_dirty = false;
        return true;
    }
    GW::HookEntry OnUIMessage_hook_entry;
    void OnUIMessage(GW::HookStatus *, GW::UI::UIMessage, void *, void *)
    {
        skillbar_pos_stale = true;
    }

    void Initialize()
    {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_hook_entry, GW::UI::UIMessage::kUIPositionChanged, OnUIMessage, 0x8000);
        // GW::UI::RegisterUIMessageCallback(&OnUIMessage_hook_entry, GW::UI::UIMessage::kPreferenceChanged, OnUIMessage, 0x8000);
    }

    void Terminate()
    {
        GW::UI::RemoveUIMessageCallback(&OnUIMessage_hook_entry);

        if (skillbar_frame && skillbar_frame->frame_callbacks[0] == OnSkillbar_UICallback)
        {
            skillbar_frame->frame_callbacks[0] = OnSkillbar_UICallback_Ret;
        }
    }

    uint8_t energy_costs[8] = {};
    int8_t energy_cost_deltas[8] = {};
    void Update()
    {
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading)
        {
            return;
        }

        const auto agent_id = GW::Agents::GetControlledCharacterId();
        const GW::Skillbar *skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (skillbar == nullptr)
        {
            return;
        }

        for (auto i = 0; i < 8; i++)
        {
            const auto skill = skillbar->skills[i];
            const auto skill_id = skill.skill_id;
            if ((uint32_t)skill_id == 0)
            {
                energy_costs[i] = 0;
                continue;
            }
            const auto skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
            if (skill_data == nullptr)
            {
                energy_costs[i] = 0;
                continue;
            }
            const auto energy_cost = skill_data->GetEnergyCost();
            const auto calc_energy_cost = Utils::CalculateEnergyCost(agent_id, skill_id);
            energy_costs[i] = calc_energy_cost;
            energy_cost_deltas[i] = (int8_t)calc_energy_cost - (int8_t)energy_cost;
        }
    }

    void Draw(IDirect3DDevice9 *device)
    {
        if (!UpdateSkillbarPos())
            return;

        const auto blue_color = Constants::GWColors::energy_blue;
        const auto black_color = IM_COL32_BLACK;
        const auto white_color = IM_COL32_WHITE;

        ImGui::PushFont(Constants::Fonts::skill_thick_font_15);
        for (size_t i = 0; i < 8; i++)
        {
            const auto energy_cost = energy_costs[i];
            if (energy_cost == 0)
                continue;

            const auto min = skill_positions_calculated[i];

            // const auto skill_size = skillbar_skill_positions[i].GetSizeOnScreen();

            const auto background_draw_list = ImGui::GetBackgroundDrawList();
            char text[8];
            auto len = snprintf(text, sizeof(text), "%d", energy_cost);
            assert(len >= 0 && len < sizeof(text));

            // ImVec2 text_size = ImGui::CalcTextSize(text.c_str());
            ImVec2 bb_min;
            ImVec2 bb_max;
            ImVec2 text_size = Utils::CalculateTextBoundingBox(Constants::Fonts::skill_thick_font_15, text, bb_min, bb_max);
            // ImVec2 text_size = ImGui::CalcTextSize(text);

            // auto image_size = ImVec2(16, 16);

            // auto center = min + image_size * 0.5f;
            auto center = min + ImVec2(9, 9);
            auto text_pos = center - text_size * 0.5f - ImVec2(bb_min.x, 0) * 0.5f;
            // auto text_pos = center - text_size * 0.5f;
            // text_pos.x = top_left.x + image_size.x - 2;
            // auto text_pos = min + ImVec2(1, 2);
            // auto image_pos = min + ImVec2(text_size.x, -1);
            // auto max = image_pos + image_size;

            // auto bg_col4 = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
            // bg_col4.w *= 0.8f;
            // const auto bg_col = ImGui::ColorConvertFloat4ToU32(bg_col4);

            // background_draw_list->AddRectFilled(min, max, bg_col, 3);

            // Render energy orb thingy
            background_draw_list->AddCircleFilled(center, 10, black_color, 16);
            background_draw_list->AddCircleFilled(center, 9, blue_color, 16);
            // TextureModule::GetPacket_ImageInAtlas(TextureModule::KnownFileIDs::UI_SkillStatsIcons, image_size, ImVec2(16, 16), 1)
            //     .AddToDrawList(background_draw_list, image_pos);

            // background_draw_list->AddRectFilled(text_pos, text_pos + text_size, black_color);
            // background_draw_list->AddRectFilled(text_pos + bb_min, text_pos + bb_max, ImColor(0.5f, 0.5f, 1.f, 1.f));
            // background_draw_list->AddLine(text_pos, text_pos + bb_min, ImColor(1.f, 0.f, 0.f, 1.f));
            // background_draw_list->AddLine(text_pos + bb_min, text_pos + bb_max, ImColor(0.f, 0.f, 1.f, 1.f));

            // Render the shadow
            background_draw_list->AddText(text_pos + ImVec2(1, 1), black_color, text);

            // Render the actual text
            const auto delta = energy_cost_deltas[i];
            ImColor color;
            if (delta > 0)
                color = IM_COL32(255, 100, 100, 255); // Red
            else if (delta < 0)
                color = IM_COL32(128, 255, 128, 255); // Green
            else
                color = IM_COL32_WHITE;
            background_draw_list->AddText(text_pos, color, text);
        }
        ImGui::PopFont();
    }
}
