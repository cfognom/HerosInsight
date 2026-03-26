#pragma once

namespace HerosInsight::ImGuiCustomize
{
    void PushGWFont(ImFont *font, GW::Constants::InterfaceSize size);
    void PushGWFont(ImFont *font);

    void RefreshStyle();
    void Init();
}