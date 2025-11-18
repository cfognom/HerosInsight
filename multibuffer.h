#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <tuple>

namespace HerosInsight::MultiBuffer
{
    namespace Detail
    {
        // -------------------- Internal helpers --------------------
        constexpr uintptr_t AlignUp(uintptr_t ptr, size_t alignment)
        {
            return (ptr + alignment - 1) & ~(alignment - 1);
        }

        template <typename... Buffers>
        std::pair<size_t, size_t> ComputeSizeAndAlignment(const Buffers &...buffers)
        {
            size_t offset = 0;
            size_t maxAlign = 1;

            // Fold expression using comma operator
            (
                (
                    maxAlign = std::max(maxAlign, alignof(typename Buffers::value_type)),
                    offset = AlignUp(offset, alignof(typename Buffers::value_type)),
                    offset += sizeof(typename Buffers::value_type) * buffers.count
                ),
                ...
            );

            return {offset, maxAlign};
        }

        template <typename... Buffers>
        auto AssignPointers(void *base, const Buffers &...buffer_specs)
        {
            assert(base);
            char *p = reinterpret_cast<char *>(base);

            return std::tuple<typename Buffers::value_type *...>{
                (
                    (p = reinterpret_cast<char *>(AlignUp(reinterpret_cast<uintptr_t>(p), alignof(typename Buffers::value_type)))),
                    (p += sizeof(typename Buffers::value_type) * buffer_specs.count),
                    reinterpret_cast<typename Buffers::value_type *>(p - sizeof(typename Buffers::value_type) * buffer_specs.count)
                )...
            };
        }
    }

    // RAII style heap multibuffer
    template <typename... Specs>
    struct HeapAllocated
    {
        void *block;
        size_t align;
        std::tuple<typename Specs::value_type *...> ptrs;

        HeapAllocated(const Specs &...specs)
        {
            auto [size, align] = Detail::ComputeSizeAndAlignment(specs...);
            this->align = align;
            this->block = ::operator new(size, std::align_val_t(align));
            this->ptrs = Detail::AssignPointers(block, specs...);
        }

        ~HeapAllocated()
        {
            ::operator delete(block, std::align_val_t(align));
        }

        template <size_t I>
        auto &get()
        {
            return std::get<I>(ptrs);
        }
    };

    // Required for some reason
    template <typename... Specs>
    HeapAllocated(const Specs &...) -> HeapAllocated<Specs...>;

    // Spec for a buffer (type and count)
    template <typename T>
    struct Spec
    {
        using value_type = T;
        size_t count;
        constexpr Spec(size_t c) : count(c) {}
    };

// Macro to create a multibuffer on the stack
#define MultiBuffer_alloca(...) HerosInsight::MultiBuffer::Detail::AssignPointers(alloca(HerosInsight::MultiBuffer::Detail::ComputeSizeAndAlignment(__VA_ARGS__).first), __VA_ARGS__);
}

// Required by the language for structured bindings
namespace std
{
    template <typename... Specs>
    struct tuple_size<HerosInsight::MultiBuffer::HeapAllocated<Specs...>>
        : std::integral_constant<size_t, sizeof...(Specs)>
    {
    };

    template <size_t I, typename... Specs>
    struct tuple_element<I, HerosInsight::MultiBuffer::HeapAllocated<Specs...>>
    {
        using type =
            typename std::tuple_element<I, std::tuple<typename Specs::value_type *...>>::type;
    };
}