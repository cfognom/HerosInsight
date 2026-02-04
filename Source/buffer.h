#pragma once

#include <array>
#include <cassert>
#include <charconv>
#include <cstdio>
#include <format>
#include <initializer_list>
#include <iterator>
#include <span>
#include <stdexcept>

namespace HerosInsight
{
    template <typename Derived, typename T>
    class BufferBase
    {
    protected:
        Derived &AsDerived() { return *static_cast<Derived *>(this); }
        const Derived &AsDerived() const { return *static_cast<const Derived *>(this); }

    public:
        template <typename... Args>
        T &emplace_back(Args &&...args)
        {
            return *std::construct_at(data() + Len()++, std::forward<Args>(args)...);
        }
        size_t &Len() { return AsDerived().len; }
        std::span<T> BackingSpan() { return std::span<T>(data(), capacity()); }
        std::span<T> AsSpan() { return std::span<T>(data(), size()); }
        std::span<const T> AsCSpan() { return std::span<const T>(data(), size()); }
        size_t size() const { return AsDerived().len; }

        bool try_push(const T &value)
        {
            if (size() >= capacity())
                return false;

            emplace_back(value);
            return true;
        }

        bool try_push(T &&value)
        {
            if (size() >= capacity())
                return false;

            emplace_back(std::forward<T>(value));
            return true;
        }

        void push_back(const T &value)
        {
            assert(size() < capacity());
            emplace_back(value);
        }

        void push_back(T &&value)
        {
            assert(size() < capacity());
            emplace_back(std::forward<T>(value));
        }

        std::span<T> WrittenSpan()
        {
            return std::span<T>(data(), size());
        }

        std::span<const T> WrittenSpan() const
        {
            return std::span<const T>(data(), size());
        }

        void AppendRange(std::span<const T> src)
        {
            assert(AvailableCapacity() >= src.size());
            std::uninitialized_copy_n(src.data(), src.size(), data() + size());
            Len() += src.size();
        }

        void AppendString(std::string_view str)
        {
            static_assert(std::is_same_v<T, char>, "AppendString is only available for char arrays");
            AppendRange(std::span<const T>(str));
        }

        void AppendString(std::wstring_view str)
        {
            static_assert(std::is_same_v<T, wchar_t>, "AppendString is only available for char arrays");
            AppendRange(std::span<const T>(str));
        }

        T pop_back()
        {
            assert(!empty());
            size_t index = --Len();
            return std::move(*(data() + index));
        }

        void resize(std::size_t size)
        {
            assert(size <= capacity());
            auto &len = Len();
            if (size < len)
            {
                std::destroy_n(data() + size, len - size);
            }
            else if (size > len)
            {
                std::uninitialized_default_construct_n(data() + size, size - len);
            }
            len = size;
        }

        void remove(std::size_t index)
        {
            assert(index < size());
            T *ptr = data() + index;
            std::destroy_at(ptr);
            size_t count = size() - index - 1;
            std::memmove(ptr, ptr + 1, sizeof(T) * count);
            --Len();
        }

        void insert(std::size_t index, const T &value)
        {
            assert(index <= size());
            assert(!full());
            T *ptr = data() + index;
            size_t count = size() - index;
            std::memmove(ptr + 1, ptr, sizeof(T) * count);
            std::construct_at(ptr, std::forward<T>(value));
            ++Len();
        }

        T &operator[](std::size_t index)
        {
            assert(index < size());
            return *(data() + index);
        }

        const T &operator[](std::size_t index) const
        {
            assert(index < size());
            return *(data() + index);
        }

        operator std::span<T>() { return AsSpan(); }
        operator std::span<const T>() const { return AsCSpan(); }

        // Conversion operator to std::string_view for FixedArrayRef<char>
        operator std::string_view() const
        {
            static_assert(std::is_same_v<T, char>, "Conversion to std::string_view is only available for char arrays");
            return std::string_view(data(), size());
        }

        template <typename Int>
        void AppendIntToChars(Int value, const int base = 10)
        {
            static_assert(std::is_same_v<T, char>, "AppendIntToChars is only available for char arrays");
            auto result = std::to_chars(data() + size(), data() + capacity(), value, base);
            assert(result.ec == std::errc());
            Len() = result.ptr - data();
        }

        template <typename Float>
        void AppendFloatToChars(Float value, std::chars_format format = std::chars_format::general)
        {
            static_assert(std::is_same_v<T, char>, "AppendIntToChars is only available for char arrays");
            auto result = std::to_chars(data() + size(), data() + capacity(), value, format);
            assert(result.ec == std::errc());
            Len() = result.ptr - data();
        }

        template <typename... Args>
        void AppendFormat(const std::basic_format_string<T, std::type_identity_t<Args>...> &format, Args &&...args)
        {
            T *dst = data() + size();
            size_t n_max = capacity() - size();
            auto result = std::format_to_n(dst, n_max, format, std::forward<Args>(args)...);
            size_t n_written = result.out - dst;
            assert(result.size == n_written); // It was not truncated
            Len() = result.out - data();
        }

        // DEPRECATED: Use AppendFormat.
        // Returns the number of written chars
        template <typename... Args>
        int PushFormat(const char *format, Args... args)
        {
            static_assert(std::is_same_v<T, char>, "Format is only available for char or wchar_t arrays");
            auto rem_size = capacity() - size();
            int n = std::snprintf(data() + size(), rem_size, format, args...);
            if (n < 0 || static_cast<std::size_t>(n) >= rem_size)
            {
                throw std::runtime_error("Format failed or output was truncated");
            }
            Len() += static_cast<std::size_t>(n);
            return n;
        }

        // DEPRECATED: Use AppendFormat.
        // Returns the number of written chars
        template <typename... Args>
        int PushFormat(const wchar_t *format, Args... args)
        {
            static_assert(std::is_same_v<T, wchar_t>, "Format is only available for char or wchar_t arrays");
            auto rem_size = capacity() - size();
            int n = std::swprintf(data() + size(), rem_size, format, args...);
            if (n < 0 || static_cast<std::size_t>(n) >= rem_size)
            {
                throw std::runtime_error("Format failed or output was truncated");
            }
            Len() += static_cast<std::size_t>(n);
            return n;
        }

        size_t capacity() const { return AsDerived().capacity(); }
        size_t AvailableCapacity() const { return capacity() - size(); }
        bool empty() const { return size() == 0; }
        bool full() const { return size() == capacity(); }

        T *data() { return AsDerived().data(); }
        const T *data() const { return AsDerived().data(); }
        T *data_end() { return data() + size(); }
        const T *data_end() const { return data() + size(); }

        T &back()
        {
            assert(size() > 0);
            return *(data() + size() - 1);
        }

        // Method to clear the buffer
        void clear()
        {
            std::destroy_n(data(), size());
            Len() = 0;
        }

        // Iterator support
        auto begin() { return WrittenSpan().begin(); }
        auto end() { return WrittenSpan().end(); }
        auto begin() const { return WrittenSpan().begin(); }
        auto end() const { return WrittenSpan().end(); }
        auto cbegin() const { return WrittenSpan().cbegin(); }
        auto cend() const { return WrittenSpan().cend(); }
    };

    // Be careful when using this with types whose constructor and destructor are not trivial: It assumes the span being written to is uninitialized.
    template <typename T>
    class SpanWriter : public BufferBase<SpanWriter<T>, T>
    {
        friend class BufferBase<SpanWriter<T>, T>;

        std::span<T> span;
        std::size_t len = 0;

    public:
        SpanWriter(std::span<T> span) : span(span) {}

        T *data() { return span.data(); }
        const T *data() const { return span.data(); }
        size_t capacity() const { return span.size(); }
    };

    template <typename T, std::size_t N>
    class FixedVector : public BufferBase<FixedVector<T, N>, T>
    {
        friend class BufferBase<FixedVector<T, N>, T>;

        alignas(alignof(T)) char array[N * sizeof(T)];
        std::size_t len = 0;

    public:
        ~FixedVector()
        {
            std::destroy_n(data(), len);
        }
        constexpr FixedVector() = default;
        constexpr FixedVector(std::initializer_list<T> init_list)
        {
            assert(init_list.size() <= N);
            for (const auto &value : init_list)
            {
                this->emplace_back(value);
            }
        }

        T *data() { return (T *)array; }
        const T *data() const { return (const T *)array; }
        size_t capacity() const { return N; }
    };

    template <typename T>
    class OutBuf : public BufferBase<OutBuf<T>, T>
    {
        friend class BufferBase<OutBuf<T>, T>;

        std::span<T> span;
        size_t &len;

    public:
        OutBuf(SpanWriter<T> &src) : span(src.BackingSpan()), len(src.Len()) {}
        template <std::size_t N>
        OutBuf(FixedVector<T, N> &src) : span(src.BackingSpan()), len(src.Len()) {}

        T *data() { return span.data(); }
        const T *data() const { return span.data(); }
        size_t capacity() const { return span.size(); }
    };
}
