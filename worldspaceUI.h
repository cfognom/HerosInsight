#pragma once

#include <d3d9.h>

namespace HerosInsight::WorldSpaceUI
{
    void PushSkillForDraw(uint32_t agent_id, GW::Constants::SkillID skill_id, float healing, DWORD timestamp_begin, uint32_t duration_ms);
    void Update();
    void Draw(IDirect3DDevice9 *device);
    void Reset();
}