#pragma once
#include <Windows.h>

#include <bitset>
#include <format>
#include <map>
#include <string>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GadgetContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/PreGameContext.h>

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/List.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <custom_skill_data.h>

namespace HerosInsight::EffectTracking
{
    struct EffectTracker
    {
        uint32_t cause_agent_id;
        GW::Constants::SkillID skill_id;
        uint32_t attribute_rank;
        uint32_t effect_id;
        uint32_t duration_sec;
        DWORD begin_timestamp;
        DWORD observed_timestamp;
        uint32_t unique_id;                                     // An id we make ourselves to track effects
        uint32_t aoe_id = std::numeric_limits<uint32_t>::max(); // the aoe id responsible for this effect
        float accum_damage;
        uint8_t charges;
        bool is_active; // Whether is it the currently active effect (there may be multiple instances of the same effect)

        DWORD GetEndTimestamp() const
        {
            if (duration_sec == std::numeric_limits<uint32_t>::max())
                return std::numeric_limits<DWORD>::max();

            return begin_timestamp + duration_sec * 1000;
        }

        uint32_t CalcRemainingMS() const
        {
            auto timestamp_now = GW::MemoryMgr::GetSkillTimer();
            auto timestamp_end = GetEndTimestamp();

            if (timestamp_now >= timestamp_end)
                return 0;

            return timestamp_end - timestamp_now;
        }
    };

    struct AgentEffectTrackers
    {
        // Sorted by older to newer
        std::vector<EffectTracker> effects;

        struct ActiveEffectIterator
        {
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = EffectTracker;
            using pointer = EffectTracker *;
            using reference = EffectTracker &;

            explicit ActiveEffectIterator(pointer ptr, pointer end) : current(ptr), end(end)
            {
                skipInactive();
            }

            reference operator*() const { return *current; }
            pointer operator->() { return current; }

            ActiveEffectIterator &operator++()
            {
                ++current;
                skipInactive();
                return *this;
            }

            ActiveEffectIterator operator++(int)
            {
                ActiveEffectIterator tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator==(const ActiveEffectIterator &a, const ActiveEffectIterator &b)
            {
                return a.current == b.current;
            }

            friend bool operator!=(const ActiveEffectIterator &a, const ActiveEffectIterator &b)
            {
                return a.current != b.current;
            }

        private:
            void skipInactive()
            {
                while (current != end && !current->is_active)
                {
                    ++current;
                }
            }

            pointer current;
            pointer end;
        };

        ActiveEffectIterator begin()
        {
            return ActiveEffectIterator(effects.data(), effects.data() + effects.size());
        }

        ActiveEffectIterator end()
        {
            return ActiveEffectIterator(effects.data() + effects.size(), effects.data() + effects.size());
        }
    };

    extern std::unordered_map<uint32_t, AgentEffectTrackers> agent_trackers;

    void ApplySkillEffect(uint32_t target_id, uint32_t cause_id, SkillEffect effect);
    void ApplySkillEffects(uint32_t target_id, uint32_t cause_id, std::span<SkillEffect> effects);
    void AddTracker(uint32_t agent_id, EffectTracker tracker);
    uint32_t CreateAOEEffect(GW::GamePos pos, float radius, GW::Constants::SkillID skill_id, uint32_t effect_id, uint32_t cause_agent_id, float duration);
    void CreateAuraEffect(uint32_t agent_id, float radius, GW::Constants::SkillID skill_id, uint32_t effect_id, uint32_t cause_agent_id, float duration);
    void CondiHexEnchRemoval(uint32_t agent_id, RemovalMask mask, uint32_t count);
    void RemoveTrackers(uint32_t agent_id, std::function<bool(EffectTracker &)> predicate);
    void SpendCharge(uint32_t agent_id, GW::Constants::SkillID effect_skill_id);
    std::span<EffectTracker> GetTrackerSpan(uint32_t agent_id);
    AgentEffectTrackers &GetTrackers(uint32_t agent_id);
    EffectTracker *GetEffectBySkillID(uint32_t agent_id, GW::Constants::SkillID skill_id);
    void Reset();
    void Update();
}
