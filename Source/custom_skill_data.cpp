#include <Windows.h>
#include <bitset>
#include <codecvt>
#include <d3d9.h>
#include <filesystem>
#include <future>
#include <iostream>
#include <regex>
#include <span>
#include <string>

#include <GWCA/GWCA.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Export.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <debug_display.h>
#include <party_data.h>
#include <update_manager.h>

#include <GWCA/GWCA.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/List.h>

#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/EventMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <async_lazy.h>
#include <attribute_or_title.h>
#include <attribute_store.h>
#include <buffer.h>
#include <custom_agent_data.h>
#include <debug_display.h>
#include <effect_tracking.h>
#include <skill_book.h>
#include <text_provider.h>
#include <utils.h>

#include "custom_skill_data.h"

namespace HerosInsight
{
    using SkillID = GW::Constants::SkillID;
    using SkillType = GW::Constants::SkillType;

    bool IsDeveloperSkill(const GW::Skill &skill)
    {
        constexpr static auto dev_skills = MakeFixedSet<SkillID>(
            SkillID::No_Skill,
            SkillID::Test_Buff,
            SkillID::Impending_Dhuum,
            SkillID::Dishonorable,
            SkillID::Unreliable
        );

        return skill.icon_file_id == 327614 || // Dev hax icon
               dev_skills.has(skill.skill_id);
    }

    bool IsArchivedSkill(CustomSkillData &custom_sd)
    {
        constexpr static auto archived_skills = MakeFixedSet<SkillID>(
            SkillID::UNUSED_Complicate,
            SkillID::UNUSED_Reapers_Mark,
            SkillID::UNUSED_Enfeeble,
            SkillID::UNUSED_Desecrate_Enchantments,
            SkillID::UNUSED_Signet_of_Lost_Souls,
            SkillID::UNUSED_Insidious_Parasite,
            SkillID::UNUSED_Searing_Flames,
            SkillID::UNUSED_Glowing_Gaze,
            SkillID::UNUSED_Steam,
            SkillID::UNUSED_Flame_Djinns_Haste,
            SkillID::UNUSED_Liquid_Flame,
            SkillID::UNUSED_Blessed_Light,
            SkillID::UNUSED_Shield_of_Absorption,
            SkillID::UNUSED_Smite_Condition,
            SkillID::UNUSED_Crippling_Slash,
            SkillID::UNUSED_Sun_and_Moon_Slash,
            SkillID::UNUSED_Enraging_Charge,
            SkillID::UNUSED_Tiger_Stance,
            SkillID::UNUSED_Burning_Arrow,
            SkillID::UNUSED_Natural_Stride,
            SkillID::UNUSED_Falling_Lotus_Strike,
            SkillID::UNUSED_Anthem_of_Weariness,
            SkillID::UNUSED_Pious_Fury,
            SkillID::UNUSED_Amulet_of_Protection,
            SkillID::UNUSED_Eviscerate,
            SkillID::UNUSED_Rush,
            SkillID::UNUSED_Lions_Comfort,
            SkillID::UNUSED_Melandrus_Shot,
            SkillID::UNUSED_Sloth_Hunters_Shot,
            SkillID::UNUSED_Reversal_of_Damage,
            SkillID::UNUSED_Empathic_Removal,
            SkillID::UNUSED_Castigation_Signet,
            SkillID::UNUSED_Wail_of_Doom,
            SkillID::UNUSED_Rip_Enchantment,
            SkillID::UNUSED_Foul_Feast,
            SkillID::UNUSED_Plague_Sending,
            SkillID::UNUSED_Overload,
            SkillID::UNUSED_Wastrels_Worry,
            SkillID::UNUSED_Lyssas_Aura,
            SkillID::UNUSED_Empathy,
            SkillID::UNUSED_Shatterstone,
            SkillID::UNUSED_Glowing_Ice,
            SkillID::UNUSED_Freezing_Gust,
            SkillID::UNUSED_Glyph_of_Immolation,
            SkillID::UNUSED_Glyph_of_Restoration,
            SkillID::UNUSED_Hidden_Caltrops,
            SkillID::UNUSED_Black_Spider_Strike,
            SkillID::UNUSED_Caretakers_Charge,
            SkillID::UNUSED_Signet_of_Mystic_Speed,
            SkillID::UNUSED_Signet_of_Rage,
            SkillID::UNUSED_Signet_of_Judgement,
            SkillID::UNUSED_Vigorous_Spirit,

            SkillID::REMOVE_Balthazars_Rage,
            SkillID::REMOVE_Boon_of_the_Gods,
            SkillID::REMOVE_Leadership_skill,
            SkillID::REMOVE_Queen_Armor,
            SkillID::Queen_Armor,
            SkillID::REMOVE_Queen_Wail,
            SkillID::Queen_Wail,
            SkillID::REMOVE_Wind_Prayers_skill,
            SkillID::REMOVE_With_Haste,
            (SkillID)807, // REMOVE_Soul_Reaping_Skill,

            SkillID::Forge_the_Way,
            SkillID::Anthem_of_Aggression,
            SkillID::Mantra_of_Celerity,
            SkillID::Signet_of_Attainment,
            SkillID::Accelerated_Growth,
            SkillID::Aim_True,
            SkillID::Aura_of_the_Great_Destroyer,
            SkillID::Bloodletting,
            SkillID::Borrowed_Energy,
            SkillID::Charr_Buff,
            SkillID::Command_of_Torment,
            SkillID::Construct_Possession,
            SkillID::Conviction_PvP,
            SkillID::Cry_of_Lament,
            SkillID::Destroy_the_Humans,
            SkillID::Dissipation,
            SkillID::Desperate_Howl,
            SkillID::Dozen_Shot,
            SkillID::Embrace_the_Pain,
            SkillID::Empathy_Koro,
            SkillID::Energy_Font,
            SkillID::Everlasting_Mobstopper_skill,
            SkillID::Fake_Spell,
            SkillID::Fire_and_Brimstone,
            SkillID::Gaze_of_MoavuKaal,
            SkillID::Guild_Monument_Protected,
            SkillID::Headshot,
            SkillID::Ice_Skates,
            SkillID::Intimidating_Aura_beta_version,
            SkillID::Kraks_Charge,
            SkillID::Lichs_Phylactery,
            SkillID::Lightning_Storm,
            SkillID::Marble_Trap,
            SkillID::Mending_Shrine_Bonus,
            SkillID::Natures_Speed,
            SkillID::Oracle_Link,
            SkillID::Recurring_Scourge,
            SkillID::Rough_Current,
            SkillID::Scepter_of_Ether,
            SkillID::Shadow_Tripwire,
            SkillID::Shiver_Touch,
            SkillID::Shrine_Backlash,
            SkillID::Signal_Flare,
            SkillID::Star_Shards,
            SkillID::Suicidal_Impulse,
            SkillID::Summoning_of_the_Scepter,
            SkillID::Sundering_Soulcrush,
            SkillID::Sunspear_Siege,
            SkillID::To_the_Death,
            SkillID::Turbulent_Flow,
            SkillID::Twisted_Spikes,
            SkillID::Unlock_Cell,
            SkillID::Vanish,
            SkillID::Vital_Blessing_monster_skill,
            SkillID::Way_of_the_Mantis,
            SkillID::Weapon_of_Mastery,
            (SkillID)2915, // NOX_Rectifier
            (SkillID)20,   // Confusion,
            SkillID::Signet_of_Illusions_beta_version,
            SkillID::Mimic,
            SkillID::Disappear,
            SkillID::Unnatural_Signet_alpha_version,
            SkillID::Unnatural_Signet_alpha_version,
            SkillID::Dont_Believe_Their_Lies,
            SkillID::Call_of_Ferocity,
            SkillID::Call_of_Elemental_Protection,
            SkillID::Call_of_Vitality,
            SkillID::Call_of_Healing,
            SkillID::Call_of_Resilience,
            SkillID::Call_of_Feeding,
            SkillID::Call_of_the_Hunter,
            SkillID::Call_of_Brutality,
            SkillID::Call_of_Disruption,
            SkillID::High_Winds,
            SkillID::Coming_of_Spring,
            SkillID::Signet_of_Creation_PvP,
            SkillID::Quickening_Terrain,
            SkillID::Massive_Damage,
            (SkillID)2781, // Minion Explosion
            SkillID::Couriers_Haste,
            (SkillID)2811, // Hive Mind
            (SkillID)2812, // Blood Pact
            SkillID::Inverse_Ninja_Law,
            (SkillID)2814, // Keep yourself alive
            (SkillID)2815, // ...
            (SkillID)2816, // Bounty Hunter
            (SkillID)2817, // ...
            (SkillID)3275, // ...
            (SkillID)3276, // ...
            (SkillID)3277, // ...
            (SkillID)3278, // ...
            (SkillID)3279, // ...
            (SkillID)3280, // ...
            (SkillID)3281, // ...
            SkillID::Victorious_Renewal,
            SkillID::A_Dying_Curse,
            (SkillID)3284, // ...
            (SkillID)3285, // ...
            (SkillID)3286, // ...
            (SkillID)3287, // ...
            SkillID::Rage_of_the_Djinn,
            SkillID::Lone_Wolf,
            SkillID::Stand_Together,
            SkillID::Unyielding_Spirit,
            SkillID::Reckless_Advance,
            SkillID::Solidarity,
            SkillID::Fight_Against_Despair,
            SkillID::Deaths_Succor,
            SkillID::Battle_of_Attrition,
            SkillID::Fight_or_Flight,
            SkillID::Renewing_Escape,
            SkillID::Battle_Frenzy,
            SkillID::The_Way_of_One,
            (SkillID)3388, // ...
            (SkillID)3389, // ...
            (SkillID)656,  // Life draining on crit
            (SkillID)2362, // Unnamed skill
            (SkillID)2239, // Unnamed skill
            (SkillID)3255, // Shield of the Champions
            (SkillID)3256, // Shield of the Champions
            (SkillID)3257, // Shield of the Champions
            (SkillID)3258, // Shield of the Champions
            SkillID::Pains_Embrace,
            SkillID::Siege_Attack_Bombardment,
            SkillID::Advance,
            SkillID::Water_Pool,
            SkillID::Torturous_Embers,
            SkillID::Torturers_Inferno,
            SkillID::Talon_Strike,
            SkillID::Shroud_of_Ash,
            SkillID::Explosive_Force,
            SkillID::Stop_Pump,
            SkillID::Sacred_Branch,
            SkillID::From_Hell,
            SkillID::Corrupted_Roots,
            SkillID::Meditation_of_the_Reaper1,
            (SkillID)654, // Energy steal on crit
            (SkillID)655, // Energy steal chance on hit
            SkillID::Party_Time,
            (SkillID)3411, // Keep the flag flying I
            (SkillID)3412, // Keep the flag flying II
            (SkillID)721,  // Feigned Sacrifice
            (SkillID)735,  // Fluidity
            (SkillID)747,  // Fury
            (SkillID)744,  // Justice
            (SkillID)718,  // Necropathy
            (SkillID)740,  // Peace
            (SkillID)759,  // Preparation
            (SkillID)742,  // Rapid Healing
            (SkillID)757,  // Rapid Shot
            (SkillID)720,  // Sacrifice
            (SkillID)733,  // Scintillation
            (SkillID)729,  // Stone
            (SkillID)738,  // Blessings
            (SkillID)675,  // Cultist's
            (SkillID)716,  // Cursing
            (SkillID)758,  // Deadliness
            (SkillID)731,  // The Bibliophile
            (SkillID)719,  // The Plague
            (SkillID)727,  // Thunder
            (SkillID)723,  // Vampire
            (SkillID)3250, // Temple Strike but not elite
            (SkillID)756   // Barrage (passive effect)
        );

        if (IsDeveloperSkill(*custom_sd.skill) ||
            archived_skills.has(custom_sd.skill_id))
            return true;

        if (custom_sd.skill->IsPvP())
        {
            auto non_pvp_version = GW::SkillbarMgr::GetSkillConstantData(custom_sd.skill->skill_id_pvp);
            if (non_pvp_version->skill_id_pvp != custom_sd.skill_id)
            {
                // The non-pvp version does not point back to the pvp version,
                // this is a sign that the skill is an archived pvp skill.
                return true;
            }
        }

        auto &text_provider = Text::GetTextProvider(GW::Constants::Language::English);
        auto name = text_provider.GetName(custom_sd.skill_id);
        auto full = text_provider.GetRawDescription(custom_sd.skill_id, false);
        auto concise = text_provider.GetRawDescription(custom_sd.skill_id, true);

        if (name == "..." &&
            full == "..." &&
            concise == "...")
        {
            // We assume that the skill is archived if the name and descriptions are missing.
            return true;
        }

        if (name.starts_with("(TEMP)") ||
            full.starts_with("(TEMP)") ||
            concise.starts_with("(TEMP)"))
        {
            // We assume that the skill is archived if the any string starts with "(TEMP)".
            return true;
        }

        return false;
    }

    bool IsBounty(GW::Skill &skill)
    {
        if (skill.type != SkillType::Bounty)
            return false;

        constexpr static auto bounties_with_non_rep_effects = MakeFixedSet<SkillID>(
            SkillID::Great_Dwarfs_Blessing,
            SkillID::Great_Dwarfs_Blessing1,
            SkillID::Energy_Channel,
            SkillID::Energy_Channel1,
            SkillID::Strength_of_the_Norn,
            SkillID::Strength_of_the_Norn1,
            SkillID::Vanguard_Commendation,
            SkillID::Vanguard_Commendation1
        );

        auto id = skill.skill_id;
        if (bounties_with_non_rep_effects.has(id))
            return false;

        if (SkillID::Skale_Hunt <= id && id <= SkillID::Undead_Hunt ||
            SkillID::Monster_Hunt <= id && id <= SkillID::Undead_Hunt1 ||
            SkillID::Monster_Hunt5 <= id && id <= SkillID::Undead_Hunt3 ||
            SkillID::Anguish_Hunt == id ||
            SkillID::Dhuum_Battle1 <= id && id <= SkillID::Monster_Hunt8 ||
            SkillID::Asuran_Bodyguard <= id && id <= SkillID::Asuran_Bodyguard3 ||
            SkillID::Veteran_Asuran_Bodyguard <= id && id <= SkillID::Time_Attack4)
            return true;

        return false;
    }

    bool IsSpellSkill(GW::Skill &skill)
    {
        return skill.type == SkillType::Spell ||
               skill.type == SkillType::Enchantment ||
               skill.type == SkillType::Hex ||
               skill.type == SkillType::Well ||
               skill.type == SkillType::Ward ||
               skill.type == SkillType::ItemSpell ||
               skill.type == SkillType::WeaponSpell;
    }

    bool EndsOnIncDamage(SkillID skill_id)
    {
        constexpr static auto skills = MakeFixedSet<SkillID>(
            SkillID::Reversal_of_Fortune,
            SkillID::Reversal_of_Damage,
            SkillID::Life_Sheath,
            SkillID::Reverse_Hex,
            SkillID::Weapon_of_Remedy,
            SkillID::Xinraes_Weapon,
            SkillID::Vengeful_Weapon,
            SkillID::Ballad_of_Restoration,
            SkillID::Ballad_of_Restoration_PvP
        );

        return skills.has(skill_id);
    }

    bool IsProjectileSkill(GW::Skill &skill)
    {
        constexpr static auto skills = MakeFixedSet<SkillID>(
            SkillID::Spear_of_Light,
            SkillID::Lightning_Orb,
            SkillID::Lightning_Orb_PvP,
            SkillID::Shock_Arrow,
            SkillID::Lightning_Bolt,
            SkillID::Lightning_Javelin,
            SkillID::Stone_Daggers,
            SkillID::Stoning,
            SkillID::Ebon_Hawk,
            SkillID::Glowstone,
            SkillID::Fireball,
            SkillID::Flare,
            SkillID::Lava_Arrows,
            SkillID::Phoenix,
            SkillID::Water_Trident,
            SkillID::Ice_Spear,
            SkillID::Shard_Storm,
            SkillID::Crippling_Dagger,
            SkillID::Dancing_Daggers,
            SkillID::Disrupting_Dagger,
            SkillID::Alkars_Alchemical_Acid,
            SkillID::Snaring_Web,
            SkillID::Corsairs_Net,
            SkillID::Bit_Golem_Breaker,
            SkillID::Fireball_obelisk,
            SkillID::NOX_Thunder,
            SkillID::Phased_Plasma_Burst,
            SkillID::Plasma_Shot,
            SkillID::Snowball_NPC,
            SkillID::Ultra_Snowball,
            SkillID::Ultra_Snowball1,
            SkillID::Ultra_Snowball2,
            SkillID::Ultra_Snowball3,
            SkillID::Ultra_Snowball4,
            SkillID::Fire_Dart,
            SkillID::Ice_Dart,
            SkillID::Poison_Dart,
            SkillID::Lightning_Orb1,
            (SkillID)2795, // Advanced Snowball
            SkillID::Mega_Snowball,
            SkillID::Snowball,
            SkillID::Snowball1,
            (SkillID)2796, // Super Mega Snowball
            SkillID::Rocket_Propelled_Gobstopper,
            SkillID::Polymock_Fireball,
            SkillID::Polymock_Flare,
            SkillID::Polymock_Frozen_Trident,
            SkillID::Polymock_Ice_Spear,
            SkillID::Polymock_Lightning_Orb,
            SkillID::Polymock_Piercing_Light_Spear,
            SkillID::Polymock_Shock_Arrow,
            SkillID::Polymock_Stone_Daggers,
            SkillID::Polymock_Stoning
        );

        if (skill.type == SkillType::Attack &&
            skill.weapon_req & 70)
        {
            return true;
        }

        return skills.has(skill.skill_id);
    }

    bool IsDragonArenaSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Spirit_of_the_Festival && skill_id <= SkillID::Imperial_Majesty);
    }

    bool IsRollerBeetleSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Rollerbeetle_Racer && skill_id <= SkillID::Spit_Rocks);
    }

    bool IsYuletideSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Blinding_Snow &&
                skill_id <= SkillID::Flurry_of_Ice) ||

               (skill_id >= SkillID::Side_Step &&
                skill_id <= SkillID::Rudis_Red_Nose) ||

               (skill_id >= (SkillID)2795 && // Advanced Snowball
                skill_id <= (SkillID)2802) ||

               skill_id == (SkillID)2964; // Snowball but with 1s aftercast
    }

    bool IsAgentOfTheMadKingSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Agent_of_the_Mad_King && skill_id <= SkillID::The_Mad_Kings_Influence);
    }

    bool IsCandyCornInfantrySkill(SkillID skill_id)
    {
        constexpr static auto skills = MakeFixedSet<SkillID>(
            (SkillID)2757, // Candy Corn Infantry disguise
            SkillID::Candy_Corn_Strike,
            SkillID::Rocket_Propelled_Gobstopper,
            SkillID::Rain_of_Terror_spell,
            SkillID::Cry_of_Madness,
            SkillID::Sugar_Infusion,
            SkillID::Feast_of_Vengeance,
            SkillID::Animate_Candy_Minions, // Animate Candy Golem
            (SkillID)2789                   // Summon Candy Golem
        );
        return skills.has(skill_id);
    }

    bool IsBrawlingSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Brawling &&
                skill_id <= SkillID::STAND_UP) ||

               (skill_id >= SkillID::Falkens_Fire_Fist &&
                skill_id <= SkillID::Falken_Quick);
    }

    bool IsPolymockSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Polymock_Power_Drain &&
                skill_id <= SkillID::Polymock_Mind_Wreck) ||

               (skill_id >= SkillID::Polymock_Deathly_Chill &&
                skill_id <= SkillID::Mursaat_Elementalist_Form);
    }

    bool IsCelestialSkill(SkillID skill_id)
    {
        constexpr static auto celestial_skills = MakeFixedSet<SkillID>(
            SkillID::Celestial_Haste,
            SkillID::Celestial_Stance,
            SkillID::Celestial_Storm,
            SkillID::Celestial_Summoning,
            SkillID::Star_Servant,
            SkillID::Star_Shine,
            SkillID::Star_Strike,
            SkillID::Storm_of_Swords
        );
        return celestial_skills.has(skill_id);
    }

    bool IsCommandoSkill(SkillID skill_id)
    {
        return skill_id == SkillID::Going_Commando ||
               (skill_id >= SkillID::Stun_Grenade &&
                skill_id <= SkillID::Tango_Down);
    }

    bool IsGolemSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::GOLEM_disguise &&
                skill_id <= SkillID::Annihilator_Toss) ||

               skill_id == SkillID::Sky_Net;
    }

    bool IsRavenSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Raven_Talons &&
                skill_id <= SkillID::Raven_Flight) ||

               (skill_id >= SkillID::Raven_Blessing_A_Gate_Too_Far &&
                skill_id <= SkillID::Raven_Talons_A_Gate_Too_Far);
    }

    bool IsUrsanSkill(SkillID skill_id)
    {
        constexpr static auto ursan_skills = MakeFixedSet<SkillID>(
            SkillID::Ursan_Strike,
            SkillID::Ursan_Rage,
            SkillID::Ursan_Roar,
            SkillID::Ursan_Force,
            SkillID::Ursan_Roar_Blood_Washes_Blood,
            SkillID::Ursan_Force_Blood_Washes_Blood,
            SkillID::Ursan_Aura,
            SkillID::Ursan_Rage_Blood_Washes_Blood,
            SkillID::Ursan_Strike_Blood_Washes_Blood,
            (SkillID)2115 // Ursan blessing effect (blood washes blood)
        );

        return ursan_skills.has(skill_id);
    }

    bool IsVolfenSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Volfen_Claw &&
                skill_id <= SkillID::Volfen_Agility) ||

               (skill_id >= SkillID::Volfen_Pounce_Curse_of_the_Nornbear &&
                skill_id <= SkillID::Volfen_Blessing_Curse_of_the_Nornbear);
    }

    bool IsNornAspectSkill(SkillID skill_id)
    {
        return (IsUrsanSkill(skill_id) || IsVolfenSkill(skill_id) || IsRavenSkill(skill_id) || skill_id == SkillID::Totem_of_Man);
    }

    bool IsKeiranSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Keiran_Thackeray_disguise &&
                skill_id <= SkillID::Rain_of_Arrows) ||

               (skill_id >= SkillID::Keirans_Sniper_Shot_Hearts_of_the_North &&
                skill_id <= SkillID::Theres_Nothing_to_Fear_Thackeray);
    }

    bool IsGwenSkill(SkillID skill_id)
    {
        return skill_id == SkillID::Gwen_disguise ||

               (skill_id >= SkillID::Distortion_Gwen &&
                skill_id <= SkillID::Sum_of_All_Fears_Gwen) ||

               (skill_id >= SkillID::Hide &&
                skill_id <= SkillID::Throw_Rock);
    }

    bool IsTogoSkill(SkillID skill_id)
    {
        constexpr static auto togo_skills = MakeFixedSet<SkillID>(
            SkillID::Togo_disguise,
            SkillID::Call_to_the_Spirit_Realm,
            SkillID::Essence_Strike_Togo,
            SkillID::Spirit_Burn_Togo,
            SkillID::Spirit_Rift_Togo,
            SkillID::Mend_Body_and_Soul_Togo,
            SkillID::Offering_of_Spirit_Togo,
            SkillID::Disenchantment_Togo,
            SkillID::Dragon_Empire_Rage,
            SkillID::Pain1,
            SkillID::Pain_attack_Togo,
            SkillID::Pain_attack_Togo1,
            SkillID::Pain_attack_Togo2
        );
        return togo_skills.has(skill_id);
    }

    bool IsTuraiSkill(SkillID skill_id)
    {
        return skill_id == SkillID::Turai_Ossa_disguise ||

               (skill_id >= SkillID::For_Elona &&
                skill_id <= SkillID::Whirlwind_Attack_Turai_Ossa) ||

               skill_id == SkillID::Dragon_Slash_Turai_Ossa;
    }

    bool IsSaulSkill(SkillID skill_id)
    {
        constexpr static auto saul_skills = MakeFixedSet<SkillID>(
            SkillID::Saul_DAlessio_disguise,
            SkillID::Signet_of_the_Unseen,
            SkillID::Castigation_Signet_Saul_DAlessio,
            SkillID::Unnatural_Signet_Saul_DAlessio,
            SkillID::Spectral_Agony_Saul_DAlessio,
            SkillID::Banner_of_the_Unseen,
            SkillID::Form_Up_and_Advance
        );
        return saul_skills.has(skill_id);
    }

    bool IsMissionSkill(SkillID skill_id)
    {
        constexpr static auto mission_skills = MakeFixedSet<SkillID>(
            SkillID::Disarm_Trap,
            SkillID::Vial_of_Purified_Water,
            SkillID::Lit_Torch,
            (SkillID)2366,             // Alkar's Concoction item skill
            SkillID::Alkars_Concoction // Alkar's Concoction effect
        );

        return mission_skills.has(skill_id);
    }

    bool IsSpiritFormSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Dhuums_Rest &&
                skill_id <= SkillID::Ghostly_Fury) ||

               skill_id == SkillID::Spirit_Form_disguise;
    }

    bool IsSiegeDevourerSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Siege_Devourer && skill_id <= SkillID::Dismount_Siege_Devourer);
    }

    bool IsJununduSkill(SkillID skill_id)
    {
        return (skill_id >= SkillID::Choking_Breath &&
                skill_id <= SkillID::Junundu_Wail) ||

               (skill_id >= SkillID::Desert_Wurm_disguise &&
                skill_id <= SkillID::Leave_Junundu) ||

               skill_id == SkillID::Unknown_Junundu_Ability;
    }

    bool IsBundleSkill(SkillID skill_id)
    {
        constexpr static auto bundle_skills = MakeFixedSet<SkillID>(
            SkillID::Asuran_Flame_Staff,
            SkillID::Aura_of_the_Staff_of_the_Mists,
            SkillID::Curse_of_the_Staff_of_the_Mists,
            SkillID::Power_of_the_Staff_of_the_Mists,
            SkillID::Balm_Bomb,
            SkillID::Barbed_Bomb,
            SkillID::Burden_Totem,
            SkillID::Courageous_Was_Saidra,
            SkillID::Dwarven_Powder_Keg,
            SkillID::Entanglement,
            SkillID::Explosives,
            SkillID::Firebomb_Explosion,
            SkillID::Flux_Overload,
            SkillID::Gelatinous_Material_Explosion,
            SkillID::Gift_of_Battle,
            SkillID::Healing_Salve,
            (SkillID)2833, // Improvised Fire Bomb
            (SkillID)2834, // Improvised Fire Trap
            SkillID::Invigorating_Mist,
            SkillID::Light_of_Seborhin,
            SkillID::Rations,
            SkillID::Scepter_of_Orrs_Aura,
            SkillID::Scepter_of_Orrs_Power,
            SkillID::Seed_of_Resurrection,
            SkillID::Seed_of_Resurrection1,
            SkillID::Urn_of_Saint_Viktor_Level_1,
            SkillID::Urn_of_Saint_Viktor_Level_2,
            SkillID::Urn_of_Saint_Viktor_Level_3,
            SkillID::Urn_of_Saint_Viktor_Level_4,
            SkillID::Urn_of_Saint_Viktor_Level_5,
            SkillID::Shield_of_Saint_Viktor,
            SkillID::Shield_of_Saint_Viktor_Celestial_Summoning,
            SkillID::Shielding_Urn_skill,
            SkillID::Spear_of_Archemorus_Level_1,
            SkillID::Spear_of_Archemorus_Level_2,
            SkillID::Spear_of_Archemorus_Level_3,
            SkillID::Spear_of_Archemorus_Level_4,
            SkillID::Spear_of_Archemorus_Level_5,
            SkillID::Splinter_Mine_skill,
            SkillID::Stun_Bomb,
            SkillID::Volatile_Charr_Crystal
        );
        return bundle_skills.has(skill_id);
    }

    bool IsPvEOnlySkill(const GW::Skill &skill)
    {
        const auto skill_id = skill.skill_id;
        bool is_title_skill = skill.title != 48; // All title skills are PvE
        return is_title_skill || skill.IsPvE();
    }

    bool IsPvPOnlySkill(const GW::Skill &skill)
    {
        constexpr static auto pvp_only_skills = MakeFixedSet<SkillID>(
            SkillID::Charm_Animal_Codex
        );

        return pvp_only_skills.has(skill.skill_id);
    }

    bool IsConsumableItemSkill(const GW::Skill &skill)
    {
        constexpr static auto item_skills = MakeFixedSet<SkillID>(
            (SkillID)2366,              // Alkar's Concoction item skill
            SkillID::Alkars_Concoction, // Effect after using the item
            SkillID::Birthday_Cupcake_skill,
            SkillID::Candy_Apple_skill,
            SkillID::Candy_Corn_skill,
            SkillID::Golden_Egg_skill,
            SkillID::Lucky_Aura,
            SkillID::Lunar_Blessing,
            SkillID::Spiritual_Possession,
            SkillID::Party_Mode,
            SkillID::Pie_Induced_Ecstasy,
            SkillID::Blue_Rock_Candy_Rush,
            SkillID::Green_Rock_Candy_Rush,
            SkillID::Red_Rock_Candy_Rush,
            SkillID::Adventurers_Insight,
            SkillID::Berserkers_Insight,
            SkillID::Heros_Insight,
            SkillID::Hunters_Insight,
            SkillID::Lightbringers_Insight,
            SkillID::Rampagers_Insight,
            SkillID::Slayers_Insight,
            SkillID::Sugar_Rush_short,
            SkillID::Sugar_Rush_medium,
            SkillID::Sugar_Rush_long,
            SkillID::Sugar_Jolt_short,
            SkillID::Sugar_Jolt_long,
            SkillID::Grail_of_Might_item_effect,
            SkillID::Essence_of_Celerity_item_effect,
            SkillID::Armor_of_Salvation_item_effect,
            SkillID::Skale_Vigor,
            SkillID::Pahnai_Salad_item_effect,
            SkillID::Drake_Skin,
            SkillID::Yo_Ho_Ho_and_a_Bottle_of_Grog,
            SkillID::Well_Supplied,
            SkillID::Weakened_by_Dhuum,
            SkillID::Tonic_Tipsiness,
            SkillID::Summoning_Sickness,
            (SkillID)3407 // Summoning_Sickness
        );

        return item_skills.has(skill.skill_id);
    }

    bool IsEffectOnly(const GW::Skill &skill)
    {
        constexpr static auto effect_only_skills = MakeFixedSet<SkillID>(
            SkillID::Phase_Shield_effect
        );

        return skill.special & (uint32_t)Utils::SkillSpecialFlags::Effect ||
               skill.type == SkillType::Bounty ||
               skill.type == SkillType::Scroll ||
               skill.type == SkillType::Condition ||
               skill.type == SkillType::Title ||
               skill.type == SkillType::Passive ||
               skill.type == SkillType::Environmental ||
               skill.type == SkillType::EnvironmentalTrap ||
               skill.type == SkillType::Disguise ||
               effect_only_skills.has(skill.skill_id) ||
               IsConsumableItemSkill(skill);
    }

    bool IsSpiritAttackSkill(const GW::Skill &skill)
    {
        constexpr static auto spirit_attack_skills = MakeFixedSet<SkillID>(
            SkillID::Gaze_of_Fury_attack,
            SkillID::Bloodsong_attack,
            SkillID::Pain_attack,
            SkillID::Pain_attack_Signet_of_Spirits,
            SkillID::Pain_attack_Signet_of_Spirits1,
            SkillID::Pain_attack_Signet_of_Spirits2,
            SkillID::Pain_attack_Togo,
            SkillID::Pain_attack_Togo1,
            SkillID::Pain_attack_Togo2,
            SkillID::Shadowsong_attack,
            SkillID::Anguish_attack,
            SkillID::Vampirism_attack,
            SkillID::Disenchantment_attack,
            SkillID::Wanderlust_attack,
            SkillID::Dissonance_attack
        );

        return spirit_attack_skills.has(skill.skill_id);
    }

    bool IsEnvironmentSkill(const GW::Skill &skill)
    {
        constexpr static auto environment_skills = MakeFixedSet<SkillID>(
            SkillID::Stormcaller_skill,
            SkillID::Teleport_Players,
            SkillID::Rurik_Must_Live,
            SkillID::Torch_Degeneration_Hex,
            SkillID::Torch_Enchantment,
            SkillID::Torch_Hex,
            SkillID::Spectral_Infusion,
            SkillID::Call_of_the_Eye,
            SkillID::Altar_Buff,
            SkillID::Capture_Point,
            SkillID::Curse_of_Dhuum,
            SkillID::Fireball_obelisk,
            SkillID::Mad_Kings_Fan,
            SkillID::Resurrect_Party,
            SkillID::Rock_Slide,
            SkillID::Avalanche_effect,
            SkillID::Exploding_Barrel,
            SkillID::Water,
            SkillID::Chimera_of_Intensity,
            SkillID::Curse_of_the_Bloodstone,
            SkillID::Fount_Of_Maguuma,
            SkillID::Healing_Fountain,
            SkillID::Icy_Ground,
            SkillID::Divine_Fire,
            SkillID::Domain_of_Elements,
            SkillID::Domain_of_Energy_Draining,
            SkillID::Domain_of_Health_Draining,
            SkillID::Domain_of_Skill_Damage,
            SkillID::Domain_of_Slow,
            SkillID::Chain_Lightning_environment,
            SkillID::Eruption_environment,
            SkillID::Fire_Storm_environment,
            SkillID::Maelstrom_environment,
            SkillID::Mursaat_Tower_skill,
            SkillID::Obelisk_Lightning,
            SkillID::Quest_Skill,
            SkillID::Quest_skill_for_Coastal_Exam,
            SkillID::Quicksand_environment_effect,
            SkillID::Siege_Attack1,
            SkillID::Siege_Attack3,
            SkillID::Chomper,
            SkillID::Blast_Furnace,
            SkillID::Sorrows_Fist,
            SkillID::Sorrows_Flame,
            SkillID::Statues_Blessing,
            SkillID::Swamp_Water,
            SkillID::Tar,
            SkillID::Archemorus_Strike,
            SkillID::Elemental_Defense_Zone,
            SkillID::Melee_Defense_Zone,
            SkillID::Rage_of_the_Sea,
            SkillID::Gods_Blessing,
            SkillID::Madness_Dart,
            SkillID::Sentry_Trap_skill,
            SkillID::Spirit_Form_Remains_of_Sahlahja,
            SkillID::The_Elixir_of_Strength,
            SkillID::Untouchable,
            SkillID::Battle_Cry,
            SkillID::Battle_Cry1,
            SkillID::Energy_Shrine_Bonus,
            SkillID::To_the_Pain_Hero_Battles,
            SkillID::Northern_Health_Shrine_Bonus,
            SkillID::Southern_Health_Shrine_Bonus,
            SkillID::Western_Health_Shrine_Bonus,
            SkillID::Eastern_Health_Shrine_Bonus,
            SkillID::Boulder,
            SkillID::Burning_Ground,
            SkillID::Dire_Snowball,
            SkillID::Fire_Boulder,
            SkillID::Fire_Dart,
            SkillID::Fire_Jet,
            SkillID::Fire_Spout,
            SkillID::Freezing_Ground,
            SkillID::Haunted_Ground,
            SkillID::Ice_Dart,
            SkillID::Poison_Dart,
            SkillID::Poison_Ground,
            SkillID::Poison_Jet,
            SkillID::Poison_Spout,
            SkillID::Sarcophagus_Spores,
            SkillID::Fire_Dart1
        );

        if (skill.type == SkillType::Environmental ||
            skill.type == SkillType::EnvironmentalTrap)
            return true;

        return environment_skills.has(skill.skill_id);
    }

    bool IsMonsterSkill(const GW::Skill &skill)
    {
        constexpr static auto monster_skills_without_monster_icon = MakeFixedSet<SkillID>(
            (SkillID)1448, // Last Rites of Torment effect
            SkillID::Torturous_Embers,
            SkillID::Last_Rites_of_Torment,
            SkillID::Healing_Breeze_Agnars_Rage,
            SkillID::Crystal_Bonds,
            SkillID::Jagged_Crystal_Skin,
            SkillID::Crystal_Hibernation,
            SkillID::Life_Vortex,
            SkillID::Soul_Vortex2,
            (SkillID)1425,
            (SkillID)1712,
            (SkillID)1713,
            (SkillID)1714,
            SkillID::Corsairs_Net,
            SkillID::Lose_your_Head,
            SkillID::Wandering_Mind,
            (SkillID)1877,
            (SkillID)1932,
            SkillID::Embrace_the_Pain,
            SkillID::Earth_Shattering_Blow,
            SkillID::Corrupt_Power,
            SkillID::Words_of_Madness,
            SkillID::Words_of_Madness_Qwytzylkak,
            SkillID::Presence_of_the_Skale_Lord,
            SkillID::The_Apocrypha_is_changing_to_another_form,
            SkillID::Reform_Carvings,
            SkillID::Soul_Torture,
            SkillID::Maddened_Strike,
            SkillID::Maddened_Stance,
            SkillID::Kournan_Siege,
            SkillID::Bonds_of_Torment,
            (SkillID)1883, // Bonds of Torment passive
            SkillID::Shadow_Smash,
            SkillID::Banish_Enchantment,
            SkillID::Jadoths_Storm_of_Judgment,
            SkillID::Twisting_Jaws,
            SkillID::Snaring_Web,
            SkillID::Ceiling_Collapse,
            SkillID::Wurm_Bile,
            SkillID::Shattered_Spirit,
            SkillID::Spirit_Roar,
            SkillID::Unseen_Aggression,
            SkillID::Charging_Spirit,
            SkillID::Powder_Keg_Explosion,
            SkillID::Unstable_Ooze_Explosion,
            SkillID::Golem_Shrapnel,
            SkillID::Crystal_Snare,
            SkillID::Paranoid_Indignation,
            SkillID::Searing_Breath,
            SkillID::Call_of_Destruction,
            SkillID::Flame_Jet,
            SkillID::Lava_Ground,
            SkillID::Lava_Wave,
            SkillID::Lava_Blast,
            SkillID::Thunderfist_Strike,
            SkillID::Murakais_Consumption,
            SkillID::Murakais_Censure,
            SkillID::Filthy_Explosion,
            SkillID::Murakais_Call,
            SkillID::Enraged_Blast,
            SkillID::Fungal_Explosion,
            SkillID::Bear_Form,
            SkillID::Tongue_Lash,
            SkillID::Soulrending_Shriek,
            SkillID::Reverse_Polarity_Fire_Shield,
            SkillID::Forgewights_Blessing,
            SkillID::Selvetarms_Blessing,
            SkillID::Thommiss_Blessing,
            SkillID::Tongue_Whip,
            SkillID::Reactor_Blast,
            SkillID::Reactor_Blast_Timer,
            SkillID::Internal_Power_Engaged,
            SkillID::Target_Acquisition,
            SkillID::NOX_Beam,
            SkillID::NOX_Field_Dash,
            SkillID::NOXion_Buster,
            SkillID::Countdown,
            SkillID::Bit_Golem_Breaker,
            SkillID::Bit_Golem_Rectifier,
            SkillID::Bit_Golem_Crash,
            SkillID::Bit_Golem_Force,
            (SkillID)1915,
            SkillID::NOX_Phantom,
            SkillID::NOX_Thunder,
            SkillID::NOX_Lock_On,
            SkillID::NOX_Fire,
            SkillID::NOX_Knuckle,
            SkillID::NOX_Divider_Drive,
            SkillID::Theres_not_enough_time,
            SkillID::Keirans_Sniper_Shot,
            SkillID::Falken_Punch,
            SkillID::Drunken_Stumbling,
            SkillID::Koros_Gaze,
            SkillID::Adoration,
            SkillID::Isaiahs_Balance,
            SkillID::Toriimos_Burning_Fury,
            SkillID::Promise_of_Death,
            SkillID::Withering_Blade,
            SkillID::Deaths_Embrace,
            SkillID::Venom_Fang,
            SkillID::Survivors_Will,
            SkillID::Charm_Animal_Ashlyn_Spiderfriend,
            SkillID::Charm_Animal_Charr_Demolisher,
            SkillID::Charm_Animal_White_Mantle,
            SkillID::Charm_Animal_monster,
            SkillID::Charm_Animal_monster1,
            SkillID::Charm_Animal1,
            SkillID::Charm_Animal2,
            (SkillID)1868, // Monster charm animal
            (SkillID)1869, // Monster charm animal
            (SkillID)1870, // Monster charm animal
            (SkillID)1906, // Monster charm animal
            (SkillID)1907, // Monster charm animal
            (SkillID)1908, // Monster charm animal
            (SkillID)1909, // Monster charm animal
            SkillID::Ehzah_from_Above,
            SkillID::Rise_From_Your_Grave,
            SkillID::Resurrect_monster_skill,
            SkillID::Charm_Animal_monster_skill,
            SkillID::Phase_Shield_monster_skill,
            SkillID::Phase_Shield_effect,
            SkillID::Vitality_Transfer,
            SkillID::Restore_Life_monster_skill,
            SkillID::Splinter_Shot_monster_skill,
            SkillID::Junundu_Tunnel_monster_skill,
            SkillID::Vital_Blessing_monster_skill,
            SkillID::Snowball_NPC,
            SkillID::Veratas_Promise,
            SkillID::Ebon_Vanguard_Assassin_Support_NPC,
            SkillID::Ebon_Vanguard_Battle_Standard_of_Power,
            SkillID::Diamondshard_Mist,
            SkillID::Diamondshard_Mist_environment_effect,
            SkillID::Diamondshard_Grave,
            SkillID::Dhuums_Rest_Reaper_skill,
            SkillID::Ghostly_Fury_Reaper_skill,
            SkillID::Spiritual_Healing_Reaper_skill,
            SkillID::Golem_Pilebunker,
            SkillID::Putrid_Flames,
            SkillID::Whirling_Fires,
            SkillID::Wave_of_Torment,
            SkillID::REMOVE_Queen_Wail,
            SkillID::Queen_Wail,
            SkillID::REMOVE_Queen_Armor,
            SkillID::Queen_Armor,
            SkillID::Queen_Heal,
            SkillID::Queen_Bite,
            SkillID::Queen_Thump,
            SkillID::Queen_Siege,
            SkillID::Infernal_Rage,
            SkillID::Flame_Call,
            SkillID::Skin_of_Stone,
            SkillID::From_Hell,
            SkillID::Feeding_Frenzy_skill,
            SkillID::Frost_Vortex,
            SkillID::Earth_Vortex,
            SkillID::Enemies_Must_Die,
            SkillID::Enchantment_Collapse,
            SkillID::Call_of_Sacrifice,
            SkillID::Corrupted_Strength,
            SkillID::Corrupted_Roots,
            SkillID::Corrupted_Healing,
            SkillID::Caltrops_monster,
            SkillID::Call_to_the_Torment,
            SkillID::Abaddons_Favor,
            SkillID::Abaddons_Chosen,
            SkillID::Suicide_Health,
            SkillID::Suicide_Energy,
            SkillID::Meditation_of_the_Reaper,
            SkillID::Meditation_of_the_Reaper1,
            SkillID::Corrupted_Breath,
            SkillID::Kilroy_Stonekin,
            SkillID::Janthirs_Gaze,
            SkillID::Its_Good_to_Be_King
        );

        constexpr static auto non_monster_skills_with_monster_icon = MakeFixedSet<SkillID>(
            SkillID::Spectral_Agony_Saul_DAlessio,
            SkillID::Burden_Totem,
            SkillID::Splinter_Mine_skill,
            SkillID::Entanglement
        );

        const auto skill_id = skill.skill_id;

        if (IsSpiritAttackSkill(skill))
            return false;

        bool has_monster_flag = skill.special & (uint32_t)Utils::SkillSpecialFlags::MonsterSkill;

        if (has_monster_flag)
            return true;

        bool has_monster_icon = skill.icon_file_id == 78225;

        if (has_monster_icon)
        {
            return !non_monster_skills_with_monster_icon.has(skill_id);
        }
        else
        {
            return monster_skills_without_monster_icon.has(skill_id);
        }
    }

    bool IsMaintainedSkill(const GW::Skill &skill)
    {
        return skill.duration0 == skill.duration15 && skill.duration0 == 131072;
    }

    bool IsSkillType(const GW::Skill &skill)
    {
        if (IsEffectOnly(skill))
            return false;

        return true;
    }

    CustomSkillData custom_skill_datas[GW::Constants::SkillMax];

    bool IsNumber(char c)
    {
        return c >= '0' && c <= '9';
    }

    bool IsChar(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    void SkipWhitespace(char *&p, char *end)
    {
        while (p < end && *p == ' ')
            p++;
    }

    void SkipChars(char *&p, char *end)
    {
        while (p < end && IsChar(*p))
            p++;
    }

    void NextWord(char *&p, char *end)
    {
        SkipChars(p, end);
        SkipWhitespace(p, end);
    }

    void PrevWord(char *start, char *&p)
    {
        while (start < p && p[-1] == ' ')
            p--;
        while (start < p && p[-1] != ' ')
            p--;
    }

    bool IsWordStart(char *str_start, char *p)
    {
        return (p == str_start || !IsChar(p[-1])) && IsChar(p[0]);
    }

    void DetermineParamType(CustomSkillData &custom_sd, SkillParam param, std::string_view before, std::string_view after)
    {
        auto p = (char *)after.data();
        auto end = p + after.size();
        SkipWhitespace(p, end);
    }

    enum struct DescToken
    {
        Null,

        And,
        Ally,
        Above,
        After,
        Armor,
        Attack,
        Absorb,
        Activate,
        Attribute,
        Adrenaline,
        Additional,

        Burn,
        Block,
        Blind,
        Blunt,
        Bleed,
        Below,

        Cost,
        Cold,
        Cast,
        Comma,
        Chaos,
        Chant,
        Chance,
        Cripple,
        Condition,
        ConditionAndHex,
        ConditionsAndHexes,
        CrackedArmor,

        Die,
        Daze,
        Dark,
        Degen,
        Damage,
        Disease,
        Disable,
        DeepWound,

        End,
        Earth,
        Expire,
        Effect,
        Energy,
        Elemental,
        Enchantment,
        EnergyStorage,

        Foe,
        For,
        Fail,
        Fire,
        Faster,
        ForEach,

        Gain,

        Hex,
        Holy,
        Health,
        Heal,

        If,
        Interrupt,
        IllusionMagic,

        Knockdown,

        Lose,
        Last,
        Less,
        Level,
        Longer,
        Literal,
        Lifespan,
        Lightning,

        Max,
        Miss,
        More,
        Minus,
        Melee,
        Movement,

        Not,
        Next,
        NonSpirit,

        Or,

        Plus,
        Poison,
        Period,
        Percent,
        Physical,
        Piercing,
        Proximity,

        Regen,
        Ranged,
        Remove,
        Reduce,
        Recharge,

        Start,
        Spell,
        Steal,
        Stance,
        Signet,
        Skills,
        Shadow,
        Slower,
        Seconds,
        Slashing,
        Secondary,

        Target,
        Transfer,

        Weakness,
        Whenever,
        WaterMagic,
    };

    TokenizedDesc::TokenizedDesc(std::string_view desc)
    {
        auto start = (char *)desc.data();
        auto end = start + desc.size();
        auto p = start;

        auto TryReadWordStartingWith = [&](std::string_view str)
        {
            auto remaining = end - p;
            if (remaining < str.size())
                return false;

            if (str[0] != p[0] &&
                str[0] != std::tolower(p[0]))
                return false;

            auto cmp = std::memcmp(p + 1, str.data() + 1, str.size() - 1);
            if (cmp != 0)
                return false;

            p += str.size();

            SkipChars(p, end);

            return true;
        };

        auto TryReadWord = [&](std::string_view str)
        {
            auto remaining = end - p;
            if (remaining < str.size())
                return false;

            auto cmp = std::memcmp(p, str.data(), str.size());
            if (cmp != 0)
                return false;

            auto p_after = p + str.size();
            if (p_after < end && IsChar(*p_after))
                return false;

            p += str.size();

            return true;
        };

#define MATCH_AND_CONTINUE(str, word) \
    if (TryReadWordStartingWith(str)) \
    {                                 \
        tokens.push_back(word);       \
        continue;                     \
    }

        while (p < end)
        {
            SkipWhitespace(p, end);

            std::string_view rem{p, (size_t)(end - p)};
            SkillParam param;
            if (SkillParam::TryRead(rem, param))
            {
                p = (char *)rem.data();
                lits.push_back(param);
                tokens.push_back(DescToken::Literal);
                continue;
            }

            // misc
            MATCH_AND_CONTINUE(".", DescToken::Period)
            MATCH_AND_CONTINUE(",", DescToken::Comma)
            MATCH_AND_CONTINUE("%", DescToken::Percent)

            if (p == start || !IsChar(p[-1]))
            {
                MATCH_AND_CONTINUE("-second", DescToken::Seconds) // Special case for Shadowsong which has unusual formatting: "30-second lifespan"
                MATCH_AND_CONTINUE("-", DescToken::Minus)
                MATCH_AND_CONTINUE("+", DescToken::Plus)

                if (TryReadWord("one") ||
                    TryReadWord("an") ||
                    TryReadWord("a"))
                {
                    lits.push_back(SkillParam(1, 1));
                    tokens.push_back(DescToken::Literal);
                    continue;
                }

                if (TryReadWord("all"))
                {
                    lits.push_back(SkillParam(std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()));
                    tokens.push_back(DescToken::Literal);
                    continue;
                }

                if (TryReadWord("Half of all"))
                {
                    lits.push_back(SkillParam(50, 50));
                    tokens.push_back(DescToken::Literal);
                    tokens.push_back(DescToken::Percent);
                    continue;
                }

                // a
                MATCH_AND_CONTINUE("and", DescToken::And)
                MATCH_AND_CONTINUE("ally", DescToken::Ally)
                MATCH_AND_CONTINUE("above", DescToken::Above)
                MATCH_AND_CONTINUE("after", DescToken::After)
                MATCH_AND_CONTINUE("armor", DescToken::Armor)
                MATCH_AND_CONTINUE("attack", DescToken::Attack)
                MATCH_AND_CONTINUE("absorb", DescToken::Absorb)
                MATCH_AND_CONTINUE("activat", DescToken::Activate)
                MATCH_AND_CONTINUE("attribute", DescToken::Attribute)
                MATCH_AND_CONTINUE("adrenaline", DescToken::Adrenaline)
                MATCH_AND_CONTINUE("additional", DescToken::Additional)

                // b
                MATCH_AND_CONTINUE("burn", DescToken::Burn)
                MATCH_AND_CONTINUE("block", DescToken::Block)
                MATCH_AND_CONTINUE("blind", DescToken::Blind)
                MATCH_AND_CONTINUE("blunt", DescToken::Blunt)
                MATCH_AND_CONTINUE("bleed", DescToken::Bleed)
                MATCH_AND_CONTINUE("below", DescToken::Below)

                // c
                MATCH_AND_CONTINUE("cost", DescToken::Cost)
                MATCH_AND_CONTINUE("cold", DescToken::Cold)
                MATCH_AND_CONTINUE("cast", DescToken::Cast)
                MATCH_AND_CONTINUE("chaos", DescToken::Chaos)
                MATCH_AND_CONTINUE("chant", DescToken::Chant)
                MATCH_AND_CONTINUE("chance", DescToken::Chance)
                MATCH_AND_CONTINUE("cripple", DescToken::Cripple)
                MATCH_AND_CONTINUE("condition and hex", DescToken::ConditionAndHex)
                MATCH_AND_CONTINUE("conditions and hexes", DescToken::ConditionsAndHexes)
                MATCH_AND_CONTINUE("condition", DescToken::Condition)
                MATCH_AND_CONTINUE("Cracked Armor", DescToken::CrackedArmor)

                // d
                MATCH_AND_CONTINUE("die", DescToken::Die)
                MATCH_AND_CONTINUE("daze", DescToken::Daze)
                MATCH_AND_CONTINUE("dark", DescToken::Dark)
                MATCH_AND_CONTINUE("degen", DescToken::Degen)
                MATCH_AND_CONTINUE("damage", DescToken::Damage)
                MATCH_AND_CONTINUE("disease", DescToken::Disease)
                MATCH_AND_CONTINUE("disable", DescToken::Disable)
                MATCH_AND_CONTINUE("Deep Wound", DescToken::DeepWound)

                // e
                MATCH_AND_CONTINUE("end", DescToken::End)
                MATCH_AND_CONTINUE("earth", DescToken::Earth)
                MATCH_AND_CONTINUE("expire", DescToken::Expire)
                MATCH_AND_CONTINUE("effect", DescToken::Effect)
                MATCH_AND_CONTINUE("Energy Storage", DescToken::EnergyStorage)
                MATCH_AND_CONTINUE("energy", DescToken::Energy)
                MATCH_AND_CONTINUE("elemental", DescToken::Elemental)
                MATCH_AND_CONTINUE("enchantment", DescToken::Enchantment)

                // f
                MATCH_AND_CONTINUE("foe", DescToken::Foe)
                MATCH_AND_CONTINUE("for", DescToken::For)
                MATCH_AND_CONTINUE("fail", DescToken::Fail)
                MATCH_AND_CONTINUE("fire", DescToken::Fire)
                MATCH_AND_CONTINUE("faster", DescToken::Faster)
                MATCH_AND_CONTINUE("for each", DescToken::ForEach)

                // g
                MATCH_AND_CONTINUE("gain", DescToken::Gain)

                // h
                MATCH_AND_CONTINUE("hex and condition", DescToken::ConditionAndHex)
                MATCH_AND_CONTINUE("hexes and conditions", DescToken::ConditionsAndHexes)
                MATCH_AND_CONTINUE("hex", DescToken::Hex)
                MATCH_AND_CONTINUE("holy", DescToken::Holy)
                MATCH_AND_CONTINUE("health", DescToken::Health)
                MATCH_AND_CONTINUE("heal", DescToken::Heal)

                // i
                MATCH_AND_CONTINUE("if", DescToken::If)
                MATCH_AND_CONTINUE("interrupt", DescToken::Interrupt)
                MATCH_AND_CONTINUE("illusion", DescToken::IllusionMagic)

                // j

                // k
                MATCH_AND_CONTINUE("knock", DescToken::Knockdown)

                // l
                MATCH_AND_CONTINUE("los", DescToken::Lose)
                MATCH_AND_CONTINUE("last", DescToken::Last)
                MATCH_AND_CONTINUE("less", DescToken::Less)
                MATCH_AND_CONTINUE("level", DescToken::Level)
                MATCH_AND_CONTINUE("longer", DescToken::Longer)
                MATCH_AND_CONTINUE("lifespan", DescToken::Lifespan)
                MATCH_AND_CONTINUE("lightning", DescToken::Lightning)

                // m
                MATCH_AND_CONTINUE("max", DescToken::Max)
                MATCH_AND_CONTINUE("miss", DescToken::Miss)
                MATCH_AND_CONTINUE("more", DescToken::More)
                MATCH_AND_CONTINUE("melee", DescToken::Melee)
                MATCH_AND_CONTINUE("move", DescToken::Movement)

                // n
                MATCH_AND_CONTINUE("non-spirit", DescToken::NonSpirit)
                MATCH_AND_CONTINUE("no", DescToken::Not)
                MATCH_AND_CONTINUE("cannot", DescToken::Not)
                MATCH_AND_CONTINUE("next", DescToken::Next)

                // o
                MATCH_AND_CONTINUE("or", DescToken::Or)

                // p
                MATCH_AND_CONTINUE("poison", DescToken::Poison)
                MATCH_AND_CONTINUE("physical", DescToken::Physical)
                MATCH_AND_CONTINUE("piercing", DescToken::Piercing)
                MATCH_AND_CONTINUE("proximity", DescToken::Proximity)

                // q

                // r
                MATCH_AND_CONTINUE("regen", DescToken::Regen)
                MATCH_AND_CONTINUE("ranged", DescToken::Ranged)
                MATCH_AND_CONTINUE("remove", DescToken::Remove)
                MATCH_AND_CONTINUE("reduc", DescToken::Reduce)
                MATCH_AND_CONTINUE("recharge", DescToken::Recharge)

                // s
                MATCH_AND_CONTINUE("start", DescToken::Start)
                MATCH_AND_CONTINUE("spell", DescToken::Spell)
                MATCH_AND_CONTINUE("steal", DescToken::Steal)
                MATCH_AND_CONTINUE("stance", DescToken::Stance)
                MATCH_AND_CONTINUE("signet", DescToken::Signet)
                MATCH_AND_CONTINUE("shadow", DescToken::Shadow)
                MATCH_AND_CONTINUE("slower", DescToken::Slower)
                MATCH_AND_CONTINUE("secondary", DescToken::Secondary)
                MATCH_AND_CONTINUE("second", DescToken::Seconds)
                MATCH_AND_CONTINUE("slashing", DescToken::Slashing)

                // t
                MATCH_AND_CONTINUE("target", DescToken::Target)
                MATCH_AND_CONTINUE("transfer", DescToken::Transfer)

                // u

                // v

                // w
                MATCH_AND_CONTINUE("weak", DescToken::Weakness)
                MATCH_AND_CONTINUE("whenever", DescToken::Whenever)
                MATCH_AND_CONTINUE("water magic", DescToken::WaterMagic)

                // x

                // y

                // z
            }
            p++;
        }
    }

    // "We have AI at home"
    void ParseParamContext(std::vector<ParsedSkillData> &pds, ParsedSkillData &pd, std::span<DescToken> sentence, int32_t lit_pos)
    {
        auto n_words = sentence.size();

        auto IsMatch = [&](int32_t index, DescToken word)
        {
            if (index >= 0 && index < n_words)
                return sentence[index] == word;
            return false;
        };
        auto IsListMatch = [&](int32_t index, std::initializer_list<DescToken> words)
        {
            if (index >= 0 && index + words.size() <= n_words)
            {
                return std::memcmp(&sentence[index], words.begin(), words.size() * sizeof(DescToken)) == 0;
            }
            return false;
        };

        auto GetDmgType = [&](int32_t index)
        {
            std::optional<DamageType> result = std::nullopt;
            if (0 <= index && index < n_words)
            {
                // clang-format off
                switch (sentence[index]) {
                    case DescToken::Fire:      result = DamageType::Fire;      break;
                    case DescToken::Lightning: result = DamageType::Lightning; break;
                    case DescToken::Earth:     result = DamageType::Earth;     break;
                    case DescToken::Cold:      result = DamageType::Cold;      break;
                    case DescToken::Slashing:  result = DamageType::Slashing;  break;
                    case DescToken::Piercing:  result = DamageType::Piercing;  break;
                    case DescToken::Blunt:     result = DamageType::Blunt;     break;
                    case DescToken::Chaos:     result = DamageType::Chaos;     break;
                    case DescToken::Dark:      result = DamageType::Dark;      break;
                    case DescToken::Holy:      result = DamageType::Holy;      break;
                    case DescToken::Shadow:    result = DamageType::Shadow;    break;
                }
                // clang-format on
            }
            return result;
        };

        auto PushParam = [&](ParsedSkillData::Type ty)
        {
            pd.type = ty;
            pds.push_back(pd);
        };

#define PUSH_PARAM_AND_RETURN(ty, ...) \
    {                                  \
        PushParam(ty, __VA_ARGS__);    \
        return;                        \
    }

#define IF_MATCH_PUSH_PARAM_AND_RETURN(index, word, ty, ...) \
    if (IsMatch(index, word))                                \
    {                                                        \
        PUSH_PARAM_AND_RETURN(ty, __VA_ARGS__)               \
    }

        bool has_plus = IsMatch(lit_pos - 1, DescToken::Plus);
        bool has_minus = IsMatch(lit_pos - 1, DescToken::Minus);
        bool has_percent = IsMatch(lit_pos + 1, DescToken::Percent);
        pd.is_percent = has_percent;
        bool is_less = has_minus;
        bool is_more = has_plus;
        bool is_reduced = false;

        int32_t just_after = has_percent ? lit_pos + 2 : lit_pos + 1;
        int32_t just_before = has_minus || has_plus ? lit_pos - 2 : lit_pos - 1;

        if (IsMatch(just_after, DescToken::Less))
        {
            is_less = true;
            just_after++;
        }

        if (IsMatch(just_after, DescToken::More) ||
            IsMatch(just_after, DescToken::Additional))
        {
            is_more = true;
            just_after++;
        }

        if (IsMatch(just_before, DescToken::Additional))
        {
            is_more = true;
            just_before--;
        }

        for (int32_t i = 0; i <= just_before; i++)
        {
            if (IsMatch(i, DescToken::Reduce))
            {
                is_reduced = true;
                break;
            }
        }

        auto GetPropsBefore = [&](std::span<const DescToken> words, std::function<void(DescToken)> handler)
        {
            int32_t i = just_before;
            bool any_found = false;
            while (i >= 0)
            {
                auto word = sentence[i];
                for (auto allowed_word : words)
                {
                    if (word == allowed_word)
                    {
                        handler(word);
                        any_found = true;
                        goto ok;
                    }
                }
                break;
            ok:
                i--;
                if (i >= 0 &&
                    (sentence[i] == DescToken::And ||
                     sentence[i] == DescToken::Comma))
                    i--;
            }
            return any_found;
        };

        if (has_percent)
        {
            if (IsListMatch(just_after, {DescToken::Chance, DescToken::Block}) ||
                IsListMatch(just_after, {DescToken::Block, DescToken::Chance}))
                PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::ChanceToBlock)

            if (IsListMatch(just_after, {DescToken::Fail, DescToken::Chance}))
                PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::ChanceToFail)

            if (IsListMatch(just_after, {DescToken::Chance, DescToken::Miss}) ||
                IsMatch(just_before, DescToken::Miss))
                PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::ChanceToMiss)

            if (IsMatch(just_after, DescToken::Faster))
            {
                constexpr std::array<DescToken, 5> allowed = {DescToken::Movement, DescToken::Attack, DescToken::Activate, DescToken::Cast, DescToken::Recharge};
                if (GetPropsBefore(allowed, [&](DescToken prop)
                                   {
                            switch (prop)
                            {
                                case DescToken::Movement:
                                    PushParam(ParsedSkillData::Type::FasterMovement);
                                    break;

                                case DescToken::Attack:
                                    PushParam(ParsedSkillData::Type::FasterAttacks);
                                    break;

                                case DescToken::Activate:
                                case DescToken::Cast:
                                    PushParam(ParsedSkillData::Type::FasterActivation);
                                    break;

                                case DescToken::Recharge:
                                    PushParam(ParsedSkillData::Type::FasterRecharge);
                                    break;
                            } }))
                    return;

                if (IsMatch(just_before, DescToken::Expire))
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::ShorterDuration)
            }

            if (IsMatch(just_after, DescToken::Slower))
            {
                if (IsMatch(just_before, DescToken::Recharge) ||
                    IsListMatch(just_before - 1, {DescToken::Recharge, DescToken::Skills}))
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::SlowerRecharge)

                if (IsMatch(just_before, DescToken::Movement) ||
                    IsMatch(just_after + 1, DescToken::Movement))
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::SlowerMovement)

                if (IsMatch(just_before, DescToken::Attack))
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::SlowerAttacks)
            }

            if (IsMatch(just_after, DescToken::Longer))
            {
                if (IsMatch(just_before, DescToken::Last))
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::LongerDuration)
            }

            if (IsMatch(just_after, DescToken::Damage))
            {
                if (is_less)
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::LessDamage)
                else if (is_more)
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::MoreDamage)
            }

            if (IsMatch(just_after, DescToken::Heal))
            {
                if (is_less)
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::LessHealing)
            }
            if (is_reduced)
            {
                for (int32_t i = just_before; i >= 0; i--)
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescToken::Heal, ParsedSkillData::Type::LessHealing)
                }
            }
        }
        else // No % after literal
        {
            IF_MATCH_PUSH_PARAM_AND_RETURN(just_before, DescToken::Level, ParsedSkillData::Type::Level)

            if (IsMatch(just_before, DescToken::Max))
            {
                if (IsMatch(just_before - 1, DescToken::Damage) ||
                    IsMatch(just_after, DescToken::Damage))
                {
                    if (IsMatch(just_after + 1, DescToken::Reduce))
                        PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::LessDamage)

                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::Damage)
                }
            }

            if (IsMatch(just_before, DescToken::For))
            {
                for (int32_t i = just_before - 1; i >= 0; i--)
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescToken::Heal, ParsedSkillData::Type::Heal)
                }
            }

            if (IsMatch(just_after, DescToken::Seconds))
            {
                if (IsMatch(just_after + 1, DescToken::Lifespan) ||
                    IsMatch(just_before, DescToken::Lifespan) ||
                    IsListMatch(just_before - 1, {DescToken::Die, DescToken::After}))
                {
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::Duration);
                }

                if ((IsMatch(just_before, DescToken::Recharge) && IsMatch(just_after + 1, DescToken::Faster)))
                {
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::FasterRecharge);
                }

                if (is_more && IsMatch(just_after + 1, DescToken::Recharge))
                {
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::SlowerRecharge);
                }

                if (!IsMatch(just_before, DescToken::Max))
                {
                    FixedVector<ParsedSkillData::Type, 8> buffer;
                    for (int32_t i = just_before; i >= 0; i--)
                    {
                        if (IsMatch(i, DescToken::Seconds))
                            break;
                        // clang-format off
                        switch (sentence[i]) {
                            case DescToken::Disable: PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::Disable)

                            case DescToken::Cripple:      buffer.try_push(ParsedSkillData::Type::Crippled);     break;
                            case DescToken::Blind:        buffer.try_push(ParsedSkillData::Type::Blind);        break;
                            case DescToken::Weakness:     buffer.try_push(ParsedSkillData::Type::Weakness);     break;
                            case DescToken::CrackedArmor: buffer.try_push(ParsedSkillData::Type::CrackedArmor); break;
                            case DescToken::Daze:         buffer.try_push(ParsedSkillData::Type::Dazed);        break;
                            case DescToken::Bleed:        buffer.try_push(ParsedSkillData::Type::Bleeding);     break;
                            case DescToken::Poison:       buffer.try_push(ParsedSkillData::Type::Poison);       break;
                            case DescToken::Disease:      buffer.try_push(ParsedSkillData::Type::Disease);      break;
                            case DescToken::Burn:         buffer.try_push(ParsedSkillData::Type::Burning);      break;
                            case DescToken::DeepWound:    buffer.try_push(ParsedSkillData::Type::DeepWound);    break;
                        }
                        // clang-format on
                    }
                    if (buffer.size() > 0)
                    {
                        while (buffer.size() > 0)
                            PushParam(buffer.pop_back());
                        return;
                    }
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::Duration);
                }
            }

            if (IsMatch(just_after, DescToken::Armor))
            {
                if (is_less)
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::ArmorDecrease)
                else
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::ArmorIncrease)
            }

            if (IsListMatch(just_after, {DescToken::Max, DescToken::Health}) ||
                IsListMatch(0, {DescToken::Max, DescToken::Health}))
            {
                PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::AdditionalHealth)
            }

            if (IsMatch(just_after, DescToken::Health))
            {
                IF_MATCH_PUSH_PARAM_AND_RETURN(just_after + 1, DescToken::Regen, ParsedSkillData::Type::HealthRegen)
                IF_MATCH_PUSH_PARAM_AND_RETURN(just_after + 1, DescToken::Degen, ParsedSkillData::Type::HealthDegen)
                for (int32_t i = just_before; i >= 0; i--)
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescToken::Gain, ParsedSkillData::Type::HealthGain)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescToken::Lose, ParsedSkillData::Type::HealthLoss)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescToken::Steal, ParsedSkillData::Type::HealthSteal)
                }
            }

            if (IsMatch(just_after, DescToken::Energy) && !IsMatch(0, DescToken::Not))
            {
                IF_MATCH_PUSH_PARAM_AND_RETURN(just_after + 1, DescToken::Regen, ParsedSkillData::Type::EnergyRegen)
                IF_MATCH_PUSH_PARAM_AND_RETURN(just_after + 1, DescToken::Degen, ParsedSkillData::Type::EnergyDegen)
                IF_MATCH_PUSH_PARAM_AND_RETURN(just_after + 1, DescToken::Lose, ParsedSkillData::Type::EnergyLoss)

                if (is_less)
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::EnergyDiscount)

                for (int32_t i = just_before; i >= 0; i--)
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescToken::Gain, ParsedSkillData::Type::EnergyGain)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescToken::Lose, ParsedSkillData::Type::EnergyLoss)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescToken::Steal, ParsedSkillData::Type::EnergySteal)
                }
            }

            if (IsMatch(just_after, DescToken::Adrenaline))
            {
                for (int32_t i = just_before; i >= 0; i--)
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescToken::Gain, ParsedSkillData::Type::AdrenalineGain)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescToken::Lose, ParsedSkillData::Type::AdrenalineLoss)
                }
            }

            {
                auto dmg_type = GetDmgType(just_after);
                auto i = just_after;
                if (dmg_type)
                {
                    pd.damage_type = dmg_type;
                    i++;
                }
                if (IsMatch(i, DescToken::Damage))
                {
                    if (is_less || IsMatch(i + 1, DescToken::Reduce))
                        PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::LessDamage)

                    for (int32_t j = just_before; j >= 0; j--)
                    {
                        IF_MATCH_PUSH_PARAM_AND_RETURN(j, DescToken::Absorb, ParsedSkillData::Type::LessDamage)
                    }

                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::Damage)
                }
            }

            for (int32_t i = just_before; i >= 0; i--)
            {
                if (IsListMatch(i - 2, {DescToken::Gain, DescToken::Or, DescToken::Lose}))
                    continue;

                if ((IsMatch(i, DescToken::Transfer) ||
                     IsMatch(i, DescToken::Remove) ||
                     IsMatch(i, DescToken::Lose)) &&
                    !IsMatch(i - 1, DescToken::Whenever))
                {
                    if (IsMatch(just_after, DescToken::ConditionAndHex) ||
                        IsMatch(just_after, DescToken::ConditionsAndHexes))
                    {
                        PushParam(ParsedSkillData::Type::ConditionsRemoved);
                        PushParam(ParsedSkillData::Type::HexesRemoved);
                        return;
                    }
                    IF_MATCH_PUSH_PARAM_AND_RETURN(just_after, DescToken::Condition, ParsedSkillData::Type::ConditionsRemoved)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(just_after, DescToken::Hex, ParsedSkillData::Type::HexesRemoved)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(just_after, DescToken::Enchantment, ParsedSkillData::Type::EnchantmentsRemoved)
                }
            }
        }

        if (IsMatch(just_before, DescToken::Activate) ||
            IsMatch(just_before, DescToken::Cast) ||
            IsListMatch(just_before - 2, {DescToken::Activate, DescToken::And, DescToken::Recharge}))
        {
            if (IsListMatch(just_after, {DescToken::Seconds, DescToken::Faster}))
            {
                PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::FasterActivation);
            }
            else if (IsListMatch(just_after, {DescToken::Percent, DescToken::Faster}))
            {
                PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::FasterActivation);
            }
        }

        if (is_reduced)
        {
            for (int32_t i = just_before; i > 0; i--)
            {
                if (IsMatch(i, DescToken::Attribute))
                    break;

                if (IsMatch(i, DescToken::Damage))
                {
                    PUSH_PARAM_AND_RETURN(ParsedSkillData::Type::LessDamage)
                }
            }
        }
    }

    void ParseSkillData(CustomSkillData &cskill)
    {
        auto &parsed_data = cskill.parsed_data;
        ParsedSkillData pd = {};

        // Hardcoded cases
        switch (cskill.skill_id)
        {
            case SkillID::Unseen_Fury:
            {
                pd.type = ParsedSkillData::Type::Blind;
                pd.param = {3, 10};
                parsed_data.push_back(pd);
                pd.type = ParsedSkillData::Type::Duration;
                pd.param = {10, 30};
                parsed_data.push_back(pd);
                return;
            }

            case SkillID::Ursan_Roar_Blood_Washes_Blood:
                pd.param = {4, 4};
            case SkillID::Ursan_Roar:
            {
                if (!pd.param)
                    pd.param = {2, 5};

                pd.type = ParsedSkillData::Type::Duration;
                parsed_data.push_back(pd);

                pd.type = ParsedSkillData::Type::Weakness;
                parsed_data.push_back(pd);
                return;
            }

            case SkillID::Ash_Blast:
            {
                pd.type = ParsedSkillData::Type::Damage;
                pd.damage_type = DamageType::Earth;
                pd.param = {35, 65};
                parsed_data.push_back(pd);
                pd = {};
                pd.type = ParsedSkillData::Type::Duration;
                pd.param = 5;
                parsed_data.push_back(pd);
                pd.type = ParsedSkillData::Type::ChanceToMiss;
                pd.param = {20, 75};
                parsed_data.push_back(pd);
                return;
            }

            case SkillID::Crippling_Victory: // Has weird formatting for a skill which applies a condition
            {
                pd.type = ParsedSkillData::Type::Crippled;
                pd.param = {3, 8};
                parsed_data.push_back(pd);
                pd.type = ParsedSkillData::Type::Damage;
                pd.damage_type = DamageType::Earth;
                pd.param = {10, 30};
                parsed_data.push_back(pd);
                return;
            }

            case SkillID::Hungers_Bite:
            {
                pd.type = ParsedSkillData::Type::HealthSteal;
                pd.param = 30;
                parsed_data.push_back(pd);

                ParsedSkillData::Type types[] = {
                    ParsedSkillData::Type::Poison,
                    ParsedSkillData::Type::Bleeding,
                    ParsedSkillData::Type::Disease,
                    ParsedSkillData::Type::DeepWound,
                    ParsedSkillData::Type::Crippled,
                    ParsedSkillData::Type::Weakness,
                    ParsedSkillData::Type::Dazed,
                };
                pd.param = 15;
                for (auto type : types)
                {
                    pd.type = type;
                    parsed_data.push_back(pd);
                }
                return;
            }
        }

        auto desc = Text::GetTextProvider(GW::Constants::Language::English).GetRawDescription(cskill.skill_id, false);
        auto tokenized_desc = TokenizedDesc(desc);

        auto tokens = tokenized_desc.tokens;
        auto start = tokens.data();
        auto end = start + tokens.size();
        auto w = start;
        uint32_t lit_counter = 0;

        while (w < end)
        {
            if (w[0] == DescToken::End && w + 1 < end && w[1] == DescToken::Effect)
            {
                pd.type = ParsedSkillData::Type::EndEffect;
                parsed_data.push_back(pd);

                w += 2;
                continue;
            }

            if (*w == DescToken::Literal)
            {
                auto sentence_start = w;
                while (start < sentence_start && (sentence_start[-1] != DescToken::Period))
                    sentence_start--;
                auto sentence_end = w;
                while (sentence_end < end && (sentence_end[0] != DescToken::Period))
                    sentence_end++;

                auto sentence = std::span(sentence_start, sentence_end - sentence_start);
                auto lit_pos = w - sentence_start;
                auto pp = ParsedSkillData();
                auto lit = tokenized_desc.lits[lit_counter++];
                if (!lit.IsNull())
                {
                    pp.param = lit;
                    ParseParamContext(parsed_data, pp, sentence, lit_pos);
                }
            }
            w++;
        }
    }

    Utils::SkillContext GetSkillContext(GW::Skill &skill)
    {
        // clang-format off
        if (IsPolymockSkill(skill.skill_id))          return Utils::SkillContext::Polymock;
        if (IsRollerBeetleSkill(skill.skill_id))      return Utils::SkillContext::Rollerbeetle;
        if (IsDragonArenaSkill(skill.skill_id))       return Utils::SkillContext::DragonArena;
        if (IsYuletideSkill(skill.skill_id))          return Utils::SkillContext::Yuletide;
        if (IsAgentOfTheMadKingSkill(skill.skill_id)) return Utils::SkillContext::AgentOfTheMadKing;
        if (IsCandyCornInfantrySkill(skill.skill_id)) return Utils::SkillContext::CandyCornInfantry;
        if (IsBrawlingSkill(skill.skill_id))          return Utils::SkillContext::Brawling;
        if (IsCommandoSkill(skill.skill_id))          return Utils::SkillContext::Commando;
        if (IsJununduSkill(skill.skill_id))           return Utils::SkillContext::Junundu;
        if (IsSiegeDevourerSkill(skill.skill_id))     return Utils::SkillContext::SiegeDevourer;
        if (IsUrsanSkill(skill.skill_id))             return Utils::SkillContext::UrsanBlessing;
        if (IsRavenSkill(skill.skill_id))             return Utils::SkillContext::RavenBlessing;
        if (IsVolfenSkill(skill.skill_id))            return Utils::SkillContext::VolfenBlessing;
        if (IsGolemSkill(skill.skill_id))             return Utils::SkillContext::Golem;
        if (IsSpiritFormSkill(skill.skill_id))        return Utils::SkillContext::SpiritForm;
        if (IsGwenSkill(skill.skill_id))              return Utils::SkillContext::Gwen;
        if (IsKeiranSkill(skill.skill_id))            return Utils::SkillContext::KeiranThackeray;
        if (IsSaulSkill(skill.skill_id))              return Utils::SkillContext::SaulDAlessio;
        if (IsTuraiSkill(skill.skill_id))             return Utils::SkillContext::TuraiOssa;
        if (IsTogoSkill(skill.skill_id))              return Utils::SkillContext::Togo;

        if (IsEffectOnly(skill))                      return Utils::SkillContext::Null;
        else                                          return Utils::SkillContext::Null;
        // clang-format on
    }

    std::string_view DamageTypeToString(DamageType type)
    {
        // clang-format off
        switch (type) {
            case DamageType::Elemental: return "Elemental";
            case DamageType::Fire:      return "Fire";
            case DamageType::Lightning: return "Lightning";
            case DamageType::Earth:     return "Earth";
            case DamageType::Cold:      return "Cold";
            case DamageType::Physical:  return "Physical";
            case DamageType::Slashing:  return "Slashing";
            case DamageType::Piercing:  return "Piercing";
            case DamageType::Blunt:     return "Blunt";
            case DamageType::Chaos:     return "Chaos";
            case DamageType::Dark:      return "Dark";
            case DamageType::Holy:      return "Holy";
            case DamageType::Shadow:    return "Shadow";

            default:                    return "ERROR";
        }
        // clang-format on
    }

    SkillParam GetSkillParam(const GW::Skill &skill, uint32_t id)
    {
        // clang-format off
        switch (id) {
            case 0: return {skill.scale0, skill.scale15};
            case 1: return {skill.bonusScale0, skill.bonusScale15};
            case 2: return {skill.duration0, skill.duration15};
            default: {
                SOFT_ASSERT(false, L"Invalid skill param id: {}", id);
                return {0, 0};
            }
        }
        // clang-format on
    }

    void ParsedSkillData::ImGuiRender(int8_t attr_lvl, float width, std::span<uint16_t> hl)
    {
        auto str = this->ToStr();
        // ImGui::TextUnformatted(str.data(), str.data() + str.size());
        Utils::DrawMultiColoredText(str, 0, width, {}, hl);
        if (damage_type)
        {
            ImGui::SameLine();
            ImGui::TextUnformatted(" (");
            ImGui::SameLine();
            str = DamageTypeToString(damage_type.value());
            ImGui::TextUnformatted(str.data(), str.data() + str.size());
            ImGui::SameLine();
            ImGui::TextUnformatted(")");
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(": ");

        if (type > ParsedSkillData::Type::DISPLAY_AS_NEGATIVE_START &&
            type < ParsedSkillData::Type::DISPLAY_AS_NEGATIVE_END)
        {
            ImGui::SameLine();
            ImGui::TextUnformatted("-");
        }

        if (type > ParsedSkillData::Type::DISPLAY_AS_POSITIVE_START &&
            type < ParsedSkillData::Type::DISPLAY_AS_POSITIVE_END)
        {
            ImGui::SameLine();
            ImGui::TextUnformatted("+");
        }

        ImGui::SameLine();
        param.ImGuiRender(attr_lvl);

        if (is_percent)
        {
            ImGui::SameLine();
            ImGui::Text("%%");
        }
        else if (type > ParsedSkillData::Type::SECONDS_START &&
                 type < ParsedSkillData::Type::SECONDS_END)
        {
            ImGui::SameLine();
            ImGui::Text(" seconds");
        }
    }

    SkillID ParsedSkillData::GetCondition() const
    {
        // clang-format off
        switch (type) {
            case Type::Bleeding:     return SkillID::Bleeding;
            case Type::Blind:        return SkillID::Blind;
            case Type::Burning:      return SkillID::Burning;
            case Type::CrackedArmor: return SkillID::Cracked_Armor;
            case Type::Crippled:     return SkillID::Crippled;
            case Type::Dazed:        return SkillID::Dazed;
            case Type::DeepWound:    return SkillID::Deep_Wound;
            case Type::Disease:      return SkillID::Disease;
            case Type::Poison:       return SkillID::Poison;
            case Type::Weakness:     return SkillID::Weakness;
            
            default:                 return SkillID::No_Skill;
        }
        // clang-format on
    }

    RemovalMask ParsedSkillData::GetRemovalMask() const
    {
        // clang-format off
        switch (type) {
            case Type::HexesRemoved:        return RemovalMask::Hex;
            case Type::ConditionsRemoved:   return RemovalMask::Condition;
            case Type::EnchantmentsRemoved: return RemovalMask::Enchantment;
            
            default:                        return RemovalMask::Null;
        }
        // clang-format on
    }

    std::string_view ParsedSkillData::ToStr() const
    {
        // clang-format off
        switch (type) {
            case Type::Duration:          return "Duration";
            case Type::Level:             return "Level";
            case Type::Disable:           return "Disable";
            case Type::ArmorIncrease:     return "Armor bonus";
            case Type::ArmorDecrease:     return "Armor penalty";
            case Type::FasterMovement:    return "Faster movement";
            case Type::SlowerMovement:    return "Slower movement";
            case Type::FasterAttacks:     return "Faster attacks";
            case Type::SlowerAttacks:     return "Slower attacks";
            case Type::FasterRecharge:    return "Faster recharge";
            case Type::SlowerRecharge:    return "Slower recharge";
            case Type::FasterActivation:  return "Faster activation";
            case Type::SlowerActivation:  return "Slower activation";
            case Type::LongerDuration:    return "Longer duration";
            case Type::ShorterDuration:   return "Shorter duration";

            case Type::ConditionsRemoved:   return "Conditions removed";
            case Type::HexesRemoved:        return "Hexes removed";
            case Type::EnchantmentsRemoved: return "Enchantments removed";

            case Type::Heal:             return "Healing";
            case Type::MoreHealing:      return "More healing";
            case Type::LessHealing:      return "Less healing";

            case Type::Damage:           return "Damage";
            case Type::MoreDamage:       return "More damage";
            case Type::LessDamage:       return "Less damage";

            case Type::ChanceToBlock:    return "Block chance";
            case Type::ChanceToFail:     return "Failure chance";
            case Type::ChanceToMiss:     return "Miss chance";

            case Type::AdditionalHealth: return "Additional health";
            case Type::HealthRegen:      return "Health regeneration";
            case Type::HealthDegen:      return "Health degeneration";
            case Type::HealthGain:       return "Health gain";
            case Type::HealthLoss:       return "Health loss";
            case Type::HealthSteal:      return "Health steal";

            case Type::EnergyDiscount:   return "Energy discount";
            case Type::EnergyRegen:      return "Energy regeneration";
            case Type::EnergyDegen:      return "Energy degeneration";
            case Type::EnergyGain:       return "Energy gain";
            case Type::EnergyLoss:       return "Energy loss";
            case Type::EnergySteal:      return "Energy steal";

            case Type::AdrenalineGain:   return "Adrenaline gain";
            case Type::AdrenalineLoss:   return "Adrenaline loss";

            case Type::Bleeding:         return "Bleeding (s)";
            case Type::Blind:            return "Blind (s)";
            case Type::Burning:          return "Burning (s)";
            case Type::CrackedArmor:     return "Cracked Armor (s)";
            case Type::Crippled:         return "Crippled (s)";
            case Type::Dazed:            return "Dazed (s)";
            case Type::DeepWound:        return "Deep Wound (s)";
            case Type::Disease:          return "Disease (s)";
            case Type::Poison:           return "Poison (s)";
            case Type::Weakness:         return "Weakness (s)";

            case Type::Null:             return "";
            default:                     return "ERROR";
        }
        // clang-format on
    }

    namespace CustomSkillDataModule
    {
        void Initialize()
        {
            auto skills = GW::SkillbarMgr::GetSkills();
            assert(skills.data());

            for (auto &skill : skills)
            {
                const auto skill_id = skill.skill_id;
                auto &custom_sd = custom_skill_datas[(size_t)skill_id];
                custom_sd.skill_id = skill_id;
                custom_sd.skill = &skill;
                custom_sd.Init();
            }
        }

        std::span<CustomSkillData> GetSkills()
        {
            return custom_skill_datas;
        }

        CustomSkillData &GetCustomSkillData(SkillID skill_id)
        {
            assert((uint32_t)skill_id < GW::Constants::SkillMax);
            return custom_skill_datas[(uint32_t)skill_id];
        }
    }

    SkillParam DetermineBaseDuration(CustomSkillData &cskill)
    {
        auto &parsed_data = cskill.parsed_data;

        // Typically this param is the duration, but it may sometimes be condition duration or other.
        auto base_duration = cskill.GetSkillParam(2);

        // Special cases
        switch (cskill.skill_id)
        {
            case SkillID::Pious_Assault:
            case SkillID::Pious_Assault_PvP:
                return {0, 0};
            default:
            {
                if (cskill.tags.SpiritAttack)
                {
                    return {0, 0};
                }

                if (!base_duration.IsNull() &&
                    cskill.skill->type == SkillType::Attack)
                {
                    auto desc = Text::GetTextProvider(GW::Constants::Language::English).GetRawDescription(cskill.skill_id, true);
                    if (desc.contains("nock"))
                    {
                        return {0, 0};
                    }
                }
            }
        }

        if (!base_duration.IsNull())
        {
            // So we have to cross-check with the parsed params
            for (auto pd : parsed_data)
            {
                if (pd.type == ParsedSkillData::Type::Duration &&
                    pd.param == base_duration)
                {
                    return base_duration;
                }
            }

            // If no match, we only assume it's the duration as long as no other parsed param has the same value
            for (auto pd : parsed_data)
            {
                if (pd.type != ParsedSkillData::Type::Duration &&
                    pd.param == base_duration)
                {
                    return {0, 0};
                }
            }
        }

        return base_duration;
    }

    void DetermineEffects(CustomSkillData &cskill)
    {
        auto skill_id = cskill.skill_id;
        auto &skill = *cskill.skill;

        // Ignored skill types
        switch (skill.type)
        {
            case SkillType::Well:
            case SkillType::Ward:
            case SkillType::Trap:
            case SkillType::Ritual:
            case SkillType::Condition:
            case SkillType::Bounty:
            case SkillType::Disguise:
            case SkillType::Scroll:
            case SkillType::Environmental:
            case SkillType::EnvironmentalTrap:
            case SkillType::Passive:
            case SkillType::Title:
                return;
        }

        FixedVector<ParsedSkillData, 8> conditions, end_conditions, removals, end_removals;

        ParsedSkillData::Type stage = ParsedSkillData::Type::Null;
        for (ParsedSkillData pd : cskill.parsed_data)
        {
            if (pd.type == ParsedSkillData::Type::EndEffect)
            {
                stage = pd.type;
                continue;
            }

            bool success = true;

            if (pd.IsCondition())
            {
                if (stage == ParsedSkillData::Type::EndEffect)
                    success &= end_conditions.try_push(pd);
                else
                    success &= conditions.try_push(pd);
            }
            else if (pd.IsRemoval())
            {
                if (stage == ParsedSkillData::Type::EndEffect)
                    success &= end_removals.try_push(pd);
                else
                    success &= removals.try_push(pd);
            }

            assert(success);
        }

        StaticSkillEffect effect = {};

        auto MakeAndPushEffect = [&](std::vector<StaticSkillEffect> &dst, std::span<ParsedSkillData> src)
        {
            for (const auto &pd : src)
            {
                effect.duration_or_count = pd.param;
                if (pd.IsCondition())
                {
                    effect.skill_id_or_removal = pd.GetCondition();
                    dst.push_back(effect);
                }
                else if (pd.IsRemoval())
                {
                    effect.skill_id_or_removal = pd.GetRemovalMask();
                    dst.push_back(effect);
                }
                else
                {
                    throw std::runtime_error("Unknown effect type");
                }
            }
        };

        auto MakeAndPushConditions = [&]()
        {
            MakeAndPushEffect(cskill.init_effects, conditions);
            MakeAndPushEffect(cskill.end_effects, end_conditions);
        };

        auto MakeAndPushRemovals = [&]()
        {
            MakeAndPushEffect(cskill.init_effects, removals);
            MakeAndPushEffect(cskill.end_effects, end_removals);
        };

        auto DetermineConditions = [&]()
        {
            effect = {};

            switch (skill_id) // CONDITIONS
            {
                // Skills inflicting initial conditions on foes around target
                case SkillID::Youre_All_Alone:
                case SkillID::Throw_Dirt:
                case SkillID::Barbed_Signet:
                case SkillID::Oppressive_Gaze:
                case SkillID::Enfeebling_Blood:
                case SkillID::Enfeebling_Blood_PvP:
                case SkillID::Weaken_Armor:
                case SkillID::Signet_of_Weariness:
                case SkillID::Ride_the_Lightning:
                case (SkillID)2807: // Ride the lightning (PvP)
                case SkillID::Blinding_Surge:
                case SkillID::Blinding_Surge_PvP:
                case SkillID::Thunderclap:
                case SkillID::Lightning_Touch:
                case SkillID::Teinais_Crystals:
                case SkillID::Eruption:
                case SkillID::Mind_Burn:
                case SkillID::UNUSED_Searing_Flames:
                case SkillID::Star_Burst:
                case SkillID::Burning_Speed:
                case SkillID::Searing_Flames:
                case SkillID::Rodgorts_Invocation:
                case SkillID::Caltrops:
                case SkillID::Blinding_Powder:
                case SkillID::Unseen_Fury: // Only stance that applies a condition on activation
                case SkillID::Holy_Spear:
                case SkillID::Finish_Him:
                case SkillID::You_Are_All_Weaklings:
                case SkillID::You_Move_Like_a_Dwarf:
                case SkillID::Maddening_Laughter:
                case SkillID::Queen_Wail:
                case SkillID::REMOVE_Queen_Wail:
                case SkillID::Raven_Shriek:
                case SkillID::Raven_Shriek_A_Gate_Too_Far:
                case SkillID::Ursan_Roar:
                case SkillID::Ursan_Roar_Blood_Washes_Blood:
                case SkillID::Armor_of_Sanctity:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Target;
                    effect.radius = (Utils::Range)skill.aoe_range;

                    MakeAndPushConditions();
                    return;
                }

                // Skills inflicting condition on self
                case SkillID::Headbutt:
                case SkillID::Signet_of_Agony:
                case SkillID::Signet_of_Agony_PvP:
                case SkillID::Blood_Drinker:
                case SkillID::Chilblains:
                case SkillID::Shadow_Sanctuary_kurzick:
                case SkillID::Shadow_Sanctuary_luxon:
                case SkillID::Wearying_Spear:
                {
                    effect.mask = EffectMask::Caster;
                    effect.location = EffectLocation::Caster;

                    MakeAndPushConditions();
                    return;
                }

                case SkillID::Signet_of_Suffering:
                {
                    effect.mask = EffectMask::Caster;
                    effect.location = EffectLocation::Caster;
                    effect.skill_id_or_removal = SkillID::Bleeding;
                    effect.duration_or_count = {6, 6};
                    cskill.init_effects.push_back(effect);
                }

                case SkillID::Shockwave:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Caster;
                    effect.duration_or_count = {1, 10};

                    effect.radius = Utils::Range::Nearby;
                    effect.skill_id_or_removal = SkillID::Cracked_Armor;
                    cskill.init_effects.push_back(effect);

                    effect.radius = Utils::Range::InTheArea;
                    effect.skill_id_or_removal = SkillID::Weakness;
                    cskill.init_effects.push_back(effect);

                    effect.radius = Utils::Range::Adjacent;
                    effect.skill_id_or_removal = SkillID::Blind;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case SkillID::Stone_Sheath:
                {
                    effect.mask = EffectMask::Foes;
                    effect.radius = Utils::Range::Nearby;
                    effect.skill_id_or_removal = SkillID::Weakness;
                    effect.duration_or_count = {5, 20};

                    effect.location = EffectLocation::Caster;
                    cskill.init_effects.push_back(effect);

                    effect.location = EffectLocation::Target;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case SkillID::Poisoned_Heart:
                {
                    effect.mask = EffectMask::Foes | EffectMask::Caster;
                    effect.location = EffectLocation::Caster;
                    effect.radius = (Utils::Range)skill.aoe_range;

                    MakeAndPushConditions();
                    return;
                }

                case SkillID::Earthen_Shackles:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Target;

                    MakeAndPushEffect(cskill.end_effects, conditions);
                }

                case SkillID::Signet_of_Midnight:
                {
                    effect.mask = EffectMask::Foes | EffectMask::Caster;
                    effect.skill_id_or_removal = SkillID::Blind;

                    effect.location = EffectLocation::Caster;
                    cskill.init_effects.push_back(effect);

                    effect.location = EffectLocation::Target;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // Conditions on foes around caster
                case SkillID::Return:
                case SkillID::Vipers_Defense:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Caster;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    MakeAndPushConditions();
                    return;
                }

                // Conditions on foes around spirit-ally closest to target
                case SkillID::Rupture_Soul:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::SpiritAllyClosestToTarget;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    MakeAndPushConditions();
                    return;
                }

                // Discards / Handled elsewhere
                case SkillID::Swift_Chop:
                case SkillID::Seeking_Blade:
                case SkillID::Drunken_Blow:     // We discard this because of randomness
                case SkillID::Desperation_Blow: // We discard this because of randomness
                case SkillID::Hungers_Bite:     // We discard this because of randomness
                case SkillID::Deflect_Arrows:
                case SkillID::Strike_as_One:
                case SkillID::Putrid_Flesh:
                case SkillID::Necrotic_Traversal:
                case SkillID::Ulcerous_Lungs:
                case SkillID::Fevered_Dreams:
                case SkillID::Ineptitude:
                case SkillID::Illusion_of_Haste:
                case SkillID::Illusion_of_Haste_PvP:
                case SkillID::Double_Dragon:
                case SkillID::Elemental_Flame:
                case SkillID::Elemental_Flame_PvP:
                case SkillID::Mark_of_Rodgort:
                case SkillID::Bed_of_Coals:
                case SkillID::Searing_Heat:
                case SkillID::Augury_of_Death:
                case SkillID::Shadow_Fang:
                case SkillID::UNUSED_Hidden_Caltrops:
                case SkillID::Hidden_Caltrops:
                case SkillID::Smoke_Powder_Defense:
                case SkillID::Sharpen_Daggers:
                case SkillID::Shadowsong:
                case SkillID::Shadowsong_Master_Riyo:
                case SkillID::Shadowsong_PvP:
                case SkillID::Sundering_Weapon:
                case SkillID::Blind_Was_Mingson:
                case SkillID::Weapon_of_Shadow:
                case SkillID::Spirit_Rift:
                case SkillID::UNUSED_Anthem_of_Weariness:
                case SkillID::Anthem_of_Weariness:
                case SkillID::Crippling_Anthem:
                case SkillID::Find_Their_Weakness:
                case SkillID::Find_Their_Weakness_PvP:
                case SkillID::Find_Their_Weakness_Thackeray:
                case SkillID::Anthem_of_Flame:
                case SkillID::Blazing_Finale:
                case SkillID::Blazing_Finale_PvP:
                case SkillID::Burning_Refrain:
                case SkillID::Burning_Shield:
                case SkillID::Cautery_Signet:
                case SkillID::Wearying_Strike:
                case SkillID::Grenths_Grasp:
                case SkillID::REMOVE_Wind_Prayers_skill:
                case SkillID::Attackers_Insight:
                case SkillID::Rending_Aura:
                case SkillID::Signet_of_Pious_Restraint:
                case SkillID::Test_of_Faith:
                case SkillID::Ebon_Dust_Aura_PvP:
                case SkillID::Shield_of_Force:
                    return;

                default:
                {
                    if (!cskill.tags.ConditionSource)
                        return;

                    if (skill.profession == GW::Constants::ProfessionByte::Dervish &&
                        skill.special & (uint32_t)Utils::SkillSpecialFlags::FlashEnchantment)
                    {
                        effect.location = EffectLocation::Caster;
                        effect.mask = EffectMask::Foes;
                        effect.radius = (Utils::Range)skill.aoe_range;
                        MakeAndPushConditions();
                        return;
                    }

                    switch (skill.type)
                    {
                        case SkillType::Enchantment:
                        case SkillType::PetAttack:
                        case SkillType::Preparation:
                        case SkillType::Glyph:
                        case SkillType::Form:
                        case SkillType::Well:
                        case SkillType::Ward:
                            return;
                    }

                    if (skill.type == SkillType::Attack ||
                        skill.type == SkillType::Spell ||
                        skill.type == SkillType::Hex ||
                        skill.type == SkillType::Signet ||
                        skill.type == SkillType::Skill ||
                        skill.type == SkillType::Skill2 ||
                        skill.special & (uint32_t)Utils::SkillSpecialFlags::Touch)
                    {
                        effect.location = EffectLocation::Target;
                        effect.mask = EffectMask::Target;
                        MakeAndPushConditions();
                        return;
                    }

                    SOFT_ASSERT(false, L"Unhandled condition source skill: {} ({})", (uint32_t)skill_id, Utils::GetSkillName(skill_id));
                }
            }
        };
        auto DetermineDuration = [&]()
        {
            effect = {};
            effect.skill_id_or_removal = skill_id;
            effect.duration_or_count = cskill.base_duration;

            switch (skill_id) // DURATIONAL SKILLS
            {
                // Skills affecting both caster and pet
                case SkillID::Never_Rampage_Alone:
                case SkillID::Run_as_One:
                case SkillID::Rampage_as_One:
                case SkillID::Strike_as_One:
                {
                    effect.mask = EffectMask::CasterAndPet;
                    effect.location = EffectLocation::Null;
                    effect.radius = Utils::Range::CompassRange;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // Affects allies
                case SkillID::Dwaynas_Sorrow:
                    effect.radius = Utils::Range::Nearby;
                case SkillID::Charge:
                case SkillID::Storm_of_Swords:
                case SkillID::Cant_Touch_This:
                case SkillID::Fall_Back:
                case SkillID::Fall_Back_PvP:
                case SkillID::Go_for_the_Eyes:
                case SkillID::Go_for_the_Eyes_PvP:
                case SkillID::Godspeed:
                case SkillID::The_Power_Is_Yours:
                case SkillID::Angelic_Bond:
                case SkillID::Its_Good_to_Be_King:
                case SkillID::Advance:
                case SkillID::Song_of_the_Mists:
                case SkillID::Enemies_Must_Die:
                case SkillID::Rands_Attack:
                case SkillID::Lets_Get_Em:
                case SkillID::Cry_of_Madness:
                case SkillID::Ursan_Roar:
                case SkillID::Ursan_Roar_Blood_Washes_Blood:
                case SkillID::Volfen_Bloodlust:
                case SkillID::Volfen_Bloodlust_Curse_of_the_Nornbear:
                case SkillID::Theres_Nothing_to_Fear_Thackeray:
                case SkillID::Natures_Blessing:
                case SkillID::For_Elona:
                case SkillID::Form_Up_and_Advance:
                {
                    effect.mask = EffectMask::Allies;
                    effect.location = EffectLocation::Target;
                    if (!effect.radius)
                        effect.radius = (Utils::Range)skill.aoe_range;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case SkillID::Save_Yourselves_kurzick:
                case SkillID::Save_Yourselves_luxon:
                {
                    effect.mask = EffectMask::OtherPartyMembers;
                    effect.location = EffectLocation::Caster;
                    effect.radius = Utils::Range::Earshot;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // AOE hexes
                case SkillID::Parasitic_Bite:
                case SkillID::Scourge_Sacrifice:
                case SkillID::Life_Transfer:
                case SkillID::Blood_Bond:
                case SkillID::Lingering_Curse:
                case SkillID::Suffering:
                case SkillID::Reckless_Haste:
                case SkillID::Meekness:
                case SkillID::Ulcerous_Lungs:
                case SkillID::Vocal_Minority:
                case SkillID::Shadow_of_Fear:
                case SkillID::Stolen_Speed:
                case SkillID::Stolen_Speed_PvP:
                case SkillID::Shared_Burden:
                case SkillID::Shared_Burden_PvP:
                case SkillID::Air_of_Disenchantment:
                case SkillID::Ineptitude:
                case SkillID::Arcane_Conundrum:
                case SkillID::Clumsiness:
                case SkillID::Fragility:
                case SkillID::Soothing_Images:
                case SkillID::Visions_of_Regret:
                case SkillID::Visions_of_Regret_PvP:
                case SkillID::Panic:
                case SkillID::Panic_PvP:
                case SkillID::Earthen_Shackles:
                case SkillID::Ash_Blast:
                case SkillID::Mark_of_Rodgort:
                case SkillID::Blurred_Vision:
                case SkillID::Deep_Freeze:
                case SkillID::Ice_Spikes:
                case SkillID::Rust:
                case SkillID::Binding_Chains:
                case SkillID::Dulled_Weapon:
                case SkillID::Lamentation:
                case SkillID::Painful_Bond:
                case SkillID::Isaiahs_Balance:
                case SkillID::Snaring_Web:
                case SkillID::Spirit_World_Retreat:
                case SkillID::Corsairs_Net:
                case SkillID::Crystal_Haze:
                case SkillID::Icicles:
                case SkillID::Shared_Burden_Gwen:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Target;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // AOE hexes around caster
                case SkillID::Amity:
                case SkillID::Grasping_Earth:
                case SkillID::Frozen_Burst:
                case SkillID::Wurm_Bile:
                case SkillID::Suicidal_Impulse:
                case SkillID::Last_Rites_of_Torment:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Caster;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case SkillID::Mirror_of_Ice:
                {
                    effect.mask = EffectMask::Foes;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    effect.location = EffectLocation::AllyClosestToTarget;
                    cskill.init_effects.push_back(effect);
                    effect.location = EffectLocation::Caster;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // Party wide skills
                case SkillID::Retreat:
                case SkillID::Celestial_Stance:
                case SkillID::Celestial_Haste:
                case SkillID::Order_of_the_Vampire:
                case SkillID::Order_of_Pain:
                case SkillID::Dark_Fury:
                case SkillID::Order_of_Apostasy:
                    effect.radius = Utils::Range::CompassRange;
                case SkillID::Shields_Up:
                case SkillID::Watch_Yourself:
                case SkillID::Watch_Yourself_PvP:
                case SkillID::Aegis:
                case SkillID::Shield_Guardian:
                case SkillID::Magnetic_Surge: // The desc is wrong: this is a party wide skill
                case SkillID::Magnetic_Aura:
                case SkillID::Swirling_Aura:
                case SkillID::Never_Surrender:
                case SkillID::Never_Surrender_PvP:
                case SkillID::Stand_Your_Ground:
                case SkillID::Stand_Your_Ground_PvP:
                case SkillID::We_Shall_Return_PvP:
                case SkillID::Theyre_on_Fire:
                case SkillID::Theres_Nothing_to_Fear:
                case SkillID::By_Urals_Hammer:
                case SkillID::Dont_Trip:
                {
                    effect.mask = EffectMask::PartyMembers;
                    effect.location = EffectLocation::Caster;
                    if (!effect.radius)
                        effect.radius = Utils::Range::Earshot;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case SkillID::Together_as_one:
                {
                    effect.mask = EffectMask::PartyMembers | EffectMask::PartyPets;
                    effect.radius = Utils::Range::InTheArea;

                    effect.location = EffectLocation::Caster;
                    cskill.init_effects.push_back(effect);

                    effect.location = EffectLocation::Pet;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case SkillID::Weight_of_Dhuum:
                {
                    effect.skill_id_or_removal = SkillID::Weight_of_Dhuum_hex;
                    effect.mask = EffectMask::Target;
                    effect.location = EffectLocation::Target;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // Discards / Handled elsewhere
                case SkillID::Arcane_Mimicry:
                case SkillID::Jaundiced_Gaze:
                case SkillID::Corrupt_Enchantment:
                case SkillID::Flurry_of_Splinters:
                case (SkillID)3001:
                case SkillID::Brutal_Mauling:
                case SkillID::Frost_Vortex:
                    return;

                default:
                {
                    if (cskill.base_duration.IsNull())
                        return;

                    // Skills affecting only pet
                    if (skill.attribute == GW::Constants::AttributeByte::BeastMastery)
                    {
                        switch (skill.type)
                        {
                            case SkillType::Shout:
                            case SkillType::Skill:
                            case SkillType::Skill2:
                            case SkillType::PetAttack:
                            {
                                effect.mask = EffectMask::CastersPet;
                                effect.location = EffectLocation::Caster;
                                effect.radius = Utils::Range::CompassRange;
                                cskill.init_effects.push_back(effect);
                                return;
                            }
                        }
                    }

                    switch (skill.type)
                    {
                        case SkillType::ItemSpell:
                        case SkillType::Preparation:
                        case SkillType::Form:
                        case SkillType::Stance:
                        case SkillType::Glyph:
                        {
                            effect.mask = EffectMask::Caster;
                            effect.location = EffectLocation::Caster;
                            cskill.init_effects.push_back(effect);
                            return;
                        }

                        case SkillType::Shout:
                        case SkillType::Signet:
                        case SkillType::WeaponSpell:
                        case SkillType::Enchantment:
                        case SkillType::Hex:
                        case SkillType::Spell:
                        case SkillType::Skill:
                        case SkillType::Skill2:
                        case SkillType::EchoRefrain:
                        {
                            effect.mask = EffectMask::Target;
                            effect.location = EffectLocation::Target;
                            cskill.init_effects.push_back(effect);
                            return;
                        }

                        case SkillType::Chant:
                        {
                            auto desc = Text::GetTextProvider(GW::Constants::Language::English).GetRawDescription(skill_id, true);

                            effect.location = EffectLocation::Caster;
                            effect.radius = Utils::Range::Earshot;
                            if (desc.contains("arty members"))
                                effect.mask = EffectMask::PartyMembers;
                            else
                                effect.mask = EffectMask::Allies;
                            cskill.init_effects.push_back(effect);
                            return;
                        }
                    }

                    SOFT_ASSERT(!cskill.init_effects.empty(), L"Unhandled durational skill: {} ({})", (uint32_t)skill_id, Utils::GetSkillName(skill_id));
                }
            }
        };
        auto DetermineRemovals = [&]()
        {
            effect = {};

            switch (skill_id) // REMOVALS
            {
                // Party wide
                case SkillID::Extinguish:
                case SkillID::Star_Shine:
                {
                    effect.mask = EffectMask::PartyMembers;
                    effect.location = EffectLocation::Caster;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    MakeAndPushRemovals();
                    return;
                }

                // AoE at target
                case SkillID::Withdraw_Hexes:
                case SkillID::Pure_Was_Li_Ming:
                    effect.mask = EffectMask::Allies;
                case SkillID::Chilblains:
                case SkillID::Air_of_Disenchantment:
                {
                    if (effect.mask == EffectMask::None)
                        effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Target;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    MakeAndPushRemovals();
                    return;
                }

                // Self removal
                case SkillID::Ether_Prodigy:
                case SkillID::Energy_Font:
                case SkillID::Second_Wind:
                {
                    effect.mask = EffectMask::Caster;
                    effect.location = EffectLocation::Caster;
                    MakeAndPushRemovals();
                    return;
                }

                case SkillID::Antidote_Signet:
                {
                    effect.mask = EffectMask::Target;
                    effect.location = EffectLocation::Target;
                    effect.skill_id_or_removal = RemovalMask::Poison | RemovalMask::Disease | RemovalMask::Blind;
                    effect.duration_or_count = std::numeric_limits<uint32_t>::max();
                    cskill.init_effects.push_back(effect);

                    effect.skill_id_or_removal = RemovalMask::Condition;
                    effect.duration_or_count = 1;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case SkillID::Crystal_Wave:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Caster;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    MakeAndPushRemovals();
                    return;
                }

                case SkillID::UNUSED_Empathic_Removal:
                case SkillID::Empathic_Removal:
                {
                    effect.mask = EffectMask::Target;
                    effect.location = EffectLocation::Target;
                    MakeAndPushRemovals();

                    effect.mask = EffectMask::Caster;
                    effect.location = EffectLocation::Caster;
                    MakeAndPushRemovals();
                    return;
                }

                // Discards / Handled elsewhere
                case SkillID::Spotless_Mind:
                case SkillID::Spotless_Soul:
                case SkillID::Divert_Hexes:
                case SkillID::Purifying_Veil:
                case SkillID::Draw_Conditions:
                case SkillID::Peace_and_Harmony:
                case SkillID::Contemplation_of_Purity:
                case SkillID::Deny_Hexes:
                case SkillID::Martyr:
                case SkillID::Holy_Veil:
                case SkillID::Veratas_Sacrifice:
                case SkillID::Well_of_the_Profane:
                case SkillID::UNUSED_Foul_Feast:
                case SkillID::Foul_Feast:
                case SkillID::Order_of_Apostasy:
                case SkillID::Plague_Signet:
                case SkillID::UNUSED_Plague_Sending:
                case SkillID::Plague_Sending:
                case SkillID::Plague_Touch:
                case SkillID::Hex_Eater_Vortex:
                case SkillID::Shatter_Delusions:
                case SkillID::Shatter_Delusions_PvP:
                case SkillID::Drain_Delusions:
                case SkillID::Hex_Eater_Signet:
                case SkillID::Hypochondria:
                case SkillID::Dark_Apostasy:
                case SkillID::Assassins_Remedy:
                case SkillID::Assassins_Remedy_PvP:
                case SkillID::Signet_of_Malice:
                case SkillID::Signet_of_Twilight:
                case SkillID::Lift_Enchantment:
                case SkillID::Disenchantment:
                case SkillID::Disenchantment_PvP:
                case SkillID::Weapon_of_Remedy:
                    return;

                default:
                {
                    effect.mask = EffectMask::Target;
                    effect.location = EffectLocation::Target;

                    bool is_end_effect = false;
                    for (auto &pd : cskill.parsed_data)
                    {
                        if (pd.type == ParsedSkillData::Type::EndEffect)
                            is_end_effect = true;

                        RemovalMask removal_mask = pd.GetRemovalMask();
                        if (removal_mask != RemovalMask::Null)
                        {
                            effect.skill_id_or_removal = removal_mask;
                            effect.duration_or_count = pd.param;

                            if (is_end_effect)
                                cskill.end_effects.push_back(effect);
                            else
                                cskill.init_effects.push_back(effect);
                        }
                    }
                }
            }
        };
        DetermineConditions();
        DetermineDuration();
        DetermineRemovals();
    }

    void CustomSkillData::Init()
    {
        context = GetSkillContext(*skill);

        // clang-format off
        if (IsArchivedSkill(*this))        tags.Archived = true;
        if (IsEffectOnly(*skill))          tags.EffectOnly = true;
        if (IsDeveloperSkill(*skill))      tags.DeveloperSkill = true;
        if (IsMonsterSkill(*skill))        tags.MonsterSkill = true;
        if (IsEnvironmentSkill(*skill))    tags.EnvironmentSkill = true;
        if (IsSpiritAttackSkill(*skill))   tags.SpiritAttack = true;
        if (IsPvEOnlySkill(*skill))        tags.PvEOnly = true;
        if (IsPvPOnlySkill(*skill))        tags.PvPOnly = true;
        if (skill->IsPvP())                tags.PvPVersion = true;
        if (skill->skill_id_pvp < SkillID::Count &&
           !skill->IsPvP())                tags.PvEVersion = true;
        if (IsConsumableItemSkill(*skill)) tags.Consumable = true;
        if (IsMaintainedSkill(*skill))     tags.Maintained = true;
        if (IsMissionSkill(skill_id))      tags.Mission = true;
        if (IsCelestialSkill(skill_id))    tags.Celestial = true;
        if (IsBundleSkill(skill_id))       tags.Bundle = true;
        if (IsBounty(*skill))              tags.Bounty = true;

        if (IsSpellSkill(*skill))          tags.Spell = true;
        if (EndsOnIncDamage(skill_id))     tags.EndsOnIncDamage = true;
        if (IsProjectileSkill(*skill))     tags.Projectile = true;
        // clang-format on

        if (context != Utils::SkillContext::Null ||
            tags.Mission ||
            tags.Celestial ||
            tags.Bundle)
            tags.Temporary = true;

        if (skill->profession != GW::Constants::ProfessionByte::None &&
            !tags.EffectOnly &&
            !tags.DeveloperSkill &&
            !tags.MonsterSkill &&
            !tags.EnvironmentSkill &&
            !tags.SpiritAttack &&
            !tags.PvEOnly &&
            !tags.PvPOnly &&
            !tags.Consumable &&
            !tags.Temporary)
            tags.Unlockable = true;

        if (skill->special & (uint32_t)Utils::SkillSpecialFlags::ExploitsCorpse ||
            skill_id == SkillID::Well_of_Ruin ||
            skill_id == SkillID::Aura_of_the_Lich)
            tags.ExploitsCorpse = true;

        if ((tags.Projectile && skill_id != SkillID::Ice_Spear) ||
            skill->type == SkillType::Attack)
            tags.HitBased = true;

        ParseSkillData(*this);

        for (auto &pd : parsed_data)
        {
            if (pd.IsCondition())
            {
                tags.ConditionSource = true;
                break;
            }
        }

        renewal = Renewal::None;
        attribute = AttributeOrTitle(*skill);

        this->base_duration = DetermineBaseDuration(*this);
        DetermineEffects(*this);
    }

    SkillParam CustomSkillData::GetSkillParam(uint32_t id) const
    {
        return HerosInsight::GetSkillParam(*skill, id);
    }

    SkillParam CustomSkillData::GetParsedSkillParam(std::function<bool(const ParsedSkillData &)> predicate) const
    {
        for (const auto &pd : this->parsed_data)
        {
            if (predicate(pd))
                return pd.param;
        }
        return {0, 0};
    }

    void CustomSkillData::GetParsedSkillParams(ParsedSkillData::Type type, OutBuf<ParsedSkillData> result) const
    {
        for (const auto &pd : this->parsed_data)
        {
            if (pd.type == type)
                result.try_push(pd);
        }
    }

    void GetConditionsFromSpan(std::span<const ParsedSkillData> parsed_data, SkillID source_skill_id, uint8_t attr_lvl, OutBuf<SkillEffect> result)
    {
        bool success = true;
        for (const auto &pd : parsed_data)
        {
            const auto condition_skill_id = pd.GetCondition();
            if (condition_skill_id == SkillID::No_Skill)
                continue;
            success &= result.try_push({condition_skill_id, source_skill_id, pd.param.Resolve(attr_lvl)});
        }
        SOFT_ASSERT(success, L"Failed to push condition");
    }

    void CustomSkillData::GetInitConditions(uint8_t attr_lvl, OutBuf<SkillEffect> result) const
    {
        if (!tags.ConditionSource)
            return;

        GetConditionsFromSpan(GetInitParsedData(), skill_id, attr_lvl, result);
    }

    void CustomSkillData::GetEndConditions(uint8_t attr_lvl, OutBuf<SkillEffect> result) const
    {
        if (!tags.ConditionSource)
            return;

        GetConditionsFromSpan(GetEndParsedData(), skill_id, attr_lvl, result);
    }

    std::span<const ParsedSkillData> CustomSkillData::GetInitParsedData() const
    {
        std::span<const ParsedSkillData> full_span = this->parsed_data;
        uint32_t init_count = 0;
        for (auto &pp : full_span)
        {
            if (pp.type == ParsedSkillData::Type::EndEffect)
                break;
            ++init_count;
        }
        return full_span.subspan(0, init_count);
    }

    std::span<const ParsedSkillData> CustomSkillData::GetEndParsedData() const
    {
        std::span<const ParsedSkillData> full_span = this->parsed_data;
        uint32_t init_count = 0;
        for (auto &pp : full_span)
        {
            if (pp.type == ParsedSkillData::Type::EndEffect)
                break;
            ++init_count;
        }
        return full_span.subspan(init_count, full_span.size() - init_count);
    }

    std::string CustomSkillData::ToString() const
    {
        return std::format("CustomSkillData[{}]", static_cast<uint32_t>(skill_id));
    }

    bool CustomSkillData::IsFlash() const
    {
        return skill->special & (uint32_t)Utils::SkillSpecialFlags::FlashEnchantment;
    }

    bool CustomSkillData::IsAttack() const
    {
        return skill->type == SkillType::Attack;
    }

    bool CustomSkillData::IsRangedAttack() const
    {
        return IsAttack() && (skill->weapon_req & 70);
    }

    void CustomSkillData::OnSkillActivation(CustomAgentData &caster, uint32_t target_id)
    {
        auto caster_id = caster.agent_id;

        auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);
        const auto attr_lvl = caster.GetAttrLvlForSkill(custom_sd);

        if (custom_sd.tags.Spell)
        {
            if (caster.signet_of_illusions_charges > 0)
                caster.signet_of_illusions_charges--;
        }

        switch (skill_id)
        {
            case SkillID::Signet_of_Illusions:
            {
                caster.signet_of_illusions_charges = custom_sd.GetSkillParam(0).Resolve(attr_lvl);
                break;
            }

            case SkillID::Barrage:
            case SkillID::Volley:
            {
                EffectTracking::RemoveTrackers(caster_id, [](EffectTracking::EffectTracker &effect)
                                               {
                                                   return GW::SkillbarMgr::GetSkillConstantData(effect.skill_id)->type == SkillType::Preparation; //
                                               });
            }
        }

        FixedVector<SkillEffect, 18> skill_effects;

        // custom_sd.GetOnActivationEffects(caster, target_id, skill_effects);

        if (skill_effects.size() == 0)
            return;

        auto range = (float)custom_sd.GetAoE();

        std::vector<uint32_t> target_ids; // May become large, so we use a heap allocated buffer
        if (custom_sd.skill->type == SkillType::Enchantment &&
            Utils::GetAgentRelations(caster_id, target_id) == Utils::AgentRelations::Hostile)
        {
            // This handles some special enchantments that require a foe (e.g. "Vampiric Spirit")
            target_ids.push_back(caster_id);
        }
        else if (range)
        {
            const auto target_living = Utils::GetAgentLivingByID(target_id);
            const auto caster_living = Utils::GetAgentLivingByID(caster_id);
            if (target_living && caster_living)
            {
                // Add any agent in the aoe range of the target that has the same ally status as the caster and target
                const auto caster_alliance = caster_living->allegiance;
                const auto caster_target_ally_status = Utils::GetAgentRelations(caster_alliance, target_living->allegiance);
                auto Predicate = [&](GW::Agent &a)
                {
                    switch (skill_id)
                    {
                        case SkillID::Extend_Conditions:
                        case SkillID::Epidemic:
                        {
                            if (a.agent_id == caster_id)
                                return false;
                            break;
                        }
                    }

                    const auto a_living = a.GetAsAgentLiving();
                    if (a_living == nullptr)
                        return false;

                    return Utils::GetAgentRelations(caster_alliance, a_living->allegiance) == caster_target_ally_status;
                };
                // target_ids = Utils::ForAgentsInCircle(target_living->pos, range, Predicate);
            }
        }
        else
        {
            target_ids.push_back(target_id);
        }

        for (const auto t_id : target_ids)
        {
            auto target = Utils::GetAgentLivingByID(t_id);
            if (target == nullptr)
                continue;

            if (target->GetIsDead() || target->GetIsDeadByTypeMap())
                continue;

            // The target has an effect array,
            // i.e. receives effect added/removed events, skip it
            if (Utils::ReceivesStoCEffects(t_id))
                continue;

            std::span<const SkillEffect> skill_effects_span = skill_effects;

            switch (skill_id)
            {
                case SkillID::Shockwave:
                {
                    auto caster = GW::Agents::GetAgentByID(caster_id);
                    auto target = GW::Agents::GetAgentByID(t_id);
                    auto distance_sqrd = GW::GetSquareDistance(caster->pos, target->pos);
                    uint32_t n_effects;
                    if (distance_sqrd <= (float)Utils::RangeSqrd::Adjacent)
                        n_effects = 3;
                    else if (distance_sqrd <= (float)Utils::RangeSqrd::Nearby)
                        n_effects = 2;
                    else
                        n_effects = 1;
                    skill_effects_span = ((std::span<const SkillEffect>)skill_effects).subspan(0, n_effects);
                    break;
                }
            }

            for (const auto &data : skill_effects_span)
            {
                auto base_duration = data.base_duration;
                auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(data.effect_skill_id);
                auto duration = Utils::CalculateDuration(*custom_sd.skill, base_duration, caster_id);
#if _DEBUG
                // Utils::FormatToChat(0xffffff00, L"Base duration: {}", base_duration);
                // Utils::FormatToChat(0xffffff00, L"Calculated duration: {}", duration);
#endif
                EffectTracking::EffectTracker new_tracker = {};
                new_tracker.cause_agent_id = caster_id;
                new_tracker.skill_id = data.effect_skill_id;
                new_tracker.effect_id = 0; // We dont know the effect id
                new_tracker.attribute_level = attr_lvl;
                new_tracker.duration_sec = duration;

                EffectTracking::AddTracker(t_id, new_tracker);
            }
        }
    }

    void CustomSkillData::OnProjectileLaunched(CustomAgentData &caster, uint32_t target_id, OwnedProjectile &projectile)
    {
    }

    int8_t CustomSkillData::GetUpkeep() const
    {
        return IsMaintainedSkill(*this->skill) ? -1 : 0;
    }

    uint8_t CustomSkillData::GetOvercast() const
    {
        if (skill->special & (uint32_t)Utils::SkillSpecialFlags::Overcast)
            return skill->overcast;

        return 0;
    }

    uint32_t CustomSkillData::GetAdrenaline() const
    {
        return skill->adrenaline;
    }

    uint32_t CustomSkillData::GetAdrenalineStrikes() const
    {
        return std::ceil((double)skill->adrenaline / 25.0);
    }

    uint8_t CustomSkillData::GetEnergy() const
    {
        return skill->GetEnergyCost();
    }

    uint8_t CustomSkillData::GetSacrifice() const
    {
        return skill->health_cost;
    }

    uint32_t CustomSkillData::GetRecharge() const
    {
        return skill->recharge;
    }

    float CustomSkillData::GetActivation() const
    {
        return skill->activation;
    }

    float CustomSkillData::GetAftercast() const
    {
        return skill->aftercast;
    }

    Utils::Range CustomSkillData::GetAoE() const
    {
        if (Utils::IsRangeValue(skill->aoe_range))
            return (Utils::Range)skill->aoe_range;
        return Utils::Range::Null;
    }

    void CustomSkillData::GetRanges(OutBuf<Utils::Range> out) const
    {
        if (Utils::IsRangeValue(skill->aoe_range))
            out.push_back((Utils::Range)skill->aoe_range);
        if (Utils::IsRangeValue(skill->const_effect))
            out.push_back((Utils::Range)skill->const_effect);
        if (skill->bonusScale0 == skill->bonusScale15 && Utils::IsRangeValue(skill->bonusScale0))
            out.push_back((Utils::Range)skill->bonusScale0);
    }

    uint32_t CustomSkillData::ResolveBaseDuration(CustomAgentData &caster, std::optional<uint8_t> skill_attr_lvl_override) const
    {
        switch (skill_id) // Special cases
        {
            case SkillID::Cultists_Fervor:
            {
                auto soul_reaping = caster.GetOrEstimateAttribute(GW::Constants::AttributeByte::SoulReaping);
                return 5 + 3 * soul_reaping;
            }
        }

        if (this->base_duration)
        {
            auto attr_lvl = skill_attr_lvl_override ? skill_attr_lvl_override.value() : caster.GetAttrLvlForSkill(*this);
            auto base_duration = this->base_duration.Resolve(attr_lvl);

            switch (skill_id)
            {
                case SkillID::Lead_the_Way:
                {
                    auto caster_agent = Utils::GetAgentLivingByID(caster.agent_id);

                    if (caster_agent)
                    {
                        uint32_t n_allies = 0;
                        Utils::ForAlliesInCircle(caster_agent->pos, (float)Utils::Range::Earshot, caster_agent->allegiance, [&](GW::AgentLiving &agent)
                                                 { n_allies++; });
                        base_duration = std::min(n_allies * base_duration, 20u);
                    }
                }
            }

            return base_duration;
        }

        return 0;
    }

    FixedVector<std::string, 128> skill_type_strings;
    std::string_view CustomSkillData::GetTypeString()
    {
        if (type_str.data() != nullptr)
            return type_str;

        // We dont have the string cached, generate it

        auto skill_type = skill->type;
        if (skill_type == SkillType::Skill2)
        {
            skill_type = SkillType::Skill;
        }

        bool is_elite = skill->IsElite();
        bool is_half_range = skill->IsHalfRange();
        bool is_flash = IsFlash();
        bool is_touch = skill->IsTouchRange();

        std::string str = {};

        // clang-format off
        if (is_elite)                                   str += "Elite ";
        if (is_half_range)                              str += "Half Range ";
        if (is_flash)                                   str += "Flash ";
        if (is_touch)                                   str += "Touch ";
        switch (skill_type) {
            case SkillType::Bounty:      str += "Blessing";    break;
            case SkillType::Scroll:      str += "Party Bonus"; break;
            case SkillType::Stance:      str += "Stance";      break;
            case SkillType::Signet:      str += "Signet";      break;
            case SkillType::Condition:   str += "Condition";   break;
            case SkillType::Glyph:       str += "Glyph";       break;
            case SkillType::Shout:       str += "Shout";       break;
            case SkillType::Preparation: str += "Preparation"; break;
            case SkillType::Trap:        str += "Trap";        break;
            case SkillType::Form:        str += "Form";        break;
            case SkillType::Chant:       str += "Chant";       break;
            case SkillType::EchoRefrain: str += "Echo";        break;
            case SkillType::Disguise:    str += "Disguise";    break;

            case SkillType::Hex:         str += "Hex ";         goto spell;
            case SkillType::Enchantment: str += "Enchantment "; goto spell;
            case SkillType::Well:        str += "Well ";        goto spell;
            case SkillType::Ward:        str += "Ward ";        goto spell;
            case SkillType::WeaponSpell: str += "Weapon ";      goto spell;
            case SkillType::ItemSpell:   str += "Item ";        goto spell;
            case SkillType::Spell:
                spell:
                str += "Spell";
                break;

            case SkillType::Skill:
            // case SkillType::Skill2: if (is_effect_only) { str += "Effect"; } else { str += "Skill"; } break;
            case SkillType::Skill2: str += "Skill"; break;

            case SkillType::PetAttack: str += "Pet "; goto attack;
            case SkillType::Attack:
                switch (skill->weapon_req) {
                    case 1: str += "Axe "; break;
                    case 2: str += "Bow "; break;
                    case 8:
                        switch (skill->combo) {
                            case 1:  str += "Lead ";     break;
                            case 2:  str += "Off-Hand "; break;
                            case 3:  str += "Dual ";     break;
                            default: str += "Dagger ";   break;
                        }
                        break;
                    case 16:  str += "Hammer "; break;
                    case 32:  str += "Scythe "; break;
                    case 64:  str += "Spear ";  break;
                    case 128: str += "Sword ";  break;
                    case 70:  str += "Ranged "; break;
                    case 185: str += "Melee ";  break;
                }
                attack:
                str += "Attack";
                break;

            case SkillType::Title:             str += "Title ";              goto effect;
            case SkillType::Passive:           str += "Passive ";            goto effect;
            case SkillType::Environmental:     str += "Environmental ";      goto effect;
            case SkillType::EnvironmentalTrap: str += "Environmental Area ";
                effect:
                str += "Effect";
                break;

            case SkillType::Ritual:
                switch (skill->profession) {
                    case GW::Constants::ProfessionByte::Ritualist: str += "Binding "; break;
                    case GW::Constants::ProfessionByte::Ranger:    str += "Nature ";  break;
                    default:                                       str += "Ebon Vanguard "; break;
                }
                str += "Ritual";
                break;

            default: str += "UNKNOWN";
        }
        // clang-format on

        for (auto &cached_str : skill_type_strings)
        {
            if (cached_str == str)
            {
                type_str = cached_str;
                return type_str;
            }
        }

        if (skill_type_strings.try_push(std::move(str)))
        {
            type_str = skill_type_strings.back();
        }
        else
        {
            SOFT_ASSERT(false, L"Could not find a free slot for skill type string");
            type_str = "ERROR";
        }

        return type_str;
    }

    std::string_view CustomSkillData::GetProfessionStr()
    {
        if (profession_str.data() == nullptr)
        {
            profession_str = Utils::GetProfessionString(skill->profession);
        }

        return profession_str;
    }

    std::string_view CustomSkillData::GetCampaignStr()
    {
        if (campaign_str.data() == nullptr)
        {
            campaign_str = Utils::GetCampaignString(skill->campaign);
        }

        return campaign_str;
    }

    std::string_view CustomSkillData::GetAttributeStr()
    {
        if (attr_str.data() == nullptr)
        {
            attr_str = attribute.GetStr();
        }

        return attr_str;
    }

    bool StaticSkillEffect::IsAffected(uint32_t caster_id, uint32_t target_id, uint32_t candidate_agent_id) const
    {
        if (candidate_agent_id == caster_id)
        {
            // If the candidate is the caster, only apply if the mask has "Caster"
            return (bool)(mask & EffectMask::Caster);
        }

        if (candidate_agent_id == target_id)
        {
            // If the candidate is the target, only apply if the mask has "Target"
            return (bool)(mask & EffectMask::Target);
        }

        if ((bool)(mask & EffectMask::CastersPet) &&
            candidate_agent_id == Utils::GetPetOfAgent(caster_id))
            return true;

        if ((bool)(mask & EffectMask::OtherPartyMembers) &&
            Utils::InSameParty(caster_id, candidate_agent_id))
            return true;

        auto caster = Utils::GetAgentLivingByID(caster_id);
        auto candidate = Utils::GetAgentLivingByID(candidate_agent_id);
        if (caster && candidate)
        {
            auto caster_candidate_rel = Utils::GetAgentRelations(caster->allegiance, candidate->allegiance);

            if (caster_candidate_rel == Utils::AgentRelations::Friendly)
            {
                if ((mask & EffectMask::Allies) == EffectMask::Allies)
                    return true;

                GW::NPC *model = nullptr;
                if (candidate->agent_model_type & 0x2000)
                {
                    model = GW::Agents::GetNPCByID(candidate->player_number);
                }

                if (model)
                {
                    if ((bool)(mask & EffectMask::MinionAllies) && model->IsMinion())
                        return true;

                    if ((bool)(mask & EffectMask::SpiritAllies) && model->IsSpirit())
                        return true;

                    if ((bool)(mask & EffectMask::NonSpiritAllies) && !model->IsSpirit())
                        return true;
                }
            }
            else if (caster_candidate_rel == Utils::AgentRelations::Hostile)
            {
                if ((bool)(mask & EffectMask::OtherFoes))
                    return true;
            }
        }

        return false;
    }

    void StaticSkillEffect::Apply(uint32_t caster_id, uint32_t target_id, uint8_t attr_lvl, std::function<bool(GW::AgentLiving &)> predicate) const
    {
        uint32_t effect_target_id;
        switch (location)
        {
            case EffectLocation::Caster:
                effect_target_id = caster_id;
                break;

            case EffectLocation::Target:
                effect_target_id = target_id;
                break;

            case EffectLocation::Pet:
                effect_target_id = Utils::GetPetOfAgent(caster_id);
                if (!effect_target_id)
                    return;
                break;

            default:
                return;
        }

        auto caster = Utils::GetAgentLivingByID(caster_id);
        auto target = Utils::GetAgentLivingByID(target_id);
        auto effect_target = Utils::GetAgentLivingByID(effect_target_id);
        if (!caster || !target || !effect_target)
            return;

        auto ApplyToValidAgents = [&](auto &&applier)
        {
            auto TryApply = [&](GW::AgentLiving &candidate)
            {
                if (predicate && !predicate(candidate))
                    return;

                auto candidate_id = candidate.agent_id;
                if (IsAffected(caster_id, target_id, candidate_id))
                {
                    applier(candidate);
                }
            };

            if (radius == Utils::Range::Null)
            {
                TryApply(*effect_target);
            }
            else
            {
                Utils::ForAgentsInCircle(effect_target->pos, (float)(uint32_t)radius, TryApply);
            }
        };

        auto duration_or_count_resolved = duration_or_count.Resolve(attr_lvl);
        if (auto skill_id = std::get_if<SkillID>(&skill_id_or_removal))
        {
            auto base_duration = duration_or_count_resolved;
            auto &skill = *GW::SkillbarMgr::GetSkillConstantData(*skill_id);
            auto duration = Utils::CalculateDuration(skill, base_duration, caster_id);

            ApplyToValidAgents(
                [&](GW::AgentLiving &agent)
                {
                    EffectTracking::EffectTracker tracker = {};
                    tracker.cause_agent_id = caster_id;
                    tracker.skill_id = *skill_id;
                    tracker.duration_sec = duration;
                    tracker.attribute_level = attr_lvl;

                    EffectTracking::AddTracker(agent.agent_id, tracker);
                }
            );
        }
        else if (auto removal = std::get_if<RemovalMask>(&skill_id_or_removal))
        {
            auto count = duration_or_count_resolved;

            ApplyToValidAgents(
                [&](GW::AgentLiving &agent)
                {
                    EffectTracking::CondiHexEnchRemoval(agent.agent_id, *removal, count);
                }
            );
        }
        else
        {
            SOFT_ASSERT(false, L"Invalid skill_id_or_removal");
        }
    }

    std::wstring StaticSkillEffect::ToWString() const
    {
        std::wstring out = L"StaticSkillEffect: ";

        FixedVector<char, 64> buffer;
        duration_or_count.Print(-1, buffer);
        auto dur_or_count_string = Utils::StrToWStr(buffer);

        if (auto skill_id = std::get_if<SkillID>(&skill_id_or_removal))
        {
            out += std::format(L"skill_id={}, duration={}", (uint32_t)*skill_id, dur_or_count_string);
        }
        else if (auto removal = std::get_if<RemovalMask>(&skill_id_or_removal))
        {
            out += std::format(L"removal={}, count={}", (uint32_t)*removal, dur_or_count_string);
        }
        else
        {
        }

        return out;
    }
}