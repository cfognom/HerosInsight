#include <rich_text.h>
#include <utils.h>

#include "matcher.h"

namespace HerosInsight
{
    Matcher::Matcher(std::string_view source, bool start_insensitive)
    {
        using Type = Matcher::Atom::Type;

        if (start_insensitive)
        {
            this->atoms.push_back({Type::ZeroOrMoreExcept, {}});
            this->atoms.push_back({Type::WordStart, {}});
        }

        auto rem = source;
        while (rem.size())
        {
            auto rem_tmp = rem;
            if ((rem_tmp.data() == source.data() || Utils::TryReadSpaces(rem_tmp)))
            {
                bool success = true;
                Matcher::Atom atom;
                if (Utils::TryRead("....", rem_tmp))
                    atom = {Type::ZeroOrMoreExcept, {}};
                else if (Utils::TryRead("...", rem_tmp))
                    atom = {Type::ZeroOrMoreExcept, ".:"};
                else if (Utils::TryRead("..", rem_tmp))
                    atom = {Type::ZeroOrMoreExcept, ".,:;"};
                else
                    success = false;

                if (success && (rem_tmp.empty() || Utils::TryReadSpaces(rem_tmp)))
                {
                    this->atoms.push_back(atom);
                    rem = rem_tmp;
                    continue;
                }
            }

            if (Utils::TryReadSpaces(rem))
            {
                this->atoms.push_back({Type::OneOrMoreSpaces, {}});
                continue;
            }

            if (Utils::TryRead('#', rem))
            {
                this->atoms.push_back({Type::Number, {}});
                continue;
            }

            // if (Utils::TryRead(':', rem))
            // {
            //     this->atoms.push_back({Type::ExactString, ":"});
            //     this->atoms.push_back({Type::ZeroOrMoreExcept, {}});
            //     continue;
            // }

            if ((rem.size() <= 3 || rem[3] != '.') &&
                Utils::TryRead("...", rem))
            {
                this->atoms.push_back({Type::ZeroOrMoreNonSpace, {}});
                continue;
            }

            if ((rem.size() <= 2 || rem[2] != '.') &&
                Utils::TryRead("..", rem))
            {
                this->atoms.push_back({Type::ZeroOrMoreAlpha, {}});
                continue;
            }

            if (Utils::IsDigit(rem[0]) || rem[0] == '-' || rem[0] == '+')
            {
                double value;
                if (Utils::TryReadNumber(rem, value))
                {
                    this->atoms.push_back({Type::NumberEqual, value});
                    continue;
                }
            }

            Type type = Type::NumberLessThan;
            switch (rem[0])
            {
                case '>':
                    type = Type::NumberGreaterThan;
                case '<':
                {
                    auto rem_tmp = rem.substr(1);
                    if (Utils::TryRead('=', rem_tmp))
                    {
                        ++(*(uint8_t *)&type);
                    }
                    double value;
                    if (Utils::TryReadNumber(rem_tmp, value))
                    {
                        this->atoms.push_back({type, value});
                        rem = rem_tmp;
                        continue;
                    }
                }
            }

            if (Utils::TryRead('"', rem))
            {
                size_t i = 0;
                while (i < rem.size() && rem[0] != '"')
                    ++i;
                auto str = rem.substr(0, i);
                if (!str.empty())
                    this->atoms.push_back({Type::ExactString, str});
                rem = rem.substr(i + 1); // +1 to skip "
                continue;
            }

            if (Utils::IsAlpha(rem[0]))
            {
                size_t i = 0;
                do
                {
                    ++i;
                } while (i < rem.size() && Utils::IsAlpha(rem[i]));
                auto str = rem.substr(0, i);
                this->atoms.push_back({Type::String, str});
                // this->atoms.push_back({Type::ZeroOrMoreNonSpace, {}});
                this->atoms.push_back({Type::ZeroOrMoreExcept, ".,:;"});
                rem = rem.substr(i);
                continue;
            }

            if (!Utils::IsSpace(rem[0]))
            {
                size_t i = 0;
                do
                {
                    ++i;
                } while (i < rem.size() && !Utils::IsSpace(rem[i]) && !Utils::IsAlpha(rem[i]));
                auto str = rem.substr(0, i);
                this->atoms.push_back({Type::ExactString, str});
                rem = rem.substr(i);
                continue;
            }
        }

        this->is_degenerate = true;
        for (const auto &atom : this->atoms)
        {
            bool is_zero_or_more = Type::ZEROORMORE_START <= atom.type && atom.type <= Type::ZEROORMORE_END;
            if (!is_zero_or_more)
            {
                this->is_degenerate = false;
                break;
            }
        }
    }

    FORCE_INLINE bool CharCompare(char a, char b)
    {
        return a == b || std::tolower(a) == b;
    }

    void SkipTags(std::string_view text, size_t &offset)
    {
#ifdef _DEBUG
        assert(offset < text.size() && text[offset] == '<');
#endif
        RichText::TextTag tag;
        auto rem = text.substr(offset);
        while (RichText::TextTag::TryRead(rem, tag))
            ;

        offset = rem.data() - text.data();
    };

    FORCE_INLINE bool TryReadAnyExcept(std::string_view text, size_t &offset, std::string_view except)
    {
        if (offset < text.size() && except.find(text[offset]) == std::string::npos)
        {
            ++offset;
            return true;
        }
        if (except[0] == '.')
        {
            // Special case that ignores "..." when we shouldn't read '.'s
            auto rem = text.substr(offset);
            if (Utils::TryRead("...", rem))
            {
                offset += 3;
                return true;
            }
        }
        return false;
    }

    FORCE_INLINE bool TryReadSpace(std::string_view text, size_t &offset)
    {
        bool success = offset < text.size() && Utils::IsSpace(text[offset]);
        offset += success;
        return success;
    }

    FORCE_INLINE bool TryReadNonSpace(std::string_view text, size_t &offset)
    {
        bool success = offset < text.size() && !Utils::IsSpace(text[offset]);
        offset += success;
        return success;
    }

    FORCE_INLINE bool TryReadAlpha(std::string_view text, size_t &offset)
    {
        bool success = offset < text.size() && Utils::IsAlpha(text[offset]);
        offset += success;
        return success;
    }

    bool Matcher::Matches(std::string_view text, std::vector<uint16_t> *matches)
    {
        if (this->is_degenerate)
            return true;

        const size_t n_atoms = this->atoms.size();

        // Do a quick check if a match is even possible
        size_t offset = 0;
        for (const auto &atom : this->atoms)
        {
            switch (atom.type)
            {
                case Atom::Type::String:
                {
                    auto req_str = std::get<std::string_view>(atom.value);
                    auto it = search(text.begin() + offset, text.end(), req_str.begin(), req_str.end(), CharCompare);

                    if (it == text.end())
                        return false;
                    offset = it - text.begin() + req_str.size();
                    break;
                }

                case Atom::Type::ExactString:
                {
                    auto req_str = std::get<std::string_view>(atom.value);
                    offset = text.find(req_str, offset); // Hopefully uses SIMD
                    if (offset == std::string_view::npos)
                        return false;
                    offset += req_str.size();
                    break;
                }

                case Atom::Type::NumberEqual:
                case Atom::Type::NumberGreaterThan:
                case Atom::Type::NumberGreaterThanOrEqual:
                case Atom::Type::NumberLessThan:
                case Atom::Type::NumberLessThanOrEqual:
                {
                    constexpr std::string_view digits = "0123456789";
                    offset = text.find_first_of(digits, offset);
                    if (offset == std::string_view::npos)
                        return false;
                    offset += 1;
                    break;
                }

                    // case Atom::Type::OneOrMoreSpaces: // Probably not worth the extra cost
                    // {
                    //     offset = text.find(' ', offset);
                    //     if (offset == std::string_view::npos)
                    //         return false;
                    //     ++offset;
                    //     break;
                    // }
            }
        }

        const size_t alloc_size = n_atoms * sizeof(size_t);
#ifdef _DEBUG
        assert(alloc_size <= 6000 && "Too big stack allocation");
#endif
        size_t *atom_ends = (size_t *)alloca(alloc_size);

        size_t size = text.size();
        offset = 0;
        bool did_match = false;
        while (offset < size)
        {
            std::memset(atom_ends, 0xFF, alloc_size);

            for (size_t i = 0; i < n_atoms; ++i)
            {
                if (atoms[i].TryReadMinimum(text, offset) && (atom_ends[i] == std::numeric_limits<size_t>::max() || atom_ends[i] < offset))
                {
                    atom_ends[i] = offset;
                }
                else // Backtrack
                {
                    if (offset == size)
                        return did_match;

                    while (true)
                    {
                        if (i == 0)
                            return did_match;

                        --i;

                        offset = atom_ends[i];

                        // This branch is not needed, but it's a common case and helps speed up the execution
                        if (atoms[i].type == Atom::Type::ZeroOrMoreExcept &&
                            std::get<std::string_view>(atoms[i].value).empty() &&
                            offset < size)
                        {
                            ++offset;
                            atom_ends[i] = offset;
                            break;
                        }

                        if (atoms[i].TryReadMore(text, offset))
                        {
                            atom_ends[i] = offset;
                            break;
                        }
                    }
                }
            }

            did_match = true;

            if (matches)
            {
                size_t atom_start = 0;
                size_t atom_end;
                size_t atom_end_success = -1;
                for (size_t i = 0; i < n_atoms; ++i)
                {
                    auto &atom = atoms[i];
                    atom_end = atom_ends[i];

                    if (atom.type < Atom::Type::REPEATING_START)
                    {
                        size_t size = atom_end - atom_start;
                        if (size > 0)
                        {
                            bool extend_prev = false;
                            if (atom_end_success != -1)
                            {
                                // Check if there is only spaces between last match and current
                                std::string_view between(text.data() + atom_end_success, atom_start - atom_end_success);
                                bool success = true;
                                for (auto c : between)
                                {
                                    if (!Utils::IsSpace(c))
                                    {
                                        success = false;
                                        break;
                                    }
                                }
                                extend_prev = success;
                            }
                            if (extend_prev)
                            {
                                matches->back() = (uint16_t)atom_end;
                            }
                            else
                            {
                                matches->push_back((uint16_t)atom_start);
                                matches->push_back((uint16_t)atom_end);
                            }

                            atom_end_success = atom_end;
                        }
                    }

                    atom_start = atom_end;
                }
            }
        }

        return did_match;
    }

    FORCE_INLINE bool Matcher::Atom::TryReadMinimum(std::string_view text, size_t &offset)
    {
        size_t size = text.size();
        bool case_insensitive = false;
        if (offset < text.size() && text[offset] == '<')
            SkipTags(text, offset);
        switch (type)
        {
            case Atom::Type::WordStart:
            {
                if (offset == 0)
                    return true;
                if (!Utils::IsAlpha(text[offset - 1]))
                    return true;
                return false;
            }

                // clang-format off
            case Atom::Type::OneOrMoreExcept:
            {
                auto except = std::get<std::string_view>(this->value);
                return TryReadAnyExcept(text, offset, except);
            }
            case Atom::Type::OneOrMoreNonSpace: return TryReadNonSpace(text, offset);
            case Atom::Type::OneOrMoreSpaces:   return TryReadSpace(text, offset);
            case Atom::Type::OneOrMoreAlpha:    return TryReadAlpha(text, offset);
                // clang-format on

            case Atom::Type::ZeroOrMoreExcept:
            case Atom::Type::ZeroOrMoreNonSpace:
            case Atom::Type::ZeroOrMoreSpaces:
            case Atom::Type::ZeroOrMoreAlpha:
            {
                return true;
            }

            case Atom::Type::String:
                case_insensitive = true;
            case Atom::Type::ExactString:
            {
                auto req_str = std::get<std::string_view>(this->value);
                size_t req_size = req_str.size();
                if (offset + req_size > size)
                    return false;

                const char *required = req_str.data();
                const char *actual = &text[offset];
                const char *end = actual + req_size;

                while (actual < end)
                {
                    bool success = *actual == *required || (case_insensitive && std::tolower(*actual) == *required);
                    if (!success)
                        return false;
                    ++actual;
                    ++required;
                }
                offset += req_size;
                return true;
            }

            case Atom::Type::Number:
            case Atom::Type::NumberEqual:
            case Atom::Type::NumberLessThan:
            case Atom::Type::NumberLessThanOrEqual:
            case Atom::Type::NumberGreaterThan:
            case Atom::Type::NumberGreaterThanOrEqual:
            {
                if (offset >= size)
                    return false;
                if (offset > 0)
                {
                    char before_char = text[offset - 1];
                    if ((Utils::IsDigit(before_char) || before_char == '-' || before_char == '+'))
                        return false;
                }
                auto rem = text.substr(offset);
                double value, value2;
                bool is_negative = false;
                bool is_dual = false;
                if (Utils::TryRead('-', rem))
                {
                    is_negative = true;
                }
                else
                {
                    Utils::TryRead('+', rem);
                }
                bool success;
                if (Utils::TryRead('(', rem))
                {
                    is_dual = true;
                    success = Utils::TryReadNumber(rem, value) &&
                              Utils::TryRead("...", rem) &&
                              Utils::TryReadNumber(rem, value2) &&
                              Utils::TryRead(')', rem);
                }
                else
                {
                    success = Utils::TryReadNumber(rem, value);
                    RichText::FracTag frac_tag;
                    if (RichText::FracTag::TryRead(rem, frac_tag))
                    {
                        auto frac = (double)frac_tag.num / (double)frac_tag.den;
                        value = success ? value + frac : frac;
                        success = true;
                    }
                    success &= rem.empty() || !Utils::IsAlpha(rem[0]);
                }
                if (!success)
                    return false;
                if (is_negative)
                {
                    value = -value;
                    if (is_dual)
                        value2 = -value2;
                }

                success = true;
                if (type != Atom::Type::Number)
                {
                    auto req_value = std::get<double>(this->value);
                    // clang-format off
                    switch (type)
                    {
                        case Atom::Type::NumberEqual:              success = value == req_value || (is_dual && value2 == req_value); break;
                        case Atom::Type::NumberLessThan:           success = value <  req_value || (is_dual && value2 <  req_value); break;
                        case Atom::Type::NumberLessThanOrEqual:    success = value <= req_value || (is_dual && value2 <= req_value); break;
                        case Atom::Type::NumberGreaterThan:        success = value >  req_value || (is_dual && value2 >  req_value); break;
                        case Atom::Type::NumberGreaterThanOrEqual: success = value >= req_value || (is_dual && value2 >= req_value); break;
                    }
                    // clang-format on
                }
                if (success)
                    offset = rem.data() - text.data();
                return success;
            }

            default:
            {
#ifdef _DEBUG
                SOFT_ASSERT(false, L"Unhandled atom type");
#endif
                return false;
            }
        }
    }

    FORCE_INLINE bool Matcher::Atom::TryReadMore(std::string_view text, size_t &offset)
    {
        if (offset < text.size() && text[offset] == '<')
            SkipTags(text, offset);
        switch (type)
        {
            case Atom::Type::ZeroOrMoreExcept:
            case Atom::Type::OneOrMoreExcept:
                return TryReadAnyExcept(text, offset, std::get<std::string_view>(value));
            case Atom::Type::ZeroOrMoreNonSpace:
            case Atom::Type::OneOrMoreNonSpace:
                return TryReadNonSpace(text, offset);
            case Atom::Type::ZeroOrMoreSpaces:
            case Atom::Type::OneOrMoreSpaces:
                return TryReadSpace(text, offset);
            case Atom::Type::ZeroOrMoreAlpha:
            case Atom::Type::OneOrMoreAlpha:
                return TryReadAlpha(text, offset);

            default:
                return false;
        }
    }

    std::string Matcher::Atom::ToString() const
    {
        switch (type)
        {
            case Atom::Type::String:
                return std::format("String: '{}'", std::get<std::string_view>(value));
            case Atom::Type::ExactString:
                return std::format("ExactString: '{}'", std::get<std::string_view>(value));

            case Atom::Type::NumberEqual:
                return std::format("Number = '{}'", std::get<double>(value));
            case Atom::Type::NumberLessThan:
                return std::format("Number < '{}'", std::get<double>(value));
            case Atom::Type::NumberLessThanOrEqual:
                return std::format("Number <= '{}'", std::get<double>(value));
            case Atom::Type::NumberGreaterThan:
                return std::format("Number > '{}'", std::get<double>(value));
            case Atom::Type::NumberGreaterThanOrEqual:
                return std::format("Number >= '{}'", std::get<double>(value));
            case Atom::Type::Number:
                return "Number";

            case Atom::Type::WordStart:
                return "WordStart";
            case Atom::Type::ZeroOrMoreExcept:
                return std::format("ZeroOrMoreExcept: '{}'", std::get<std::string_view>(value));
            case Atom::Type::ZeroOrMoreNonSpace:
                return "ZeroOrMoreNonSpace";
            case Atom::Type::ZeroOrMoreSpaces:
                return "ZeroOrMoreSpaces";
            case Atom::Type::ZeroOrMoreAlpha:
                return "ZeroOrMoreAlpha";

            case Atom::Type::OneOrMoreExcept:
                return "OneOrMoreAnything";
            case Atom::Type::OneOrMoreNonSpace:
                return "OneOrMoreNonSpace";
            case Atom::Type::OneOrMoreSpaces:
                return "OneOrMoreSpaces";
            case Atom::Type::OneOrMoreAlpha:
                return "OneOrMoreAlpha";

            default:
                return "Unknown";
        }
    }

    std::string Matcher::ToString() const
    {
        std::string result;
        for (size_t i = 0; i < atoms.size(); i++)
        {
            result += std::format("{}: {}\n", i, atoms[i].ToString());
        }
        return result;
    }
}