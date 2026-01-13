#pragma once

#include <cassert>
#include <cctype>
#include <iostream>
#include <string_view>
#include <tuple>

#include <buffer.h>
#include <constants.h>

namespace
{
    using HerosInsight::OutBuf;

    // clang-format off
    struct LitTag{};
    struct ToMemberTag{};
    struct IntTag{};
    struct ColorTag{};
    struct CaptureUntilTag{};
    struct TransformTag{};
    // clang-format on

    template <size_t N>
    struct FixedString
    {
        char data[N] = {};

        constexpr FixedString(const char (&str)[N])
        {
            for (size_t i = 0; i < N; ++i)
            {
                data[i] = str[i];
            }
        }

        // Conversion to string_view for easy use
        constexpr std::string_view str() const
        {
            return std::string_view(data, N - 1); // Exclude null terminator
        }
    };

    // We embed a string directly into the struct at the type level
    template <FixedString S>
    struct Lit : LitTag
    {
        constexpr static std::string_view str() { return S.str(); }
    };

    template <auto MemberPtr>
    struct ToMember : ToMemberTag
    {
        inline static constexpr auto member_ptr = MemberPtr;
    };
    template <size_t base, auto MemberPtr>
    struct Int : IntTag, ToMember<MemberPtr>
    {
        inline static constexpr size_t base = base;
    };
    template <auto MemberPtr>
    struct Color : ColorTag, ToMember<MemberPtr>
    {
    };
    template <FixedString S, auto MemberPtr>
    struct CaptureUntil : CaptureUntilTag, ToMember<MemberPtr>
    {
        constexpr static std::string_view str() { return S.str(); }
    };
    template <uint32_t (*F)(uint32_t)>
    struct Transform : TransformTag
    {
        inline static constexpr uint32_t (*f)(uint32_t) = F;
    };

    constexpr bool TryRead(std::string_view input, size_t &pos, std::string_view expected)
    {
        bool success = pos + expected.size() <= input.size() && input.substr(pos, expected.size()) == expected;
        if (success)
            pos += expected.size();
        return success;
    }

    template <typename Element>
    constexpr bool parse_element(std::string_view input, size_t &pos, auto *dst_object)
    {
        if constexpr (std::is_base_of_v<LitTag, Element>)
        {
            return TryRead(input, pos, Element::str());
        }
        else if constexpr (std::is_base_of_v<ToMemberTag, Element>)
        {
            auto &val = dst_object->*(Element::member_ptr);
            if constexpr (std::is_base_of_v<IntTag, Element>)
            {
                auto [ptr, ec] = std::from_chars(input.data() + pos, input.data() + input.size(), val, Element::base);
                if (ec == std::errc())
                {
                    pos = ptr - input.data();
                    return true;
                }
            }
            else if constexpr (std::is_base_of_v<ColorTag, Element>)
            {
                if (input[pos] == '#')
                {
                    ++pos;
                    uint32_t color;
                    auto [ptr, ec] = std::from_chars(input.data() + pos, input.data() + input.size(), color, 16);
                    if (ec != std::errc())
                        return false;

                    auto end_pos = ptr - input.data();
                    auto len = end_pos - pos;
                    if (len == 6)
                    {
                        color |= 0xFF000000;
                    }
                    else if (len != 8)
                    {
                        return false;
                    }
                    val = color;
                    pos = end_pos;
                    return true;
                }
                else if (input[pos] == '@')
                {
                    ++pos;
                    if (TryRead(input, pos, "skilldull"))
                    {
                        val = Constants::GWColors::skill_dull_gray;
                        return true;
                    }
                    else if (TryRead(input, pos, "skilldyn"))
                    {
                        val = Constants::GWColors::skill_dynamic_green;
                        return true;
                    }
                }
            }
            else if constexpr (std::is_base_of_v<CaptureUntilTag, Element>)
            {
                auto start = pos;
                pos = input.find(Element::str(), pos);
                if (pos == std::string_view::npos)
                    return false;
                val = input.substr(start, pos - start);
                return true;
            }
        }
        else
        {
            static_assert("Not implemented");
        }

        return false;
    }

    template <typename Element>
    constexpr void serialize_element(OutBuf<char> output, auto *src_object)
    {
        if constexpr (std::is_base_of_v<LitTag, Element>)
        {
            // Lit: just append the string
            output.AppendRange(Element::str());
        }
        else if constexpr (std::is_base_of_v<ToMemberTag, Element>)
        {
            auto &val = src_object->*(Element::member_ptr);
            if constexpr (std::is_base_of_v<IntTag, Element>)
            {
                output.AppendIntToChars(val, Element::base);
            }
            else if constexpr (std::is_base_of_v<ColorTag, Element>)
            {
                output.push_back('#');
                output.AppendIntToChars(val, 16);
            }
            else if (std::is_base_of_v<CaptureUntilTag, Element>)
            {
                output.AppendRange(val);
                output.AppendRange(Element::str());
            }
        }
        else
        {
            static_assert("Not implemented");
        }
    }

    template <typename... Elements>
    constexpr bool parse_elements(std::string_view input, size_t &pos, auto *dst_object)
    {
        // Fold expression over types
        return (parse_element<Elements>(input, pos, dst_object) && ...);
    }

    template <typename... Elements>
    constexpr void serialize_elements(OutBuf<char> output, auto *src_object)
    {
        // Fold expression over types
        (serialize_element<Elements>(output, src_object), ...);
    }
}

namespace HerosInsight::Parsing
{
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
        static_assert(std::is_base_of_v<LitTag, First>, "First element must have static str");

        // Find the literal
        auto start_pos = input.find(First::str(), 0);
        if (start_pos == std::string_view::npos)
            return {};

        auto pos = start_pos + First::str().size();

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

    template <typename Pattern>
    constexpr void write_pattern(OutBuf<char> output, auto *src_object)
    {
        return [&]<typename... Ts>(std::tuple<Ts...>)
        {
            serialize_elements<Ts...>(output, src_object);
        }(Pattern{});
    }
}