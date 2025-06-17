#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <vector>

struct BufferMetrics
{
    size_t size = 0;
    const size_t capacity;

    BufferMetrics(size_t capacity) : capacity(capacity) {}
};

template <typename T>
struct Buffer;

template <typename T, size_t N>
struct SizedBuffer
{
    BufferMetrics metrics{N};
    alignas(T) std::byte data[sizeof(T) * N];

    operator Buffer<T> *() { return reinterpret_cast<Buffer<T> *>(this); }
};

template <typename T>
struct Buffer
{
    BufferMetrics metrics;

    T *allocate(size_t n)
    {
        auto new_used = metrics.size + n;
        if (new_used > metrics.capacity)
        {
            assert(false && "bad_alloc");
            throw std::bad_alloc();
        }
        auto ret = reinterpret_cast<T *>(reinterpret_cast<std::byte *>(this) + DATA_OFFSET) + metrics.size;
        metrics.size = new_used;
        return ret;
    }

    Buffer() = delete;

private:
    static constexpr size_t DATA_OFFSET = reinterpret_cast<size_t>(
        &(reinterpret_cast<const SizedBuffer<T, 1> *>(nullptr)->data));
};

template <typename T>
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

    FixedAllocator(Buffer<T> *buffer)
        : buffer(buffer) {}

    template <typename U>
    FixedAllocator(const FixedAllocator<U> &other)
    {
#ifdef _DEBUG
        if constexpr (std::is_same_v<T, U>)
            buffer = reinterpret_cast<Buffer<T> *>(other.buffer);
        else
            buffer = nullptr; // Rebound allocator uses heap
#else
        static_assert(std::is_same_v<T, U>, "Rebound allocator between different types is not supported in release mode");
        buffer = reinterpret_cast<Buffer<T> *>(other.buffer);
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
        using other = FixedAllocator<U>;
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
    Buffer<T> *buffer;

    template <typename U>
    friend class FixedAllocator;
};

template <typename T, size_t N = 0>
class FixedVector : public std::vector<T, FixedAllocator<T>>
{
public:
    using allocator_type = FixedAllocator<T>;
    using base = std::vector<T, allocator_type>;

    FixedVector() : base(allocator_type(buffer))
    {
        base::reserve(N);
    }

    using base::vector;

    operator FixedVector<T> &() { return *reinterpret_cast<FixedVector<T> *>(this); }

private:
    SizedBuffer<T, N> buffer;
};

template <size_t N = 0>
class FixedString : public std::basic_string<char, std::char_traits<char>, FixedAllocator<char>>
{
public:
    static_assert(N % 16 == 0, "N must be a multiple of 16");

    using allocator_type = FixedAllocator<char>;
    using base = std::basic_string<char, std::char_traits<char>, allocator_type>;

    FixedString() : base(allocator_type(buffer))
    {
        base::reserve(N - 1); // reserve space for null-terminator
    }

    using base::basic_string;

    operator FixedString &() { return *reinterpret_cast<FixedString *>(this); }

private:
    SizedBuffer<char, N> buffer;
};

template <size_t N = 0>
class FixedWString : public std::basic_string<wchar_t, std::char_traits<wchar_t>, FixedAllocator<wchar_t>>
{
public:
    static_assert(N % 16 == 0, "N must be a multiple of 16");

    using allocator_type = FixedAllocator<wchar_t>;
    using base = std::basic_string<wchar_t, std::char_traits<wchar_t>, allocator_type>;

    FixedWString() : base(allocator_type(buffer))
    {
        base::reserve(N - 1); // reserve space for null-terminator
    }

    using base::basic_string;

    operator FixedWString &() { return *reinterpret_cast<FixedWString *>(this); }

private:
    SizedBuffer<wchar_t, N> buffer;
};
