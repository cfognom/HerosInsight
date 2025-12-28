#pragma once

#include <bit>
#include <limits.h>

namespace HerosInsight
{
    struct BitView;

    template <typename Derived>
    class BitViewBase
    {
    public:
        using word_t = uint64_t;

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

        void SetAll(bool value)
        {
            if constexpr (std::is_same<Derived, BitView>::value)
            {
                auto data = uncompress();

                word_t val = value ? std::numeric_limits<word_t>::max() : 0;

                if (data.has_partial_head)
                    data.head.Set(value);

                std::memset(data.whole_words.data(), val, data.whole_words.size() * sizeof(word_t));

                if (data.has_partial_tail)
                    data.tail.Set(value);
            }
            else // For BitArray and BitVector we dont mind if we spill over, since we own all memory
            {
                init_bits(value);
            }
        }

        struct reference
        {
            void Set(bool value)
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

            bool Get() const
            {
                return GetMaskedWord() != 0;
            }

            reference &operator=(bool value)
            {
                this->Set(value);
                return *this;
            }

            reference &operator=(const reference &other)
            {
                this->Set(other.Get());
                return *this;
            }

            bool operator~() const
            {
                return !this->Get();
            }

            operator bool() const
            {
                return this->Get();
            }

        private:
            reference(word_t *word, word_t bit)
                : word(word), mask(bit) {}

            word_t GetMaskedWord() const { return *word & mask; }

            word_t *word;
            word_t mask;

            friend struct iterator;
            friend struct BitViewBase;
            friend struct BitVector;
        };

        reference operator[](size_t index)
        {
            assert(index < size());
            return ref_unchecked(index);
        }

        bool operator[](size_t index) const
        {
            auto pos = get_bit_pos(index);
            return (data()[pos.word_offset] & (word_t(1) << pos.bit_offset)) != word_t(0);
        }

        word_t *data() { return as_derived().data(); }
        const word_t *data() const { return as_derived().data(); }
        size_t WordCount() const { return CalcWordCount(size()); }
        std::span<word_t> Words() const { return std::span<word_t>(data(), CalcWordCount(size())); }
        size_t size() const { return as_derived().size(); }

        // We will impl this after the BitView class is defined
        BitView Subview(size_t offset, size_t count);

        size_t PopCount()
        {
            auto data = uncompress();
            size_t count = 0;
            if (data.has_partial_head)
                count += std::popcount(data.head.GetMaskedWord());
            for (auto word : data.whole_words)
                count += std::popcount(word);
            if (data.has_partial_tail)
                count += std::popcount(data.tail.GetMaskedWord());
            return count;
        }

        bool Any()
        {
            auto data = uncompress();
            if (data.has_partial_head)
                if (data.head.GetMaskedWord() != 0)
                    return true;
            for (auto word : data.whole_words)
                if (word != 0)
                    return true;
            if (data.has_partial_tail)
                if (data.tail.GetMaskedWord() != 0)
                    return true;
            return false;
        }

        // Finds the index of the next set bit from the specificed index.
        // Returns n_bits if there are no more set bits.
        size_t FindNextSetBit(size_t index) const
        {
            const auto physical_end = this->bit_offset() + this->size();
            const auto word_count = CalcWordCount(physical_end);
            auto physical_index = this->bit_offset() + index;
            auto word_index = physical_index / BitView::BITS_PER_WORD;
            auto bit_index = physical_index % BitView::BITS_PER_WORD;
            auto mask = std::numeric_limits<word_t>::max() << bit_index;
            auto word = this->data()[word_index] & mask;
            while (word == 0 && ++word_index < word_count)
            {
                word = this->data()[word_index];
            }
            auto trailing_zeros = std::countr_zero(word);
            physical_index = word_index * BitView::BITS_PER_WORD + trailing_zeros;
            index = std::min(physical_index, physical_end) - bit_offset();
            return index;
        }

        bool IsSubsetOf(const BitView &other) const
        {
            for (auto index : this->IterSetBits())
            {
                if (other[index] == false)
                    return false;
            }
            return true;
        }

        struct iterator
        {
            reference ref;
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
                *this;
            }

            reference operator*() const
            {
                return ref;
            }

            bool operator==(const iterator &other) const
            {
                return ref.word == other.ref.word &&
                       ref.mask == other.ref.mask;
            }
        };

        iterator begin() { return iterator{ref_unchecked(0)}; }
        iterator end() { return iterator{ref_unchecked(size())}; }

        struct IteratorAdapter
        {
            struct iterator
            {
                size_t operator*() const { return index; }

                iterator &operator++()
                {
                    index = bitview.FindNextSetBit(index + 1);
                    return *this;
                }

                bool operator==(const iterator &other) const
                {
                    return bitview.data() == other.bitview.data() &&
                           index == other.index;
                }

            private:
                iterator(const BitView &bitview, size_t index)
                    : bitview(bitview), index(index) {}

                const BitView &bitview;
                size_t index;

                friend struct IteratorAdapter;
            };

            iterator begin()
            {
                if (this->bitview.size() == 0)
                    return end();
                auto it = iterator(this->bitview, -1);
                ++it;
                return it;
            }
            iterator end()
            {
                return iterator(this->bitview, this->bitview.size());
            }

            const Derived &bitview;
        };

        IteratorAdapter IterSetBits() const { return IteratorAdapter{as_derived()}; }

    protected:
        Derived &as_derived() { return *static_cast<Derived *>(this); }
        const Derived &as_derived() const { return *static_cast<const Derived *>(this); }

        // Initializes all bits to value
        // WARNING: This will overwrite any head/tail bits not belonging to this bitview
        void init_bits(bool value)
        {
            std::memset(data(), value ? 0xFF : 0, CalcReqMemSize(size()));
        }

        struct BitPos
        {
            size_t word_offset;
            size_t bit_offset;
        };

        inline BitPos get_bit_pos(size_t index) const
        {
            size_t physical_index = bit_offset() + index;
            size_t word_offset = physical_index / BITS_PER_WORD;
            size_t bit_offset = physical_index % BITS_PER_WORD;
            return BitPos{word_offset, bit_offset};
        }

        reference ref_unchecked(size_t index)
        {
            auto pos = get_bit_pos(index);
            return reference(data() + pos.word_offset, word_t(1) << pos.bit_offset);
        }

        struct Properties
        {
            bool has_partial_head;
            bool has_partial_tail;
            reference head;
            reference tail;
            std::span<word_t> whole_words;
        };

        inline Properties uncompress()
        {
            const auto end = get_bit_pos(size());
            const auto start_bit_offset = bit_offset();
            word_t head_mask = std::numeric_limits<word_t>::max() << start_bit_offset;  // 1 = included in view.
            word_t tail_mask = ~(std::numeric_limits<word_t>::max() << end.bit_offset); // 1 = included in view. Not valid if end_offset == 0.

            bool has_partial_head = start_bit_offset > 0;
            bool has_partial_tail = end.bit_offset > 0;
            auto span_ptr = this->data() + has_partial_head;
            auto span_len = end.word_offset - has_partial_head;

            bool head_is_tail = end.word_offset == 0; // Head and tail are the same word.
            if (head_is_tail)
            {
                head_mask &= tail_mask;
                has_partial_head |= has_partial_tail;
                has_partial_tail = false;
                span_len = 0;
            }
            return Properties{
                .has_partial_head = has_partial_head,
                .has_partial_tail = has_partial_tail,
                .head = reference(this->data(), head_mask),
                .tail = reference(this->data() + end.word_offset, tail_mask),
                .whole_words = std::span<word_t>(span_ptr, span_len),
            };
        }

        size_t bit_offset() const { return as_derived().bit_offset(); }
    };

    class BitView : public BitViewBase<BitView>
    {
    public:
        // using base = typename BitViewBase<BitView>;
        // using word_t = typename base::word_t;

        BitView() : words(nullptr), _bit_offset(0), n_bits(0) {}

        BitView(word_t *words, size_t n_bits, bool init_val)
            : BitView(words, size_t(0), n_bits)
        {
            init_bits(init_val);
#ifdef _DEBUG
            assert(PopCount() == (init_val ? n_bits : 0));
#endif
        }

        BitView(word_t *words, size_t bit_offset, size_t n_bits)
            : words(words), _bit_offset(bit_offset), n_bits(n_bits)
        {
            assert(bit_offset <= MAX_BIT_OFFSET);
            assert(n_bits <= MAX_LENGTH);
        }

        word_t *data() { return words; }
        const word_t *data() const { return words; }
        size_t size() const { return n_bits; }
        size_t bit_offset() const { return _bit_offset; }

    private:
        word_t *words;
        size_t _bit_offset : BIT_OFFSET_WIDTH; // The bit-offset into the first word where the view starts
        size_t n_bits : LENGTH_WIDTH;
    };
    static_assert(sizeof(BitView) == sizeof(size_t) * 2);

    template <size_t N>
    class BitArray : public BitViewBase<BitArray<N>>
    {
        friend class BitViewBase<BitArray<N>>;

    public:
        using base = typename BitViewBase<BitArray<N>>;
        using word_t = typename base::word_t;

        operator BitView() { return BitView(words.data(), 0, N); }

        word_t *data() { return words.data(); }
        const word_t *data() const { return words.data(); }
        size_t size() const { return N; }

        constexpr BitArray() : BitArray(false) {}
        constexpr BitArray(bool inital_value) : words{inital_value ? std::numeric_limits<word_t>::max() : 0} {}

    private:
        size_t bit_offset() const { return 0; }

        std::array<word_t, base::CalcWordCount(N)> words;
    };

    class BitVector : public BitViewBase<BitVector>
    {
        friend class BitViewBase<BitVector>;

    public:
        // using base = typename BitViewBase<BitVector>;
        // using word_t = typename base::word_t;

        operator BitView() { return BitView(words.data(), 0, n_bits); }

        void clear() { words.clear(); }
        void resize(size_t n_bits, bool value = false)
        {
            if (n_bits > this->n_bits) // Grow
            {
                // When growing, we may need to grow the tail
                auto data = this->uncompress();
                if (data.has_partial_tail)
                {
                    // All bits that are beyond the new size are set in this mask
                    auto off_limits_mask = ~data.tail.mask << std::min(n_bits - this->n_bits, BitView::MAX_BIT_OFFSET);
                    auto mask = ~(data.tail.mask | off_limits_mask); // The newly added bits that we need to set
                    auto ref = reference(data.tail.word, mask);
                    ref.Set(value);
                }
            }
            this->words.resize(CalcWordCount(n_bits), value ? std::numeric_limits<word_t>::max() : word_t(0));
            this->n_bits = n_bits;
        }
        word_t *data() { return words.data(); }
        const word_t *data() const { return words.data(); }
        size_t size() const { return n_bits; }
        bool is_empty() const { return n_bits == 0; }

    private:
        size_t bit_offset() const { return 0; }

        std::vector<word_t> words;
        size_t n_bits = 0;
    };

#define BitView_alloca(n_bits, init_val) HerosInsight::BitView((word_t *)alloca(HerosInsight::BitView::CalcReqMemSize(n_bits)), n_bits, init_val)

    template <typename Derived>
    BitView BitViewBase<Derived>::Subview(size_t offset, size_t count)
    {
        assert(offset + count <= size());
        auto pos = this->get_bit_pos(offset);
        return BitView(this->data() + pos.word_offset, pos.bit_offset, count);
    }
}