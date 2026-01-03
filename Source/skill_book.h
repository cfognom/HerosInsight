#pragma once

namespace HerosInsight::SkillBook
{
    void Initialize();
    void Terminate();
    void Enable();
    void Disable();
    void Update();
    void Draw(IDirect3DDevice9 *device);
}
