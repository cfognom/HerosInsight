#pragma once

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>

#include <capacity_hints.h>
#include <utils.h>

#ifdef _DEBUG
#define SAFETY_CHECKS
#endif

namespace HerosInsight
{
    template <typename T>
    class SpanVector
    {
        // We use all bytes of T for eq and hashing, so T cannot have any padding etc...
        static_assert(std::has_unique_object_representations_v<T>, "T must have unique object representations");

    protected:
        constexpr static bool is_char = std::is_same_v<T, char>;
        constexpr static bool is_wchar = std::is_same_v<T, wchar_t>;
        using T_const_view = std::conditional_t<is_char, std::string_view, std::conditional_t<is_wchar, std::wstring_view, std::span<const T>>>;

    public:
        using T_ends = uint32_t;
        using T_span_id = uint32_t;

    protected:
        static std::string_view AsStringView(std::span<T> span)
        {
            return std::string_view((const char *)span.data(), span.size() * sizeof(T));
        }

        std::vector<T> elements;
        std::vector<T_ends> ends;
        T_ends GetSpanStart(size_t index) const
        {
            if (index == 0)
                return 0;
            auto start = ends[index - 1];
            if constexpr (is_char || is_wchar)
            {
                start += 1;
            }
            return start;
        }

    public:
        struct Deduper
        {
            struct Hasher
            {
                size_t operator()(T_span_id index) const
                {
                    return std::hash<std::string_view>{}(AsStringView(arena->Get(index)));
                }

                SpanVector *arena;
            };

            struct Eq
            {
                bool operator()(T_span_id a, T_span_id b) const
                {
                    return AsStringView(arena->Get(a)) == AsStringView(arena->Get(b));
                }

                SpanVector *arena;
            };

            std::unordered_set<T_span_id, Hasher, Eq> uniques;
            SpanVector *arena = nullptr;

            void clear()
            {
                uniques.clear();
            }

            Deduper() = default;
            Deduper(SpanVector *arena, size_t n_buckets)
                : uniques(n_buckets, Hasher{arena}, Eq{arena}), arena(arena) {}
        };

        // The count of strings in the arena
        size_t SpanCount() const { return ends.size(); }
        std::vector<T> &Elements() { return elements; }
        T_const_view Elements() const { return elements; }

        void Reserve(size_t n_spans, size_t n_elements)
        {
            ends.reserve(n_spans);
            elements.reserve(n_elements);
        }

        void ReserveFromHint(std::string_view id)
        {
            auto n_spans = CapacityHints::GetHint(std::format("{}_spans", id));
            auto n_elements = CapacityHints::GetHint(std::format("{}_elements", id));
            Reserve(n_spans, n_elements);
        }

        void StoreCapacityHint(std::string_view &id)
        {
            CapacityHints::UpdateHint(std::format("{}_spans", id), ends.size());
            CapacityHints::UpdateHint(std::format("{}_elements", id), elements.size());
        }

        float CalcAvgSpanSize() const { return ends.empty() ? 0.f : (float)elements.size() / (float)ends.size(); }

        void clear()
        {
            elements.clear();
            ends.clear();
        }

        void Prune(size_t n_spans)
        {
            assert(n_spans <= SpanCount());
            elements.erase(elements.begin() + GetSpanStart(n_spans), elements.end());
            ends.erase(ends.begin() + n_spans, ends.end());
        }

        // Deduper is valid until the SpanVector is moved!
        Deduper CreateDeduper(size_t n_buckets)
        {
            return Deduper(this, n_buckets);
        }

        // Discards the span being written
        void DiscardWritten()
        {
            auto new_end = GetSpanStart(ends.size());
            elements.erase(elements.begin() + new_end, elements.end());
        }

        void PopBack()
        {
            ends.pop_back();
            DiscardWritten();
        }

        // Returns the size of the span being written
        size_t GetPendingSize() const
        {
            auto start = GetSpanStart(ends.size());
            return elements.size() - start;
        }

        std::span<T> GetPendingSpan()
        {
            auto start = GetSpanStart(ends.size());
            return std::span<T>(elements.data() + start, elements.size() - start);
        }

        // Commits the span being written and returns its id
        T_span_id CommitWritten(Deduper *deduper = nullptr)
        {
            auto span_id = ends.size();
            auto end = elements.size();
#ifdef SAFETY_CHECKS
            assert(span_id <= std::numeric_limits<T_span_id>::max());
            assert(end <= std::numeric_limits<T_ends>::max());
#endif
            T_span_id span_id_cast = static_cast<T_span_id>(span_id);
            T_ends ends_cast = static_cast<T_ends>(end);

            ends.push_back(ends_cast);

            if (deduper != nullptr)
            {
#ifdef SAFETY_CHECKS
                assert(deduper->arena == this);
#endif
                auto it = deduper->uniques.find(span_id_cast);
                if (it == deduper->uniques.end())
                {
                    // We found no previous occurrence of this string:
                    deduper->uniques.insert(span_id_cast); // Track it
                }
                else
                {
                    // We found a previous occurrence of this string:
                    PopBack();                   // Discard the duplicate
                    auto span_id_existing = *it; // Reuse the old one
                    return span_id_existing;
                }
            }

            if constexpr (is_char || is_wchar)
            {
                elements.push_back('\0');
            }

            return span_id_cast;
        }

        // Gets a reference to a string in the arena
        std::span<T> Get(size_t span_id)
        {
            // if (span_id >= ends.size())
            //     return std::span<T>();

            auto end = ends[span_id];
            auto start = GetSpanStart(span_id);
            auto len = end - start;
            std::span<T> span(elements.data() + start, len);
            return span;
        }
        // Gets a reference to a string in the arena
        T_const_view CGet(size_t span_id) const
        {
            // if (span_id >= ends.size())
            //     return std::span<const T>();

            auto end = ends[span_id];
            auto start = GetSpanStart(span_id);
            auto len = end - start;
            T_const_view span(elements.data() + start, len);
            return span;
        }

        std::span<T> operator[](size_t span_id) { return Get(span_id); }
        T_const_view operator[](size_t span_id) const { return CGet(span_id); }

        std::span<T> back()
        {
            assert(!ends.empty());
            return Get(ends.size() - 1);
        }

        T_span_id push_back(std::span<const T> span, Deduper *deduper = nullptr)
        {
            elements.append_range(span);
            return CommitWritten(deduper);
        }

        // Writer should modify the span size to the number of elements written
        template <class Writer>
            requires std::invocable<Writer, std::span<T> &>
        void AppendWriteBuffer(size_t buf_size, Writer &&writer)
        {
            const size_t size = elements.size();
            elements.resize(size + buf_size);
            auto buffer_span = std::span<T>(elements.data() + size, buf_size);
            writer(buffer_span);
            elements.resize(size + buffer_span.size());
        }

        struct iterator
        {
            using iterator_category = std::random_access_iterator_tag;
            using difference_type = std::size_t;
            using value_type = std::span<T>;
            using reference = value_type; // spans act like views
            using pointer = void;         // not meaningful for spans

            value_type operator*() const { return arena->Get(index); }

            iterator &operator++()
            {
                ++index;
                return *this;
            }
            iterator &operator+=(difference_type offset)
            {
                index += offset;
                return *this;
            }
            difference_type operator-(const iterator &other) const { return index - other.index; }

            bool operator==(const iterator &other) const
            {
                return arena == other.arena && index == other.index;
            }

            size_t Index() const { return index; }

        private:
            friend class SpanVector<T>;
            SpanVector *arena;
            size_t index;
            iterator(SpanVector &arena, size_t index) : arena(&arena), index(index) {}
        };

        struct const_iterator
        {
            using iterator_category = std::random_access_iterator_tag;
            ;
            using difference_type = std::size_t;
            using value_type = std::span<const T>;
            using reference = value_type;
            using pointer = void;

            value_type operator*() const { return arena->Get(index); }

            const_iterator &operator++()
            {
                ++index;
                return *this;
            }
            const_iterator &operator+=(difference_type offset)
            {
                index += offset;
                return *this;
            }
            difference_type operator-(const const_iterator &other) const { return index - other.index; }

            bool operator==(const const_iterator &other) const
            {
                return arena == other.arena && index == other.index;
            }

            size_t Index() const { return index; }

        private:
            friend class SpanVector<T>;
            const SpanVector *arena;
            size_t index;
            const_iterator(const SpanVector &arena, size_t index) : arena(&arena), index(index) {}
        };
        iterator begin() { return iterator(*this, 0); }
        iterator end() { return iterator(*this, ends.size()); }
        const_iterator begin() const { return const_iterator(*this, 0); }
        const_iterator end() const { return const_iterator(*this, ends.size()); }
        const_iterator cbegin() const { return begin(); }
        const_iterator cend() const { return end(); }
    };

    // A datastructure that can associate ids with a set of unique spans of data.
    // Auto-dedupes equal entries.
    template <typename T>
    class IndexedStringArena : public SpanVector<T>
    {
        using base = SpanVector<T>;

    public:
        using T_span_id = uint16_t;
        constexpr static T_span_id NULL_SPAN_ID = std::numeric_limits<T_span_id>::max();

        std::vector<T_span_id> index_to_id;

    private:
        base::Deduper deduper = base::CreateDeduper(0);

    public:
        using value_type = T;

        IndexedStringArena() {}
        IndexedStringArena(size_t n_indices, size_t n_spans, size_t n_elements)
        {
            this->Reserve(n_indices, n_spans, n_elements);

#ifdef _DEBUG
            GetDebugInfo(); // To prevent the function from being optimized out
#endif
        }

        IndexedStringArena(const IndexedStringArena &other)
            : base(other)
        {
            index_to_id = other.index_to_id;
            // Deep copy the deduper
            deduper.uniques.reserve(other.deduper.uniques.size());
            deduper.uniques.insert(other.deduper.uniques.begin(), other.deduper.uniques.end());
        }

        // Custom copy assignment
        IndexedStringArena &operator=(const IndexedStringArena &other)
        {
            if (this != &other)
            {
                base::operator=(other);
                index_to_id = other.index_to_id;
                deduper.clear();
                deduper.reserve(other.deduper.size());
                deduper.insert(other.deduper.begin(), other.deduper.end());
            }
            return *this;
        }

        void ReserveIndices(size_t n_indices)
        {
            if (index_to_id.size() < n_indices)
                index_to_id.resize(n_indices, NULL_SPAN_ID);
            deduper.reserve(n_indices);
        }

        void Reserve(size_t n_indices, size_t n_spans, size_t n_elements)
        {
            ReserveIndices(n_indices);
            base::Reserve(n_spans, n_elements);
        }

        void ReserveFromHint(std::string_view id)
        {
            size_t n_indices = CapacityHints::GetHint(std::format("{}_indices", id));
            ReserveIndices(n_indices);
            base::ReserveFromHint(id);
        }

        void StoreCapacityHint(std::string_view id)
        {
            CapacityHints::UpdateHint(std::format("{}_indices", id), index_to_id.size());
            base::StoreCapacityHint(id);
        }

        float CalcSpansPerIndex() const { return index_to_id.empty() ? 0.f : this->id_to_span.size() / static_cast<float>(index_to_id.size()); }

        void clear()
        {
            base::clear();
            index_to_id.clear();
            deduper.clear();
        }

        std::span<T_span_id> SpanIds() { return index_to_id; }

        std::optional<T_span_id> GetSpanId(size_t index) const
        {
            if (index >= index_to_id.size())
                return std::nullopt;
            auto span_id = index_to_id[index];
            if (span_id == NULL_SPAN_ID)
                return std::nullopt;
            return span_id;
        }

        void SetSpanId(size_t index, T_span_id span_id)
        {
            if (index >= index_to_id.size())
            {
                index_to_id.resize(index + 1, NULL_SPAN_ID);
            }
            index_to_id[index] = span_id;
        }

        std::span<T> GetIndexed(size_t index)
        {
            auto span_id_opt = GetSpanId(index);
            if (!span_id_opt.has_value())
                return {};
            auto span_id = span_id_opt.value();
            return base::Get(span_id);
        }
        base::T_const_view GetIndexed(size_t index) const
        {
            auto span_id_opt = GetSpanId(index);
            if (!span_id_opt.has_value())
                return {};
            auto span_id = span_id_opt.value();
            return base::Get(span_id);
        }

        T_span_id CommitWrittenToIndex(size_t index)
        {
            auto span_id = base::CommitWritten(&deduper);
            SetSpanId(index, span_id);
            return span_id;
        }

        std::string GetDebugInfo()
        {
            std::string info = "IndexedStringArena:\n";
            info += "Num Elements: " + std::to_string(this->size()) + "\n";
            info += "Num Spans: " + std::to_string(this->id_to_span.size()) + "\n";
            info += "Num Indices: " + std::to_string(index_to_id.size()) + "\n";
            info += "Avg Span Size: " + std::to_string(this->CalcAvgSpanSize()) + "\n";
            info += "Spans Per Index: " + std::to_string(this->CalcSpansPerIndex()) + "\n";

            if constexpr (this->is_char || this->is_wchar)
            {
                struct SortData
                {
                    T_span_id id;
                    size_t count;
                };

                std::vector<SortData> span_counts(this->id_to_span.size(), {0, 0});
                for (auto id : index_to_id)
                {
                    if (id == NULL_SPAN_ID)
                        continue;
                    span_counts[id].count++;
                }
                for (size_t i = 0; i < span_counts.size(); ++i)
                {
                    span_counts[i].id = i;
                }

                std::sort(span_counts.begin(), span_counts.end(), [](auto &a, auto &b)
                          { return a.count > b.count; });

                info += "Top 10 most common spans:\n";
                for (size_t i = 0; i < span_counts.size() && i < 10; i++)
                {
                    auto span = this->Get(span_counts[i].id);
                    info += std::to_string(span_counts[i].count) + " times: " + std::string(span.begin(), span.end()) + "\n";
                }
            }
            return info;
        }
    };
}