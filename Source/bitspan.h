#pragma once

#include <bit>
#include <cstring>
#include <limits.h>
#include <type_traits>

template <bool IsConst>
class BitSpanBase;

template <typename Derived, bool IsConst>
class BitOperations // Base class for CRTP (Curiously Recurring Template Pattern)
{
public:
    using word_t = uint64_t;
    using NaturalBitSpan = BitSpanBase<IsConst>;
    using Self = BitOperations<Derived, IsConst>;

    static constexpr size_t BITS_PER_WORD = sizeof(word_t) * CHAR_BIT;
    static constexpr size_t MAX_BIT_OFFSET = BITS_PER_WORD - 1;
    static constexpr size_t BIT_OFFSET_WIDTH = std::bit_width(MAX_BIT_OFFSET);
    static constexpr size_t LENGTH_WIDTH = sizeof(size_t) * CHAR_BIT - BIT_OFFSET_WIDTH;
    static constexpr size_t MAX_LENGTH = (size_t(1) << LENGTH_WIDTH) - 1;
    static constexpr size_t BIT_MASK = MAX_BIT_OFFSET; // Region of index representing the bit offset
    static constexpr size_t WORD_MASK = ~BIT_MASK;     // Region of index representing the word offset

    // Calculates how many words are required to store n_bits
    FORCE_INLINE static constexpr size_t CalcWordCount(size_t n_bits)
    {
        return (n_bits + BITS_PER_WORD - 1) / BITS_PER_WORD;
    }

    // Calculates how much to allocate to store n_bits
    FORCE_INLINE static constexpr size_t CalcReqMemSize(size_t n_bits)
    {
        return CalcWordCount(n_bits) * sizeof(word_t);
    }

    constexpr void SetAll(bool value)
        requires(!IsConst)
    {
        if constexpr (std::is_same_v<Derived, NaturalBitSpan>)
        {
            auto data = get_segments();

            word_t val = value ? std::numeric_limits<word_t>::max() : 0;

            if (data.head.has_value())
                data.head->Set(value);

            std::memset(data.whole_words.data(), val, data.whole_words.size() * sizeof(word_t));

            if (data.tail.has_value())
                data.tail->Set(value);
        }
        else // For BitArray and BitVector we dont mind if we spill over, since we own all memory
        {
            init_bits(value);
        }
    }

    constexpr void Flip()
        requires(!IsConst)
    {
        auto data = get_segments();
        if (data.head.has_value())
            data.head->SetMaskedWord(~*data.head->word);
        for (auto &word : data.middle)
            word = ~word;
        if (data.tail.has_value())
            data.tail.SetMaskedWord(~*data.tail->word);
    }

    template <bool IsConst>
    struct ReferenceBase
    {
        using consty_word_t = std::conditional_t<IsConst, const word_t, word_t>;

        constexpr void Set(bool value)
            requires(!IsConst)
        {
            if (value)
            {
                *word |= mask;
            }
            else
            {
                *word &= ~mask;
            }
        }

        constexpr bool Get() const
        {
            return GetMaskedWord() != 0;
        }

        // clang-format off
        constexpr ReferenceBase& operator=(bool value)                 requires(!IsConst) { this->Set(value);       return *this; }
        constexpr ReferenceBase& operator=(const ReferenceBase &other) requires(!IsConst) { this->Set(other.Get()); return *this; }
        constexpr bool operator~() const { return !this->Get(); }
        constexpr operator  bool() const { return  this->Get(); }
        // clang-format on

    private:
        constexpr ReferenceBase(consty_word_t *word, word_t bit) : word(word), mask(bit) {}

        constexpr word_t GetMaskedWord() const { return *word & mask; }
        constexpr void SetMaskedWord(word_t value)
            requires(!IsConst)
        {
            *word = (*word & ~mask) | (value & mask);
        }

        consty_word_t *word;
        word_t mask;

        friend struct iterator;
        friend struct BitOperations;
        friend struct BitVector;
    };
    using Reference = ReferenceBase<false>;
    using ConstReference = ReferenceBase<true>;

    constexpr Reference operator[](size_t index)
        requires(!IsConst)
    {
        assert(index < size());
        return ref_unchecked(index);
    }

    constexpr bool operator[](size_t index) const
    {
        auto pos = get_bit_pos(index);
        return (data()[pos.word_offset] & (word_t(1) << pos.bit_offset)) != word_t(0);
    }

    template <typename T, bool OtherIsConst>
    constexpr Derived &operator|=(const BitOperations<T, OtherIsConst> &other)
        requires(!IsConst)
    {
        if (this->bit_offset() == other.bit_offset())
        {
            auto data = get_segments();
            auto other_data = other.get_segments();
            auto min_size = std::min(data.middle.size(), other_data.middle.size());
            if (data.head.has_value())
                data.head->SetMaskedWord(*data.head->word | other_data.head->GetMaskedWord());
            for (size_t i = 0; i < min_size; ++i)
                data.middle[i] |= other_data.middle[i];
            if (data.tail.has_value())
                data.tail->SetMaskedWord(*data.tail->word | other_data.tail->GetMaskedWord());
            return as_derived();
        }

        throw std::runtime_error("Not implemented");
    }

    // clang-format off
    constexpr       word_t* data() requires(!IsConst) { return as_derived().data(); }
    constexpr const word_t* data() const              { return as_derived().data(); }
    constexpr       size_t  size() const              { return as_derived().size(); }
    constexpr BitSpanBase<false> Subview(size_t offset, size_t count) requires(!IsConst);
    constexpr BitSpanBase<true>  Subview(size_t offset, size_t count) const;
    // clang-format on

    constexpr size_t PopCount() const
    {
        auto data = get_segments();
        size_t count = 0;
        if (data.head.has_value())
            count += std::popcount(data.head->GetMaskedWord());
        for (auto word : data.middle)
            count += std::popcount(word);
        if (data.tail.has_value())
            count += std::popcount(data.tail->GetMaskedWord());
        return count;
    }

    constexpr bool Any() const
    {
        auto data = get_segments();
        if (data.head.has_value() && data.head->GetMaskedWord() != 0)
            return true;
        for (auto word : data.middle)
            if (word != 0)
                return true;
        if (data.tail.has_value() && data.tail->GetMaskedWord() != 0)
            return true;
        return false;
    }

    constexpr bool All() const
    {
        auto data = get_segments();
        if (data.head.has_value() && data.head->GetMaskedWord() != data.head->mask)
            return false;
        for (auto word : data.middle)
            if (word != std::numeric_limits<word_t>::max())
                return false;
        if (data.tail.has_value() && data.tail->GetMaskedWord() != data.tail->mask)
            return false;
        return true;
    }

    // Finds the index of the next set bit from the specificed index.
    // Returns n_bits if there are no more set bits.
    size_t FindNextSetBit(size_t index) const
    {
        const auto physical_end = this->bit_offset() + this->size();
        const auto word_count = CalcWordCount(physical_end);
        auto physical_index = this->bit_offset() + index;
        auto word_index = physical_index / BITS_PER_WORD;
        auto bit_index = physical_index % BITS_PER_WORD;
        auto mask = std::numeric_limits<word_t>::max() << bit_index;
        auto word = this->data()[word_index] & mask;
        while (word == 0 && ++word_index < word_count)
        {
            word = this->data()[word_index];
        }
        auto trailing_zeros = std::countr_zero(word);
        physical_index = word_index * BITS_PER_WORD + trailing_zeros;
        index = std::min(physical_index, physical_end) - bit_offset();
        return index;
    }

    template <typename T, bool OtherIsConst>
    bool IsSubsetOf(const BitOperations<T, OtherIsConst> &other) const
    {
        for (auto index : this->IterSetBits())
        {
            if (!other[index])
                return false;
        }
        return true;
    }

    struct iterator
    {
        Reference ref;
        iterator &operator++()
        {
            if (ref.mask == 0)
            {
                ++ref.word;
                ref.mask = 1;
            }
            else
            {
                ref.mask <<= 1;
            }
            return *this;
        }

        Reference operator*() const
        {
            return ref;
        }

        bool operator==(const iterator &other) const
        {
            return ref.word == other.ref.word &&
                   ref.mask == other.ref.mask;
        }
    };

    // clang-format off
    iterator begin() requires(!IsConst) { return iterator{ref_unchecked(0)}; }
    iterator end()   requires(!IsConst) { return iterator{ref_unchecked(size())}; }
    // clang-format on

    struct SetBitsIterable
    {
        struct iterator
        {
            size_t operator*() const { return index; }

            iterator &operator++()
            {
                index = bit_obj.FindNextSetBit(index + 1);
                return *this;
            }

            bool operator==(const iterator &other) const
            {
                return bit_obj.data() == other.bit_obj.data() &&
                       index == other.index;
            }

        private:
            iterator(const Self &bit_obj, size_t index)
                : bit_obj(bit_obj), index(index) {}

            const Self &bit_obj;
            size_t index;

            friend struct SetBitsIterable;
        };

        iterator begin()
        {
            if (this->bit_obj.size() == 0)
                return end();
            auto it = iterator(this->bit_obj, size_t(-1));
            ++it;
            return it;
        }
        iterator end()
        {
            return iterator(this->bit_obj, this->bit_obj.size());
        }

        const Self &bit_obj;
    };

    SetBitsIterable IterSetBits() const { return SetBitsIterable{*this}; }

protected:
    constexpr Derived &as_derived() { return *static_cast<Derived *>(this); }
    constexpr const Derived &as_derived() const { return *static_cast<const Derived *>(this); }

    // Initializes all bits to value
    // WARNING: This will overwrite any head/tail bits not belonging to this bitview
    constexpr void init_bits(bool value)
        requires(!IsConst)
    {
        std::memset(data(), value ? 0xFF : 0, CalcReqMemSize(size()));
    }

    struct BitPos
    {
        size_t word_offset;
        size_t bit_offset;
    };

    constexpr inline BitPos get_bit_pos(size_t index) const
    {
        size_t physical_index = bit_offset() + index;
        size_t word_offset = physical_index / BITS_PER_WORD;
        size_t bit_offset = physical_index % BITS_PER_WORD;
        return BitPos{word_offset, bit_offset};
    }

    constexpr Reference ref_unchecked(size_t index)
        requires(!IsConst)
    {
        auto pos = get_bit_pos(index);
        return Reference(this->data() + pos.word_offset, word_t(1) << pos.bit_offset);
    }

public: // These should be protected but c++ is being a bitch
    template <bool IsConst>
    struct SegmentsBase
    {
        using consty_word_t = std::conditional_t<IsConst, const word_t, word_t>;

        std::optional<ReferenceBase<IsConst>> head;
        std::optional<ReferenceBase<IsConst>> tail;
        std::span<consty_word_t> middle;
    };
    using Segments = SegmentsBase<false>;
    using ConstSegments = SegmentsBase<true>;

    constexpr inline auto get_segments(this auto &self)
    {
        constexpr bool IsConst = std::is_const_v<std::remove_reference_t<decltype(self)>>;

        const auto end = self.get_bit_pos(self.size());
        const auto start_bit_offset = self.bit_offset();
        word_t head_mask = std::numeric_limits<word_t>::max() << start_bit_offset;  // 1 = included in view.
        word_t tail_mask = ~(std::numeric_limits<word_t>::max() << end.bit_offset); // 1 = included in view. Not valid if end_offset == 0.

        auto data = self.data();
        bool has_partial_head = start_bit_offset > 0;
        bool has_partial_tail = end.bit_offset > 0;
        auto span_ptr = data + has_partial_head;
        auto span_len = end.word_offset - has_partial_head;

        bool head_is_tail = end.word_offset == 0; // Head and tail are the same word.
        if (head_is_tail)
        {
            head_mask &= tail_mask;
            has_partial_head |= has_partial_tail;
            has_partial_tail = false;
            span_len = 0;
        }
        return SegmentsBase<IsConst>{
            .head = has_partial_head ? std::optional{ReferenceBase<IsConst>(data, head_mask)} : std::nullopt,
            .tail = has_partial_tail ? std::optional{ReferenceBase<IsConst>(data + end.word_offset, tail_mask)} : std::nullopt,
            .middle = std::span{span_ptr, span_len},
        };
    }

    constexpr size_t bit_offset() const { return as_derived().bit_offset(); }
};

// Template BitSpan implementation. IsConst toggles constness of the underlying data pointer
template <bool IsConst>
class BitSpanBase : public BitOperations<BitSpanBase<IsConst>, IsConst>
{
public:
    using base = BitOperations<BitSpanBase<IsConst>, IsConst>;
    using word_t = typename base::word_t;
    using consty_word_t = std::conditional_t<IsConst, const word_t, word_t>;

    BitSpanBase() : words(nullptr), _bit_offset(0), n_bits(0) {}

    constexpr BitSpanBase(consty_word_t *words, size_t n_bits, bool init_val)
        requires(!IsConst)
        : BitSpanBase(words, size_t(0), n_bits)
    {
        this->init_bits(init_val);
#ifdef _DEBUG
        assert(this->PopCount() == (init_val ? n_bits : 0));
#endif
    }

    constexpr BitSpanBase(consty_word_t *words, size_t bit_offset, size_t n_bits)
        : words(words), _bit_offset(bit_offset), n_bits(n_bits)
    {
        assert(bit_offset <= base::MAX_BIT_OFFSET);
        assert(n_bits <= base::MAX_LENGTH);
    }

    constexpr BitSpanBase(const BitSpanBase &) = default;

    // Allow implicit conversion from mutable view to const view
    constexpr BitSpanBase(const BitSpanBase<false> &other)
        requires(IsConst)
        : words(other.data()), _bit_offset(other.bit_offset()), n_bits(other.size())
    {
    }

    // clang-format off
    constexpr       word_t *data() requires(!IsConst) { return words; }
    constexpr const word_t *data() const              { return words; }
    constexpr size_t size()       const { return n_bits; }
    constexpr size_t bit_offset() const { return _bit_offset; }
    // clang-format on

private:
    consty_word_t *words;
    size_t _bit_offset : base::BIT_OFFSET_WIDTH; // The bit-offset into the first word where the view starts
    size_t n_bits : base::LENGTH_WIDTH;
};

using BitSpan = BitSpanBase<false>;
using ConstBitSpan = BitSpanBase<true>;
static_assert(sizeof(BitSpan) == sizeof(size_t) * 2);

template <size_t N>
class BitArray : public BitOperations<BitArray<N>, false>
{
    friend class BitOperations<BitArray<N>, false>;

public:
    using base = typename BitOperations<BitArray<N>, false>;
    using word_t = typename base::word_t;

    constexpr operator BitSpan() { return BitSpan(words.data(), 0, N); }
    constexpr operator ConstBitSpan() const { return ConstBitSpan(words.data(), 0, N); }

    constexpr word_t *data() { return words.data(); }
    constexpr const word_t *data() const { return words.data(); }
    constexpr size_t size() const { return N; }

    constexpr BitArray() : words{} {} // Sets all bits to 0
    constexpr BitArray(bool inital_value)
    {
        words.fill(inital_value ? std::numeric_limits<word_t>::max() : 0);
    }

    std::array<word_t, base::CalcWordCount(N)> words; // Cannot be private because this type must be "structural".

private:
    constexpr size_t bit_offset() const { return 0; }
};

template <size_t N, bool BaseValue, size_t... FlipIndices>
static inline constexpr ConstBitSpan BitLit()
{
    static constexpr BitArray<N> bits = []
    {
        BitArray<N> result(BaseValue);
        ((result[FlipIndices] = !BaseValue), ...);
        return result;
    }();
    return bits;
}

class BitVector : public BitOperations<BitVector, false>
{
    friend class BitOperations<BitVector, false>;

public:
    // using base = typename BitSpanBase<BitVector>;
    // using word_t = typename base::word_t;

    BitVector() = default;
    BitVector(size_t n_bits, bool value) : words(CalcWordCount(n_bits), value ? std::numeric_limits<word_t>::max() : 0), n_bits(n_bits) {};

    operator BitSpan() { return BitSpan(words.data(), 0, n_bits); }

    void clear() { words.clear(); }
    void resize(size_t n_bits, bool value = false)
    {
        if (n_bits > this->n_bits) // Grow
        {
            // When growing, we may need to grow the tail
            auto data = this->get_segments();
            if (data.tail.has_value())
            {
                // All bits that are beyond the new size are set in this mask
                auto off_limits_mask = ~data.tail->mask << std::min(n_bits - this->n_bits, BitSpan::MAX_BIT_OFFSET);
                auto mask = ~(data.tail->mask | off_limits_mask); // The newly added bits that we need to set
                auto ref = Reference{data.tail->word, mask};
                ref.Set(value);
            }
        }
        this->words.resize(CalcWordCount(n_bits), value ? std::numeric_limits<word_t>::max() : word_t(0));
        this->n_bits = n_bits;
    }
    void append_range(const ConstBitSpan &range)
    {
        // TODO: Maybe do this with bit shifting instead?
        size_t original_size = this->n_bits;
        this->resize(original_size + range.size(), false);
        for (auto index : range.IterSetBits())
            this->ref_unchecked(original_size + index).Set(true);
    }
    constexpr word_t *data() { return words.data(); }
    constexpr const word_t *data() const { return words.data(); }
    constexpr size_t size() const { return n_bits; }
    constexpr bool is_empty() const { return n_bits == 0; }

private:
    constexpr size_t bit_offset() const { return 0; }

    std::vector<word_t> words;
    size_t n_bits = 0;
};

#define BitSpan_alloca(n_bits, init_val) HerosInsight::BitSpan((word_t *)alloca(HerosInsight::BitSpan::CalcReqMemSize(n_bits)), n_bits, init_val)

template <typename Derived, bool IsConst>
constexpr BitSpan BitOperations<Derived, IsConst>::Subview(size_t offset, size_t count)
    requires(!IsConst)
{
    assert(offset + count <= size());
    auto pos = this->get_bit_pos(offset);
    return BitSpan{this->data() + pos.word_offset, pos.bit_offset, count};
}
template <typename Derived, bool IsConst>
constexpr ConstBitSpan BitOperations<Derived, IsConst>::Subview(size_t offset, size_t count) const
{
    assert(offset + count <= size());
    auto pos = this->get_bit_pos(offset);
    return ConstBitSpan{this->data() + pos.word_offset, pos.bit_offset, count};
}
