#pragma once

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

#include <capacity_hints.h>
#include <utils.h>

#ifdef _DEBUG
#define SAFETY_CHECKS
#endif

namespace HerosInsight
{
    template <typename T>
    class StringArena : public std::vector<T>
    {
        static_assert(std::has_unique_object_representations_v<T>, "T must have unique object representations");

    protected:
        constexpr static bool is_char = std::is_same_v<T, char>;

    public:
        using T_span = std::conditional_t<is_char, std::string_view, std::span<T>>;
        using T_offset = uint32_t;
        using T_span_id = uint16_t;

    protected:
        struct LocalSpan
        {
            uint32_t offset;
            uint32_t size;

            auto resolve(std::vector<T> &vec) const
            {
#ifdef SAFETY_CHECKS
                assert(offset + size <= vec.size());
#endif
                return T_span(vec.data() + offset, size);
            }

            std::string_view resolve_as_str(std::vector<T> &vec) const
            {
#ifdef SAFETY_CHECKS
                assert(offset + size <= vec.size());
#endif
                constexpr size_t element_size = sizeof(T);
                const size_t bytes = element_size * size;
                return std::string_view((const char *)(vec.data() + offset), bytes);
            }
        };

        struct Hasher
        {
            size_t operator()(const LocalSpan &span) const
            {
                return std::hash<std::string_view>{}(std::string_view((const char *)(vec.data() + span.offset), span.size * sizeof(T)));
            }

            std::vector<T> &vec;
        };

        struct Eq
        {
            bool operator()(const LocalSpan &a, const LocalSpan &b) const
            {
                if constexpr (is_char)
                {
                    return a.resolve(vec) == b.resolve(vec);
                }
                else
                {
                    return a.resolve_as_str(vec) == b.resolve_as_str(vec);
                }
            }

            std::vector<T> &vec;
        };

    protected:
        std::vector<LocalSpan> id_to_span;
#ifdef SAFETY_CHECKS
        bool is_writing = false;
#endif

    public:
        using deduper = std::unordered_map<LocalSpan, T_span_id, Hasher, Eq>;

        // The count of strings in the arena
        size_t SpanCount() const { return id_to_span.size(); }

        void Reserve(size_t n_spans, size_t n_elements)
        {
            id_to_span.reserve(n_spans);
            this->reserve(n_elements);
        }

        void ReserveFromHint(const std::string &id)
        {
            auto n_spans = g_capacityHints.get(id + "_spans");
            auto n_elements = g_capacityHints.get(id + "_elements");
            Reserve(n_spans, n_elements);
        }

        void StoreCapacityHint(const std::string &id)
        {
            g_capacityHints.update(id + "_spans", id_to_span.size());
            g_capacityHints.update(id + "_elements", this->size());
        }

        float CalcAvgSpanSize() const { return id_to_span.empty() ? 0.f : (float)this->size() / (float)id_to_span.size(); }

        void Reset()
        {
            std::vector<T>::clear();
            id_to_span.clear();
        }

        // Deduper is valid until the StringArena is moved!
        deduper CreateDeduper(size_t n_buckets)
        {
            return deduper(n_buckets, Hasher{*this}, Eq{*this});
        }

        void BeginWrite()
        {
            id_to_span.emplace_back();
            auto &span = id_to_span.back();
#ifdef SAFETY_CHECKS
            assert(!is_writing);
            is_writing = true;
            assert(span.offset == 0);
            assert(span.size == 0);
#endif
            auto offset = this->size();
#ifdef SAFETY_CHECKS
            assert(offset <= std::numeric_limits<decltype(span.offset)>::max());
#endif
            span.offset = static_cast<decltype(span.offset)>(offset);
        }

        void PopBack()
        {
            auto &back = id_to_span.back();
            auto new_size = this->size() - back.size;
            this->erase(this->begin() + new_size, this->end());
            id_to_span.pop_back();
#ifdef SAFETY_CHECKS
            is_writing = false;
#endif
        }

        T_span_id EndWrite(deduper *deduper = nullptr)
        {
#ifdef SAFETY_CHECKS
            assert(is_writing);
            is_writing = false;
#endif

            auto span_id = id_to_span.size() - 1;
#ifdef SAFETY_CHECKS
            assert(span_id <= std::numeric_limits<T_span_id>::max());
#endif
            T_span_id span_id_cast = static_cast<T_span_id>(span_id);

            auto &span = id_to_span[span_id];
            size_t span_size = this->size() - span.offset;
#ifdef SAFETY_CHECKS
            assert(span_size <= std::numeric_limits<decltype(span.size)>::max());
#endif
            span.size = static_cast<decltype(span.size)>(span_size);

            if (deduper != nullptr)
            {
                auto it = deduper->find(span);
                if (it == deduper->end())
                {
                    deduper->emplace(span, span_id_cast);
                }
                else
                {
                    PopBack();
                    return it->second;
                }
            }

            if constexpr (is_char)
            {
                this->push_back('\0');
            }

            return span_id_cast;
        }

        // Gets a reference to a string in the arena
        T_span Get(size_t span_id)
        {
#ifdef SAFETY_CHECKS
            assert(is_writing == false);
#endif
            if (span_id >= id_to_span.size())
                return T_span();

            return id_to_span[span_id].resolve(*this);
        }

        // Writer should modify the span size to the number of elements written
        template <class Writer>
            requires std::invocable<Writer, std::span<T> &>
        void AppendWriteBuffer(size_t buf_size, Writer &&writer)
        {
            const size_t size = this->size();
            this->resize(size + buf_size);
            auto buffer_span = std::span<T>(this->data() + size, buf_size);
            writer(buffer_span);
            this->resize(size + buffer_span.size());
        }
    };

    // A datastructure that can associate ids with a set of unique spans of data.
    // Auto-dedupes equal entries.
    template <typename T>
    class IndexedStringArena : public StringArena<T>
    {
        using base = StringArena<T>;
        using T_span = base::T_span;

    public:
        using T_span_id = base::T_span_id;
        constexpr static T_span_id NULL_SPAN_ID = std::numeric_limits<T_span_id>::max();

        std::vector<T_span_id> index_to_id;

    private:
        base::deduper deduper = base::CreateDeduper(0);

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

        void ReserveFromHint(const std::string &id)
        {
            size_t n_indices = g_capacityHints.get(id + "_indices");
            ReserveIndices(n_indices);
            base::ReserveFromHint(id);
        }

        void StoreCapacityHint(const std::string &id)
        {
            g_capacityHints.update(id + "_indices", index_to_id.size());
            base::StoreCapacityHint(id);
        }

        float CalcSpansPerIndex() const { return index_to_id.empty() ? 0.f : this->id_to_span.size() / static_cast<float>(index_to_id.size()); }

        void Reset()
        {
            base::Reset();
            index_to_id.clear();
            deduper.clear();
        }

        std::span<T_span_id> SpanIds() { return index_to_id; }

        T_span_id GetSpanId(size_t index)
        {
            if (index >= index_to_id.size())
                return NULL_SPAN_ID;
            return index_to_id[index];
        }

        void SetSpanId(size_t index, T_span_id span_id)
        {
            if (index >= index_to_id.size())
            {
                index_to_id.resize(index + 1, NULL_SPAN_ID);
            }
            index_to_id[index] = span_id;
        }

        T_span GetIndexed(size_t index) { return base::Get(GetSpanId(index)); }

        void BeginWrite()
        {
            base::BeginWrite();
        }

        T_span_id EndWrite(size_t index)
        {
            auto span_id = base::EndWrite(&deduper);
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

            if constexpr (this->is_char)
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