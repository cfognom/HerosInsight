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
    bool IsDeveloperSkill(const GW::Skill &skill)
    {
        constexpr auto dev_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::No_Skill,
            GW::Constants::SkillID::Test_Buff,
            GW::Constants::SkillID::Impending_Dhuum,
            GW::Constants::SkillID::Dishonorable,
            GW::Constants::SkillID::Unreliable
        );

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
            (GW::Constants::SkillID)723,  // Vampire
            (GW::Constants::SkillID)3250  // Temple Strike but not elite
            (GW::Constants::SkillID)756,  // Barrage (passive effect)
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
            GW::Constants::SkillID::Ballad_of_Restoration_PvP
        );

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
            GW::Constants::SkillID::Polymock_Stoning
        );

        if (skill.type == GW::Constants::SkillType::Attack &&
            skill.weapon_req & 70)
        {
            return true;
        }

        return skills.has(skill.skill_id);
    }

    bool IsDragonArenaSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Spirit_of_the_Festival && skill_id <= GW::Constants::SkillID::Imperial_Majesty);
    }

    bool IsRollerBeetleSkill(GW::Constants::SkillID skill_id)
    {
        return (skill_id >= GW::Constants::SkillID::Rollerbeetle_Racer && skill_id <= GW::Constants::SkillID::Spit_Rocks);
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
        return (skill_id >= GW::Constants::SkillID::Agent_of_the_Mad_King && skill_id <= GW::Constants::SkillID::The_Mad_Kings_Influence);
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
            GW::Constants::SkillID::Storm_of_Swords
        );
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
        return (IsUrsanSkill(skill_id) || IsVolfenSkill(skill_id) || IsRavenSkill(skill_id) || skill_id == GW::Constants::SkillID::Totem_of_Man);
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
            GW::Constants::SkillID::Pain_attack_Togo2
        );
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
            GW::Constants::SkillID::Form_Up_and_Advance
        );
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
        return (skill_id >= GW::Constants::SkillID::Siege_Devourer && skill_id <= GW::Constants::SkillID::Dismount_Siege_Devourer);
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
            GW::Constants::SkillID::Volatile_Charr_Crystal
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
        constexpr auto pvp_only_skills = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Charm_Animal_Codex
        );

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
            GW::Constants::SkillID::Phase_Shield_effect
        );

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
            GW::Constants::SkillID::Dissonance_attack
        );

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
            GW::Constants::SkillID::Fire_Dart1
        );

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
            GW::Constants::SkillID::Its_Good_to_Be_King
        );

        constexpr auto non_monster_skills_with_monster_icon = MakeFixedSet<GW::Constants::SkillID>(
            GW::Constants::SkillID::Spectral_Agony_Saul_DAlessio,
            GW::Constants::SkillID::Burden_Totem,
            GW::Constants::SkillID::Splinter_Mine_skill,
            GW::Constants::SkillID::Entanglement
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
            case GW::Constants::SkillID::Unseen_Fury:
            {
                pd.type = ParsedSkillData::Type::Blind;
                pd.param = {3, 10};
                parsed_data.push_back(pd);
                pd.type = ParsedSkillData::Type::Duration;
                pd.param = {10, 30};
                parsed_data.push_back(pd);
                return;
            }

            case GW::Constants::SkillID::Ursan_Roar_Blood_Washes_Blood:
                pd.param = {4, 4};
            case GW::Constants::SkillID::Ursan_Roar:
            {
                if (!pd.param)
                    pd.param = {2, 5};

                pd.type = ParsedSkillData::Type::Duration;
                parsed_data.push_back(pd);

                pd.type = ParsedSkillData::Type::Weakness;
                parsed_data.push_back(pd);
                return;
            }

            case GW::Constants::SkillID::Ash_Blast:
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

            case GW::Constants::SkillID::Crippling_Victory: // Has weird formatting for a skill which applies a condition
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

            case GW::Constants::SkillID::Hungers_Bite:
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

    GW::Constants::SkillID ParsedSkillData::GetCondition() const
    {
        // clang-format off
        switch (type) {
            case Type::Bleeding:     return GW::Constants::SkillID::Bleeding;
            case Type::Blind:        return GW::Constants::SkillID::Blind;
            case Type::Burning:      return GW::Constants::SkillID::Burning;
            case Type::CrackedArmor: return GW::Constants::SkillID::Cracked_Armor;
            case Type::Crippled:     return GW::Constants::SkillID::Crippled;
            case Type::Dazed:        return GW::Constants::SkillID::Dazed;
            case Type::DeepWound:    return GW::Constants::SkillID::Deep_Wound;
            case Type::Disease:      return GW::Constants::SkillID::Disease;
            case Type::Poison:       return GW::Constants::SkillID::Poison;
            case Type::Weakness:     return GW::Constants::SkillID::Weakness;
            
            default:                 return GW::Constants::SkillID::No_Skill;
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

        CustomSkillData &GetCustomSkillData(GW::Constants::SkillID skill_id)
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
            case GW::Constants::SkillID::Pious_Assault:
            case GW::Constants::SkillID::Pious_Assault_PvP:
                return {0, 0};
            default:
            {
                if (cskill.tags.SpiritAttack)
                {
                    return {0, 0};
                }

                if (!base_duration.IsNull() &&
                    cskill.skill->type == GW::Constants::SkillType::Attack)
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
            case GW::Constants::SkillType::Well:
            case GW::Constants::SkillType::Ward:
            case GW::Constants::SkillType::Trap:
            case GW::Constants::SkillType::Ritual:
            case GW::Constants::SkillType::Condition:
            case GW::Constants::SkillType::Bounty:
            case GW::Constants::SkillType::Disguise:
            case GW::Constants::SkillType::Scroll:
            case GW::Constants::SkillType::Environmental:
            case GW::Constants::SkillType::EnvironmentalTrap:
            case GW::Constants::SkillType::Passive:
            case GW::Constants::SkillType::Title:
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
                case GW::Constants::SkillID::Youre_All_Alone:
                case GW::Constants::SkillID::Throw_Dirt:
                case GW::Constants::SkillID::Barbed_Signet:
                case GW::Constants::SkillID::Oppressive_Gaze:
                case GW::Constants::SkillID::Enfeebling_Blood:
                case GW::Constants::SkillID::Enfeebling_Blood_PvP:
                case GW::Constants::SkillID::Weaken_Armor:
                case GW::Constants::SkillID::Signet_of_Weariness:
                case GW::Constants::SkillID::Ride_the_Lightning:
                case (GW::Constants::SkillID)2807: // Ride the lightning (PvP)
                case GW::Constants::SkillID::Blinding_Surge:
                case GW::Constants::SkillID::Blinding_Surge_PvP:
                case GW::Constants::SkillID::Thunderclap:
                case GW::Constants::SkillID::Lightning_Touch:
                case GW::Constants::SkillID::Teinais_Crystals:
                case GW::Constants::SkillID::Eruption:
                case GW::Constants::SkillID::Mind_Burn:
                case GW::Constants::SkillID::UNUSED_Searing_Flames:
                case GW::Constants::SkillID::Star_Burst:
                case GW::Constants::SkillID::Burning_Speed:
                case GW::Constants::SkillID::Searing_Flames:
                case GW::Constants::SkillID::Rodgorts_Invocation:
                case GW::Constants::SkillID::Caltrops:
                case GW::Constants::SkillID::Blinding_Powder:
                case GW::Constants::SkillID::Unseen_Fury: // Only stance that applies a condition on activation
                case GW::Constants::SkillID::Holy_Spear:
                case GW::Constants::SkillID::Finish_Him:
                case GW::Constants::SkillID::You_Are_All_Weaklings:
                case GW::Constants::SkillID::You_Move_Like_a_Dwarf:
                case GW::Constants::SkillID::Maddening_Laughter:
                case GW::Constants::SkillID::Queen_Wail:
                case GW::Constants::SkillID::REMOVE_Queen_Wail:
                case GW::Constants::SkillID::Raven_Shriek:
                case GW::Constants::SkillID::Raven_Shriek_A_Gate_Too_Far:
                case GW::Constants::SkillID::Ursan_Roar:
                case GW::Constants::SkillID::Ursan_Roar_Blood_Washes_Blood:
                case GW::Constants::SkillID::Armor_of_Sanctity:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Target;
                    effect.radius = (Utils::Range)skill.aoe_range;

                    MakeAndPushConditions();
                    return;
                }

                // Skills inflicting condition on self
                case GW::Constants::SkillID::Headbutt:
                case GW::Constants::SkillID::Signet_of_Agony:
                case GW::Constants::SkillID::Signet_of_Agony_PvP:
                case GW::Constants::SkillID::Blood_Drinker:
                case GW::Constants::SkillID::Chilblains:
                case GW::Constants::SkillID::Shadow_Sanctuary_kurzick:
                case GW::Constants::SkillID::Shadow_Sanctuary_luxon:
                case GW::Constants::SkillID::Wearying_Spear:
                {
                    effect.mask = EffectMask::Caster;
                    effect.location = EffectLocation::Caster;

                    MakeAndPushConditions();
                    return;
                }

                case GW::Constants::SkillID::Signet_of_Suffering:
                {
                    effect.mask = EffectMask::Caster;
                    effect.location = EffectLocation::Caster;
                    effect.skill_id_or_removal = GW::Constants::SkillID::Bleeding;
                    effect.duration_or_count = {6, 6};
                    cskill.init_effects.push_back(effect);
                }

                case GW::Constants::SkillID::Shockwave:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Caster;
                    effect.duration_or_count = {1, 10};

                    effect.radius = Utils::Range::Nearby;
                    effect.skill_id_or_removal = GW::Constants::SkillID::Cracked_Armor;
                    cskill.init_effects.push_back(effect);

                    effect.radius = Utils::Range::InTheArea;
                    effect.skill_id_or_removal = GW::Constants::SkillID::Weakness;
                    cskill.init_effects.push_back(effect);

                    effect.radius = Utils::Range::Adjacent;
                    effect.skill_id_or_removal = GW::Constants::SkillID::Blind;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case GW::Constants::SkillID::Stone_Sheath:
                {
                    effect.mask = EffectMask::Foes;
                    effect.radius = Utils::Range::Nearby;
                    effect.skill_id_or_removal = GW::Constants::SkillID::Weakness;
                    effect.duration_or_count = {5, 20};

                    effect.location = EffectLocation::Caster;
                    cskill.init_effects.push_back(effect);

                    effect.location = EffectLocation::Target;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case GW::Constants::SkillID::Poisoned_Heart:
                {
                    effect.mask = EffectMask::Foes | EffectMask::Caster;
                    effect.location = EffectLocation::Caster;
                    effect.radius = (Utils::Range)skill.aoe_range;

                    MakeAndPushConditions();
                    return;
                }

                case GW::Constants::SkillID::Earthen_Shackles:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Target;

                    MakeAndPushEffect(cskill.end_effects, conditions);
                }

                case GW::Constants::SkillID::Signet_of_Midnight:
                {
                    effect.mask = EffectMask::Foes | EffectMask::Caster;
                    effect.skill_id_or_removal = GW::Constants::SkillID::Blind;

                    effect.location = EffectLocation::Caster;
                    cskill.init_effects.push_back(effect);

                    effect.location = EffectLocation::Target;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // Conditions on foes around caster
                case GW::Constants::SkillID::Return:
                case GW::Constants::SkillID::Vipers_Defense:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Caster;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    MakeAndPushConditions();
                    return;
                }

                // Conditions on foes around spirit-ally closest to target
                case GW::Constants::SkillID::Rupture_Soul:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::SpiritAllyClosestToTarget;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    MakeAndPushConditions();
                    return;
                }

                // Discards / Handled elsewhere
                case GW::Constants::SkillID::Swift_Chop:
                case GW::Constants::SkillID::Seeking_Blade:
                case GW::Constants::SkillID::Drunken_Blow:     // We discard this because of randomness
                case GW::Constants::SkillID::Desperation_Blow: // We discard this because of randomness
                case GW::Constants::SkillID::Hungers_Bite:     // We discard this because of randomness
                case GW::Constants::SkillID::Deflect_Arrows:
                case GW::Constants::SkillID::Strike_as_One:
                case GW::Constants::SkillID::Putrid_Flesh:
                case GW::Constants::SkillID::Necrotic_Traversal:
                case GW::Constants::SkillID::Ulcerous_Lungs:
                case GW::Constants::SkillID::Fevered_Dreams:
                case GW::Constants::SkillID::Ineptitude:
                case GW::Constants::SkillID::Illusion_of_Haste:
                case GW::Constants::SkillID::Illusion_of_Haste_PvP:
                case GW::Constants::SkillID::Double_Dragon:
                case GW::Constants::SkillID::Elemental_Flame:
                case GW::Constants::SkillID::Elemental_Flame_PvP:
                case GW::Constants::SkillID::Mark_of_Rodgort:
                case GW::Constants::SkillID::Bed_of_Coals:
                case GW::Constants::SkillID::Searing_Heat:
                case GW::Constants::SkillID::Augury_of_Death:
                case GW::Constants::SkillID::Shadow_Fang:
                case GW::Constants::SkillID::UNUSED_Hidden_Caltrops:
                case GW::Constants::SkillID::Hidden_Caltrops:
                case GW::Constants::SkillID::Smoke_Powder_Defense:
                case GW::Constants::SkillID::Sharpen_Daggers:
                case GW::Constants::SkillID::Shadowsong:
                case GW::Constants::SkillID::Shadowsong_Master_Riyo:
                case GW::Constants::SkillID::Shadowsong_PvP:
                case GW::Constants::SkillID::Sundering_Weapon:
                case GW::Constants::SkillID::Blind_Was_Mingson:
                case GW::Constants::SkillID::Weapon_of_Shadow:
                case GW::Constants::SkillID::Spirit_Rift:
                case GW::Constants::SkillID::UNUSED_Anthem_of_Weariness:
                case GW::Constants::SkillID::Anthem_of_Weariness:
                case GW::Constants::SkillID::Crippling_Anthem:
                case GW::Constants::SkillID::Find_Their_Weakness:
                case GW::Constants::SkillID::Find_Their_Weakness_PvP:
                case GW::Constants::SkillID::Find_Their_Weakness_Thackeray:
                case GW::Constants::SkillID::Anthem_of_Flame:
                case GW::Constants::SkillID::Blazing_Finale:
                case GW::Constants::SkillID::Blazing_Finale_PvP:
                case GW::Constants::SkillID::Burning_Refrain:
                case GW::Constants::SkillID::Burning_Shield:
                case GW::Constants::SkillID::Cautery_Signet:
                case GW::Constants::SkillID::Wearying_Strike:
                case GW::Constants::SkillID::Grenths_Grasp:
                case GW::Constants::SkillID::REMOVE_Wind_Prayers_skill:
                case GW::Constants::SkillID::Attackers_Insight:
                case GW::Constants::SkillID::Rending_Aura:
                case GW::Constants::SkillID::Signet_of_Pious_Restraint:
                case GW::Constants::SkillID::Test_of_Faith:
                case GW::Constants::SkillID::Ebon_Dust_Aura_PvP:
                case GW::Constants::SkillID::Shield_of_Force:
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
                        case GW::Constants::SkillType::Enchantment:
                        case GW::Constants::SkillType::PetAttack:
                        case GW::Constants::SkillType::Preparation:
                        case GW::Constants::SkillType::Glyph:
                        case GW::Constants::SkillType::Form:
                        case GW::Constants::SkillType::Well:
                        case GW::Constants::SkillType::Ward:
                            return;
                    }

                    if (skill.type == GW::Constants::SkillType::Attack ||
                        skill.type == GW::Constants::SkillType::Spell ||
                        skill.type == GW::Constants::SkillType::Hex ||
                        skill.type == GW::Constants::SkillType::Signet ||
                        skill.type == GW::Constants::SkillType::Skill ||
                        skill.type == GW::Constants::SkillType::Skill2 ||
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
                case GW::Constants::SkillID::Never_Rampage_Alone:
                case GW::Constants::SkillID::Run_as_One:
                case GW::Constants::SkillID::Rampage_as_One:
                case GW::Constants::SkillID::Strike_as_One:
                {
                    effect.mask = EffectMask::CasterAndPet;
                    effect.location = EffectLocation::Null;
                    effect.radius = Utils::Range::CompassRange;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // Affects allies
                case GW::Constants::SkillID::Dwaynas_Sorrow:
                    effect.radius = Utils::Range::Nearby;
                case GW::Constants::SkillID::Charge:
                case GW::Constants::SkillID::Storm_of_Swords:
                case GW::Constants::SkillID::Cant_Touch_This:
                case GW::Constants::SkillID::Fall_Back:
                case GW::Constants::SkillID::Fall_Back_PvP:
                case GW::Constants::SkillID::Go_for_the_Eyes:
                case GW::Constants::SkillID::Go_for_the_Eyes_PvP:
                case GW::Constants::SkillID::Godspeed:
                case GW::Constants::SkillID::The_Power_Is_Yours:
                case GW::Constants::SkillID::Angelic_Bond:
                case GW::Constants::SkillID::Its_Good_to_Be_King:
                case GW::Constants::SkillID::Advance:
                case GW::Constants::SkillID::Song_of_the_Mists:
                case GW::Constants::SkillID::Enemies_Must_Die:
                case GW::Constants::SkillID::Rands_Attack:
                case GW::Constants::SkillID::Lets_Get_Em:
                case GW::Constants::SkillID::Cry_of_Madness:
                case GW::Constants::SkillID::Ursan_Roar:
                case GW::Constants::SkillID::Ursan_Roar_Blood_Washes_Blood:
                case GW::Constants::SkillID::Volfen_Bloodlust:
                case GW::Constants::SkillID::Volfen_Bloodlust_Curse_of_the_Nornbear:
                case GW::Constants::SkillID::Theres_Nothing_to_Fear_Thackeray:
                case GW::Constants::SkillID::Natures_Blessing:
                case GW::Constants::SkillID::For_Elona:
                case GW::Constants::SkillID::Form_Up_and_Advance:
                {
                    effect.mask = EffectMask::Allies;
                    effect.location = EffectLocation::Target;
                    if (!effect.radius)
                        effect.radius = (Utils::Range)skill.aoe_range;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case GW::Constants::SkillID::Save_Yourselves_kurzick:
                case GW::Constants::SkillID::Save_Yourselves_luxon:
                {
                    effect.mask = EffectMask::OtherPartyMembers;
                    effect.location = EffectLocation::Caster;
                    effect.radius = Utils::Range::Earshot;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // AOE hexes
                case GW::Constants::SkillID::Parasitic_Bite:
                case GW::Constants::SkillID::Scourge_Sacrifice:
                case GW::Constants::SkillID::Life_Transfer:
                case GW::Constants::SkillID::Blood_Bond:
                case GW::Constants::SkillID::Lingering_Curse:
                case GW::Constants::SkillID::Suffering:
                case GW::Constants::SkillID::Reckless_Haste:
                case GW::Constants::SkillID::Meekness:
                case GW::Constants::SkillID::Ulcerous_Lungs:
                case GW::Constants::SkillID::Vocal_Minority:
                case GW::Constants::SkillID::Shadow_of_Fear:
                case GW::Constants::SkillID::Stolen_Speed:
                case GW::Constants::SkillID::Stolen_Speed_PvP:
                case GW::Constants::SkillID::Shared_Burden:
                case GW::Constants::SkillID::Shared_Burden_PvP:
                case GW::Constants::SkillID::Air_of_Disenchantment:
                case GW::Constants::SkillID::Ineptitude:
                case GW::Constants::SkillID::Arcane_Conundrum:
                case GW::Constants::SkillID::Clumsiness:
                case GW::Constants::SkillID::Fragility:
                case GW::Constants::SkillID::Soothing_Images:
                case GW::Constants::SkillID::Visions_of_Regret:
                case GW::Constants::SkillID::Visions_of_Regret_PvP:
                case GW::Constants::SkillID::Panic:
                case GW::Constants::SkillID::Panic_PvP:
                case GW::Constants::SkillID::Earthen_Shackles:
                case GW::Constants::SkillID::Ash_Blast:
                case GW::Constants::SkillID::Mark_of_Rodgort:
                case GW::Constants::SkillID::Blurred_Vision:
                case GW::Constants::SkillID::Deep_Freeze:
                case GW::Constants::SkillID::Ice_Spikes:
                case GW::Constants::SkillID::Rust:
                case GW::Constants::SkillID::Binding_Chains:
                case GW::Constants::SkillID::Dulled_Weapon:
                case GW::Constants::SkillID::Lamentation:
                case GW::Constants::SkillID::Painful_Bond:
                case GW::Constants::SkillID::Isaiahs_Balance:
                case GW::Constants::SkillID::Snaring_Web:
                case GW::Constants::SkillID::Spirit_World_Retreat:
                case GW::Constants::SkillID::Corsairs_Net:
                case GW::Constants::SkillID::Crystal_Haze:
                case GW::Constants::SkillID::Icicles:
                case GW::Constants::SkillID::Shared_Burden_Gwen:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Target;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // AOE hexes around caster
                case GW::Constants::SkillID::Amity:
                case GW::Constants::SkillID::Grasping_Earth:
                case GW::Constants::SkillID::Frozen_Burst:
                case GW::Constants::SkillID::Wurm_Bile:
                case GW::Constants::SkillID::Suicidal_Impulse:
                case GW::Constants::SkillID::Last_Rites_of_Torment:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Caster;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case GW::Constants::SkillID::Mirror_of_Ice:
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
                case GW::Constants::SkillID::Retreat:
                case GW::Constants::SkillID::Celestial_Stance:
                case GW::Constants::SkillID::Celestial_Haste:
                case GW::Constants::SkillID::Order_of_the_Vampire:
                case GW::Constants::SkillID::Order_of_Pain:
                case GW::Constants::SkillID::Dark_Fury:
                case GW::Constants::SkillID::Order_of_Apostasy:
                    effect.radius = Utils::Range::CompassRange;
                case GW::Constants::SkillID::Shields_Up:
                case GW::Constants::SkillID::Watch_Yourself:
                case GW::Constants::SkillID::Watch_Yourself_PvP:
                case GW::Constants::SkillID::Aegis:
                case GW::Constants::SkillID::Shield_Guardian:
                case GW::Constants::SkillID::Magnetic_Surge: // The desc is wrong: this is a party wide skill
                case GW::Constants::SkillID::Magnetic_Aura:
                case GW::Constants::SkillID::Swirling_Aura:
                case GW::Constants::SkillID::Never_Surrender:
                case GW::Constants::SkillID::Never_Surrender_PvP:
                case GW::Constants::SkillID::Stand_Your_Ground:
                case GW::Constants::SkillID::Stand_Your_Ground_PvP:
                case GW::Constants::SkillID::We_Shall_Return_PvP:
                case GW::Constants::SkillID::Theyre_on_Fire:
                case GW::Constants::SkillID::Theres_Nothing_to_Fear:
                case GW::Constants::SkillID::By_Urals_Hammer:
                case GW::Constants::SkillID::Dont_Trip:
                {
                    effect.mask = EffectMask::PartyMembers;
                    effect.location = EffectLocation::Caster;
                    if (!effect.radius)
                        effect.radius = Utils::Range::Earshot;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case GW::Constants::SkillID::Together_as_one:
                {
                    effect.mask = EffectMask::PartyMembers | EffectMask::PartyPets;
                    effect.radius = Utils::Range::InTheArea;

                    effect.location = EffectLocation::Caster;
                    cskill.init_effects.push_back(effect);

                    effect.location = EffectLocation::Pet;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                case GW::Constants::SkillID::Weight_of_Dhuum:
                {
                    effect.skill_id_or_removal = GW::Constants::SkillID::Weight_of_Dhuum_hex;
                    effect.mask = EffectMask::Target;
                    effect.location = EffectLocation::Target;
                    cskill.init_effects.push_back(effect);
                    return;
                }

                // Discards / Handled elsewhere
                case GW::Constants::SkillID::Arcane_Mimicry:
                case GW::Constants::SkillID::Jaundiced_Gaze:
                case GW::Constants::SkillID::Corrupt_Enchantment:
                case GW::Constants::SkillID::Flurry_of_Splinters:
                case (GW::Constants::SkillID)3001:
                case GW::Constants::SkillID::Brutal_Mauling:
                case GW::Constants::SkillID::Frost_Vortex:
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
                            case GW::Constants::SkillType::Shout:
                            case GW::Constants::SkillType::Skill:
                            case GW::Constants::SkillType::Skill2:
                            case GW::Constants::SkillType::PetAttack:
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
                        case GW::Constants::SkillType::ItemSpell:
                        case GW::Constants::SkillType::Preparation:
                        case GW::Constants::SkillType::Form:
                        case GW::Constants::SkillType::Stance:
                        case GW::Constants::SkillType::Glyph:
                        {
                            effect.mask = EffectMask::Caster;
                            effect.location = EffectLocation::Caster;
                            cskill.init_effects.push_back(effect);
                            return;
                        }

                        case GW::Constants::SkillType::Shout:
                        case GW::Constants::SkillType::Signet:
                        case GW::Constants::SkillType::WeaponSpell:
                        case GW::Constants::SkillType::Enchantment:
                        case GW::Constants::SkillType::Hex:
                        case GW::Constants::SkillType::Spell:
                        case GW::Constants::SkillType::Skill:
                        case GW::Constants::SkillType::Skill2:
                        case GW::Constants::SkillType::EchoRefrain:
                        {
                            effect.mask = EffectMask::Target;
                            effect.location = EffectLocation::Target;
                            cskill.init_effects.push_back(effect);
                            return;
                        }

                        case GW::Constants::SkillType::Chant:
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
                case GW::Constants::SkillID::Extinguish:
                case GW::Constants::SkillID::Star_Shine:
                {
                    effect.mask = EffectMask::PartyMembers;
                    effect.location = EffectLocation::Caster;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    MakeAndPushRemovals();
                    return;
                }

                // AoE at target
                case GW::Constants::SkillID::Withdraw_Hexes:
                case GW::Constants::SkillID::Pure_Was_Li_Ming:
                    effect.mask = EffectMask::Allies;
                case GW::Constants::SkillID::Chilblains:
                case GW::Constants::SkillID::Air_of_Disenchantment:
                {
                    if (effect.mask == EffectMask::None)
                        effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Target;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    MakeAndPushRemovals();
                    return;
                }

                // Self removal
                case GW::Constants::SkillID::Ether_Prodigy:
                case GW::Constants::SkillID::Energy_Font:
                case GW::Constants::SkillID::Second_Wind:
                {
                    effect.mask = EffectMask::Caster;
                    effect.location = EffectLocation::Caster;
                    MakeAndPushRemovals();
                    return;
                }

                case GW::Constants::SkillID::Antidote_Signet:
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

                case GW::Constants::SkillID::Crystal_Wave:
                {
                    effect.mask = EffectMask::Foes;
                    effect.location = EffectLocation::Caster;
                    effect.radius = (Utils::Range)skill.aoe_range;
                    MakeAndPushRemovals();
                    return;
                }

                case GW::Constants::SkillID::UNUSED_Empathic_Removal:
                case GW::Constants::SkillID::Empathic_Removal:
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
                case GW::Constants::SkillID::Spotless_Mind:
                case GW::Constants::SkillID::Spotless_Soul:
                case GW::Constants::SkillID::Divert_Hexes:
                case GW::Constants::SkillID::Purifying_Veil:
                case GW::Constants::SkillID::Draw_Conditions:
                case GW::Constants::SkillID::Peace_and_Harmony:
                case GW::Constants::SkillID::Contemplation_of_Purity:
                case GW::Constants::SkillID::Deny_Hexes:
                case GW::Constants::SkillID::Martyr:
                case GW::Constants::SkillID::Holy_Veil:
                case GW::Constants::SkillID::Veratas_Sacrifice:
                case GW::Constants::SkillID::Well_of_the_Profane:
                case GW::Constants::SkillID::UNUSED_Foul_Feast:
                case GW::Constants::SkillID::Foul_Feast:
                case GW::Constants::SkillID::Order_of_Apostasy:
                case GW::Constants::SkillID::Plague_Signet:
                case GW::Constants::SkillID::UNUSED_Plague_Sending:
                case GW::Constants::SkillID::Plague_Sending:
                case GW::Constants::SkillID::Plague_Touch:
                case GW::Constants::SkillID::Hex_Eater_Vortex:
                case GW::Constants::SkillID::Shatter_Delusions:
                case GW::Constants::SkillID::Shatter_Delusions_PvP:
                case GW::Constants::SkillID::Drain_Delusions:
                case GW::Constants::SkillID::Hex_Eater_Signet:
                case GW::Constants::SkillID::Hypochondria:
                case GW::Constants::SkillID::Dark_Apostasy:
                case GW::Constants::SkillID::Assassins_Remedy:
                case GW::Constants::SkillID::Assassins_Remedy_PvP:
                case GW::Constants::SkillID::Signet_of_Malice:
                case GW::Constants::SkillID::Signet_of_Twilight:
                case GW::Constants::SkillID::Lift_Enchantment:
                case GW::Constants::SkillID::Disenchantment:
                case GW::Constants::SkillID::Disenchantment_PvP:
                case GW::Constants::SkillID::Weapon_of_Remedy:
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
        if (skill->skill_id_pvp < GW::Constants::SkillID::Count &&
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

    void GetConditionsFromSpan(std::span<const ParsedSkillData> parsed_data, GW::Constants::SkillID source_skill_id, uint8_t attr_lvl, OutBuf<SkillEffect> result)
    {
        bool success = true;
        for (const auto &pd : parsed_data)
        {
            const auto condition_skill_id = pd.GetCondition();
            if (condition_skill_id == GW::Constants::SkillID::No_Skill)
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

        FixedVector<SkillEffect, 18> skill_effects;

        // custom_sd.GetOnActivationEffects(caster, target_id, skill_effects);

        if (skill_effects.size() == 0)
            return;

        auto range = (float)custom_sd.GetAoE();

        std::vector<uint32_t> target_ids; // May become large, so we use a heap allocated buffer
        if (custom_sd.skill->type == GW::Constants::SkillType::Enchantment &&
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
        if (auto skill_id = std::get_if<GW::Constants::SkillID>(&skill_id_or_removal))
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

        if (auto skill_id = std::get_if<GW::Constants::SkillID>(&skill_id_or_removal))
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