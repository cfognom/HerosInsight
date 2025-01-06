#pragma once

#include <array>
#include <span>
#include <stdexcept>
#include <initializer_list>
#include <cstdio>   // Include for snprintf
#include <iterator> // Include for iterator support

// AI GENERATED CODE (some of it)

namespace HerosInsight
{
    template <typename T>
    struct FixedArrayRef
    {
        std::span<T> buffer;
        std::size_t *len_ptr;

        FixedArrayRef() = default;
        FixedArrayRef(std::span<T> buf, std::size_t &len)
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

        T pop()
        {
            if (size() == 0)
            {
                throw std::out_of_range("FixedArray is empty");
            }
            return std::move(buffer[--(*len_ptr)]);
        }

        void remove(std::size_t index)
        {
            if (index >= size())
            {
                throw std::out_of_range("Index out of range");
            }
            std::copy(data() + index + 1, data_end(), data() + index);
            --(*len_ptr);
        }

        void insert(std::size_t index, const T &value)
        {
            if (index > size())
            {
                throw std::out_of_range("Index out of range");
            }
            if (size() >= capacity())
            {
                throw std::out_of_range("FixedArray is full");
            }
            std::copy_backward(data() + index, data_end(), data_end() + 1);
            buffer[index] = value;
            ++(*len_ptr);
        }

        T &operator[](std::size_t index)
        {
            if (index >= size())
            {
                throw std::out_of_range("Index out of range");
            }
            return buffer[index];
        }

        const T &operator[](std::size_t index) const
        {
            if (index >= size())
            {
                throw std::out_of_range("Index out of range");
            }
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

        bool is_valid() const
        {
            return len_ptr != nullptr;
        }

        // Method to get the capacity
        std::size_t capacity() const
        {
            return buffer.size();
        }

        std::size_t size() const
        {
            assert(is_valid());
            return *len_ptr;
        }

        void set_size(std::size_t new_size)
        {
            assert(is_valid());
            if (new_size > capacity())
            {
                throw std::out_of_range("New size is larger than capacity");
            }
            *len_ptr = new_size;
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
    struct FixedArray
    {
        std::array<T, N> buffer_storage;
        std::size_t length_storage = 0;

        FixedArray() = default;

        constexpr FixedArray(std::initializer_list<T> init_list)
        {
            if (init_list.size() > N)
            {
                throw std::out_of_range("Initializer list too large");
            }
            for (const auto &value : init_list)
            {
                buffer_storage[length_storage++] = value;
            }
        }

        FixedArrayRef<T> ref()
        {
            return FixedArrayRef<T>(std::span<T>(buffer_storage.data(), buffer_storage.size()), length_storage);
        }

        const FixedArrayRef<const T> ref() const
        {
            auto length_ref = (std::size_t *)&length_storage;
            return FixedArrayRef<const T>(std::span<const T>(buffer_storage.data(), buffer_storage.size()), *length_ref);
        }

        // Method to get the capacity
        constexpr std::size_t capacity() const
        {
            return N;
        }
    };
}
