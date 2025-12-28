#pragma once

#include <attribute_or_title.h>

namespace HerosInsight
{
    struct AttributeStore
    {
        uint16_t data[32]; // Each uint16_t has 3 attribute values (We only need 62 bytes, so the last 2 bytes are unused)

        AttributeStore() : data(-1) {}
        AttributeStore(uint8_t init_level)
        {
#ifdef _DEBUG
            assert(init_level < 64);
#endif
            uint16_t filler = init_level | (init_level << 5) | (init_level << 10);
            std::fill(data, data + std::size(data), filler);
        }
        AttributeStore(std::span<GW::Attribute> attrs)
        {
            for (const auto &attr : attrs)
            {
                if (attr.level != 0)
                {
                    SetAttribute(AttributeOrTitle(attr.id), attr.level);
                }
            }
        }

        void SetAttribute(AttributeOrTitle id, uint8_t level)
        {
#ifdef _DEBUG
            assert(level < 64);
#endif
            auto word_index = id.value / 3;
            auto inner_index = id.value % 3;
            auto mask = 0x1F << (inner_index * 5);
            auto value = level << (inner_index * 5);
            data[word_index] = (data[word_index] & ~mask) | value;
        }
        std::optional<uint8_t> GetAttribute(AttributeOrTitle id) const
        {
            auto word_index = id.value / 3;
            auto inner_index = id.value % 3;
            auto value = (data[word_index] >> (inner_index * 5)) & 0x1F;
            if (value == 0x1F)
                return std::nullopt;
            return value;
        }
    };
}