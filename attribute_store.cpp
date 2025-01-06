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

        auto datas = attributes.ref();
        for (auto &data : datas)
        {
            if (data.id == id)
            {
                data.level = level;
                return;
            }
        }
        if (datas.try_push({id, level}))
            return;

        SOFT_ASSERT(false, L"AttributeStore::SetAttribute: No space for new attribute");
    }

    bool AttributeStore::RemoveAttribute(AttributeOrTitle id)
    {
        auto datas = attributes.ref();
        for (size_t i = 0; i < datas.size(); i++)
        {
            if (datas[i].id == id)
            {
                datas.remove(i);
                return true;
            }
        }
        return false;
    }

    std::optional<uint8_t> AttributeStore::GetAttribute(AttributeOrTitle id) const
    {
        auto datas = attributes.ref();
        for (const auto &data : datas)
        {
            if (data.id == id)
                return {data.level};
        }
        return std::nullopt;
    }
}