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

    struct LoweredStringView;
    // CRTP base
    template <class Derived>
    struct LoweredStringBase
    {
        constexpr std::string_view Text() const
        {
            return static_cast<const Derived *>(this)->TextView();
        }

        constexpr BitView Uppercase() const
        {
            return static_cast<const Derived *>(this)->UppercaseView();
        }

        void ToReadableString(char *dst) const
        {
            auto src = Text();
            std::memcpy(dst, src.data(), src.size());
            for (auto index : Uppercase().IterSetBits())
            {
                dst[index] -= 32; // Make uppercase
            }
        }

        void GetReadableString(OutBuf<char> out) const
        {
            auto dst = out.AppendDstBuffer(Text().size());
            GetReadableString(dst);
        }

        constexpr LoweredStringView SubStr(size_t offset, size_t count);

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

    struct LoweredStringView : LoweredStringBase<LoweredStringView>
    {
        std::string_view text;
        BitView uppercase;

        LoweredStringView() = default;
        LoweredStringView(std::string_view text, BitView uppercase) : text(text), uppercase(uppercase) {}

        constexpr std::string_view TextView() const { return text; }
        constexpr BitView UppercaseView() const { return uppercase; }
    };

    template <typename Derived>
    constexpr LoweredStringView LoweredStringBase<Derived>::SubStr(size_t offset, size_t count)
    {
        return LoweredStringView{
            Text().substr(offset, count),
            Uppercase().Subview(offset, count)
        };
    }

    template <size_t N>
    struct LoweredString : LoweredStringBase<LoweredString<N>>
    {
        std::array<char, N> text;
        BitArray<N> uppercase;

        constexpr std::string_view TextView() const { return {text.data(), N}; }

        constexpr BitView UppercaseView() { return (BitView)uppercase; }

        constexpr operator LoweredStringView() { return LoweredStringView(TextView(), UppercaseView()); }

        constexpr explicit LoweredString(std::string_view chars)
        {
            for (size_t i = 0; i < N; ++i)
                text[i] = chars[i];
            LoweredStringBase<LoweredString<N>>::FoldText(std::span<char>(text.data(), N), (BitView)uppercase);
        }
    };

    template <size_t M>
    LoweredString(const char (&)[M])
        -> LoweredString<M - 1>;

    struct LoweredStringVector
    {
        SpanVector<char> arena;
        BitVector uppercase;

        LoweredStringVector() = default;
        template <std::ranges::input_range R>
            requires std::convertible_to<std::ranges::range_reference_t<R>, std::string_view>
        LoweredStringVector(R &&r)
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
        LoweredStringVector(const SpanVector<char> &arena) : arena(arena)
        {
            LowercaseFold();
        }
        LoweredStringVector(SpanVector<char> &&arena) : arena(std::move(arena))
        {
            LowercaseFold();
        }

        void clear()
        {
            arena.clear();
            uppercase.clear();
        }

        size_t CommitAndFold()
        {
            auto strId = arena.CommitWritten();
            auto committed = arena.Get(strId);
            auto size = arena.Elements().size();
            uppercase.resize(size, false);
            auto pending_uppercase = uppercase.Subview(committed.data() - arena.Elements().data(), committed.size());
            LoweredStringView::FoldText(committed, pending_uppercase);
            return strId;
        }

        LoweredStringView Get(size_t index)
        {
            auto str = arena.CGet(index);
            auto offset = str.data() - arena.Elements().data();
            return LoweredStringView{
                str,
                uppercase.Subview(offset, str.size())
            };
        }

        LoweredStringView operator[](size_t index)
        {
            return Get(index);
        }

        // Assumes arena contains text to be lowercase folded
        void LowercaseFold()
        {
            std::span<char> chars = arena.Elements();
            uppercase.clear();
            uppercase.resize(chars.size(), false);
            LoweredStringView::FoldText(chars, uppercase);
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
            LoweredStringView needle;

            std::string_view GetNeedleStr() const { return type == Type::String ? needle.text : std::string_view(enc_num.data(), enc_num.size()); }

            std::string ToString() const;
        };

        Matcher() = default;
        Matcher(std::string_view source);

        bool Match(LoweredStringView text, size_t offset);
        bool Match(LoweredStringView text, size_t &offset, std::vector<uint16_t> &matches);
        bool Matches(LoweredStringView text, std::vector<uint16_t> &matches);

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

        LoweredStringVector strings;
        std::vector<MatchRecord> work_mem;
    };
}