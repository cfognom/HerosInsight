// Source https://github.com/gwdevhub/GWToolboxpp/blob/6a0415b160fc966c19e5b2e47556998f322bca28/GWToolboxdll/Modules/GwDatTextureModule.cpp

#include <Windows.h>
#include <bitset>
#include <codecvt>
#include <d3d9.h>
#include <filesystem>
#include <future>
#include <iostream>
#include <queue>
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
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

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
#include <GWCA/Managers/AssetMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/EventMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <constants.h>
#include <debug_display.h>
#include <utils.h>

#include "texture_module.h"

namespace TextureModule
{
    IDirect3DTexture9 *CreateTexture(IDirect3DDevice9 *device, uint32_t file_id, GW::Dims &dims)
    {
        if (!device || !file_id)
        {
            return nullptr;
        }

        GW::AssetMgr::DecodedImage decoded(file_id);
        auto image = decoded.image;
        dims = decoded.dims;
        if (!image)
            return nullptr;

        // Create a texture: http://msdn.microsoft.com/en-us/library/windows/desktop/bb174363(v=vs.85).aspx
        IDirect3DTexture9 *tex = nullptr; // The return value
        int levels = 1;
        if (device->CreateTexture(dims.width, dims.height, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0) != D3D_OK)
            return nullptr;

        // Lock the texture for writing: http://msdn.microsoft.com/en-us/library/windows/desktop/bb205913(v=vs.85).aspx
        D3DLOCKED_RECT rect;
        if (tex->LockRect(0, &rect, 0, D3DLOCK_DISCARD) != D3D_OK)
        {
            tex->Release();
            return nullptr;
        }

        for (int y = 0; y < dims.height; y++)
        {
            auto dst = (uint8_t *)rect.pBits + y * rect.Pitch;
            auto src = (uint32_t *)image + y * dims.width;
            memcpy(dst, src, dims.width * 4);
            /* for (int x = 0; x < dims.x; ++x) {
                uint8_t* destAddr = ((uint8_t*)rect.pBits + y * rect.Pitch + 4 * x);

                // unsigned int data = 0xFF000000 | (*srcdata >> 24 & 0xFF) | (*srcdata >> 16 & 0xFF00) | (*srcdata >> 8 & 0xFF0000);
                // memcpy(destAddr, &data, 4);
                memcpy(destAddr, srcdata, 4);
                srcdata++;
            }*/
        }
        // Unlock the texture so it can be used.
        tex->UnlockRect(0);
        return tex;
    }

    struct GwImg
    {
        uint32_t m_file_id = 0;
        GW::Dims m_dims;
        IDirect3DTexture9 *m_tex = nullptr;
    };

    std::map<uint32_t, GwImg *> textures_by_file_id;

    // tasks to be done in the render thread
    std::queue<std::function<void(IDirect3DDevice9 *)>> dx_jobs;
    std::recursive_mutex dx_mutex;
    void EnqueueDxTask(const std::function<void(IDirect3DDevice9 *)> &f)
    {
        dx_mutex.lock();
        dx_jobs.push(f);
        dx_mutex.unlock();
    }

    void DxUpdate(IDirect3DDevice9 *device)
    {
        while (true)
        {
            dx_mutex.lock();
            if (dx_jobs.empty())
            {
                dx_mutex.unlock();
                return;
            }
            const std::function<void(IDirect3DDevice9 *)> func = std::move(dx_jobs.front());
            dx_jobs.pop();
            dx_mutex.unlock();
            func(device);
        }
    }

    IDirect3DTexture9 **LoadTextureFromFileId(uint32_t file_id)
    {
        auto found = textures_by_file_id.find(file_id);
        if (found != textures_by_file_id.end())
            return &found->second->m_tex;

        auto gwimg_ptr = new GwImg{file_id};
        textures_by_file_id[file_id] = gwimg_ptr;
        auto device = GW::Render::GetDevice();
        if (GW::Render::GetIsInRenderLoop() && device)
        {
            auto tex = CreateTexture(device, gwimg_ptr->m_file_id, gwimg_ptr->m_dims);
            if (tex)
            {
                gwimg_ptr->m_tex = tex;
                return &gwimg_ptr->m_tex;
            }
        }

        EnqueueDxTask(
            [gwimg_ptr](IDirect3DDevice9 *device)
            {
                gwimg_ptr->m_tex = CreateTexture(device, gwimg_ptr->m_file_id, gwimg_ptr->m_dims);
                SOFT_ASSERT(gwimg_ptr->m_tex, L"Failed to create texture for file id {}", gwimg_ptr->m_file_id);
            }
        );

        return &gwimg_ptr->m_tex;
    }
    void Terminate()
    {
        for (auto gwimg_ptr : textures_by_file_id)
        {
            delete gwimg_ptr.second;
        }
        textures_by_file_id.clear();
    }

    IDirect3DTexture9 **GetSkillImage(GW::Constants::SkillID skill_id)
    {
        const auto skill = GW::SkillbarMgr::GetSkillConstantData(skill_id);

        if (!skill)
            return nullptr;

        bool use_hd = GW::UI::GetPreference(GW::UI::FlagPreference::EnableHDSkillIcons);
        auto file_id = use_hd ? skill->icon_file_id_reforged : skill->icon_file_id;
        if (!file_id)
            file_id = GW::SkillbarMgr::GetSkillConstantData(GW::Constants::SkillID::No_Skill)->icon_file_id;

        return LoadTextureFromFileId(file_id);
    }

    struct ResourcesImg
    {
        std::string m_filename;
        GW::Dims m_dims;
        IDirect3DTexture9 *m_tex = nullptr;
    };

    D3DSURFACE_DESC GetTextureDesc(IDirect3DTexture9 *texture)
    {
        D3DSURFACE_DESC desc;
        if (texture)
        {
            texture->GetLevelDesc(0, &desc);

            return desc;
        }

        return {};
    }

    std::unordered_map<std::string, ResourcesImg *> textures_by_filename;
    // Load texture from resources folder
    IDirect3DTexture9 **GetResourceTexture(const char *filename)
    {
        auto found = textures_by_filename.find(filename);
        if (found != textures_by_filename.end())
            return &found->second->m_tex;

        auto img_ptr = new ResourcesImg{filename};
        textures_by_filename[filename] = img_ptr;
        auto Loadtask = [img_ptr](IDirect3DDevice9 *device)
        {
            auto full_path_str = (Constants::paths.resources() / img_ptr->m_filename).string();

            D3DXIMAGE_INFO image_info;
            auto result = D3DXGetImageInfoFromFileA(full_path_str.c_str(), &image_info);
            assert(result == D3D_OK);
            img_ptr->m_dims.width = static_cast<float>(image_info.Width);
            img_ptr->m_dims.height = static_cast<float>(image_info.Height);

            result = D3DXCreateTextureFromFileExA(
                device,
                full_path_str.c_str(),
                image_info.Width,  // Width
                image_info.Height, // Height
                1,                 // MipLevels
                0,                 // Usage
                D3DFMT_A8R8G8B8,   // Format
                D3DPOOL_MANAGED,   // Pool
                D3DX_FILTER_POINT, // Filter
                D3DX_DEFAULT,      // MipFilter
                0,                 // ColorKey
                nullptr,           // pSrcInfo
                nullptr,           // pPalette
                &img_ptr->m_tex
            );
            assert(result == D3D_OK);
        };

        EnqueueDxTask(Loadtask);

        return &img_ptr->m_tex;
    }

    // Define a global or static variable to store the start time
    static auto start_time = std::chrono::steady_clock::now();

    // Function to get the elapsed time in seconds
    float GetElapsedTime()
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsed = now - start_time;
        return elapsed.count();
    }

    bool DrawSkill(const GW::Skill &skill, ImVec2 pos, float icon_size, bool as_effect, bool as_hovered, ImDrawList *draw_list)
    {
        // icon_size = 100;
        auto skill_icon = TextureModule::GetSkillImage(skill.skill_id);
        auto skill_overlays = TextureModule::LoadTextureFromFileId(KnownFileIDs::UI_SkillEffectBorders1);
        auto skill_type_icons = TextureModule::LoadTextureFromFileId(KnownFileIDs::UI_SkillLeadOffhandEncDualHexWepSpIcons);

        if (!(skill_icon && *skill_icon &&
              skill_overlays && *skill_overlays))
            return false;

        uint32_t overlay_index = 0;
        if (as_effect)
        {
            overlay_index = 2 * HerosInsight::Utils::GetSkillEffectBorderIndex(skill);
        }

        if (skill.IsElite())
            overlay_index += 1;

        if (!draw_list)
            draw_list = ImGui::GetWindowDrawList();

        const auto icon_size_half = icon_size * 0.5f;
        const auto size = ImVec2(icon_size, icon_size);
        const auto size_half = ImVec2(icon_size_half, icon_size_half);
        auto min = pos;
        auto max = min + size;
        auto uv0 = ImVec2(0, 0);
        auto uv1 = ImVec2(1, 1);
        const auto tint = ImGui::GetColorU32(IM_COL32_WHITE);

        // Draw the skill icon
        auto icon_desc = GetTextureDesc(*skill_icon);
        auto icon_tex_size = ImVec2(icon_desc.Width, icon_desc.Height);
        int32_t uv_offset = ((int32_t)icon_desc.Width * 3) / 64; // 3 when 64px, 6 when 128px ...
        OffsetUVsByPixels(icon_tex_size, uv0, ImVec2(uv_offset, uv_offset));
        OffsetUVsByPixels(icon_tex_size, uv1, ImVec2(-uv_offset, -uv_offset));
        draw_list->AddImage(*skill_icon, min, max, uv0, uv1, tint);

        // Draw the skill overlay/lens
        auto overlay_desc = GetTextureDesc(*skill_overlays);
        auto overlay_tex_size = ImVec2(overlay_desc.Width, overlay_desc.Height);
        GetImageUVsInAtlas(*skill_overlays, GW::Dims(56, 56), overlay_index, uv0, uv1);
        // OffsetUVsByPixels(overlay_tex_size, uv0, ImVec2(0, 0));
        OffsetUVsByPixels(overlay_tex_size, uv1, ImVec2(0, -1));
        draw_list->AddImage(*skill_overlays, min, max, uv0, uv1, tint);

        if (as_hovered)
        {
            // Draw the hover effect
            auto skill_hover_effect = TextureModule::LoadTextureFromFileId(KnownFileIDs::UI_SkillHoverOverlay);
            if (skill_hover_effect && *skill_hover_effect)
            {
                draw_list->AddCallback(
                    [](const ImDrawList *parent_list, const ImDrawCmd *cmd)
                    {
                        // The hover effect texture's colors are premultiplied by alpha
                        // By using D3DBLEND_DESTCOLOR and D3DBLEND_ONE we essentially do: dst = dst + dst * tex
                        // So where the texture is white, the color is multiplied by 2, where it's black, it's unchanged
                        auto device = GW::Render::GetDevice();
                        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
                        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
                    },
                    nullptr
                );
                draw_list->AddImage(*skill_hover_effect, min, max, ImVec2(0, 0), ImVec2(1, 1), ImColor(0.6f, 0.6f, 0.6f, 1.f));
                draw_list->AddCallback(
                    [](const ImDrawList *parent_list, const ImDrawCmd *cmd)
                    {
                        auto device = GW::Render::GetDevice();
                        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
                        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
                    },
                    nullptr
                );
            }
        }

        if (skill_type_icons && *skill_type_icons)
        {
            int32_t offset = -1;
            if (skill.combo == 1)
                offset = 0;
            else if (skill.combo == 2)
                offset = 1;
            else if (skill.type == GW::Constants::SkillType::Enchantment)
                offset = 2;
            else if (skill.combo == 3)
                offset = 3;
            else if (skill.type == GW::Constants::SkillType::Hex)
                offset = 4;
            else if (skill.type == GW::Constants::SkillType::WeaponSpell)
                offset = 5;

            if (offset != -1)
            {
                // auto type_icon_desc = GetTextureDesc(*skill_type_icons);
                // auto type_icon_tex_size = ImVec2(type_icon_desc.Width, type_icon_desc.Height);
                auto min_ur = min + ImVec2(icon_size * 0.42f, 0);
                auto max_ur = max + ImVec2(0, -icon_size * 0.42f);
                GetImageUVsInAtlas(*skill_type_icons, GW::Dims(32, 32), offset, uv0, uv1);
                draw_list->AddImage(*skill_type_icons, min_ur, max_ur, uv0, uv1, tint);
            }
        }

        ImGui::ItemSize(size);
        ImGui::ItemAdd(ImRect(min, max), 0);

        return true;
    }

    void GetImageUVsInAtlas(GW::Dims atlas_size, GW::Dims image_size, uint32_t index, ImVec2 &uv0, ImVec2 &uv1)
    {
        if (image_size.width == 0 || image_size.height == 0)
        {
            uv0 = ImVec2(0, 0);
            uv1 = ImVec2(1, 1);
            return;
        }
        float cols = (float)atlas_size.width / image_size.width;
        float rows = (float)atlas_size.height / image_size.height;
        auto n_cols = static_cast<uint32_t>(cols);
        if (cols == 0 ||
            rows == 0 ||
            n_cols == 0)
        {
            uv0 = ImVec2(0, 0);
            uv1 = ImVec2(1, 1);
            return;
        }
        auto row = index / n_cols;
        auto col = index % n_cols;
        uv0 = ImVec2(col / cols, row / rows);
        uv1 = ImVec2((col + 1) / cols, (row + 1) / rows);
    }

    void GetImageUVsInAtlas(IDirect3DTexture9 *texture, GW::Dims image_size, uint32_t index, ImVec2 &uv0, ImVec2 &uv1)
    {
        if (texture == nullptr)
        {
            uv0 = ImVec2(0, 0);
            uv1 = ImVec2(1, 1);
            return;
        }
        D3DSURFACE_DESC desc;
        texture->GetLevelDesc(0, &desc);
        GetImageUVsInAtlas(GW::Dims(desc.Width, desc.Height), image_size, index, uv0, uv1);
    }

    void OffsetUVsByPixels(ImVec2 source_texture_size, ImVec2 &uv, ImVec2 offset)
    {
        uv.x += offset.x / source_texture_size.x;
        uv.y += offset.y / source_texture_size.y;
    }

    DrawPacket GetPacket_Image(uint32_t file_id, ImVec2 size, ImVec2 uv0, ImVec2 uv1, ImVec4 tint_col, ImVec4 border_col)
    {
        auto packet = DrawPacket{};
        packet.tex = TextureModule::LoadTextureFromFileId(file_id);
        if (!(packet.tex && *packet.tex))
            return packet;

        packet.size = size;
        packet.uv0 = uv0;
        packet.uv1 = uv1;
        packet.tint_col = tint_col;
        packet.border_col = border_col;

        return packet;
    }

    DrawPacket GetPacket_ImageInAtlas(uint32_t file_id, ImVec2 size, ImVec2 atlas_image_size, uint32_t atlas_index, ImVec2 uv0_pixel_offset, ImVec2 uv1_pixel_offset, ImVec4 tint_col, ImVec4 border_col)
    {
        auto packet = DrawPacket{};
        packet.tex = TextureModule::LoadTextureFromFileId(file_id);
        if (!(packet.tex && *packet.tex))
            return packet;

        packet.size = size;
        packet.tint_col = tint_col;
        packet.border_col = border_col;

        D3DSURFACE_DESC desc;
        (*packet.tex)->GetLevelDesc(0, &desc);
        auto atlas_size = ImVec2(desc.Width, desc.Height);
        TextureModule::GetImageUVsInAtlas(*packet.tex, GW::Dims(atlas_image_size.x, atlas_image_size.y), atlas_index, packet.uv0, packet.uv1);
        TextureModule::OffsetUVsByPixels(atlas_size, packet.uv0, uv0_pixel_offset);
        TextureModule::OffsetUVsByPixels(atlas_size, packet.uv1, uv1_pixel_offset);

        return packet;
    }

    const auto number_size_actual = ImVec2(20, 26); // Physical bounding box of the number (has some padding)
    const float number_stride = 16;                 // Space between numbers
    ImVec2 CalculateDamageNumberSize(int32_t number, float scale)
    {
        auto count = 0;
        if (number == 0)
            count = 1;
        else
        {
            count += 1;
            count += std::log10(std::abs(number)) + 1;
        }

        auto number_size_scaled = number_size_actual * scale;
        auto number_stride_scaled = number_stride * scale;
        auto number_size = ImVec2(number_stride_scaled * count + number_size_scaled.x - number_stride_scaled, number_size_scaled.y);

        return number_size;
    }

    void DrawDamageNumber(int32_t number, ImVec2 pos, float scale, DamageNumberColor color, ImDrawList *draw_list)
    {
        IDirect3DTexture9 **texture;
        // clang-format off
        switch (color) {
            case DamageNumberColor::Blue:   texture = TextureModule::LoadTextureFromFileId(KnownFileIDs::UI_BlueNumbers);   break;
            case DamageNumberColor::Green:  texture = TextureModule::LoadTextureFromFileId(KnownFileIDs::UI_GreenNumbers);  break;
            case DamageNumberColor::Red:    texture = TextureModule::LoadTextureFromFileId(KnownFileIDs::UI_RedNumbers1);   break;
            case DamageNumberColor::Pink:   texture = TextureModule::LoadTextureFromFileId(KnownFileIDs::UI_PinkNumbers);   break;
            case DamageNumberColor::Yellow:
            default:                        texture = TextureModule::LoadTextureFromFileId(KnownFileIDs::UI_YellowNumbers); break;
        }
        // clang-format on

        if (!(texture && *texture))
            return;

        auto atlas = *texture;
        auto atlas_size = ImVec2(128, 128);
        auto number_size = ImVec2(32, 32);
        auto number_size_scaled = number_size_actual * scale;
        auto number_stride_scaled = number_stride * scale;

        HerosInsight::FixedVector<uint8_t, 16> digits;
        auto rem = std::abs(number);
        while (rem)
        {
            digits.try_push(rem % 10);
            rem /= 10;
        }

        if (!draw_list)
            draw_list = ImGui::GetWindowDrawList();
        auto uv_pixel_offsets = (number_size - number_size_actual) / 2;
        ImVec2 uv_offsets = ImVec2(0, 0);
        TextureModule::OffsetUVsByPixels(atlas_size, uv_offsets, uv_pixel_offsets);

        auto ss_cursor = pos;
        auto item_min = ss_cursor;
        auto alpha = ImGui::GetStyle().Alpha;
        auto hue_color = ImGui::GetColorU32(ImVec4(1, 1, 1, alpha));

        auto DrawDigit = [&](uint32_t atlas_index)
        {
            ImVec2 uv0, uv1;
            TextureModule::GetImageUVsInAtlas(atlas, GW::Dims(atlas_size.x, atlas_size.y), atlas_index, uv0, uv1);
            uv0 += uv_offsets;
            uv1 -= uv_offsets;
            auto min = ImVec2(ss_cursor.x, ss_cursor.y);
            auto max = min + number_size_scaled;
            draw_list->AddImage(atlas, min, max, uv0, uv1, hue_color);
            ss_cursor.x += number_stride_scaled;
        };

        bool is_negative = number < 0;
        bool is_positive = number > 0;

        if (is_negative)
        {
            DrawDigit(10);
        }
        else if (is_positive)
        {
            DrawDigit(11);
        }

        while (digits.size() > 0)
        {
            auto digit = digits.pop();
            DrawDigit(digit);
        }

        auto item_max = ss_cursor + ImVec2(0, number_size_scaled.y);
        ImRect item_rect = ImRect(item_min, item_max);
        ImGui::ItemSize(item_rect);
        ImGui::ItemAdd(item_rect, 0);
    }

    bool DrawPacket::DrawOnWindow()
    {
        if (!CanDraw())
            return false;
        ImGui::Image(*tex, size, uv0, uv1, tint_col, border_col);
        return true;
    }

    bool DrawPacket::AddToDrawList(ImDrawList *draw_list, ImVec2 position, float rot_rads)
    {
        if (!CanDraw())
            return false;
        ImVec2 min = position;
        ImVec2 max = min + size;
        const auto tint = ImGui::GetColorU32(tint_col);
        if (rot_rads)
        {
            ImVec2 lr = size / 2;
            ImVec2 center = min + lr;
            ImVec2 ur(lr.x, -lr.y);
            float sin = sinf(rot_rads);
            float cos = cosf(rot_rads);
            lr = ImRotate(lr, cos, sin);
            ur = ImRotate(ur, cos, sin);
            ImVec2 ll(-ur.x, -ur.y);
            ImVec2 ul(-lr.x, -lr.y);
            lr += center;
            ur += center;
            ll += center;
            ul += center;
            ImVec2 ul_uv = uv0;
            ImVec2 lr_uv = uv1;
            ImVec2 ur_uv = ImVec2(uv1.x, uv0.y);
            ImVec2 ll_uv = ImVec2(uv0.x, uv1.y);

            draw_list->AddImageQuad(*tex, ul, ur, lr, ll, ul_uv, ur_uv, lr_uv, ll_uv, tint);
        }
        else
        {
            draw_list->AddImage(*tex, min, max, uv0, uv1, tint);
        }
        return true;
    }
}