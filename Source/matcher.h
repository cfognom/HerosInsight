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

    template <bool IsConst>
    struct LoweredStringViewBase;

    template <class Derived, bool IsConst>
    struct LoweredStringBase // CRTP base
    {
        // clang-format off
        constexpr std::string_view Text() const { return static_cast<const Derived *>(this)->TextView(); }
        constexpr ConstBitView Uppercase() const { return static_cast<const Derived *>(this)->UppercaseView(); }
        constexpr BitView Uppercase() requires(!IsConst) { return static_cast<const Derived *>(this)->UppercaseView(); }
        // clang-format on

        constexpr size_t size() const { return Text().size(); }
        constexpr bool empty() const { return Text().empty(); }

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
            ToReadableString(dst);
        }

        constexpr LoweredStringViewBase<true> SubStr(size_t offset, size_t count) const;
        constexpr LoweredStringViewBase<false> SubStr(size_t offset, size_t count)
            requires(!IsConst);

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

    template <bool IsConst>
    struct LoweredStringViewBase : LoweredStringBase<LoweredStringViewBase<IsConst>, IsConst>
    {
        std::string_view text;
        BitViewBase<IsConst> uppercase;

        LoweredStringViewBase() = default;
        constexpr LoweredStringViewBase(std::string_view text, BitViewBase<IsConst> uppercase) : text(text), uppercase(uppercase)
        {
#ifdef _DEBUG
            assert(text.size() == uppercase.size());
#endif
        }

        // clang-format off
        constexpr std::string_view TextView() const { return text; }
        constexpr ConstBitView UppercaseView() const { return uppercase; }
        constexpr BitView UppercaseView() requires(!IsConst) { return uppercase; }
        // clang-format on
    };
    using LoweredStringView = LoweredStringViewBase<false>;
    using ConstLoweredStringView = LoweredStringViewBase<true>;

    template <typename Derived, bool IsConst>
    constexpr ConstLoweredStringView LoweredStringBase<Derived, IsConst>::SubStr(size_t offset, size_t count) const
    {
        return ConstLoweredStringView{
            Text().substr(offset, count),
            Uppercase().Subview(offset, count)
        };
    }
    template <typename Derived, bool IsConst>
    constexpr LoweredStringView LoweredStringBase<Derived, IsConst>::SubStr(size_t offset, size_t count)
        requires(!IsConst)
    {
        return LoweredStringView{
            Text().substr(offset, count),
            Uppercase().Subview(offset, count)
        };
    }

    template <size_t N>
    struct LoweredString : LoweredStringBase<LoweredString<N>, false>
    {
        std::array<char, N> text;
        BitArray<N - 1> uppercase;

        constexpr std::string_view TextView() const { return {text.data(), N - 1}; }

        constexpr BitView UppercaseView() { return uppercase; }
        constexpr ConstBitView UppercaseView() const { return uppercase; }

        constexpr operator LoweredStringView() { return LoweredStringView(TextView(), UppercaseView()); }
        constexpr operator ConstLoweredStringView() const { return ConstLoweredStringView(TextView(), UppercaseView()); }

        constexpr LoweredString(const char (&str)[N])
        {
            std::copy(str, str + N, text.begin());
            this->FoldText(std::span<char>(text.data(), N - 1), (BitView)uppercase);
        }
    };

    template <LoweredString s>
    constexpr ConstLoweredStringView operator""_folded()
    {
        return (ConstLoweredStringView)s;
    }

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

        size_t size() const { return arena.SpanCount(); }
        void clear()
        {
            arena.clear();
            uppercase.clear();
        }

        void AppendPartial(ConstLoweredStringView str)
        {
            arena.AppendPartial(str.text);
            uppercase.append_range(str.uppercase);
        }

        size_t Commit()
        {
            auto id = arena.CommitWritten();
            uppercase.resize(arena.Elements().size(), false);
            return id;
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

    private:
        static auto get_internal(auto &self, size_t index)
        {
            constexpr bool IsConst = std::is_const_v<std::remove_reference_t<decltype(self)>>;
            auto str = self.arena.CGet(index);
            auto offset = str.data() - self.arena.Elements().data();
            return LoweredStringViewBase<IsConst>{
                str,
                (BitViewBase<IsConst>)self.uppercase.Subview(offset, str.size())
            };
        }

    public:
        // clang-format off
        LoweredStringView             Get(size_t index)       { return get_internal(*this, index); }
        ConstLoweredStringView       CGet(size_t index) const { return get_internal(*this, index); }
        LoweredStringView      operator[](size_t index)       { return  Get(index); }
        ConstLoweredStringView operator[](size_t index) const { return CGet(index); }
        // clang-format on

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

        bool Match(ConstLoweredStringView text, size_t offset);
        bool Match(ConstLoweredStringView text, size_t &offset, std::vector<uint16_t> &matches);
        bool Matches(ConstLoweredStringView text, std::vector<uint16_t> &matches);

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