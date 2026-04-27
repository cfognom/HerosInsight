#include <GWCA/Managers/SkillbarMgr.h>

#include <buffer.h>
#include <custom_skill_data.h>
#include <profiling.h>
#include <text_provider.h>

#include "skill_filtering.h"

namespace HerosInsight::SkillFiltering
{
    using Propset = BitArray<SkillFiltering::PROP_COUNT + 1>; // +1 for meta props
    constexpr static Propset ALL_PROPS = Propset(true);

    void SortSkills(std::span<uint16_t> skills)
    {
        ProfilingScope profiler;

        struct Comparer
        {
            std::span<GW::Skill> skills = GW::SkillbarMgr::GetSkills();
            std::span<CustomSkillData> cskills = CustomSkillDataModule::GetSkills();
            Text::Provider &text_provider = Text::GetTextProvider(GW::Constants::Language::English);

            bool operator()(uint16_t a, uint16_t b) const
            {
                std::strong_ordering cmp;

                auto &a_skill = skills[a];
                auto &b_skill = skills[b];
                // If the skill is a pvp version, refer to the pve version
                auto a_pve_id = a_skill.IsPvP() ? (uint16_t)a_skill.skill_id_pvp : a;
                auto b_pve_id = b_skill.IsPvP() ? (uint16_t)b_skill.skill_id_pvp : b;
                auto &a_pve_skill = skills[a_pve_id];
                auto &b_pve_skill = skills[b_pve_id];
                if (a_pve_id == b_pve_id)
                {
                    cmp = a_skill.IsPvP() <=> b_skill.IsPvP(); // Put pve version before pvp version
                    if (cmp != 0)
                        return cmp < 0;
                }
                else
                {
                    auto &a_pve_cskill = cskills[a_pve_id];
                    auto &b_pve_cskill = cskills[b_pve_id];

                    auto ca = a_pve_cskill.context;
                    auto cb = b_pve_cskill.context;
                    cmp = ca <=> cb;
                    if (cmp != 0)
                        return cmp < 0;

                    auto pa = (uint32_t)a_pve_skill.profession - 1; // We do this so that None comes last
                    auto pb = (uint32_t)b_pve_skill.profession - 1;
                    cmp = pa <=> pb;
                    if (cmp != 0)
                        return cmp < 0;

                    auto attr_a = a_pve_cskill.attribute.value;
                    auto attr_b = b_pve_cskill.attribute.value;
                    cmp = attr_a <=> attr_b;
                    if (cmp != 0)
                        return cmp < 0;

                    // Skills with neither profession nor attribute/title are sorted by type and campaign
                    if (a_pve_skill.profession == GW::Constants::ProfessionByte::None &&
                        a_pve_cskill.attribute.IsNone())
                    {
                        assert(b_pve_skill.profession == GW::Constants::ProfessionByte::None);
                        assert(b_pve_cskill.attribute.IsNone());

                        auto ty_a = a_pve_cskill.GetTypeString();
                        auto ty_b = b_pve_cskill.GetTypeString();
                        cmp = ty_a <=> ty_b;
                        if (cmp != 0)
                            return cmp < 0;

                        auto ca = a_pve_skill.campaign;
                        auto cb = b_pve_skill.campaign;
                        cmp = ca <=> cb;
                        if (cmp != 0)
                            return cmp < 0;
                    }

                    auto n_a = text_provider.GetName((GW::Constants::SkillID)a_pve_id);
                    auto n_b = text_provider.GetName((GW::Constants::SkillID)b_pve_id);
                    cmp = n_a <=> n_b;
                    if (cmp != 0)
                        return cmp < 0;
                }

                return a < b;
            }
        };
        std::sort(skills.begin(), skills.end(), Comparer{});
    }

    void InitBaseSkills()
    {
        using element_t = std::ranges::range_value_t<decltype(base_skills)>;
        element_t next_skill_id = 1;
        for (auto &skill_id : base_skills)
        {
            skill_id = next_skill_id++;
        }
        SortSkills(base_skills);
    }

    struct TextStorage
    {
        std::array<Filtering::IncrementalProp, PROP_COUNT> static_props;
        std::vector<Propset> meta_propsets;
        LoweredTextVector meta_prop_names;

        template <auto Func, auto Unit, auto Icon>
        static Text::StringTemplateAtom NumberAndIcon(Text::StringTemplateAtom::Builder &b, size_t skill_id, void *)
        {
            auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
            auto value = (cskill.*Func)();
            if (value == 0)
                return {};

            FixedVector<Text::StringTemplateAtom, 4> args;

            if (value < 0)
            {
                value = -value;
                args.push_back(b.Char('-'));
            }

            args.push_back(b.MixedNumber(value));
            if constexpr (Unit != nullptr)
            {
                args.push_back(b.Chars(*Unit));
            }
            args.push_back(b.ExplicitString(*Icon));

            return b.ExplicitSequence(args);
        }

        template <auto Func>
        static void GetSkillProp(SpanVector<char> &dst, size_t skill_id)
        {
            auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
            dst.Elements().append_range((cskill.*Func)());
        }

        void InitProps()
        {
            static std::string_view Percent = "%";

            static_props[(size_t)SkillProp::Energy].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetEnergy, nullptr, &RichText::Icons::EnergyOrb>);
            static_props[(size_t)SkillProp::Recharge].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetRecharge, nullptr, &RichText::Icons::Recharge>);

            static_props[(size_t)SkillProp::Upkeep].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetUpkeep, nullptr, &RichText::Icons::Upkeep>);
            static_props[(size_t)SkillProp::Overcast].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetOvercast, nullptr, &RichText::Icons::Overcast>);
            static_props[(size_t)SkillProp::Sacrifice].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetSacrifice, &Percent, &RichText::Icons::Sacrifice>);
            static_props[(size_t)SkillProp::Activation].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetActivation, nullptr, &RichText::Icons::Activation>);

            static_props[(size_t)SkillProp::Name].SetupIncremental(
                nullptr,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *) -> Text::StringTemplateAtom
                {
                    auto &text_provider = Text::GetTextProvider(GW::Constants::Language::English);
                    return text_provider.MakeSkillName(b, (GW::Constants::SkillID)skill_id);
                }
            );

            static_props[(size_t)SkillProp::Type].PopulateItems("SkillBookProp_Type", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetTypeString>);
            static_props[(size_t)SkillProp::Attribute].PopulateItems("SkillBookProp_Attribute", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetAttributeStr>);
            static_props[(size_t)SkillProp::Profession].PopulateItems("SkillBookProp_Profession", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetProfessionStr>);
            static_props[(size_t)SkillProp::Campaign].PopulateItems("SkillBookProp_Campaign", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetCampaignStr>);
            static_props[(size_t)SkillProp::Id].SetupIncremental(
                nullptr,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *) -> Text::StringTemplateAtom
                {
                    return b.Number((float)skill_id);
                }
            );

            static_props[(size_t)SkillProp::Aftercast].SetupIncremental(
                nullptr,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *) -> Text::StringTemplateAtom
                {
                    auto &skill = GW::SkillbarMgr::GetSkills()[skill_id];
                    const bool is_normal_aftercast = (skill.activation > 0 && skill.aftercast == 0.75f) ||
                                                     (skill.activation == 0 && skill.aftercast == 0);
                    if (skill.aftercast == 0.f && is_normal_aftercast)
                        return {};

                    FixedVector<Text::StringTemplateAtom, 5> number_and_icon;

                    if (!is_normal_aftercast)
                    {
                        number_and_icon.push_back(b.Tag(RichText::ColorTag(IM_COL32(255, 255, 120, 255))));
                    }
                    number_and_icon.push_back(b.MixedNumber(skill.aftercast));
                    if (!is_normal_aftercast)
                    {
                        number_and_icon.push_back(b.Char('*'));
                        number_and_icon.push_back(b.Tag(RichText::ColorTag(NULL)));
                    }
                    number_and_icon.push_back(b.ExplicitString(RichText::Icons::Aftercast));

                    return b.ExplicitSequence(number_and_icon);
                }
            );

            static_props[(size_t)SkillProp::AoE].SetupIncremental(
                nullptr,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *) -> Text::StringTemplateAtom
                {
                    FixedVector<Utils::Range, 4> ranges;
                    auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
                    cskill.GetRanges(ranges);
                    FixedVector<Text::StringTemplateAtom, 32> range_atoms;
                    for (size_t i = 0; i < ranges.size(); ++i)
                    {
                        auto range = ranges[i];
                        range_atoms.push_back(b.Number((float)(std::underlying_type_t<Utils::Range>)range));
                        auto range_name = Utils::GetRangeStr(range);
                        if (range_name.has_value())
                        {
                            range_atoms.push_back(b.Chars(" ("));
                            range_atoms.push_back(b.ExplicitString(range_name.value()));
                            range_atoms.push_back(b.Char(')'));
                        }

                        if (i < ranges.size() - 1)
                            range_atoms.push_back(b.Chars(", "));
                    }
                    return b.ExplicitSequence(range_atoms);
                }
            );
        }

        template <typename... Ts>
        static Propset CreatePropset(Ts... args)
        {
            Propset propset;
            ((propset[static_cast<size_t>(args)] = true), ...);
            return propset;
        }

        void SetupMetaProps()
        {
            auto SetupMetaProp = [&](std::string_view name, Propset propset)
            {
                meta_prop_names.arena.push_back(name);
                meta_propsets.push_back(propset);
            };

            std::string_view capacity_hint_key = "meta_prop_names";
            meta_prop_names.arena.ReserveFromHint(capacity_hint_key);
            meta_propsets.reserve(meta_prop_names.arena.SpanCount());

            SetupMetaProp("", ALL_PROPS); // Must be first
            SetupMetaProp("Meta", CreatePropset(SkillProp::COUNT));
            SetupMetaProp("Name", CreatePropset(SkillProp::Name));
            SetupMetaProp("Type", CreatePropset(SkillProp::Type));
            SetupMetaProp("Tags", CreatePropset(SkillProp::Tag));
            SetupMetaProp("Energy", CreatePropset(SkillProp::Energy));
            SetupMetaProp("Recharge", CreatePropset(SkillProp::Recharge));
            SetupMetaProp("Activation", CreatePropset(SkillProp::Activation));
            SetupMetaProp("Aftercast", CreatePropset(SkillProp::Aftercast));
            SetupMetaProp("Sacrifice", CreatePropset(SkillProp::Sacrifice));
            SetupMetaProp("Overcast", CreatePropset(SkillProp::Overcast));
            SetupMetaProp("Adrenaline", CreatePropset(SkillProp::Adrenaline));
            SetupMetaProp("Upkeep", CreatePropset(SkillProp::Upkeep));
            SetupMetaProp("Full Description", CreatePropset(SkillProp::Description));
            SetupMetaProp("Concise Description", CreatePropset(SkillProp::Concise));
            SetupMetaProp("Description", CreatePropset(SkillProp::Description, SkillProp::Concise));
            SetupMetaProp("Attribute", CreatePropset(SkillProp::Attribute));
            SetupMetaProp("Profession", CreatePropset(SkillProp::Profession));
            SetupMetaProp("Campaign", CreatePropset(SkillProp::Campaign));
            SetupMetaProp("AoE", CreatePropset(SkillProp::AoE));

            SetupMetaProp("Id", CreatePropset(SkillProp::Id));

            meta_prop_names.LowercaseFold();
            meta_prop_names.arena.StoreCapacityHint(capacity_hint_key);
        }

        bool is_initialized = false;
        void TryInitialize()
        {
            if (is_initialized)
                return;
            InitBaseSkills();
            SetupMetaProps();
            InitProps();
            is_initialized = true;
        }
    } text_storage;

    template <bool IsConcise>
    Text::StringTemplateAtom MakeDescription(Text::StringTemplateAtom::Builder &b, size_t skill_id, void *data)
    {
        auto &settings = *(Adapter::Settings *)data;
        auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
        auto &text_provider = Text::GetTextProvider(GW::Constants::Language::English);
        auto attr_rank = settings.attr_src.GetAttrLvl(cskill.attribute);
        return text_provider.MakeSkillDescription(b, (GW::Constants::SkillID)skill_id, IsConcise, attr_rank);
    }

    Adapter::Adapter(Adapter::Settings &settings)
    {
        text_storage.TryInitialize();

        dynamic_props.reserve(8);
        dynamic_props[SkillProp::Description].SetupIncremental(&settings, MakeDescription<false>);
        dynamic_props[SkillProp::Concise].SetupIncremental(&settings, MakeDescription<true>);
        dynamic_props[SkillProp::Adrenaline].SetupIncremental(
            &settings,
            +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *data) -> Text::StringTemplateAtom
            {
                auto &settings = *(Adapter::Settings *)data;
                auto &skill = GW::SkillbarMgr::GetSkills()[skill_id];
                auto adrenaline = skill.adrenaline;
                if (adrenaline == 0)
                    return {};

                auto adrenaline_strikes = adrenaline / 25;
                auto adrenaline_units = adrenaline % 25;

                if (!settings.use_exact_adrenaline)
                {
                    if (adrenaline_units > 0)
                    {
                        adrenaline_strikes += 1;
                        adrenaline_units = 0;
                    }
                }

                return b.ExplicitSequence(
                    {b.MixedNumber(adrenaline_strikes, adrenaline_units, 25),
                     b.ExplicitString(RichText::Icons::Adrenaline)}
                );
            }
        );
        dynamic_props[SkillProp::Tag].SetupIncremental(
            &settings,
            +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *data) -> Text::StringTemplateAtom
            {
                auto &settings = *(Adapter::Settings *)data;
                auto skills = GW::SkillbarMgr::GetSkills();
                auto cskills = CustomSkillDataModule::GetSkills();
                auto &skill = skills[skill_id];
                auto &cskill = cskills[skill_id];
                auto skill_id_to_check = skill.IsPvP() ? skill.skill_id_pvp : skill.skill_id;
                auto &pve_skill = skills[(uint32_t)skill_id_to_check];
                auto &pve_cskill = cskills[(uint32_t)skill_id_to_check];

                auto &tags = cskill.tags;
                bool is_unlocked = GW::SkillbarMgr::GetIsSkillUnlocked(skill_id_to_check);
                bool is_learned = Utils::IsSkillLearned(pve_skill, settings.focused_agent_id);
                bool is_equipable = Utils::IsSkillEquipable(skill, settings.focused_agent_id);
                bool is_locked = tags.Unlockable && !is_unlocked;
                bool is_learnable = Utils::IsSkillLearnable(pve_cskill, settings.focused_agent_id);
                bool is_unlearned = is_learnable && !is_learned;

                FixedVector<Text::StringTemplateAtom, 16> args;

                auto PushTag = [&](std::string_view str, ImU32 color = NULL, std::string_view icon = {})
                {
                    if (color != NULL)
                        args.push_back(b.Tag(RichText::ColorTag(color)));

                    args.push_back(b.ExplicitString(str));

                    if (color != NULL)
                        args.push_back(b.Tag(RichText::ColorTag(NULL)));

                    if (!icon.empty())
                        args.push_back(b.ExplicitString(icon));

                    args.push_back(b.ExplicitString(", "));
                };

                constexpr auto soft_red = IM_COL32(255, 100, 100, 255);
                constexpr auto soft_green = IM_COL32(143, 255, 143, 255);

                // clang-format off
                    if (tags.PvEOnly)          PushTag("PvE-only");
                    if (is_unlocked)           PushTag("Unlocked" , soft_green);
                    if (is_locked)             PushTag("Locked"   , soft_red);
                    if (is_learned)            PushTag("Learned"  , soft_green);
                    if (is_unlearned)          PushTag("Unlearned", soft_red);
                    if (tags.Temporary)        PushTag("Temporary");
                    if (is_equipable)          PushTag("Equipable", IM_COL32(100, 255, 255, 255));
                    
                    if (cskill.context != Utils::SkillContext::Null)
                        PushTag(Utils::GetSkillContextString(cskill.context));

                    if (tags.Archived)         PushTag("Archived");
                    if (tags.EffectOnly)       PushTag("Effect-only");
                    if (tags.PvPOnly)          PushTag("PvP-only");
                    if (tags.PvEVersion)       PushTag("PvE Version");
                    if (tags.PvPVersion)       PushTag("PvP Version");
                    if (tags.Bounty)           PushTag("Bounty");

                    if (tags.DeveloperSkill)   PushTag("Developer Skill");
                    if (tags.EnvironmentSkill) PushTag("Environment Skill");
                    if (tags.MonsterSkill)     PushTag("Monster Skill", NULL, RichText::Icons::MonsterSkull);
                    if (tags.SpiritAttack)     PushTag("Spirit Attack");
                    
                    if (tags.Maintained)       PushTag("Maintained");
                    if (tags.ConditionSource)  PushTag("Condition Source");
                    if (tags.ExploitsCorpse)   PushTag("Exploits Corpse");
                    if (tags.Consumable)       PushTag("Consumable");
                    if (tags.Celestial)        PushTag("Celestial");
                    if (tags.Mission)          PushTag("Mission");
                    if (tags.Bundle)           PushTag("Bundle");
                // clang-format on

                if (!args.empty())
                {
                    // Cut off the last ", "
                    args.pop_back();
                }

                return b.ExplicitSequence(args);
            }
        );

        for (size_t i = 0; i < PROP_COUNT; ++i)
        {
            auto prop_id = (SkillProp)i;
            auto it = dynamic_props.find(prop_id);
            if (it != dynamic_props.end())
            {
                props[i] = &it->second;
            }
            else
            {
                props[i] = &text_storage.static_props[i];
            }
        }
    }

    size_t Adapter::MetaCount() const
    {
        return text_storage.meta_propsets.size();
    }

    LoweredText Adapter::GetMetaName(size_t meta)
    {
        return text_storage.meta_prop_names.Get(meta);
    }

    BitView Adapter::GetMetaPropset(size_t meta) const
    {
        return text_storage.meta_propsets[meta];
    }

    int8_t SkillFiltering::AttributeSource::GetAttrLvl(AttributeOrTitle id) const
    {
        switch (this->type)
        {
            case Type::FromAgent:
            {
                auto agent_id = this->value;
                auto custom_agent_data = CustomAgentDataModule::GetCustomAgentData(agent_id);
                return custom_agent_data.GetOrEstimateAttribute(id);
            }
            default:
            case Type::ZeroToFifteen:
            case Type::Manual:
            {
                return (int8_t)this->value;
            }
        }
    }
}
