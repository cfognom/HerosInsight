#pragma once

#include <texture_module.h>
#include <utils.h>

namespace HerosInsight
{
    namespace TextImageProvider
    {
        namespace Id
        {
            enum
            {
                MonsterSkull,
                EnergyOrb,

                COUNT,
            };
        }

        void Draw(ImVec2 pos, size_t id);
        float GetWidth(size_t id);

        Utils::ImageDrawerFns GetImpl();
    }
}