#pragma once

#include <string_view>
#include <variant>
#include <vector>

#include <utils.h>

namespace HerosInsight
{
    struct Matcher
    {
        struct Atom
        {
            enum struct Type
            {
                String,
                ExactString,
                Number,
                NumberEqual,
                NumberLessThan,
                NumberLessThanOrEqual,
                NumberGreaterThan,
                NumberGreaterThanOrEqual,

                REPEATING_START,

                ZEROORMORE_START,

                ZeroOrMoreAnything = ZEROORMORE_START,
                ZeroOrMoreNonSpace,
                ZeroOrMoreSpaces,

                ZEROORMORE_END,

                ONEORMORE_START,

                OneOrMoreAnything = ONEORMORE_START,
                OneOrMoreNonSpace,
                OneOrMoreSpaces,

                ONEORMORE_END,
            };
            constexpr static size_t ZEROORMORE_COUNT = (size_t)Type::ZEROORMORE_END - (size_t)Type::ZEROORMORE_START;
            constexpr static size_t ONEORMORE_COUNT = (size_t)Type::ONEORMORE_END - (size_t)Type::ONEORMORE_START;
            constexpr static size_t ZERO_TO_ONE_SHIFT = (size_t)Type::ONEORMORE_START - (size_t)Type::ZEROORMORE_START;
            static_assert(ONEORMORE_COUNT == ZEROORMORE_COUNT, "ZEROORMORE_COUNT != ONEORMORE_COUNT");

            Type type;
            std::variant<std::string_view, double> value;

            std::string ToString() const;

            bool TryReadMinimum(std::string_view text, size_t &offset);
            bool TryReadMore(std::string_view text, size_t &offset);
        };

        Matcher(std::string_view source);

        bool Matches(std::string_view text, std::vector<uint16_t> &matches);

        bool IsEmpty() const { return atoms.empty(); }

        std::string ToString() const;

    private:
        std::vector<Atom> atoms;
    };
}