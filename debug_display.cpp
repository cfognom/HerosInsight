#ifdef _DEBUG

#include <Windows.h>

#include <bitset>
#include <format>
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

#include <behaviours.h>
#include <damage_display.h>
#include <effect_tracking.h>
#include <energy_display.h>
#include <packet_reader.h>
#include <party_data.h>
#include <texture_module.h>
#include <update_manager.h>
#include <utils.h>

#include "debug_display.h"

namespace HerosInsight::DebugDisplay
{
    std::map<std::string, std::string> displayEntries;

    void ClearDisplay(std::string filter = "")
    {
        if (filter.empty())
        {
            displayEntries.clear();
        }
        else
        {
            for (auto it = displayEntries.begin(); it != displayEntries.end();)
            {
                if (it->first.find(filter) != std::string::npos) // || it->second.find(filter) != std::string::npos)
                {
                    it = displayEntries.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    void PushToDisplay(const std::string &key, const wchar_t *value)
    {
        displayEntries[key] = Utils::WStrToStr(value);
    }

    void PushToDisplay(const std::string &key, const uint32_t value)
    {
        displayEntries[key] = std::to_string(value);
    }

    void PushToDisplay(const std::string &key, const std::string &value)
    {
        displayEntries[key] = value;
    }

    void PushToDisplay(const std::string &key, const D3DXMATRIX &matrix)
    {
        std::string matrix_str = "\n";
        for (int i = 0; i < 4; i++)
        {
            matrix_str += std::format("\t{:f}, \t{:f}, \t{:f}, \t{:f}\n", matrix(i, 0), matrix(i, 1), matrix(i, 2), matrix(i, 3));
        }
        displayEntries[key] = matrix_str;
    }

    void PushToDisplay(const std::string &key, const D3DXVECTOR2 &vector)
    {
        displayEntries[key] = std::format("{:f}, {:f}", vector.x, vector.y);
    }
    void PushToDisplay(const std::string &key, const D3DXVECTOR3 &vector)
    {
        displayEntries[key] = std::format("{:f}, {:f}, {:f}", vector.x, vector.y, vector.z);
    }

    char filter[128] = ""; // Filter input
    void Draw(IDirect3DDevice9 *device)
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 600), ImGuiCond_FirstUseEver);
        if (auto _ = WINDOW_ALPHA_SCOPE(); ImGui::Begin("Debug Display", &UpdateManager::open_debug, UpdateManager::GetWindowFlags()))
        {
            ImGui::InputText("Filter", filter, IM_ARRAYSIZE(filter)); // Filter input

            std::vector<std::string> filterTokens;
            std::string filterString(filter);
            size_t pos = 0;
            while ((pos = filterString.find('|')) != std::string::npos)
            {
                std::string token = filterString.substr(0, pos);
                filterTokens.push_back(token);
                filterString.erase(0, pos + 1);
            }
            filterTokens.push_back(filterString);

            std::vector<std::vector<std::string>> orAndFiler;

            for (auto &filterToken : filterTokens)
            {
                std::vector<std::string> andTokens;
                while ((pos = filterToken.find('&')) != std::string::npos)
                {
                    std::string token = filterToken.substr(0, pos);
                    andTokens.push_back(token);
                    filterToken.erase(0, pos + 1);
                }
                andTokens.push_back(filterToken);
                orAndFiler.push_back(andTokens);
            }

            for (const auto &pair : displayEntries)
            {
                std::string key = pair.first;
                std::string value = pair.second;

                bool match = false;
                for (const auto &ors : orAndFiler)
                {
                    bool matched_all_ands = true;
                    for (const auto &token : ors)
                    {
                        if (strstr(key.c_str(), token.c_str()) == nullptr &&
                            strstr(value.c_str(), token.c_str()) == nullptr)
                        {
                            matched_all_ands = false;
                            break;
                        }
                    }

                    if (matched_all_ands)
                    {
                        match = true;
                        break;
                    }
                }

                if (match)
                {
                    ImGui::Text("%s: %s", key.c_str(), value.c_str());
                }
            }
        }
        ImGui::End();
    }
}

#endif