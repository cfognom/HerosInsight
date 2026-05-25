#pragma once

#include <optional>
#include <set>
#include <span>

#include <bitview.h>
#include <front_back_pair.h>
#include <matcher.h>
#include <multibuffer.h>
#include <profiling.h>
#include <span_vector.h>
#include <string_manager.h>
#include <text_provider.h>
#include <utils.h>

namespace HerosInsight::Filtering
{
    std::strong_ordering CompareSubstrs(std::span<const uint16_t> asubs, std::string_view astr, std::span<const uint16_t> bsubs, std::string_view bstr);

    struct Filter
    {
        size_t meta_prop_id = 0;
        std::string_view source_text;
        std::string_view filter_text;
        std::vector<Matcher> matchers; // We have several because we can OR them
        bool is_exclusion;
    };
    struct SortCommand
    {
        struct Arg
        {
            size_t sort_target_id = 0;
            bool is_negated = false;
        };

        std::vector<Arg> args;
    };
    using Command = std::variant<SortCommand>;

    template <class... Ts>
    struct VisitHelper : Ts...
    {
        using Ts::operator()...;
    };
    template <class... Ts>
    VisitHelper(Ts...) -> VisitHelper<Ts...>;

    struct SortAtom
    {
        uint16_t prop_id = 0;
        bool is_negated = false;
        bool sort_by_matched = false;
    };

    struct Query
    {
        std::vector<Filter> filters; // Exclusion filters first, then inclusion
        std::vector<size_t> filters_in_decl_order;
        std::span<Filter> exclusion_filters;
        std::span<Filter> inclusion_filters;
        bool reverse_output = false;
        std::vector<SortAtom> sort_atoms;
        std::vector<SortCommand::Arg> sort_args;

        Filter &GetFilterInDeclOrder(size_t index)
        {
            return filters[filters_in_decl_order[index]];
        }

        void clear()
        {
            reverse_output = false;
            filters.clear();
            filters_in_decl_order.clear();
            sort_atoms.clear();
            sort_args.clear();
        }
    };

    struct RelevantMeta
    {
        uint32_t meta_id;
        uint32_t popcount;
    };

    struct ResultItem
    {
        LoweredStringView searchable_text;
        FixedVector<char, 512> presentable_text;
        std::span<uint16_t> searchable_hl;
        std::span<uint16_t> presentable_hl;

        std::vector<uint16_t> _hl_storage; // Highlight
    };

    struct IncrementalProp
    {
        using BuildTemplateFn =
            Text::StringTemplateAtom (*)(
                Text::StringTemplateAtom::Builder &b,
                size_t itemId,
                void *data
            );

        Text::StringManager &mgr = Text::s_Manager;
        LoweredStringVector searchable_text;
        SpanVector<Text::StringTemplateAtom> string_templates;
        SpanVector<Text::StringTemplateAtom>::Deduper string_templates_deduper;
        std::vector<uint16_t> item_to_str;
        void *build_template_data;
        BuildTemplateFn Build_template_fn;

        size_t GetUniqueCount() const { return searchable_text.arena.SpanCount(); }

        void SetupIncremental(void *data, BuildTemplateFn fn)
        {
            build_template_data = data;
            Build_template_fn = fn;
            string_templates_deduper = string_templates.CreateDeduper(0);
        }

        // TODO: Settle on one strategy?: incremental or this.
        void PopulateItems(std::string_view hint_key, size_t count, auto &&itemPropertyGetter, bool dedupe = true)
        {
            auto &arena = searchable_text.arena;
            arena.clear();
            arena.ReserveFromHint(hint_key);
            item_to_str.clear();
            item_to_str.reserve(count);

            std::string deduper_hint_key;
            SpanVector<char>::Deduper deduper;
            if (dedupe)
            {
                deduper_hint_key = std::format("{}_deduper", hint_key);
                auto bucket_count = CapacityHints::GetHint(deduper_hint_key);
                deduper = arena.CreateDeduper(bucket_count);
            }
            auto p_deduper = dedupe ? &deduper : nullptr;

            auto skills = GW::SkillbarMgr::GetSkills();
            for (size_t i = 0; i < count; ++i)
            {
                itemPropertyGetter(arena, i);
                auto strId = arena.CommitWritten(p_deduper);
                item_to_str.push_back(strId);
            }
            searchable_text.LowercaseFold();

            if (dedupe)
            {
                CapacityHints::UpdateHint(deduper_hint_key, deduper.uniques.bucket_count());
            }
            arena.StoreCapacityHint(hint_key);
        }
        void Reset();

        size_t GetStrId(size_t itemId);
        Text::StringTemplate GetStringTemplate(size_t strId);
        LoweredStringView GetSearchableStr(size_t strId) { return searchable_text[strId]; }
        void GetReadableString(size_t strId, OutBuf<char> out, OutBuf<Text::PosDelta> deltas);
    };

    struct Feedback
    {
        std::string filter_feedback;
        std::string command_feedback;
    };

    struct MetaProp
    {
        LoweredStringView name;
        BitView propset; // Which props this meta-prop targets
    };

    struct Device
    {
        using index_t = uint16_t;

        std::span<MetaProp> metas;
        std::span<IncrementalProp *> props;
        SpanVector<RelevantMeta> relevant_metas_per_prop; // For each prop this is sorted from most specific to least

        Device(std::span<MetaProp> metas, std::span<IncrementalProp *> props);

        void ParseQuery(std::string_view source, Query &query) const;
        void RunQuery(Query &query, std::vector<index_t> &items) const;

        void GetFeedback(Query &query, Feedback &out, bool verbose = false) const;
        ResultItem CalcItemResult(Query &q, size_t prop_id, index_t item_id) const;
        ResultItem CalcPropResult(Query &q, size_t prop_id) const;
    };
}