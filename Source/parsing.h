#pragma once

#include <cassert>
#include <cctype>
#include <iostream>
#include <string_view>
#include <tuple>

namespace
{
    struct LitBase
    {
    };
    struct UIntBase
    {
    };
    struct TransformBase
    {
    };

    template <typename Element>
    constexpr bool parse_element(std::string_view input, size_t &pos, auto *dst_object)
    {
        if constexpr (std::is_base_of_v<LitBase, Element>)
        {
            constexpr size_t len = std::char_traits<char>::length(Element::str);
            if (pos + len > input.size())
                return false;
            if (input.substr(pos, len) != Element::str)
                return false;
            pos += len;
        }
        else if constexpr (std::is_base_of_v<UIntBase, Element>)
        {
            auto &val = dst_object->*(Element::member_ptr);
            auto [ptr, ec] = std::from_chars(input.data() + pos, input.data() + input.size(), val, Element::base);
            if (ec != std::errc())
                return false;
            pos = ptr - input.data();
        }
        else if constexpr (std::is_base_of_v<TransformBase, Element>)
        {
            // apply transform to last element
            static_assert("Not implemented");
        }

        return true;
    }

    template <typename... Elements>
    constexpr bool parse_elements(std::string_view input, size_t &pos, auto *dst_object)
    {
        // Fold expression over types
        return (parse_element<Elements>(input, pos, dst_object) && ...);
    }
}

namespace HerosInsight::Parsing
{
    template <const char *s>
    struct Lit : LitBase
    {
        inline static constexpr const char *str = s;
    };
    template <size_t base, auto MemberPtr>
    struct UInt : UIntBase
    {
        inline static constexpr auto member_ptr = MemberPtr;
        inline static constexpr size_t base = base;
    };
    template <uint32_t (*F)(uint32_t)>
    struct Transform : TransformBase
    {
        inline static constexpr uint32_t (*f)(uint32_t) = F;
    };

    template <typename Pattern>
    constexpr bool read_pattern(std::string_view &remaining, auto *dst_object)
    {
        size_t pos = 0;
        bool success = [&]<typename... Ts>(std::tuple<Ts...>)
        {
            return parse_elements<Ts...>(remaining, pos, dst_object);
        }(Pattern{});
        if (!success)
            return false;
        remaining = remaining.substr(pos);
        return true;
    }

    template <typename Pattern>
    constexpr std::string_view find_pattern(std::string_view input, auto *dst_object)
    {
        // Check first element is LitBase at compile time
        using First = std::tuple_element_t<0, Pattern>;
        static_assert(std::is_base_of_v<LitBase, First>, "First element must be a Lit type");

        // Find the literal
        auto start_pos = input.find(First::str, 0);
        if (start_pos == std::string_view::npos)
            return {};

        auto pos = start_pos + std::char_traits<char>::length(First::str);

        // Parse the rest using compile-time tuple unpacking
        bool success = [&]<typename First2, typename... Rest>(std::tuple<First2, Rest...>)
        {
            // First2 should be same as First, we just parse Rest...
            return (parse_elements<Rest...>(input, pos, dst_object));
        }(Pattern{});
        if (!success)
            return {};
        return input.substr(start_pos, pos - start_pos);
    }
}