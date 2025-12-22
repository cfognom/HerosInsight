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
        void AssignPointers(void *base, Buffers &...buffer_specs)
        {
            assert(base);
            uintptr_t p = reinterpret_cast<uintptr_t>(base);

            // Fold expression using comma operator
            (
                (
                    // Align p for this buffer type
                    p = AlignUp(p, alignof(typename Buffers::value_type)),

                    // Assign pointer
                    buffer_specs.ptr = reinterpret_cast<typename Buffers::value_type *>(p),

                    // Advance p past this buffer
                    p += sizeof(typename Buffers::value_type) * buffer_specs.count
                ),
                ...
            );
        }
    }

    // RAII style heap multibuffer
    struct HeapAllocated
    {
        void *block;
        size_t align;

        template <typename... Specs>
        HeapAllocated(Specs &...specs)
        {
            auto [size, align] = Detail::ComputeSizeAndAlignment(specs...);
            this->align = align;
            this->block = ::operator new(size, std::align_val_t(align));
            Detail::AssignPointers(block, specs...);
        }

        ~HeapAllocated()
        {
            ::operator delete(block, std::align_val_t(align));
        }
    };

    // Spec for a buffer (type and count)
    template <typename T>
    struct Spec
    {
        using value_type = T;
        size_t count;
        T *ptr = nullptr;
        std::span<T> Span() const { return std::span<T>(ptr, count); }
        constexpr Spec(size_t c) : count(c) {}
    };

// Macro to create a multibuffer on the stack
#define MultiBuffer_alloca(...) HerosInsight::MultiBuffer::Detail::AssignPointers(alloca(HerosInsight::MultiBuffer::Detail::ComputeSizeAndAlignment(__VA_ARGS__).first), __VA_ARGS__);
}