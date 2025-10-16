#include <Windows.h>
#include <bitset>
#include <codecvt>
#include <d3d9.h>
#include <d3dx9math.h>
#include <filesystem>
#include <future>
#include <iostream>
#include <regex>
#include <span>
#include <string>

#include <GWCA/GWCA.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
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

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
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

#include <d3d9.h>
#include <imgui.h>

#include "worldspaceUI.h"
#include <constants.h>
#include <debug_display.h>
#include <party_data.h>
#include <texture_module.h>
#include <update_manager.h>
#include <utils.h>

namespace HerosInsight::WorldSpaceUI
{
    enum struct PositionType : uint8_t
    {
        None,
        AgentTopLeft,
        AgentTopRight,
        AgentCenter,
    };

    struct EffectDrawData
    {
        uint32_t agent_id;
        GW::Constants::SkillID skill_id;
        float accum_healing;
        uint32_t frame_id_applied;
        uint32_t frame_id_changed;
        uint32_t last_render_pos;
        DWORD timestamp_finished;
        DWORD timestamp_first_update;
        DWORD timestamp_first_number;

        DWORD timestamp_begin;
        uint32_t duration_sec;

        PositionType pos_type;

        void SetFinished(DWORD timestamp)
        {
            timestamp_finished = timestamp;
        }

        bool IsFinished() const
        {
            return timestamp_finished != 0;
        }

        DWORD GetEndTimestamp() const
        {
            return timestamp_begin + duration_sec * 1000;
        }
    };

    std::vector<EffectDrawData> effect_draw_list; // Sorted by distance to camera

    // The API is simple: Call this function each frame for each skill that should be drawn
    void PushSkillForDraw(uint32_t agent_id, GW::Constants::SkillID skill_id, float healing, DWORD timestamp_begin, uint32_t duration_sec)
    {
        auto Finder = [&](const EffectDrawData &entry)
        {
            return entry.agent_id == agent_id &&
                   entry.skill_id == skill_id &&
                   !entry.IsFinished();
        };

        auto it = std::find_if(effect_draw_list.begin(), effect_draw_list.end(), Finder);

        if (it != effect_draw_list.end())
        {
            it->accum_healing += healing;
            it->frame_id_changed = UpdateManager::frame_id;
            it->timestamp_begin = timestamp_begin;
            it->duration_sec = duration_sec;
            return;
        }

        EffectDrawData entry = {};
        entry.agent_id = agent_id;
        entry.skill_id = skill_id;
        entry.accum_healing = healing;
        entry.frame_id_applied = UpdateManager::frame_id;
        entry.frame_id_changed = UpdateManager::frame_id;
        entry.timestamp_begin = timestamp_begin;
        entry.duration_sec = duration_sec;
        const auto &skill = *GW::SkillbarMgr::GetSkillConstantData(skill_id);

        if (skill.type == GW::Constants::SkillType::Stance)
        {
            entry.pos_type = PositionType::AgentCenter;
        }
        else if (healing <= 0 &&
                 (skill.type == GW::Constants::SkillType::Hex ||
                  skill.type == GW::Constants::SkillType::Condition))
        {
            entry.pos_type = PositionType::AgentTopLeft;
        }
        else
        {
            entry.pos_type = PositionType::AgentTopRight;
        }

        effect_draw_list.push_back(entry);
    }

#ifdef _DEBUG
    void DebugDrawList()
    {
        DebugDisplay::ClearDisplay("Worldspace draw list");
        uint32_t i = 0;
        for (const auto &entry : effect_draw_list)
        {
            DebugDisplay::PushToDisplay(L"Worldspace draw list [{}] = {}", i++, Utils::GetSkillName(entry.skill_id));
        }
    }
#endif

    void Update()
    {
#ifdef _DEBUG
        DebugDrawList();
#endif

        if (effect_draw_list.size() <= 1)
            return;

        const auto camera = GW::CameraMgr::GetCamera();
        if (!camera)
            return;

        auto camera_pos = camera->position;

        auto Sorter = [camera_pos](const EffectDrawData &a, const EffectDrawData &b)
        {
            if (a.agent_id == b.agent_id)
            {
                if (a.IsFinished() != b.IsFinished())
                    return a.IsFinished() > b.IsFinished();

                return a.pos_type > b.pos_type;
            }

            const auto agent_a = GW::Agents::GetAgentByID(a.agent_id);
            const auto agent_b = GW::Agents::GetAgentByID(b.agent_id);
            if (!agent_a || !agent_b)
                return false;

            const auto dist_a = Utils::DistanceSqrd(GW::Vec3f(agent_a->x, agent_a->y, agent_a->z), camera_pos);
            const auto dist_b = Utils::DistanceSqrd(GW::Vec3f(agent_b->x, agent_b->y, agent_b->z), camera_pos);

            return dist_a > dist_b;
        };

        std::sort(effect_draw_list.begin(), effect_draw_list.end(), Sorter);
    }

    struct AgentBB
    {
        AgentBB(GW::Agent &agent, GW::Camera &camera)
        {
            const auto agent_feet = D3DXVECTOR3(agent.x, agent.y, -agent.z);
            const auto agent_head = agent_feet + D3DXVECTOR3(0, 0, agent.height1);
            const auto cam_to_feet = agent_feet - D3DXVECTOR3(camera.position.x, camera.position.y, -camera.position.z);
            D3DXVECTOR3 side_normal;
            const D3DXVECTOR3 up(0, 0, 1);
            D3DXVec3Cross(&side_normal, &up, &cam_to_feet);
            D3DXVec3Normalize(&side_normal, &side_normal);
            D3DXVECTOR3 side = side_normal * agent.width1;

            ul = agent_head - side;
            ur = agent_head + side;
            ll = agent_feet - side;
            lr = agent_feet + side;
        }
        D3DXVECTOR3 ul;
        D3DXVECTOR3 ur;
        D3DXVECTOR3 ll;
        D3DXVECTOR3 lr;
    };

    std::vector<bool> is_occluded;
    std::vector<AgentBB> agent_bounds;
    std::vector<IDirect3DQuery9 *> occlusion_queries;
    void OcclusionCheck(IDirect3DDevice9 *device)
    {
        auto cam = GW::CameraMgr::GetCamera();
        if (!cam)
            return;

        IDirect3DStateBlock9 *saved_state;
        if (device->CreateStateBlock(D3DSBT_ALL, &saved_state) < 0)
            return;

        // Disable lighting (so it doesn't interfere with color)
        device->SetRenderState(D3DRS_LIGHTING, FALSE);

        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        // Disable color writes
#ifdef _DEBUG
        device->SetRenderState(D3DRS_COLORWRITEENABLE, TRUE); // During debug we show the point
#else
        device->SetRenderState(D3DRS_COLORWRITEENABLE, FALSE);
#endif

        const D3DXMATRIX identity = D3DXMATRIX(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);

        D3DXMATRIX viewMatrix = Utils::GetViewMatrix();
        D3DXMATRIX projMatrix = Utils::GetProjectionMatrix();

        device->SetTransform(D3DTS_PROJECTION, &projMatrix);
        device->SetTransform(D3DTS_VIEW, &viewMatrix);
        device->SetTransform(D3DTS_WORLD, &identity);

        agent_bounds.clear();
        uint32_t prev_agent_id = 0;
        for (auto &draw_data : effect_draw_list)
        {
            auto agent = GW::Agents::GetAgentByID(draw_data.agent_id);
            if (!agent)
                continue;

            if (draw_data.agent_id != prev_agent_id)
            {
                agent_bounds.push_back(AgentBB(*agent, *cam));
                prev_agent_id = draw_data.agent_id;
            }
        }

        // Use an occlusion query to check if the point is not occluded by other geometry
        occlusion_queries.clear();
        for (auto &quad : agent_bounds)
        {
            IDirect3DQuery9 *pOcclusionQuery = nullptr;
            device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &pOcclusionQuery);
            pOcclusionQuery->Issue(D3DISSUE_BEGIN);
            device->SetFVF(D3DFVF_XYZ); // Using XYZ position and color
            device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, &quad, sizeof(D3DXVECTOR3));
            pOcclusionQuery->Issue(D3DISSUE_END);

            occlusion_queries.push_back(pOcclusionQuery);
        }

        is_occluded.clear();
        for (auto &query : occlusion_queries)
        {
            DWORD numPixelsDrawn;
            while (query->GetData(NULL, 0, D3DGETDATA_FLUSH) == S_FALSE)
                ;

            query->GetData(&numPixelsDrawn, sizeof(DWORD), D3DGETDATA_FLUSH);
            is_occluded.push_back(numPixelsDrawn == 0);
            query->Release();
        }

        // Restore state
        saved_state->Apply();
        saved_state->Release();
    }

    void Draw(IDirect3DDevice9 *device)
    {
        // WARNING: This thing is a mess, but it works (and/or worked)

        OcclusionCheck(device);

        auto player_agent_id = GW::Agents::GetControlledCharacterId();

        DWORD timestamp_now = GW::MemoryMgr::GetSkillTimer();

        const auto camera = GW::CameraMgr::GetCamera();
        if (!camera)
            return;

        auto bg_draw_list = ImGui::GetBackgroundDrawList();

        GW::Vec3f icon_world_pos = GW::Vec3f(0, 0, 0);
        ImVec2 screen_base_pos = ImVec2(0, 0);
        ImVec2 screen_body_pos = ImVec2(0, 0);
        uint32_t prev_agent_id = 0;
        int32_t agent_index = -1;
        for (auto it = effect_draw_list.begin(); it != effect_draw_list.end();)
        {
            const auto skill_id = it->skill_id;
            const auto agent_id = it->agent_id;

            if (prev_agent_id != agent_id)
            {
                agent_index++;
                prev_agent_id = agent_id;
            }

            const auto agent = GW::Agents::GetAgentByID(agent_id);

            if (!agent)
            {
                it++;
                continue;
            }

            const auto animation_ms = 1000; // The total time of the "numbers move up" animation
            const auto fadein_ms = 200;     // The time it takes for the numbers to fade in at the beginning of the animation
            const auto fadeout_ms = 300;    // The time it takes for the numbers to fade out at the end of the animation
            const auto animation_min_scale = 1.f;
            const auto animation_max_scale = 1.4f;
            const auto animation_screen_distance = 65.f;
            const auto distance_max = 5000.f;
            float icon_size = 28.f;
            float alpha = 1.f;

            // In GW, up is down
            const auto agent_pos_feet = GW::Vec3f(agent->x, agent->y, -agent->z);
            const auto agent_pos_head = GW::Vec3f(agent->x, agent->y, -agent->z + agent->height1);
            const auto cam_pos = GW::Vec3f(camera->position.x, camera->position.y, -camera->position.z);
            const auto cam_to_target = camera->look_at_target - cam_pos;
            const auto cam_to_agent_feet = agent_pos_feet - cam_pos;
            const auto target_is_infront = Utils::Dot(cam_to_target, cam_to_agent_feet) > 0;
            const auto distance_to_agent_feet = std::sqrt(Utils::Dot(cam_to_agent_feet, cam_to_agent_feet));

            auto ms_since_finished = 0;
            if (it->timestamp_finished)
            {
                ms_since_finished = timestamp_now - it->timestamp_finished;
                const auto fadeout_begins = animation_ms - fadeout_ms;

                if (ms_since_finished >= fadeout_begins)
                {
                    const auto ms_since_fadeout = ms_since_finished - fadeout_begins;
                    alpha = 1.0f - ((float)ms_since_fadeout / (float)fadeout_ms);
                }

                if (ms_since_finished > animation_ms)
                {
                    it = effect_draw_list.erase(it);
                    continue;
                }
            }
            else if (it->frame_id_changed != UpdateManager::frame_id)
            {
                it->SetFinished(timestamp_now);
            }

            if (!target_is_infront ||
                distance_to_agent_feet > distance_max ||
                ((agent_index >= is_occluded.size() || is_occluded[agent_index]) && !it->timestamp_finished))
            {
                it++;
                continue;
            }

            uint32_t n_same_pos = 0;
            for (auto it2 = it; it2 != effect_draw_list.begin();) // Count how many other non finished effects are on the same agent
            {
                it2--;

                if (it2->agent_id != agent_id)
                    break;

                if (it2->pos_type != it->pos_type)
                    continue;

                if (!it2->IsFinished())
                    n_same_pos++;
            }

            if (!it->timestamp_first_update)
            {
                it->timestamp_first_update = timestamp_now;
            }

            const auto ms_since_start = timestamp_now - it->timestamp_first_update;
            if (ms_since_start < fadein_ms)
            {
                alpha = (float)ms_since_start / (float)fadein_ms;
            }

            const auto animation_progress = (float)ms_since_finished / (float)animation_ms;

            if (n_same_pos == 0)
            {
                // We only have to calculate this once per agent
                icon_world_pos = agent_pos_head;

                if (it->pos_type == PositionType::AgentCenter)
                {
                    icon_world_pos.z = agent_pos_feet.z + agent->height1 * 0.5f;
                }
                else
                {
                    icon_world_pos.z += 20.f;
                }

                screen_base_pos = Utils::WorldSpaceToScreenSpace(icon_world_pos);
            }

            const auto cam_to_icon = icon_world_pos - cam_pos;
            const auto distance_to_icon = std::sqrt(Utils::Dot(cam_to_icon, cam_to_icon)) / 800.f;

            auto scale = 1.0f;

            scale *= std::sqrt(distance_to_icon) / distance_to_icon; // The sqrt factor is not realistic but it makes closer numbers smaller and further ones bigger which improves readability

            scale *= std::clamp(
                Utils::Remap(0.f, 1.f, animation_min_scale, animation_max_scale, animation_progress),
                animation_min_scale,
                animation_max_scale
            );

            auto screen_pos = screen_base_pos;
            if (it->pos_type == PositionType::AgentTopLeft)
                screen_pos.x -= 20.f * scale;
            else if (it->pos_type == PositionType::AgentTopRight)
                screen_pos.x += 20.f * scale;

            if (!it->IsFinished())
            {
                it->last_render_pos = n_same_pos;
            }
            screen_pos.y -= (animation_screen_distance * scale) * animation_progress + it->last_render_pos * 30.f * scale;

            icon_size *= scale;
            const auto icon_half_size = icon_size / 2;
            auto icon_screen_pos = screen_pos;
            icon_screen_pos.y -= icon_half_size;

            const auto icon_min = icon_screen_pos - ImVec2(icon_half_size, icon_half_size);
            const auto icon_max = icon_min + ImVec2(icon_size, icon_size);

            const auto now_or_ended_timestamp = it->timestamp_finished ? it->timestamp_finished : timestamp_now;
            const auto effect_remaining_progress =
                std::clamp(1.f - (float)(now_or_ended_timestamp - it->timestamp_begin) / (float)(it->duration_sec * 1000), 0.f, 1.f);

            // Draw skill image
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            const auto &skill = *GW::SkillbarMgr::GetSkillConstantData(skill_id);
            const auto effect_border_index = Utils::GetSkillEffectBorderIndex(skill);
            TextureModule::DrawSkill(skill, icon_min, icon_size, true, false, bg_draw_list);
            // Draw remaining seconds
            if (now_or_ended_timestamp < it->GetEndTimestamp())
            {
                const auto rem_sec_ceil = (it->GetEndTimestamp() - now_or_ended_timestamp + 999) / 1000;
                bool skip = it->IsFinished() && now_or_ended_timestamp > it->GetEndTimestamp() - 300;
                if (!skip)
                {
                    FixedVector<char, 16> time_str;
                    if (rem_sec_ceil > 999)
                        time_str.PushFormat("999+");
                    else
                        time_str.PushFormat("%d", rem_sec_ceil);
                    const auto rem_sec_pos = icon_min + ImVec2(2, 2);
                    const auto font_size = scale * Constants::Fonts::skill_thick_font_12->FontSize;

                    bg_draw_list->AddText(
                        Constants::Fonts::skill_thick_font_12,
                        font_size,
                        rem_sec_pos + ImVec2(1, 1),
                        ImGui::GetColorU32(IM_COL32_BLACK),
                        time_str.data(),
                        time_str.data() + time_str.size()
                    );

                    bg_draw_list->AddText(
                        Constants::Fonts::skill_thick_font_12,
                        font_size,
                        rem_sec_pos,
                        ImGui::GetColorU32(IM_COL32_WHITE),
                        time_str.data(),
                        time_str.data() + time_str.size()
                    );
                }
            }
            // Draw progressbar
            const auto progress_bar_color = Constants::GWColors::effect_border_colors[effect_border_index];
            const auto progress_bar_outline_min = ImVec2(icon_min.x + 2 * scale, icon_max.y - 1 * scale - 3);
            const auto progress_bar_outline_max = ImVec2(icon_max.x - 2 * scale, icon_max.y - 1 * scale);
            const auto progress_bar_min = ImVec2(progress_bar_outline_min.x + 1, progress_bar_outline_min.y + 1);
            const auto progress_bar_max = ImVec2(progress_bar_outline_max.x - 1, progress_bar_outline_max.y - 1);
            const auto progress_bar_max_width = progress_bar_max.x - progress_bar_min.x;
            const auto progress_bar_width = progress_bar_max_width * effect_remaining_progress;
            const auto progress_bar_max_current = ImVec2(progress_bar_min.x + progress_bar_width, progress_bar_max.y);
            bg_draw_list->AddRectFilled(progress_bar_outline_min, progress_bar_outline_max, ImGui::GetColorU32(IM_COL32_BLACK));
            bg_draw_list->AddRectFilled(progress_bar_min, progress_bar_max_current, ImGui::GetColorU32(progress_bar_color));

            const auto healing = (int32_t)(it->IsFinished() ? std::round(it->accum_healing) : it->accum_healing);

            if (healing != 0)
            {
                // Draw healing/damage text
                if (!it->timestamp_first_number)
                {
                    it->timestamp_first_number = timestamp_now;
                }

                const auto ms_since_number_start = timestamp_now - it->timestamp_first_number;
                if (ms_since_number_start < fadein_ms)
                {
                    alpha = (float)ms_since_number_start / (float)fadein_ms;
                }

                auto number_scale = scale * 0.75f;
                auto number_size = TextureModule::CalculateDamageNumberSize(healing, number_scale);
                auto number_half_size = number_size * 0.5f;

                auto number_screen_pos = icon_screen_pos;
                number_screen_pos.x += (it->pos_type == PositionType::AgentTopLeft ? -1 : 1) * (icon_half_size + 5 * scale + number_half_size.x);
                number_screen_pos -= number_half_size;

                auto number_color = healing < 0
                                        ? player_agent_id == agent_id
                                              ? TextureModule::DamageNumberColor::Red
                                              : TextureModule::DamageNumberColor::Yellow
                                        : TextureModule::DamageNumberColor::Blue;

                TextureModule::DrawDamageNumber(healing, number_screen_pos, number_scale, number_color, bg_draw_list);
            }
            ImGui::PopStyleVar(); // Alpha

            ++it;
        }
    }

    void Reset()
    {
        effect_draw_list.clear();
    }
}