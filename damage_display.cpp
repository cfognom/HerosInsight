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
#include <party_data.h>
#include <debug_display.h>
#include <packet_reader.h>
#include <update_manager.h>

namespace HerosInsight::DamageDisplay
{
    void Draw(IDirect3DDevice9 *device)
    {
        ImGui::SetNextWindowPos(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
        if (auto _ = WINDOW_ALPHA_SCOPE(); ImGui::Begin("Damage Display", &UpdateManager::open_damage, UpdateManager::GetWindowFlags()))
        {
            const int max_heroes = 9;
            const int bar_spacing = 10;
            const int half_spacing = bar_spacing / 2;
            const auto red_color = ImColor(0.85f, 0.15f, 0.15f, 1.0f);
            const auto blue_color = ImColor(0.0f, 0.75f, 1.0f, 1.0f);
            const auto grey_color = ImColor(0.5f, 0.5f, 0.5f, 1.0f);

            auto style = ImGui::GetStyle();
            auto window_padding = style.WindowPadding;

            auto heroes = PartyDataModule::party_members;

            // Find the greatest value of each type
            float max_outgoing = 0.0f;
            float max_incoming = 0.0f;
            for (const auto &hero : heroes)
            {
                max_outgoing = std::max(max_outgoing, hero.state.accum_outgoing_damage);
                max_outgoing = std::max(max_outgoing, hero.state.accum_outgoing_healing);
                max_incoming = std::max(max_incoming, hero.state.accum_incoming_damage);
                max_incoming = std::max(max_incoming, hero.state.accum_incoming_healing);
            }

            const auto outgoing_name = "Outgoing";
            const auto incoming_name = "Incoming";
            auto max_out_text = std::to_string((uint32_t)std::round(max_outgoing));
            auto max_in_text = std::to_string((uint32_t)std::round(max_incoming));
            auto out_text_size = ImGui::CalcTextSize(max_out_text.c_str());
            auto in_text_size = ImGui::CalcTextSize(max_in_text.c_str());
            auto out_name_text_size = ImGui::CalcTextSize(outgoing_name);
            auto in_name_text_size = ImGui::CalcTextSize(incoming_name);
            auto name_size_y = std::max(out_name_text_size.y, in_name_text_size.y);
            auto text_size_y = std::max(out_text_size.y, in_text_size.y);
            auto header_size_y = name_size_y + text_size_y;

            ImVec2 content_min_local = ImGui::GetWindowContentRegionMin();
            ImVec2 content_max_local = ImGui::GetWindowContentRegionMax();
            ImVec2 window_pos = ImGui::GetWindowPos();
            ImVec2 content_min = window_pos + content_min_local;
            ImVec2 content_max = window_pos + content_max_local;
            ImVec2 content_size = content_max - content_min;
            ImVec2 plot_min = content_min + ImVec2(0, header_size_y);
            ImVec2 plot_max = content_max;
            auto plot_size = plot_max - plot_min;

            ImGui::SetCursorScreenPos(content_min + ImVec2(0, 0));
            ImGui::TextUnformatted(outgoing_name);
            ImGui::SetCursorScreenPos(content_min + ImVec2(0, name_size_y));
            ImGui::TextUnformatted(max_out_text.c_str());
            ImGui::SetCursorScreenPos(content_min + ImVec2(plot_size.x - in_name_text_size.x, 0));
            ImGui::TextUnformatted(incoming_name);
            ImGui::SetCursorScreenPos(content_min + ImVec2(plot_size.x - in_text_size.x, name_size_y));
            ImGui::TextUnformatted(max_in_text.c_str());

            auto plot_width = plot_size.x;
            auto plot_height = plot_size.y;
            auto cell_height = plot_height / heroes.size();
            auto bar_height = (cell_height - bar_spacing) / 2;

            auto damage_span = max_outgoing + max_incoming;
            auto x_zero = plot_min.x + plot_size.x * (max_outgoing / damage_span);

            auto draw_list = ImGui::GetWindowDrawList();

            // Iterate over heroes
            for (size_t i = 0; i < heroes.size(); i++)
            {
                const auto &hero = heroes[i];

                ImVec2 top_left_out;
                ImVec2 top_left_in;
                ImVec2 bottom_right_out;
                ImVec2 bottom_right_in;
                float y = i * cell_height + plot_min.y;
                draw_list->AddLine(ImVec2(plot_min.x, y), ImVec2(plot_max.x, y), grey_color);

                y += half_spacing;

                float y_name = y;

                top_left_out = ImVec2(x_zero - plot_width * (hero.state.accum_outgoing_damage / damage_span), y);
                top_left_in = ImVec2(x_zero, y);
                y += bar_height;
                bottom_right_out = ImVec2(x_zero, y);
                bottom_right_in = ImVec2(x_zero + plot_width * (hero.state.accum_incoming_damage / damage_span), y);
                draw_list->AddRectFilled(top_left_out, bottom_right_out, red_color);
                draw_list->AddRectFilled(top_left_in, bottom_right_in, red_color);

                top_left_out = ImVec2(x_zero - plot_width * (hero.state.accum_outgoing_healing / damage_span), y);
                top_left_in = ImVec2(x_zero, y);
                y += bar_height;
                bottom_right_out = ImVec2(x_zero, y);
                bottom_right_in = ImVec2(x_zero + plot_width * (hero.state.accum_incoming_healing / damage_span), y);
                draw_list->AddRectFilled(top_left_out, bottom_right_out, blue_color);
                draw_list->AddRectFilled(top_left_in, bottom_right_in, blue_color);

                draw_list->AddText(ImVec2(plot_min.x, y_name), IM_COL32_WHITE, hero.decoded_name.c_str());
            }
            draw_list->AddLine(ImVec2(x_zero, plot_min.y), ImVec2(x_zero, plot_min.y + content_size.y), IM_COL32_WHITE);
        }
        ImGui::End();
    }
}