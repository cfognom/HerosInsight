#pragma once

#include <attribute_or_title.h>
#include <update_manager.h>
#include <utils.h>

namespace HerosInsight
{
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

    enum struct SkillParamType
    {
        Null,

        Level,
        Heal,
        Damage,
        EnergyDiscount,
        DamageReduction,

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
        PERCENT_START,

        ChanceToBlock,
        ChanceToFail,
        ChanceToMiss,

        MAY_BE_NEGATIVE_AFTER, // ------------------

        MovementSpeedMod,
        DurationMod,
        AttackTimeMod,
        RechargeTimeMod,
        HealMod,
        DamageMod,

        PERCENT_END,

        ActivationTimeAdd,
        RechargeTimeAdd,

        SECONDS_END,

        ArmorChange,
        MaxHealthAdd,
        HealthPips,
        EnergyPips,

        COUNT,
    };

    GW::Constants::SkillID SkillParamTypeToSkillID(SkillParamType type);
    std::string_view SkillParamTypeToString(SkillParamType type);

    struct SkillParam
    {
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

        uint32_t Resolve(uint32_t attribute_lvl) const
        {
            return IsConstant() ? val0 : Utils::LinearAttributeScale(val0, val15, attribute_lvl);
        }

        int Print(FixedArrayRef<char> buffer, int32_t attribute_lvl, bool show_pos_sign = false) const
        {
            if (IsConstant())
            {
                return buffer.PushFormat("%u", val0);
            }
            else if (attribute_lvl != -1)
            {
                return buffer.PushFormat("%u", Utils::LinearAttributeScale(val0, val15, attribute_lvl));
            }
            else
            {
                return buffer.PushFormat("(%u...%u)", val0, val15);
            }
        }

        void ImGuiRender(int32_t attribute_lvl)
        {
            FixedArray<char, 32> salloc;
            auto buffer = salloc.ref();
            auto len = Print(buffer, attribute_lvl);

            if (!IsConstant())
                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dynamic_green);

            ImGui::TextUnformatted(buffer.data(), buffer.data() + len);

            if (!IsConstant())
                ImGui::PopStyleColor();
        }
    };

    SkillParam GetSkillParam(const GW::Skill &skill, uint32_t id);

    struct ParsedSkillParam
    {
        SkillParamType type;
        std::optional<DamageType> damage_type;
        SkillParam param;
        bool is_negative;

        bool IsCondition() const
        {
            return type >= SkillParamType::CONDITION_START && type < SkillParamType::CONDITION_END;
        }
        void ImGuiRender(int8_t attr_lvl);
    };

    struct CustomSkillData;

    struct DescKey
    {
        DescKey()
        {
            std::memset(this, 0, sizeof(*this));
        }

        bool is_singular0 : 1;
        bool is_singular1 : 1;
        bool is_singular2 : 1;
        bool is_concise : 1;
        bool is_valid : 1;
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

    enum struct DescWord;

    struct MachineDesc
    {
        MachineDesc() = default;
        MachineDesc(std::string_view desc);

        std::vector<DescWord> words;
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

    enum struct SkillTag
    {
        Archived,

        Equipable,
        Unlocked,
        Locked,
        Temporary,
        DeveloperSkill,
        EnvironmentSkill,
        MonsterSkill,
        SpiritAttack,

        PvEOnly,
        PvPOnly,
        PvEVersion,
        PvPVersion,
        Consumable,
        EffectOnly,
        Maintained,
        ConditionSource,
        ExploitsCorpse,
        Celestial,
        Mission,
        Bundle,

        CONTEXT,
        CONTEXT_END = CONTEXT + static_cast<uint32_t>(Utils::SkillContext::Count),
    };

    std::string_view SkillTagToString(SkillTag tag);

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
        Pet,
    };

    struct StaticSkillEffect
    {
        EffectMask mask;
        EffectLocation location;
        Utils::Range radius;
        GW::Constants::SkillID skill_id;
        SkillParam duration;

        bool IsAffected(uint32_t caster_id, uint32_t target_id, uint32_t candidate_agent_id) const;
        void Apply(uint32_t caster_id, uint32_t target_id, uint8_t attr_lvl) const;
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
        FixedArray<SkillEffect, 8> carried_effects;

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

    struct CustomSkillData
    {
        GW::Constants::SkillID skill_id;
        GW::Skill *skill;

        DescKey last_desc_key;
        DescKey last_concise_key;

        SkillTags tags;
        Utils::SkillContext context;
        AttributeOrTitle attribute;
        Renewal renewal;
        FixedArray<ParsedSkillParam, 8> parsed_params;
        std::optional<int16_t> end_effect_index; // index in "params" of where "End Effect:" was found in the description
        SkillParam base_duration;
        std::vector<StaticSkillEffect> init_effects;
        std::vector<StaticSkillEffect> end_effects;

        void Init();

        std::string *TryGetName();
        DescKey GetDescKey(bool is_concise, int32_t attribute_lvl) const;
        std::string *TryGetPredecodedDescription(DescKey key);
        HerosInsight::Utils::RichString *TryGetDescription(bool is_concise = false, int32_t attribute_lvl = -1);

        SkillParam GetSkillParam(uint32_t id) const;
        SkillParam GetParsedSkillParam(std::function<bool(const ParsedSkillParam &)> predicate) const;
        void GetParsedSkillParams(SkillParamType type, FixedArrayRef<ParsedSkillParam> result) const;
        void GetInitConditions(uint8_t attr_lvl, FixedArrayRef<SkillEffect> result) const;
        void GetEndConditions(uint8_t attr_lvl, FixedArrayRef<SkillEffect> result) const;
        std::span<const ParsedSkillParam> GetInitParsedParams() const;
        std::span<const ParsedSkillParam> GetEndParsedParams() const;

        void GetOnExpireEffects(CustomAgentData &caster, FixedArrayRef<SkillEffect> result) const;

        std::string ToString() const;
        std::string_view GetTypeString();
        std::string_view GetProfessionString();
        std::string_view GetCampaignString();
        std::string_view GetAttributeString();

        bool IsFlash() const;
        bool IsAttack() const;
        bool IsRangedAttack() const;

        void OnSkillActivation(CustomAgentData &caster, uint32_t target_id);
        void OnProjectileLaunched(CustomAgentData &caster, uint32_t target_id, OwnedProjectile &projectile);

        uint8_t GetOvercast() const;
        uint32_t GetAdrenaline() const;
        uint32_t GetAdrenalineStrikes() const;
        uint8_t GetEnergy() const;
        uint8_t GetSacrifice() const;
        uint32_t GetRecharge() const;
        float GetActivation() const;
        float GetAftercast() const;
        Utils::Range GetAoE() const;
        void GetRanges(FixedArrayRef<Utils::Range> out) const;
        uint32_t ResolveBaseDuration(CustomAgentData &custom_ad, std::optional<uint8_t> skill_attr_lvl_override = std::nullopt) const;
        void GetTags(FixedArrayRef<SkillTag> out) const;

    private:
        std::string_view type_str;
        std::string_view profession_str;
        std::string_view campaign_str;
        std::string_view attr_str;
        std::string_view avail_str;
        std::string name;
        Utils::RichString last_desc;
        Utils::RichString last_concise;
        std::string predecoded_descriptions[16];
        Utils::InitGuard name_guard;
        Utils::InitGuard desc_guard;
    };

    namespace CustomSkillDataModule
    {
        extern bool is_initialized;
        bool TryInitialize();

        CustomSkillData &GetCustomSkillData(GW::Constants::SkillID skill_id);
    }
}