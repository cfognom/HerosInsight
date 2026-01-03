#pragma once

#include <GWCA/GWCA.h>
#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

namespace HerosInsight
{
    struct AttributeOrTitle
    {
        uint8_t value;

        constexpr static uint8_t AttrCount = (uint8_t)GW::Constants::AttributeByte::Mysticism + 1;
        constexpr static uint8_t Count = AttrCount + (uint8_t)GW::Constants::TitleID::Codex;

        constexpr AttributeOrTitle(uint8_t v) : value(v) {}
        constexpr AttributeOrTitle() : value(0xFF) {}

        AttributeOrTitle(GW::Skill &skill);
        AttributeOrTitle(GW::Constants::Attribute attr);
        AttributeOrTitle(GW::Constants::AttributeByte attr);
        AttributeOrTitle(GW::Constants::TitleID t);

        bool IsNone() const
        {
            return value == 0xFF;
        }

        bool IsAttribute() const
        {
            return value < AttrCount;
        }

        bool IsTitle() const
        {
            return !IsAttribute() && !IsNone();
        }
        // Implicit conversion to GW::Constants::AttributeByte
        operator GW::Constants::AttributeByte() const
        {
            return IsAttribute() ? (GW::Constants::AttributeByte)value : GW::Constants::AttributeByte::None;
        }

        // Implicit conversion to GW::Constants::TitleID
        operator GW::Constants::TitleID() const
        {
            return IsTitle() ? (GW::Constants::TitleID)(value - AttrCount) : GW::Constants::TitleID::None;
        }

        GW::Constants::AttributeByte GetAttribute() const
        {
            return IsAttribute() ? (GW::Constants::AttributeByte)value : GW::Constants::AttributeByte::None;
        }

        GW::Constants::TitleID GetTitle() const
        {
            return IsTitle() ? (GW::Constants::TitleID)(value - AttrCount) : GW::Constants::TitleID::None;
        }

        // Equality operator ==
        bool operator==(const AttributeOrTitle other) const
        {
            return value == other.value;
        }

        // Inequality operator !=
        bool operator!=(const AttributeOrTitle other) const
        {
            return !(*this == other);
        }

        // Equality operator ==
        bool operator==(const GW::Constants::AttributeByte other) const
        {
            return GetAttribute() == other;
        }

        // Inequality operator !=
        bool operator!=(const GW::Constants::AttributeByte other) const
        {
            return !(*this == other);
        }

        // Equality operator ==
        bool operator==(const GW::Constants::TitleID other) const
        {
            return GetTitle() == other;
        }

        // Inequality operator !=
        bool operator!=(const GW::Constants::TitleID other) const
        {
            return !(*this == other);
        }

        std::string_view GetStr() const;
    };
}