#pragma once

#include <GWCA/Constants/Constants.h>
#include <GWCA/Managers/TextMgr.h>

namespace Constants::Fonts
{
    inline ImFont *default_font;
    inline ImFont *button_font;
    inline ImFont *window_name_font;
    inline ImFont *skill_name_font;
    inline ImFont *skill_thick_font;
}

namespace HerosInsight::ImGuiCustomize
{
    ImFont *GetOrCreateGWFont(
        GW::TextMgr::EngFont fontId = GW::TextMgr::EngFont::Default,
        GW::TextMgr::BlitFontFlags blitFlags = GW::TextMgr::BlitFontFlags::None
    );
    void PushFontSize(GW::Constants::InterfaceSize size);
    void PushFontSize(int32_t interfaceSizeChange);
    void PushFontSizeDefault();
    void PushFont(ImFont *font);
    void PushFont(GW::TextMgr::EngFont fontId);
    void PushFontFlags(GW::TextMgr::BlitFontFlags blitFlags);

    void RefreshStyle();
    void Init();
}