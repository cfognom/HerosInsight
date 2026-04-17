#pragma once

#include <GWCA/Managers/SkillbarMgr.h>

#include <custom_agent_data.h>
#include <filtering.h>

namespace HerosInsight::SkillFiltering
{
    // The order they are listed in here is also the order the properties are filtered by.
    // This order has big impacts in the filtering speed.
    // The filtering device works by trying to confirm if an item matches the filter, if so it can discard it for further processing.
    // It therefore makes sense to try to put properties which are likely to match first,
    // and also similar properties far away from each other. Since if, for example, no matches were found in the description,
    // it is unlikely any will be found in the concise description.
    enum struct SkillProp : uint16_t
    {
        Description,
        Type,
        Attribute,
        Profession,
        Campaign,
        Tag,
        AoE,
        Name,

        Energy,
        Recharge,
        Activation,
        Adrenaline,
        Sacrifice,
        Overcast,
        Aftercast,
        Upkeep,

        Concise,
        // Parsed,
        Id,

        COUNT,
    };

    constexpr size_t PROP_COUNT = static_cast<size_t>(SkillProp::COUNT);

    struct AttributeSource
    {
        enum struct Type : int
        {
            ZeroToFifteen,
            FromAgent,
            Manual,
        };

        AttributeSource() : type(Type::ZeroToFifteen), value(-1) {}

        Type type;
        size_t value;

        int8_t GetAttrLvl(AttributeOrTitle id) const;
    };

    struct Adapter
    {
        struct Settings
        {
            bool use_exact_adrenaline;
            uint32_t focused_agent_id;
            AttributeSource attr_src;

            void FocusAgent(uint32_t agent_id)
            {
                focused_agent_id = agent_id;
                if (attr_src.type == AttributeSource::Type::FromAgent)
                    attr_src.value = agent_id;
            }
        };

        std::unordered_map<SkillProp, Filtering::IncrementalProp> dynamic_props;
        std::array<Filtering::IncrementalProp *, PROP_COUNT> props;

        void RefreshDynamicProps()
        {
            for (auto &prop : dynamic_props)
            {
                prop.second.MarkDirty();
            }
        }

        explicit Adapter(Settings &settings);

        using index_type = uint16_t;
        constexpr static size_t MaxSpanCount() { return GW::Constants::SkillMax; }
        constexpr static size_t PropCount() { return PROP_COUNT; }
        size_t MetaCount() const;

        LoweredText GetMetaName(size_t meta);
        BitView GetMetaPropset(size_t meta) const;

        Filtering::IncrementalProp *GetProperty(size_t prop) { return props[prop]; }
    };

    using Device = Filtering::Device<Adapter>;

    std::span<uint16_t> GetBaseSkills();
}