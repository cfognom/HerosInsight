#pragma once

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>

#ifdef _DEBUG
#define SAFETY_CHECKS
#endif

namespace HerosInsight
{
    template <typename T>
    class StringArena : public std::vector<T>
    {
    protected:
        constexpr static bool is_char = std::is_same_v<T, char>;

    public:
        using T_span = std::conditional_t<is_char, std::string_view, std::span<T>>;
        using T_offset = uint32_t;
        using T_span_id = uint16_t;

    protected:
        struct LocalSpan
        {
            size_t offset;
            size_t size;

            auto resolve(std::vector<T> &vec) const
            {
#ifdef SAFETY_CHECKS
                assert(offset + size <= vec.size());
#endif
                return T_span(vec.data() + offset, size);
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
                    if (a.size != b.size)
                        return false;
                    auto as = a.resolve(vec);
                    auto bs = b.resolve(vec);
                    return std::equal(as.begin(), as.end(), bs.begin());
                }
            }

            std::vector<T> &vec;
        };

    protected:
        std::vector<T_span> id_to_span;
#ifdef SAFETY_CHECKS
        bool is_writing = false;
#endif

    public:
        using deduper = std::unordered_map<LocalSpan, T_span_id, Hasher, Eq>;

        // The count of strings in the arena
        size_t Count() const { return id_to_span.size(); }

        void clear()
        {
            std::vector<T>::clear();
            id_to_span.clear();
        }

        std::span<T_span> Spans() const { return id_to_span; }

        void Assign(StringArena<T> &other)
        {
            this->assign(other);
            id_to_span.assign(other.id_to_span);
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
            assert(span.data() == nullptr);
            assert(span.size() == 0);
#endif
            auto offset = this->size();
            // #ifdef SAFETY_CHECKS
            //             assert(offset <= std::numeric_limits<decltype(span.offset)>::max());
            // #endif
            span = T_span(this->data() + offset, 0);
            // span.offset = static_cast<decltype(span.offset)>(offset);
        }

        void PopBack()
        {
            auto &back = id_to_span.back();
            this->resize(back.data() - this->data());
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

            if (!id_to_span.empty())
            {
                auto offset = this->data() - id_to_span[0].data();
                if (offset != 0)
                {
                    // The backing vector was reallocated, we need to patch all stored views.
                    for (auto &span : id_to_span)
                    {
                        span = T_span(span.data() + offset, span.size());
                    }
                }
            }

            auto string_id = id_to_span.size() - 1;
#ifdef SAFETY_CHECKS
            assert(string_id <= std::numeric_limits<T_span_id>::max());
#endif
            T_span_id string_id_cast = static_cast<T_span_id>(string_id);

            auto &span = id_to_span[string_id];
            size_t span_size = this->data() + this->size() - span.data();
            // #ifdef SAFETY_CHECKS
            //             assert(span_size <= std::numeric_limits<decltype(span.size)>::max());
            // #endif
            //             span.size = static_cast<decltype(span.size)>(span_size);
            span = T_span(span.data(), span_size);

            if (deduper != nullptr)
            {
                auto local_span = LocalSpan{static_cast<size_t>(span.data() - this->data()), span_size};
                auto it = deduper->find(local_span);
                if (it == deduper->end())
                {
                    deduper->emplace(local_span, string_id_cast);
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

            return string_id_cast;
        }

        // Gets a reference to a string in the arena
        T_span Get(size_t span_id) const
        {
#ifdef SAFETY_CHECKS
            assert(is_writing == false);
#endif
            if (span_id >= id_to_span.size())
                return T_span();

            // return id_to_span[span_id].resolve(*this);
            return id_to_span[span_id];
        }

        template <class Op>
            requires std::invocable<Op, std::span<T> &>
        void AppendBufferAndOverwrite(size_t buf_size, Op &&op)
        {
            const size_t size = this->size();
            this->resize(size + buf_size);
            auto span = std::span<T>(this->data() + size, buf_size);
            op(span);
            this->resize(size + span.size());
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

        std::vector<T_span_id> span_ids;

    private:
        base::deduper deduper = base::CreateDeduper(0);

    public:
        using value_type = T;

        IndexedStringArena() {}
        IndexedStringArena(size_t init_view_count, size_t init_element_count = 0)
        {
            this->ReserveViews(init_view_count);
            this->ReserveElements(init_element_count);
        }

        void ReserveElements(size_t count)
        {
            this->reserve(count);
        }

        void ReserveViews(size_t count)
        {
            if (span_ids.size() < count)
                span_ids.resize(count, NULL_SPAN_ID);
            deduper.reserve(count);
        }

        void Clear()
        {
            base::Clear();
            span_ids.clear();
            deduper.clear();
        }

        std::span<T_span_id> SpanIds() { return span_ids; }

        T_span_id &GetSpanId(size_t index) { return span_ids[index]; }

        T_span GetIndexed(size_t index) { return base::Get(GetSpanId(index)); }

        void BeginWrite()
        {
            base::BeginWrite();
        }

        T_span_id EndWrite(size_t index)
        {
            auto span_id = base::EndWrite(&deduper);
            if (index >= span_ids.size())
            {
                span_ids.resize(index + 1, NULL_SPAN_ID);
            }
            span_ids[index] = span_id;
            return span_id;
        }
    };
}