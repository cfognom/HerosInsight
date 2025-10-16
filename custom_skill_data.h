#pragma once

#include <variant>

#include <attribute_or_title.h>
#include <skill_text_provider.h>
#include <update_manager.h>

namespace HerosInsight
{
    struct HighlightData
    {
        std::vector<uint16_t> offsets;
        std::span<uint16_t> header_offsets;
        std::span<uint16_t> value_offsets;

        void clear()
        {
            offsets.clear();
            header_offsets = {};
            value_offsets = {};
        }
    };

    struct SkillRegenType
    {
        static const uint32_t None = 0;
        static const uint32_t NoRequirement = 1 << 0;
        static const uint32_t PerCondition = 1 << 1;
        static const uint32_t PerHex = 1 << 2;
        static const uint32_t PerEnchantment = 1 << 3;
    };

    enum struct Renewal
    {
        None = 0,
        WhenChantOrShoutEnds = 1 << 0,
        WhenEnchantmentEnds = 1 << 1,
    };

    enum struct DamageType : uint32_t
    {
        Blunt = 0,
        Piercing = 1,
        Slashing = 2,
        Cold = 3,
        Lightning = 4,
        Fire = 5,
        Dark = 7,
        Earth = 11,
        Shadow,
        Chaos,
        Holy,
        Elemental,
        Physical,

        None = 0xFF,
    };

    struct SkillParam
    {
        SkillParam(uint32_t val0, uint32_t val15) : val0(val0), val15(val15) {}
        SkillParam(uint32_t const_val) : val0(const_val), val15(const_val) {}
        SkillParam() : val0(0), val15(0) {}

        uint32_t val0;
        uint32_t val15;

        bool IsConstant() const
        {
            return val0 == val15;
        }

        bool IsNull() const
        {
            return val0 == 0 && val15 == 0;
        }

        explicit operator bool() const
        {
            return !IsNull();
        }

        bool operator==(const SkillParam &other) const
        {
            return val0 == other.val0 && val15 == other.val15;
        }

        bool IsSingular(int32_t attribute_lvl) const
        {
            return 1 == (attribute_lvl == -1 ? val15 : Resolve(attribute_lvl));
        }

        // Computes the value at a given attribute level
        uint32_t Resolve(uint32_t attribute_lvl) const
        {
            return IsConstant() ? val0 : Utils::LinearAttributeScale(val0, val15, attribute_lvl);
        }

        void Print(int32_t attribute_lvl, std::span<char> &out) const
        {
            auto dst = out.data();
            auto dst_size = out.size();
            auto dst_end = dst + dst_size;
            if (IsConstant() || attribute_lvl != -1)
            {
                uint32_t value = Resolve(attribute_lvl);
                auto result = std::to_chars(dst, dst_end, value, 10);
                assert(result.ec == std::errc());
                out = std::span<char>(dst, result.ptr - dst);
            }
            else
            {
                auto result = std::format_to_n(dst, dst_size, "({}...{})", val0, val15);
                assert(result.out - dst == result.size);
                out = std::span<char>(dst, result.out - dst);
            }
        }

        void ImGuiRender(int32_t attribute_lvl)
        {
            char buffer[32];
            std::span<char> view = buffer;
            Print(attribute_lvl, view);

            if (!IsConstant())
                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dynamic_green);

            ImGui::TextUnformatted(view.data(), view.data() + view.size());

            if (!IsConstant())
                ImGui::PopStyleColor();
        }

        static bool TryRead(std::string_view &remaining, SkillParam &out)
        {
            auto rem = remaining;
            bool is_opening = Utils::TryRead('(', rem);
            bool is_number = Utils::TryReadInt(rem, *(int32_t *)&out.val0);
            if (!is_opening)
            {
                if (is_number)
                {
                    out.val15 = out.val0;
                    remaining = rem;
                    return true;
                }
                return false;
            }
            if (Utils::TryRead("...", rem) &&
                Utils::TryReadInt(rem, *(int32_t *)&out.val15) &&
                Utils::TryRead(')', rem))
            {
                remaining = rem;
                return true;
            }
            return false;
        }
    };

    SkillParam GetSkillParam(const GW::Skill &skill, uint32_t id);

    enum struct RemovalMask
    {
        Null = 0,

        Bleeding = 1 << 0,
        Blind = 1 << 1,
        Burning = 1 << 2,
        Crippled = 1 << 3,
        Deep_Wound = 1 << 4,
        Disease = 1 << 5,
        Poison = 1 << 6,
        Dazed = 1 << 7,
        Weakness = 1 << 8,

        CrackedArmor = 1 << 9,

        Hex = 1 << 10,
        Enchantment = 1 << 11,
        Condition = Bleeding | Blind | Burning | Crippled | Deep_Wound | Disease | Poison | Dazed | Weakness | CrackedArmor,
    };

    struct ParsedSkillData
    {
        enum struct Type
        {
            Null,

            InitialEffect,
            EndEffect,
            DropEffect,

            Level,
            Heal,
            Damage,
            EnergyDiscount,

            ConditionsRemoved,
            HexesRemoved,
            EnchantmentsRemoved,

            HealthGain,
            HealthLoss,
            HealthSteal,

            EnergyGain,
            EnergyLoss,
            EnergySteal,

            AdrenalineGain,
            AdrenalineLoss,

            SECONDS_START,

            Duration,
            Disable,

            CONDITION_START,

            Bleeding,
            Blind,
            Burning,
            CrackedArmor,
            Crippled,
            Dazed,
            DeepWound,
            Disease,
            Poison,
            Weakness,

            CONDITION_END,
            SECONDS_END,

            ChanceToBlock,
            ChanceToFail,
            ChanceToMiss,

            MAY_BE_NEGATIVE_AFTER, // ------------------

            FasterMovement,
            SlowerMovement,
            LongerDuration,
            ShorterDuration,
            FasterAttacks,
            SlowerAttacks,
            FasterRecharge,
            SlowerRecharge,
            FasterActivation,
            SlowerActivation,
            MoreHealing,
            LessHealing,

            AdditionalHealth, // Additional Max Health, there is no "Reduced" Max Health variant

            DISPLAY_AS_POSITIVE_START,

            MoreDamage,
            ArmorIncrease,
            HealthRegen,
            EnergyRegen,

            DISPLAY_AS_POSITIVE_END,
            DISPLAY_AS_NEGATIVE_START,

            LessDamage,
            ArmorDecrease,
            HealthDegen,
            EnergyDegen,

            DISPLAY_AS_NEGATIVE_END,

            COUNT,
        };

        Type type;
        std::optional<DamageType> damage_type;
        SkillParam param;
        bool is_percent;
        bool is_negative;

        bool IsCondition() const
        {
            return type >= Type::CONDITION_START && type < Type::CONDITION_END;
        }
        bool IsRemoval() const
        {
            return type >= Type::ConditionsRemoved && type <= Type::EnchantmentsRemoved;
        }
        void ImGuiRender(int8_t attr_lvl, float width, std::span<uint16_t> hl);

        GW::Constants::SkillID GetCondition() const;
        RemovalMask GetRemovalMask() const;
        std::string_view ToStr() const;
    };

    struct SkillPropertyID
    {
        static constexpr uint32_t MAX_TAGS = 16;
        enum struct Type
        {
            None,
            Unknown,

            // String
            TEXT,

            Name,
            SkillType,
            TAGS,
            TAG_END = TAGS + MAX_TAGS,
            Description,
            Concise,
            Attribute,
            Profession,
            Campaign,

            TEXT_END,

            // Number
            NUMBER,

            Overcast,
            Adrenaline,
            Sacrifice,
            Upkeep,
            Energy,
            Activation,
            Recharge,
            Aftercast,
            Range,
            ID,

            // Raw struct data
            RAW,

            u8,
            u16,
            u32,
            f32,

            RAW_END,

            PARSED,
            PARSED_END = PARSED + (uint32_t)ParsedSkillData::Type::COUNT,

            NUMBER_END,
            MAX,
        };

        Type type;
        uint8_t byte_offset;

        bool IsStringType() const
        {
            return type >= Type::TEXT && type < Type::TEXT_END;
        }

        bool IsNumberType() const
        {
            return type >= Type::NUMBER && type < Type::NUMBER_END;
        }

        bool IsRawNumberType() const
        {
            return type >= Type::RAW && type < Type::RAW_END;
        }

        std::string_view ToStr() const
        {
            if (type >= Type::PARSED && type < Type::PARSED_END)
            {
                ParsedSkillData temp = {};
                temp.type = static_cast<ParsedSkillData::Type>((uint32_t)type - (uint32_t)Type::PARSED);
                return temp.ToStr();
            }

            if (IsRawNumberType())
            {
                // clang-format off
                switch (type)
                {
                    case Type::u8:  return "Raw u8";
                    case Type::u16: return "Raw u16";
                    case Type::u32: return "Raw u32";
                    case Type::f32: return "Raw f32";
                    default:        return "...";
                }
                // clang-format on
            }

            // clang-format off
            switch (type) {
                case Type::TEXT:         return "Text";
                case Type::Name:         return "Name";
                case Type::SkillType:    return "Type";
                case Type::TAGS:         return "Tag";
                case Type::Description:  return "Description";
                case Type::Concise:      return "Concise Description";
                case Type::Attribute:    return "Attribute";
                case Type::Profession:   return "Profession";
                case Type::Campaign:     return "Campaign";

                case Type::Overcast:     return "Overcast";
                case Type::Adrenaline:   return "Adrenaline";
                case Type::Sacrifice:    return "Sacrifice";
                case Type::Upkeep:       return "Upkeep";
                case Type::Energy:       return "Energy";
                case Type::Activation:   return "Activation";
                case Type::Recharge:     return "Recharge";
                case Type::Aftercast:    return "Aftercast";
                case Type::Range:        return "Range";
                case Type::ID:           return "ID";

                default:                 return "...";
            }
            // clang-format on
        }
    };

    constexpr SkillPropertyID::Type ParsedToProp(ParsedSkillData::Type type)
    {
        return static_cast<SkillPropertyID::Type>((uint32_t)SkillPropertyID::Type::PARSED + (uint32_t)type);
    }

    struct CustomSkillData;

    enum struct DescToken;

    struct TokenizedDesc
    {
        TokenizedDesc() = default;
        TokenizedDesc(std::string_view desc);

        std::vector<DescToken> tokens;
        std::vector<SkillParam> lits;
    };

    struct SkillTags
    {
        bool Archived : 1;

        bool EffectOnly : 1;
        bool Unlockable : 1;
        bool Temporary : 1;
        bool MonsterSkill : 1;
        bool EnvironmentSkill : 1;
        bool DeveloperSkill : 1;
        bool SpiritAttack : 1;

        bool PvEOnly : 1;
        bool PvPOnly : 1;
        bool PvEVersion : 1;
        bool PvPVersion : 1;
        bool Maintained : 1;
        bool Consumable : 1;
        bool Celestial : 1;
        bool Mission : 1;
        bool Bundle : 1;

        bool Spell : 1;
        bool ConditionSource : 1;
        bool ExploitsCorpse : 1;
        bool EndsOnIncDamage : 1;
        bool Projectile : 1;
        bool HitBased : 1; // Skills whose effects are applied on hit rather than on activation.
    };
    static_assert(sizeof(SkillTags) <= 8);

    enum struct EffectMask
    {
        None,

        Caster = 1 << 0,
        Target = 1 << 1,
        CastersPet = 1 << 2,
        PartyPets = 1 << 3,
        OtherPartyMembers = 1 << 4,
        MinionAllies = 1 << 5,
        SpiritAllies = 1 << 6,
        NonSpiritAllies = 1 << 7,
        OtherFoes = 1 << 8,

        CasterAndPet = Caster | CastersPet,
        PartyMembers = Caster | OtherPartyMembers,
        Allies = NonSpiritAllies | SpiritAllies,
        Foes = Target | OtherFoes,
    };

    enum struct EffectLocation
    {
        Null,

        Caster,
        Target,
        AllyClosestToTarget,
        SpiritAllyClosestToTarget,
        MinionAllyClosestToTarget,
        Pet,
    };

    struct StaticSkillEffect
    {
        EffectMask mask;
        EffectLocation location;
        Utils::Range radius;
        std::variant<GW::Constants::SkillID, RemovalMask> skill_id_or_removal;
        SkillParam duration_or_count;

        bool IsAffected(uint32_t caster_id, uint32_t target_id, uint32_t candidate_agent_id) const;
        void Apply(uint32_t caster_id, uint32_t target_id, uint8_t attr_lvl, std::function<bool(GW::AgentLiving &)> predicate = nullptr) const;
        std::wstring ToWString() const;
    };

    struct SkillEffect
    {
        GW::Constants::SkillID effect_skill_id;
        GW::Constants::SkillID source_skill_id;
        uint32_t base_duration; // sec
    };

    struct OwnedProjectile
    {
        uint32_t owner_agent_id;
        uint32_t calculated_target_id; // The assumed target of the projectile
        uint32_t original_target_id;   // The targeted agent when the projectile was launched
        GW::Constants::SkillID skill_id;
        DWORD timestamp_impact;
        GW::Vec2f destination;
        uint32_t impact_frame;
        Buffer<SkillEffect, 8> carried_effects;

        bool HasExpired() const
        {
            return impact_frame && UpdateManager::frame_id > impact_frame;
        }

        bool IsValid() const
        {
            return owner_agent_id && !HasExpired();
        }
    };

    SkillParam GetPoisonDuration(GW::Skill &skill);

    struct DescKey
    {
        DescKey(const GW::Skill &skill, bool is_concise, int8_t attribute_lvl)
        {
            is_singular0 = GetSkillParam(skill, 0).IsSingular(attribute_lvl);
            is_singular1 = GetSkillParam(skill, 1).IsSingular(attribute_lvl);
            is_singular2 = GetSkillParam(skill, 2).IsSingular(attribute_lvl);
            is_concise = is_concise;
            attribute_lvl = attribute_lvl;
        }

        bool is_singular0 : 1;
        bool is_singular1 : 1;
        bool is_singular2 : 1;
        bool is_concise : 1;
        int8_t attribute_lvl : 8;

        uint16_t value() const
        {
            return *(uint16_t *)(this);
        }

        uint16_t pre_key() const
        {
            return value() & 0xF;
        }

        bool operator==(const DescKey &other) const
        {
            return value() == other.value();
        }
    };
    static_assert(sizeof(DescKey) == sizeof(uint16_t));
    static_assert(std::is_trivially_copyable_v<DescKey>);

    struct CustomSkillData
    {
        GW::Constants::SkillID skill_id;
        GW::Skill *skill;

        SkillTags tags;
        Utils::SkillContext context;
        AttributeOrTitle attribute;
        Renewal renewal;
        SkillParam base_duration;
        std::vector<ParsedSkillData> parsed_data;
        std::vector<StaticSkillEffect> init_effects;
        std::vector<StaticSkillEffect> end_effects;
        std::vector<StaticSkillEffect> drop_effects;

        void Init();

        SkillParam GetSkillParam(uint32_t id) const;
        SkillParam GetParsedSkillParam(std::function<bool(const ParsedSkillData &)> predicate) const;
        void GetParsedSkillParams(ParsedSkillData::Type type, std::span<ParsedSkillData> &result) const;
        void GetInitConditions(uint8_t attr_lvl, std::span<SkillEffect> &result) const;
        void GetEndConditions(uint8_t attr_lvl, std::span<SkillEffect> &result) const;
        std::span<const ParsedSkillData> GetInitParsedData() const;
        std::span<const ParsedSkillData> GetEndParsedData() const;

        void GetOnExpireEffects(CustomAgentData &caster, std::span<SkillEffect> &result) const;

        std::string ToString() const;
        std::string_view GetTypeString();
        std::string_view GetProfessionStr();
        std::string_view GetCampaignStr();
        std::string_view GetAttributeStr();

        bool IsFlash() const;
        bool IsAttack() const;
        bool IsRangedAttack() const;

        void OnSkillActivation(CustomAgentData &caster, uint32_t target_id);
        void OnProjectileLaunched(CustomAgentData &caster, uint32_t target_id, OwnedProjectile &projectile);

        int8_t GetUpkeep() const;
        uint8_t GetOvercast() const;
        uint32_t GetAdrenaline() const;
        uint32_t GetAdrenalineStrikes() const;
        uint8_t GetEnergy() const;
        uint8_t GetSacrifice() const;
        uint32_t GetRecharge() const;
        float GetActivation() const;
        float GetAftercast() const;
        Utils::Range GetAoE() const;
        void GetRanges(std::span<Utils::Range> &out) const;
        uint32_t ResolveBaseDuration(CustomAgentData &custom_ad, std::optional<uint8_t> skill_attr_lvl_override = std::nullopt) const;

    private:
        std::string_view type_str;
        std::string_view profession_str;
        std::string_view campaign_str;
        std::string_view attr_str;
        std::string_view avail_str;
    };

    namespace CustomSkillDataModule
    {
        void Initialize();

        CustomSkillData &GetCustomSkillData(GW::Constants::SkillID skill_id);
    }
}