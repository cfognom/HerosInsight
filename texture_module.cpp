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
    class RecObj;

    struct Vec2i
    {
        int x = 0;
        int y = 0;
    };

    typedef enum : uint32_t
    {
        GR_FORMAT_A8R8G8B8 = 0, // raw?
        GR_FORMAT_UNK = 0x4,    //.bmp,...
        GR_FORMAT_DXT1 = 0xF,
        GR_FORMAT_DXT2,
        GR_FORMAT_DXT3,
        GR_FORMAT_DXT4,
        GR_FORMAT_DXT5,
        GR_FORMAT_DXTA,
        GR_FORMAT_DXTL,
        GR_FORMAT_DXTN,
        GR_FORMATS
    } GR_FORMAT;

    typedef uint8_t *gw_image_bits; // array of pointers to mipmap images

    typedef BOOL(__cdecl *DecodeImage_pt)(int size, char *bytes, gw_image_bits *out_bits, uint8_t *pallete, GR_FORMAT *format, Vec2i *dims, int *levels);
    DecodeImage_pt DecodeImage_Func;

    typedef gw_image_bits(__cdecl *AllocateImage_pt)(GR_FORMAT format, Vec2i *destDims, uint32_t levels, uint32_t unk2);
    AllocateImage_pt AllocateImage_Func;

    typedef void(__cdecl *Depalletize_pt)(
        gw_image_bits *destBits, uint8_t *destPalette, GR_FORMAT destFormat, int *destMipWidths, gw_image_bits sourceBits, uint8_t *sourcePallete, GR_FORMAT sourceFormat, int *sourceMipWidths, Vec2i *sourceDims, uint32_t sourceLevels,
        uint32_t unk1_0, int *unk2_0
    );
    Depalletize_pt Depalletize_Func;

    // typedef void(__cdecl *ConvertImage_pt) (uint8_t *destBytes, int *destPallete, uint32_t destFormat, Vec2i *destDims,
    //                                         uint8_t *sourceBytes, int *sourcePallete, uint32_t sourceFormat, Vec2i *sourceDims, float sharpness);
    // ConvertImage_pt ConvertImage_func;

    // typedef uint8_t*(__cdecl *ConvertToRaw_pt) (uint8_t *sourceBits, int *sourcePallete, uint32_t sourceFormat, Vec2i *sourceDims,
    //     Vec2i *maybeDestDims, uint32_t levelsProvided, uint32_t levelsRequested, float sharpness);
    // ConvertUnk_pt ConvertUnk_func;

    // typedef void(__cdecl *GetLevelWidths_pt) (int format, int width, uint32_t levels, int *widths);
    // GetLevelWidths_pt GetLevelWidths_func;

    char *strnstr(char *str, const char *substr, size_t n)
    {
        char *p = str, *pEnd = str + n;
        size_t substr_len = strlen(substr);

        if (0 == substr_len)
            return str; // the empty string is contained everywhere.

        pEnd -= (substr_len - 1);
        for (; p < pEnd; ++p)
        {
            if (0 == strncmp(p, substr, substr_len))
                return p;
        }
        return NULL;
    }

    IDirect3DTexture9 *CreateTexture(IDirect3DDevice9 *device, uint32_t file_id, Vec2i &dims)
    {
        if (!device || !file_id)
        {
            return nullptr;
        }

        struct AutoFree // Helper struct that automatically calls MemFree when it goes out of scope
        {
            gw_image_bits &object;
            ~AutoFree()
            {
                GW::MemoryMgr::MemFree(object);
                object = nullptr;
            }
        };

        IDirect3DTexture9 *tex = nullptr; // The return value
        gw_image_bits decoded_image = nullptr;
        gw_image_bits allocated_image = nullptr;
        uint8_t *nullptr_palette = nullptr; // We only use a var here to show were the palette goes
        GR_FORMAT format;
        int levels = 0;
        bool decode_success;

        wchar_t fileHash[4] = {0};
        GW::AssetMgr::FileIdToFileHash(file_id, fileHash);

        { // Read scope
            auto readable = GW::AssetMgr::TryReadFile(fileHash);
            if (readable)
            {
                GWCA_ASSERT(readable->data != nullptr);

                char *image_bytes = readable->data;
                uint32_t image_size = readable->size;

                if (memcmp((char *)image_bytes, "ffna", 4) == 0)
                {
                    // Model file format; try to find first instance of image from this.
                    auto found = strnstr((char *)image_bytes, "ATEX", image_size);
                    if (!found)
                        return nullptr;
                    image_bytes = found;
                    image_size = *(int *)(found - 4);
                }
                // Decodes the file data into a malloc'd block 'decoded_image' (we need to free this later)
                decode_success = DecodeImage_Func(
                    image_size, image_bytes,                                 // Inputs
                    &decoded_image, nullptr_palette, &format, &dims, &levels // Outputs
                );
                if (decoded_image == nullptr)
                    return nullptr;
            }
        } // File is cleaned up here

        { // Decoded scope
            AutoFree x{decoded_image};

            if (!decode_success ||
                format >= GR_FORMATS ||
                !dims.x || !dims.y ||
                levels > 13) // Depalletize_Func does not support more than 12 levels
                return nullptr;

            levels = 1;
            // Allocates a block of memory for the depalletized image (needs to be freed later)
            allocated_image = AllocateImage_Func(GR_FORMAT_A8R8G8B8, &dims, levels, 0);
            if (allocated_image == nullptr)
                return nullptr;

            Depalletize_Func(&allocated_image, nullptr, GR_FORMAT_A8R8G8B8, nullptr, decoded_image, nullptr_palette, format, nullptr, &dims, levels, 0, 0);
            GWCA_ASSERT(allocated_image != nullptr);
        }

        { // Depalletized scope
            AutoFree x{allocated_image};

            // Create a texture: http://msdn.microsoft.com/en-us/library/windows/desktop/bb174363(v=vs.85).aspx
            if (device->CreateTexture(dims.x, dims.y, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, 0) != D3D_OK)
                return nullptr;

            // Lock the texture for writing: http://msdn.microsoft.com/en-us/library/windows/desktop/bb205913(v=vs.85).aspx
            D3DLOCKED_RECT rect;
            if (tex->LockRect(0, &rect, 0, D3DLOCK_DISCARD) != D3D_OK)
            {
                tex->Release();
                return nullptr;
            }

            for (int y = 0; y < dims.y; y++)
            {
                auto dst = (uint8_t *)rect.pBits + y * rect.Pitch;
                auto src = (uint32_t *)allocated_image + y * dims.x;
                memcpy(dst, src, dims.x * 4);
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
    }

    struct GwImg
    {
        uint32_t m_file_id = 0;
        Vec2i m_dims;
        IDirect3DTexture9 *m_tex = nullptr;
    };

    std::map<uint32_t, GwImg *> textures_by_file_id;

    void Initialize()
    {
        using namespace GW;

        uintptr_t address = 0;

        DecodeImage_Func = (DecodeImage_pt)Scanner::ToFunctionStart(Scanner::FindAssertion("GrImage.cpp", "bits || !palette", 0, 0));

        AllocateImage_Func = (AllocateImage_pt)Scanner::ToFunctionStart(Scanner::Find(MemPattern("7c 11 6a 5c")));

        address = Scanner::ToFunctionStart(Scanner::Find(MemPattern("83 ?? 04 a8 08 74 1a 85 ?? 75 1d")));
        Depalletize_Func = (Depalletize_pt)address;

        assert(DecodeImage_Func);
        assert(AllocateImage_Func);
        assert(Depalletize_Func);
    }

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
        Vec2i m_dims;
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
            img_ptr->m_dims.x = static_cast<float>(image_info.Width);
            img_ptr->m_dims.y = static_cast<float>(image_info.Height);

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
        OffsetUVsByPixels(icon_tex_size, uv0, ImVec2(3, 3));
        OffsetUVsByPixels(icon_tex_size, uv1, ImVec2(-3, -3));
        draw_list->AddImage(*skill_icon, min, max, uv0, uv1, tint);

        // Draw the skill overlay/lens
        auto overlay_desc = GetTextureDesc(*skill_overlays);
        auto overlay_tex_size = ImVec2(overlay_desc.Width, overlay_desc.Height);
        GetImageUVsInAtlas(*skill_overlays, ImVec2(56, 56), overlay_index, uv0, uv1);
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
                GetImageUVsInAtlas(*skill_type_icons, ImVec2(32, 32), offset, uv0, uv1);
                draw_list->AddImage(*skill_type_icons, min_ur, max_ur, uv0, uv1, tint);
            }
        }

        ImGui::ItemSize(size);
        ImGui::ItemAdd(ImRect(min, max), 0);

        return true;
    }

    void GetImageUVsInAtlas(IDirect3DTexture9 *texture, ImVec2 image_size, uint32_t index, ImVec2 &uv0, ImVec2 &uv1)
    {
        if (texture == nullptr || image_size.x == 0 || image_size.y == 0)
        {
            uv0 = ImVec2(0, 0);
            uv1 = ImVec2(1, 1);
            return;
        }
        D3DSURFACE_DESC desc;
        texture->GetLevelDesc(0, &desc);
        float cols = desc.Width / image_size.x;
        float rows = desc.Height / image_size.y;
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
        TextureModule::GetImageUVsInAtlas(*packet.tex, atlas_image_size, atlas_index, packet.uv0, packet.uv1);
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
            TextureModule::GetImageUVsInAtlas(atlas, number_size, atlas_index, uv0, uv1);
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