#pragma once

#include <array>
#include <cassert>
#include <cstdio> // Include for snprintf
#include <initializer_list>
#include <iterator> // Include for iterator support
#include <span>
#include <stdexcept>

// AI GENERATED CODE (some of it)

namespace HerosInsight
{
    template <typename T>
    struct BufferWriter
    {
        std::span<T> buffer;
        std::size_t *len_ptr; // TODO: Store len directly and rename struct to BufferWriter

        BufferWriter(std::span<T> buf, std::size_t &len)
            : buffer(buf), len_ptr(&len) {}

        bool try_push(const T &value)
        {
            if (size() >= capacity())
            {
                return false;
            }
            buffer[(*len_ptr)++] = std::move(value);
            return true;
        }

        void push_back(const T &value)
        {
            assert(size() < capacity());
            buffer[(*len_ptr)++] = std::move(value);
        }

        T &emplace_back()
        {
            assert(size() < capacity());
            return buffer[(*len_ptr)++];
        }

        std::span<T> RemainingSpan() const
        {
            return std::span<T>(buffer.data() + size(), remaining());
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
            return std::move(buffer[--(*len_ptr)]);
        }

        void remove(std::size_t index)
        {
            assert(index < size());
            std::copy(data() + index + 1, data_end(), data() + index);
            --(*len_ptr);
        }

        void insert(std::size_t index, const T &value)
        {
            assert(index <= size());
            assert(!full());
            std::copy_backward(data() + index, data_end(), data_end() + 1);
            buffer[index] = value;
            ++(*len_ptr);
        }

        T &operator[](std::size_t index)
        {
            assert(index < size());
            return buffer[index];
        }

        const T &operator[](std::size_t index) const
        {
            assert(index < size());
            return buffer[index];
        }

        operator std::span<T>()
        {
            return std::span<T>(buffer.data(), size());
        }

        operator std::span<const T>() const
        {
            return std::span<const T>(buffer.data(), size());
        }

        // Conversion operator to std::string_view for FixedArrayRef<char>
        operator std::string_view() const
        {
            static_assert(std::is_same_v<T, char>, "Conversion to std::string_view is only available for char arrays");
            return std::string_view(buffer.data(), size());
        }

        template <typename... Args>
        void AppendFormat(const std::format_string<Args...> &format, Args &&...args)
        {
            static_assert(std::is_same_v<T, char>, "Format is only available for char arrays");
            auto rem = RemainingSpan();
            auto result = std::format_to_n(rem.begin(), rem.size(), format, std::forward<Args>(args)...);
            size_t size = result.out - rem.begin();
            assert(size == result.size);
            AddSize(size);
        }

        // Returns the number of written chars
        template <typename... Args>
        int PushFormat(const char *format, Args... args)
        {
            static_assert(std::is_same_v<T, char>, "Format is only available for char or wchar_t arrays");
            auto rem_size = capacity() - size();
            int n = std::snprintf(buffer.data() + size(), rem_size, format, args...);
            if (n < 0 || static_cast<std::size_t>(n) >= rem_size)
            {
                throw std::runtime_error("Format failed or output was truncated");
            }
            *len_ptr += static_cast<std::size_t>(n);
            return n;
        }

        // Returns the number of written chars
        template <typename... Args>
        int PushFormat(const wchar_t *format, Args... args)
        {
            static_assert(std::is_same_v<T, wchar_t>, "Format is only available for char or wchar_t arrays");
            auto rem_size = capacity() - size();
            int n = std::swprintf(buffer.data() + size(), rem_size, format, args...);
            if (n < 0 || static_cast<std::size_t>(n) >= rem_size)
            {
                throw std::runtime_error("Format failed or output was truncated");
            }
            *len_ptr += static_cast<std::size_t>(n);
            return n;
        }

        // Method to get the capacity
        std::size_t capacity() const
        {
            return buffer.size();
        }

        std::size_t remaining() const
        {
            return capacity() - size();
        }

        std::size_t size() const
        {
            return *len_ptr;
        }

        bool empty() const
        {
            return size() == 0;
        }

        bool full() const
        {
            return size() == capacity();
        }

        void set_size(std::size_t new_size)
        {
            assert(new_size <= capacity());
            *len_ptr = new_size;
        }

        void AddSize(std::size_t add_size)
        {
            set_size(size() + add_size);
        }

        T *data()
        {
            return buffer.data();
        }

        // Method to get a pointer to the end of the buffer
        T *data_end()
        {
            return buffer.data() + size();
        }

        // Method to get a const pointer to the end of the buffer
        const T *data_end() const
        {
            return buffer.data() + size();
        }

        T &back()
        {
            assert(size() > 0);
            return buffer[size() - 1];
        }

        // Method to clear the buffer
        void clear()
        {
            *len_ptr = 0;
        }

        // Iterator support
        using iterator = typename std::span<T>::iterator;
        using const_iterator = typename std::span<T>::const_iterator;

        iterator begin()
        {
            return buffer.begin();
        }

        iterator end()
        {
            return buffer.begin() + size();
        }

        const_iterator begin() const
        {
            return buffer.begin();
        }

        const_iterator end() const
        {
            return buffer.begin() + size();
        }

        const_iterator cbegin() const
        {
            return buffer.cbegin();
        }

        const_iterator cend() const
        {
            return buffer.cbegin() + size();
        }
    };

    template <typename T, std::size_t N>
    struct Buffer
    {
        std::array<T, N> buffer_storage;
        std::size_t length_storage = 0;

        Buffer() = default;

        constexpr Buffer(std::initializer_list<T> init_list)
        {
            assert(init_list.size() <= N);
            for (const auto &value : init_list)
            {
                buffer_storage[length_storage++] = value;
            }
        }

        BufferWriter<T> ref()
        {
            return BufferWriter<T>(std::span<T>(buffer_storage.data(), buffer_storage.size()), length_storage);
        }

        const BufferWriter<const T> ref() const
        {
            auto length_ref = (std::size_t *)&length_storage;
            return BufferWriter<const T>(std::span<const T>(buffer_storage.data(), buffer_storage.size()), *length_ref);
        }

        // Method to get the capacity
        constexpr std::size_t capacity() const
        {
            return N;
        }
    };
}
