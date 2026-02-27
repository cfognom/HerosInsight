#include <numbers>

#include <GWCA/Managers/AssetMgr.h>
#include <GWCA/Managers/TextMgr.h>

#include <bitview.h>
#include <color_conv.h>
#include <constants.h>
#include <imgui.h>
#include <make_color.h>
#include <rich_text.h>
#include <settings.h>

#include "imgui_customize.h"

struct GWFontConfig
{
    uint32_t fontIndex = 0;
    GW::TextMgr::BlitFontFlags blitFlags = (GW::TextMgr::BlitFontFlags)0;
    int32_t glyphPadding = 0;
    int32_t advanceAdjustment = 0;
    ImVec2 glyphOffset = ImVec2(0, 0);
    ImVec2 iconOffset = ImVec2(0, 0);
    uint32_t color = 0xffffffff;
    float scale = 1.f;
    std::unordered_map<wchar_t, uint32_t> advanceAdjustmentOverrides;

    uint32_t GetAdvanceAdjustment(wchar_t ch) const
    {
        auto it = advanceAdjustmentOverrides.find(ch);
        if (it != advanceAdjustmentOverrides.end())
            return it->second;
        return advanceAdjustment;
    }
};

// struct IconMapping
// {
//     uint32_t gwImageFileId;
//     uint32_t atlasIndex;
//     GW::Dims atlasSlotDims;
//     uint32_t tint;
// };

// struct IconMapper
// {
//     std::unordered_map<ImWchar, IconMapping> iconMappings;
//     IconMapper(GWFontConfig &cfg)
//     {
//         ImWchar customCh = 0xE000;

//         auto padding = cfg.glyphPadding;

//         GW::Dims iconSize(16, 16);
//         ImVec2 iconOffset{-1, -1};
//         iconOffset.y += padding;
//         auto iconEffectiveHeight = (int32_t)iconSize.height - 2;
//         iconOffset.y += std::round(((float)font->glyphHeight * 0.8f - iconEffectiveHeight) / 2);
//         auto advanceAdjustment = -2;

//         auto AddIconGlyph = [&](size_t atlas_index, uint32_t tint = IM_COL32_WHITE)
//         {
//             auto advance = iconSize.width + advanceAdjustment;
//             auto offset = iconOffset + cfg.iconOffset;
//             auto &mapping = iconMappings[customCh++];
//             mapping.gwImageFileId = TextureModule::KnownFileIDs::UI_SkillStatsIcons;
//             mapping.atlasIndex = atlas_index;
//             mapping.atlasSlotDims = iconSize;
//             mapping.tint = tint;
//         };

//         for (size_t atlas_index = 0; atlas_index <= 10; ++atlas_index)
//         {
//             AddIconGlyph(atlas_index);
//         }
//         AddIconGlyph(2, MakeColor::U32::rgb(77, 84, 179));
//     }
// };

uint32_t Col4444ToCol8888(uint16_t px)
{
    uint8_t _1 = px & 0xF; // high 4 bits
    uint8_t _2 = (px >> 4) & 0xF;
    uint8_t _3 = (px >> 8) & 0xF;
    uint8_t _4 = (px >> 12) & 0xF;

    // expand 4-bit to 8-bit
    _1 *= 0x11; // 0xF → 0xFF
    _2 *= 0x11;
    _3 *= 0x11;
    _4 *= 0x11;

    return (_4 << 24) | (_3 << 16) | (_2 << 8) | _1;
}

inline constexpr uint8_t MulChannel(uint8_t a, uint8_t b)
{
    // a*b ranges 0..65025
    // add 127 for rounding, divide by 255 to scale back to 0..255
    return (a * b + 127) / 255;
}

// Multiply two 32-bit colors (RGBA) channel-wise
inline uint32_t MultiplyColors32(uint32_t c1, uint32_t c2)
{
    uint8_t x = MulChannel((c1 >> 0) & 0xFF, (c2 >> 0) & 0xFF);
    uint8_t y = MulChannel((c1 >> 8) & 0xFF, (c2 >> 8) & 0xFF);
    uint8_t z = MulChannel((c1 >> 16) & 0xFF, (c2 >> 16) & 0xFF);
    uint8_t w = MulChannel((c1 >> 24) & 0xFF, (c2 >> 24) & 0xFF);

    return (w << 24) | (z << 16) | (y << 8) | x;
}

struct GwFontLoader : ImFontLoader
{
    struct LoaderData
    {
        GW::TextMgr::FontHandle fontHandle;

        LoaderData(uint32_t fontIndex)
            : fontHandle(fontIndex)
        {
        }
    };

    static bool SrcInit(ImFontAtlas *atlas, ImFontConfig *src)
    {
        auto *fontData = (GWFontConfig *)src->FontData;
        assert(!src->FontLoaderData);
        src->FontLoaderData = new LoaderData(fontData->fontIndex);
        return true;
    }

    static void SrcDestroy(ImFontAtlas *atlas, ImFontConfig *src)
    {
        auto *fontData = (GWFontConfig *)src->FontData;
        auto *loaderData = (LoaderData *)src->FontLoaderData;
        if (loaderData)
        {
            delete loaderData;
            src->FontLoaderData = nullptr;
        }
    }

    static bool SrcContainsGlyph(ImFontAtlas *atlas, ImFontConfig *src, ImWchar codepoint)
    {
        auto *loaderData = (LoaderData *)src->FontLoaderData;
        auto font = loaderData->fontHandle.font;
        auto fontRanges = font->fontRanges.span();
        for (auto &range : fontRanges)
        {
            if (range.begin_ch <= codepoint && codepoint < range.term_ch)
                return true;
        }
        // if (codepoint >= 0xE000 && codepoint <= 0xE0FF) // adjust to your icons
        //     return true;

        return false;
    }

    static bool BakedInit(ImFontAtlas *atlas, ImFontConfig *src, ImFontBaked *baked, void *loader_data_for_baked_src)
    {
        return true;
    }

    static void BakedDestroy(ImFontAtlas *atlas, ImFontConfig *src, ImFontBaked *baked, void *loader_data_for_baked_src)
    {
    }

    static bool GwLoader_FontBakedLoadGlyph(
        ImFontAtlas *atlas,
        ImFontConfig *src,
        ImFontBaked *baked,
        void *loader_data_for_baked_src,
        ImWchar codepoint,
        ImFontGlyph *out_glyph,
        float *out_advance_x
    )
    {
        auto &cfg = *(GWFontConfig *)src->FontData;
        auto *loaderData = (LoaderData *)src->FontLoaderData;

        const auto padding = cfg.glyphPadding;

        ImFontGlyph &g = *out_glyph;
        g.Codepoint = codepoint;

        // Handle icons in your 0xE000+ range separately
        const bool is_icon = (codepoint >= 0xE000);

        uint32_t padded_width = 0;

        if (!is_icon)
        {
            auto &fontHandle = loaderData->fontHandle;
            auto font = fontHandle.font;

            const auto glyph_height = font->glyphHeight;
            const auto padded_height = glyph_height + padding * 2;

            // Normal glyph path (same as your EnqueueRange)
            uint32_t glyph_width, glyph_offset_x;
            if (auto glyph = GW::TextMgr::GetGlyphByChar(font, codepoint))
            {
                glyph_offset_x = std::numeric_limits<uint32_t>::max();
                glyph_width = 0;
                for (size_t i = 0; i < 4; ++i)
                {
                    // GW uses 4 collision lanes for each glyph to achieve nice kerning, but imgui uses only one: We take max of all lanes
                    glyph_offset_x = std::min(glyph_offset_x, glyph->metrics.lane_start[i]);
                    glyph_width = std::max(glyph_width, glyph->metrics.lane_width[i]);
                }
                g.Visible = true;
            }
            else
            {
                glyph_offset_x = 0;
                glyph_width = font->advance_unit;
                g.Visible = false;
            }

            padded_width = glyph_width + padding * 2;

            uint32_t advanceAdjustment = cfg.GetAdvanceAdjustment(codepoint);

            g.AdvanceX = float(glyph_width + advanceAdjustment);
            g.Colored = false;

            // Mimic your offset usage: (-padding, 0) + cfg.glyphOffset for text
            auto offset = is_icon ? cfg.iconOffset
                                  : (ImVec2(-padding, 0.0f) + cfg.glyphOffset);
            g.X0 = offset.x;
            g.Y0 = offset.y;
            g.X1 = g.X0 + (float)padded_width;
            g.Y1 = g.Y0 + (float)padded_height;

            ImFontAtlasRect rect;
            g.PackId = atlas->AddCustomRect(padded_width, padded_height, &rect);
            if (g.PackId == -1)
                return false;

            const auto blitSize = padded_width * padded_height;

            std::vector<uint8_t> glyphDataBuffer;
            std::vector<uint16_t> blitDataBuffer;
            glyphDataBuffer.resize(blitSize);
            blitDataBuffer.resize(blitSize);

            GW::Ptr2D glyphDataPtr{glyphDataBuffer.data(), padded_width};
            GW::Ptr2D blitDataPtr{blitDataBuffer.data(), padded_width};

            std::memset(glyphDataBuffer.data(), 0, blitSize * sizeof(decltype(glyphDataBuffer)::value_type));
            std::memset(blitDataBuffer.data(), 0, blitSize * sizeof(decltype(blitDataBuffer)::value_type));

            auto encGlyph = GW::TextMgr::GetGlyphByChar(font, codepoint);
            GW::TextMgr::DecodeGlyph(font, encGlyph, glyphDataPtr.Index(padding, padding));

            GW::Dims glyphDims{padded_width, padded_height};
            GW::TextMgr::BlitFontARGB4444(font, blitDataPtr, glyphDataBuffer.data(), glyphDims, cfg.color, cfg.blitFlags);

            GW::Ptr2D<uint32_t> slotPtr{
                .data = (uint32_t *)atlas->TexData->GetPixelsAt(rect.x, rect.y),
                .pitch = (uint32_t)atlas->TexData->Width,
            };

            for (int y = 0; y < rect.h; ++y)
            {
                for (int x = 0; x < rect.w; ++x)
                {
                    auto blittedValue = *blitDataPtr.Index(x + glyph_offset_x, y).data;
                    auto dstPx = slotPtr.Index(x, y).data;
                    *dstPx = Col4444ToCol8888(blittedValue);
                }
            }
        }
        else
        {
            // Icon path similar to your AddIconGlyph, using a fixed size
            const GW::Dims iconSize(16, 16); // or store in cfg
            padded_width = iconSize.width;
            g.AdvanceX = float(iconSize.width - 2); // like your advanceAdjustment
        }

        if (out_advance_x)
        {
            *out_advance_x = g.AdvanceX;
        }

        // if (!is_icon)
        // {
        // }
        // else
        // {
        //     // Icon glyph: copy from decoded image, apply tint, like your icon loop
        //     // You’ll need some mapping from codepoint to (fileId, atlasIndex, tint).
        //     // A simple approach is to encode that mapping into GWFontConfig or a
        //     // separate global map keyed by codepoint.

        //     auto it = iconMappings.find(codepoint);
        //     if (it == iconMappings.end())
        //         return false;

        //     IconMapping &mapping = it->second;

        //     GW::AssetMgr::DecodedImage decodedImg(mapping.gwImageFileId);
        //     GW::Ptr2D<uint32_t> decodedPtr{(uint32_t *)decodedImg.image, decodedImg.dims.width};

        //     ImVec2 uv0, uv1;
        //     TextureModule::GetImageUVsInAtlas(decodedImg.dims, mapping.atlasSlotDims, mapping.atlasIndex, uv0, uv1);

        //     auto slotPtr = atlasPtr.Index(rect->x, rect->y);
        //     auto srcPtr = decodedPtr.Index(
        //         (uint32_t)std::round(uv0.x * decodedImg.dims.width),
        //         (uint32_t)std::round(uv0.y * decodedImg.dims.height)
        //     );
        //     srcPtr.CopyTo(slotPtr, mapping.atlasSlotDims);

        //     if (mapping.tint != 0xFFFFFFFF)
        //     {
        //         slotPtr.ForEach(
        //             [tint = mapping.tint](uint32_t &px)
        //             {
        //                 px = MultiplyColors32(px, tint);
        //             },
        //             mapping.atlasSlotDims
        //         );
        //     }
        // }

        return true;
    }

    GwFontLoader()
    {
        Name = "GWFontLoader";
        FontSrcInit = SrcInit;
        FontSrcDestroy = SrcDestroy;
        FontSrcContainsGlyph = SrcContainsGlyph;
        FontBakedInit = BakedInit;
        FontBakedDestroy = BakedDestroy;
        FontBakedLoadGlyph = GwLoader_FontBakedLoadGlyph;
        // FontBakedSrcLoaderDataSize = sizeof(GwFontSrcData);
    }
};
GwFontLoader g_GwFontLoader;

ImFont *CreateGWFont(GWFontConfig cfg)
{
    ImGuiIO &io = ImGui::GetIO();

    ImFontConfig config;
    auto *fontData = new GWFontConfig(std::move(cfg));
    config.FontData = fontData;
    config.FontDataSize = sizeof(*fontData);

    GW::TextMgr::FontHandle fontHandle{cfg.fontIndex};
    auto font = fontHandle.font;
    const int padded_height = font->glyphHeight + cfg.glyphPadding * 2;
    config.SizePixels = (float)padded_height;
    config.FontLoader = &g_GwFontLoader;
    ImFont *imFont = io.Fonts->AddFont(&config);
    imFont->Flags = ImFontFlags_LockBakedSizes;
    imFont->Scale = cfg.scale;

    return imFont;
}

struct FontBlitCommand
{
    struct Glyph
    {
        ImWchar ch;
        int dstRect;
        uint32_t x_offset;
    };
    struct IconFile
    {
        struct Mapping
        {
            int dstRect;
            int atlasIndex;
            uint32_t tint;
        };
        uint32_t gwImageFileId;
        GW::Dims iconDims;
        std::vector<Mapping> mappings;
    };
    GWFontConfig config;
    ImFont *imFont;
    std::vector<Glyph> glyphs;
    std::vector<IconFile> icons;
};
std::vector<FontBlitCommand> font_blit_commands;

ImFont *CreateGWFontOld(GWFontConfig cfg)
{
    auto &io = ImGui::GetIO();

    GW::TextMgr::FontHandle fontHandle(cfg.fontIndex);
    auto font = fontHandle.font;

    auto padding = cfg.glyphPadding;
    int32_t padded_height = font->glyphHeight + padding * 2;

    ImFontConfig config;
    config.SizePixels = padded_height;
    auto imFont = io.Fonts->AddFontDefault(&config);
    imFont->Scale = cfg.scale;

    auto &command = font_blit_commands.emplace_back();
    command.config = cfg;
    command.imFont = imFont;

    auto EnqueueRange = [&](ImWchar begin, ImWchar end)
    {
        for (ImWchar ch = begin; ch < end; ++ch)
        {
            auto glyph = GW::TextMgr::GetGlyphByChar(font, ch);
            uint32_t glyph_offset_x, glyph_width;
            if (glyph)
            {
                glyph_offset_x = std::numeric_limits<uint32_t>::max();
                glyph_width = 0;
                for (size_t i = 0; i < 4; ++i)
                {
                    // GW uses 4 collision lanes for each glyph to achieve nice kerning, but imgui uses only one: We take max of all lanes
                    glyph_offset_x = std::min(glyph_offset_x, glyph->metrics.lane_start[i]);
                    glyph_width = std::max(glyph_width, glyph->metrics.lane_width[i]);
                }
            }
            else
            {
                glyph_offset_x = 0;
                glyph_width = font->advance_unit;
            }
            uint32_t padded_width = glyph_width + padding * 2;
            auto advanceOverrride_it = cfg.advanceAdjustmentOverrides.find(ch);
            uint32_t advanceAdjustment = advanceOverrride_it != cfg.advanceAdjustmentOverrides.end() ? advanceOverrride_it->second : cfg.advanceAdjustment;
            uint32_t advance = glyph_width + advanceAdjustment;
            auto offset = ImVec2(-padding, 0) + cfg.glyphOffset;
            auto id = io.Fonts->AddCustomRectFontGlyph(imFont, ch, padded_width, padded_height, advance, offset);
            auto &g = command.glyphs.emplace_back();
            g.ch = ch;
            g.dstRect = id;
            g.x_offset = glyph_offset_x;
        }
    };
    auto defaultGlyphRanges = io.Fonts->GetGlyphRangesDefault();
    auto r = defaultGlyphRanges;
    while (*r)
    {
        auto begin = *r++;
        assert(*r);
        auto end = *r++;
        EnqueueRange(begin, end);
    }

    ImWchar customCh = 0xE000;

    { // Add custom icons
        GW::Dims iconSize(16, 16);
        auto &entry = command.icons.emplace_back();
        entry.gwImageFileId = TextureModule::KnownFileIDs::UI_SkillStatsIcons;
        entry.iconDims = iconSize;
        ImVec2 iconOffset{-1, -1};
        iconOffset.y += padding;
        auto iconEffectiveHeight = (int32_t)iconSize.height - 2;
        iconOffset.y += std::round(((float)font->glyphHeight * 0.8f - iconEffectiveHeight) / 2);
        auto advanceAdjustment = -2;

        auto AddIconGlyph = [&](size_t atlas_index, uint32_t tint = IM_COL32_WHITE)
        {
            auto advance = iconSize.width + advanceAdjustment;
            auto offset = iconOffset + cfg.iconOffset;
            auto id = io.Fonts->AddCustomRectFontGlyph(imFont, customCh++, iconSize.width, iconSize.height, advance, offset);
            auto &mapping = entry.mappings.emplace_back();
            mapping.atlasIndex = atlas_index;
            mapping.dstRect = id;
            mapping.tint = tint;
        };

        for (size_t atlas_index = 0; atlas_index <= 10; ++atlas_index)
        {
            AddIconGlyph(atlas_index);
        }
        AddIconGlyph(2, MakeColor::U32::rgb(77, 84, 179));
    }

    return imFont;
}

void AddFonts(ImGuiIO &io)
{
    auto res_path = Constants::paths.resources();
    // First font is used by default
    Constants::Fonts::gw_font_16 = CreateGWFont(GWFontConfig{
        // .blitFlags = GW::TextMgr::BlitFontFlags::Outline,
        .glyphPadding = 1,
        .advanceAdjustment = 1,
        // .glyphOffset = ImVec2(1, 0),
    });
    Constants::Fonts::gw_font_16_hl = CreateGWFont(GWFontConfig{
        .blitFlags = GW::TextMgr::BlitFontFlags::Outline,
        .glyphPadding = 1,
        .advanceAdjustment = 1,
    });
    Constants::Fonts::button_font = CreateGWFont(GWFontConfig{
        .blitFlags = GW::TextMgr::BlitFontFlags::AmbientOcclusion,
        .glyphPadding = 1,
        .advanceAdjustment = 1,
    });
    Constants::Fonts::window_name_font = CreateGWFont(GWFontConfig{
        .fontIndex = 1,
        .blitFlags = GW::TextMgr::BlitFontFlags::AmbientOcclusion,
        .glyphPadding = 2,
        .advanceAdjustment = 1,
        .color = MakeColor::U32::rgb(221, 221, 221),
    });
    Constants::Fonts::skill_thick_font_15 = CreateGWFont(GWFontConfig{
        .fontIndex = 1,
        .glyphPadding = 1,
        .advanceAdjustmentOverrides = {
            {L'*', -2},
        },
    });
    Constants::Fonts::skill_name_font = CreateGWFont(GWFontConfig{
        .fontIndex = 3,
        .glyphPadding = 1,
        .advanceAdjustment = 1,
    });

    // ImFontConfig config;
    // config.GlyphOffset.y = 2;
    // Constants::Fonts::skill_thick_font_12 = io.Fonts->AddFontFromFileTTF((res_path / "friz-quadrata-std-bold-587034a220f9f.otf").string().c_str(), 12.0f, &config, io.Fonts->GetGlyphRangesDefault());
    // Constants::Fonts::skill_thick_font_9 = io.Fonts->AddFontFromFileTTF((res_path / "friz-quadrata-std-bold-587034a220f9f.otf").string().c_str(), 9.0f, &config, io.Fonts->GetGlyphRangesDefault());
    Constants::Fonts::skill_thick_font_12 = CreateGWFont(GWFontConfig{
        .fontIndex = 1,
        .glyphPadding = 1,
        .scale = 0.8f,
        .advanceAdjustmentOverrides = {
            {L'*', -2},
        },
    });
    Constants::Fonts::skill_thick_font_9 = CreateGWFont(GWFontConfig{
        .fontIndex = 1,
        .glyphPadding = 1,
        .scale = 0.6f,
        .advanceAdjustmentOverrides = {
            {L'*', -2},
        },
    });
}

static void BlitGWGlyphsToFontAtlas(ImGuiIO &io)
{
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
    int bytes_per_pixel;
    io.Fonts->GetTexDataAsRGBA32((unsigned char **)&pixels, (int32_t *)&width, (int32_t *)&height, &bytes_per_pixel);
    assert(bytes_per_pixel == 4);

    std::vector<uint8_t> glyphDataBuffer;
    std::vector<uint16_t> blitDataBuffer;

    GW::Ptr2D atlasDataPtr{pixels, width};

    for (auto &com : font_blit_commands)
    {
        auto padding = com.config.glyphPadding;
        GW::TextMgr::FontHandle fontHandle(com.config.fontIndex);
        auto font = fontHandle.font;
        auto blitWidth = font->glyphWidth + padding * 2;
        auto blitHeight = font->glyphHeight + padding * 2;
        auto blitSize = blitWidth * blitHeight;
        GW::Dims glyphDims{blitWidth, blitHeight};
        glyphDataBuffer.resize(blitSize);
        blitDataBuffer.resize(blitSize);
        GW::Ptr2D glyphDataPtr{glyphDataBuffer.data(), blitWidth};
        GW::Ptr2D blitDataPtr{blitDataBuffer.data(), blitWidth};

        for (auto &g : com.glyphs)
        {
            std::memset(glyphDataBuffer.data(), 0, blitSize * sizeof(decltype(glyphDataBuffer)::value_type));
            // std::memset(blitDataBuffer.data(), 0, blitSize * sizeof(decltype(blitDataBuffer)::value_type));
            auto encGlyph = GW::TextMgr::GetGlyphByChar(font, g.ch);
            GW::TextMgr::DecodeGlyph(font, encGlyph, glyphDataPtr.Index(padding, padding));

            auto rect = io.Fonts->GetCustomRectByIndex(g.dstRect);
            auto slotPtr = atlasDataPtr.Index(rect->x, rect->y);

            GW::TextMgr::BlitFontARGB4444(font, blitDataPtr, glyphDataBuffer.data(), glyphDims, com.config.color, com.config.blitFlags);

            for (int y = 0; y < rect->h; y++)
            {
                for (int x = 0; x < rect->w; x++)
                {
                    auto blittedValue = *blitDataPtr.Index(x + g.x_offset, y).data;
                    auto dstPx = slotPtr.Index(x, y).data;
                    *dstPx = Col4444ToCol8888(blittedValue);
                }
            }
        }

        for (auto &entry : com.icons)
        {
            GW::AssetMgr::DecodedImage decoded(entry.gwImageFileId);
            assert(decoded.image);
            GW::Ptr2D<uint32_t> decodedPtr{(uint32_t *)decoded.image, decoded.dims.width};
            ImVec2 uv0, uv1;
            for (auto &mapping : entry.mappings)
            {
                auto rect = io.Fonts->GetCustomRectByIndex(mapping.dstRect);
                auto slotPtr = atlasDataPtr.Index(rect->x, rect->y);
                TextureModule::GetImageUVsInAtlas(decoded.dims, entry.iconDims, mapping.atlasIndex, uv0, uv1);
                auto srcPtr = decodedPtr.Index(
                    std::round(uv0.x * decoded.dims.width),
                    std::round(uv0.y * decoded.dims.height)
                );
                srcPtr.CopyTo(slotPtr, entry.iconDims);
                if (mapping.tint != 0xFFFFFFFF)
                {
                    slotPtr.ForEach(
                        [tint = mapping.tint](uint32_t &px)
                        {
                            px = MultiplyColors32(px, tint);
                        },
                        entry.iconDims
                    );
                }
            }
        }
    }
}

void InvertDefaultStyleColors()
{
    auto &style = ImGui::GetStyle();
    auto &colors = style.Colors;
    // Inverting red and blue channels as a simple method for getting a unique style.
    for (int i = 0; i < ImGuiCol_COUNT; i++)
    {
        ImVec4 &col = colors[i];
        float temp = col.x;
        col.x = col.z;
        col.z = temp;
    }
}

float GeometricalMul(float a, float b)
{
    return std::clamp(std::sqrt(a * b), 0.f, 1.f);
};

struct ColorTheme
{
    ImVec4 colors[ImGuiCol_COUNT];
    operator std::span<ImVec4, ImGuiCol_COUNT>() { return std::span<ImVec4, ImGuiCol_COUNT>(colors); }
};

ColorTheme themes[HerosInsight::Settings::Style::Theme::COUNT];

float Dot(const ImVec2 &a, const ImVec2 &b)
{
    return a.x * b.x + a.y * b.y;
}

float SqrMag(const ImVec2 &a)
{
    return Dot(a, a);
}

float Mag(const ImVec2 &a)
{
    return std::sqrt(SqrMag(a));
}

ImVec2 Normalize(const ImVec2 &a)
{
    float len = Mag(a);
    if (len == 0.f)
        return ImVec2{1.f, 0.f};
    return ImVec2{a.x / len, a.y / len};
}

ImVec2 RebaseVec2(const ImVec2 &v, const ImVec2 &basis)
{
    ImVec2 basis_x = Normalize(basis);
    ImVec2 basis_y{-basis_x.y, basis_x.x};
    return ImVec2{
        Dot(v, basis_x),
        Dot(v, basis_y)
    };
}

ImVec2 HueVec(float h)
{
    auto h_rad = h * 2.f * std::numbers::pi_v<float>;
    return ImVec2{std::cos(h_rad), std::sin(h_rad)};
}

struct ColorDescriptor
{
    BitArray<ImGuiCol_COUNT> targets;
    template <size_t... ColorId>
    static constexpr ColorDescriptor Make()
    {
        ColorDescriptor cd;
        ((cd.targets[ColorId] = true), ...);
        return cd;
    }
};

struct ThemeCustomizer
{
    BitArray<ImGuiCol_COUNT> unmodified_colors{true};
    std::span<ImVec4, ImGuiCol_COUNT> src_colors;
    std::span<ImVec4, ImGuiCol_COUNT> dst_colors;

    ThemeCustomizer(std::span<ImVec4, ImGuiCol_COUNT> src_colors, std::span<ImVec4, ImGuiCol_COUNT> dst_colors)
        : src_colors(src_colors), dst_colors(dst_colors) {}

    void TintColors(ColorDescriptor &&colors, ImVec4 tint)
    {
        BitView affected = colors.targets;
        float th, ts, tv;
        {
            ImGui::ColorConvertRGBtoHSV(tint.x, tint.y, tint.z, th, ts, tv);
        }
        for (auto color_idx : affected.IterSetBits())
        {
            auto color = src_colors[color_idx];
            float h, s, v;
            ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
            ImGui::ColorConvertHSVtoRGB(th, GeometricalMul(s, ts), GeometricalMul(v, tv), color.x, color.y, color.z);
            color.w = color.w * tint.w;
            dst_colors[color_idx] = color;
        }
    }

    void RebaseColors(ColorDescriptor &colors, ImVec4 base_color)
    {
        BitView affected = colors.targets;

        const auto count = affected.PopCount();

        std::vector<ImVec4> hsla_colors;
        std::vector<ImVec2> hue_vecs;
        hsla_colors.reserve(count);
        hue_vecs.reserve(count);
        for (auto color_idx : affected.IterSetBits())
        {
            auto &rgba = src_colors[color_idx];
            auto hsva = ColorConv::RGBAToHSVA(rgba);
            hsla_colors.push_back(hsva);
            auto hue = hsva.x;
            hue_vecs.push_back(HueVec(hue));
        }

        ImVec4 base_color_hsla = ColorConv::RGBAToHSVA(base_color);
        ImVec2 base_hue_vec = HueVec(base_color_hsla.x);
        ImVec4 avg_hsla{0, 0, 0, 0};
        ImVec2 avg_hue{0, 0};
        for (size_t i = 0; i < count; ++i)
        {
            auto &color = hsla_colors[i];
            auto &hue_vec = hue_vecs[i];
            avg_hsla = avg_hsla + color;
            avg_hue = avg_hue + hue_vec;
        }
        avg_hsla.x /= count;
        avg_hsla.y /= count;
        avg_hsla.z /= count;
        avg_hsla.w /= count;
        avg_hue /= count;
        size_t i = 0;
        auto avg_hue_rads = std::atan2(avg_hue.y, avg_hue.x);
        auto base_hue_rads = std::atan2(base_hue_vec.y, base_hue_vec.x);
        auto hue_rads_diff = base_hue_rads - avg_hue_rads;
        for (auto color_idx : affected.IterSetBits())
        {
            auto &hsva = hsla_colors[i];
            auto &hue_vec = hue_vecs[i];
            ++i;

            auto hue_rads = std::atan2(hue_vec.y, hue_vec.x);
            hue_rads += base_hue_rads;
            auto hue = hue_rads / (2.f * std::numbers::pi_v<float>);
            hue -= std::floor(hue);
            hue = std::clamp(hue, 0.f, 1.f);

            // auto rebased_color_hsla = base_color_hsla + color - avg_hsla;
            ImVec4 new_hsla{hue, hsva.y, hsva.z, hsva.w};
            auto rgba = ColorConv::HSVAToRGBA(new_hsla);
            dst_colors[color_idx] = rgba;
        }
    }

    void SetColors(ColorDescriptor &&colors, ImVec4 color)
    {
        BitView affected = colors.targets;
        for (auto color_id : affected.IterSetBits())
        {
            unmodified_colors[color_id] = false;
            dst_colors[color_id] = color;
        }
    }

    void UseDefault(ColorDescriptor &&colors)
    {
        BitView affected = colors.targets;
        for (auto color_id : affected.IterSetBits())
        {
            unmodified_colors[color_id] = false;
            dst_colors[color_id] = src_colors[color_id];
        }
    }
};

void HerosInsight::ImGuiCustomize::RefreshStyle()
{
    HerosInsight::SettingsGuard settings_guard{};
    auto &settings = settings_guard.Access();
    auto &style = settings.style;
    auto themeId = (Settings::Style::Theme)style.theme.value;
    auto &theme = themes[themeId];

    auto &style_colors = ImGui::GetStyle().Colors;
    ThemeCustomizer customizer(
        theme,
        style_colors
    );

    // TintStyleColors(base_colors_set, style.base_tint.value);
}

void SaveCurrentStyleColors(std::span<ImVec4, ImGuiCol_COUNT> dst_colors)
{
    auto &style = ImGui::GetStyle();
    auto &colors = style.Colors;
    static_assert(std::size(colors) == std::size(dst_colors));
    std::memcpy(dst_colors.data(), colors, sizeof(ImVec4) * std::size(colors));
}

void CreateGWTheme()
{
    ThemeCustomizer customizer(
        themes[HerosInsight::Settings::Style::Theme::ImGuiDefault],
        themes[HerosInsight::Settings::Style::Theme::GuildWars]
    );

    customizer.SetColors(
        ColorDescriptor::Make<ImGuiCol_WindowBg>(),
        MakeColor::ImVec4::rgba(15, 15, 15, 0.92)
    );
    customizer.UseDefault(
        ColorDescriptor::Make<
            ImGuiCol_Text,
            ImGuiCol_TextDisabled,
            ImGuiCol_TextSelectedBg
        >()
    );
    customizer.TintColors(
        ColorDescriptor::Make<
            ImGuiCol_TabHovered,
            ImGuiCol_TabActive,
            ImGuiCol_TabUnfocusedActive
        >(),
        Constants::GWColors::tabs_blue
    );
    customizer.SetColors(
        ColorDescriptor::Make<ImGuiCol_CheckMark>(),
        Constants::GWColors::checkmark_beige
    );
    customizer.TintColors(
        ColorDescriptor::Make<
            ImGuiCol_Button,
            ImGuiCol_ButtonHovered,
            ImGuiCol_ButtonActive
        >(),
        Constants::GWColors::button_blue
    );
}

void HerosInsight::ImGuiCustomize::Init()
{
    SaveCurrentStyleColors(themes[Settings::Style::Theme::ImGuiDefault].colors);
    CreateGWTheme();

    auto &io = ImGui::GetIO();
    auto &style = ImGui::GetStyle();

    // style.WindowBorderSize = 1.f;
    style.WindowBorderHoverPadding = 8.f;

    RefreshStyle();

    AddFonts(io);
    // BlitGWGlyphsToFontAtlas(io);

    static std::string imgui_ini_path = (Constants::paths.cache() / "imgui.ini").string();
    static std::string imgui_log_path = (Constants::paths.cache() / "imgui_log.txt").string();
    io.IniFilename = imgui_ini_path.c_str();
    io.LogFilename = imgui_log_path.c_str();
}