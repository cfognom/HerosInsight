#pragma once

namespace HerosInsight::Debug
{
    void SetHoveredSkill(GW::Constants::SkillID skill_id);
    void Initialize();
    void Terminate();
    void Update();
}
