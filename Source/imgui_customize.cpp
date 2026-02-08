#include <GWCA/Managers/AssetMgr.h>
#include <GWCA/Managers/TextMgr.h>

#include <constants.h>
#include <rich_text.h>

#include <imgui.h>

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
};

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

ImFont *CreateGWFont(GWFontConfig cfg)
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
    imFont->FallbackAdvanceX = font->advance_unit;

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
        AddIconGlyph(2, IM_COL32(77, 84, 179, 255));
    }

    return imFont;
}

void AddFonts(ImGuiIO &io)
{
    auto res_path = Constants::paths.resources();
    // First font is used by default
    Constants::Fonts::gw_font_16 = CreateGWFont(GWFontConfig{
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
        .color = 0xffdddddd,
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

uint32_t ARGB4444ToARGB8888(uint16_t px)
{
    uint8_t a = (px >> 12) & 0xF; // high 4 bits
    uint8_t r = (px >> 8) & 0xF;
    uint8_t g = (px >> 4) & 0xF;
    uint8_t b = px & 0xF;

    // expand 4-bit to 8-bit
    a = a * 0x11; // 0xF → 0xFF
    r = r * 0x11;
    g = g * 0x11;
    b = b * 0x11;

    return (a << 24) | (r << 16) | (g << 8) | b;
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
            auto slotPtr = atlasDataPtr.Index(rect->X, rect->Y);

            GW::TextMgr::BlitFontARBG4444(font, blitDataPtr, glyphDataBuffer.data(), glyphDims, com.config.color, com.config.blitFlags);

            for (int y = 0; y < rect->Height; y++)
            {
                for (int x = 0; x < rect->Width; x++)
                {
                    auto blittedValue = *blitDataPtr.Index(x + g.x_offset, y).data;
                    auto dstPx = slotPtr.Index(x, y).data;
                    *dstPx = ARGB4444ToARGB8888(blittedValue);
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
                auto slotPtr = atlasDataPtr.Index(rect->X, rect->Y);
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

void HerosInsight::ImGuiCustomize::Init()
{
    auto &io = ImGui::GetIO();
    AddFonts(io);
    BlitGWGlyphsToFontAtlas(io);
    // InvertDefaultStyleColors();

    static std::string imgui_ini_path = (Constants::paths.cache() / "imgui.ini").string();
    static std::string imgui_log_path = (Constants::paths.cache() / "imgui_log.txt").string();
    io.IniFilename = imgui_ini_path.c_str();
    io.LogFilename = imgui_log_path.c_str();
}