#include <utils.h>

#include "attribute_or_title.h"

namespace HerosInsight
{
    AttributeOrTitle::AttributeOrTitle(GW::Skill &skill)
    {
        bool has_attribute = skill.attribute != 51;
        bool has_title = skill.title != 48;
        assert(!(has_attribute && has_title));
        if (has_attribute)
            value = (uint8_t)skill.attribute;
        else if (has_title)
            value = AttrCount + (uint8_t)skill.title;
        else
            value = 0xFF;
    }

    AttributeOrTitle::AttributeOrTitle(GW::Constants::Attribute attr)
    {
        value = attr == 51 ? 0xFF : (uint8_t)attr;
    }

    AttributeOrTitle::AttributeOrTitle(GW::Constants::AttributeByte attr)
    {
        value = attr == 51 ? 0xFF : (uint8_t)attr;
    }

    AttributeOrTitle::AttributeOrTitle(GW::Constants::TitleID t)
    {
        value = t == 48 ? 0xFF : AttrCount + (uint8_t)t;
    }

    std::string_view AttributeOrTitle::GetStr() const
    {
        if (IsAttribute())
            return Utils::GetAttributeString(GetAttribute());
        else
            return Utils::GetTitleString(GetTitle());
    }
}