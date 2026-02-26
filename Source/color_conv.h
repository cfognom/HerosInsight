#pragma once

#include <imgui.h>

namespace ColorConv
{
    ImVec4 RGBAToHSVA(ImVec4 rgba);
    ImVec4 HSVAToRGBA(ImVec4 hsva);

    ImVec4 HSVAToHSLA(ImVec4 hsva);
    ImVec4 HSLAToHSVA(ImVec4 hsla);
}