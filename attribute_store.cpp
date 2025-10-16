#include <GWCA/GameEntities/Attribute.h>

#include "attribute_store.h"
#include <utils.h>

namespace HerosInsight
{
    AttributeStore::AttributeStore(std::span<GW::Attribute> attrs)
    {
        for (const auto &attr : attrs)
        {
            if (attr.level != 0)
            {
                SetAttribute(AttributeOrTitle(attr.id), attr.level);
            }
        }
    }

    void AttributeStore::SetAttribute(AttributeOrTitle id, uint8_t level)
    {
        if (id.IsNone())
            return;

        for (auto &data : attributes)
        {
            if (data.id == id)
            {
                data.level = level;
                return;
            }
        }
        if (attributes.try_push(AttributeData{id, level}))
            return;

        SOFT_ASSERT(false, L"AttributeStore::SetAttribute: No space for new attribute");
    }

    bool AttributeStore::RemoveAttribute(AttributeOrTitle id)
    {
        for (size_t i = 0; i < attributes.size(); i++)
        {
            if (attributes[i].id == id)
            {
                attributes.remove(i);
                return true;
            }
        }
        return false;
    }

    std::optional<uint8_t> AttributeStore::GetAttribute(AttributeOrTitle id) const
    {
        for (const auto &data : attributes)
        {
            if (data.id == id)
                return {data.level};
        }
        return std::nullopt;
    }
}