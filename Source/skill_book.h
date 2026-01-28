#pragma once

namespace HerosInsight::SkillBook
{
    bool IsDragging(); // True when we are doing a skill drag operation.
    void Initialize();
    void Terminate();
    void Enable();
    void Disable();
    void Update();
    void Draw(IDirect3DDevice9 *device);
}
