#pragma once

#include <bitset>
#include <format>
#include <optional>
#include <span>
#include <variant>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hook.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/Context/AgentContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <d3d9.h>
#include <d3dx9.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <async_lazy.h>
#include <constants.h>
#include <stack_string.h>
#include <texture_module.h>

namespace HerosInsight
{
    struct CustomAgentData;
    struct CustomSkillData;
}

namespace HerosInsight::Utils
{
// Checks if the condition is true, if not, writes a message to chat and returns
#define SOFT_ASSERT(condition, ...) HerosInsight::Utils::SoftAssert((condition), __FILEW__, __LINE__, __VA_ARGS__);

    void WriteMessageRaw(const wchar_t *message, GW::Chat::Color color = NULL);

    // Checks if the condition is true, if not, writes a formatted message to chat
    template <typename... Args>
    void SoftAssert(bool condition, const wchar_t *file, int line, const std::wstring &format_str = L"Condition is false", Args &&...args)
    {
        if (!condition)
        {
            std::wstring user_message = std::vformat(format_str, std::make_wformat_args(args...));
            std::wstring file_str(file);
            std::replace(file_str.begin(), file_str.end(), L'\\', L'/'); // Replace '\\' with '/'
            std::wstring message = std::format(L"ASSERT FAILED: {}. File: {}, Line: {}", user_message, file_str, line);
            WriteMessageRaw(message.c_str(), 0xFFFF0000);
        }
    }

    template <size_t N>
    struct BitsetHelpers
    {
        constexpr static size_t SIZE = sizeof(std::bitset<N>);
        constexpr static size_t WORD_SIZE = sizeof(size_t);
        constexpr static size_t WORD_COUNT = SIZE / WORD_SIZE;
        constexpr static size_t WORD_BITS = WORD_SIZE * 8;
        static_assert(alignof(std::bitset<N>) >= alignof(size_t));
    };

    template <size_t N>
    size_t CountTrailingZeros(const std::bitset<N> &mask)
    {
        auto words = (const size_t *)&mask;
        size_t count = 0;
        for (size_t i = 0; i < BitsetHelpers<N>::WORD_COUNT; ++i)
        {
            auto word = words[i];
            if (word == 0)
            {
                count += BitsetHelpers<N>::WORD_BITS;
            }
            else
            {
                count += std::countr_zero(word);
                break;
            }
        }
        return std::min(count, N);
    }

    template <size_t N>
    void ClearLowestSetBit(std::bitset<N> &mask)
    {
        auto words = (size_t *)&mask;
        for (size_t i = 0; i < BitsetHelpers<N>::WORD_COUNT; ++i)
        {
            auto &word = words[i];
            if (word > 0)
            {
                word &= word - 1;
                break;
            }
        }
    }

    template <size_t N>
    struct BitsetIterator
    {
        size_t index;
        void Next()
        {
            if (word_copy == 0)
            {
                while (true)
                {
                    ++word_index;
                    if (word_index == BitsetHelpers<N>::WORD_COUNT)
                        break;
                    static_assert(alignof(std::bitset<N>) >= alignof(size_t));
                    auto words = (const size_t *)&bitset; // We assume bitset is an array of size_t
                    word_copy = words[word_index];
                    if (word_copy != 0)
                        break;
                }
            }
            auto trailing_zeros = std::countr_zero(word_copy);
            index = std::min(word_index * BitsetHelpers<N>::WORD_BITS + trailing_zeros, N);
            if (word_copy != 0)
            {
                word_copy &= word_copy - 1; // Clear lowest set bit
            }
        }
        bool IsDone() const { return index == N; }
        BitsetIterator(std::bitset<N> &bitset)
            : bitset(bitset)
        {
            Next();
        }

    private:
        size_t word_index = -1;
        size_t word_copy = 0;
        std::bitset<N> &bitset;
    };

    enum struct SkillTargetType : uint8_t
    {
        None = 0,
        ClosestToTarget = 1,
        Ally = 3,
        OtherAlly = 4,
        Foe = 5,
        PartyMember = 6, // Resurrection skills use this, for example
        MinionOrSpirit = 14,
        FoeAoE = 16,
    };

    enum struct SkillSpecialFlags : uint32_t
    {
        None = 0,
        Overcast = 1 << 0,
        Touch = 1 << 1,
        Elite = 1 << 2,
        HalfRange = 1 << 3,
        SignetOfCapture = 1 << 4,
        EasilyInterruptible = 1 << 5,
        FailureChance = 1 << 6,
        Minus40ArmorDuringActivation = 1 << 7,
        Unknown1 = 1 << 8, // Mostly traps/preparations/rituals
        Degen = 1 << 9,    // Any skill that causes degen has this flag
        SiegeAttack = 1 << 10,
        Resurrection = 1 << 11,   // Most skills that resurrects a player has this flag
        OathShot = 1 << 12,       // Only Oath Shot has this flag
        Condition = 1 << 13,      // Used for conditions, but also many passive effects has this flag.
        MonsterSkill = 1 << 14,   // Many monster skills has this flag
        DualAttack = 1 << 15,     // All dual attacks and "Brawling Combo Punch" has this flag
        IsStacking = 1 << 16,     // Runes etc.
        IsNonStacking = 1 << 17,  // Runes etc.
        ExploitsCorpse = 1 << 18, // Most skills that exploits a corpse has this flag
        PvESkill = 1 << 19,
        Effect = 1 << 20,            // They exist only as effects, not as skills
        NaturalResistance = 1 << 21, // The effect that causes conditions and hexes to last half as long. Used by boss monsters.
        PvP = 1 << 22,
        FlashEnchantment = 1 << 23,
        SummoningSickness = 1 << 24,
        IsUnplayable = 1 << 25,
        // Remaining bits are unused

        UnusualSkillFlags =
            SignetOfCapture |
            SiegeAttack |
            Condition |
            IsStacking |
            IsNonStacking |
            Effect |
            NaturalResistance |
            SummoningSickness |
            IsUnplayable,
    };

    std::string SkillSpecialToString(SkillSpecialFlags single_flag);

    enum struct Range
    {
        Null = 0,
        AdjacentToTarget = 72,
        Adjacent = 156,
        Nearby = 240,
        InTheArea = 312,
        Earshot = 1000,
        SpiritRange = 2500,
        CompassRange = 5000,
    };

    enum struct RangeSqrd
    {
        Null = 0,
        AdjacentToTarget = (uint32_t)Range::AdjacentToTarget * (uint32_t)Range::AdjacentToTarget,
        Adjacent = (uint32_t)Range::Adjacent * (uint32_t)Range::Adjacent,
        Nearby = (uint32_t)Range::Nearby * (uint32_t)Range::Nearby,
        InTheArea = (uint32_t)Range::InTheArea * (uint32_t)Range::InTheArea,
        Earshot = (uint32_t)Range::Earshot * (uint32_t)Range::Earshot,
        SpiritRange = (uint32_t)Range::SpiritRange * (uint32_t)Range::SpiritRange,
        CompassRange = (uint32_t)Range::CompassRange * (uint32_t)Range::CompassRange,
    };

    std::optional<std::string_view> GetRangeStr(Range range);

    bool IsSpace(char c);
    bool IsAlpha(char c);
    bool IsDigit(char c);
    bool IsAlphaNum(char c);
    size_t StrCountEqual(std::string_view a, std::string_view b);
    size_t StrCountEqual(std::wstring_view a, std::wstring_view b);
    std::string_view PopWord(std::string_view &str);
    bool StartsWith(const wchar_t *str, const wchar_t *prefix);
    bool TryReadSpaces(std::string_view &remaining);
    bool TryRead(const char c, std::string_view &remaining);
    bool TryRead(const wchar_t c, std::wstring_view &remaining);
    bool TryRead(const std::string_view str, std::string_view &remaining);
    bool TryRead(const std::wstring_view str, std::wstring_view &remaining);
    bool TryReadInt(std::string_view &remaining, int32_t &out);
    bool TryReadNumber(std::string_view &remaining, double &out);
    bool TryReadNumber(std::wstring_view &remaining, double &out);
    size_t TryReadPartial(const std::string_view str, std::string_view &remaining);
    bool TryReadAhead(std::string_view str, std::string_view &remaining);
    bool TryReadAhead(std::wstring_view str, std::wstring_view &remaining);
    bool TryFindAhead(std::string_view str, std::string_view &remaining);
    bool TryReadHex(std::string_view &remaining, uint32_t &out);
    bool TryReadHexColor(std::string_view &remaining, ImU32 &out);
    size_t ReadWhitespace(std::string_view &remaining);

    struct Unit
    {
        float value;
        std::string_view name;
    };
    inline Unit SIPrefix[]{
        {1000000000000, "T"},
        {1000000000, "G"},
        {1000000, "M"},
        {1000, "k"},
        {1, ""},
        {0.001f, "m"},
        {0.000001f, "μ"},
        {0.000000001f, "n"},
    };
    inline Unit Memory[]{
        {1024 * 1024 * 1024 * 1024, "TiB"},
        {1024 * 1024 * 1024, "GiB"},
        {1024 * 1024, "MiB"},
        {1024, "KiB"},
        {1, "B"},
    };
    std::string ToHumanReadable(float number, std::span<Unit> units = SIPrefix);
    std::string UInt32ToBinaryStr(uint32_t value);
    std::wstring StrToWStr(std::string_view str);
    std::string WStrToStr(const wchar_t *wstr, const wchar_t *end = nullptr);
    void StrToWStr(const char *str, std::span<wchar_t> &out);
    void WStrToStr(const wchar_t *wstr, std::span<char> &out);
    bool TryGetDecodedString(std::wstring_view enc_str, std::wstring_view &out);
    std::wstring DecodeString(const wchar_t *enc_str, std::chrono::microseconds timeout = std::chrono::microseconds(200));
    std::wstring StrIDToWStr(uint32_t id);
    std::string StrIDToStr(uint32_t id);

    template <typename... Args>
    void FormatToChat(GW::Chat::Color color, const std::wformat_string<Args...> &format_str, Args &&...args)
    {
        wchar_t buf[1024];
        auto result = std::format_to_n(buf, sizeof(buf) - 1, format_str, std::forward<Args>(args)...);
        *result.out = '\0';
        WriteMessageRaw(buf, color);
    }

    template <typename... Args>
    void FormatToChat(const std::wformat_string<Args...> &format_str, Args &&...args)
    {
        FormatToChat(NULL, format_str, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void FormatToChat(GW::Chat::Color color, const std::format_string<Args...> &format_str, Args &&...args)
    {
        static_assert(alignof(wchar_t) >= alignof(char));
        constexpr size_t n_chars = 1024;
        wchar_t wbuf[n_chars];
        char *buf = &((char *)&wbuf[n_chars])[-n_chars];
        auto result = std::format_to_n(buf, n_chars - 1, format_str, std::forward<Args>(args)...);
        *result.out = '\0';
        std::span<wchar_t> wstr(wbuf, n_chars - 1);
        Utils::StrToWStr(buf, wstr);
        wbuf[wstr.size()] = '\0';
        WriteMessageRaw(wbuf, color);
    }

    template <typename... Args>
    void FormatToChat(const std::format_string<Args...> &format_str, Args &&...args)
    {
        FormatToChat(NULL, format_str, std::forward<Args>(args)...);
    }

    uint32_t DivHalfToEven(uint32_t nom, uint32_t den);
    uint32_t RoundHalfToEven(double num);
    uint32_t RoundHalfToEven(float num);

    bool IsFoe(GW::AgentLiving *agent);
    std::wstring GetAgentName(uint32_t agent_id);
    std::wstring GetSkillName(GW::Constants::SkillID skill_id);
    std::wstring EffectToString(const GW::Effect *effect);
    std::wstring BuffToString(const GW::Buff *buff);

    GW::UI::ControlAction GetSkillControlAction(uint32_t hero_index, uint32_t slot_index);

    struct ChancyValue
    {
        float value;
        float half;
        uint32_t half_chance;
        float avg;
        uint32_t rounded;
        uint32_t half_rounded;
    };

    std::optional<ChancyValue> CalculateRecharge(uint32_t agent_id, GW::Constants::SkillID skill_id);
    std::optional<ChancyValue> CalculateCasttime(uint32_t agent_id, GW::Constants::SkillID skill_id);
    uint32_t CalculateDuration(GW::Skill &skill, uint32_t base_duration, uint32_t caster_id);
    uint8_t CalculateEnergyCost(uint32_t agent_id, GW::Constants::SkillID skill_id);
    struct Maintainability
    {
        float value;
        float avg_value;
        float avg_energy_per_sec;
        bool valid;
    };
    Maintainability CalculateMaintainability(GW::AgentLiving *agent, GW::Constants::SkillID skill_id);
    struct EffectHPPerSec
    {
        float caster_hp_sec;
        float target_hp_sec;
    };
    std::optional<EffectHPPerSec> CalculateEffectHPPerSec(GW::Constants::SkillID skill_id, uint32_t attribute_level);

    bool UseSkill(uint32_t hero_index, GW::Constants::SkillID skill_id, uint32_t target = NULL);
    bool UseSkill(uint32_t hero_index, uint32_t slot_index, uint32_t target = NULL);

    bool CanUseSkill(uint32_t hero_index, uint32_t slot_index);

    bool ToggleSkill(uint32_t hero_index, uint32_t slot_index);
    bool EnableSkill(uint32_t hero_index, uint32_t slot_index);
    bool DisableSkill(uint32_t hero_index, uint32_t slot_index);

    D3DVIEWPORT9 GetViewport();
    D3DXMATRIX GetViewMatrix();
    D3DXMATRIX GetProjectionMatrix();
    ImVec2 WorldSpaceToScreenSpace(GW::Vec3f world_pos);
    IDirect3DStateBlock9 *PrepareWorldSpaceRendering(IDirect3DDevice9 *device);
    IDirect3DStateBlock9 *PrepareStencilHoles(IDirect3DDevice9 *device, std::span<ImRect> holes);

    float DistanceSqrd(GW::Vec3f a, GW::Vec3f b);
    float Dot(GW::Vec2f a, GW::Vec2f b);
    float Dot(GW::Vec3f a, GW::Vec3f b);
    float Cross(GW::Vec2f a, GW::Vec2f b);
    GW::Vec2f Cross(GW::Vec2f a, float b);
    GW::Vec2f Cross(float a, GW::Vec2f b);
    GW::Vec2f Cross(GW::Vec3f a, GW::Vec3f b);
    bool TryNormalize(GW::Vec2f &a);
    bool TryNormalize(GW::Vec3f &a);

    float SecondsTilContact(uint32_t hero_index);
    float DistanceToPlayer(uint32_t agent_id);
    uint32_t GetClosestAgentID(GW::Vec2f pos);
    void ForEnemiesInCircle(GW::Vec2f center, float radius, GW::Constants::Allegiance allegiance, std::function<void(GW::AgentLiving &)> op);
    void ForAlliesInCircle(GW::Vec2f center, float radius, GW::Constants::Allegiance allegiance, std::function<void(GW::AgentLiving &)> op);
    void ForPartyMembersInCircle(GW::Vec2f center, float radius, uint32_t agent_id, std::function<void(GW::AgentLiving &)> op);
    void ForAgentsInCircle(GW::Vec2f center, float radius, std::function<void(GW::AgentLiving &)> op);

    GW::AgentLiving *GetHeroAsAgentLiving(uint32_t hero_index);

    GW::AgentInfoArray *GetAgentInfoArray();
    GW::AgentInfo *GetAgentInfoByID(uint32_t agent_id);
    GW::AgentMovementArray *GetAgentMovementArray();
    GW::AgentMovement *GetAgentMovementByID(uint32_t agent_id);
    GW::Array<GW::AgentSummaryInfo> *GetAgentSummaryInfoArray();
    GW::AgentSummaryInfo *GetAgentSummaryInfoByID(uint32_t agent_id);

    float SecondsTilFullEnergy(GW::AgentLiving *agent);
    float SecondsTilEnergyRecovered(GW::AgentLiving *agent, uint32_t energy);

    GW::Item *GetAgentItem(GW::AgentLiving &agent, uint32_t item_index);
    std::span<uint16_t> GetAgentWeaponAndOffhandItemIds(GW::AgentLiving &agent);
    std::span<GW::ItemModifier> GetItemModifiers(const GW::Item &item);
    enum struct ModifierID : uint32_t
    {
        Damage_Range = 42920,      // arg1 = 'max damage', arg2 = 'min damage'
        Damage_Range2 = 42120,     // arg1 = 'max damage', arg2 = 'min damage'
        DamageType = 9400,         // arg1 = 'damage type id'
        WeaponRequirement = 10136, // arg1 = 'attribute id', arg2 = 'requirement value'
        Customized = 42136,        // arg1 = 'damage multiplier in percent' (e.g. +20% = 120)

        EnergyBonus = 8920,   // Martial, arg2 = 'energy increase'
        EnergyBonus2 = 25288, // Staffs, arg1 = 'energy increase'
        OfEnchanting = 8888,  // arg2 = 'percent duration increase'
        Sundering = 9208,     // arg1 = 'percent chance', arg2 = 'armor pen percent'
        OfFortitude = 9032,   // arg1 = 'health increase'
        OfFortitude2 = 26776, // arg2 = 'health increase'
        OfAttribute = 9240,   // arg1 = 'attribute id', arg2 = 'percent chance'

        HCT_Generic = 8712,           // arg1 = 'percent trigger chance',
        HCT_OfAttribute = 8728,       // arg1 = 'percent trigger chance', arg2 = 'attribute id'
        HCT_OfItemsAttribute = 10248, // arg1 = 'percent trigger chance'

        HSR_Generic = 9128,           // arg1 = 'percent trigger chance',
        HSR_OfAttribute = 9112,       // arg1 = 'percent trigger chance', arg2 = 'attribute id'
        HSR_OfItemsAttribute = 10280, // arg1 = 'percent trigger chance'

        StrengthAndHonor = 8824,   // arg1 = 'health percentage threshold', arg2 = 'damage increase percent'
        VengeanceIsMine = 8840,    // arg1 = 'health percentage threshold', arg2 = 'damage increase percent'
        DanceWithDeath = 8872,     // arg2 = 'damage increase percent'
        TooMuchInformation = 8792, // arg2 = 'damage increase percent'
        GuidedByFate = 8808,       // arg2 = 'damage increase percent'

        DamageVsUndead = 41544,      // arg1 = 'damage percent increase'
        ArmorVsElemental = 8488,     // arg2 = 'bonus armor'
        ArmorVsDmgType = 41240,      // arg1 = 'damage type', arg2 = 'bonus armor'
        Armor = 8456,                // arg2 = 'bonus armor'
        ArmorValue = 42936,          // Shields, arg1 = 'armor value with req', arg2 = 'armor value without req'
        DoubleAdrenalineGain = 9144, // arg2 = 'percent chance'
        HealthWhileEnchanted = 9064, // arg1 = 'health increase'
        ConditionDecrease20 = 10328, // arg1 = 'skill_id-offset from bleeding (478), designating condition affected by -20%'
        DurationIncrease33 = 9320,   // arg = 'skill_id affected by +33%'
    };
    GW::ItemModifier GetItemModifier(const GW::Item &item, ModifierID mod_id);
    uint32_t GetHCTChance(GW::AgentLiving &agent, GW::Constants::AttributeByte attribute);
    uint32_t GetHSRChance(GW::AgentLiving &agent, GW::Constants::AttributeByte attribute);

    std::wstring GenericValueIDToString(uint32_t id);

    ImVec2 CalcExactTextSize(const char *text, const char *end = nullptr);
    ImVec2 CalculateTextBoundingBox(ImFont *font, const char *text, ImVec2 &out_min, ImVec2 &out_max);

    std::string SkillConstantDataToString(GW::Skill &skill);

    std::span<GW::Skill> GetSkillSpan();

    uint32_t LinearAttributeScale(uint32_t value0, uint32_t value15, uint32_t attribute_level);
    std::optional<std::pair<uint8_t, uint8_t>> ReverseLinearAttributeScale(uint32_t value0, uint32_t value15, uint32_t value);
    std::span<GW::Attribute> GetAgentAttributeSpan(uint32_t agent_id);
    uint32_t GetAgentAttributeForSkill(uint32_t agent_id, GW::Skill &skill);

    std::vector<GW::Effect *> GetAgentUniqueEffects(uint32_t agent_id);
    GW::Effect *GetEffectByID(uint32_t agent_id, uint32_t effect_id);
    GW::Effect *GetUniqueEffectByID(uint32_t agent_id, uint32_t effect_id);

    template <typename Container, typename Element>
    bool LinearSearch(const Container &array, const Element &value)
    {
        for (const auto &element : array)
        {
            if (element == value)
                return true;
        }
        return false;
    }

    GW::AgentLiving *GetAgentLivingByID(uint32_t agent_id);
    GW::AgentLiving &GetAgentLivingByIDOrThrow(uint32_t agent_id);
    bool ReceivesStoCEffects(uint32_t agent_id);
    bool IsPartyMember(GW::PartyInfo &party, uint32_t agent_id);
    bool IsPartyMember(uint32_t agent_id, uint32_t party_id = 0);

    std::string_view GetEffectIDStr(GW::Constants::EffectID effect_id);
    bool HasVisibleEffect(GW::AgentLiving &agent_living, GW::Constants::EffectID visible_effect_id);

    enum struct AgentRelations
    {
        Null,
        Friendly,
        Neutral,
        Hostile,
    };
    AgentRelations GetAgentRelations(GW::Constants::Allegiance agent1_allegiance, GW::Constants::Allegiance agent2_allegiance);
    AgentRelations GetAgentRelations(uint32_t agent1_id, uint32_t agent2_id);

    float Remap(float input_min, float input_max, float output_min, float output_max, float value);
    float ParabolicRemap(float input_min, float input_max, float output_min, float output_max, float value);

    struct ColorChange
    {
        size_t pos;
        ImU32 color; // 0 = pop color
    };

    struct TextEmoji
    {
        size_t pos;
        TextureModule::DrawPacket draw_packet;
    };

    struct TextTooltip
    {
        size_t start;
        size_t end;
        size_t id;

        bool operator==(const TextTooltip &other) const { return start == other.start && end == other.end && id == other.id; }
    };

    // void UnrichText(std::string_view rich_text, std::vector<char> &out_chars, std::vector<ColorChange> &color_changes, std::vector<TextTooltip> &tooltips);
    void DrawMultiColoredText(
        std::string_view text,
        float wrapping_min, float wrapping_max,
        std::span<ColorChange> color_changes = {},
        std::span<uint16_t> highlighting = {},
        std::span<TextTooltip> tooltips = {},
        std::function<void(uint32_t)> draw_tooltip = nullptr,
        std::span<TextEmoji> emojis = {}
    );

    enum struct SkillContext
    {
        Null,

        Rollerbeetle,
        DragonArena,
        Yuletide,
        AgentOfTheMadKing,
        CandyCornInfantry,
        Polymock,
        Brawling,
        Commando,
        Golem,
        UrsanBlessing,
        RavenBlessing,
        VolfenBlessing,
        KeiranThackeray,
        TuraiOssa,
        SaulDAlessio,
        Togo,
        Gwen,
        SpiritForm,
        SiegeDevourer,
        Junundu,

        Count,
    };

    enum struct MissType
    {
        Block = 0,
        Dodge = 1,
        Miss = 3,
        Obstructed = 4,
        Unk1 = 5,
    };

    enum struct OffhandItemType
    {
        // None = 0,
        Shield = 1,
        // Focus = 2,
    };

    const std::string_view GetAttributeString(GW::Constants::AttributeByte attribute);
    const std::string_view GetSkillContextString(SkillContext context);
    const std::string_view GetTitleString(GW::Constants::TitleID title_id);
    const std::string_view GetProfessionString(GW::Constants::ProfessionByte profession);
    const std::string_view GetCampaignString(GW::Constants::Campaign campaign);

    uint32_t GetSkillEffectBorderIndex(const GW::Skill &skill);

    bool IsControllableAgentOfPlayer(uint32_t agent_id, uint32_t player_number = 0);
    void GetControllableAgentsOfPlayer(std::span<uint32_t> &out, uint32_t player_number = 0);

    void OpenWikiPage(std::string_view page);
    void ImGuiCenterAlignCursorX(float size_x);
    void ImGuiThickText(const char *text);

    bool AppendFormattedV(char *buf, size_t buf_size, size_t &len, const char *fmt, va_list args);
    bool AppendFormatted(std::string &str, size_t append_max_size, const char *fmt, ...);

    bool IsFrameValid(GW::UI::Frame *frame);

    uint32_t GetPetOfAgent(uint32_t agent_id);
    bool InSameParty(uint32_t agent1_id, uint32_t agent2_id);
    bool IsRangeValue(float value);

    bool GetIsPet(uint32_t agent_id);

    bool IsOvercast(GW::AgentLiving &agent);

    ImRect GetFrameRect(const GW::UI::Frame &frame);
    bool IsHoveringFrame(const GW::UI::Frame &frame);
    void ForEachChildFrame(const GW::UI::Frame &parent_frame, std::function<void(GW::UI::Frame &)> func);
    void DrawOutlineOnFrame(const GW::UI::Frame &frame, ImColor color = IM_COL32(255, 0, 0, 255), std::string_view label = "", ImVec2 rel_label_pos = ImVec2(0.0f, 0.0f));

    struct GetSkillFrameResult
    {
        enum struct Error
        {
            None = 0,
            SkillAndAttributesNotOpened = 1,
            SkillFrameNotFound = 2,
        };
        Error error = Error::None;
        GW::UI::Frame *frame = nullptr;
    };

    GetSkillFrameResult GetSkillFrame(GW::Constants::SkillID skill_id);

    const std::wstring UIMessageToWString(GW::UI::UIMessage msg);

    uint32_t GetProfessionMask(uint32_t agent_id);
    bool IsSkillEquipable(GW::Skill &skill, uint32_t agent_id);

    GW::UI::Frame *GetTooltipFrame();
    GW::UI::Frame *GetDraggedSkillFrame();

    void ImGuiDebugLastItemRect();
}