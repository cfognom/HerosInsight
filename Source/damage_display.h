#pragma once

struct IDirect3DDevice9;

namespace HerosInsight::DamageDisplay
{
    inline bool enabled = false;
    void Draw(IDirect3DDevice9 *device);
}