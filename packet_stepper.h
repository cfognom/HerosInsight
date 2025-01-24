#pragma once

#include <coroutine>
#include <set>
#include <vector>

#include <GWCA/GWCA.h>
#include <GWCA/Packets/StoC.h>

namespace StoC = GW::Packet::StoC;

namespace HerosInsight::PacketStepper
{
    void Initialize();
    void Terminate();

    using GenericPacketCallback = std::function<void(const StoC::PacketBase *)>;

    // These support register/unregister from inside callbacks, unlike GWCA's
    uint32_t RegisterListener(uint32_t header, GenericPacketCallback callback);
    void UnregisterListener(uint32_t header, uint32_t id);

    struct TrackedCoroutine // Just a handle to the actual coroutine frame
    {
        struct Promise
        {
            Promise();
            ~Promise();

            TrackedCoroutine get_return_object()
            {
                return TrackedCoroutine{
                    std::coroutine_handle<Promise>::from_promise(*this),
                    id,
                };
            }

            void unhandled_exception() noexcept {}
            void return_void() noexcept {}

            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; } // it frees itself once done

            uint32_t id = 0;
        };
        using promise_type = Promise;

        bool is_finished() const;

        void destroy()
        {
            if (!is_finished())
                handle.destroy();
        }

        std::coroutine_handle<Promise> handle = nullptr;
        uint32_t id = 0;
    };

    struct FrameEndAwaiter
    {
        bool await_ready() const noexcept;
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() noexcept
        {
            this->handle = nullptr;
        }

        void stop() const;

    private:
        std::coroutine_handle<> handle = nullptr;
    };

    struct DelayAwaiter
    {
        DelayAwaiter(uint32_t delay_ms) : delay_ms(delay_ms) {}

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() noexcept
        {
            this->handle = nullptr;
        }

        void stop() const;

    private:
        uint32_t delay_ms;
        std::coroutine_handle<> handle;
    };

    struct NextPacketAwaiter
    {
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        const StoC::PacketBase *await_resume() const noexcept
        {
            assert(next_packet && "next_packet is null");
            return next_packet;
        }

    private:
        std::coroutine_handle<> handle;
        const StoC::PacketBase *next_packet = nullptr;

        friend void OmniHandler(GW::HookStatus *, const StoC::PacketBase *packet);
    };

    // Awaits until another ExclusiveAwaiter is awaited
    struct ExclusiveAwaiter
    {
        ExclusiveAwaiter(uint8_t channel) : channel(channel) {}
        ~ExclusiveAwaiter();

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() noexcept
        {
            this->handle = nullptr;
        }

    private:
        std::coroutine_handle<> handle;
        uint8_t channel;
    };

    struct AfterEffectsAwaiter
    {
        AfterEffectsAwaiter(uint32_t caster_id, std::span<uint32_t> target_ids)
            : caster_id(caster_id), target_ids(target_ids)
        {
        }

        AfterEffectsAwaiter(uint32_t caster_id)
            : caster_id(caster_id), target_ids(std::nullopt)
        {
        }

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() noexcept
        {
            handle = nullptr;
        }

        void stop() const;

    private:
        std::coroutine_handle<> handle = nullptr;
        uint32_t caster_id;
        std::optional<std::span<uint32_t>> target_ids;

        friend void OmniHandler(GW::HookStatus *, const StoC::PacketBase *packet);
        friend void OnFrameEnd(GW::HookStatus *);
    };

    struct PermaAwaiter
    {
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) noexcept
        {
            this->handle = handle;
        }
        void await_resume() const noexcept {}

        void stop() const
        {
            assert(handle && "handle is null");
            handle.resume();
        }

    private:
        std::coroutine_handle<> handle;
    };

    // When it exists, it listens
    class PacketListenerScope
    {
        // Function traits
        template <typename T>
        struct function_traits;

        template <typename R, typename Arg>
        struct function_traits<R (*)(Arg)>
        {
            using argument_type = Arg;
        };

        template <typename R, typename C, typename Arg>
        struct function_traits<R (C::*)(Arg) const>
        {
            using argument_type = Arg;
        };

        template <typename R, typename Arg>
        struct function_traits<R(Arg)>
        {
            using argument_type = Arg;
        };

        template <typename Callable>
        struct function_traits : function_traits<decltype(&Callable::operator())>
        {
        };

    public:
        template <typename PacketCallback>
        PacketListenerScope(PacketCallback &&callback)
        {
            using PacketType = std::decay_t<typename function_traits<PacketCallback>::argument_type>;
            header = PacketType::STATIC_HEADER;
            registered_id = RegisterListener(header, [callback](const StoC::PacketBase *packet)
                {
                    return callback(*(static_cast<const PacketType *>(packet))); //
                });
        }

        ~PacketListenerScope()
        {
            UnregisterListener(header, registered_id);
        }

    private:
        uint32_t header;
        uint32_t registered_id;
    };
}