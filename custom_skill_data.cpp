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
#include <custom_agent_data.h>
#include <debug_display.h>
#include <effect_tracking.h>
#include <skill_book.h>
#include <utils.h>

#include "custom_skill_data.h"

namespace HerosInsight
{
    bool IsDeveloperSkill(const GW::Skill &skill)
    {
        constexpr auto dev_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::No_Skill,
            GW::Constants::SkillID::Test_Buff,
            GW::Constants::SkillID::Impending_Dhuum,
            GW::Constants::SkillID::Dishonorable,
            GW::Constants::SkillID::Unreliable);

        return skill.icon_file_id == 327614 || // Dev hax icon
               dev_skills.has(skill.skill_id);
    }

    bool IsArchivedSkill(CustomSkillData &custom_sd)
    {
        constexpr auto archived_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::UNUSED_Complicate,
            GW::Constants::SkillID::UNUSED_Reapers_Mark,
            GW::Constants::SkillID::UNUSED_Enfeeble,
            GW::Constants::SkillID::UNUSED_Desecrate_Enchantments,
            GW::Constants::SkillID::UNUSED_Signet_of_Lost_Souls,
            GW::Constants::SkillID::UNUSED_Insidious_Parasite,
            GW::Constants::SkillID::UNUSED_Searing_Flames,
            GW::Constants::SkillID::UNUSED_Glowing_Gaze,
            GW::Constants::SkillID::UNUSED_Steam,
            GW::Constants::SkillID::UNUSED_Flame_Djinns_Haste,
            GW::Constants::SkillID::UNUSED_Liquid_Flame,
            GW::Constants::SkillID::UNUSED_Blessed_Light,
            GW::Constants::SkillID::UNUSED_Shield_of_Absorption,
            GW::Constants::SkillID::UNUSED_Smite_Condition,
            GW::Constants::SkillID::UNUSED_Crippling_Slash,
            GW::Constants::SkillID::UNUSED_Sun_and_Moon_Slash,
            GW::Constants::SkillID::UNUSED_Enraging_Charge,
            GW::Constants::SkillID::UNUSED_Tiger_Stance,
            GW::Constants::SkillID::UNUSED_Burning_Arrow,
            GW::Constants::SkillID::UNUSED_Natural_Stride,
            GW::Constants::SkillID::UNUSED_Falling_Lotus_Strike,
            GW::Constants::SkillID::UNUSED_Anthem_of_Weariness,
            GW::Constants::SkillID::UNUSED_Pious_Fury,
            GW::Constants::SkillID::UNUSED_Amulet_of_Protection,
            GW::Constants::SkillID::UNUSED_Eviscerate,
            GW::Constants::SkillID::UNUSED_Rush,
            GW::Constants::SkillID::UNUSED_Lions_Comfort,
            GW::Constants::SkillID::UNUSED_Melandrus_Shot,
            GW::Constants::SkillID::UNUSED_Sloth_Hunters_Shot,
            GW::Constants::SkillID::UNUSED_Reversal_of_Damage,
            GW::Constants::SkillID::UNUSED_Empathic_Removal,
            GW::Constants::SkillID::UNUSED_Castigation_Signet,
            GW::Constants::SkillID::UNUSED_Wail_of_Doom,
            GW::Constants::SkillID::UNUSED_Rip_Enchantment,
            GW::Constants::SkillID::UNUSED_Foul_Feast,
            GW::Constants::SkillID::UNUSED_Plague_Sending,
            GW::Constants::SkillID::UNUSED_Overload,
            GW::Constants::SkillID::UNUSED_Wastrels_Worry,
            GW::Constants::SkillID::UNUSED_Lyssas_Aura,
            GW::Constants::SkillID::UNUSED_Empathy,
            GW::Constants::SkillID::UNUSED_Shatterstone,
            GW::Constants::SkillID::UNUSED_Glowing_Ice,
            GW::Constants::SkillID::UNUSED_Freezing_Gust,
            GW::Constants::SkillID::UNUSED_Glyph_of_Immolation,
            GW::Constants::SkillID::UNUSED_Glyph_of_Restoration,
            GW::Constants::SkillID::UNUSED_Hidden_Caltrops,
            GW::Constants::SkillID::UNUSED_Black_Spider_Strike,
            GW::Constants::SkillID::UNUSED_Caretakers_Charge,
            GW::Constants::SkillID::UNUSED_Signet_of_Mystic_Speed,
            GW::Constants::SkillID::UNUSED_Signet_of_Rage,
            GW::Constants::SkillID::UNUSED_Signet_of_Judgement,
            GW::Constants::SkillID::UNUSED_Vigorous_Spirit,

            GW::Constants::SkillID::REMOVE_Balthazars_Rage,
            GW::Constants::SkillID::REMOVE_Boon_of_the_Gods,
            GW::Constants::SkillID::REMOVE_Leadership_skill,
            GW::Constants::SkillID::REMOVE_Queen_Armor,
            GW::Constants::SkillID::Queen_Armor,
            GW::Constants::SkillID::REMOVE_Queen_Wail,
            GW::Constants::SkillID::Queen_Wail,
            GW::Constants::SkillID::REMOVE_Wind_Prayers_skill,
            GW::Constants::SkillID::REMOVE_With_Haste,
            (GW::Constants::SkillID)807, // REMOVE_Soul_Reaping_Skill,

            GW::Constants::SkillID::Forge_the_Way,
            GW::Constants::SkillID::Anthem_of_Aggression,
            GW::Constants::SkillID::Mantra_of_Celerity,
            GW::Constants::SkillID::Signet_of_Attainment,
            GW::Constants::SkillID::Accelerated_Growth,
            GW::Constants::SkillID::Aim_True,
            GW::Constants::SkillID::Aura_of_the_Great_Destroyer,
            GW::Constants::SkillID::Bloodletting,
            GW::Constants::SkillID::Borrowed_Energy,
            GW::Constants::SkillID::Charr_Buff,
            GW::Constants::SkillID::Command_of_Torment,
            GW::Constants::SkillID::Construct_Possession,
            GW::Constants::SkillID::Conviction_PvP,
            GW::Constants::SkillID::Cry_of_Lament,
            GW::Constants::SkillID::Destroy_the_Humans,
            GW::Constants::SkillID::Dissipation,
            GW::Constants::SkillID::Desperate_Howl,
            GW::Constants::SkillID::Dozen_Shot,
            GW::Constants::SkillID::Embrace_the_Pain,
            GW::Constants::SkillID::Empathy_Koro,
            GW::Constants::SkillID::Energy_Font,
            GW::Constants::SkillID::Everlasting_Mobstopper_skill,
            GW::Constants::SkillID::Fake_Spell,
            GW::Constants::SkillID::Fire_and_Brimstone,
            GW::Constants::SkillID::Gaze_of_MoavuKaal,
            GW::Constants::SkillID::Guild_Monument_Protected,
            GW::Constants::SkillID::Headshot,
            GW::Constants::SkillID::Ice_Skates,
            GW::Constants::SkillID::Intimidating_Aura_beta_version,
            GW::Constants::SkillID::Kraks_Charge,
            GW::Constants::SkillID::Lichs_Phylactery,
            GW::Constants::SkillID::Lightning_Storm,
            GW::Constants::SkillID::Marble_Trap,
            GW::Constants::SkillID::Mending_Shrine_Bonus,
            GW::Constants::SkillID::Natures_Speed,
            GW::Constants::SkillID::Oracle_Link,
            GW::Constants::SkillID::Recurring_Scourge,
            GW::Constants::SkillID::Rough_Current,
            GW::Constants::SkillID::Scepter_of_Ether,
            GW::Constants::SkillID::Shadow_Tripwire,
            GW::Constants::SkillID::Shiver_Touch,
            GW::Constants::SkillID::Shrine_Backlash,
            GW::Constants::SkillID::Signal_Flare,
            GW::Constants::SkillID::Star_Shards,
            GW::Constants::SkillID::Suicidal_Impulse,
            GW::Constants::SkillID::Summoning_of_the_Scepter,
            GW::Constants::SkillID::Sundering_Soulcrush,
            GW::Constants::SkillID::Sunspear_Siege,
            GW::Constants::SkillID::To_the_Death,
            GW::Constants::SkillID::Turbulent_Flow,
            GW::Constants::SkillID::Twisted_Spikes,
            GW::Constants::SkillID::Unlock_Cell,
            GW::Constants::SkillID::Vanish,
            GW::Constants::SkillID::Vital_Blessing_monster_skill,
            GW::Constants::SkillID::Way_of_the_Mantis,
            GW::Constants::SkillID::Weapon_of_Mastery,
            (GW::Constants::SkillID)2915, // NOX_Rectifier
            (GW::Constants::SkillID)20,   // Confusion,
            GW::Constants::SkillID::Signet_of_Illusions_beta_version,
            GW::Constants::SkillID::Mimic,
            GW::Constants::SkillID::Disappear,
            GW::Constants::SkillID::Unnatural_Signet_alpha_version,
            GW::Constants::SkillID::Unnatural_Signet_alpha_version,
            GW::Constants::SkillID::Dont_Believe_Their_Lies,
            GW::Constants::SkillID::Call_of_Ferocity,
            GW::Constants::SkillID::Call_of_Elemental_Protection,
            GW::Constants::SkillID::Call_of_Vitality,
            GW::Constants::SkillID::Call_of_Healing,
            GW::Constants::SkillID::Call_of_Resilience,
            GW::Constants::SkillID::Call_of_Feeding,
            GW::Constants::SkillID::Call_of_the_Hunter,
            GW::Constants::SkillID::Call_of_Brutality,
            GW::Constants::SkillID::Call_of_Disruption,
            GW::Constants::SkillID::High_Winds,
            GW::Constants::SkillID::Coming_of_Spring,
            GW::Constants::SkillID::Signet_of_Creation_PvP,
            GW::Constants::SkillID::Quickening_Terrain,
            GW::Constants::SkillID::Massive_Damage,
            (GW::Constants::SkillID)2781, // Minion Explosion
            GW::Constants::SkillID::Couriers_Haste,
            (GW::Constants::SkillID)2811, // Hive Mind
            (GW::Constants::SkillID)2812, // Blood Pact
            GW::Constants::SkillID::Inverse_Ninja_Law,
            (GW::Constants::SkillID)2814, // Keep yourself alive
            (GW::Constants::SkillID)2815, // ...
            (GW::Constants::SkillID)2816, // Bounty Hunter
            (GW::Constants::SkillID)2817, // ...
            (GW::Constants::SkillID)3275, // ...
            (GW::Constants::SkillID)3276, // ...
            (GW::Constants::SkillID)3277, // ...
            (GW::Constants::SkillID)3278, // ...
            (GW::Constants::SkillID)3279, // ...
            (GW::Constants::SkillID)3280, // ...
            (GW::Constants::SkillID)3281, // ...
            GW::Constants::SkillID::Victorious_Renewal,
            GW::Constants::SkillID::A_Dying_Curse,
            (GW::Constants::SkillID)3284, // ...
            (GW::Constants::SkillID)3285, // ...
            (GW::Constants::SkillID)3286, // ...
            (GW::Constants::SkillID)3287, // ...
            GW::Constants::SkillID::Rage_of_the_Djinn,
            GW::Constants::SkillID::Lone_Wolf,
            GW::Constants::SkillID::Stand_Together,
            GW::Constants::SkillID::Unyielding_Spirit,
            GW::Constants::SkillID::Reckless_Advance,
            GW::Constants::SkillID::Solidarity,
            GW::Constants::SkillID::Fight_Against_Despair,
            GW::Constants::SkillID::Deaths_Succor,
            GW::Constants::SkillID::Battle_of_Attrition,
            GW::Constants::SkillID::Fight_or_Flight,
            GW::Constants::SkillID::Renewing_Escape,
            GW::Constants::SkillID::Battle_Frenzy,
            GW::Constants::SkillID::The_Way_of_One,
            (GW::Constants::SkillID)3388, // ...
            (GW::Constants::SkillID)3389, // ...
            (GW::Constants::SkillID)656,  // Life draining on crit
            (GW::Constants::SkillID)2362, // Unnamed skill
            (GW::Constants::SkillID)2239, // Unnamed skill
            (GW::Constants::SkillID)3255, // Shield of the Champions
            (GW::Constants::SkillID)3256, // Shield of the Champions
            (GW::Constants::SkillID)3257, // Shield of the Champions
            (GW::Constants::SkillID)3258, // Shield of the Champions
            GW::Constants::SkillID::Pains_Embrace,
            GW::Constants::SkillID::Siege_Attack_Bombardment,
            GW::Constants::SkillID::Advance,
            GW::Constants::SkillID::Water_Pool,
            GW::Constants::SkillID::Torturous_Embers,
            GW::Constants::SkillID::Torturers_Inferno,
            GW::Constants::SkillID::Talon_Strike,
            GW::Constants::SkillID::Shroud_of_Ash,
            GW::Constants::SkillID::Explosive_Force,
            GW::Constants::SkillID::Stop_Pump,
            GW::Constants::SkillID::Sacred_Branch,
            GW::Constants::SkillID::From_Hell,
            GW::Constants::SkillID::Corrupted_Roots,
            GW::Constants::SkillID::Meditation_of_the_Reaper1,
            (GW::Constants::SkillID)654, // Energy steal on crit
            (GW::Constants::SkillID)655, // Energy steal chance on hit
            GW::Constants::SkillID::Party_Time,
            (GW::Constants::SkillID)3411, // Keep the flag flying I
            (GW::Constants::SkillID)3412, // Keep the flag flying II
            (GW::Constants::SkillID)721,  // Feigned Sacrifice
            (GW::Constants::SkillID)735,  // Fluidity
            (GW::Constants::SkillID)747,  // Fury
            (GW::Constants::SkillID)744,  // Justice
            (GW::Constants::SkillID)718,  // Necropathy
            (GW::Constants::SkillID)740,  // Peace
            (GW::Constants::SkillID)759,  // Preparation
            (GW::Constants::SkillID)742,  // Rapid Healing
            (GW::Constants::SkillID)757,  // Rapid Shot
            (GW::Constants::SkillID)720,  // Sacrifice
            (GW::Constants::SkillID)733,  // Scintillation
            (GW::Constants::SkillID)729,  // Stone
            (GW::Constants::SkillID)738,  // Blessings
            (GW::Constants::SkillID)675,  // Cultist's
            (GW::Constants::SkillID)716,  // Cursing
            (GW::Constants::SkillID)758,  // Deadliness
            (GW::Constants::SkillID)731,  // The Bibliophile
            (GW::Constants::SkillID)719,  // The Plague
            (GW::Constants::SkillID)727,  // Thunder
            (GW::Constants::SkillID)723   // Vampire
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

        std::string_view name = *custom_sd.TryGetName();
        std::string_view full = *custom_sd.TryGetPredecodedDescription(custom_sd.GetDescKey(false, -1));
        std::string_view concise = *custom_sd.TryGetPredecodedDescription(custom_sd.GetDescKey(true, -1));

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

    bool IsSpellSkill(GW::Skill &skill)
    {
        return skill.type == GW::Constants::SkillType::Spell ||
               skill.type == GW::Constants::SkillType::Enchantment ||
               skill.type == GW::Constants::SkillType::Hex ||
               skill.type == GW::Constants::SkillType::Well ||
               skill.type == GW::Constants::SkillType::Ward ||
               skill.type == GW::Constants::SkillType::ItemSpell ||
               skill.type == GW::Constants::SkillType::WeaponSpell;
    }

    bool EndsOnIncDamage(GW::Constants::SkillID skill_id)
    {
        constexpr auto skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Reversal_of_Fortune,
            GW::Constants::SkillID::Reversal_of_Damage,
            GW::Constants::SkillID::Life_Sheath,
            GW::Constants::SkillID::Reverse_Hex,
            GW::Constants::SkillID::Weapon_of_Remedy,
            GW::Constants::SkillID::Xinraes_Weapon,
            GW::Constants::SkillID::Vengeful_Weapon,
            GW::Constants::SkillID::Ballad_of_Restoration,
            GW::Constants::SkillID::Ballad_of_Restoration_PvP);

        return skills.has(skill_id);
    }

    bool IsProjectileSkill(GW::Skill &skill)
    {
        constexpr auto skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Spear_of_Light,
            GW::Constants::SkillID::Lightning_Orb,
            GW::Constants::SkillID::Lightning_Orb_PvP,
            GW::Constants::SkillID::Shock_Arrow,
            GW::Constants::SkillID::Lightning_Bolt,
            GW::Constants::SkillID::Lightning_Javelin,
            GW::Constants::SkillID::Stone_Daggers,
            GW::Constants::SkillID::Stoning,
            GW::Constants::SkillID::Ebon_Hawk,
            GW::Constants::SkillID::Glowstone,
            GW::Constants::SkillID::Fireball,
            GW::Constants::SkillID::Flare,
            GW::Constants::SkillID::Lava_Arrows,
            GW::Constants::SkillID::Phoenix,
            GW::Constants::SkillID::Water_Trident,
            GW::Constants::SkillID::Ice_Spear,
            GW::Constants::SkillID::Shard_Storm,
            GW::Constants::SkillID::Crippling_Dagger,
            GW::Constants::SkillID::Dancing_Daggers,
            GW::Constants::SkillID::Disrupting_Dagger,
            GW::Constants::SkillID::Alkars_Alchemical_Acid,
            GW::Constants::SkillID::Snaring_Web,
            GW::Constants::SkillID::Corsairs_Net,
            GW::Constants::SkillID::Bit_Golem_Breaker,
            GW::Constants::SkillID::Fireball_obelisk,
            GW::Constants::SkillID::NOX_Thunder,
            GW::Constants::SkillID::Phased_Plasma_Burst,
            GW::Constants::SkillID::Plasma_Shot,
            GW::Constants::SkillID::Snowball_NPC,
            GW::Constants::SkillID::Ultra_Snowball,
            GW::Constants::SkillID::Ultra_Snowball1,
            GW::Constants::SkillID::Ultra_Snowball2,
            GW::Constants::SkillID::Ultra_Snowball3,
            GW::Constants::SkillID::Ultra_Snowball4,
            GW::Constants::SkillID::Fire_Dart,
            GW::Constants::SkillID::Ice_Dart,
            GW::Constants::SkillID::Poison_Dart,
            GW::Constants::SkillID::Lightning_Orb1,
            (GW::Constants::SkillID)2795, // Advanced Snowball
            GW::Constants::SkillID::Mega_Snowball,
            GW::Constants::SkillID::Snowball,
            GW::Constants::SkillID::Snowball1,
            (GW::Constants::SkillID)2796, // Super Mega Snowball
            GW::Constants::SkillID::Rocket_Propelled_Gobstopper,
            GW::Constants::SkillID::Polymock_Fireball,
            GW::Constants::SkillID::Polymock_Flare,
            GW::Constants::SkillID::Polymock_Frozen_Trident,
            GW::Constants::SkillID::Polymock_Ice_Spear,
            GW::Constants::SkillID::Polymock_Lightning_Orb,
            GW::Constants::SkillID::Polymock_Piercing_Light_Spear,
            GW::Constants::SkillID::Polymock_Shock_Arrow,
            GW::Constants::SkillID::Polymock_Stone_Daggers,
            GW::Constants::SkillID::Polymock_Stoning);

        if (skill.type == GW::Constants::SkillType::Attack &&
            skill.weapon_req & 70)
        {
            return true;
        }

        return skills.has(skill.skill_id);
    }

    bool IsDragonArenaSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Spirit_of_the_Festival &&
                skill_id <= GW::Constants::SkillID::Imperial_Majesty);
    }

    bool IsRollerBeetleSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Rollerbeetle_Racer &&
                skill_id <= GW::Constants::SkillID::Spit_Rocks);
    }

    bool IsYuletideSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Blinding_Snow &&
                   skill_id <= GW::Constants::SkillID::Flurry_of_Ice) ||

               (skill_id >= GW::Constants::SkillID::Side_Step &&
                   skill_id <= GW::Constants::SkillID::Rudis_Red_Nose) ||

               (skill_id >= (GW::Constants::SkillID)2795 && // Advanced Snowball
                   skill_id <= (GW::Constants::SkillID)2802) ||

               skill_id == (GW::Constants::SkillID)2964; // Snowball but with 1s aftercast
    }

    bool IsAgentOfTheMadKingSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Agent_of_the_Mad_King &&
                skill_id <= GW::Constants::SkillID::The_Mad_Kings_Influence);
    }

    bool IsCandyCornInfantrySkill(GW::Constants::SkillID skill_id)
    {
        constexpr auto skills = MakeFixedSet<GW::Constants::SkillID>(
            (GW::Constants::SkillID)2757, // Candy Corn Infantry disguise
            GW::Constants::SkillID::Candy_Corn_Strike,
            GW::Constants::SkillID::Rocket_Propelled_Gobstopper,
            GW::Constants::SkillID::Rain_of_Terror_spell,
            GW::Constants::SkillID::Cry_of_Madness,
            GW::Constants::SkillID::Sugar_Infusion,
            GW::Constants::SkillID::Feast_of_Vengeance,
            GW::Constants::SkillID::Animate_Candy_Minions, // Animate Candy Golem
            (GW::Constants::SkillID)2789                   // Summon Candy Golem
        );
        return skills.has(skill_id);
    }

    bool IsBrawlingSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Brawling &&
                   skill_id <= GW::Constants::SkillID::STAND_UP) ||

               (skill_id >= GW::Constants::SkillID::Falkens_Fire_Fist &&
                   skill_id <= GW::Constants::SkillID::Falken_Quick);
    }

    bool IsPolymockSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Polymock_Power_Drain &&
                   skill_id <= GW::Constants::SkillID::Polymock_Mind_Wreck) ||

               (skill_id >= GW::Constants::SkillID::Polymock_Deathly_Chill &&
                   skill_id <= GW::Constants::SkillID::Mursaat_Elementalist_Form);
    }

    bool IsCelestialSkill(GW::Constants::SkillID skill_id)
    {
        constexpr auto celestial_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Celestial_Haste,
            GW::Constants::SkillID::Celestial_Stance,
            GW::Constants::SkillID::Celestial_Storm,
            GW::Constants::SkillID::Celestial_Summoning,
            GW::Constants::SkillID::Star_Servant,
            GW::Constants::SkillID::Star_Shine,
            GW::Constants::SkillID::Star_Strike,
            GW::Constants::SkillID::Storm_of_Swords);
        return celestial_skills.has(skill_id);
    }

    bool IsCommandoSkill(GW::Constants::SkillID skill_id)
    {
        return skill_id == GW::Constants::SkillID::Going_Commando ||
               (skill_id >= GW::Constants::SkillID::Stun_Grenade &&
                   skill_id <= GW::Constants::SkillID::Tango_Down);
    }

    bool IsGolemSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::GOLEM_disguise &&
                   skill_id <= GW::Constants::SkillID::Annihilator_Toss) ||

               skill_id == GW::Constants::SkillID::Sky_Net;
    }

    bool IsRavenSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Raven_Talons &&
                   skill_id <= GW::Constants::SkillID::Raven_Flight) ||

               (skill_id >= GW::Constants::SkillID::Raven_Blessing_A_Gate_Too_Far &&
                   skill_id <= GW::Constants::SkillID::Raven_Talons_A_Gate_Too_Far);
    }

    bool IsUrsanSkill(GW::Constants::SkillID skill_id)
    {
        constexpr auto ursan_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Ursan_Strike,
            GW::Constants::SkillID::Ursan_Rage,
            GW::Constants::SkillID::Ursan_Roar,
            GW::Constants::SkillID::Ursan_Force,
            GW::Constants::SkillID::Ursan_Roar_Blood_Washes_Blood,
            GW::Constants::SkillID::Ursan_Force_Blood_Washes_Blood,
            GW::Constants::SkillID::Ursan_Aura,
            GW::Constants::SkillID::Ursan_Rage_Blood_Washes_Blood,
            GW::Constants::SkillID::Ursan_Strike_Blood_Washes_Blood,
            (GW::Constants::SkillID)2115 // Ursan blessing effect (blood washes blood)
        );

        return ursan_skills.has(skill_id);
    }

    bool IsVolfenSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Volfen_Claw &&
                   skill_id <= GW::Constants::SkillID::Volfen_Agility) ||

               (skill_id >= GW::Constants::SkillID::Volfen_Pounce_Curse_of_the_Nornbear &&
                   skill_id <= GW::Constants::SkillID::Volfen_Blessing_Curse_of_the_Nornbear);
    }

    bool IsNornAspectSkill(GW::Constants::SkillID skill_id)
    {
        return (IsUrsanSkill(skill_id) ||
                IsVolfenSkill(skill_id) ||
                IsRavenSkill(skill_id) ||
                skill_id == GW::Constants::SkillID::Totem_of_Man);
    }

    bool IsKeiranSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Keiran_Thackeray_disguise &&
                   skill_id <= GW::Constants::SkillID::Rain_of_Arrows) ||

               (skill_id >= GW::Constants::SkillID::Keirans_Sniper_Shot_Hearts_of_the_North &&
                   skill_id <= GW::Constants::SkillID::Theres_Nothing_to_Fear_Thackeray);
    }

    bool IsGwenSkill(GW::Constants::SkillID skill_id)
    {
        return skill_id == GW::Constants::SkillID::Gwen_disguise ||

               (skill_id >= GW::Constants::SkillID::Distortion_Gwen &&
                   skill_id <= GW::Constants::SkillID::Sum_of_All_Fears_Gwen) ||

               (skill_id >= GW::Constants::SkillID::Hide &&
                   skill_id <= GW::Constants::SkillID::Throw_Rock);
    }

    bool IsTogoSkill(GW::Constants::SkillID skill_id)
    {
        constexpr auto togo_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Togo_disguise,
            GW::Constants::SkillID::Call_to_the_Spirit_Realm,
            GW::Constants::SkillID::Essence_Strike_Togo,
            GW::Constants::SkillID::Spirit_Burn_Togo,
            GW::Constants::SkillID::Spirit_Rift_Togo,
            GW::Constants::SkillID::Mend_Body_and_Soul_Togo,
            GW::Constants::SkillID::Offering_of_Spirit_Togo,
            GW::Constants::SkillID::Disenchantment_Togo,
            GW::Constants::SkillID::Dragon_Empire_Rage,
            GW::Constants::SkillID::Pain1,
            GW::Constants::SkillID::Pain_attack_Togo,
            GW::Constants::SkillID::Pain_attack_Togo1,
            GW::Constants::SkillID::Pain_attack_Togo2);
        return togo_skills.has(skill_id);
    }

    bool IsTuraiSkill(GW::Constants::SkillID skill_id)
    {
        return skill_id == GW::Constants::SkillID::Turai_Ossa_disguise ||

               (skill_id >= GW::Constants::SkillID::For_Elona &&
                   skill_id <= GW::Constants::SkillID::Whirlwind_Attack_Turai_Ossa) ||

               skill_id == GW::Constants::SkillID::Dragon_Slash_Turai_Ossa;
    }

    bool IsSaulSkill(GW::Constants::SkillID skill_id)
    {
        constexpr auto saul_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Saul_DAlessio_disguise,
            GW::Constants::SkillID::Signet_of_the_Unseen,
            GW::Constants::SkillID::Castigation_Signet_Saul_DAlessio,
            GW::Constants::SkillID::Unnatural_Signet_Saul_DAlessio,
            GW::Constants::SkillID::Spectral_Agony_Saul_DAlessio,
            GW::Constants::SkillID::Banner_of_the_Unseen,
            GW::Constants::SkillID::Form_Up_and_Advance);
        return saul_skills.has(skill_id);
    }

    bool IsMissionSkill(GW::Constants::SkillID skill_id)
    {
        constexpr auto mission_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Disarm_Trap,
            GW::Constants::SkillID::Vial_of_Purified_Water,
            GW::Constants::SkillID::Lit_Torch,
            (GW::Constants::SkillID)2366,             // Alkar's Concoction item skill
            GW::Constants::SkillID::Alkars_Concoction // Alkar's Concoction effect
        );

        return mission_skills.has(skill_id);
    }

    bool IsSpiritFormSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Dhuums_Rest &&
                   skill_id <= GW::Constants::SkillID::Ghostly_Fury) ||

               skill_id == GW::Constants::SkillID::Spirit_Form_disguise;
    }

    bool IsSiegeDevourerSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Siege_Devourer &&
                skill_id <= GW::Constants::SkillID::Dismount_Siege_Devourer);
    }

    bool IsJununduSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Choking_Breath &&
                   skill_id <= GW::Constants::SkillID::Junundu_Wail) ||

               (skill_id >= GW::Constants::SkillID::Desert_Wurm_disguise &&
                   skill_id <= GW::Constants::SkillID::Leave_Junundu) ||

               skill_id == GW::Constants::SkillID::Unknown_Junundu_Ability;
    }

    bool IsBundleSkill(GW::Constants::SkillID skill_id)
    {
        constexpr auto bundle_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Asuran_Flame_Staff,
            GW::Constants::SkillID::Aura_of_the_Staff_of_the_Mists,
            GW::Constants::SkillID::Curse_of_the_Staff_of_the_Mists,
            GW::Constants::SkillID::Power_of_the_Staff_of_the_Mists,
            GW::Constants::SkillID::Balm_Bomb,
            GW::Constants::SkillID::Barbed_Bomb,
            GW::Constants::SkillID::Burden_Totem,
            GW::Constants::SkillID::Courageous_Was_Saidra,
            GW::Constants::SkillID::Dwarven_Powder_Keg,
            GW::Constants::SkillID::Entanglement,
            GW::Constants::SkillID::Explosives,
            GW::Constants::SkillID::Firebomb_Explosion,
            GW::Constants::SkillID::Flux_Overload,
            GW::Constants::SkillID::Gelatinous_Material_Explosion,
            GW::Constants::SkillID::Gift_of_Battle,
            GW::Constants::SkillID::Healing_Salve,
            (GW::Constants::SkillID)2833, // Improvised Fire Bomb
            (GW::Constants::SkillID)2834, // Improvised Fire Trap
            GW::Constants::SkillID::Invigorating_Mist,
            GW::Constants::SkillID::Light_of_Seborhin,
            GW::Constants::SkillID::Rations,
            GW::Constants::SkillID::Scepter_of_Orrs_Aura,
            GW::Constants::SkillID::Scepter_of_Orrs_Power,
            GW::Constants::SkillID::Seed_of_Resurrection,
            GW::Constants::SkillID::Seed_of_Resurrection1,
            GW::Constants::SkillID::Urn_of_Saint_Viktor_Level_1,
            GW::Constants::SkillID::Urn_of_Saint_Viktor_Level_2,
            GW::Constants::SkillID::Urn_of_Saint_Viktor_Level_3,
            GW::Constants::SkillID::Urn_of_Saint_Viktor_Level_4,
            GW::Constants::SkillID::Urn_of_Saint_Viktor_Level_5,
            GW::Constants::SkillID::Shield_of_Saint_Viktor,
            GW::Constants::SkillID::Shield_of_Saint_Viktor_Celestial_Summoning,
            GW::Constants::SkillID::Shielding_Urn_skill,
            GW::Constants::SkillID::Spear_of_Archemorus_Level_1,
            GW::Constants::SkillID::Spear_of_Archemorus_Level_2,
            GW::Constants::SkillID::Spear_of_Archemorus_Level_3,
            GW::Constants::SkillID::Spear_of_Archemorus_Level_4,
            GW::Constants::SkillID::Spear_of_Archemorus_Level_5,
            GW::Constants::SkillID::Splinter_Mine_skill,
            GW::Constants::SkillID::Stun_Bomb,
            GW::Constants::SkillID::Volatile_Charr_Crystal);
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
        constexpr auto pvp_only_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Charm_Animal_Codex);

        return pvp_only_skills.has(skill.skill_id);
    }

    bool IsConsumableItemSkill(const GW::Skill &skill)
    {
        constexpr auto item_skills = MakeFixedSet<GW::Constants::SkillID>(
            (GW::Constants::SkillID)2366,              // Alkar's Concoction item skill
            GW::Constants::SkillID::Alkars_Concoction, // Effect after using the item
            GW::Constants::SkillID::Birthday_Cupcake_skill,
            GW::Constants::SkillID::Candy_Apple_skill,
            GW::Constants::SkillID::Candy_Corn_skill,
            GW::Constants::SkillID::Golden_Egg_skill,
            GW::Constants::SkillID::Lucky_Aura,
            GW::Constants::SkillID::Lunar_Blessing,
            GW::Constants::SkillID::Spiritual_Possession,
            GW::Constants::SkillID::Party_Mode,
            GW::Constants::SkillID::Pie_Induced_Ecstasy,
            GW::Constants::SkillID::Blue_Rock_Candy_Rush,
            GW::Constants::SkillID::Green_Rock_Candy_Rush,
            GW::Constants::SkillID::Red_Rock_Candy_Rush,
            GW::Constants::SkillID::Adventurers_Insight,
            GW::Constants::SkillID::Berserkers_Insight,
            GW::Constants::SkillID::Heros_Insight,
            GW::Constants::SkillID::Hunters_Insight,
            GW::Constants::SkillID::Lightbringers_Insight,
            GW::Constants::SkillID::Rampagers_Insight,
            GW::Constants::SkillID::Slayers_Insight,
            GW::Constants::SkillID::Sugar_Rush_short,
            GW::Constants::SkillID::Sugar_Rush_medium,
            GW::Constants::SkillID::Sugar_Rush_long,
            GW::Constants::SkillID::Sugar_Jolt_short,
            GW::Constants::SkillID::Sugar_Jolt_long,
            GW::Constants::SkillID::Grail_of_Might_item_effect,
            GW::Constants::SkillID::Essence_of_Celerity_item_effect,
            GW::Constants::SkillID::Armor_of_Salvation_item_effect,
            GW::Constants::SkillID::Skale_Vigor,
            GW::Constants::SkillID::Pahnai_Salad_item_effect,
            GW::Constants::SkillID::Drake_Skin,
            GW::Constants::SkillID::Yo_Ho_Ho_and_a_Bottle_of_Grog,
            GW::Constants::SkillID::Well_Supplied,
            GW::Constants::SkillID::Weakened_by_Dhuum,
            GW::Constants::SkillID::Tonic_Tipsiness,
            GW::Constants::SkillID::Summoning_Sickness,
            (GW::Constants::SkillID)3407 // Summoning_Sickness
        );

        return item_skills.has(skill.skill_id);
    }

    bool IsEffectOnly(const GW::Skill &skill)
    {
        constexpr auto effect_only_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Phase_Shield_effect);

        return skill.special & (uint32_t)Utils::SkillSpecialFlags::Effect ||
               skill.type == GW::Constants::SkillType::Bounty ||
               skill.type == GW::Constants::SkillType::Scroll ||
               skill.type == GW::Constants::SkillType::Condition ||
               skill.type == GW::Constants::SkillType::Title ||
               skill.type == GW::Constants::SkillType::Passive ||
               skill.type == GW::Constants::SkillType::Environmental ||
               skill.type == GW::Constants::SkillType::EnvironmentalTrap ||
               skill.type == GW::Constants::SkillType::Disguise ||
               effect_only_skills.has(skill.skill_id) ||
               IsConsumableItemSkill(skill);
    }

    bool IsSpiritAttackSkill(const GW::Skill &skill)
    {
        constexpr auto spirit_attack_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Gaze_of_Fury_attack,
            GW::Constants::SkillID::Bloodsong_attack,
            GW::Constants::SkillID::Pain_attack,
            GW::Constants::SkillID::Pain_attack_Signet_of_Spirits,
            GW::Constants::SkillID::Pain_attack_Signet_of_Spirits1,
            GW::Constants::SkillID::Pain_attack_Signet_of_Spirits2,
            GW::Constants::SkillID::Pain_attack_Togo,
            GW::Constants::SkillID::Pain_attack_Togo1,
            GW::Constants::SkillID::Pain_attack_Togo2,
            GW::Constants::SkillID::Shadowsong_attack,
            GW::Constants::SkillID::Anguish_attack,
            GW::Constants::SkillID::Vampirism_attack,
            GW::Constants::SkillID::Disenchantment_attack,
            GW::Constants::SkillID::Wanderlust_attack,
            GW::Constants::SkillID::Dissonance_attack);

        return spirit_attack_skills.has(skill.skill_id);
    }

    bool IsEnvironmentSkill(const GW::Skill &skill)
    {
        constexpr auto environment_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Stormcaller_skill,
            GW::Constants::SkillID::Teleport_Players,
            GW::Constants::SkillID::Rurik_Must_Live,
            GW::Constants::SkillID::Torch_Degeneration_Hex,
            GW::Constants::SkillID::Torch_Enchantment,
            GW::Constants::SkillID::Torch_Hex,
            GW::Constants::SkillID::Spectral_Infusion,
            GW::Constants::SkillID::Call_of_the_Eye,
            GW::Constants::SkillID::Altar_Buff,
            GW::Constants::SkillID::Capture_Point,
            GW::Constants::SkillID::Curse_of_Dhuum,
            GW::Constants::SkillID::Fireball_obelisk,
            GW::Constants::SkillID::Mad_Kings_Fan,
            GW::Constants::SkillID::Resurrect_Party,
            GW::Constants::SkillID::Rock_Slide,
            GW::Constants::SkillID::Avalanche_effect,
            GW::Constants::SkillID::Exploding_Barrel,
            GW::Constants::SkillID::Water,
            GW::Constants::SkillID::Chimera_of_Intensity,
            GW::Constants::SkillID::Curse_of_the_Bloodstone,
            GW::Constants::SkillID::Fount_Of_Maguuma,
            GW::Constants::SkillID::Healing_Fountain,
            GW::Constants::SkillID::Icy_Ground,
            GW::Constants::SkillID::Divine_Fire,
            GW::Constants::SkillID::Domain_of_Elements,
            GW::Constants::SkillID::Domain_of_Energy_Draining,
            GW::Constants::SkillID::Domain_of_Health_Draining,
            GW::Constants::SkillID::Domain_of_Skill_Damage,
            GW::Constants::SkillID::Domain_of_Slow,
            GW::Constants::SkillID::Chain_Lightning_environment,
            GW::Constants::SkillID::Eruption_environment,
            GW::Constants::SkillID::Fire_Storm_environment,
            GW::Constants::SkillID::Maelstrom_environment,
            GW::Constants::SkillID::Mursaat_Tower_skill,
            GW::Constants::SkillID::Obelisk_Lightning,
            GW::Constants::SkillID::Quest_Skill,
            GW::Constants::SkillID::Quest_skill_for_Coastal_Exam,
            GW::Constants::SkillID::Quicksand_environment_effect,
            GW::Constants::SkillID::Siege_Attack1,
            GW::Constants::SkillID::Siege_Attack3,
            GW::Constants::SkillID::Chomper,
            GW::Constants::SkillID::Blast_Furnace,
            GW::Constants::SkillID::Sorrows_Fist,
            GW::Constants::SkillID::Sorrows_Flame,
            GW::Constants::SkillID::Statues_Blessing,
            GW::Constants::SkillID::Swamp_Water,
            GW::Constants::SkillID::Tar,
            GW::Constants::SkillID::Archemorus_Strike,
            GW::Constants::SkillID::Elemental_Defense_Zone,
            GW::Constants::SkillID::Melee_Defense_Zone,
            GW::Constants::SkillID::Rage_of_the_Sea,
            GW::Constants::SkillID::Gods_Blessing,
            GW::Constants::SkillID::Madness_Dart,
            GW::Constants::SkillID::Sentry_Trap_skill,
            GW::Constants::SkillID::Spirit_Form_Remains_of_Sahlahja,
            GW::Constants::SkillID::The_Elixir_of_Strength,
            GW::Constants::SkillID::Untouchable,
            GW::Constants::SkillID::Battle_Cry,
            GW::Constants::SkillID::Battle_Cry1,
            GW::Constants::SkillID::Energy_Shrine_Bonus,
            GW::Constants::SkillID::To_the_Pain_Hero_Battles,
            GW::Constants::SkillID::Northern_Health_Shrine_Bonus,
            GW::Constants::SkillID::Southern_Health_Shrine_Bonus,
            GW::Constants::SkillID::Western_Health_Shrine_Bonus,
            GW::Constants::SkillID::Eastern_Health_Shrine_Bonus,
            GW::Constants::SkillID::Boulder,
            GW::Constants::SkillID::Burning_Ground,
            GW::Constants::SkillID::Dire_Snowball,
            GW::Constants::SkillID::Fire_Boulder,
            GW::Constants::SkillID::Fire_Dart,
            GW::Constants::SkillID::Fire_Jet,
            GW::Constants::SkillID::Fire_Spout,
            GW::Constants::SkillID::Freezing_Ground,
            GW::Constants::SkillID::Haunted_Ground,
            GW::Constants::SkillID::Ice_Dart,
            GW::Constants::SkillID::Poison_Dart,
            GW::Constants::SkillID::Poison_Ground,
            GW::Constants::SkillID::Poison_Jet,
            GW::Constants::SkillID::Poison_Spout,
            GW::Constants::SkillID::Sarcophagus_Spores,
            GW::Constants::SkillID::Fire_Dart1);

        if (skill.type == GW::Constants::SkillType::Environmental ||
            skill.type == GW::Constants::SkillType::EnvironmentalTrap)
            return true;

        return environment_skills.has(skill.skill_id);
    }

    bool IsMonsterSkill(const GW::Skill &skill)
    {
        constexpr auto monster_skills_without_monster_icon = MakeFixedSet<GW::Constants::SkillID>(
            (GW::Constants::SkillID)1448, // Last Rites of Torment effect
            GW::Constants::SkillID::Torturous_Embers,
            GW::Constants::SkillID::Last_Rites_of_Torment,
            GW::Constants::SkillID::Healing_Breeze_Agnars_Rage,
            GW::Constants::SkillID::Crystal_Bonds,
            GW::Constants::SkillID::Jagged_Crystal_Skin,
            GW::Constants::SkillID::Crystal_Hibernation,
            GW::Constants::SkillID::Life_Vortex,
            GW::Constants::SkillID::Soul_Vortex2,
            (GW::Constants::SkillID)1425,
            (GW::Constants::SkillID)1712,
            (GW::Constants::SkillID)1713,
            (GW::Constants::SkillID)1714,
            GW::Constants::SkillID::Corsairs_Net,
            GW::Constants::SkillID::Lose_your_Head,
            GW::Constants::SkillID::Wandering_Mind,
            (GW::Constants::SkillID)1877,
            (GW::Constants::SkillID)1932,
            GW::Constants::SkillID::Embrace_the_Pain,
            GW::Constants::SkillID::Earth_Shattering_Blow,
            GW::Constants::SkillID::Corrupt_Power,
            GW::Constants::SkillID::Words_of_Madness,
            GW::Constants::SkillID::Words_of_Madness_Qwytzylkak,
            GW::Constants::SkillID::Presence_of_the_Skale_Lord,
            GW::Constants::SkillID::The_Apocrypha_is_changing_to_another_form,
            GW::Constants::SkillID::Reform_Carvings,
            GW::Constants::SkillID::Soul_Torture,
            GW::Constants::SkillID::Maddened_Strike,
            GW::Constants::SkillID::Maddened_Stance,
            GW::Constants::SkillID::Kournan_Siege,
            GW::Constants::SkillID::Bonds_of_Torment,
            (GW::Constants::SkillID)1883, // Bonds of Torment passive
            GW::Constants::SkillID::Shadow_Smash,
            GW::Constants::SkillID::Banish_Enchantment,
            GW::Constants::SkillID::Jadoths_Storm_of_Judgment,
            GW::Constants::SkillID::Twisting_Jaws,
            GW::Constants::SkillID::Snaring_Web,
            GW::Constants::SkillID::Ceiling_Collapse,
            GW::Constants::SkillID::Wurm_Bile,
            GW::Constants::SkillID::Shattered_Spirit,
            GW::Constants::SkillID::Spirit_Roar,
            GW::Constants::SkillID::Unseen_Aggression,
            GW::Constants::SkillID::Charging_Spirit,
            GW::Constants::SkillID::Powder_Keg_Explosion,
            GW::Constants::SkillID::Unstable_Ooze_Explosion,
            GW::Constants::SkillID::Golem_Shrapnel,
            GW::Constants::SkillID::Crystal_Snare,
            GW::Constants::SkillID::Paranoid_Indignation,
            GW::Constants::SkillID::Searing_Breath,
            GW::Constants::SkillID::Call_of_Destruction,
            GW::Constants::SkillID::Flame_Jet,
            GW::Constants::SkillID::Lava_Ground,
            GW::Constants::SkillID::Lava_Wave,
            GW::Constants::SkillID::Lava_Blast,
            GW::Constants::SkillID::Thunderfist_Strike,
            GW::Constants::SkillID::Murakais_Consumption,
            GW::Constants::SkillID::Murakais_Censure,
            GW::Constants::SkillID::Filthy_Explosion,
            GW::Constants::SkillID::Murakais_Call,
            GW::Constants::SkillID::Enraged_Blast,
            GW::Constants::SkillID::Fungal_Explosion,
            GW::Constants::SkillID::Bear_Form,
            GW::Constants::SkillID::Tongue_Lash,
            GW::Constants::SkillID::Soulrending_Shriek,
            GW::Constants::SkillID::Reverse_Polarity_Fire_Shield,
            GW::Constants::SkillID::Forgewights_Blessing,
            GW::Constants::SkillID::Selvetarms_Blessing,
            GW::Constants::SkillID::Thommiss_Blessing,
            GW::Constants::SkillID::Tongue_Whip,
            GW::Constants::SkillID::Reactor_Blast,
            GW::Constants::SkillID::Reactor_Blast_Timer,
            GW::Constants::SkillID::Internal_Power_Engaged,
            GW::Constants::SkillID::Target_Acquisition,
            GW::Constants::SkillID::NOX_Beam,
            GW::Constants::SkillID::NOX_Field_Dash,
            GW::Constants::SkillID::NOXion_Buster,
            GW::Constants::SkillID::Countdown,
            GW::Constants::SkillID::Bit_Golem_Breaker,
            GW::Constants::SkillID::Bit_Golem_Rectifier,
            GW::Constants::SkillID::Bit_Golem_Crash,
            GW::Constants::SkillID::Bit_Golem_Force,
            (GW::Constants::SkillID)1915,
            GW::Constants::SkillID::NOX_Phantom,
            GW::Constants::SkillID::NOX_Thunder,
            GW::Constants::SkillID::NOX_Lock_On,
            GW::Constants::SkillID::NOX_Fire,
            GW::Constants::SkillID::NOX_Knuckle,
            GW::Constants::SkillID::NOX_Divider_Drive,
            GW::Constants::SkillID::Theres_not_enough_time,
            GW::Constants::SkillID::Keirans_Sniper_Shot,
            GW::Constants::SkillID::Falken_Punch,
            GW::Constants::SkillID::Drunken_Stumbling,
            GW::Constants::SkillID::Koros_Gaze,
            GW::Constants::SkillID::Adoration,
            GW::Constants::SkillID::Isaiahs_Balance,
            GW::Constants::SkillID::Toriimos_Burning_Fury,
            GW::Constants::SkillID::Promise_of_Death,
            GW::Constants::SkillID::Withering_Blade,
            GW::Constants::SkillID::Deaths_Embrace,
            GW::Constants::SkillID::Venom_Fang,
            GW::Constants::SkillID::Survivors_Will,
            GW::Constants::SkillID::Charm_Animal_Ashlyn_Spiderfriend,
            GW::Constants::SkillID::Charm_Animal_Charr_Demolisher,
            GW::Constants::SkillID::Charm_Animal_White_Mantle,
            GW::Constants::SkillID::Charm_Animal_monster,
            GW::Constants::SkillID::Charm_Animal_monster1,
            GW::Constants::SkillID::Charm_Animal1,
            GW::Constants::SkillID::Charm_Animal2,
            (GW::Constants::SkillID)1868, // Monster charm animal
            (GW::Constants::SkillID)1869, // Monster charm animal
            (GW::Constants::SkillID)1870, // Monster charm animal
            (GW::Constants::SkillID)1906, // Monster charm animal
            (GW::Constants::SkillID)1907, // Monster charm animal
            (GW::Constants::SkillID)1908, // Monster charm animal
            (GW::Constants::SkillID)1909, // Monster charm animal
            GW::Constants::SkillID::Ehzah_from_Above,
            GW::Constants::SkillID::Rise_From_Your_Grave,
            GW::Constants::SkillID::Resurrect_monster_skill,
            GW::Constants::SkillID::Charm_Animal_monster_skill,
            GW::Constants::SkillID::Phase_Shield_monster_skill,
            GW::Constants::SkillID::Phase_Shield_effect,
            GW::Constants::SkillID::Vitality_Transfer,
            GW::Constants::SkillID::Restore_Life_monster_skill,
            GW::Constants::SkillID::Splinter_Shot_monster_skill,
            GW::Constants::SkillID::Junundu_Tunnel_monster_skill,
            GW::Constants::SkillID::Vital_Blessing_monster_skill,
            GW::Constants::SkillID::Snowball_NPC,
            GW::Constants::SkillID::Veratas_Promise,
            GW::Constants::SkillID::Ebon_Vanguard_Assassin_Support_NPC,
            GW::Constants::SkillID::Ebon_Vanguard_Battle_Standard_of_Power,
            GW::Constants::SkillID::Diamondshard_Mist,
            GW::Constants::SkillID::Diamondshard_Mist_environment_effect,
            GW::Constants::SkillID::Diamondshard_Grave,
            GW::Constants::SkillID::Dhuums_Rest_Reaper_skill,
            GW::Constants::SkillID::Ghostly_Fury_Reaper_skill,
            GW::Constants::SkillID::Spiritual_Healing_Reaper_skill,
            GW::Constants::SkillID::Golem_Pilebunker,
            GW::Constants::SkillID::Putrid_Flames,
            GW::Constants::SkillID::Whirling_Fires,
            GW::Constants::SkillID::Wave_of_Torment,
            GW::Constants::SkillID::REMOVE_Queen_Wail,
            GW::Constants::SkillID::Queen_Wail,
            GW::Constants::SkillID::REMOVE_Queen_Armor,
            GW::Constants::SkillID::Queen_Armor,
            GW::Constants::SkillID::Queen_Heal,
            GW::Constants::SkillID::Queen_Bite,
            GW::Constants::SkillID::Queen_Thump,
            GW::Constants::SkillID::Queen_Siege,
            GW::Constants::SkillID::Infernal_Rage,
            GW::Constants::SkillID::Flame_Call,
            GW::Constants::SkillID::Skin_of_Stone,
            GW::Constants::SkillID::From_Hell,
            GW::Constants::SkillID::Feeding_Frenzy_skill,
            GW::Constants::SkillID::Frost_Vortex,
            GW::Constants::SkillID::Earth_Vortex,
            GW::Constants::SkillID::Enemies_Must_Die,
            GW::Constants::SkillID::Enchantment_Collapse,
            GW::Constants::SkillID::Call_of_Sacrifice,
            GW::Constants::SkillID::Corrupted_Strength,
            GW::Constants::SkillID::Corrupted_Roots,
            GW::Constants::SkillID::Corrupted_Healing,
            GW::Constants::SkillID::Caltrops_monster,
            GW::Constants::SkillID::Call_to_the_Torment,
            GW::Constants::SkillID::Abaddons_Favor,
            GW::Constants::SkillID::Abaddons_Chosen,
            GW::Constants::SkillID::Suicide_Health,
            GW::Constants::SkillID::Suicide_Energy,
            GW::Constants::SkillID::Meditation_of_the_Reaper,
            GW::Constants::SkillID::Meditation_of_the_Reaper1,
            GW::Constants::SkillID::Corrupted_Breath,
            GW::Constants::SkillID::Kilroy_Stonekin,
            GW::Constants::SkillID::Janthirs_Gaze,
            GW::Constants::SkillID::Its_Good_to_Be_King);

        constexpr auto non_monster_skills_with_monster_icon = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Spectral_Agony_Saul_DAlessio,
            GW::Constants::SkillID::Burden_Totem,
            GW::Constants::SkillID::Splinter_Mine_skill,
            GW::Constants::SkillID::Entanglement);

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

    GW::Constants::SkillID degen_skills[] = {
        GW::Constants::SkillID::Faintheartedness,
        GW::Constants::SkillID::Life_Siphon,
        GW::Constants::SkillID::Life_Transfer,
        GW::Constants::SkillID::Lingering_Curse,
        GW::Constants::SkillID::Parasitic_Bond,
        GW::Constants::SkillID::Putrid_Bile,
        GW::Constants::SkillID::Reapers_Mark,
        GW::Constants::SkillID::Suffering,
        GW::Constants::SkillID::Weaken_Knees,
        GW::Constants::SkillID::Wither,
        GW::Constants::SkillID::Conjure_Nightmare,
        GW::Constants::SkillID::Conjure_Phantasm,
        GW::Constants::SkillID::Crippling_Anguish,
        GW::Constants::SkillID::Crippling_Anguish_PvP,
        GW::Constants::SkillID::Illusion_of_Pain,
        GW::Constants::SkillID::Illusion_of_Pain_PvP,
        GW::Constants::SkillID::Images_of_Remorse,
        GW::Constants::SkillID::Migraine,
        GW::Constants::SkillID::Migraine_PvP,
        GW::Constants::SkillID::Overload,
        GW::Constants::SkillID::Phantom_Pain,
        GW::Constants::SkillID::Shrinking_Armor,
    };

    GW::Constants::SkillID degen_well_skills[] = {
        GW::Constants::SkillID::Well_of_Silence,
        GW::Constants::SkillID::Well_of_Suffering,
    };

    GW::Constants::SkillID regen_skills[] = {
        GW::Constants::SkillID::I_Will_Avenge_You,
        GW::Constants::SkillID::Never_Surrender,
        GW::Constants::SkillID::Never_Surrender_PvP,
        GW::Constants::SkillID::Together_as_one,
        GW::Constants::SkillID::Blood_Renewal,
        GW::Constants::SkillID::Feel_No_Pain,
        GW::Constants::SkillID::Feigned_Neutrality,
        GW::Constants::SkillID::Healing_Breeze,
        GW::Constants::SkillID::Hexers_Vigor,
        GW::Constants::SkillID::Ice_Spear,
        GW::Constants::SkillID::Mending,
        GW::Constants::SkillID::Mending_Refrain,
        GW::Constants::SkillID::Mending_Refrain_PvP,
        GW::Constants::SkillID::Never_Rampage_Alone,
        GW::Constants::SkillID::Restful_Breeze,
        GW::Constants::SkillID::Shadow_Refuge,
        GW::Constants::SkillID::Shadow_Sanctuary_kurzick,
        GW::Constants::SkillID::Shadow_Sanctuary_luxon,
        GW::Constants::SkillID::Shield_of_Regeneration,
        GW::Constants::SkillID::Succor,
        GW::Constants::SkillID::Swirling_Aura,
        GW::Constants::SkillID::Troll_Unguent,
        GW::Constants::SkillID::Vampiric_Spirit,
        GW::Constants::SkillID::Volfen_Blessing,
        GW::Constants::SkillID::Volfen_Blessing_Curse_of_the_Nornbear,
        GW::Constants::SkillID::Vow_of_Piety,
        GW::Constants::SkillID::Ward_Against_Harm,
        GW::Constants::SkillID::Watchful_Healing,
        GW::Constants::SkillID::Watchful_Spirit,
        GW::Constants::SkillID::Weapon_of_Warding,
    };

    GW::Constants::SkillID regen_well_skills[] = {
        GW::Constants::SkillID::Well_of_Blood,
        GW::Constants::SkillID::Well_of_Power,
    };

    GW::Constants::SkillID regen_per_condition_skills[] = {
        GW::Constants::SkillID::I_Will_Survive,
        GW::Constants::SkillID::Conviction,
    };

    GW::Constants::SkillID regen_per_condition_or_hex_skills[] = {
        GW::Constants::SkillID::Melandrus_Resilience,
        GW::Constants::SkillID::Resilient_Was_Xiko,
    };

    GW::Constants::SkillID regen_per_enchantment_skills[] = {
        GW::Constants::SkillID::Mystic_Regeneration,
        GW::Constants::SkillID::Mystic_Regeneration_PvP,
    };

    CustomSkillData custom_skill_datas[GW::Constants::SkillMax];

    std::string *CustomSkillData::TryGetPredecodedDescription(DescKey key)
    {
        auto pre_key = key.pre_key();
        if (desc_guard.IsInit(pre_key))
            return &predecoded_descriptions[pre_key];

        if (desc_guard.TryBeginInit(pre_key))
        {
            // Start decoding the description
            const auto size = 256;
            wchar_t buffer[size];
            bool success = Utils::SkillDescriptionToEncStr(buffer, size, *skill, key.is_concise, key.attribute_lvl);
            assert(success);

            auto Callback = [](void *param, const wchar_t *s)
            {
                auto param_data = reinterpret_cast<uintptr_t>(param);
                auto skill_id = param_data & 0xFFFF;
                auto pre_key = param_data >> 16;
                auto &entry = custom_skill_datas[skill_id];
                entry.predecoded_descriptions[pre_key] = Utils::WStrToStr(s);
                entry.desc_guard.FinishInit(pre_key);
            };

            assert((uint32_t)skill_id < 0x10000);
            auto param = reinterpret_cast<void *>((uint32_t)skill_id | pre_key << 16);

            GW::UI::AsyncDecodeStr(buffer, Callback, param); // This is almost always not async

            // Check if the description was decoded instantly
            if (desc_guard.IsInit(pre_key))
            {
                return &predecoded_descriptions[pre_key];
            }
        }

        return nullptr;
    }

    DescKey CustomSkillData::GetDescKey(bool is_concise, int32_t attribute_lvl) const
    {
        auto key = DescKey();
        key.is_singular0 = GetSkillParam(0).IsSingular(attribute_lvl);
        key.is_singular1 = GetSkillParam(1).IsSingular(attribute_lvl);
        key.is_singular2 = GetSkillParam(2).IsSingular(attribute_lvl);
        key.is_concise = is_concise;
        key.is_valid = true;
        key.attribute_lvl = attribute_lvl;
        return key;
    }

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

    bool TryGetProp(CustomSkillData &custom_sd, std::string_view &range, char *&p, std::string_view name, SkillParam &out)
    {
        auto start = (char *)range.data();
        auto end = start + range.size();
        auto p_at = p;
        auto p_after = p;
        if (Utils::TryRead(name, p_after, end))
        {
            PrevWord(start, p);
            if (*p == '+' || *p == '-')
                p++;
            char *q;
            uint32_t literal = strtol(p, &q, 10);
            if (p < q)
            {
                out = {literal, literal};
            }
            else if (Utils::TryReadAfter("<rep=", p, p_at))
            {
                auto param_id = std::strtol(p, nullptr, 10);
                out = custom_sd.GetSkillParam(param_id);
            }
            p = p_after;
            range = std::string_view(p, end - p);
            return true;
        }
        p = p_at;
        return false;
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

    enum struct DescWord
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

    MachineDesc::MachineDesc(std::string_view desc)
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
        words.push_back(word);        \
        continue;                     \
    }

        while (p < end)
        {
            SkipWhitespace(p, end);

            if (IsNumber(*p))
            {
                uint32_t lo = strtol(p, &p, 10);
                uint32_t hi = lo;
                if (Utils::TryRead("...", p, end))
                {
                    hi = strtol(p, &p, 10);
                }
                auto param = SkillParam(lo, hi);
                lits.push_back(param);
                words.push_back(DescWord::Literal);
                continue;
            }

            // misc
            MATCH_AND_CONTINUE(".", DescWord::Period)
            MATCH_AND_CONTINUE(",", DescWord::Comma)
            MATCH_AND_CONTINUE("%", DescWord::Percent)

            if (p == start || !IsChar(p[-1]))
            {
                MATCH_AND_CONTINUE("-second", DescWord::Seconds) // Special case for Shadowsong which has unusual formatting: "30-second lifespan"
                MATCH_AND_CONTINUE("-", DescWord::Minus)
                MATCH_AND_CONTINUE("+", DescWord::Plus)

                if (TryReadWord("one") ||
                    TryReadWord("an") ||
                    TryReadWord("a"))
                {
                    lits.push_back(SkillParam(1, 1));
                    words.push_back(DescWord::Literal);
                    continue;
                }

                if (TryReadWord("all"))
                {
                    lits.push_back(SkillParam(std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()));
                    words.push_back(DescWord::Literal);
                    continue;
                }

                if (TryReadWord("Half of all"))
                {
                    lits.push_back(SkillParam(50, 50));
                    words.push_back(DescWord::Literal);
                    words.push_back(DescWord::Percent);
                    continue;
                }

                // a
                MATCH_AND_CONTINUE("and", DescWord::And)
                MATCH_AND_CONTINUE("ally", DescWord::Ally)
                MATCH_AND_CONTINUE("above", DescWord::Above)
                MATCH_AND_CONTINUE("after", DescWord::After)
                MATCH_AND_CONTINUE("armor", DescWord::Armor)
                MATCH_AND_CONTINUE("attack", DescWord::Attack)
                MATCH_AND_CONTINUE("absorb", DescWord::Absorb)
                MATCH_AND_CONTINUE("activat", DescWord::Activate)
                MATCH_AND_CONTINUE("attribute", DescWord::Attribute)
                MATCH_AND_CONTINUE("adrenaline", DescWord::Adrenaline)
                MATCH_AND_CONTINUE("additional", DescWord::Additional)

                // b
                MATCH_AND_CONTINUE("burn", DescWord::Burn)
                MATCH_AND_CONTINUE("block", DescWord::Block)
                MATCH_AND_CONTINUE("blind", DescWord::Blind)
                MATCH_AND_CONTINUE("blunt", DescWord::Blunt)
                MATCH_AND_CONTINUE("bleed", DescWord::Bleed)
                MATCH_AND_CONTINUE("below", DescWord::Below)

                // c
                MATCH_AND_CONTINUE("cost", DescWord::Cost)
                MATCH_AND_CONTINUE("cold", DescWord::Cold)
                MATCH_AND_CONTINUE("cast", DescWord::Cast)
                MATCH_AND_CONTINUE("chaos", DescWord::Chaos)
                MATCH_AND_CONTINUE("chant", DescWord::Chant)
                MATCH_AND_CONTINUE("chance", DescWord::Chance)
                MATCH_AND_CONTINUE("cripple", DescWord::Cripple)
                MATCH_AND_CONTINUE("condition and hex", DescWord::ConditionAndHex)
                MATCH_AND_CONTINUE("conditions and hexes", DescWord::ConditionsAndHexes)
                MATCH_AND_CONTINUE("condition", DescWord::Condition)
                MATCH_AND_CONTINUE("Cracked Armor", DescWord::CrackedArmor)

                // d
                MATCH_AND_CONTINUE("die", DescWord::Die)
                MATCH_AND_CONTINUE("daze", DescWord::Daze)
                MATCH_AND_CONTINUE("dark", DescWord::Dark)
                MATCH_AND_CONTINUE("degen", DescWord::Degen)
                MATCH_AND_CONTINUE("damage", DescWord::Damage)
                MATCH_AND_CONTINUE("disease", DescWord::Disease)
                MATCH_AND_CONTINUE("disable", DescWord::Disable)
                MATCH_AND_CONTINUE("Deep Wound", DescWord::DeepWound)

                // e
                MATCH_AND_CONTINUE("end", DescWord::End)
                MATCH_AND_CONTINUE("earth", DescWord::Earth)
                MATCH_AND_CONTINUE("expire", DescWord::Expire)
                MATCH_AND_CONTINUE("effect", DescWord::Effect)
                MATCH_AND_CONTINUE("Energy Storage", DescWord::EnergyStorage)
                MATCH_AND_CONTINUE("energy", DescWord::Energy)
                MATCH_AND_CONTINUE("elemental", DescWord::Elemental)
                MATCH_AND_CONTINUE("enchantment", DescWord::Enchantment)

                // f
                MATCH_AND_CONTINUE("foe", DescWord::Foe)
                MATCH_AND_CONTINUE("for", DescWord::For)
                MATCH_AND_CONTINUE("fail", DescWord::Fail)
                MATCH_AND_CONTINUE("fire", DescWord::Fire)
                MATCH_AND_CONTINUE("faster", DescWord::Faster)
                MATCH_AND_CONTINUE("for each", DescWord::ForEach)

                // g
                MATCH_AND_CONTINUE("gain", DescWord::Gain)

                // h
                MATCH_AND_CONTINUE("hex and condition", DescWord::ConditionAndHex)
                MATCH_AND_CONTINUE("hexes and conditions", DescWord::ConditionsAndHexes)
                MATCH_AND_CONTINUE("hex", DescWord::Hex)
                MATCH_AND_CONTINUE("holy", DescWord::Holy)
                MATCH_AND_CONTINUE("health", DescWord::Health)
                MATCH_AND_CONTINUE("heal", DescWord::Heal)

                // i
                MATCH_AND_CONTINUE("if", DescWord::If)
                MATCH_AND_CONTINUE("interrupt", DescWord::Interrupt)
                MATCH_AND_CONTINUE("illusion", DescWord::IllusionMagic)

                // j

                // k
                MATCH_AND_CONTINUE("knock", DescWord::Knockdown)

                // l
                MATCH_AND_CONTINUE("los", DescWord::Lose)
                MATCH_AND_CONTINUE("last", DescWord::Last)
                MATCH_AND_CONTINUE("less", DescWord::Less)
                MATCH_AND_CONTINUE("level", DescWord::Level)
                MATCH_AND_CONTINUE("longer", DescWord::Longer)
                MATCH_AND_CONTINUE("lifespan", DescWord::Lifespan)
                MATCH_AND_CONTINUE("lightning", DescWord::Lightning)

                // m
                MATCH_AND_CONTINUE("max", DescWord::Max)
                MATCH_AND_CONTINUE("miss", DescWord::Miss)
                MATCH_AND_CONTINUE("more", DescWord::More)
                MATCH_AND_CONTINUE("melee", DescWord::Melee)
                MATCH_AND_CONTINUE("move", DescWord::Movement)

                // n
                MATCH_AND_CONTINUE("non-spirit", DescWord::NonSpirit)
                MATCH_AND_CONTINUE("no", DescWord::Not)
                MATCH_AND_CONTINUE("cannot", DescWord::Not)
                MATCH_AND_CONTINUE("next", DescWord::Next)

                // o
                MATCH_AND_CONTINUE("or", DescWord::Or)

                // p
                MATCH_AND_CONTINUE("poison", DescWord::Poison)
                MATCH_AND_CONTINUE("physical", DescWord::Physical)
                MATCH_AND_CONTINUE("piercing", DescWord::Piercing)
                MATCH_AND_CONTINUE("proximity", DescWord::Proximity)

                // q

                // r
                MATCH_AND_CONTINUE("regen", DescWord::Regen)
                MATCH_AND_CONTINUE("ranged", DescWord::Ranged)
                MATCH_AND_CONTINUE("remove", DescWord::Remove)
                MATCH_AND_CONTINUE("reduc", DescWord::Reduce)
                MATCH_AND_CONTINUE("recharge", DescWord::Recharge)

                // s
                MATCH_AND_CONTINUE("start", DescWord::Start)
                MATCH_AND_CONTINUE("spell", DescWord::Spell)
                MATCH_AND_CONTINUE("steal", DescWord::Steal)
                MATCH_AND_CONTINUE("stance", DescWord::Stance)
                MATCH_AND_CONTINUE("signet", DescWord::Signet)
                MATCH_AND_CONTINUE("skills", DescWord::Skills)
                MATCH_AND_CONTINUE("shadow", DescWord::Shadow)
                MATCH_AND_CONTINUE("slower", DescWord::Slower)
                MATCH_AND_CONTINUE("secondary", DescWord::Secondary)
                MATCH_AND_CONTINUE("second", DescWord::Seconds)
                MATCH_AND_CONTINUE("slashing", DescWord::Slashing)

                // t
                MATCH_AND_CONTINUE("target", DescWord::Target)
                MATCH_AND_CONTINUE("transfer", DescWord::Transfer)

                // u

                // v

                // w
                MATCH_AND_CONTINUE("weak", DescWord::Weakness)
                MATCH_AND_CONTINUE("whenever", DescWord::Whenever)
                MATCH_AND_CONTINUE("water magic", DescWord::WaterMagic)

                // x

                // y

                // z
            }
            p++;
        }
    }

    // "We have AI at home"
    void ParseContext(FixedArrayRef<ParsedSkillParam> pps, ParsedSkillParam &pp, std::span<DescWord> sentence, int32_t lit_pos)
    {
        auto n_words = sentence.size();

        auto IsMatch = [&](int32_t index, DescWord word)
        {
            if (index >= 0 && index < n_words)
                return sentence[index] == word;
            return false;
        };
        auto IsListMatch = [&](int32_t index, std::initializer_list<DescWord> words)
        {
            if (index >= 0 && index + words.size() <= n_words)
            {
                return std::memcmp(&sentence[index], words.begin(), words.size() * sizeof(DescWord)) == 0;
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
                    case DescWord::Fire:      result = DamageType::Fire;      break;
                    case DescWord::Lightning: result = DamageType::Lightning; break;
                    case DescWord::Earth:     result = DamageType::Earth;     break;
                    case DescWord::Cold:      result = DamageType::Cold;      break;
                    case DescWord::Slashing:  result = DamageType::Slashing;  break;
                    case DescWord::Piercing:  result = DamageType::Piercing;  break;
                    case DescWord::Blunt:     result = DamageType::Blunt;     break;
                    case DescWord::Chaos:     result = DamageType::Chaos;     break;
                    case DescWord::Dark:      result = DamageType::Dark;      break;
                    case DescWord::Holy:      result = DamageType::Holy;      break;
                    case DescWord::Shadow:    result = DamageType::Shadow;    break;
                }
                // clang-format on
            }
            return result;
        };

        auto PushParam = [&](SkillParamType ty, bool is_negative = false)
        {
            pp.type = ty;
            pp.is_negative = is_negative;
            pps.try_push(pp);
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

        bool has_plus = IsMatch(lit_pos - 1, DescWord::Plus);
        bool has_minus = IsMatch(lit_pos - 1, DescWord::Minus);
        bool has_percent = IsMatch(lit_pos + 1, DescWord::Percent);
        bool is_less = has_minus;
        bool is_more = has_plus;
        bool is_reduced = false;

        int32_t just_after = has_percent ? lit_pos + 2 : lit_pos + 1;
        int32_t just_before = has_minus || has_plus ? lit_pos - 2 : lit_pos - 1;

        if (IsMatch(just_after, DescWord::Less))
        {
            is_less = true;
            just_after++;
        }

        if (IsMatch(just_after, DescWord::More) ||
            IsMatch(just_after, DescWord::Additional))
        {
            is_more = true;
            just_after++;
        }

        if (IsMatch(just_before, DescWord::Additional))
        {
            is_more = true;
            just_before--;
        }

        for (int32_t i = 0; i <= just_before; i++)
        {
            if (IsMatch(i, DescWord::Reduce))
            {
                is_reduced = true;
                break;
            }
        }

        auto GetPropsBefore = [&](std::span<const DescWord> words, std::function<void(DescWord)> handler)
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
                    (sentence[i] == DescWord::And ||
                        sentence[i] == DescWord::Comma))
                    i--;
            }
            return any_found;
        };

        if (has_percent)
        {
            if (IsListMatch(just_after, {DescWord::Chance, DescWord::Block}) ||
                IsListMatch(just_after, {DescWord::Block, DescWord::Chance}))
                PUSH_PARAM_AND_RETURN(SkillParamType::ChanceToBlock)

            if (IsListMatch(just_after, {DescWord::Fail, DescWord::Chance}))
                PUSH_PARAM_AND_RETURN(SkillParamType::ChanceToFail)

            if (IsListMatch(just_after, {DescWord::Chance, DescWord::Miss}) ||
                IsMatch(just_before, DescWord::Miss))
                PUSH_PARAM_AND_RETURN(SkillParamType::ChanceToMiss)

            if (IsMatch(just_after, DescWord::Faster))
            {
                if (IsMatch(just_before, DescWord::Recharge) ||
                    IsListMatch(just_before - 1, {DescWord::Recharge, DescWord::Skills}))
                    PUSH_PARAM_AND_RETURN(SkillParamType::RechargeTimeMod, true)

                constexpr std::array<DescWord, 2> allowed = {DescWord::Movement, DescWord::Attack};
                auto Handler = [&](DescWord prop)
                {
                    if (prop == DescWord::Movement)
                        PushParam(SkillParamType::MovementSpeedMod);
                    else if (prop == DescWord::Attack)
                        PushParam(SkillParamType::AttackTimeMod, true);
                };
                if (GetPropsBefore(allowed, Handler))
                    return;

                if (IsMatch(just_before, DescWord::Expire))
                    PUSH_PARAM_AND_RETURN(SkillParamType::DurationMod, true)
            }

            if (IsMatch(just_after, DescWord::Slower))
            {
                if (IsMatch(just_before, DescWord::Recharge) ||
                    IsListMatch(just_before - 1, {DescWord::Recharge, DescWord::Skills}))
                    PUSH_PARAM_AND_RETURN(SkillParamType::RechargeTimeMod)

                if (IsMatch(just_before, DescWord::Movement) ||
                    IsMatch(just_after + 1, DescWord::Movement))
                    PUSH_PARAM_AND_RETURN(SkillParamType::MovementSpeedMod, true)

                if (IsMatch(just_before, DescWord::Attack))
                    PUSH_PARAM_AND_RETURN(SkillParamType::AttackTimeMod)
            }

            if (IsMatch(just_after, DescWord::Longer))
            {
                if (IsMatch(just_before, DescWord::Last))
                    PUSH_PARAM_AND_RETURN(SkillParamType::DurationMod)
            }

            if (IsMatch(just_after, DescWord::Damage))
            {
                if (is_less)
                    PUSH_PARAM_AND_RETURN(SkillParamType::DamageMod, true)
                else if (is_more)
                    PUSH_PARAM_AND_RETURN(SkillParamType::DamageMod)
            }

            if (IsMatch(just_after, DescWord::Heal))
            {
                if (is_less)
                    PUSH_PARAM_AND_RETURN(SkillParamType::HealMod, true)
            }
            if (is_reduced)
            {
                for (int32_t i = just_before; i >= 0; i--)
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescWord::Heal, SkillParamType::HealMod, true)
                }
            }
        }
        else // No % after literal
        {
            IF_MATCH_PUSH_PARAM_AND_RETURN(just_before, DescWord::Level, SkillParamType::Level)

            if (IsMatch(just_before, DescWord::Max))
            {
                if (IsMatch(just_before - 1, DescWord::Damage) ||
                    IsMatch(just_after, DescWord::Damage))
                {
                    if (IsMatch(just_after + 1, DescWord::Reduce))
                        PUSH_PARAM_AND_RETURN(SkillParamType::DamageReduction)

                    PUSH_PARAM_AND_RETURN(SkillParamType::Damage)
                }
            }

            if (IsMatch(just_after, DescWord::Seconds))
            {
                if (IsMatch(just_after + 1, DescWord::Lifespan) ||
                    IsMatch(just_before, DescWord::Lifespan) ||
                    IsListMatch(just_before - 1, {DescWord::Die, DescWord::After}))
                {
                    PUSH_PARAM_AND_RETURN(SkillParamType::Duration);
                }

                if ((IsMatch(just_before, DescWord::Recharge) && IsMatch(just_after + 1, DescWord::Faster)))
                {
                    PUSH_PARAM_AND_RETURN(SkillParamType::RechargeTimeAdd, true)
                }

                if (is_more && IsMatch(just_after + 1, DescWord::Recharge))
                {
                    PUSH_PARAM_AND_RETURN(SkillParamType::RechargeTimeAdd)
                }

                if ((IsMatch(just_before, DescWord::Activate) && IsMatch(just_after + 1, DescWord::Faster)))
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(just_before, DescWord::Cast, SkillParamType::ActivationTimeAdd, true)
                }
                else if (!IsMatch(just_before, DescWord::Max))
                {
                    FixedArray<SkillParamType, 8> salloc;
                    auto buffer = salloc.ref();
                    for (int32_t i = just_before; i >= 0; i--)
                    {
                        if (IsMatch(i, DescWord::Seconds))
                            break;
                        // clang-format off
                        switch (sentence[i]) {
                            case DescWord::Disable: PUSH_PARAM_AND_RETURN(SkillParamType::Disable)

                            case DescWord::Cripple:      buffer.try_push(SkillParamType::Crippled);     break;
                            case DescWord::Blind:        buffer.try_push(SkillParamType::Blind);        break;
                            case DescWord::Weakness:     buffer.try_push(SkillParamType::Weakness);     break;
                            case DescWord::CrackedArmor: buffer.try_push(SkillParamType::CrackedArmor); break;
                            case DescWord::Daze:         buffer.try_push(SkillParamType::Dazed);        break;
                            case DescWord::Bleed:        buffer.try_push(SkillParamType::Bleeding);     break;
                            case DescWord::Poison:       buffer.try_push(SkillParamType::Poison);       break;
                            case DescWord::Disease:      buffer.try_push(SkillParamType::Disease);      break;
                            case DescWord::Burn:         buffer.try_push(SkillParamType::Burning);      break;
                            case DescWord::DeepWound:    buffer.try_push(SkillParamType::DeepWound);    break;
                        }
                        // clang-format on
                    }
                    if (buffer.size() > 0)
                    {
                        while (buffer.size() > 0)
                            PushParam(buffer.pop());
                        return;
                    }
                    PUSH_PARAM_AND_RETURN(SkillParamType::Duration);
                }
            }

            if (IsMatch(just_after, DescWord::Armor))
            {
                if (is_less)
                    PUSH_PARAM_AND_RETURN(SkillParamType::ArmorChange, true)
                else
                    PUSH_PARAM_AND_RETURN(SkillParamType::ArmorChange)
            }

            if (IsListMatch(just_after, {DescWord::Max, DescWord::Health}) ||
                IsListMatch(0, {DescWord::Max, DescWord::Health}))
            {
                if (is_less || is_reduced)
                    PUSH_PARAM_AND_RETURN(SkillParamType::MaxHealthAdd, true)
                else
                    PUSH_PARAM_AND_RETURN(SkillParamType::MaxHealthAdd)
            }

            if (IsMatch(just_after, DescWord::Health))
            {
                IF_MATCH_PUSH_PARAM_AND_RETURN(just_after + 1, DescWord::Regen, SkillParamType::HealthPips)
                IF_MATCH_PUSH_PARAM_AND_RETURN(just_after + 1, DescWord::Degen, SkillParamType::HealthPips, true)
                for (int32_t i = just_before; i >= 0; i--)
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescWord::Gain, SkillParamType::HealthGain)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescWord::Lose, SkillParamType::HealthLoss)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescWord::Steal, SkillParamType::HealthSteal)
                }
            }

            if (IsMatch(just_after, DescWord::Energy) && !IsMatch(0, DescWord::Not))
            {
                IF_MATCH_PUSH_PARAM_AND_RETURN(just_after + 1, DescWord::Regen, SkillParamType::EnergyPips)
                IF_MATCH_PUSH_PARAM_AND_RETURN(just_after + 1, DescWord::Degen, SkillParamType::EnergyPips, true)
                IF_MATCH_PUSH_PARAM_AND_RETURN(just_after + 1, DescWord::Lose, SkillParamType::EnergyLoss)

                if (is_less)
                    PUSH_PARAM_AND_RETURN(SkillParamType::EnergyDiscount)

                for (int32_t i = just_before; i >= 0; i--)
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescWord::Gain, SkillParamType::EnergyGain)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescWord::Lose, SkillParamType::EnergyLoss)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescWord::Steal, SkillParamType::EnergySteal)
                }
            }

            if (IsMatch(just_after, DescWord::Adrenaline))
            {
                for (int32_t i = just_before; i >= 0; i--)
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescWord::Gain, SkillParamType::AdrenalineGain)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescWord::Lose, SkillParamType::AdrenalineLoss)
                }
            }

            if (IsMatch(just_before, DescWord::For))
            {
                for (int32_t i = just_before - 1; i >= 0; i--)
                {
                    IF_MATCH_PUSH_PARAM_AND_RETURN(i, DescWord::Heal, SkillParamType::Heal)
                }
            }

            {
                auto dmg_type = GetDmgType(just_after);
                auto i = just_after;
                if (dmg_type)
                {
                    pp.damage_type = dmg_type;
                    i++;
                }
                if (IsMatch(i, DescWord::Damage))
                {
                    if (is_less || IsMatch(i + 1, DescWord::Reduce))
                        PUSH_PARAM_AND_RETURN(SkillParamType::DamageReduction)

                    for (int32_t j = just_before; j >= 0; j--)
                    {
                        IF_MATCH_PUSH_PARAM_AND_RETURN(j, DescWord::Absorb, SkillParamType::DamageReduction)
                    }

                    PUSH_PARAM_AND_RETURN(SkillParamType::Damage)
                }
            }

            for (int32_t i = just_before; i >= 0; i--)
            {
                if (IsListMatch(i - 2, {DescWord::Gain, DescWord::Or, DescWord::Lose}))
                    continue;

                if ((IsMatch(i, DescWord::Transfer) ||
                        IsMatch(i, DescWord::Remove) ||
                        IsMatch(i, DescWord::Lose)) &&
                    !IsMatch(i - 1, DescWord::Whenever))
                {
                    if (IsMatch(just_after, DescWord::ConditionAndHex) ||
                        IsMatch(just_after, DescWord::ConditionsAndHexes))
                    {
                        PushParam(SkillParamType::ConditionsRemoved);
                        PushParam(SkillParamType::HexesRemoved);
                        return;
                    }
                    IF_MATCH_PUSH_PARAM_AND_RETURN(just_after, DescWord::Condition, SkillParamType::ConditionsRemoved)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(just_after, DescWord::Hex, SkillParamType::HexesRemoved)
                    IF_MATCH_PUSH_PARAM_AND_RETURN(just_after, DescWord::Enchantment, SkillParamType::EnchantmentsRemoved)
                }
            }
        }

        if (is_reduced)
        {
            for (int32_t i = just_before; i > 0; i--)
            {
                if (IsMatch(i, DescWord::Attribute))
                    break;

                if (IsMatch(i, DescWord::Damage))
                {
                    if (has_percent)
                        PUSH_PARAM_AND_RETURN(SkillParamType::DamageMod, true)
                    else
                        PUSH_PARAM_AND_RETURN(SkillParamType::DamageReduction)
                }
            }
        }
    }

    void ParseSkillData(CustomSkillData &custom_sd)
    {
        // if (custom_sd.skill_id == GW::Constants::SkillID::Fragility)

        auto rich_desc = custom_sd.TryGetDescription(true, -1);
        assert(rich_desc);
        auto &desc = rich_desc->str;
        auto machine_description = MachineDesc(desc);

        auto words = machine_description.words;
        auto start = words.data();
        auto end = start + words.size();
        auto w = start;
        uint32_t lit_counter = 0;

        custom_sd.end_effect_index = -1;
        while (w < end)
        {
            if (w[0] == DescWord::End && w + 1 < end && w[1] == DescWord::Effect)
            {
                assert(custom_sd.end_effect_index == -1);
                custom_sd.end_effect_index = custom_sd.parsed_params.ref().size();

                w += 2;
                continue;
            }

            if (*w == DescWord::Literal)
            {
                auto sentence_start = w;
                while (start < sentence_start && (sentence_start[-1] != DescWord::Period))
                    sentence_start--;
                auto sentence_end = w;
                while (sentence_end < end && (sentence_end[0] != DescWord::Period))
                    sentence_end++;

                auto sentence = std::span(sentence_start, sentence_end - sentence_start);
                auto lit_pos = w - sentence_start;
                auto pps = custom_sd.parsed_params.ref();
                auto pp = ParsedSkillParam();
                auto lit = machine_description.lits[lit_counter++];
                if (!lit.IsNull())
                {
                    if (custom_sd.skill_id == GW::Constants::SkillID::Brambles)
                    {
                        int a = 0;
                    }
                    pp.param = lit;
                    ParseContext(pps, pp, sentence, lit_pos);
                }
            }
            w++;
        }
        custom_sd.end_effect_index = custom_sd.parsed_params.ref().size();
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

    GW::Constants::SkillID SkillParamTypeToSkillID(SkillParamType type)
    {
        // clang-format off
        switch (type) {
            case SkillParamType::Bleeding:     return GW::Constants::SkillID::Bleeding;
            case SkillParamType::Blind:        return GW::Constants::SkillID::Blind;
            case SkillParamType::Burning:      return GW::Constants::SkillID::Burning;
            case SkillParamType::CrackedArmor: return GW::Constants::SkillID::Cracked_Armor;
            case SkillParamType::Crippled:     return GW::Constants::SkillID::Crippled;
            case SkillParamType::Dazed:        return GW::Constants::SkillID::Dazed;
            case SkillParamType::DeepWound:    return GW::Constants::SkillID::Deep_Wound;
            case SkillParamType::Disease:      return GW::Constants::SkillID::Disease;
            case SkillParamType::Poison:       return GW::Constants::SkillID::Poison;
            case SkillParamType::Weakness:     return GW::Constants::SkillID::Weakness;
            
            default:                           return GW::Constants::SkillID::No_Skill;
        }
        // clang-format on
    }

    std::string_view SkillParamTypeToString(SkillParamType type)
    {
        // clang-format off
        switch (type) {
            case SkillParamType::Duration:          return "Duration (s)";
            case SkillParamType::Level:             return "Level";
            case SkillParamType::Disable:           return "Disable (s)";
            case SkillParamType::ArmorChange:       return "Armor";
            case SkillParamType::MovementSpeedMod:  return "Movement speed modifier (%)";
            case SkillParamType::AttackTimeMod:     return "Attack time modifier (%)";
            case SkillParamType::RechargeTimeMod:   return "Recharge time modifier (%)";
            case SkillParamType::RechargeTimeAdd:   return "Recharge time added (s)";
            case SkillParamType::ActivationTimeAdd: return "Activation time added (s)";
            case SkillParamType::DurationMod:       return "Duration modifier (%)";

            case SkillParamType::ConditionsRemoved:   return "Conditions removed";
            case SkillParamType::HexesRemoved:        return "Hexes removed";
            case SkillParamType::EnchantmentsRemoved: return "Enchantments removed";

            case SkillParamType::Heal:             return "Healing";
            case SkillParamType::HealMod:          return "Healing modifier (%)";

            case SkillParamType::Damage:           return "Damage";
            case SkillParamType::DamageReduction:  return "Damage reduction";
            case SkillParamType::DamageMod:        return "Damage modifier (%)";

            case SkillParamType::ChanceToBlock:    return "Block chance (%)";
            case SkillParamType::ChanceToFail:     return "Failure chance (%)";
            case SkillParamType::ChanceToMiss:     return "Miss chance (%)";

            case SkillParamType::MaxHealthAdd:     return "Max health";
            case SkillParamType::HealthPips:       return "Health pips";
            case SkillParamType::HealthGain:       return "Health gain";
            case SkillParamType::HealthLoss:       return "Health loss";
            case SkillParamType::HealthSteal:      return "Health steal";

            case SkillParamType::EnergyDiscount:   return "Energy discount";
            case SkillParamType::EnergyPips:       return "Energy pips";
            case SkillParamType::EnergyGain:       return "Energy gain";
            case SkillParamType::EnergyLoss:       return "Energy loss";
            case SkillParamType::EnergySteal:      return "Energy steal";

            case SkillParamType::AdrenalineGain:   return "Adrenaline gain";
            case SkillParamType::AdrenalineLoss:   return "Adrenaline loss";

            case SkillParamType::Bleeding:         return "Bleeding (s)";
            case SkillParamType::Blind:            return "Blind (s)";
            case SkillParamType::Burning:          return "Burning (s)";
            case SkillParamType::CrackedArmor:     return "Cracked Armor (s)";
            case SkillParamType::Crippled:         return "Crippled (s)";
            case SkillParamType::Dazed:            return "Dazed (s)";
            case SkillParamType::DeepWound:        return "Deep Wound (s)";
            case SkillParamType::Disease:          return "Disease (s)";
            case SkillParamType::Poison:           return "Poison (s)";
            case SkillParamType::Weakness:         return "Weakness (s)";

            case SkillParamType::Null:             return "";
            default:                               return "ERROR";
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

    void ParsedSkillParam::ImGuiRender(int8_t attr_lvl)
    {
        auto str = SkillParamTypeToString(type);
        ImGui::TextUnformatted(str.data(), str.data() + str.size());
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

        if (type > SkillParamType::MAY_BE_NEGATIVE_AFTER)
        {
            ImGui::SameLine();
            if (is_negative)
            {
                ImGui::TextUnformatted("-");
            }
            else
            {
                ImGui::TextUnformatted("+");
            }
        }

        ImGui::SameLine();
        param.ImGuiRender(attr_lvl);

        if (type > SkillParamType::PERCENT_START && type < SkillParamType::PERCENT_END)
        {
            ImGui::SameLine();
            ImGui::Text("%%");
        }
        else if (type > SkillParamType::SECONDS_START && type < SkillParamType::SECONDS_END)
        {
            ImGui::SameLine();
            ImGui::Text(" seconds");
        }
    }

    namespace CustomSkillDataModule
    {
        bool CustomSkillDataModule::is_initialized = false;
        bool TryInitialize()
        {
            auto skills = Utils::GetSkillSpan();
            assert(!skills.empty());

            // Ensure the descriptions and names are decoded
            bool fail = false;
            for (auto &skill : skills)
            {
                const auto skill_id = skill.skill_id;
                auto &custom_sd = custom_skill_datas[(uint32_t)skill_id];
                custom_sd.skill_id = skill_id;
                custom_sd.skill = &skill;

                bool ok = custom_sd.TryGetDescription(true, -1) &&
                          custom_sd.TryGetDescription(false, -1) &&
                          custom_sd.TryGetName();

                if (!ok)
                {
                    fail = true;
                    continue;
                }
            }

            if (fail)
                return false;

            for (auto &custom_sd : custom_skill_datas)
            {
                custom_sd.Init();
            }

            CustomSkillDataModule::is_initialized = true;
            return true;
        }

        CustomSkillData &GetCustomSkillData(GW::Constants::SkillID skill_id)
        {
            assert((uint32_t)skill_id < GW::Constants::SkillMax);
            return custom_skill_datas[(uint32_t)skill_id];
        }
    }

    SkillParam DetermineBaseDuration(CustomSkillData &custom_sd)
    {
        // Typically this param is the duration, but it may sometimes be condition duration or other.
        auto base_duration = custom_sd.GetSkillParam(2);

        if (!base_duration.IsNull())
        {
            // So we have to cross-check with the parsed params
            for (auto pp : custom_sd.parsed_params.ref())
            {
                if (pp.type == SkillParamType::Duration &&
                    pp.param == base_duration)
                {
                    return base_duration;
                }
            }

            // If no match, we only assume it's the duration as long as no other parsed param has the same value
            for (auto pp : custom_sd.parsed_params.ref())
            {
                if (pp.type != SkillParamType::Duration &&
                    pp.param == base_duration)
                {
                    return {0, 0};
                }
            }
        }

        return base_duration;
    }

    EffectTarget DetermineEffectTarget(CustomSkillData &custom_sd)
    {
        auto skill_id = custom_sd.skill_id;
        auto &skill = *custom_sd.skill;

        if (custom_sd.base_duration.IsNull())
            return EffectTarget::None;

        // Special cases
        switch (skill_id)
        {
            // Skills affecting both caster and pet
            case GW::Constants::SkillID::Never_Rampage_Alone:
            case GW::Constants::SkillID::Run_as_One:
            case GW::Constants::SkillID::Rampage_as_One:
            case GW::Constants::SkillID::Strike_as_One:
                return EffectTarget::CasterAndPet;

            case GW::Constants::SkillID::Save_Yourselves_kurzick:
            case GW::Constants::SkillID::Save_Yourselves_luxon:
                return EffectTarget::CasterAOEOtherAllies;

            // Skills with aoe but not application aoe
            case GW::Constants::SkillID::Healing_Seed:
            case GW::Constants::SkillID::Balthazars_Aura:
                return EffectTarget::Target;
        }

        // Skills affecting only pet
        if (skill.attribute == GW::Constants::AttributeByte::BeastMastery)
        {
            switch (skill.type)
            {
                case GW::Constants::SkillType::Shout:
                case GW::Constants::SkillType::Skill:
                case GW::Constants::SkillType::Skill2:
                    return EffectTarget::Pet;
            }
        }

        switch (skill.type)
        {
            case GW::Constants::SkillType::ItemSpell:
            case GW::Constants::SkillType::Preparation:
            case GW::Constants::SkillType::Form:
            case GW::Constants::SkillType::Stance:
                return EffectTarget::Caster;

            case GW::Constants::SkillType::PetAttack:
                return EffectTarget::Pet;

            case GW::Constants::SkillType::WeaponSpell:
                return EffectTarget::Target;

            case GW::Constants::SkillType::Ward:
            case GW::Constants::SkillType::Well:
            case GW::Constants::SkillType::Ritual:
                return EffectTarget::None;

            case GW::Constants::SkillType::Shout:
            {

                // if (skill.aoe_range == 1000)
                // {
                //     Utils::ForAlliesInCircle(caster_agent->pos, 1000, caster_agent->allegiance,
                //         [&](GW::AgentLiving &agent)
                //         {
                //             switch (skill_id)
                //             {
                //                 case GW::Constants::SkillID::Save_Yourselves_kurzick:
                //                 case GW::Constants::SkillID::Save_Yourselves_luxon:
                //                 {
                //                     if (agent.agent_id == caster_id)
                //                         return;
                //                     break;
                //                 }
                //                 case GW::Constants::SkillID::Charge:
                //                 {
                //                     EffectTracking::RemoveTrackers(agent.agent_id,
                //                         [](EffectTracking::EffectTracker &effect)
                //                         {
                //                             return effect.skill_id == GW::Constants::SkillID::Crippled;
                //                         });
                //                     break;
                //                 }
                //             }
                //             EffectTracking::ApplySkillEffect(agent.agent_id, caster_id, skill_effect);
                //         });
                // }
            }
        }

        if (Utils::IsRangeValue(skill.aoe_range))
        {
            auto desc = custom_sd.TryGetPredecodedDescription(custom_sd.GetDescKey(true, -1));
            assert(desc);
            bool party_members = desc->contains("arty members");
            if (party_members)
            {
                return EffectTarget::CasterAOEParty;
            }
            else
            {
                return EffectTarget::TargetAOE;
            }
        }

        return EffectTarget::Target;
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
        if (skill->skill_id_pvp != GW::Constants::SkillID::Count &&
           !skill->IsPvP())                tags.PvEVersion = true;
        if (IsConsumableItemSkill(*skill)) tags.Consumable = true;
        if (IsMaintainedSkill(*skill))     tags.Maintained = true;
        if (IsMissionSkill(skill_id))      tags.Mission = true;
        if (IsCelestialSkill(skill_id))    tags.Celestial = true;
        if (IsBundleSkill(skill_id))       tags.Bundle = true;

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
            skill_id == GW::Constants::SkillID::Well_of_Ruin ||
            skill_id == GW::Constants::SkillID::Aura_of_the_Lich)
            tags.ExploitsCorpse = true;

        if ((tags.Projectile && skill_id != GW::Constants::SkillID::Ice_Spear) ||
            skill->type == GW::Constants::SkillType::Attack)
            tags.HitBased = true;

        ParseSkillData(*this);

        for (auto &pp : parsed_params.ref())
        {
            if (pp.type > SkillParamType::CONDITION_START &&
                pp.type < SkillParamType::CONDITION_END)
            {
                tags.ConditionSource = true;
                break;
            }
        }

        renewal = Renewal::None;
        attribute = AttributeOrTitle(*skill);

        this->base_duration = DetermineBaseDuration(*this);
        this->effect_target = DetermineEffectTarget(*this);
    }

    SkillParam CustomSkillData::GetSkillParam(uint32_t id) const
    {
        return HerosInsight::GetSkillParam(*skill, id);
    }

    SkillParam CustomSkillData::GetParsedSkillParam(std::function<bool(const ParsedSkillParam &)> predicate) const
    {
        for (const auto &param : parsed_params.ref())
        {
            if (predicate(param))
                return param.param;
        }
        return {0, 0};
    }

    void CustomSkillData::GetParsedSkillParams(SkillParamType type, FixedArrayRef<ParsedSkillParam> result) const
    {
        for (const auto &param : parsed_params.ref())
        {
            if (param.type == type)
                result.try_push(param);
        }
    }

    void GetConditionsFromSpan(std::span<const ParsedSkillParam> params, GW::Constants::SkillID source_skill_id, uint8_t attr_lvl, FixedArrayRef<SkillEffect> result)
    {
        bool success = true;
        for (const auto &param : params)
        {
            const auto condition_skill_id = SkillParamTypeToSkillID(param.type);
            if (condition_skill_id == GW::Constants::SkillID::No_Skill)
                continue;
            success &= result.try_push({condition_skill_id, source_skill_id, param.param.Resolve(attr_lvl)});
        }
        SOFT_ASSERT(success, L"Failed to push condition");
    }

    void CustomSkillData::GetInitConditions(uint8_t attr_lvl, FixedArrayRef<SkillEffect> result) const
    {
        if (!tags.ConditionSource)
            return;

        std::span<const ParsedSkillParam> full_span = parsed_params.ref();
        auto span = full_span.subspan(0, end_effect_index);
        GetConditionsFromSpan(span, skill_id, attr_lvl, result);
    }

    void CustomSkillData::GetEndConditions(uint8_t attr_lvl, FixedArrayRef<SkillEffect> result) const
    {
        if (!tags.ConditionSource)
            return;

        std::span<const ParsedSkillParam> full_span = parsed_params.ref();
        auto span = full_span.subspan(end_effect_index, full_span.size() - end_effect_index);
        GetConditionsFromSpan(span, skill_id, attr_lvl, result);
    }

    void CustomSkillData::GetOnActivationEffects(CustomAgentData &caster, uint32_t target_id, FixedArrayRef<SkillEffect> result) const
    {
        if (tags.HitBased)
            return;

        GetInitEffects(caster, target_id, result);
    }

    // Effects carried by the projectile, if any
    void CustomSkillData::GetProjectileEffects(CustomAgentData &caster, FixedArrayRef<SkillEffect> result) const
    {
        if (skill_id != GW::Constants::SkillID::No_Skill)
        {
            if (!tags.Projectile) // Non-projectile skills have no projectile effects
                return;

            if (skill_id == GW::Constants::SkillID::Ice_Spear) // Ice Spear has no projectile effects even though it is a projectile
                return;
        }

        GetInitEffects(caster, 0, result);

        bool is_ranged_attack = skill_id == GW::Constants::SkillID::No_Skill ||
                                (skill->weapon_req & 70);
        if (is_ranged_attack)
        {
            bool has_bow = false;
            auto agent = Utils::GetAgentLivingByID(caster.agent_id);
            if (agent)
                has_bow = agent->weapon_type = 1;

            auto caster_effects = EffectTracking::GetTrackerSpan(caster.agent_id);
            for (const auto &effect : caster_effects)
            {
                auto effect_skill_id = effect.skill_id;
                // clang-format off
                switch (effect_skill_id)
                {
                    case GW::Constants::SkillID::Glass_Arrows:
                    case GW::Constants::SkillID::Glass_Arrows_PvP:
                    case GW::Constants::SkillID::Barbed_Arrows:
                    case GW::Constants::SkillID::Melandrus_Arrows:
                        if (!has_bow)
                            break;
                    case GW::Constants::SkillID::Apply_Poison:
                    {
                        auto condition_skill_id = effect_skill_id == GW::Constants::SkillID::Apply_Poison ?
                            GW::Constants::SkillID::Poison : GW::Constants::SkillID::Bleeding;

                        uint32_t duration_id = effect_skill_id == GW::Constants::SkillID::Barbed_Arrows ? 0 : 1;
                        auto &preparation_skill = CustomSkillDataModule::GetCustomSkillData(effect_skill_id);
                        auto base_duration = preparation_skill.GetSkillParam(duration_id).Resolve(effect.attribute_level);
                        result.try_push({condition_skill_id, effect_skill_id, base_duration});
                        break;
                    }
                }
                // clang-format on
            }
        }
    }

    void CustomSkillData::GetOnHitEffects(CustomAgentData &caster, uint32_t target_id, bool is_projectile, FixedArrayRef<SkillEffect> result) const
    {
        if (IsAttack() && !IsRangedAttack())
        {
            GetInitEffects(caster, target_id, result);
        }

        auto caster_effects = EffectTracking::GetTrackerSpan(caster.agent_id);
        for (const auto &effect : caster_effects)
        {
            auto effect_skill_id = effect.skill_id;

            auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(effect.skill_id);

            switch (effect_skill_id)
            {
                case GW::Constants::SkillID::Apply_Poison: // It does not require physical dmg type, it works with any martial weapon
                    if (is_projectile)
                        break; // We already have the effect from GetProjectileEffects
                case GW::Constants::SkillID::Poison_Tip_Signet:
                {
                    auto Finder = [](const ParsedSkillParam &param)
                    {
                        return param.type == SkillParamType::Poison;
                    };
                    auto base_duration = custom_sd.GetParsedSkillParam(Finder).Resolve(effect.attribute_level);
                    result.try_push({GW::Constants::SkillID::Poison, effect_skill_id, base_duration});

                    break;
                }
            }
        }
    }

    void CustomSkillData::GetInitEffects(CustomAgentData &caster, uint32_t target_id, FixedArrayRef<SkillEffect> result) const
    {
        auto attr_lvl = caster.GetAttrLvlForSkill(*this);

        auto caster_living = Utils::GetAgentLivingByID(caster.agent_id);

        switch (skill_id)
        {
            case GW::Constants::SkillID::Epidemic:
            {
                auto timestamp_now = GW::MemoryMgr::GetSkillTimer();
                auto target_effects = EffectTracking::GetTrackerSpan(target_id);
                for (const auto &effect : target_effects)
                {
                    auto &effect_skill = *GW::SkillbarMgr::GetSkillConstantData(effect.skill_id);
                    if (effect_skill.type == GW::Constants::SkillType::Condition)
                    {
                        auto timestamp_end = effect.GetEndTimestamp();
                        if (timestamp_end > timestamp_now)
                        {
                            auto rem_ms = timestamp_end - timestamp_now;
                            auto rem_sec = (rem_ms + 999) / 1000; // Is wrong?
                            result.try_push({effect.skill_id, skill_id, rem_sec});
                        }
                    }
                }
                break;
            }

            case GW::Constants::SkillID::Ice_Spear:
            case GW::Constants::SkillID::Smoldering_Embers:
            case GW::Constants::SkillID::Magnetic_Surge:
            case GW::Constants::SkillID::Stone_Daggers:
            {
                // For these skills, only if the caster is overcast, we continue.
                if (caster_living)
                {
                    auto overcast_lower_bound = *(float *)&caster_living->h0118;
                    bool is_overcast = overcast_lower_bound < 1.f;
                    if (is_overcast)
                    {
                        // continue
                        break;
                    }
                }
                // return early
                return;
            }
        }

        auto base_duration = ResolveBaseDuration(caster, attr_lvl);
        if (base_duration)
        {
            result.try_push({skill_id, skill_id, base_duration});
        }

        GetInitConditions(attr_lvl, result);

        switch (skill_id)
        {
            case GW::Constants::SkillID::Mystic_Corruption:
                if (caster_living->GetIsEnchanted())
                {
                    result[1].base_duration *= 2;
                }
        }
    }

    std::string *CustomSkillData::TryGetName()
    {
        if (name_guard.IsInit())
            return &name;

        if (name_guard.TryBeginInit())
        {
            const auto skill_id = this->skill_id;
            const auto &skill = *GW::SkillbarMgr::GetSkillConstantData(skill_id);
            const auto size = 32;
            wchar_t buffer[size];
            bool success = GW::UI::UInt32ToEncStr(skill.name, buffer, size);
            assert(success);

            auto Callback = [](void *param, const wchar_t *s)
            {
                auto entry = reinterpret_cast<CustomSkillData *>(param);
                entry->name = Utils::WStrToStr(s);
                entry->name_guard.FinishInit();
            };

            GW::UI::AsyncDecodeStr(buffer, Callback, reinterpret_cast<void *>(this));

            if (name_guard.IsInit())
                return &name;
        }

        return nullptr;
    }

    Utils::RichString *CustomSkillData::TryGetDescription(bool is_concise, int32_t attribute_lvl)
    {
        // #ifdef _TIMING
        //         auto start_timestamp = std::chrono::high_resolution_clock::now();
        // #endif

        const auto key = GetDescKey(is_concise, attribute_lvl);
        if (key == last_desc_key)
        {
            // We already have the exact description
            return &last_desc;
        }
        if (key == last_concise_key)
        {
            // We already have the concise description
            return &last_concise;
        }

        auto generic_desc = TryGetPredecodedDescription(key);
        if (generic_desc)
        {
            // The description is decoded but not specialized
            auto &desc = is_concise ? last_concise : last_desc;
            auto &last_key = is_concise ? last_concise_key : last_desc_key;

            desc.str.assign(*generic_desc);
            assert(desc.str.data() != generic_desc->data());
            desc.color_changes.clear();
            desc.tooltips.clear();
            std::string_view replacements[3];

            FixedArray<char, 64> salloc;
            auto buffer = salloc.ref();
            for (uint32_t i = 0; i < 3; i++)
            {
                auto p = buffer.data_end();
                auto len = GetSkillParam(i).Print(buffer, attribute_lvl);
                replacements[i] = std::string_view(p, len);
            }
            // Specialize the description
            Utils::UnrichText(desc.str, desc.color_changes, desc.tooltips, replacements);
            last_key = key;

            return &desc;
        }

        // #ifdef _TIMING
        //         auto end_timestamp = std::chrono::high_resolution_clock::now();
        //         auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_timestamp - start_timestamp).count();
        //         Utils::FormatToChat(L"TryGetDescription took {} micro s", duration);
        // #endif

        return nullptr;
    }

    std::string CustomSkillData::ToString() const
    {
        FixedArray<char, 64> salloc1;
        auto result = salloc1.ref();

        FixedArray<ParsedSkillParam, 3> salloc2;
        auto health_pips = salloc2.ref();

        GetParsedSkillParams(SkillParamType::HealthPips, health_pips);

        for (const auto &pp : health_pips)
        {
            auto reg_or_deg = pp.param.val0 < 0 ? "degen" : "regen";

            result.PushFormat("Health %s: ", reg_or_deg);
            pp.param.Print(result, -1, true);
        }

        return std::string(result.data(), result.size());
    }

    bool CustomSkillData::IsFlash() const
    {
        return skill->special & (uint32_t)Utils::SkillSpecialFlags::FlashEnchantment;
    }

    bool CustomSkillData::IsAttack() const
    {
        return skill->type == GW::Constants::SkillType::Attack;
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
            case GW::Constants::SkillID::Signet_of_Illusions:
            {
                caster.signet_of_illusions_charges = custom_sd.GetSkillParam(0).Resolve(attr_lvl);
                break;
            }

            case GW::Constants::SkillID::Barrage:
            case GW::Constants::SkillID::Volley:
            {
                EffectTracking::RemoveTrackers(caster_id, [](EffectTracking::EffectTracker &effect)
                    {
                        return GW::SkillbarMgr::GetSkillConstantData(effect.skill_id)->type == GW::Constants::SkillType::Preparation; //
                    });
            }
        }

        FixedArray<SkillEffect, 18> skill_effects_salloc;
        auto skill_effects = skill_effects_salloc.ref();

        custom_sd.GetOnActivationEffects(caster, target_id, skill_effects);

        if (skill_effects.size() == 0)
            return;

        auto range = (float)custom_sd.GetAoE();

        std::vector<uint32_t> target_ids; // May become large, so we use a heap allocated buffer
        if (custom_sd.skill->type == GW::Constants::SkillType::Enchantment &&
            Utils::GetAgentRelation(caster_id, target_id) == Utils::AgentRelations::Hostile)
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
                        case GW::Constants::SkillID::Extend_Conditions:
                        case GW::Constants::SkillID::Epidemic:
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
                case GW::Constants::SkillID::Shockwave:
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

    void CustomSkillData::GetRanges(FixedArrayRef<Utils::Range> out) const
    {
        bool success = true;
        if (Utils::IsRangeValue(skill->aoe_range))
            success &= out.try_push((Utils::Range)skill->aoe_range);
        if (Utils::IsRangeValue(skill->const_effect))
            success &= out.try_push((Utils::Range)skill->const_effect);
        if (skill->bonusScale0 == skill->bonusScale15 && Utils::IsRangeValue(skill->bonusScale0))
            success &= out.try_push((Utils::Range)skill->bonusScale0);
    }

    uint32_t CustomSkillData::ResolveBaseDuration(CustomAgentData &caster, std::optional<uint8_t> skill_attr_lvl_override) const
    {
        switch (skill_id) // Special cases
        {
            case GW::Constants::SkillID::Cultists_Fervor:
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
                case GW::Constants::SkillID::Lead_the_Way:
                {
                    auto caster_agent = Utils::GetAgentLivingByID(caster.agent_id);

                    if (caster_agent)
                    {
                        uint32_t n_allies = 0;
                        Utils::ForAlliesInCircle(caster_agent->pos, (float)Utils::Range::Earshot, caster_agent->allegiance,
                            [&](GW::AgentLiving &agent)
                            {
                                n_allies++;
                            });
                        base_duration = std::min(n_allies * base_duration, 20u);
                    }
                }
            }

            return base_duration;
        }

        return 0;
    }

    FixedArray<std::string, 128> skill_type_strings;
    std::string_view CustomSkillData::GetTypeString()
    {
        if (type_str.data() != nullptr)
            return type_str;

        // We dont have the string cached, generate it

        auto skill_type = skill->type;
        if (skill_type == GW::Constants::SkillType::Skill2)
        {
            skill_type = GW::Constants::SkillType::Skill;
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
            case GW::Constants::SkillType::Bounty:      str += "Blessing";    break;
            case GW::Constants::SkillType::Scroll:      str += "Party Bonus"; break;
            case GW::Constants::SkillType::Stance:      str += "Stance";      break;
            case GW::Constants::SkillType::Signet:      str += "Signet";      break;
            case GW::Constants::SkillType::Condition:   str += "Condition";   break;
            case GW::Constants::SkillType::Glyph:       str += "Glyph";       break;
            case GW::Constants::SkillType::Shout:       str += "Shout";       break;
            case GW::Constants::SkillType::Preparation: str += "Preparation"; break;
            case GW::Constants::SkillType::Trap:        str += "Trap";        break;
            case GW::Constants::SkillType::Form:        str += "Form";        break;
            case GW::Constants::SkillType::Chant:       str += "Chant";       break;
            case GW::Constants::SkillType::EchoRefrain: str += "Echo";        break;
            case GW::Constants::SkillType::Disguise:    str += "Disguise";    break;

            case GW::Constants::SkillType::Hex:         str += "Hex ";         goto spell;
            case GW::Constants::SkillType::Enchantment: str += "Enchantment "; goto spell;
            case GW::Constants::SkillType::Well:        str += "Well ";        goto spell;
            case GW::Constants::SkillType::Ward:        str += "Ward ";        goto spell;
            case GW::Constants::SkillType::WeaponSpell: str += "Weapon ";      goto spell;
            case GW::Constants::SkillType::ItemSpell:   str += "Item ";        goto spell;
            case GW::Constants::SkillType::Spell:
                spell:
                str += "Spell";
                break;

            case GW::Constants::SkillType::Skill:
            // case GW::Constants::SkillType::Skill2: if (is_effect_only) { str += "Effect"; } else { str += "Skill"; } break;
            case GW::Constants::SkillType::Skill2: str += "Skill"; break;

            case GW::Constants::SkillType::PetAttack: str += "Pet "; goto attack;
            case GW::Constants::SkillType::Attack:
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

            case GW::Constants::SkillType::Title:             str += "Title ";              goto effect;
            case GW::Constants::SkillType::Passive:           str += "Passive ";            goto effect;
            case GW::Constants::SkillType::Environmental:     str += "Environmental ";      goto effect;
            case GW::Constants::SkillType::EnvironmentalTrap: str += "Environmental Area ";
                effect:
                str += "Effect";
                break;

            case GW::Constants::SkillType::Ritual:
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

        auto cached_strs = skill_type_strings.ref();
        for (auto &cached_str : cached_strs)
        {
            if (cached_str == str)
            {
                type_str = cached_str;
                return type_str;
            }
        }

        if (cached_strs.try_push(std::move(str)))
        {
            type_str = cached_strs[cached_strs.size() - 1];
        }
        else
        {
            SOFT_ASSERT(false, L"Could not find a free slot for skill type string");
            type_str = "ERROR";
        }

        return type_str;
    }

    std::string_view CustomSkillData::GetProfessionString()
    {
        if (profession_str.data() == nullptr)
        {
            profession_str = Utils::GetProfessionString(skill->profession);
        }

        return profession_str;
    }

    std::string_view CustomSkillData::GetCampaignString()
    {
        if (campaign_str.data() == nullptr)
        {
            campaign_str = Utils::GetCampaignString(skill->campaign);
        }

        return campaign_str;
    }

    std::string_view CustomSkillData::GetAttributeString()
    {
        if (attr_str.data() == nullptr)
        {
            attr_str = attribute.GetStr();
        }

        return attr_str;
    }

    std::string_view SkillTagToString(SkillTag tag)
    {
        if (tag >= SkillTag::CONTEXT && tag < SkillTag::CONTEXT_END)
        {
            auto context = (Utils::SkillContext)((uint32_t)tag - (uint32_t)SkillTag::CONTEXT);
            return Utils::GetSkillContextString(context);
        }

        // clang-format off
        switch (tag) {
            case SkillTag::Archived:          return "Archived";
            case SkillTag::EffectOnly:        return "Effect-only";
            case SkillTag::PvEOnly:           return "PvE-only";
            case SkillTag::PvPOnly:           return "PvP-only";
            case SkillTag::PvEVersion:        return "PvE Version";
            case SkillTag::PvPVersion:        return "PvP Version";
            case SkillTag::Equipable:         return "Equipable";
            case SkillTag::Unlocked:          return "Unlocked";
            case SkillTag::Temporary:         return "Temporary";
            case SkillTag::Locked:            return "Locked";
            case SkillTag::DeveloperSkill:    return "Developer Skill";
            case SkillTag::EnvironmentSkill:  return "Environment Skill";
            case SkillTag::MonsterSkill:      return "Monster Skill";
            case SkillTag::SpiritAttack:      return "Spirit Attack";
            case SkillTag::Maintained:        return "Maintained";
            case SkillTag::ConditionSource:   return "Condition Source";
            case SkillTag::ExploitsCorpse:    return "Exploits Corpse";
            case SkillTag::Consumable:        return "Consumable";
            case SkillTag::Celestial:         return "Celestial";
            case SkillTag::Mission:           return "Mission";
            case SkillTag::Bundle:            return "Bundle";
        }
        // clang-format on

        SOFT_ASSERT(false, L"Invalid SkillTag: {}", (uint32_t)tag);
        return "ERROR";
    }

    void CustomSkillData::GetTags(FixedArrayRef<SkillTag> out) const
    {
        bool success = true;

        auto skill_id_to_check = skill->IsPvP() ? skill->skill_id_pvp : skill->skill_id;

        bool is_equipable = GW::SkillbarMgr::GetIsSkillLearnt(skill->skill_id);
        bool is_unlocked = GW::SkillbarMgr::GetIsSkillUnlocked(skill_id_to_check);
        bool is_locked = tags.Unlockable && !is_unlocked;

        // clang-format off
        if (is_equipable)           success &= out.try_push(SkillTag::Equipable);
        if (is_unlocked)            success &= out.try_push(SkillTag::Unlocked);
        if (tags.Temporary)         success &= out.try_push(SkillTag::Temporary);
        if (is_locked)              success &= out.try_push(SkillTag::Locked);

        if (tags.Archived)          success &= out.try_push(SkillTag::Archived);
        if (tags.EffectOnly)        success &= out.try_push(SkillTag::EffectOnly);
        if (tags.PvEOnly)           success &= out.try_push(SkillTag::PvEOnly);
        if (tags.PvPOnly)           success &= out.try_push(SkillTag::PvPOnly);
        if (tags.PvEVersion)        success &= out.try_push(SkillTag::PvEVersion);
        if (tags.PvPVersion)        success &= out.try_push(SkillTag::PvPVersion);

        if (tags.DeveloperSkill)    success &= out.try_push(SkillTag::DeveloperSkill);
        if (tags.EnvironmentSkill)  success &= out.try_push(SkillTag::EnvironmentSkill);
        if (tags.MonsterSkill)      success &= out.try_push(SkillTag::MonsterSkill);
        if (tags.SpiritAttack)      success &= out.try_push(SkillTag::SpiritAttack);
        
        if (tags.Maintained)        success &= out.try_push(SkillTag::Maintained);
        if (tags.ConditionSource)   success &= out.try_push(SkillTag::ConditionSource);
        if (tags.ExploitsCorpse)    success &= out.try_push(SkillTag::ExploitsCorpse);
        if (tags.Consumable)        success &= out.try_push(SkillTag::Consumable);
        if (tags.Celestial)         success &= out.try_push(SkillTag::Celestial);
        if (tags.Mission)           success &= out.try_push(SkillTag::Mission);
        if (tags.Bundle)            success &= out.try_push(SkillTag::Bundle);
        // clang-format on

        if (context != Utils::SkillContext::Null)
        {
            auto tag = (SkillTag)((uint32_t)SkillTag::CONTEXT + (uint32_t)context);
            success &= out.try_push(tag);
        }

        SOFT_ASSERT(success);
    }
}