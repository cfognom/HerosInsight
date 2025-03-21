#include "utils.h"

#include "matcher.h"

namespace HerosInsight
{
    Matcher::Matcher(std::string_view source)
    {
        using Type = Matcher::Atom::Type;

        char *p = (char *)source.data();
        char *end = p + source.size();
        while (p < end)
        {
            if (Utils::TryReadSpaces(p, end))
            {
                this->atoms.push_back({Type::OneOrMoreSpaces, {}});
                continue;
            }

            if (Utils::TryRead('#', p, end))
            {
                this->atoms.push_back({Type::Number, {}});
                continue;
            }

            if ((p + 3 >= end || p[3] != '.') &&
                Utils::TryRead("...", p, end))
            {
                this->atoms.push_back({Type::ZeroOrMoreAnything, {}});
                continue;
            }

            if ((p + 2 >= end || p[2] != '.') &&
                Utils::TryRead("..", p, end))
            {
                this->atoms.push_back({Type::OneOrMoreNonSpace, {}});
                continue;
            }

            if (Utils::IsDigit(p[0]))
            {
                double value = strtod(p, &p);
                this->atoms.push_back({Type::NumberEqual, value});
                continue;
            }

            Type type = Type::NumberLessThan;
            switch (p[0])
            {
                case '>':
                    type = Type::NumberGreaterThan;
                case '<':
                {
                    ++p;
                    if (Utils::TryRead('=', p, end))
                    {
                        ++(*(uint8_t *)&type);
                    }
                    double value = strtod(p, &p);
                    this->atoms.push_back({type, value});
                    continue;
                }
            }

            if (Utils::TryRead('"', p, end))
            {
                auto start = p;
                while (p < end && *p != '"')
                    ++p;
                this->atoms.push_back({Type::ExactString, std::string_view(start, p - start)});
                ++p;
                continue;
            }

            if (Utils::IsAlpha(*p))
            {
                auto start = p;
                do
                {
                    ++p;
                } while (p < end && Utils::IsAlpha(*p));
                this->atoms.push_back({Type::String, std::string_view(start, p - start)});
                if (Utils::TryReadSpaces(p, end))
                {
                    this->atoms.push_back({Type::ZeroOrMoreNonSpace, {}});
                    this->atoms.push_back({Type::OneOrMoreSpaces, {}});
                }
                continue;
            }

            if (!Utils::IsSpace(*p))
            {
                auto start = p;
                do
                {
                    ++p;
                } while (p < end && !Utils::IsSpace(*p));
                this->atoms.push_back({Type::ExactString, std::string_view(start, p - start)});
                continue;
            }
        }
    }

    FORCE_INLINE bool CharCompare(char a, char b)
    {
        return a == b || std::tolower(a) == b;
    }

    FORCE_INLINE bool TryReadAnyChar(std::string_view text, size_t &offset)
    {
        if (offset < text.size())
        {
            ++offset;
            return true;
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

    bool Matcher::Matches(std::string_view text, std::vector<uint16_t> &matches)
    {
        const size_t n_atoms = this->atoms.size();

        // Do a quick check if a match is even possible
        size_t offset = 0;
        for (const auto &atom : this->atoms)
        {
            switch (atom.type)
            {
                case Atom::Type::ExactString:
                {
                    auto req_str = std::get<std::string_view>(atom.value);
                    offset = text.find(req_str, offset); // Hopefully uses SIMD
                    if (offset == std::string_view::npos)
                        return false;
                    break;
                }

                case Atom::Type::String:
                {
                    auto req_str = std::get<std::string_view>(atom.value);
                    auto it = search(text.begin() + offset, text.end(), req_str.begin(),
                        req_str.end(), CharCompare);

                    if (it == text.end())
                        return false;
                    offset = it - text.begin();
                    break;
                }

                    // case Atom::Type::OneOrMoreSpaces:
                    // {
                    //     offset = text.find(' ', offset);
                    //     if (offset == std::string_view::npos)
                    //         return false;
                    //     break;
                    // }
            }
        }

        const size_t alloc_size = n_atoms * sizeof(size_t);
        assert(alloc_size <= 6000 && "Too big stack allocation");

        size_t *atom_ends = (size_t *)alloca(alloc_size);
        size_t size = text.size();
        std::memset(atom_ends, 0xFF, alloc_size);

        offset = 0;
        for (size_t i = 0; i < n_atoms; ++i)
        {
            if (atoms[i].TryReadMinimum(text, offset) && (atom_ends[i] == std::numeric_limits<size_t>::max() || atom_ends[i] < offset))
            {
                atom_ends[i] = offset;
            }
            else // Backtrack
            {
                if (offset == size)
                    return false;

                while (true)
                {
                    if (i == 0)
                        return false;

                    --i;

                    offset = atom_ends[i];

                    // This branch is not needed, but it's a common case and helps speed up the execution
                    if (atoms[i].type == Atom::Type::ZeroOrMoreAnything && offset < size)
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

        uint16_t atom_start = 0;
        uint16_t atom_end;
        for (size_t i = 0; i < n_atoms; ++i)
        {
            auto &atom = atoms[i];
            atom_end = (uint16_t)atom_ends[i];

            if (atom.type < Atom::Type::REPEATING_START)
            {
                auto size = atom_end - atom_start;
                if (size > 0)
                {
                    matches.push_back(atom_start);
                    matches.push_back(atom_end);
                }
            }

            atom_start = atom_end;
        }

        return true;
    }

    FORCE_INLINE bool Matcher::Atom::TryReadMinimum(std::string_view text, size_t &offset)
    {
        size_t size = text.size();
        bool case_insensitive = false;
        switch (type)
        {
                // clang-format off
            case Atom::Type::OneOrMoreAnything: return TryReadAnyChar(text, offset);
            case Atom::Type::OneOrMoreNonSpace: return TryReadNonSpace(text, offset);
            case Atom::Type::OneOrMoreSpaces:   return TryReadSpace(text, offset);
                // clang-format on

            case Atom::Type::ZeroOrMoreAnything:
            case Atom::Type::ZeroOrMoreNonSpace:
            case Atom::Type::ZeroOrMoreSpaces:
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
                if (Utils::IsSpace(text[offset]))
                    return false;
                if (offset > 0 && Utils::IsDigit(text[offset - 1]))
                    return false;
                const char *text_ptr = &text[offset];
                char *after_number;
                double value = strtod(text_ptr, &after_number);
                if (after_number == text_ptr || Utils::IsAlpha(*after_number))
                    return false;
                if (after_number[-1] == '.')
                    --after_number;
                bool success;
                auto req_value = std::get<double>(this->value);
                // clang-format off
                switch (type)
                {
                    case Atom::Type::NumberEqual:              success = value == req_value; break;
                    case Atom::Type::NumberLessThan:           success = value <  req_value; break;
                    case Atom::Type::NumberLessThanOrEqual:    success = value <= req_value; break;
                    case Atom::Type::NumberGreaterThan:        success = value >  req_value; break;
                    case Atom::Type::NumberGreaterThanOrEqual: success = value >= req_value; break;
                    default:                                   success = true;               break;
                }
                // clang-format on
                if (success)
                    offset = after_number - text.data();
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
        switch (type)
        {
            case Atom::Type::ZeroOrMoreAnything:
            case Atom::Type::OneOrMoreAnything:
                return TryReadAnyChar(text, offset);
            case Atom::Type::ZeroOrMoreNonSpace:
            case Atom::Type::OneOrMoreNonSpace:
                return TryReadNonSpace(text, offset);
            case Atom::Type::ZeroOrMoreSpaces:
            case Atom::Type::OneOrMoreSpaces:
                return TryReadSpace(text, offset);

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

            case Atom::Type::ZeroOrMoreAnything:
                return "ZeroOrMoreAnything";
            case Atom::Type::ZeroOrMoreNonSpace:
                return "ZeroOrMoreNonSpace";
            case Atom::Type::ZeroOrMoreSpaces:
                return "ZeroOrMoreSpaces";

            case Atom::Type::OneOrMoreAnything:
                return "OneOrMoreAnything";
            case Atom::Type::OneOrMoreNonSpace:
                return "OneOrMoreNonSpace";
            case Atom::Type::OneOrMoreSpaces:
                return "OneOrMoreSpaces";

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