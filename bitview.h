#pragma once

#include <bit>
#include <limits.h>

namespace HerosInsight
{
    struct Bitview
    {
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

#define Bitview_alloca(n_bits, init_val) HerosInsight::Bitview((word_t *)alloca(HerosInsight::Bitview::CalcReqMemSize(n_bits)), n_bits, init_val)

        Bitview() : words(nullptr), bit_offset(0), n_bits(0) {}

        Bitview(word_t *words, size_t n_bits, bool init_val)
            : Bitview(words, size_t(0), n_bits)
        {
            init_bits(init_val);
#ifdef _DEBUG
            assert(PopCount() == (init_val ? n_bits : 0));
#endif
        }

        void SetAll(bool value)
        {
            auto data = uncompress();

            word_t val = value ? std::numeric_limits<word_t>::max() : 0;

            if (data.has_partial_head)
                data.head.Set(value);

            std::memset(data.whole_words.data(), val, data.whole_words.size() * sizeof(word_t));

            if (data.has_partial_tail)
                data.tail.Set(value);
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
            friend struct Bitview;
        };

        reference operator[](size_t index)
        {
            assert(index < n_bits);
            return ref_unchecked(index);
        }
        bool operator[](size_t index) const
        {
            auto pos = get_bit_pos(index);
            return (words[pos.word_offset] & (word_t(1) << pos.bit_offset)) != word_t(0);
        }

        word_t *data() const { return words; }
        size_t WordCount() const { return CalcWordCount(n_bits); }
        std::span<word_t> Words() const { return std::span<word_t>(words, CalcWordCount(n_bits)); }
        size_t size() const { return n_bits; }

        Bitview Subview(size_t offset, size_t count) const
        {
            assert(offset + count <= n_bits);
            auto physical_offset = this->bit_offset + offset;
            auto word_offset = physical_offset / BITS_PER_WORD;
            auto bit_offset = physical_offset % BITS_PER_WORD;
            return Bitview(words + word_offset, bit_offset, count);
        }

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

        // Finds the index of the next set bit from the specificed index.
        // Returns n_bits if there are no more set bits.
        size_t FindNextSetBit(size_t index) const
        {
            const auto physical_end = this->bit_offset + this->n_bits;
            const auto word_count = CalcWordCount(physical_end);
            auto physical_index = this->bit_offset + index;
            auto word_index = physical_index / Bitview::BITS_PER_WORD;
            auto bit_index = physical_index % Bitview::BITS_PER_WORD;
            auto mask = std::numeric_limits<word_t>::max() << bit_index;
            auto word = this->words[word_index] & mask;
            while (word == 0 && ++word_index < word_count)
            {
                word = this->words[word_index];
            }
            auto trailing_zeros = std::countr_zero(word);
            physical_index = word_index * Bitview::BITS_PER_WORD + trailing_zeros;
            index = std::min(physical_index, physical_end) - bit_offset;
            return index;
        }

        bool IsSubsetOf(const Bitview &other) const
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
        iterator end() { return iterator{ref_unchecked(n_bits)}; }

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
                iterator(const Bitview &bitview, size_t index)
                    : bitview(bitview), index(index) {}

                const Bitview &bitview;
                size_t index;

                friend struct IteratorAdapter;
            };

            iterator begin()
            {
                if (this->bitset.size() == 0)
                    return end();
                auto it = iterator(this->bitset, -1);
                ++it;
                return it;
            }
            iterator end()
            {
                return iterator(this->bitset, this->bitset.size());
            }

            const Bitview &bitset;
        };

        IteratorAdapter IterSetBits() const { return IteratorAdapter{*this}; }

    private:
        Bitview(word_t *words, size_t bit_offset, size_t n_bits)
            : words(words), bit_offset(bit_offset), n_bits(n_bits)
        {
            assert(bit_offset <= MAX_BIT_OFFSET);
            assert(n_bits <= MAX_LENGTH);
        }

        // Initializes all bits to value
        // WARNING: This will overwrite any head/tail bits not belonging to this bitview
        void init_bits(bool value)
        {
            std::memset(words, value ? 0xFF : 0, CalcReqMemSize(n_bits));
        }

        struct BitPosition
        {
            size_t word_offset;
            size_t bit_offset;
        };

        inline BitPosition get_bit_pos(size_t index) const
        {
            size_t physical_index = bit_offset + index;
            size_t word_offset = physical_index / BITS_PER_WORD;
            size_t bit_offset = physical_index % BITS_PER_WORD;
            return BitPosition{word_offset, bit_offset};
        }

        reference ref_unchecked(size_t index)
        {
            auto pos = get_bit_pos(index);
            return reference(words + pos.word_offset, word_t(1) << pos.bit_offset);
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
            size_t physical_end = bit_offset + n_bits;
            bool head_is_tail = physical_end < BITS_PER_WORD; // Head and tail are the same word.
            size_t end_word_offset = physical_end / BITS_PER_WORD;
            size_t end_bit_offset = physical_end % BITS_PER_WORD;
            word_t head_mask = std::numeric_limits<word_t>::max() << bit_offset;        // 1 = included in view.
            word_t tail_mask = ~(std::numeric_limits<word_t>::max() << end_bit_offset); // 1 = included in view. Not valid if end_offset == 0.

            bool has_head_bits = bit_offset > 0;
            bool has_tail_bits = end_bit_offset > 0;
            auto span_ptr = this->words + has_head_bits;
            auto span_len = end_word_offset - has_head_bits;
            if (head_is_tail)
            {
                head_mask &= tail_mask;
                has_head_bits |= has_tail_bits;
                has_tail_bits = false;
                span_len = 0;
            }
            return Properties{
                .has_partial_head = has_head_bits,
                .has_partial_tail = has_tail_bits,
                .head = reference(this->words, head_mask),
                .tail = reference(this->words + end_word_offset, tail_mask),
                .whole_words = std::span<word_t>(span_ptr, span_len),
            };
        }

        word_t *const words;
        const size_t bit_offset : BIT_OFFSET_WIDTH; // The bit-offset into the first word where the view starts
        const size_t n_bits : LENGTH_WIDTH;
    };
    static_assert(sizeof(Bitview) == sizeof(size_t) * 2);
}