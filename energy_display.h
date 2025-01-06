#pragma once

struct IDirect3DDevice9;

namespace HerosInsight::EnergyDisplay
{
    void Initialize();
    void Terminate();
    void Update();
    void Draw(IDirect3DDevice9 *device);
}