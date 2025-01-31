#pragma once

#include <concepts>
#include <coroutine>
#include <optional>
#include <set>
#include <type_traits>
#include <vector>

#include <GWCA/GWCA.h>
#include <GWCA/Packets/StoC.h>

namespace StoC = GW::Packet::StoC;

namespace HerosInsight::PacketStepper
{
    void Initialize();
    void Terminate();

#define NEXT_PACKET_SWITCH                           \
    auto base_packet = co_await NextPacketAwaiter(); \
    switch (base_packet->header)

#define NEXT_PACKET_CASE(type, name, expr)                          \
    case type::STATIC_HEADER:                                       \
    {                                                               \
        const type &name = static_cast<const type &>(*base_packet); \
        expr;                                                       \
    }

    using GenericPacketCallback = std::function<void(const StoC::PacketBase *)>;

    enum struct Altitude
    {
        Before,
        After,
    };

    // These support register/unregister from inside callbacks, unlike GWCA's
    struct PacketListenerScope
    {
        PacketListenerScope() = default;
        PacketListenerScope(PacketListenerScope &&) = default;
        PacketListenerScope &operator=(PacketListenerScope &&) = default;
        PacketListenerScope(std::optional<uint32_t> header, GenericPacketCallback callback, Altitude altitude = Altitude::Before);

        // Callable traits
        template <typename T>
        struct callable_traits;

        template <typename R, typename Arg>
        struct callable_traits<R (*)(Arg)>
        {
            using argument_type = Arg;
        };

        template <typename R, typename C, typename Arg>
        struct callable_traits<R (C::*)(Arg) const>
        {
            using argument_type = Arg;
        };

        template <typename R, typename Arg>
        struct callable_traits<R(Arg)>
        {
            using argument_type = Arg;
        };

        template <typename Callable>
        struct callable_traits : callable_traits<decltype(&Callable::operator())>
        {
        };

        template <typename TypedPacketCallback>
        PacketListenerScope(TypedPacketCallback &&callback, Altitude altitude = Altitude::Before)
            : PacketListenerScope(
                  std::decay_t<typename callable_traits<TypedPacketCallback>::argument_type>::STATIC_HEADER,
                  [callback = std::forward<TypedPacketCallback>(callback)](const StoC::PacketBase *packet) mutable
                  {
                      return callback(*(static_cast<const std::decay_t<typename callable_traits<TypedPacketCallback>::argument_type> *>(packet)));
                  },
                  altitude)
        {
        }

        ~PacketListenerScope();

    private:
        uint32_t id;
    };

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
        NextPacketAwaiter(Altitude altitude = Altitude::Before) : altitude(altitude) {};

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) noexcept
        {
            this->listener.emplace(
                std::nullopt,
                [=](const StoC::PacketBase *packet)
                {
                    this->next_packet = packet;
                    handle.resume();
                },
                altitude);
        }
        const StoC::PacketBase *await_resume() noexcept
        {
            assert(next_packet && "next_packet is null");
            this->listener = std::nullopt;
            return next_packet;
        }

    private:
        Altitude altitude;
        std::optional<PacketListenerScope> listener = std::nullopt;
        const StoC::PacketBase *next_packet = nullptr;
    };

    template <typename PacketType>
    struct PacketAwaiter
    {
        static_assert(std::is_base_of_v<StoC::PacketBase, PacketType>,
            "PacketType must inherit from StoC::PacketBase");

        PacketAwaiter(Altitude altitude = Altitude::Before) : altitude(altitude) {};

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) noexcept
        {
            this->listener.emplace(
                PacketType::STATIC_HEADER,
                [=](const StoC::PacketBase *packet)
                {
                    this->next_packet = static_cast<const PacketType *>(packet);
                    handle.resume();
                },
                altitude);
        }
        PacketType await_resume() noexcept
        {
            assert(next_packet && "next_packet is null");
            auto packet = *next_packet;
            next_packet = nullptr;
            this->listener = std::nullopt;
            return packet;
        }

    private:
        Altitude altitude;
        std::optional<PacketListenerScope> listener = std::nullopt;
        const PacketType *next_packet = nullptr;
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

        friend void OmniHandler(const StoC::PacketBase *packet, Altitude altitude);
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
        std::coroutine_handle<> handle = nullptr;
    };

    //     // When it exists, it listens
    //     class PacketListenerScope
    //     {
    //         // Function traits
    //         template <typename T>
    //         struct function_traits;

    //         template <typename R, typename Arg>
    //         struct function_traits<R (*)(Arg)>
    //         {
    //             using argument_type = Arg;
    //         };

    //         template <typename R, typename C, typename Arg>
    //         struct function_traits<R (C::*)(Arg) const>
    //         {
    //             using argument_type = Arg;
    //         };

    //         template <typename R, typename Arg>
    //         struct function_traits<R(Arg)>
    //         {
    //             using argument_type = Arg;
    //         };

    //         template <typename Callable>
    //         struct function_traits : function_traits<decltype(&Callable::operator())>
    //         {
    //         };

    // #define PACKET_TYPE std::decay_t<typename function_traits<PacketCallback>::argument_type>

    //     public:
    //         template <typename PacketCallback>
    //         PacketListenerScope(PacketCallback &&callback, Altitude altitude = Altitude::Before)
    //             : listener(
    //                   [callback](const StoC::PacketBase *packet)
    //                   {
    //                       return callback(*(static_cast<const PACKET_TYPE *>(packet)));
    //                   },
    //                   PACKET_TYPE::STATIC_HEADER,
    //                   altitude)
    //         {
    //         }

    //     private:
    //         ActiveListener listener;
    //     };
}