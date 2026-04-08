#include <cstdint>
#include <numbers>

#include <GWCA/Managers/AssetMgr.h>
#include <GWCA/Managers/TextMgr.h>

#include <bitview.h>
#include <buffer.h>
#include <color_conv.h>
#include <constants.h>
#include <imgui.h>
#include <make_color.h>
#include <rich_text.h>
#include <settings.h>
#include <utils.h>

#include "imgui_custom.h"

struct GWFontConfig
{
    using Raw = uint64_t;
    union
    {
        struct
        {
            uint32_t color;
            uint8_t language;
            uint8_t fontIndex;
            uint8_t blitFlags;
            uint8_t _padding;
        };
        Raw raw;
    };

    GW::Constants::Language Language() const { return (GW::Constants::Language)language; }
    GW::TextMgr::BlitFontFlags BlitFlags() const { return (GW::TextMgr::BlitFontFlags)blitFlags; }

    GWFontConfig(GW::TextMgr::EngFont fontId, GW::TextMgr::BlitFontFlags blitFlags = GW::TextMgr::BlitFontFlags::None)
        : color(0xffffffff),
          language((uint8_t)GW::Constants::Language::English),
          fontIndex((uint8_t)fontId),
          blitFlags((uint8_t)blitFlags),
          _padding(0)
    {
    }
};

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

template <typename T>
struct Buffer2D
{
    std::vector<T> data;

    template <bool Zeroed>
    GW::Ptr2D<T> GetSized(GW::Dims dims)
    {
        auto size = dims.width * dims.height;
        data.resize(size);
        if constexpr (Zeroed)
        {
            std::memset(data.data(), 0, size * sizeof(T));
        }
        return GW::Ptr2D{data.data(), dims.width};
    }
};

uint32_t GetBlitPadding(GW::TextMgr::BlitFontFlags flags)
{
    if (HerosInsight::Utils::HasAnyFlag(flags, GW::TextMgr::BlitFontFlags::AmbientOcclusion))
        return 3;

    if (HerosInsight::Utils::HasAnyFlag(flags, GW::TextMgr::BlitFontFlags::ShadowBR | GW::TextMgr::BlitFontFlags::ShadowTL))
        return 2;

    if (HerosInsight::Utils::HasAnyFlag(flags, GW::TextMgr::BlitFontFlags::Outline))
        return 1;

    return 0;
}

template <typename B>
concept Blitter = requires(B blitter, wchar_t ch, GW::Dims &dims, GW::Ptr2D<uint32_t> dstPtr) {
    { blitter.DetermineSize(ch, dims) } -> std::same_as<bool>;
    { blitter.Blit(ch, dstPtr, dims) };
};

struct GWGlyphBlitter
{
    struct StyleKey
    {
        using Raw = uint64_t;
        union
        {
            struct
            {
                uint32_t color;
                uint8_t blitFlags;
                uint8_t blitPadding; // Extra padding used when blitting. If zero, effects like ambient occlusion, might not have enough room
                uint16_t _padding;
            };
            Raw raw;
        };

        StyleKey(uint32_t color, GW::TextMgr::BlitFontFlags blitFlags, uint32_t blitPadding)
            : color(color), blitFlags((uint8_t)blitFlags), blitPadding((uint8_t)blitPadding), _padding(0) {}

        StyleKey(uint32_t color, GW::TextMgr::BlitFontFlags blitFlags)
            : color(color), blitFlags((uint8_t)blitFlags), blitPadding(GetBlitPadding(blitFlags)), _padding(0) {}
    };

    GW::TextMgr::GrFont &font;
    StyleKey style;

    GWGlyphBlitter(GW::TextMgr::GrFont &font, StyleKey style)
        : font(font), style(style) {}

    // Transient vars
    GW::TextMgr::EncodedGlyph *encGlyph;
    uint32_t glyphStart;
    GW::Dims blitDims;

    bool DetermineSize(wchar_t ch, GW::Dims &dims)
    {
        encGlyph = GW::TextMgr::GetGlyphByChar(font, ch);
        if (!encGlyph)
            return false;

        auto &metrics = encGlyph->metrics;
        glyphStart = std::numeric_limits<uint32_t>::max();
        uint32_t glyphEnd = 0;
        for (size_t i = 0; i < 4; ++i)
        {
            // GW uses 4 collision lanes for each glyph to achieve nice kerning, but imgui uses only one: We take max of all lanes
            uint32_t &lane_width = metrics.lane_width[i];
            if (lane_width == 0)
                continue;
            uint32_t &lane_start = metrics.lane_start[i];
            uint32_t lane_end = lane_start + lane_width;
            glyphStart = std::min(glyphStart, lane_start);
            glyphEnd = std::max(glyphEnd, lane_end);
        }
        const uint32_t glyphWidth = glyphEnd - glyphStart;
        const uint32_t glyphHeight = font.glyphHeight;
        dims = GW::Dims{
            glyphWidth + style.blitPadding * 2,
            glyphHeight + style.blitPadding * 2,
        };
        blitDims = GW::Dims{
            glyphEnd + style.blitPadding * 2,
            dims.height,
        };
        return true;
    }

    void Blit(wchar_t ch, GW::Ptr2D<uint32_t> dstPtr, const GW::Dims &dims)
    {
        thread_local Buffer2D<uint8_t> decodeDataVector;
        thread_local Buffer2D<uint16_t> blitDataVector;
        GW::Ptr2D decodeDataPtr = decodeDataVector.GetSized<true>(blitDims);
        GW::Ptr2D blitDataPtr = blitDataVector.GetSized<false>(blitDims);

        GW::TextMgr::DecodeGlyph(font, *encGlyph, decodeDataPtr.Index(style.blitPadding, style.blitPadding));

        GW::TextMgr::BlitFontARGB4444(font, blitDataPtr, decodeDataPtr.data, blitDims, style.color, (GW::TextMgr::BlitFontFlags)style.blitFlags);

        for (int y = 0; y < dims.height; ++y)
        {
            for (int x = 0; x < dims.width; ++x)
            {
                auto blittedValue = *blitDataPtr.Index(glyphStart + x, y).data;
                auto dstPx = dstPtr.Index(x, y).data;
                *dstPx = Col4444ToCol8888(blittedValue);
            }
        }
    }
};

struct CharRange
{
    wchar_t first;
    wchar_t last;

    bool Contains(wchar_t ch) const { return first <= ch && ch <= last; }
};

struct CharRanges
{
    std::vector<CharRange> ranges;

    void AddRanges(std::span<GW::TextMgr::FontData::CharRange> charRanges)
    {
        ranges.reserve(charRanges.size());
        for (auto &range : charRanges)
        {
            ranges.push_back({range.first, range.last});
        }
    }
    void AddRange(wchar_t first, wchar_t last)
    {
        ranges.push_back({first, last});
    }

    bool Contains(wchar_t ch) const
    {
        for (const auto &range : ranges)
        {
            if (range.Contains(ch))
                return true;
        }
        return false;
    }
};

wchar_t iconsBeginCodepoint = 0xE000;

struct GWIconBlitter
{
    inline static CharRange charRange;
    struct Registry
    {
        struct TextureIcon
        {
            uint32_t gwFileId;
            uint8_t atlasIndex;
            GW::Dims iconSize;
            uint32_t tintColor;
        };
        std::vector<TextureIcon> iconTextures;

        Registry()
        {
            uint32_t gwFileId = TextureModule::KnownFileIDs::UI_SkillStatsIcons;
            GW::Dims iconSize{16, 16};

            auto AddIconSpecifier = [&](uint8_t atlasIndex, uint32_t tint = 0xFFFFFFFF)
            {
                iconTextures.emplace_back(gwFileId, atlasIndex, iconSize, tint);
            };

            for (size_t atlas_index = 0; atlas_index <= 10; ++atlas_index)
            {
                AddIconSpecifier(atlas_index);
            }
            AddIconSpecifier(2, MakeColor::U32::rgb(77, 84, 179));

            charRange = CharRange{
                iconsBeginCodepoint,
                (wchar_t)(iconsBeginCodepoint + iconTextures.size() - 1)
            };
        }
    };
    inline static Registry registry{};

    bool DetermineSize(wchar_t ch, GW::Dims &dims)
    {
        assert(ch >= iconsBeginCodepoint);
        auto index = ch - iconsBeginCodepoint;
        if (index >= registry.iconTextures.size())
            return false;
        auto specifier = registry.iconTextures[index];
        dims = specifier.iconSize;
        return true;
    }

    void Blit(wchar_t ch, GW::Ptr2D<uint32_t> dstPtr, const GW::Dims &dims)
    {
        assert(ch >= iconsBeginCodepoint);
        auto index = ch - iconsBeginCodepoint;
        auto specifier = registry.iconTextures[index];

        GW::AssetMgr::DecodedImage decoded(specifier.gwFileId);
        assert(decoded.image);
        GW::Ptr2D<uint32_t> decodedPtr{(uint32_t *)decoded.image, decoded.dims.width};

        ImVec2 uv0, uv1;
        TextureModule::GetImageUVsInAtlas(decoded.dims, dims, specifier.atlasIndex, uv0, uv1);
        auto srcPtr = decodedPtr.Index(
            std::round(uv0.x * decoded.dims.width),
            std::round(uv0.y * decoded.dims.height)
        );
        srcPtr.CopyTo(dstPtr, dims);
        if (specifier.tintColor != 0xFFFFFFFF)
        {
            dstPtr.ForEach(
                [tint = specifier.tintColor](uint32_t &px)
                {
                    px = MultiplyColors32(px, tint);
                },
                dims
            );
        }
    }
};

template <Blitter B>
struct ImGuiBlitter
{
    ImFontAtlas &atlas;
    B blitter;

    ImGuiBlitter(ImFontAtlas &atlas, B &&blitter) : atlas(atlas), blitter(std::move(blitter)) {}

    int32_t GetBlittedGlyph(wchar_t ch, ImFontAtlasRect &atlasRect)
    {
        GW::Dims dims;
        if (!blitter.DetermineSize(ch, dims))
            return -1;
        auto packId = atlas.AddCustomRect(dims.width, dims.height, &atlasRect);
        assert(atlasRect.w == dims.width);
        assert(atlasRect.h == dims.height);
        assert(atlas.TexData->BytesPerPixel == 4);
        auto dstPtr = GW::Ptr2D{
            .data = (uint32_t *)atlas.TexData->GetPixelsAt(atlasRect.x, atlasRect.y),
            .pitch = (uint32_t)atlas.TexData->Width,
        };
        blitter.Blit(ch, dstPtr, dims);
        return packId;
    }
};

// Warning: PackIds are not stable!!! This cached blitter does not work!
template <Blitter B>
struct CachedImGuiBlitter
{
    struct BlittedGlyph
    {
        int32_t packId;
    };

    ImGuiBlitter<B> blitter;
    std::unordered_map<wchar_t, BlittedGlyph> charToGlyph;

    CachedImGuiBlitter(ImGuiBlitter<B> &&blitter) : blitter(std::move(blitter)) {}
    ~CachedImGuiBlitter()
    {
        for (auto &[ch, g] : charToGlyph)
        {
            if (g.packId != -1)
                blitter.atlas.RemoveCustomRect(g.packId);
        }
        charToGlyph.clear();
    }

    int32_t GetBlittedGlyph(wchar_t ch, ImFontAtlasRect &atlasRect)
    {
        auto [it, inserted] = charToGlyph.try_emplace(ch);
        auto &g = it->second;
        if (!inserted)
        {
            if (g.packId != -1)
            {
                blitter.atlas.GetCustomRect(g.packId, &atlasRect);
            }
            return g.packId;
        }
        g.packId = blitter.GetBlittedGlyph(ch, atlasRect);
        return g.packId;
    }
};

constexpr float FontLineSpacing = 2.f;

GW::TextMgr::FontData *GetGWFontDataForSize(GW::Constants::Language language, uint32_t fontIndex, float desiredHeight)
{
    auto fontDataArrays = GW::TextMgr::GetFontDataArrays(language);
    GW::TextMgr::FontData *gwFontData = nullptr;
    for (auto &fontData : fontDataArrays)
    {
        gwFontData = &fontData[fontIndex];
        auto height = gwFontData->height + FontLineSpacing;
        if (height >= desiredHeight)
            break;
    }
    return gwFontData;
}

using GlyphBlitter = ImGuiBlitter<GWGlyphBlitter>;
using IconBlitter = ImGuiBlitter<GWIconBlitter>;

struct GwFontLoader : ImFontLoader
{
    struct LoaderData
    {
        GW::Constants::Language language;
        IconBlitter iconBlitter;
        CharRanges ranges;
        std::vector<float> perfectSizes;

        LoaderData(ImFontAtlas &atlas, ImFontConfig &src)
            : iconBlitter(atlas, GWIconBlitter())
        {
            auto &cfg = *(GWFontConfig *)src.FontData;
            this->language = cfg.Language();

            auto fontDataArrays = GW::TextMgr::GetFontDataArrays(language);

            std::span<GW::TextMgr::FontData::CharRange> charRanges{};
            for (auto &fontDatas : fontDataArrays)
            {
                auto &fontData = fontDatas[cfg.fontIndex];
                auto fontHeight = fontData.height + FontLineSpacing;
                this->perfectSizes.push_back(fontHeight);
                auto newCharRanges = fontData.GetCharRanges();
                if (charRanges.empty())
                    charRanges = newCharRanges;
                assert(std::ranges::equal(charRanges, newCharRanges));
            }
            this->ranges.AddRanges(charRanges);
        }

        float GetPerfectFontSize(GW::Constants::InterfaceSize size) const
        {
            auto sizeIndex = std::clamp((int32_t)size, 0, (int32_t)perfectSizes.size() - 1);
            return perfectSizes[sizeIndex];
        }

        GW::Constants::InterfaceSize GetInterfaceSize(float fontSize) const
        {
            GW::Constants::InterfaceSize interfaceSize = GW::Constants::InterfaceSize::Small;
            for (int32_t i = perfectSizes.size() - 1; i >= 0; --i)
            {
                if (perfectSizes[i] <= fontSize)
                {
                    interfaceSize = (GW::Constants::InterfaceSize)i;
                    break;
                }
            }
            return interfaceSize;
        }
    };

    static bool SrcInit(ImFontAtlas *atlas, ImFontConfig *src)
    {
        assert(!src->FontLoaderData);
        auto loaderData = new LoaderData(*atlas, *src);
        src->FontLoaderData = loaderData;

        return true;
    }

    static void SrcDestroy(ImFontAtlas *atlas, ImFontConfig *src)
    {
        auto &fontData = *(GWFontConfig *)src->FontData;
        auto loaderData = (LoaderData *)src->FontLoaderData;
        if (loaderData)
        {
            delete loaderData;
            src->FontLoaderData = nullptr;
        }
    }

    static bool SrcContainsGlyph(ImFontAtlas *atlas, ImFontConfig *src, ImWchar codepoint)
    {
        auto &loaderData = *(LoaderData *)src->FontLoaderData;
        if (loaderData.ranges.Contains(codepoint))
            return true;
        if (GWIconBlitter::charRange.Contains(codepoint))
            return true;
        return false;
    }

    struct BakedData
    {
        GW::TextMgr::FontHandle fontHandle;
        GlyphBlitter glyphBlitter;

        BakedData(ImFontAtlas &atlas, GWFontConfig &cfg, LoaderData &loader, ImFontBaked &baked)
            : fontHandle(*GetGWFontDataForSize(
                  loader.language,
                  cfg.fontIndex,
                  baked.Size * baked.RasterizerDensity
              )),
              glyphBlitter(
                  atlas,
                  GWGlyphBlitter(
                      *fontHandle.FontPtr(),
                      GWGlyphBlitter::StyleKey{
                          cfg.color,
                          cfg.BlitFlags(),
                      }
                  )
              )
        {
        }
    };

    static bool BakedInit(ImFontAtlas *atlas, ImFontConfig *src, ImFontBaked *baked, void *loader_data_for_baked_src)
    {
        auto &fontData = *(GWFontConfig *)src->FontData;
        auto &loaderData = *(LoaderData *)src->FontLoaderData;

        auto bakedData = new (loader_data_for_baked_src) BakedData(*atlas, fontData, loaderData, *baked);

        auto font = bakedData->fontHandle.FontPtr();
        if (font == nullptr)
        {
            bakedData->~BakedData();
            return false;
        }

        return true;
    }

    static void BakedDestroy(ImFontAtlas *atlas, ImFontConfig *src, ImFontBaked *baked, void *loader_data_for_baked_src)
    {
        auto bakedData = (BakedData *)loader_data_for_baked_src;
        bakedData->~BakedData();
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
        auto &loaderData = *(LoaderData *)src->FontLoaderData;
        auto &bakedData = *(BakedData *)loader_data_for_baked_src;

        const auto blitPadding = GetBlitPadding(cfg.BlitFlags());
        auto &fontHandle = bakedData.fontHandle;
        auto &font = *fontHandle.FontPtr();
        auto desiredHeight = baked->Size * baked->RasterizerDensity - FontLineSpacing;
        auto scale = desiredHeight / (float)font.glyphHeight;

        ImFontGlyph &g = *out_glyph;
        g.Codepoint = codepoint;

        const bool isIcon = GWIconBlitter::charRange.Contains(codepoint);

        ImFontAtlasRect atlasRect;
        float advance;
        ImVec2 glyphPos;
        ImVec2 glyphSize;
        if (!isIcon)
        {
            g.PackId = bakedData.glyphBlitter.GetBlittedGlyph(codepoint, atlasRect);
            bool hasTexture = g.PackId != -1;
            uint32_t width;
            if (hasTexture)
            {
                width = atlasRect.w - 2 * blitPadding + font.base_advance;
                glyphPos.x = -(float)blitPadding * scale;
                glyphPos.y = -(float)blitPadding * scale;
                glyphSize.x = (float)atlasRect.w * scale;
                glyphSize.y = (float)atlasRect.h * scale;
            }
            else
            {
                width = font.fallback_advance;
            }

            g.Visible = hasTexture;
            g.Colored = cfg.color != 0xffffffff;
            advance = std::ceil((float)width * scale);
        }
        else // isIcon
        {
            g.PackId = loaderData.iconBlitter.GetBlittedGlyph(codepoint, atlasRect);

            float y_offset = ((desiredHeight * 0.75f) - (float)atlasRect.h) / 2.f; // 0.75f is to skip the descender lane height when aligning icons

            glyphPos.x = 0.f;
            glyphPos.y = y_offset;
            glyphSize.x = (float)atlasRect.w;
            glyphSize.y = (float)atlasRect.h;

            g.Visible = true;
            g.Colored = true;
            advance = atlasRect.w;
        }

        g.X0 = glyphPos.x;
        g.Y0 = glyphPos.y + FontLineSpacing / 2.f;
        g.X1 = g.X0 + glyphSize.x;
        g.Y1 = g.Y0 + glyphSize.y;
        g.AdvanceX = advance;

        if (out_advance_x)
        {
            *out_advance_x = g.AdvanceX;
        }

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
        FontBakedSrcLoaderDataSize = sizeof(BakedData);
    }
};
GwFontLoader g_GwFontLoader;

ImFont *CreateGWFont(GWFontConfig gwcfg)
{
    ImGuiIO &io = ImGui::GetIO();

    ImFontConfig imcfg;
    auto *fontData = new GWFontConfig(std::move(gwcfg));
    imcfg.FontData = fontData;
    imcfg.FontDataSize = sizeof(*fontData);

    imcfg.FontLoader = &g_GwFontLoader;

    ImFont *imFont = io.Fonts->AddFont(&imcfg);

    return imFont;
}

ImFont *HerosInsight::ImGuiCustom::GetOrCreateGWFont(GW::TextMgr::EngFont fontId, GW::TextMgr::BlitFontFlags blitFlags)
{
    static std::unordered_map<GWFontConfig::Raw, ImFont *> fonts;

    GWFontConfig gwcfg{fontId, blitFlags};

    auto it = fonts.find(gwcfg.raw);
    if (it != fonts.end())
        return it->second;

    auto *font = CreateGWFont(gwcfg);
    fonts[gwcfg.raw] = font;

    return font;
}

ImFontConfig *GetGWFontConfig(ImFont *font)
{
    auto src = font->Sources[0];
    assert(src->FontLoader == &g_GwFontLoader);
    return src;
}

void HerosInsight::ImGuiCustom::PushFontSize(GW::Constants::InterfaceSize size)
{
    auto currentFont = ImGui::GetFont();
    auto src = GetGWFontConfig(currentFont);
    auto &loaderData = *(GwFontLoader::LoaderData *)src->FontLoaderData;
    auto fontSize = loaderData.GetPerfectFontSize(size);
    ImGui::PushFont(NULL, fontSize);
}

GW::Constants::InterfaceSize GetCurrentFontInterfaceSize()
{
    auto currentFont = ImGui::GetFont();
    auto src = GetGWFontConfig(currentFont);
    auto &loaderData = *(GwFontLoader::LoaderData *)src->FontLoaderData;
    auto currentBakedFont = ImGui::GetFontBaked();
    auto currentFontSize = currentBakedFont->Size;
    return loaderData.GetInterfaceSize(currentFontSize);
}

void HerosInsight::ImGuiCustom::PushFontSize(int32_t interfaceSizeChange)
{
    auto interfaceSize = GetCurrentFontInterfaceSize();
    auto newInterfaceSize = (GW::Constants::InterfaceSize)((int32_t)interfaceSize + interfaceSizeChange);
    PushFontSize(newInterfaceSize);
}

void HerosInsight::ImGuiCustom::PushFontSizeDefault()
{
    auto pref = GW::UI::GetPreference(GW::UI::EnumPreference::InterfaceSize);
    PushFontSize((GW::Constants::InterfaceSize)pref);
}

ImFont *GetOrCreateChangedFont(GW::TextMgr::EngFont fontId)
{
    auto currentFont = ImGui::GetFont();
    auto src = GetGWFontConfig(currentFont);
    auto cfg = *(GWFontConfig *)src->FontData;
    return HerosInsight::ImGuiCustom::GetOrCreateGWFont(fontId, cfg.BlitFlags());
}

ImFont *GetOrCreateChangedFont(GW::TextMgr::BlitFontFlags blitFlags)
{
    auto currentFont = ImGui::GetFont();
    auto src = GetGWFontConfig(currentFont);
    auto cfg = *(GWFontConfig *)src->FontData;
    return HerosInsight::ImGuiCustom::GetOrCreateGWFont((GW::TextMgr::EngFont)cfg.fontIndex, blitFlags);
}

// Pushes font and ensures InterfaceSize is preserved.
void HerosInsight::ImGuiCustom::PushFont(ImFont *font)
{
    auto src = GetGWFontConfig(font);
    auto &loaderData = *(GwFontLoader::LoaderData *)src->FontLoaderData;
    auto interfaceSize = GetCurrentFontInterfaceSize();
    auto fontSize = loaderData.GetPerfectFontSize(interfaceSize);
    ImGui::PushFont(font, fontSize);
}

void HerosInsight::ImGuiCustom::PushFont(GW::TextMgr::EngFont fontId)
{
    auto font = GetOrCreateChangedFont(fontId);
    PushFont(font);
}

void HerosInsight::ImGuiCustom::PushFontFlags(GW::TextMgr::BlitFontFlags blitFlags)
{
    auto font = GetOrCreateChangedFont(blitFlags);
    ImGui::PushFont(font, 0.0f); // 0.0f = use current font size
}

void InitFonts()
{
    // First font is used by default
    Constants::Fonts::default_font = HerosInsight::ImGuiCustom::GetOrCreateGWFont(
        GW::TextMgr::EngFont::Default
    );
    Constants::Fonts::button_font = HerosInsight::ImGuiCustom::GetOrCreateGWFont(
        GW::TextMgr::EngFont::Default,
        GW::TextMgr::BlitFontFlags::AmbientOcclusion
    );
    Constants::Fonts::window_name_font = HerosInsight::ImGuiCustom::GetOrCreateGWFont(
        GW::TextMgr::EngFont::Thick,
        GW::TextMgr::BlitFontFlags::AmbientOcclusion
    );
    Constants::Fonts::skill_thick_font = HerosInsight::ImGuiCustom::GetOrCreateGWFont(
        GW::TextMgr::EngFont::Thick
    );
    Constants::Fonts::skill_name_font = HerosInsight::ImGuiCustom::GetOrCreateGWFont(
        GW::TextMgr::EngFont::DefaultL
    );
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

void HerosInsight::ImGuiCustom::RefreshStyle()
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

void Roundify(ImGuiStyle &style, float radius)
{
    style.TabRounding = radius;
    style.GrabRounding = radius;
    style.ChildRounding = radius;
    style.FrameRounding = radius;
    style.ImageRounding = radius;
    style.PopupRounding = radius;
    style.WindowRounding = radius;
    style.ScrollbarRounding = radius;
    style.TreeLinesRounding = radius;
    style.DragDropTargetRounding = radius;
}

void TweakStyle()
{
    auto &style = ImGui::GetStyle();

    // style.WindowBorderSize = 1.f;
    style.WindowBorderHoverPadding = 8.f;
    style.ScrollbarSize = 20.f;
    Roundify(style, 4.f);
}

#ifdef _DEBUG
void DebugGWFontData()
{
    FixedVector<GW::TextMgr::FontHandle, 100> fontHandles;

    struct FontRelatedData
    {
        GW::TextMgr::FontHandle &handle;
        GW::TextMgr::GrFont &font;
    };

    std::vector<std::vector<FontRelatedData>> fontsView;

    auto fontDataArrays = GW::TextMgr::GetFontDataArrays(GW::Constants::Language::English);
    for (auto &fontDataArray : fontDataArrays)
    {
        auto &vec = fontsView.emplace_back();
        for (GW::TextMgr::FontData &fontData : fontDataArray)
        {
            auto &handle = fontHandles.emplace_back(fontData);
            vec.emplace_back(handle, *handle.FontPtr());
        }
    }
    __debugbreak();
}
#endif

void HerosInsight::ImGuiCustom::Init()
{
#ifdef _DEBUG
    // DebugGWFontData();
#endif

    SaveCurrentStyleColors(themes[Settings::Style::Theme::ImGuiDefault].colors);
    CreateGWTheme();

    auto &io = ImGui::GetIO();

    TweakStyle();
    RefreshStyle();

    InitFonts();

    static std::string imgui_ini_path = (Constants::paths.cache() / "imgui.ini").string();
    static std::string imgui_log_path = (Constants::paths.cache() / "imgui_log.txt").string();
    io.IniFilename = imgui_ini_path.c_str();
    io.LogFilename = imgui_log_path.c_str();
}
