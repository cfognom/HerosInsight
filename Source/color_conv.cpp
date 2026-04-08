#include "color_conv.h"

ImVec4 ColorConv::RGBAToHSVA(ImVec4 rgba)
{
    float r = rgba.x;
    float g = rgba.y;
    float b = rgba.z;
    float a = rgba.w;
    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(r, g, b, h, s, v);
    return ImVec4(h, s, v, a);
}

ImVec4 ColorConv::HSVAToRGBA(ImVec4 hsva)
{
    float h = hsva.x;
    float s = hsva.y;
    float v = hsva.z;
    float a = hsva.w;
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
    return ImVec4(r, g, b, a);
}

ImVec4 ColorConv::HSVAToHSLA(ImVec4 hsva)
{
    float h = hsva.x;
    float s = hsva.y;
    float v = hsva.z;
    float a = hsva.w;
    float l = v * (1.f - s * 0.5f);
    s = 0.f;
    if (0.f < l && l < 1.f)
    {
        s = (v - l) / std::min(l, 1.f - l);
    }
    return ImVec4(h, s, l, a);
}

ImVec4 ColorConv::HSLAToHSVA(ImVec4 hsla)
{
    float h = hsla.x;
    float s = hsla.y;
    float l = hsla.z;
    float a = hsla.w;
    float v = l + s * std::min(l, 1.f - l);
    s = 0.f;
    if (v > 0.f)
    {
        s = 2.f * (1.f - l / v);
    }
    return ImVec4(h, s, v, a);
}
