#include <cassert>
#include <chrono>
#include <coroutine>
#include <deque>
#include <format>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <GWCA/GWCA.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/List.h>

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
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>

#include <StoC_packets.h>
#include <attribute_or_title.h>
#include <autovec.h>
#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <debug_display.h>
#include <effect_tracking.h>
#include <hero_ai.h>
#include <party_data.h>
#include <update_manager.h>
#include <utils.h>

#include "packet_stepper.h"

namespace HerosInsight::PacketStepper
{
    constexpr uint32_t HEADER_COUNT = 485;

    struct DelayedCoro
    {
        std::coroutine_handle<> handle;
        DWORD timestamp_resume;
    };

    struct RegisteredListener
    {
        GenericPacketCallback *callback;
        std::optional<uint32_t> header;
        Altitude altitude;
    };

    uint32_t last_coroutine_id = 0;
    std::set<uint32_t> live_coroutine_ids;

    std::vector<std::coroutine_handle<>> awaiting_frame_end;

    // We use deques to prevent reallocations that would invalidate references
    std::deque<DelayedCoro> delayed_coros; // sorted by timestamp_resume: lower to higher
    std::deque<AfterEffectsAwaiter> after_effects_awaiters;
    std::deque<std::optional<RegisteredListener>> listeners;
    bool is_frame_end = false;

    void RegisterListener(std::optional<uint32_t> header, GenericPacketCallback &callback, Altitude altitude)
    {
        listeners.emplace_back(std::in_place, &callback, header, altitude);
    }

    bool UnregisterListener(GenericPacketCallback &callback)
    {
        for (size_t i = 0; i < listeners.size(); ++i)
        {
            auto &listener = listeners[i];
            if (listener.has_value() && listener->callback == &callback)
            {
                if (i == 0)
                {
                    do
                    {
                        ++i;
                    } while (i < listeners.size() && !listeners[i].has_value());

                    listeners.erase(listeners.begin(), listeners.begin() + i);
                }
                else
                {
                    listener = std::nullopt;
                }
                return true;
            }
        }
        return false;
    }

    bool IsAssociatedWithSkillPacket(const StoC::PacketBase *packet, uint32_t caster_id, std::optional<std::span<uint32_t>> target_ids)
    {
        constexpr auto allowed_value_ids = MakeFixedSet<uint32_t>(
            StoC::GenericValueID::knocked_down,
            StoC::GenericValueID::armorignoring,
            StoC::GenericValueID::damage,
            StoC::GenericValueID::skill_damage,
            StoC::GenericValueID::critical,
            StoC::GenericValueID::add_effect,
            StoC::GenericValueID::remove_effect,
            StoC::GenericValueID::effect_on_agent,
            StoC::GenericValueID::change_health_regen,
            StoC::GenericValueID::max_hp_update,
            StoC::GenericValueID::effect_on_target);

        auto IsRelated = [=](uint32_t agent_id)
        {
            if (caster_id == agent_id)
                return true;

            if (target_ids.has_value())
            {
                auto target_ids_value = target_ids.value();
                return std::find(target_ids_value.begin(), target_ids_value.end(), agent_id) != target_ids_value.end();
            }
            else
            {
                return true; // We assume its related if no target span is provided
            }

            return false;
        };

        switch (packet->header)
        {
            case StoC::GenericValue::STATIC_HEADER:
            {
                auto p = (StoC::GenericValue *)packet;
                return allowed_value_ids.has(p->value_id) &&
                       IsRelated(p->agent_id);
            }
            case StoC::GenericValueTarget::STATIC_HEADER:
            {
                auto p = (StoC::GenericValueTarget *)packet;
                return allowed_value_ids.has(p->Value_id) &&
                       (IsRelated(p->target) || IsRelated(p->caster));
            }
            case StoC::GenericFloat::STATIC_HEADER:
            {
                auto p = (StoC::GenericFloat *)packet;
                return allowed_value_ids.has(p->type) &&
                       IsRelated(p->agent_id);
            }
            case StoC::GenericModifier::STATIC_HEADER:
            {
                auto p = (StoC::GenericModifier *)packet;
                return allowed_value_ids.has(p->type) &&
                       IsRelated(p->target_id);
            }
            case StoC::AddEffect::STATIC_HEADER:
            {
                auto p = (StoC::AddEffect *)packet;
                return IsRelated(p->agent_id);
            }
            case StoC::RemoveEffect::STATIC_HEADER:
            {
                auto p = (StoC::RemoveEffect *)packet;
                return IsRelated(p->agent_id);
            }
            case StoC::AgentState::STATIC_HEADER:
            {
                auto p = (StoC::AgentState *)packet;
                return IsRelated(p->agent_id);
            }
            case StoC::ProjectileCreated::STATIC_HEADER:
            {
                auto p = (StoC::ProjectileCreated *)packet;
                return caster_id == p->agent_id;
            }
        }
        return false;
    }

    void OmniHandler(const StoC::PacketBase *packet, Altitude altitude)
    {
        auto header = packet->header;
        //
        if (altitude == Altitude::Before && !after_effects_awaiters.empty())
        {
            auto it = after_effects_awaiters.begin();
            auto rem_count = after_effects_awaiters.size(); // Anything added during the loop will be skipped this time
            while (rem_count > 0)
            {
                if (!IsAssociatedWithSkillPacket(packet, it->caster_id, it->target_ids))
                {
                    it->handle.resume();
                    it = after_effects_awaiters.erase(it);
                }
                else
                {
                    ++it;
                }
                --rem_count;
            }
        }

        //
        for (auto &listener : listeners)
        {
            if (!listener.has_value())
                continue;

            if (listener->header && listener->header != header)
                continue;

            if (listener->altitude != altitude)
                continue;

            (*listener->callback)(packet);
        }
    }

    void OnFrameEnd(GW::HookStatus *)
    {
        is_frame_end = true;

        //
        for (auto handle : awaiting_frame_end) // This wont get invalidated because we dont push to it while we already are at the frame end
        {
            handle.resume();
        }
        awaiting_frame_end.clear();

        //
        DWORD timestamp_now = GW::MemoryMgr::GetSkillTimer();
        for (auto it = delayed_coros.begin(); it != delayed_coros.end();)
        {
            if (timestamp_now < it->timestamp_resume)
                break;

            it = delayed_coros.erase(it);
        }

        //
        auto size = after_effects_awaiters.size();
        for (uint32_t i = 0; i < size; ++i)
        {
            auto &awaiter = after_effects_awaiters[i];
            awaiter.handle.resume();
        }
        after_effects_awaiters.erase(after_effects_awaiters.begin(), after_effects_awaiters.begin() + size);

        is_frame_end = false;
    }

    GW::HookEntry entry;
    GW::HookEntry entry2;
    void HerosInsight::PacketStepper::Initialize()
    {
        for (uint32_t i = 0; i < HEADER_COUNT; ++i)
        {
            GW::StoC::RegisterPacketCallback(
                &entry, i,
                [](GW::HookStatus *, const StoC::PacketBase *packet)
                {
                    OmniHandler(packet, Altitude::Before);
                },
                -2);
            GW::StoC::RegisterPacketCallback(
                &entry2, i,
                [](GW::HookStatus *, const StoC::PacketBase *packet)
                {
                    OmniHandler(packet, Altitude::After);
                },
                2);
        }
        GW::GameThread::RegisterGameThreadCallback(&entry, &OnFrameEnd, -1);
    }

    void HerosInsight::PacketStepper::Terminate()
    {
        GW::StoC::RemoveCallbacks(&entry);
        GW::StoC::RemoveCallbacks(&entry2);
        GW::GameThread::RemoveGameThreadCallback(&entry);
    }

    TrackedCoroutine::Promise::Promise()
    {
        id = ++last_coroutine_id;
        if (id == 0)
            id = ++last_coroutine_id;
        live_coroutine_ids.insert(id);
#ifdef _DEBUG
        DebugDisplay::PushToDisplay("Running coroutines", std::to_string(live_coroutine_ids.size()));
#endif
    }
    TrackedCoroutine::Promise::~Promise()
    {
        live_coroutine_ids.erase(id);
#ifdef _DEBUG
        DebugDisplay::PushToDisplay("Running coroutines", std::to_string(live_coroutine_ids.size()));
#endif
    }
    bool TrackedCoroutine::is_finished() const
    {
        return live_coroutine_ids.find(id) == live_coroutine_ids.end();
    }
    bool FrameEndAwaiter::await_ready() const noexcept
    {
        return is_frame_end;
    }
    void FrameEndAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        this->handle = handle;
        awaiting_frame_end.push_back(handle);
    }
    void FrameEndAwaiter::stop() const
    {
        assert(handle && "handle is null");
        awaiting_frame_end.erase(std::remove(awaiting_frame_end.begin(), awaiting_frame_end.end(), handle), awaiting_frame_end.end());
        this->handle.resume();
    }
    void DelayAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        auto timestamp_resume = GW::MemoryMgr::GetSkillTimer() + this->delay_ms;
        this->handle = handle;
        // Do binary search to insert in order: lower to higher
        auto it = std::lower_bound(delayed_coros.begin(), delayed_coros.end(), timestamp_resume, [](const DelayedCoro &a, DWORD b)
            { return a.timestamp_resume < b; });
        delayed_coros.insert(it, DelayedCoro{handle, timestamp_resume});
    }
    void DelayAwaiter::stop() const
    {
        assert(handle && "handle is null");
        auto it = std::find_if(delayed_coros.begin(), delayed_coros.end(), [=](const DelayedCoro &a)
            { return a.handle == handle; });
        if (it != delayed_coros.end())
            delayed_coros.erase(it);
        this->handle.resume();
    }

    std::array<std::coroutine_handle<>, std::numeric_limits<uint8_t>::max()> channel_exclusives;
    ExclusiveAwaiter::~ExclusiveAwaiter()
    {
        if (handle)
        {
            channel_exclusives[channel] = nullptr;
        }
    }
    void ExclusiveAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        this->handle = handle;
        auto existing = channel_exclusives[channel];
        channel_exclusives[channel] = handle;
        if (existing)
        {
            existing.resume();
        }
    }

    void AfterEffectsAwaiter::await_suspend(std::coroutine_handle<> handle) noexcept
    {
        this->handle = handle;
        after_effects_awaiters.push_back(*this);
    }
}