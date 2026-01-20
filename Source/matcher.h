#pragma once

#include <ranges>
#include <string_view>
#include <variant>
#include <vector>

#include <bitview.h>
#include <multibuffer.h>
#include <string_arena.h>
#include <string_manager.h>
#include <utils.h>

namespace HerosInsight
{
    FORCE_INLINE bool IsUpper(char c)
    {
        return c >= 'A' && c <= 'Z';
    }

    FORCE_INLINE char ToLower(char c)
    {
        if (IsUpper(c))
            return c + 32;
        return c;
    }

    struct LoweredText
    {
        std::string_view text;
        BitView uppercase;

        static void FoldText(std::span<char> text, BitView uppercase)
        {
            auto n_chars = text.size();
            for (size_t i = 0; i < n_chars; ++i)
            {
                auto c = text[i];
                if (IsUpper(c))
                {
                    c += 32;
                    uppercase[i] = true;
                }
                text[i] = c;
            }
        }

        void GetRenderableString(OutBuf<char> out)
        {
            out.AppendRange(text);
            for (auto index : uppercase.IterSetBits())
            {
                out[index] -= 32;
            }
        }

        LoweredText SubStr(size_t offset, size_t count)
        {
            return LoweredText(
                text.substr(offset, count),
                uppercase.Subview(offset, count)
            );
        }
    };

    struct LoweredTextVector
    {
        StringArena<char> arena;
        BitVector uppercase;

        LoweredTextVector() = default;
        template <std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, std::string_view>
        LoweredTextVector(R &&r)
        {
            size_t element_count = 0;
            size_t span_count = 0;
            for (auto &str : r)
            {
                span_count += 1;
                element_count += str.size();
            }
            element_count += span_count; // We add this for the null-terminators
            arena.Reserve(span_count, element_count);
            for (auto &str : r)
            {
                arena.push_back(str);
            }
            LowercaseFold();
        }
        LoweredTextVector(const StringArena<char> &arena) : arena(arena)
        {
            LowercaseFold();
        }
        LoweredTextVector(StringArena<char> &&arena) : arena(std::move(arena))
        {
            LowercaseFold();
        }

        size_t CommitAndFold()
        {
            auto strId = arena.CommitWritten();
            auto committed = arena.Get(strId);
            auto size = arena.Elements().size();
            uppercase.resize(size, false);
            auto pending_uppercase = uppercase.Subview(committed.data() - arena.Elements().data(), committed.size());
            LoweredText::FoldText(committed, pending_uppercase);
            return strId;
        }

        LoweredText Get(size_t index)
        {
            std::string_view str(arena[index]);
            auto offset = str.data() - arena.Elements().data();
            return LoweredText{
                str,
                uppercase.Subview(offset, str.size())
            };
        }

        LoweredText operator[](size_t index)
        {
            return Get(index);
        }

        // Assumes arena contains text to be lowercase folded
        void LowercaseFold()
        {
            std::span<char> chars = arena.Elements();
            uppercase.clear();
            uppercase.resize(chars.size(), false);
            LoweredText::FoldText(chars, uppercase);
        }
    };

    struct Matcher
    {
        struct Atom
        {
            enum struct Type
            {
                String,
                AnyNonSpace,
                AnyNumber,
                EncodedNumber,
            };

            enum struct Location
            {
                WithinWord,
                WithinNextWord,
                WithinSentence,
                Anywhere,
            };

            enum struct PostCheck
            {
                Null = 0,
                WordStart = 1,
                CaseEqual = 2,
                CaseSubset = 4,
                NumEqual = 16,
                NumLess = 32,
                NumGreater = 64,
                NumChecks = NumEqual | NumLess | NumGreater,
            };

            Atom(Type type, Location location, std::string_view value) : type(type), location(location), src_str(value) {}
            Atom(Type type, Location location) : type(type), location(location) {}

            Type type;
            PostCheck post_check = PostCheck::Null;
            Location location;
            std::string_view src_str;
            Text::EncodedNumber enc_num;
            LoweredText needle;

            std::string_view GetNeedleStr() const { return type == Type::String ? needle.text : std::string_view(enc_num.data(), enc_num.size()); }

            std::string ToString() const;
        };

        Matcher() = default;
        Matcher(std::string_view source);

        bool Matches(LoweredText text, std::vector<uint16_t> &matches);
        bool Match(LoweredText text, size_t offset);

        bool IsEmpty() const { return atoms.empty(); }

        std::string ToString() const;

    private:
        struct MatchRecord
        {
            uint16_t limit;
            uint16_t begin;
            uint16_t end;
        };

        LoweredTextVector strings;
        std::vector<Atom> atoms;
        std::vector<MatchRecord> work_mem;
    };
}