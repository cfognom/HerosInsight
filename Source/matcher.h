#pragma once

#include <ranges>
#include <string_view>
#include <variant>
#include <vector>

#include <bitview.h>
#include <multibuffer.h>
#include <span_vector.h>
#include <string_manager.h>
#include <utils.h>

namespace HerosInsight
{
    FORCE_INLINE constexpr bool IsUpper(char c)
    {
        return c >= 'A' && c <= 'Z';
    }

    FORCE_INLINE char ToLower(char c)
    {
        if (IsUpper(c))
            return c + 32;
        return c;
    }

    struct LoweredText;
    // CRTP base
    template <class Derived>
    struct LoweredTextBase
    {
        constexpr std::string_view Text() const
        {
            return static_cast<const Derived *>(this)->TextView();
        }

        constexpr BitView Uppercase() const
        {
            return static_cast<const Derived *>(this)->UppercaseView();
        }

        void GetRenderableString(OutBuf<char> out) const
        {
            out.AppendRange(Text());
            for (auto index : Uppercase().IterSetBits())
            {
                out[index] -= 32;
            }
        }

        constexpr LoweredText SubStr(size_t offset, size_t count);

        constexpr static void FoldText(std::span<char> text, BitView uppercase)
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
    };

    struct LoweredText : LoweredTextBase<LoweredText>
    {
        std::string_view text;
        BitView uppercase;

        LoweredText() = default;
        LoweredText(std::string_view text, BitView uppercase) : text(text), uppercase(uppercase) {}

        constexpr std::string_view TextView() const { return text; }
        constexpr BitView UppercaseView() const { return uppercase; }
    };

    template <typename Derived>
    constexpr LoweredText LoweredTextBase<Derived>::SubStr(size_t offset, size_t count)
    {
        return LoweredText{
            Text().substr(offset, count),
            Uppercase().Subview(offset, count)
        };
    }

    template <size_t N>
    struct LoweredTextOwned : LoweredTextBase<LoweredTextOwned<N>>
    {
        std::array<char, N> text;
        BitArray<N> uppercase;

        constexpr std::string_view TextView() const { return {text.data(), N}; }

        constexpr BitView UppercaseView() { return (BitView)uppercase; }

        constexpr operator LoweredText() { return LoweredText(TextView(), UppercaseView()); }

        constexpr explicit LoweredTextOwned(const char (&chars)[N])
        {
            for (size_t i = 0; i < N; ++i)
                text[i] = chars[i];
            LoweredTextBase<LoweredTextOwned<N>>::FoldText(std::span<char>(text.data(), N), (BitView)uppercase);
        }
    };

    struct LoweredTextVector
    {
        SpanVector<char> arena;
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
        LoweredTextVector(const SpanVector<char> &arena) : arena(arena)
        {
            LowercaseFold();
        }
        LoweredTextVector(SpanVector<char> &&arena) : arena(std::move(arena))
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
            enum struct Type : uint8_t
            {
                String,
                ExactNumber,
                AnyNumber,
            };

            enum struct SearchBound : uint8_t
            {
                // They must be in order of strength
                WithinXWords,
                Anywhere,
            };

            enum struct PostCheck : uint8_t
            {
                Null = 0,
                Distinct = 1 << 0,  // If the match must start at a boundary
                CaseEqual = 1 << 1, // Not used
                CaseSubset = 1 << 2,
                NumEqual = 1 << 3,
                NumLess = 1 << 4,
                NumGreater = 1 << 5,
                NumChecks = NumEqual | NumLess | NumGreater,
            };

            Type type;
            PostCheck post_check = PostCheck::Null;
            SearchBound search_bound;
            uint8_t within_count = 0;
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

        std::string_view src_str;
        std::vector<Atom> atoms;

    private:
        struct MatchRecord
        {
            uint16_t limit;
            uint16_t begin;
            uint16_t end;
        };

        LoweredTextVector strings;
        std::vector<MatchRecord> work_mem;
    };
}