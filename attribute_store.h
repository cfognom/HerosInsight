#pragma once

#include <attribute_or_title.h>
#include <fixed_array.h>

namespace HerosInsight
{
    struct AttributeData
    {
        AttributeOrTitle id = 0xFF;
        uint8_t level = 0;
    };

    struct AttributeStore
    {
        FixedArray<AttributeData, 12> attributes = {};

        AttributeStore() = default;
        AttributeStore(std::span<GW::Attribute> attrs);

        void SetAttribute(AttributeOrTitle id, uint8_t level);
        bool RemoveAttribute(AttributeOrTitle id);
        std::optional<uint8_t> GetAttribute(AttributeOrTitle id) const;
    };
}