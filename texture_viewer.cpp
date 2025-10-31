#ifdef _DEBUG

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

#include <debug_display.h>
#include <texture_module.h>
#include <update_manager.h>
#include <variable_size_clipper.h>

#include "utils.h"

namespace HerosInsight::TextureViewer
{
    struct LoadedTex
    {
        uint32_t file_id;
        IDirect3DTexture9 *texture;
    };

    uint32_t search_start = 0;
    uint32_t search_end = 0;
    std::vector<LoadedTex> loaded_textures;
    int start_offset = 0;
    void Draw()
    {
        ImGui::SetNextWindowPos(ImVec2(600, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

        static VariableSizeClipper clipper{};
        if (ImGui::Begin("Texture Viewer", &UpdateManager::open_texture_viewer, UpdateManager::GetWindowFlags()))
        {
            if (ImGui::InputInt("Start Offset", &start_offset))
            {
                clipper.Reset();
                loaded_textures.clear();
                search_start = start_offset;
                search_end = start_offset;
            }

            ImGui::BeginChild("Texture List");

            for (uint32_t i = search_start; i < search_end; i++)
            {
                auto entry = TextureModule::LoadTextureFromFileId(i); // Get the textures we loaded last frame
                if (*entry != nullptr)
                    loaded_textures.push_back({i, *entry});
            }
            search_start = search_end;
            if (loaded_textures.size() - clipper.GetCurrentScroll().item_index < 256)
            {
                search_end = std::min(search_end + 256, TextureModule::KnownFileIDs::MAX);
                for (uint32_t i = search_start; i < search_end; i++)
                    TextureModule::LoadTextureFromFileId(i); // Load these for next frame
            }

            const auto avg_height = 100.f;

            auto DrawItem = [&](uint32_t i)
            {
                auto &entry = loaded_textures[i];
                auto file_id = entry.file_id;
                auto tex = entry.texture;
                auto desc = TextureModule::GetTextureDesc(tex);

                ImGui::Text("file_id: %d", file_id);
                ImGui::Text("Width: %d, Height: %d", desc.Width, desc.Height);
                ImGui::Image(tex, ImVec2(desc.Width, desc.Height) + ImVec2(2, 2), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1));
            };

            clipper.Draw(loaded_textures.size(), avg_height, true, DrawItem);

            ImGui::EndChild();
        }

        ImGui::End();
    }
}

#endif