#include <ranges>

#include <bitview.h>
#include <multibuffer.h>
#include <rich_text.h>
#include <utils.h>

#include "matcher.h"

namespace HerosInsight
{
    FORCE_INLINE bool CharCompare(char a, char b)
    {
        return a == b || (a >= 'A' && a <= 'Z' && ((a + 32) == b));
    }

    FORCE_INLINE bool IsBoundary(std::string_view text, size_t offset)
    {
        if (offset == 0 || offset == text.size())
            return true;
        if ((text[offset] & 0xC0) == 0x80) // UTF-8 continuation
            return false;
        if (text[offset] == '\x1') // start of number in our own encoding
            return true;
        if (Utils::IsAlpha(text[offset]))
            return !Utils::IsAlpha(text[offset - 1]);
        // if (Utils::IsDigit(text[offset]))
        //     return !Utils::IsDigit(text[offset - 1]);
        if (text[offset - 1] != text[offset])
            return true;
        return false;
    }

    bool TryReadNumberOrFraction(std::string_view &rem, float &num)
    {
        auto r = rem;
        if (Utils::TryRead('-', r) ||
            Utils::TryRead('+', r)) // Leading + or - is not treated as part of the number
            return false;
        if (Utils::TryReadNumber(r, num))
        {
            float den;
            if (Utils::TryRead('/', r) &&
                Utils::TryReadNumber(r, den))
            {
                num /= den;
            }
            rem = r;
            return true;
        }
        return false;
    }

    // ' ' (space) means: "new word" and makes distinct
    // '..' (2 dots) means: skip non-space
    // '...' (3 dots) means: skip anything

    Matcher::Matcher(std::string_view source)
    {
        using Type = Matcher::Atom::Type;
        using SearchBound = Matcher::Atom::SearchBound;
        using PostCheck = Matcher::Atom::PostCheck;

        auto search_bound = SearchBound::Anywhere;

        auto rem = source;
        for (; !rem.empty(); search_bound = SearchBound::WithinXWords)
        {
            auto &atom = this->atoms.emplace_back();

            bool is_distinct = true;
            while (true) // Parse search bound
            {
                bool has_spaces = Utils::TryReadSpaces(rem);
                atom.within_count += has_spaces;
                is_distinct |= has_spaces;
                if (has_spaces)
                {
                    search_bound = std::max(search_bound, SearchBound::WithinXWords);
                }
                bool has_2dots = Utils::TryRead("..", rem);
                bool has_3dots = has_2dots && Utils::TryRead('.', rem);
                if (has_3dots)
                {
                    search_bound = SearchBound::Anywhere;
                }
                is_distinct &= !has_2dots;
                if (!has_2dots)
                    break;
            }
            atom.search_bound = search_bound;

            if (rem.empty())
            {
                atoms.pop_back();
                break;
            }

            if (Utils::TryRead('#', rem))
            {
                atom.type = Type::AnyNumber;
                continue;
            }

            {
                auto r = rem;
                if (Utils::TryRead('<', r))
                {
                    atom.post_check |= PostCheck::NumLess;
                }
                else if (Utils::TryRead('>', r))
                {
                    atom.post_check |= PostCheck::NumGreater;
                }
                else
                {
                    atom.post_check |= PostCheck::NumEqual;
                }

                if (Utils::TryRead('=', r))
                {
                    atom.post_check |= PostCheck::NumEqual;
                }

                float num;
                if (TryReadNumberOrFraction(r, num))
                {
                    auto type = Type::AnyNumber;
                    if ((atom.post_check & PostCheck::NumChecks) == PostCheck::NumEqual)
                    {
                        // Pure equal, we can just std::find it
                        Utils::RemoveFlags(atom.post_check, PostCheck::NumChecks);
                        type = Type::ExactNumber;
                    }
                    atom.type = type;
                    atom.src_str = std::string_view(rem.data(), r.data() - rem.data());
                    atom.enc_num = Text::EncodeSearchableNumber(num);
                    rem = r;
                    continue;
                }
            }

            atom.type = Type::String;
            if (is_distinct)
                atom.post_check |= PostCheck::Distinct;

            if (!rem.empty() && !Utils::IsSpace(rem[0]))
            {
                size_t i = 0;
                do
                {
                    ++i;
                } while (!IsBoundary(rem, i));
                auto str = rem.substr(0, i);
                atom.src_str = str;
                atom.post_check |= PostCheck::CaseSubset;
                rem = rem.substr(i);
                continue;
            }

            atom.src_str = "";
        }

        auto values = atoms | std::views::transform(&Atom::src_str);
        this->strings = LoweredTextVector(values);
        for (size_t i = 0; i < atoms.size(); ++i)
        {
            auto &atom = atoms[i];
            atom.needle = this->strings[i];

            // Optimization: When the atom requires to be a case subset
            // but there are no upper-case letters in the needle, we can skip the case check
            // ("nothing" is guaranteed to be a subset of "anything")
            if (Utils::HasAnyFlag(atom.post_check, PostCheck::CaseSubset) &&
                !atom.needle.uppercase.Any())
            {
                Utils::RemoveFlags(atom.post_check, PostCheck::CaseSubset);
            }
        }
        work_mem.resize(atoms.size());
    }

    void SkipTags(std::string_view text, size_t &offset)
    {
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

    size_t FindNextWord(std::string_view text, size_t offset)
    {
        offset = text.find(' ', offset);
        if (offset == std::string::npos)
            return std::string::npos; // failed
        offset = text.find_first_not_of(' ', offset);
        if (offset == std::string::npos)
            return std::string::npos; // failed
        return offset;
    }

    bool IsSubstring(LoweredText text, LoweredText sub, size_t offset)
    {
        if (offset + sub.text.size() > text.text.size())
            return false;
        return text.text.substr(offset, sub.text.size()) == sub.text &&
               sub.uppercase.IsSubsetOf(text.uppercase.Subview(offset, sub.text.size()));
    }

    size_t FindAnyNumber()
    {
        return std::string::npos;
    }

    size_t FindPrefixedWord(LoweredText haystack, LoweredText prefix, size_t offset)
    {
        while (true)
        {
            offset = haystack.text.find(prefix.text, offset);
            if (offset == std::string::npos)
            {
                return std::string::npos; // failed
            }

            if ((offset > 0 && haystack.text[offset - 1] != ' ') ||
                !prefix.uppercase.IsSubsetOf(haystack.uppercase.Subview(offset, prefix.text.size())))
            {
                ++offset;
                continue; // failed but try again
            }

            return offset;
        }
    }

    size_t CalcLimit(Matcher::Atom atom, LoweredText text, size_t offset)
    {
        using SearchBound = Matcher::Atom::SearchBound;

        size_t search_space_max;
        switch (atom.search_bound)
        {
            case SearchBound::WithinXWords:
                for (size_t i = 0; i < atom.within_count; ++i)
                {
                    offset = FindNextWord(text.text, offset);
                    if (offset == std::string::npos)
                        return text.text.size();
                }
                search_space_max = text.text.find(' ', offset);
                break;

            case SearchBound::Anywhere:
            default:
                return text.text.size();
        }
        return std::min(search_space_max, text.text.size());
    }

    struct FindResult
    {
        size_t begin;
        size_t end;

        bool Success() const
        {
            return begin != std::string::npos;
        }

        size_t Len() const { return end - begin; }
    };

    size_t Find(Matcher::Atom &atom, LoweredText haystack, size_t offset)
    {
        using Type = Matcher::Atom::Type;

        switch (atom.type)
        {
            case Type::String:
            case Type::ExactNumber:
                offset = haystack.text.find(atom.GetNeedleStr(), offset);
                break;
            case Type::AnyNumber:
                offset = haystack.text.find('\x1', offset);
                break;
        }

        return offset;
    }

    bool PostChecks(Matcher::Atom &atom, LoweredText haystack, size_t offset)
    {
        if (Utils::HasAnyFlag(atom.post_check, Matcher::Atom::PostCheck::Distinct))
        {
            if (!IsBoundary(haystack.text, offset))
                return false;
        }

        if (Utils::HasAnyFlag(atom.post_check, Matcher::Atom::PostCheck::CaseSubset))
        {
            if (!atom.needle.uppercase.IsSubsetOf(haystack.uppercase.Subview(offset, atom.needle.text.size())))
                return false;
        }

        if (Utils::HasAnyFlag(atom.post_check, Matcher::Atom::PostCheck::NumChecks))
        {
            bool num_in_range = false;
            auto needle = atom.GetNeedleStr();
            auto found = haystack.text.substr(offset, needle.size());
            // clang-format off
            if (                 Utils::HasAnyFlag(atom.post_check, Matcher::Atom::PostCheck::NumLess   )) num_in_range |= found <  needle; else
            if (                 Utils::HasAnyFlag(atom.post_check, Matcher::Atom::PostCheck::NumGreater)) num_in_range |= found >  needle;
            if (!num_in_range && Utils::HasAnyFlag(atom.post_check, Matcher::Atom::PostCheck::NumEqual  )) num_in_range |= found == needle;
            // clang-format on
            if (!num_in_range)
                return false;
        }

        return true;
    }

    bool Matcher::Match(LoweredText text, size_t offset)
    {
        auto n_atoms = atoms.size();

        size_t horizon = 0;
        size_t match_end;
        for (size_t i = 0; i < n_atoms; ++i)
        {
            auto atom = &atoms[i];
            auto mrec = &work_mem[i];

            mrec->limit = CalcLimit(*atom, text, offset);

            if (i < horizon && offset <= mrec->begin) // We already have a cached 'Find'
            {
                offset = mrec->begin;
                goto skip_find;
            }
            --offset;
            horizon += (i == horizon);
            while (true)
            {
                offset = Find(*atom, text, offset + 1);
                if (offset == std::string::npos)
                    return false;
                mrec->begin = offset;

            skip_find:
                if (offset >= mrec->limit)
                {
                    // Backtrack
                    if (i == 0)
                        return false;
                    --i;
                    atom = &atoms[i];
                    mrec = &work_mem[i];
                    offset = mrec->begin;
                    continue;
                }
                if (!PostChecks(*atom, text, offset))
                {
                    // Retry
                    continue;
                }
                // Success, record end
                auto needle = atom->GetNeedleStr();
                offset += needle.size();
                mrec->end = offset;
                break;
            }
        }

        return true;
    }

    bool Matcher::Matches(LoweredText text, std::vector<uint16_t> &matches)
    {
        if (atoms.empty())
            return true;

        size_t offset = 0;
        bool matched = false;
        while (offset < text.text.size())
        {
            if (!Match(text, offset))
                return matched;

            matched = true;

            for (auto &record : work_mem)
            {
                matches.push_back(record.begin);
                matches.push_back(record.end);
            }
            offset = work_mem.back().end + 1;
        }
        return matched;
    }

    std::string Matcher::Atom::ToString() const
    {
        switch (type)
        {
                // case Atom::Type::PrefixedWord:
                //     return std::format("String: '{}'", std::get<std::string_view>(value));
                // case Atom::Type::ExactString:
                //     return std::format("ExactString: '{}'", std::get<std::string_view>(value));

                // case Atom::Type::NumberEqual:
                //     return std::format("Number = '{}'", std::get<double>(value));
                // case Atom::Type::NumberLessThan:
                //     return std::format("Number < '{}'", std::get<double>(value));
                // case Atom::Type::NumberLessThanOrEqual:
                //     return std::format("Number <= '{}'", std::get<double>(value));
                // case Atom::Type::NumberGreaterThan:
                //     return std::format("Number > '{}'", std::get<double>(value));
                // case Atom::Type::NumberGreaterThanOrEqual:
                //     return std::format("Number >= '{}'", std::get<double>(value));
                // case Atom::Type::Number:
                //     return "Number";

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