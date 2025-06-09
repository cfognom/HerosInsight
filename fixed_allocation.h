#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

template <typename T, size_t N>
struct FixedStorage
{
    alignas(T) std::byte data[sizeof(T) * N];
    size_t used = 0;

    T *allocate(size_t n)
    {
        auto new_used = used + n;
        if (new_used > N)
            throw std::bad_alloc();
        auto ret = reinterpret_cast<T *>(data) + used;
        used = new_used;
        return ret;
    }
};

template <typename T, size_t N>
class FixedAllocator
{
public:
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    FixedAllocator(FixedStorage<T, N> *buffer)
        : buffer(buffer) {}

    template <typename U>
    FixedAllocator(const FixedAllocator<U, N> &other)
    {
#ifdef _DEBUG
        if constexpr (std::is_same_v<T, U>)
            buffer = reinterpret_cast<FixedStorage<T, N> *>(other.buffer);
        else
            buffer = nullptr; // Rebound allocator uses heap
#else
        static_assert(std::is_same_v<T, U>, "Rebound allocator between different types is not supported in release mode");
        buffer = reinterpret_cast<FixedStorage<T, N> *>(other.buffer);
#endif
    }

    T *allocate(std::size_t n)
    {
#ifdef _DEBUG
        if (buffer == nullptr)
        {
            // fallback to heap using std::allocator
            return std::allocator<T>{}.allocate(n);
        }
#endif

        return buffer->allocate(n);
    }

    void deallocate(T *p, std::size_t n)
    {
        // No-op (deallocation doesn't free memory in this allocator)
    }

    template <typename U>
    struct rebind
    {
        using other = FixedAllocator<U, N>;
    };

    bool operator==(const FixedAllocator &other) const noexcept
    {
        return buffer == other.buffer;
    }

    bool operator!=(const FixedAllocator &other) const noexcept
    {
        return !(*this == other);
    }

private:
    FixedStorage<T, N> *buffer;

    template <typename U, size_t M>
    friend class FixedAllocator;
};

template <typename T, size_t N>
class FixedVector : public std::vector<T, FixedAllocator<T, N>>
{
public:
    using allocator_type = FixedAllocator<T, N>;
    using base = std::vector<T, allocator_type>;

    FixedVector() : base(allocator_type(&storage))
    {
        base::reserve(N);
    }

    using base::vector;

private:
    FixedStorage<T, N> storage;
};

template <size_t N>
class FixedString : public std::basic_string<char, std::char_traits<char>, FixedAllocator<char, N>>
{
public:
    using allocator_type = FixedAllocator<char, N>;
    using base = std::basic_string<char, std::char_traits<char>, FixedAllocator<char, N>>;

    FixedString() : base(allocator_type(&storage))
    {
        base::reserve(N);
    }

    using base::basic_string;

private:
    FixedStorage<char, N> storage;
};

template <size_t N>
class FixedWString : public std::basic_string<wchar_t, std::char_traits<wchar_t>, FixedAllocator<wchar_t, N>>
{
public:
    using allocator_type = FixedAllocator<wchar_t, N>;
    using base = std::basic_string<wchar_t, std::char_traits<wchar_t>, FixedAllocator<wchar_t, N>>;

    FixedWString() : base(allocator_type(&storage))
    {
        base::reserve(N);
    }

    using base::basic_string;

private:
    FixedStorage<wchar_t, N> storage;
};
