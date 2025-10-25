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
        std::size_t len = 0;

    protected:
        void push_unchecked(const T &value)
        {
            Span()[len++] = value;
        }

        void push_unchecked(T &&value)
        {
            Span()[len++] = std::move(value);
        }

    public:
        std::span<const T> Span() const
        {
            return static_cast<const Derived *>(this)->Span();
        }

        std::span<T> Span()
        {
            return static_cast<Derived *>(this)->Span();
        }

        bool try_push(const T &value)
        {
            if (size() >= capacity())
                return false;

            push_unchecked(value);
            return true;
        }

        bool try_push(T &&value)
        {
            if (size() >= capacity())
                return false;

            push_unchecked(std::move(value));
            return true;
        }

        void push_back(const T &value)
        {
            assert(size() < capacity());
            push_unchecked(value);
        }

        void push_back(T &&value)
        {
            assert(size() < capacity());
            push_unchecked(std::move(value));
        }

        operator std::span<T>() const
        {
            return WrittenSpan();
        }

        T &emplace_back()
        {
            assert(size() < capacity());
            return Span()[len++];
        }

        std::span<T> RemainingSpan()
        {
            return std::span<T>(Span().data() + size(), remaining());
        }

        std::span<T> WrittenSpan()
        {
            return std::span<T>(Span().data(), size());
        }

        std::span<const T> WrittenSpan() const
        {
            return std::span<const T>(Span().data(), size());
        }

        void AppendRange(std::span<const T> src)
        {
            auto dst = RemainingSpan();
            assert(dst.size() >= src.size());
            std::copy(src.begin(), src.end(), dst.begin());
            AddSize(src.size());
        }

        template <class Writer>
            requires std::invocable<Writer, std::span<T> &>
        void AppendWith(Writer &&op)
        {
            auto span = RemainingSpan();
            op(span);
            AddSize(span.size());
        }

        T pop()
        {
            assert(!empty());
            return std::move(Span()[--len]);
        }

        void remove(std::size_t index)
        {
            assert(index < size());
            std::copy(data() + index + 1, data_end(), data() + index);
            --len;
        }

        void insert(std::size_t index, const T &value)
        {
            assert(index <= size());
            assert(!full());
            std::copy_backward(data() + index, data_end(), data_end() + 1);
            Span()[index] = value;
            ++len;
        }

        T &operator[](std::size_t index)
        {
            assert(index < size());
            return Span()[index];
        }

        const T &operator[](std::size_t index) const
        {
            assert(index < size());
            return Span()[index];
        }

        operator std::span<T>()
        {
            return std::span<T>(Span().data(), size());
        }

        operator std::span<const T>() const
        {
            return std::span<const T>(Span().data(), size());
        }

        // Conversion operator to std::string_view for FixedArrayRef<char>
        operator std::string_view() const
        {
            static_assert(std::is_same_v<T, char>, "Conversion to std::string_view is only available for char arrays");
            return std::string_view(Span().data(), size());
        }

        template <typename Int>
        void AppendIntToChars(Int value, const int base = 10)
        {
            static_assert(std::is_same_v<T, char>, "AppendIntToChars is only available for char arrays");
            auto rem = RemainingSpan();
            auto result = std::to_chars(rem.data(), rem.data() + rem.size(), value, base);
            size_t added_size = result.ptr - rem.data();
            assert(result.ec == std::errc());
            AddSize(added_size);
        }

        template <typename... Args>
        void AppendFormat(const std::format_string<Args...> &format, Args &&...args)
        {
            static_assert(std::is_same_v<T, char>, "Format is only available for char arrays");
            auto rem = RemainingSpan();
            auto result = std::format_to_n(rem.begin(), rem.size(), format, std::forward<Args>(args)...);
            size_t added_size = result.out - rem.begin();
            assert(added_size == result.size);
            AddSize(added_size);
        }

        // Returns the number of written chars
        template <typename... Args>
        int PushFormat(const char *format, Args... args)
        {
            static_assert(std::is_same_v<T, char>, "Format is only available for char or wchar_t arrays");
            auto rem_size = capacity() - size();
            int n = std::snprintf(Span().data() + size(), rem_size, format, args...);
            if (n < 0 || static_cast<std::size_t>(n) >= rem_size)
            {
                throw std::runtime_error("Format failed or output was truncated");
            }
            len += static_cast<std::size_t>(n);
            return n;
        }

        // Returns the number of written chars
        template <typename... Args>
        int PushFormat(const wchar_t *format, Args... args)
        {
            static_assert(std::is_same_v<T, wchar_t>, "Format is only available for char or wchar_t arrays");
            auto rem_size = capacity() - size();
            int n = std::swprintf(Span().data() + size(), rem_size, format, args...);
            if (n < 0 || static_cast<std::size_t>(n) >= rem_size)
            {
                throw std::runtime_error("Format failed or output was truncated");
            }
            len += static_cast<std::size_t>(n);
            return n;
        }

        // Method to get the capacity
        std::size_t capacity() const
        {
            return Span().size();
        }

        std::size_t remaining() const
        {
            return capacity() - size();
        }

        std::size_t size() const
        {
            return len;
        }

        bool empty() const
        {
            return size() == 0;
        }

        bool full() const
        {
            return size() == capacity();
        }

        void SetSize(std::size_t new_size)
        {
            assert(new_size <= capacity());
            len = new_size;
        }

        void AddSize(std::size_t add_size)
        {
            SetSize(size() + add_size);
        }

        T *data()
        {
            return Span().data();
        }

        // Method to get a pointer to the end of the buffer
        T *data_end()
        {
            return Span().data() + size();
        }

        // Method to get a const pointer to the end of the buffer
        const T *data_end() const
        {
            return Span().data() + size();
        }

        T &back()
        {
            assert(size() > 0);
            return WrittenSpan().back();
        }

        // Method to clear the buffer
        void clear()
        {
            len = 0;
        }

        // Iterator support

        auto begin()
        {
            return WrittenSpan().begin();
        }

        auto end()
        {
            return WrittenSpan().end();
        }

        auto begin() const
        {
            return WrittenSpan().begin();
        }

        auto end() const
        {
            return WrittenSpan().end();
        }

        auto cbegin() const
        {
            return WrittenSpan().cbegin();
        }

        auto cend() const
        {
            return WrittenSpan().cend();
        }
    };

    template <typename T>
    class SpanWriter : public BufferBase<SpanWriter<T>, T>
    {
        std::span<T> span;

    public:
        SpanWriter(std::span<T> span) : span(span) {}

        std::span<T> Span() const
        {
            return span;
        }
    };

    template <typename T, std::size_t N>
    class FixedVector : public BufferBase<FixedVector<T, N>, T>
    {
        std::array<T, N> array;

    public:
        constexpr FixedVector() = default;
        constexpr FixedVector(std::initializer_list<T> init_list)
        {
            assert(init_list.size() <= N);
            for (const auto &value : init_list)
            {
                this->push_unchecked(value);
            }
        }

        std::span<const T> Span() const
        {
            return std::span<const T>(array.data(), array.size());
        }

        std::span<T> Span()
        {
            return std::span<T>(array.data(), array.size());
        }
    };
}
