#include <Windows.h>
#include <bitset>
#include <codecvt>
#include <d3d9.h>
#include <filesystem>
#include <future>
#include <iostream>
#include <optional>
#include <ranges>
#include <regex>
#include <span>
#include <string>
#include <variant>

#include <GWCA/GWCA.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/AssetMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Utilities/Export.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/GWCA.h>
#include <GWCA/Packets/Opcodes.h>
#include <GWCA/Packets/StoC.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/List.h>

#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/PreGameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Attribute.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/EventMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Debug.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <StoC_packets.h>
#include <attribute_or_title.h>
#include <attribute_store.h>
#include <capacity_hints.h>
#include <constants.h>
#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <debug.h>
#include <debug_display.h>
#include <filtering.h>
#include <matcher.h>
#include <party_data.h>
#include <rich_text.h>
#include <string_arena.h>
#include <string_manager.h>
#include <texture_module.h>
#include <update_manager.h>
#include <utils.h>
#include <variable_size_clipper.h>

#include "skill_book.h"

// #define ASYNC_FILTERING

bool IsInitialCursorPos()
{
    auto cr_min = ImGui::GetWindowContentRegionMin();
    auto cursor = ImGui::GetCursorPos();
    return cr_min.x == cursor.x && cr_min.y == cursor.y;
}

// This adds a "skill book" window where the user can search and filter all skills in the game.
namespace HerosInsight::SkillBook
{
    std::vector<uint16_t> base_skills; // skill ids

    using SkillID = GW::Constants::SkillID;

    void InitBaseSkills()
    {
        auto &text_provider = Text::GetTextProvider(GW::Constants::Language::English);

        base_skills.reserve(GW::Constants::SkillMax - 1);
        for (uint16_t i = 1; i < GW::Constants::SkillMax; i++)
        {
            base_skills.push_back(i);
        }

        auto Comparer = [&text_provider](uint16_t a, uint16_t b)
        {
            // auto &skill_a = *GW::SkillbarMgr::GetSkillConstantData((GW::Constants::SkillID)a);
            // auto &skill_b = *GW::SkillbarMgr::GetSkillConstantData((GW::Constants::SkillID)b);
            // auto a_id = skill_a.IsPvP() ? skill_a.skill_id_pvp : (GW::Constants::SkillID)a;
            // auto b_id = skill_b.IsPvP() ? skill_b.skill_id_pvp : (GW::Constants::SkillID)b;
            // if (a_id == b_id)
            // {
            //     assert(skill_a.IsPvP() != skill_b.IsPvP());
            //     return skill_a.IsPvP() < skill_b.IsPvP();
            // }
            auto &custom_sd_a = CustomSkillDataModule::GetCustomSkillData((GW::Constants::SkillID)a);
            auto &custom_sd_b = CustomSkillDataModule::GetCustomSkillData((GW::Constants::SkillID)b);

            auto ca = custom_sd_a.context;
            auto cb = custom_sd_b.context;
            if (ca != cb)
                return ca < cb;

            auto pa = (uint32_t)custom_sd_a.skill->profession - 1; // We do this so that None comes last
            auto pb = (uint32_t)custom_sd_b.skill->profession - 1;
            if (pa != pb)
                return pa < pb;

            auto attr_a = custom_sd_a.attribute.value;
            auto attr_b = custom_sd_b.attribute.value;
            if (attr_a != attr_b)
                return attr_a < attr_b;

            auto ty_a = custom_sd_a.GetTypeString();
            auto ty_b = custom_sd_b.GetTypeString();
            if (ty_a != ty_b)
                return ty_a < ty_b;

            auto camp_a = custom_sd_a.GetCampaignStr();
            auto camp_b = custom_sd_b.GetCampaignStr();
            if (camp_a != camp_b)
                return camp_a < camp_b;

            auto n_a = text_provider.GetName((GW::Constants::SkillID)a);
            auto n_b = text_provider.GetName((GW::Constants::SkillID)b);
            if (n_a != n_b)
                return n_a < n_b;

            return a < b;
        };
        std::sort(base_skills.begin(), base_skills.end(), Comparer);
    }

    enum struct AttributeMode : uint32_t
    {
        Null,
        Generic,
        Characters,
        Manual
    };

    // The order they are listed in here is also the order the properties are filtered by.
    // This order has big impacts in the filtering speed.
    // The filtering device works by trying to confirm if an item matches the filter, if so it can discard it for further processing.
    // It therefore makes sense to try to put properties which are likely to match first,
    // and also similar properties far away from each other. Since if, for example, no matches were found in the description,
    // it is unlikely any will be found in the concise description.
    enum struct SkillProp : uint16_t
    {
        Description,
        Type,
        Attribute,
        Profession,
        Campaign,
        Tag,
        Range,
        Name,

        Energy,
        Recharge,
        Activation,
        Adrenaline,
        Sacrifice,
        Overcast,
        Aftercast,
        Upkeep,

        Concise,
        // Parsed,
        Id,

        COUNT,
    };

    constexpr size_t PROP_COUNT = static_cast<size_t>(SkillProp::COUNT);

    uint32_t focused_agent_id = 0;

    struct AttributeSource
    {
        enum struct Type
        {
            FromAgent,
            Constant,
        };

        AttributeSource() : type(Type::Constant), value(-1) {}

        Type type;
        size_t value;

        int8_t GetAttrLvl(AttributeOrTitle id) const
        {
            switch (this->type)
            {
                case Type::FromAgent:
                {
                    // auto agent_id = this->value;
                    auto agent_id = focused_agent_id;
                    auto custom_agent_data = CustomAgentDataModule::GetCustomAgentData(agent_id);
                    return custom_agent_data.GetOrEstimateAttribute(id);
                }
                default:
                case Type::Constant:
                {
                    return (int8_t)this->value;
                }
            }
        }
    };

    class SkillTooltipProvider : public RichText::TextTooltipProvider
    {
    public:
        GW::Constants::SkillID skill_id;
        SkillTooltipProvider(GW::Constants::SkillID skill_id) : skill_id(skill_id) {}
        void DrawTooltip(uint32_t tooltip_id)
        {
            auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);
            const auto attr_str = custom_sd.GetAttributeStr();

            const auto buffer_size = 8;
            char buffer1[buffer_size];
            char buffer2[buffer_size];
            std::string_view text1;
            std::string_view text2;

            ImGui::BeginTooltip();
            ImGui::PushFont(Constants::Fonts::gw_font_16);
            auto draw_list = ImGui::GetWindowDrawList();
            const auto init_cursor_ss = ImGui::GetCursorScreenPos();
            auto cursor = init_cursor_ss;

            auto text_height = ImGui::GetTextLineHeight();
            const auto thick_font_offset = 2;
            const auto x_padding = 3;
            const auto separator_overflow = 1;

            for (int32_t i = -1; i <= 21; i++)
            {
                bool thick = i == 0 || i == 12 || i == 15;
                if (thick)
                {
                    ImGui::PushFont(Constants::Fonts::skill_thick_font_15);
                }

                if (i == -1)
                {
                    text1 = attr_str;
                    text2 = "Value";
                }
                else
                {
                    auto start = cursor + ImVec2(0, -separator_overflow);
                    auto end = cursor + ImVec2(0, 2 * text_height + separator_overflow);
                    float thickness = 1.0f;
                    draw_list->AddLine(start, end, ImGui::GetColorU32(ImGuiCol_Border), thickness);
                    cursor.x += thickness;

                    snprintf(buffer1, buffer_size, "%d", i);
                    snprintf(buffer2, buffer_size, "%d", custom_sd.GetSkillParam(tooltip_id).Resolve(i));
                    text1 = buffer1;
                    text2 = buffer2;
                }
                float width1 = ImGui::CalcTextSize(text1.data()).x;
                float width2 = ImGui::CalcTextSize(text2.data()).x;
                auto max_width = std::max(width1, width2);

                cursor.x += x_padding;
                auto color = ImGui::GetColorU32(IM_COL32_WHITE);

                auto text_cursor1 = cursor;

                if (thick)
                {
                    text_cursor1.y += thick_font_offset;
                }

                auto text_cursor2 = text_cursor1;
                text_cursor2.y += text_height;

                text_cursor1.x += (max_width - width1) / 2;
                draw_list->AddText(text_cursor1, ImGui::GetColorU32(IM_COL32_WHITE), text1.data(), text1.data() + text1.size());

                text_cursor2.x += (max_width - width2) / 2;
                draw_list->AddText(text_cursor2, ImGui::GetColorU32(Constants::GWColors::skill_dynamic_green), text2.data(), text2.data() + text2.size());

                cursor.x += max_width + x_padding;

                if (thick)
                {
                    ImGui::PopFont();
                }
            }

            const auto min = init_cursor_ss;
            const auto max = cursor + ImVec2(0, 2 * text_height);
            ImRect rect(min, max);

            ImGui::ItemSize(rect);
            ImGui::ItemAdd(rect, 0);

            ImGui::PopFont();
            ImGui::EndTooltip();
        }
    };

    class SkillFracProvider : public RichText::TextFracProvider
    {
        // #define DEBUG_FRACS

        using FracTag = RichText::FracTag;

#define BIG_FONT Constants::Fonts::skill_thick_font_15
#define SMALL_FONT Constants::Fonts::skill_thick_font_9

        static constexpr GW::Vec2f SLASH_SIZE{15, 16};
        static constexpr GW::Vec2f SLASH_NUMERATOR_MAX{5, 7};
        static constexpr GW::Vec2f SLASH_DENOMINATOR_MIN{7, 5};

        std::string_view GetSimple(FracTag tag)
        {
            // clang-format off
            switch (tag.GetValue()) {
                case FracTag{1, 2}.GetValue(): return "½";
                case FracTag{1, 4}.GetValue(): return "¼";
                case FracTag{3, 4}.GetValue(): return "¾";
            }
            // clang-format on
            return {};
        }

        struct Cache
        {
            uint16_t val = std::numeric_limits<uint16_t>::max();
            char text[18];
            float over_width;
        };

        Cache caches[2];

        void DrawFraction(ImVec2 pos, FracTag tag) override
        {
            auto window = ImGui::GetCurrentWindow();
            auto draw_list = window->DrawList;
            auto color = ImGui::GetColorU32(ImGuiCol_Text);
            auto simple_str = GetSimple(tag);
            if (simple_str.data() != nullptr)
            {
                draw_list->AddText(BIG_FONT, BIG_FONT->FontSize, pos, color, simple_str.data(), simple_str.data() + simple_str.size());
                return;
            }

            if (caches[0].val != tag.num || caches[1].val != tag.den)
                CalcWidth(tag);

            const std::string_view base_str = "½";
            const auto size = ImVec2(SLASH_SIZE.x, SLASH_SIZE.y);
            ImGui::PushFont(BIG_FONT);
#ifdef _DEBUG
            const auto size_check = ImGui::CalcTextSize(base_str.data(), base_str.data() + base_str.size());
            assert(size_check.x == size.x && size_check.y == size.y);
#endif

            // We need to draw the ½ char while masking out the 1 and 2 and then fill in with our own numerator and denominator
            // So we need 3 draw calls with 3 different clip rects
            // 1▪□  ▪ = clip rect 1
            // ■■□  □ = clip rect 2
            // ■■2  ■ = clip rect 3
            ImVec4 local_clip_rects[]{
                ImVec4(SLASH_NUMERATOR_MAX.x, 0, SLASH_DENOMINATOR_MIN.x, SLASH_NUMERATOR_MAX.y),
                ImVec4(SLASH_DENOMINATOR_MIN.x, 0, SLASH_SIZE.x, SLASH_DENOMINATOR_MIN.y),
                ImVec4(0, SLASH_NUMERATOR_MAX.y, SLASH_DENOMINATOR_MIN.x, SLASH_SIZE.y)
            };

            ImVec2 slash_min{pos.x + caches[0].over_width, pos.y};
            ImVec2 slash_max = slash_min + size;
            for (auto &local_clip_rect : local_clip_rects)
            {
                auto clip_rect = local_clip_rect + ImVec4(slash_min.x, slash_min.y, slash_min.x, slash_min.y);
                draw_list->AddText(BIG_FONT, BIG_FONT->FontSize, slash_min, color, base_str.data(), base_str.data() + base_str.size(), 0.0f, &clip_rect);

#ifdef DEBUG_FRACS
                draw_list->AddRect(ImVec2(clip_rect.x, clip_rect.y), ImVec2(clip_rect.z, clip_rect.w), 0xFF0000FF, 0.0f, 0);
#endif
            }

            ImGui::PopFont();

            ImGui::PushFont(Constants::Fonts::skill_thick_font_9);
            auto num_pos = pos;
            auto den_pos = slash_min + ImVec2(SLASH_DENOMINATOR_MIN.x, SLASH_DENOMINATOR_MIN.y - 1);
            draw_list->AddText(num_pos, color, caches[0].text);
            draw_list->AddText(den_pos, color, caches[1].text);
            ImGui::PopFont();

            ImRect bb{pos, slash_max + ImVec2(caches[1].over_width, 0)};
            ImGui::ItemSize(bb);
            ImGui::ItemAdd(bb, 0);

#ifdef DEBUG_FRACS
            draw_list->AddRect(bb.Min, bb.Max, 0xFF0000FF, 0.0f, 0);
#endif
        }

        float CalcWidth(FracTag tag) override
        {
            float total_width = SLASH_SIZE.x;
            auto simple_str = GetSimple(tag);
            if (simple_str.data() != nullptr)
            {
#ifdef _DEBUG
                ImGui::PushFont(BIG_FONT);
                auto size_check = ImGui::CalcTextSize(simple_str.data(), simple_str.data() + simple_str.size());
                ImGui::PopFont();
                assert(size_check.x == SLASH_SIZE.x && size_check.y == SLASH_SIZE.y);
#endif
                return total_width;
            }

            ImGui::PushFont(SMALL_FONT);
            caches[0].val = tag.num;
            caches[1].val = tag.den;
            for (auto &cache : caches)
            {
                auto result = std::to_chars(cache.text, cache.text + sizeof(cache.text) - 1, cache.val);
                assert(result.ec == std::errc());
                *result.ptr = '\0';
                cache.over_width = std::max(0.f, ImGui::CalcTextSize(cache.text, result.ptr).x - SLASH_NUMERATOR_MAX.x);
                total_width += cache.over_width;
            }
            ImGui::PopFont();
            return total_width;
        }
    };
    SkillFracProvider frac_provider;
    RichText::Drawer text_drawer{nullptr, &frac_provider};

    using Propset = BitArray<PROP_COUNT + 1>; // +1 for meta props
    constexpr static Propset ALL_PROPS = Propset(true);
    constexpr static uint16_t INVALID_SPAN = std::numeric_limits<uint16_t>::max();

    struct TextStorage
    {
        std::array<Filtering::IncrementalProp, PROP_COUNT> static_props;
        std::vector<Propset> meta_propsets;
        LoweredTextVector meta_prop_names;

        static Text::StringTemplateAtom MakeNumOrFraction(Text::StringTemplateAtom::Builder &b, float value)
        {
            float value_int;
            float value_fract = std::modf(value, &value_int);

            std::optional<Text::StringTemplateAtom> fraction = std::nullopt;
            if (value_fract == 0.25f)
                fraction = b.Fraction(1, 4);
            else if (value_fract == 0.5f)
                fraction = b.Fraction(1, 2);
            else if (value_fract == 0.75f)
                fraction = b.Fraction(3, 4);

            if (fraction.has_value())
            {
                FixedVector<Text::StringTemplateAtom, 2> seq;
                if (value_int > 0.f)
                {
                    seq.push_back(b.Number(value_int));
                }
                seq.push_back(fraction.value());
                return b.ExplicitSequence(seq);
            }
            else
            {
                // Default: just output a floating point number
                return b.Number(value);
            }
        };

        template <auto Func, auto Icon>
        static auto NumberAndIconLambda(Text::StringTemplateAtom::Builder &b, size_t skill_id, void *)
        {
            auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
            return b.ExplicitSequence(
                {MakeNumOrFraction(b, (cskill.*Func)()),
                 b.ExplicitString(*Icon)}
            );
        }

        template <auto Func>
        static void GetSkillProp(StringArena<char> &dst, size_t skill_id)
        {
            auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
            dst.Elements().append_range((cskill.*Func)());
        }

        void InitProps()
        {
            static_props[(size_t)SkillProp::Energy].SetupIncremental(nullptr, NumberAndIconLambda<&CustomSkillData::GetEnergy, &RichText::Icons::EnergyOrb>);
            static_props[(size_t)SkillProp::Recharge].SetupIncremental(nullptr, NumberAndIconLambda<&CustomSkillData::GetRecharge, &RichText::Icons::Sacrifice>);

            static_props[(size_t)SkillProp::Upkeep].SetupIncremental(nullptr, NumberAndIconLambda<&CustomSkillData::GetUpkeep, &RichText::Icons::Upkeep>);
            static_props[(size_t)SkillProp::Overcast].SetupIncremental(nullptr, NumberAndIconLambda<&CustomSkillData::GetOvercast, &RichText::Icons::Overcast>);
            static_props[(size_t)SkillProp::Sacrifice].SetupIncremental(nullptr, NumberAndIconLambda<&CustomSkillData::GetSacrifice, &RichText::Icons::Sacrifice>);
            static_props[(size_t)SkillProp::Activation].SetupIncremental(nullptr, NumberAndIconLambda<&CustomSkillData::GetActivation, &RichText::Icons::Activation>);

            auto &text_provider = Text::GetTextProvider(GW::Constants::Language::English);
            static_props[(size_t)SkillProp::Name].PopulateItems(*text_provider.GetNames());

            static_props[(size_t)SkillProp::Type].PopulateItems("SkillBookProp_Type", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetTypeString>);
            static_props[(size_t)SkillProp::Attribute].PopulateItems("SkillBookProp_Attribute", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetAttributeStr>);
            static_props[(size_t)SkillProp::Profession].PopulateItems("SkillBookProp_Profession", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetProfessionStr>);
            static_props[(size_t)SkillProp::Campaign].PopulateItems("SkillBookProp_Campaign", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetCampaignStr>);
            static_props[(size_t)SkillProp::Id].PopulateItems(
                "SkillBookProp_Id",
                GW::Constants::SkillMax,
                [](StringArena<char> &dst, size_t skill_id)
                {
                    std::format_to(std::back_inserter(dst.Elements()), "{}", skill_id);
                }
            );

            static_props[(size_t)SkillProp::Aftercast].SetupIncremental(
                nullptr,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *) -> Text::StringTemplateAtom
                {
                    auto &skill = GW::SkillbarMgr::GetSkills()[skill_id];
                    const bool is_normal_aftercast = (skill.activation > 0 && skill.aftercast == 0.75f) ||
                                                     (skill.activation == 0 && skill.aftercast == 0);
                    if (skill.aftercast == 0.f && is_normal_aftercast)
                        return {};

                    FixedVector<Text::StringTemplateAtom, 5> number_and_icon;

                    if (!is_normal_aftercast)
                    {
                        number_and_icon.push_back(b.Color(IM_COL32(255, 255, 0, 255)));
                    }
                    number_and_icon.push_back(b.Number(skill.aftercast));
                    if (!is_normal_aftercast)
                    {
                        number_and_icon.push_back(b.Char('*'));
                        number_and_icon.push_back(b.Color(NULL));
                    }
                    number_and_icon.push_back(b.ExplicitString(RichText::Icons::Aftercast));

                    return b.ExplicitSequence(number_and_icon);
                }
            );

            static_props[(size_t)SkillProp::Range].PopulateItems(
                "SkillBookProp_Range",
                GW::Constants::SkillMax,
                [](StringArena<char> &dst, size_t skill_id)
                {
                    FixedVector<Utils::Range, 4> ranges;
                    auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
                    cskill.GetRanges(ranges);
                    for (size_t i = 0; i < ranges.size(); ++i)
                    {
                        auto range = ranges[i];
                        dst.AppendWriteBuffer(
                            64,
                            [range](std::span<char> &buffer)
                            {
                                SpanWriter<char> writer(buffer);
                                writer.AppendIntToChars((int)range);
                                auto range_name = Utils::GetRangeStr(range);
                                if (range_name.has_value())
                                {
                                    writer.AppendFormat(" ({})", range_name.value());
                                }
                                buffer = writer.WrittenSpan();
                            }
                        );

                        if (i < ranges.size() - 1)
                            dst.Elements().append_range(std::string_view(", "));
                    }
                }
            );
        }

        struct MetaPropSection
        {
            uint32_t start_index = 0;
            uint32_t end_index = 0;
        };

        MetaPropSection footer_meta_section;

        template <typename... Ts>
        static Propset CreatePropset(Ts... args)
        {
            Propset propset;
            ((propset[static_cast<size_t>(args)] = true), ...);
            return propset;
        }

        void SetupMetaProps()
        {
            auto SetupMetaProp = [&](std::string_view name, Propset propset)
            {
                meta_prop_names.arena.push_back(name);
                meta_propsets.push_back(propset);
            };

            struct SectionRecordingScope
            {
                SectionRecordingScope(MetaPropSection *dst, std::vector<Propset> &meta_propsets) : dst(dst), meta_propsets(meta_propsets)
                {
                    dst->start_index = static_cast<uint32_t>(meta_propsets.size());
                }
                ~SectionRecordingScope()
                {
                    dst->end_index = static_cast<uint32_t>(meta_propsets.size());
                }

            private:
                MetaPropSection *dst;
                std::vector<Propset> &meta_propsets;
            };

            std::string_view capacity_hint_key = "meta_prop_names";
            meta_prop_names.arena.ReserveFromHint(capacity_hint_key);
            meta_propsets.reserve(meta_prop_names.arena.SpanCount());

            SetupMetaProp("Anything", ALL_PROPS); // Must be first
            SetupMetaProp("Name", CreatePropset(SkillProp::Name));
            SetupMetaProp("Type", CreatePropset(SkillProp::Type));
            SetupMetaProp("Tags", CreatePropset(SkillProp::Tag));
            SetupMetaProp("Energy", CreatePropset(SkillProp::Energy));
            SetupMetaProp("Recharge", CreatePropset(SkillProp::Recharge));
            SetupMetaProp("Activation", CreatePropset(SkillProp::Activation));
            SetupMetaProp("Aftercast", CreatePropset(SkillProp::Aftercast));
            SetupMetaProp("Sacrifice", CreatePropset(SkillProp::Sacrifice));
            SetupMetaProp("Overcast", CreatePropset(SkillProp::Overcast));
            SetupMetaProp("Adrenaline", CreatePropset(SkillProp::Adrenaline));
            SetupMetaProp("Upkeep", CreatePropset(SkillProp::Upkeep));
            SetupMetaProp("Full Description", CreatePropset(SkillProp::Description));
            SetupMetaProp("Concise Description", CreatePropset(SkillProp::Concise));
            SetupMetaProp("Description", CreatePropset(SkillProp::Description, SkillProp::Concise));

            {
                SectionRecordingScope section(&footer_meta_section, meta_propsets);
                SetupMetaProp("Attribute", CreatePropset(SkillProp::Attribute));
                SetupMetaProp("Profession", CreatePropset(SkillProp::Profession));
                SetupMetaProp("Campaign", CreatePropset(SkillProp::Campaign));
                SetupMetaProp("Range", CreatePropset(SkillProp::Range));
            }

            SetupMetaProp("Id", CreatePropset(SkillProp::Id));

            meta_prop_names.LowercaseFold();
            meta_prop_names.arena.StoreCapacityHint(capacity_hint_key);
        }

        void Initialize()
        {
            InitBaseSkills();
            SetupMetaProps();
            InitProps();
        }

        struct DoubleWriter
        {
            double value;

            void operator()(std::span<char> &dst) const
            {
                auto [after, ec] = std::to_chars(dst.data(), dst.data() + dst.size(), value, std::chars_format::general);
                const bool success = (ec == std::errc());
#ifdef _DEBUG
                assert(success);
#endif
                auto written_count = success ? after - dst.data() : 0;
                dst = dst.subspan(0, written_count);
            }
        };
    };
    TextStorage text_storage;

    struct BookSettings
    {
        // AttributeMode attribute_mode = AttributeMode::Generic;
        AttributeSource attr_src;
        int attr_lvl_slider = 12;
        bool IsGeneric() const { return attr_src.type == AttributeSource::Type::Constant && attr_src.value == -1; }
        bool IsCharacters() const { return attr_src.type == AttributeSource::Type::FromAgent; }
        bool IsManual() const { return attr_src.type == AttributeSource::Type::Constant && attr_src.value != -1; }

        bool include_pve_only_skills = true;
        bool include_temporary_skills = false;
        bool include_npc_skills = false;
        bool include_archived_skills = false;
        bool include_disguises = false;

        bool use_exact_adrenaline = false;
        bool prefer_concise_descriptions = false;
        bool limit_to_characters_professions = false;
        bool show_null_stats = false;
        bool snap_to_skill = true;
    };

    struct FilteringAdapter
    {
        TextStorage &ts;
        std::unordered_map<SkillProp, Filtering::IncrementalProp> dynamic_props;
        std::array<Filtering::IncrementalProp *, PROP_COUNT> props;

        void RefreshDynamicProps(AttributeSource attr_src)
        {
            for (auto &prop : dynamic_props)
            {
                prop.second.MarkDirty();
            }
        }

        template <bool IsConcise>
        static Text::StringTemplateAtom MakeDescription(Text::StringTemplateAtom::Builder &b, size_t skill_id, void *data)
        {
            auto &settings = *(BookSettings *)data;
            auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
            auto &text_provider = Text::GetTextProvider(GW::Constants::Language::English);
            auto attr_lvl = settings.attr_src.GetAttrLvl(cskill.attribute);
            return text_provider.MakeSkillDescription(b, (GW::Constants::SkillID)skill_id, IsConcise, attr_lvl);
        }

        explicit FilteringAdapter(TextStorage &storage, BookSettings &settings)
            : ts(storage)
        {
            dynamic_props[SkillProp::Description].SetupIncremental(&settings, MakeDescription<false>);
            dynamic_props[SkillProp::Concise].SetupIncremental(&settings, MakeDescription<true>);
            dynamic_props[SkillProp::Adrenaline].SetupIncremental(
                &settings,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *data) -> Text::StringTemplateAtom
                {
                    auto &settings = *(BookSettings *)data;
                    auto &skill = GW::SkillbarMgr::GetSkills()[skill_id];
                    auto adrenaline = skill.adrenaline;
                    if (adrenaline == 0)
                        return {};

                    auto adrenaline_strikes = adrenaline / 25;
                    auto adrenaline_units = adrenaline % 25;

                    if (!settings.use_exact_adrenaline)
                    {
                        if (adrenaline_units > 0)
                        {
                            adrenaline_strikes += 1;
                            adrenaline_units = 0;
                        }
                    }

                    FixedVector<Text::StringTemplateAtom, 4> args;

                    if (adrenaline_strikes > 0)
                        args.push_back(b.Number(adrenaline_strikes));

                    if (adrenaline_units > 0)
                        args.push_back(b.Fraction(adrenaline_units, 25));

                    args.push_back(b.ExplicitString(RichText::Icons::Adrenaline));

                    return b.ExplicitSequence(args);
                }
            );
            dynamic_props[SkillProp::Tag].SetupIncremental(
                &settings,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *data) -> Text::StringTemplateAtom
                {
                    auto &skill = GW::SkillbarMgr::GetSkills()[skill_id];
                    auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
                    auto &tags = cskill.tags;
                    auto skill_id_to_check = skill.IsPvP() ? skill.skill_id_pvp : skill.skill_id;

                    bool is_unlocked = GW::SkillbarMgr::GetIsSkillUnlocked(skill_id_to_check);
                    bool is_learned = Utils::IsSkillLearned(skill, focused_agent_id);
                    bool is_locked = tags.Unlockable && !is_unlocked;

                    FixedVector<Text::StringTemplateAtom, 16> args;

                    auto PushTag = [&](std::string_view str, ImU32 color = NULL, std::string_view icon = {})
                    {
                        if (color != NULL)
                            args.push_back(b.Color(color));

                        args.push_back(b.ExplicitString(str));

                        if (color != NULL)
                            args.push_back(b.Color(NULL));

                        if (!icon.empty())
                            args.push_back(b.ExplicitString(icon));

                        args.push_back(b.ExplicitString(", "));
                    };

                    // clang-format off
                    if (is_learned)            PushTag("Learned"  , IM_COL32(100, 255, 255, 255));
                    if (is_unlocked)           PushTag("Unlocked" , IM_COL32(143, 255, 143, 255));
                    if (tags.Temporary)        PushTag("Temporary"                              );
                    if (is_locked)             PushTag("Locked"   , IM_COL32(255, 100, 100, 255));
                    
                    if (cskill.context != Utils::SkillContext::Null)
                        PushTag(Utils::GetSkillContextString(cskill.context));

                    if (tags.Archived)         PushTag("Archived");
                    if (tags.EffectOnly)       PushTag("Effect-only");
                    if (tags.PvEOnly)          PushTag("PvE-only");
                    if (tags.PvPOnly)          PushTag("PvP-only");
                    if (tags.PvEVersion)       PushTag("PvE Version");
                    if (tags.PvPVersion)       PushTag("PvP Version");

                    if (tags.DeveloperSkill)   PushTag("Developer Skill");
                    if (tags.EnvironmentSkill) PushTag("Environment Skill");
                    if (tags.MonsterSkill)     PushTag("Monster Skill", NULL, RichText::Icons::MonsterSkull);
                    if (tags.SpiritAttack)     PushTag("Spirit Attack");
                    
                    if (tags.Maintained)       PushTag("Maintained");
                    if (tags.ConditionSource)  PushTag("Condition Source");
                    if (tags.ExploitsCorpse)   PushTag("Exploits Corpse");
                    if (tags.Consumable)       PushTag("Consumable");
                    if (tags.Celestial)        PushTag("Celestial");
                    if (tags.Mission)          PushTag("Mission");
                    if (tags.Bundle)           PushTag("Bundle");
                    // clang-format on

                    if (!args.empty())
                    {
                        // Cut off the last ", "
                        args.pop();
                    }

                    return b.ExplicitSequence(args);
                }
            );

            for (size_t i = 0; i < PROP_COUNT; ++i)
            {
                auto prop_id = (SkillProp)i;
                auto it = dynamic_props.find(prop_id);
                if (it != dynamic_props.end())
                {
                    props[i] = &it->second;
                }
                else
                {
                    props[i] = &ts.static_props[i];
                }
            }
        }

        using index_type = uint16_t;
        size_t MaxSpanCount() const { return GW::Constants::SkillMax; }
        size_t PropCount() const { return PROP_COUNT; }
        size_t MetaCount() const { return ts.meta_propsets.size(); }

        LoweredText GetMetaName(size_t meta) { return ts.meta_prop_names.Get(meta); }
        BitView GetMetaPropset(size_t meta) const { return ts.meta_propsets[meta]; }

        Filtering::IncrementalProp &GetProperty(size_t prop) { return *props[prop]; }
    };

    bool is_dragging = false;
    std::atomic<GW::Constants::SkillID> skill_id_to_drag = GW::Constants::SkillID::No_Skill;
    void RequestSkillDragging(GW::Constants::SkillID skill_id)
    {
        GW::Constants::SkillID expected = GW::Constants::SkillID::No_Skill;
        skill_id_to_drag.compare_exchange_strong(expected, skill_id);
    }
    void CancelSkillDragging()
    {
        if (!is_dragging)
            return;

        is_dragging = false;

        // Because GW did not receive the mouse down event when we started dragging it will ignore the mouse up event.
        // A workaround is to simulate a right click to cancel the drag.
        INPUT inputs[2] = {};
        ZeroMemory(inputs, sizeof(inputs));
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
        SendInput(2, inputs, sizeof(INPUT));
    }
    void UpdateSkillDragging()
    {
        if (is_dragging && ImGui::IsMouseReleased(0))
        {
            CancelSkillDragging();
        }

        if (skill_id_to_drag == GW::Constants::SkillID::No_Skill)
            return; // No skill to drag

        auto result = Utils::GetSkillFrame(skill_id_to_drag);

        static auto CancelDragRequest = []()
        {
            skill_id_to_drag.store(GW::Constants::SkillID::No_Skill);
        };

        switch (result.error)
        {
            case Utils::GetSkillFrameResult::Error::SkillAndAttributesNotOpened:
                GW::GameThread::Enqueue(
                    []()
                    {
                        // Try to open "Skills and Attributes"
                        if (!GW::UI::Keypress(GW::UI::ControlAction_OpenSkillsAndAttributes))
                        {
                            // If we can't open "Skills and Attributes" we have to cancel the request
                            CancelDragRequest();
                        }
                    }
                );
                return; // Mission failed, we'll get them next time

            case Utils::GetSkillFrameResult::Error::None:
                // Success!
                GW::GameThread::Enqueue(
                    [frame = result.frame]()
                    {
                        auto packet = GW::UI::UIPacket::kMouseClick{0};
                        GW::UI::SendFrameUIMessage(frame, GW::UI::UIMessage::kMouseClick, &packet);
                    }
                );
                is_dragging = true;
                [[fallthrough]];
            default:
                CancelDragRequest();
        }
    }

    struct BookState;
    std::vector<std::unique_ptr<BookState>> books;
    BookState *AddBook()
    {
        auto &book = books.emplace_back(std::make_unique<BookState>());
        return book.get();
    }
    struct BookState
    {
        struct WindowDims
        {
            ImVec2 window_pos;
            ImVec2 window_size;
        };
        std::optional<WindowDims> init_dims = std::nullopt;
        BookSettings settings;
        FilteringAdapter adapter{text_storage, settings};
        Filtering::Device<FilteringAdapter> filter_device{adapter};
        Filtering::Query query;
        std::vector<uint16_t> filtered_skills; // skill ids

        Text::Provider &text_provider = Text::GetTextProvider(GW::Constants::Language::English);

        bool is_opened = true;
        bool filter_dirty = true;
        bool props_dirty = true;
        bool first_draw = true;

        struct ScrollTracking
        {
            std::vector<VariableSizeClipper::Position> scroll_positions;
            StringArena<char> input_text_history;
            size_t current_index = 0;

            ScrollTracking() { input_text_history.push_back(std::string_view("")); }

            void UpdateScrollTracking(std::string_view input_text, VariableSizeClipper &clipper)
            {
                auto target_scroll = clipper.GetTargetScroll();
                if (scroll_positions.size() < current_index + 1)
                    scroll_positions.resize(current_index + 1, target_scroll);
                scroll_positions[current_index] = target_scroll;

                // Get the range of input_text_history strings that is equal in size to input_text
                auto it = std::lower_bound(
                    input_text_history.begin(),
                    input_text_history.end(),
                    input_text,
                    [](auto a, auto b)
                    {
                        return a.size() < b.size();
                    }
                );
                current_index = it.Index();
                if (it == input_text_history.end() ||
                    std::string_view(*it) != input_text) // Not found
                {
                    // Discard diverging history
                    scroll_positions.resize(current_index);
                    input_text_history.Prune(current_index);
                    input_text_history.push_back(input_text);
                    // Reset scroll
                    clipper.Reset();
                }
                else
                {
                    // Restore old scroll
                    auto old_scoll = scroll_positions[current_index];
                    clipper.SetScroll(old_scoll);
                }
            }
        };
        ScrollTracking scroll_tracking;
        char input_text[1024] = {'\0'};

        std::string feedback;

        VariableSizeClipper clipper{};

        void Update()
        {
            bool filtered_outdated = props_dirty || filter_dirty;
            if (props_dirty)
            {
                adapter.RefreshDynamicProps(this->settings.attr_src);
                props_dirty = false;
            }
            if (filtered_outdated)
            {
                UpdateQuery();
            }
            filter_dirty = false;
        }

        void CopyStateFrom(const BookState &other)
        {
            settings = other.settings;
            query = other.query;
            filtered_skills = other.filtered_skills;
            feedback = other.feedback;
            scroll_tracking = other.scroll_tracking;
            clipper.SetScroll(other.clipper.GetTargetScroll());
            std::copy(other.input_text, other.input_text + 1024, input_text);
        }

        // Draws a button to duplicate the current book
        void DrawDupeButton()
        {
            auto window = ImGui::GetCurrentWindow();
            auto title_bar_height = window->TitleBarHeight();
            auto title_bar_rect = window->TitleBarRect();
            const auto button_padding = 0;
            auto button_side = title_bar_height - 2 * button_padding;
            auto button_size = ImVec2(button_side, button_side);
            auto saved_cursor = ImGui::GetCursorPos();
            auto button_pos = ImVec2(window->Size.x - button_size.x - 24, button_padding);

            ImGui::PushClipRect(title_bar_rect.Min, title_bar_rect.Max, false);
            ImGui::SetCursorPos(button_pos);
            if (ImGui::Button("##DupeButton", button_size))
            {
                auto book = AddBook();
                book->CopyStateFrom(*this);
                book->init_dims = {window->Pos + ImVec2(16, 16), window->Size};
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Duplicate book");
            }
            const auto center = window->Pos + button_pos + button_size / 2;
            const auto plus_radius = button_side * 0.3f;
            auto plus_color = IM_COL32_WHITE;
            const auto plus_thickness = 1.25f;
            window->DrawList->AddLine(center - ImVec2(plus_radius, 0), center + ImVec2(plus_radius, 0), plus_color, plus_thickness);
            window->DrawList->AddLine(center - ImVec2(0, plus_radius), center + ImVec2(0, plus_radius), plus_color, plus_thickness);
            ImGui::PopClipRect();

            ImGui::SetCursorPos(saved_cursor);
        }

        void DrawCheckboxes()
        {
            // if (ImGui::CollapsingHeader("Options"))
            {
                ImGui::Columns(2, nullptr, false);

                filter_dirty |= ImGui::Checkbox("Include PvE-only skills", &settings.include_pve_only_skills);
                filter_dirty |= ImGui::Checkbox("Include Temporary skills", &settings.include_temporary_skills);
                filter_dirty |= ImGui::Checkbox("Include NPC skills", &settings.include_npc_skills);
                filter_dirty |= ImGui::Checkbox("Include Archived skills", &settings.include_archived_skills);
                filter_dirty |= ImGui::Checkbox("Include Disguises", &settings.include_disguises);

                ImGui::NextColumn();

                ImGui::Checkbox("Show exact adrenaline", &settings.use_exact_adrenaline);
                // ImGui::Checkbox("Show null stats", &settings.show_null_stats);
                ImGui::Checkbox("Snap to skill", &settings.snap_to_skill);
                ImGui::Checkbox("Prefer concise descriptions", &settings.prefer_concise_descriptions);
                filter_dirty |= ImGui::Checkbox("Limit to character's professions", &settings.limit_to_characters_professions);

                ImGui::Columns(1);
            }
        }

        void DrawAttributeModeSelection()
        {
            auto cursor_before = ImGui::GetCursorPos();
            ImGui::SetCursorPosY(cursor_before.y + 2);
            ImGui::TextUnformatted("Attributes");
            ImGui::SameLine();
            ImGui::SetCursorPosY(cursor_before.y);

            if (ImGui::RadioButton("(0...15)", settings.IsGeneric()))
            {
                settings.attr_src.type = AttributeSource::Type::Constant;
                settings.attr_src.value = -1;
                props_dirty = true;
            }

            ImGui::SameLine();
            if (ImGui::RadioButton("Character's", settings.IsCharacters()))
            {
                settings.attr_src.type = AttributeSource::Type::FromAgent;
                props_dirty = true;
            }

            ImGui::SameLine();
            if (ImGui::RadioButton("Manual", settings.IsManual()))
            {
                settings.attr_src.type = AttributeSource::Type::Constant;
                settings.attr_src.value = settings.attr_lvl_slider;
                props_dirty = true;
            }

            if (settings.IsManual())
            {
                if (ImGui::SliderInt("Attribute level", &settings.attr_lvl_slider, 0, 21))
                {
                    settings.attr_src.value = settings.attr_lvl_slider;
                    props_dirty = true;
                }
            }
        }

        void InitFilterList()
        {
            uint32_t prof_mask;
            if (settings.limit_to_characters_professions)
            {
                prof_mask = Utils::GetProfessionMask(focused_agent_id);
            }

            filtered_skills.clear();
            for (auto skill_id_16 : base_skills)
            {
                const auto skill_id = static_cast<GW::Constants::SkillID>(skill_id_16);
                auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);

                if (!settings.include_pve_only_skills)
                {
                    if (custom_sd.tags.PvEOnly)
                        continue;
                }
                if (!settings.include_temporary_skills)
                {
                    if (custom_sd.tags.Temporary)
                        continue;
                }
                if (!settings.include_archived_skills)
                {
                    if (custom_sd.tags.Archived)
                        continue;
                }
                if (!settings.include_npc_skills)
                {
                    if (custom_sd.tags.MonsterSkill ||
                        custom_sd.tags.EnvironmentSkill)
                        continue;
                }
                if (!settings.include_disguises)
                {
                    if (custom_sd.skill->type == GW::Constants::SkillType::Disguise ||
                        custom_sd.skill_id == GW::Constants::SkillID::Tonic_Tipsiness)
                        continue;
                }

                if (settings.limit_to_characters_professions)
                {
                    auto skill_prof_mask = 1 << (uint32_t)custom_sd.skill->profession;
                    if ((prof_mask & skill_prof_mask) == 0)
                        continue;
                }

                filtered_skills.push_back(static_cast<uint16_t>(skill_id));
            }
        }

        void UpdateQuery()
        {
            auto input_text_view = std::string_view(input_text, strlen(input_text));
            scroll_tracking.UpdateScrollTracking(input_text_view, clipper);
            filter_device.ParseQuery(input_text_view, query);

            InitFilterList();

            this->filter_device.RunQuery(this->query, this->filtered_skills);

            // for (auto &command : parsed_commands)
            // {
            //     ApplyCommand(command, filtered_skills);
            // }

            filter_device.GetFeedback(query, feedback);

            // if (active_state->filtered_skills.size() > 0)
            // {
            //     // This is messy
            //     uint32_t max_count = std::min(active_state->filtered_skills.size(), filtered_skills.size());
            //     max_count = std::min(max_count, clipper.visible_index_start + 1);
            //     uint32_t same_count = 0;
            //     while (same_count < max_count &&
            //            filtered_skills[same_count] == active_state->filtered_skills[same_count])
            //         same_count++;

            //     if (same_count > 1)
            //         clipper.SetScrollToIndex(same_count - 1);
            //     else
            //         clipper.Reset();

            //     // auto focused_skill = state.filtered_skills[clipper.visible_index_start];
            //     // auto it = std::find(filtered_skills.begin(), filtered_skills.end(), focused_skill);
            //     // if (it != filtered_skills.end())
            //     // {
            //     //     auto index = std::distance(filtered_skills.begin(), it);
            //     //     clipper.SetScrollToIndex(index);
            //     // }
            //     // else
            //     // {
            //     //     clipper.Reset();
            //     // }
            // }
        }

        void DrawSkillStats(const GW::Skill &skill)
        {
            ImGui::BeginGroup();
            ImGui::PushFont(Constants::Fonts::skill_thick_font_15);

            const auto skill_id = skill.skill_id;

            const float width_per_stat = 45;

            const auto draw_list = ImGui::GetWindowDrawList();

            auto window = ImGui::GetCurrentWindow();
            auto work_width = window->WorkRect.GetWidth();

            float max_pos_x = work_width - 20;
            auto cursor_start_pos = ImGui::GetCursorPos();
            float min_pos_x = cursor_start_pos.x;
            float base_pos_y = cursor_start_pos.y + 1;

            ImGui::SetCursorPosY(base_pos_y);

            const auto icons = TextureModule::LoadTextureFromFileId(TextureModule::KnownFileIDs::UI_SkillStatsIcons);
            const ImVec2 icon_size = ImVec2(16, 16);

            struct Layout
            {
                SkillProp id;
                size_t atlas_index;
                size_t pos_from_right;
            };

            Layout layout[]{
                {SkillProp::Overcast, 10, 4},
                {SkillProp::Sacrifice, 7, 4},
                {SkillProp::Upkeep, 0, 4},
                {SkillProp::Energy, 1, 3},
                {SkillProp::Adrenaline, 3, 3},
                {SkillProp::Activation, 2, 2},
                {SkillProp::Recharge, 1, 1},
                {SkillProp::Aftercast, 2, 0},
            };

            for (const auto &l : layout)
            {
                auto r = filter_device.CalcItemResult(query, (size_t)l.id, (size_t)skill_id);
                if (r.text.empty())
                    continue;
                FixedVector<RichText::TextSegment, 16> segments;
                text_drawer.MakeTextSegments(r.text, segments, r.hl);
                const auto text_width = RichText::CalcTextSegmentsWidth(segments);
                float start_x = max_pos_x - l.pos_from_right * width_per_stat - text_width;
                float current_x = std::max(start_x, min_pos_x);
                const auto ls_text_cursor = ImVec2(current_x, base_pos_y + 2);
                ImGui::SetCursorPos(ls_text_cursor);

                text_drawer.DrawTextSegments(segments, 0, -1);

                current_x += text_width;
                min_pos_x = current_x + 5;
            }

            ImGui::PopFont();
            ImGui::EndGroup();
        }

        void DrawSkillHeader(CustomSkillData &custom_sd, bool is_equipable)
        {
            ImGui::BeginGroup(); // Whole header

            const auto &skill = *custom_sd.skill;
            const auto skill_id = custom_sd.skill_id;

            { // Draw skill icon
                const auto skill_icon_size = 56;
                auto icon_cursor_ss = ImGui::GetCursorScreenPos();
                bool is_hovered = ImGui::IsMouseHoveringRect(icon_cursor_ss, icon_cursor_ss + ImVec2(skill_icon_size, skill_icon_size));
                bool is_effect = custom_sd.tags.EffectOnly;
                if (TextureModule::DrawSkill(*custom_sd.skill, icon_cursor_ss, skill_icon_size, is_effect, is_hovered) ||
                    TextureModule::DrawSkill(*GW::SkillbarMgr::GetSkillConstantData(GW::Constants::SkillID::No_Skill), icon_cursor_ss, skill_icon_size, is_effect, is_hovered))
                {
                    if (is_hovered && is_equipable)
                    {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted("Click + Drag to equip");
                        ImGui::EndTooltip();
                    }

                    ImGui::SameLine();
                }
            }

            { // Draw stuff rightward of icon
                ImGui::BeginGroup();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7, 0));

                auto name_cursor_ss = ImGui::GetCursorScreenPos();
                auto wrapping_min = ImGui::GetCursorPosX();
                auto window = ImGui::GetCurrentWindow();
                auto wrapping_max = window->WorkRect.GetWidth();
                auto width = wrapping_max - wrapping_min;

                { // Draw skill name
                    ImGui::PushFont(Constants::Fonts::skill_name_font);
                    auto name_color = custom_sd.tags.Archived ? Constants::GWColors::skill_dull_gray : Constants::GWColors::header_beige;
                    ImGui::PushStyleColor(ImGuiCol_Text, name_color);

                    auto r = filter_device.CalcItemResult(query, (size_t)SkillProp::Name, (size_t)skill_id);
                    // auto name_hl = GetHL(SkillProp::Name, skill_id);
                    FixedVector<RichText::TextSegment, 32> segments;
                    text_drawer.MakeTextSegments(r.text, segments, r.hl);
                    text_drawer.DrawTextSegments(segments, wrapping_min, wrapping_max);

                    auto size = ImGui::GetItemRectSize();

                    { // Strikethrough and underscore handling
                        float strike_offsets_alloc[2];
                        float *strike_offsets = strike_offsets_alloc;
                        auto line_height = ImGui::GetTextLineHeight();
                        if (custom_sd.tags.Archived) // Strikethrough
                            *strike_offsets++ = line_height / 2;
                        if (ImGui::GetIO().KeyCtrl) // Underscore
                            *strike_offsets++ = line_height;

                        if (strike_offsets != strike_offsets_alloc)
                        {
                            auto y_pos = name_cursor_ss.y;
                            auto cumulative_width = 0.f;
                            for (size_t i = 0; i <= segments.size(); i++)
                            {
                                auto seg_width = i < segments.size() ? segments[i].width : std::numeric_limits<float>::max();
                                auto next_cumulative_width = cumulative_width + seg_width;
                                if (next_cumulative_width > width)
                                {
                                    for (float *p_offset = strike_offsets_alloc; p_offset < strike_offsets; p_offset++)
                                    {
                                        auto y = y_pos + *p_offset;
                                        ImGui::GetWindowDrawList()->AddLine(ImVec2(name_cursor_ss.x, y), ImVec2(name_cursor_ss.x + cumulative_width, y), name_color, 1.0f);
                                    }
                                    y_pos += line_height;
                                    cumulative_width = seg_width;
                                }
                                else
                                {
                                    cumulative_width = next_cumulative_width;
                                }
                            }
                        }
                    }

                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                }

                { // Draw skill stats
                    ImGui::SameLine();
                    DrawSkillStats(skill);
                }

                { // Draw skill type
                    auto r = filter_device.CalcItemResult(query, (size_t)SkillProp::Type, (size_t)skill_id);
                    // auto hl = GetHL(SkillProp::Type, skill_id);
                    text_drawer.DrawRichText(r.text, wrapping_min, wrapping_max, r.hl);
                }

                { // Draw skill tags
                    auto r = filter_device.CalcItemResult(query, (size_t)SkillProp::Tag, (size_t)skill_id);
                    // auto hl = GetHL(SkillProp::Tag, skill_id);
                    ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
                    text_drawer.DrawRichText(r.text, wrapping_min, wrapping_max, r.hl);
                    ImGui::PopStyleColor();
                }

                ImGui::PopStyleVar();
                ImGui::EndGroup();
            }

            ImGui::EndGroup();
        }

        void DrawFilterTooltip()
        {
            ImGui::BeginTooltip();

            auto width = 600;
            std::string_view text;

            { // General info

                text = "Here you can narrow down the results by using a filter.";
                Utils::DrawMultiColoredText(text, 0, width);

                text = "A filter requires a target, an operator and one or more values. "
                       "If you do not specify target or operator, the target will be 'Text', and the operator will be ':'. "
                       "This will essentially check if any text of the skill contains the provided string.";
                Utils::DrawMultiColoredText(text, 0, width);

                text = "Multiple values can be specified for a filter by separating them with '|', "
                       "then ANY of them must be satisfied for that filter to match.";
                Utils::DrawMultiColoredText(text, 0, width);

                text = "You can use '&' (AND) or '|' (OR) to combine multiple filters.";
                Utils::DrawMultiColoredText(text, 0, width);

                text = "Once you are satisified with the filters, you may want to sort the results. "
                       "This can be achieved by typing the special command \"#sort\" followed by a "
                       "whitespace-separated list of targets to sort by. The results will be sorted by the first target, "
                       "then by the second, and so on. To invert the order prepend a '!' character before the target";
                Utils::DrawMultiColoredText(text, 0, width);

                text = "If you are unsure how to proceed, take a look at the examples below.";
                Utils::DrawMultiColoredText(text, 0, width);
            }

            ImGui::Spacing();

            auto Example = [&](std::string_view filter, std::string_view desc)
            {
                ImGui::Text(" - \"%s\"", filter.data());
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
                ImGui::Text("(%s)", desc.data());
                ImGui::PopStyleColor();
            };
            { // Examples

                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
                ImGui::TextUnformatted("Example filters:");
                ImGui::PopStyleColor();
                ImGui::TextUnformatted("Try these examples by typing them in the search bar.");

                Example("health regen", "Show health regeneration skills");
                Example("camp = night & ava = locked/unlocked", "Show Nightfall skills you have yet to learn");
                Example("prof = mo/d & ty: enc", "Show monk or dervish enchantment spells");
                Example("adr > 0 & ty = stance #sort !adr nam", "Show adrenal stances, sort by cost and then name");
            }

            ImGui::Spacing();

            FixedVector<char, 128> char_buffer;
            FixedVector<Utils::ColorChange, 8> col_buffer;
            FixedVector<uint32_t, 8> boundaries;
            ImU32 req_color = IM_COL32(0, 255, 0, 255);

            { // Target and operator info
                ImGui::Columns(3, "mycolumns", false);

                auto DrawTargets = [&](std::string_view type, const auto &target_array)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
                    ImGui::Text("%s targets:", type);
                    ImGui::PopStyleColor();
                    uint32_t i = 0;
                    for (auto &[_, target_type] : target_array)
                    {
                        if (i++ == 25)
                            ImGui::NextColumn();
                        auto ident = SkillPropertyID{target_type, 0}.ToStr();
                        // bool success = CalcIdentMinChars(target_type, ident, char_buffer, boundaries);
                        // assert(success);

                        // assert(boundaries.size() % 2 == 0);
                        // col_buffer.clear();
                        // for (int32_t i = 0; i < boundaries.size(); i += 2)
                        // {
                        //     col_buffer.try_push({boundaries[i] + 2, req_color});
                        //     col_buffer.try_push({boundaries[i + 1] + 2, 0});
                        // }

                        char_buffer.clear();
                        char_buffer.PushFormat("- %s", ident.data());
                        Utils::DrawMultiColoredText(char_buffer, 0, width);
                    }
                };

                auto DrawOperators = [&](std::string_view type, const auto &op_array, SkillPropertyID::Type prop_type)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
                    ImGui::Text("%s operators:", type);
                    ImGui::PopStyleColor();
                    for (auto &[ident, op] : op_array)
                    {
                        const auto desc = GetOpDescription({prop_type, 0}, op);
                        char_buffer.clear();
                        char_buffer.PushFormat("- %s %s", ident.data(), desc.data());

                        col_buffer.clear();
                        col_buffer.try_push({2, req_color});
                        col_buffer.try_push({2 + ident.size(), 0});
                        Utils::DrawMultiColoredText(char_buffer, 0, width, col_buffer);
                    }
                };

                // DrawTargets("Text", text_filter_targets);
                ImGui::Spacing();
                // DrawOperators("Text", text_filter_operators, SkillPropertyID::Type::TEXT);
                ImGui::Spacing();
                // DrawOperators("Number", number_filter_operators, SkillPropertyID::Type::NUMBER);

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
                text = "The highlighted characters indicate the minimum required to uniquely identify it.";
                Utils::DrawMultiColoredText(text, 0, width / 3);
                ImGui::PopStyleColor();

                ImGui::NextColumn();
                // DrawTargets("Number", number_filter_targets);

                ImGui::Columns(1);
            }

            { // Advanced info
                ImGui::Spacing();

                if (!ImGui::GetIO().KeyCtrl)
                {
                    ImGui::TextUnformatted("Hold CTRL to show advanced info");
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
                    ImGui::TextUnformatted("Advanced:");
                    ImGui::PopStyleColor();

                    ImGui::TextUnformatted("For advanced users it's also possible to target specific bytes in the \"raw\" skill data\nstructure. "
                                           "This can be useful for targeting specific flags or values that are not directly\nexposed.");

                    ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
                    ImGui::TextUnformatted("Example raw filter:");
                    ImGui::PopStyleColor();
                    Example("0x10 u32 : 4", "Show skills with the 3rd bit set in the u32 at offset 0x10");

                    ImGui::Columns(2, "mycolumns", false);
                    ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
                    ImGui::TextUnformatted("Raw target:");
                    ImGui::PopStyleColor();

                    ImGui::TextUnformatted("A raw target consists a hex byte offset\nand the type of value to read at that offset.");
                    ImGui::TextUnformatted("- ");
                    ImGui::PushStyleColor(ImGuiCol_Text, req_color);
                    ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted("0x");
                    ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::hp_red);
                    ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted("<HEX_OFFSET> ");
                    ImGui::PopStyleColor(2);
                    ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted("[u8|u16|u32|f32]");

                    ImGui::NextColumn();

                    ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
                    ImGui::TextUnformatted("Raw operators:");
                    ImGui::PopStyleColor();

                    ImGui::TextUnformatted("Any number operator OR:");
                    // for (auto &[ident, op] : adv_filter_operators)
                    // {
                    //     const auto desc = GetOpDescription(SkillPropertyID{SkillPropertyID::Type::RAW, 0}, op);
                    //     char_buffer.clear();
                    //     char_buffer.PushFormat("- %s %s", ident.data(), desc.data());
                    //     col_buffer.try_push({2, req_color});
                    //     col_buffer.try_push({2 + ident.size(), 0});
                    //     Utils::DrawMultiColoredText(char_buffer, 0, width, col_buffer);
                    // }
                }
            }

            ImGui::EndTooltip();
        }

        void DrawSearchBox()
        {
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();

            ImGui::InputTextWithHint(
                "",
                "Search for a skill...",
                input_text, sizeof(input_text),
                ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_AutoSelectAll,
                [](ImGuiInputTextCallbackData *data)
                {
                    auto &book = *static_cast<BookState *>(data->UserData);
                    book.filter_dirty = true;

                    return 0;
                },
                this
            );

            ImGui::SameLine();
            auto pos = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y - 1));
            ImGui::TextUnformatted("(?)");
            if (ImGui::IsItemHovered())
            {
                DrawFilterTooltip();
            }

            // if (state_update_start_timestamp)
            // {
            //     DWORD now_timestamp = GW::MemoryMgr::GetSkillTimer();
            //     float elapsed_sec = (float)(now_timestamp - state_update_start_timestamp) / 1000.0f;

            //     if (elapsed_sec > 0.2f)
            //     {
            //         // Draw spinning arrow
            //         auto packet = TextureModule::GetPacket_ImageInAtlas(
            //             TextureModule::KnownFileIDs::UI_SkillStatsIcons,
            //             ImVec2(23, 23),
            //             ImVec2(16, 16),
            //             3,
            //             ImVec2(0, 0),
            //             ImVec2(0, 0),
            //             ImColor(0.95f, 0.6f, 0.2f)
            //         );
            //         auto rads = 2.f * elapsed_sec * 6.28f;
            //         auto draw_list = ImGui::GetWindowDrawList();
            //         packet.AddToDrawList(draw_list, ImVec2(pos.x + 20, pos.y), rads);
            //     }
            // }

            if (!feedback.empty())
            {
                auto window = ImGui::GetCurrentWindow();
                auto wrapping_max = window->WorkRect.GetWidth();
                text_drawer.DrawRichText(feedback, 0, wrapping_max);
            }
        }

        void DrawDescription(GW::Constants::SkillID skill_id, float work_width)
        {
            auto tt_provider = SkillTooltipProvider(skill_id);
            text_drawer.tooltip_provider = &tt_provider;

            auto type = settings.prefer_concise_descriptions ? SkillProp::Concise : SkillProp::Description;
            auto alt_type = settings.prefer_concise_descriptions ? SkillProp::Description : SkillProp::Concise;

            auto main_r = filter_device.CalcItemResult(query, (size_t)type, (size_t)skill_id);
            auto alt_r = filter_device.CalcItemResult(query, (size_t)alt_type, (size_t)skill_id);

            text_drawer.DrawRichText(main_r.text, 0, work_width, main_r.hl);

            bool draw_alt = alt_r.hl.size() > main_r.hl.size();
            if (!draw_alt)
            {
                // auto len = std::min(main_hl.size(), alt_hl.size());
                // assert(len % 2 == 0);
                // uint32_t i = 0;
                // while (i < len)
                // {
                //     std::string_view hl_a = ((std::string_view)desc.str).substr(main_hl[i], main_hl[i + 1] - main_hl[i]);
                //     std::string_view hl_b = ((std::string_view)desc_alt.str).substr(alt_hl[i], alt_hl[i + 1] - alt_hl[i]);
                //     i += 2;
                //     bool eq = (std::tolower(hl_a[0]) == std::tolower(hl_b[0])) &&
                //               (hl_a.substr(1) == hl_b.substr(1));
                //     if (!eq)
                //     {
                //         draw_alt = true;
                //         break;
                //     }
                // }
            }

            if (draw_alt)
            {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
                ImGui::TextUnformatted(settings.prefer_concise_descriptions ? "Additional matches in full description: " : "Additional matches in concise description: ");
                ImGui::PopStyleColor();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3);
                // ImGui::SameLine(0, 0);
                // auto wrapping_min = ImGui::GetCursorPosX();

                text_drawer.DrawRichText(alt_r.text, 0, work_width, alt_r.hl);
            }
        }

        void DrawSkillFooter(CustomSkillData &custom_sd, float work_width)
        {
            auto &skill = *custom_sd.skill;
            auto skill_id = custom_sd.skill_id;
            ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            auto &section = adapter.ts.footer_meta_section;
            for (auto i_meta = section.start_index; i_meta < section.end_index; ++i_meta)
            {
                auto meta_propset = adapter.GetMetaPropset(i_meta);
                bool has_drawn_header = false;
                for (auto prop : meta_propset.IterSetBits())
                {
                    auto r = filter_device.CalcItemResult(query, (size_t)prop, (size_t)skill_id);
                    if (r.text.empty())
                        continue;

                    if (!has_drawn_header)
                    {
                        auto r = filter_device.CalcMetaResult(query, i_meta);
                        // auto meta_hl = GetMetaHL(i_meta);
                        text_drawer.DrawRichText(r.text, 0, work_width, r.hl);
                        ImGui::SameLine();
                        text_drawer.DrawRichText(": ", 0, work_width);
                        ImGui::SameLine();
                        has_drawn_header = true;
                    }
                    // auto str_hl = GetHL(prop, skill_id);
                    text_drawer.DrawRichText(r.text, 0, work_width, r.hl);
                }
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            { // Draw skill id
                ImGui::SetWindowFontScale(0.7f);
                auto r = filter_device.CalcItemResult(query, (size_t)SkillProp::Id, (size_t)skill_id);
                const auto id_str_size = ImGui::CalcTextSize(r.text.data(), r.text.data() + r.text.size());
                ImGui::SetCursorPosX(work_width - id_str_size.x - 4);
                ImVec4 color(1, 1, 1, 0.3f);
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                text_drawer.DrawRichText(r.text, 0, work_width, r.hl);
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(1.f);
            }
        }

        void MakeBookName(OutBuf<char> name, size_t book_index)
        {
            name.AppendRange(std::string_view("Skill Book (Ctrl + K)"));
            if (book_index > 0)
            {
                name.AppendFormat(" ({})", book_index);
            }

            // Add unique id: Hex-string of pointer to BookState
            name.AppendRange(std::string_view("###"));
            name.AppendIntToChars(reinterpret_cast<size_t>(this), 16);

            name.push_back('\0');
        }

        void Draw(IDirect3DDevice9 *device, size_t book_index)
        {
            if (first_draw)
            {
                ImVec2 pos, size;
                if (init_dims.has_value())
                {
                    auto &init_dims_value = init_dims.value();
                    pos = init_dims_value.window_pos;
                    size = init_dims_value.window_size;
                }
                else
                {
                    auto vpw = GW::Render::GetViewportWidth();
                    auto vph = GW::Render::GetViewportHeight();
                    const auto window_width = 600;
                    const auto offset_from_top = ImGui::GetFrameHeight();
                    pos = ImVec2(vpw - window_width, offset_from_top);
                    size = ImVec2(window_width, vph - offset_from_top);
                }
                ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
                first_draw = false;
            }

            FixedVector<char, 64> name;
            MakeBookName(name, book_index);

            if (ImGui::Begin(name.data(), &is_opened))
            {
                DrawDupeButton();
                DrawCheckboxes();
                DrawAttributeModeSelection();
                DrawSearchBox();

                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
                auto n_skills = filtered_skills.size();
                ImGui::Text("%s results", std::to_string(n_skills).c_str());
                ImGui::PopStyleColor();
                ImGui::Separator();

                if (ImGui::BeginChild("SkillList"))
                {
                    auto est_item_height = 128.f;
                    // auto est_item_height = 1.f;

                    auto draw_list = ImGui::GetWindowDrawList();

                    auto instance_type = GW::Map::GetInstanceType();

                    auto DrawItem = [&](uint32_t i)
                    {
                        const auto skill_id = static_cast<GW::Constants::SkillID>(filtered_skills[i]);
                        auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);

                        const auto initial_screen_cursor = ImGui::GetCursorScreenPos();
                        const auto initial_cursor = ImGui::GetCursorPos();
                        const auto window = ImGui::GetCurrentWindow();
                        const auto work_width = window->WorkRect.GetWidth();

                        bool is_learned = Utils::IsSkillLearned(*custom_sd.skill, focused_agent_id);
                        bool is_equipable = is_learned && instance_type == GW::Constants::InstanceType::Outpost;

                        DrawSkillHeader(custom_sd, is_equipable);
                        DrawDescription(skill_id, work_width);
                        ImGui::Spacing();
                        DrawSkillFooter(custom_sd, work_width);
                        auto final_screen_cursor = ImGui::GetCursorScreenPos();

                        auto entry_screen_max = final_screen_cursor;
                        entry_screen_max.x += work_width;

                        if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(initial_screen_cursor, entry_screen_max))
                        {
                            bool ctrl_down = ImGui::GetIO().KeyCtrl;
                            if (ctrl_down)
                            {
                                ImGui::BeginTooltip();
                                if (IsInitialCursorPos())
                                {
                                    ImGui::TextUnformatted("Ctrl + Click to open wiki!");
                                }
                                ImGui::EndTooltip();
                            }

                            if (ImGui::IsMouseClicked(0))
                            {
                                if (ctrl_down)
                                {
                                    auto r = filter_device.CalcItemResult(query, (size_t)SkillProp::Name, (size_t)skill_id);
                                    Utils::OpenWikiPage(r.text);
                                }
                                else
                                {
                                    if (is_equipable)
                                    {
                                        RequestSkillDragging(skill_id);
                                    }
                                }
                            }
#ifdef _DEBUG
                            Debug::SetHoveredSkill(skill_id);
#endif
                        }

                        ImGui::Separator();
                    };

                    clipper.Draw(n_skills, est_item_height, settings.snap_to_skill, DrawItem);
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
    };

    GW::HookEntry attribute_update_entry;
    GW::HookEntry select_hero_entry;

    void AttributeUpdatedCallback(GW::HookStatus *, const GW::Packet::StoC::PacketBase *packet)
    {
        const auto p = reinterpret_cast<const GW::Packet::StoC::AttributeUpdatePacket *>(packet);
        auto packet_agent_id = p->agent_id;

        if (packet_agent_id == focused_agent_id)
        {
            for (auto &book : books)
            {
                if (book->settings.IsCharacters())
                {
                    // auto attr = AttributeOrTitle((GW::Constants::AttributeByte)p->attribute);
                    // attributes[attr.value] = p->value;
                    book->props_dirty = true;
                    // book->FetchDescriptions();
                }
            }
        }
    }
    void TitleUpdateCallback(GW::HookStatus *, const GW::Packet::StoC::UpdateTitle *packet)
    {
        for (auto &book : books)
        {
            if (book->settings.IsCharacters())
            {
                // auto attr = AttributeOrTitle((GW::Constants::TitleID)packet->title_id);
                // attributes[attr.value] = packet->new_value;
                book->props_dirty = true;
            }
        }
    }

    void LoadAgentSkillsCallback(GW::HookStatus *, GW::UI::UIMessage msg, void *wparam, void *lparam)
    {
        // This is called whenever a new set of skills are loaded in the deckbuilder

        auto agent_id = (uint32_t)wparam;
        if (agent_id > 0)
        {
            focused_agent_id = agent_id;
            for (auto &book : books)
            {
                book->props_dirty = true;
            }
        }
    }

    void SelectHeroCallback(GW::HookStatus *, GW::UI::UIMessage msg, void *wparam, void *lparam)
    {
        // This is called whenever the user selects a hero in either the deckbuilder or the inventory,
        // but it doesn't come with any params/info

        auto db = GW::UI::GetFrameByLabel(L"DeckBuilder");
        if (!db)
        {
            // At this point, the inventory must be open
            GW::GameThread::Enqueue(
                [&]()
                {
                    // Open and close the deckbuilder to trigger LoadHeroSkillsCallback where we get the agent_id
                    GW::UI::Keypress(GW::UI::ControlAction_OpenSkillsAndAttributes);
                    GW::UI::Keypress(GW::UI::ControlAction_OpenSkillsAndAttributes);
                }
            );
        }
    }

    void MapLoadedCallback(GW::HookStatus *, GW::UI::UIMessage msg, void *wparam, void *lparam)
    {
        // This is to keep it in sync with gw
        focused_agent_id = GW::Agents::GetControlledCharacterId();
        for (auto &book : books)
        {
            book->props_dirty = true;
        }
    }

    void ProfessionUpdatedCallback(GW::HookStatus *, GW::UI::UIMessage msg, void *wparam, void *lparam)
    {
        auto p = (GW::UI::UIPacket::kProfessionUpdated *)wparam;
        if (p->agent_id == focused_agent_id)
        {
            for (auto &book : books)
            {
                book->props_dirty = true;
            }
        }
    }

    void ForceDeckbuilderCallbacks()
    {
        auto Refresh_DB = [&]()
        {
            GW::UI::Keypress(GW::UI::ControlAction_OpenSkillsAndAttributes);
            GW::UI::Keypress(GW::UI::ControlAction_OpenSkillsAndAttributes);
        };

        auto inv = GW::UI::GetFrameByLabel(L"Inventory");
        if (inv) // Inventory open
        {
            GW::GameThread::Enqueue(Refresh_DB);
        }
        else // Inventory closed
        {
            auto db = GW::UI::GetFrameByLabel(L"DeckBuilder");
            if (db) // Deckbuilder open
            {
                GW::GameThread::Enqueue(
                    [&]()
                    {
                        GW::UI::Keypress(GW::UI::ControlAction_ToggleInventoryWindow);
                        GW::UI::Keypress(GW::UI::ControlAction_OpenSkillsAndAttributes);
                        GW::UI::Keypress(GW::UI::ControlAction_OpenSkillsAndAttributes);
                        GW::UI::Keypress(GW::UI::ControlAction_ToggleInventoryWindow);
                    }
                );
            }
            else // Deckbuilder closed
            {
                GW::GameThread::Enqueue(Refresh_DB);
            }
        }
    }

    void Enable()
    {
        GW::StoC::RegisterPacketCallback(&attribute_update_entry, GAME_SMSG_AGENT_UPDATE_ATTRIBUTE, AttributeUpdatedCallback);
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::UpdateTitle>(&attribute_update_entry, TitleUpdateCallback);

        GW::UI::RegisterUIMessageCallback(&select_hero_entry, GW::UI::UIMessage::kLoadAgentSkills, LoadAgentSkillsCallback);
        GW::UI::RegisterUIMessageCallback(&select_hero_entry, GW::UI::UIMessage::kSelectHeroPane, SelectHeroCallback);
        GW::UI::RegisterUIMessageCallback(&select_hero_entry, GW::UI::UIMessage::kMapLoaded, MapLoadedCallback);
        GW::UI::RegisterUIMessageCallback(&select_hero_entry, GW::UI::UIMessage::kProfessionUpdated, ProfessionUpdatedCallback);
    }

    void Disable()
    {
        GW::StoC::RemoveCallbacks(&attribute_update_entry);

        GW::UI::RemoveUIMessageCallback(&select_hero_entry, GW::UI::UIMessage::kLoadAgentSkills);
        GW::UI::RemoveUIMessageCallback(&select_hero_entry, GW::UI::UIMessage::kSelectHeroPane);
        GW::UI::RemoveUIMessageCallback(&select_hero_entry, GW::UI::UIMessage::kMapLoaded);
        GW::UI::RemoveUIMessageCallback(&select_hero_entry, GW::UI::UIMessage::kProfessionUpdated);
    }

    void Initialize()
    {
        Enable();

        ForceDeckbuilderCallbacks();

        text_storage.Initialize();
        AddBook(); // Add first book
    }

    void Terminate()
    {
        CancelSkillDragging();

        Disable();
    }

    void FormatActivation(char *buffer, uint32_t len, float value)
    {
        float intpart;
        float activation_fract = std::modf(value, &intpart);
        uint32_t activation_int = static_cast<uint32_t>(intpart);

        const auto thresh = 0.001f;
        const char *str = nullptr;
        if (((std::abs(activation_fract - 0.25f)) < thresh && (str = "¼")) ||
            ((std::abs(activation_fract - 0.5f)) < thresh && (str = "½")) ||
            ((std::abs(activation_fract - 0.75f)) < thresh && (str = "¾")))
        {
            if (activation_int)
            {
                auto written = snprintf(buffer, len, "%d", activation_int);
                assert(written >= 0 && written < len);
                buffer += written;
                len -= written;
            }

            auto result = snprintf(buffer, len, str);
            assert(result >= 0 && result < len);
        }
        else
        {
            auto result = snprintf(buffer, len, "%g", value);
            assert(result >= 0 && result < len);
        }
    }

    enum struct FilterJoin
    {
        None,
        And,
        Or,
    };

    enum struct FilterOperator
    {
        None,
        Unknown,

        Contain,
        NotContain,
        GreaterThanOrEqual,
        LessThanOrEqual,
        NotEqual,
        Equal,
        GreaterThan,
        LessThan,
    };

    struct SortCommandArg
    {
        bool is_negated;
        SkillPropertyID target;
    };

    struct SortCommand
    {
        std::vector<SortCommandArg> args;
    };

    struct Command
    {
        std::variant<SortCommand> data;
    };

    // TODO: Remove this! We will implement it later
    // Must not contain any whitespace
    static constexpr uint32_t n_text_filter_targets = 7; // Must match the array below
    static constexpr std::pair<std::string_view, SkillPropertyID::Type> filter_targets[] = {
        // Text

        // {"Text", SkillPropertyID::Type::TEXT},
        {"Name", SkillPropertyID::Type::Name},
        {"Type", SkillPropertyID::Type::SkillType},
        {"Tag", SkillPropertyID::Type::TAGS},
        {"Description", SkillPropertyID::Type::Description},
        {"Attribute", SkillPropertyID::Type::Attribute},
        {"Profession", SkillPropertyID::Type::Profession},
        {"Campaign", SkillPropertyID::Type::Campaign},

        // Number

        {"Upkeep", SkillPropertyID::Type::Upkeep},
        {"Overcast", SkillPropertyID::Type::Overcast},
        {"Adrenaline", SkillPropertyID::Type::Adrenaline},
        {"Sacrifice", SkillPropertyID::Type::Sacrifice},
        {"Energy", SkillPropertyID::Type::Energy},
        {"Activation", SkillPropertyID::Type::Activation},
        {"Recharge", SkillPropertyID::Type::Recharge},
        {"Aftercast", SkillPropertyID::Type::Aftercast},
        {"Range", SkillPropertyID::Type::Range},
        {"ID", SkillPropertyID::Type::ID},

        {"Duration", ParsedToProp(ParsedSkillData::Type::Duration)},
        {"Disable", ParsedToProp(ParsedSkillData::Type::Disable)},
        {"Level", ParsedToProp(ParsedSkillData::Type::Level)},
        {"Damage", ParsedToProp(ParsedSkillData::Type::Damage)},
        {"Healing", ParsedToProp(ParsedSkillData::Type::Heal)},
        {"ArmorBonus", ParsedToProp(ParsedSkillData::Type::ArmorIncrease)},
        {"ArmorPenalty", ParsedToProp(ParsedSkillData::Type::ArmorDecrease)},

        {"Conditions Removed", ParsedToProp(ParsedSkillData::Type::ConditionsRemoved)},
        {"Hexes Removed", ParsedToProp(ParsedSkillData::Type::HexesRemoved)},
        {"Enchantments Removed", ParsedToProp(ParsedSkillData::Type::EnchantmentsRemoved)},

        {"Health Regeneration", ParsedToProp(ParsedSkillData::Type::HealthRegen)},
        {"Health Degeneration", ParsedToProp(ParsedSkillData::Type::HealthDegen)},
        {"Health Gain", ParsedToProp(ParsedSkillData::Type::HealthGain)},
        {"Health Loss", ParsedToProp(ParsedSkillData::Type::HealthLoss)},
        {"Health Steal", ParsedToProp(ParsedSkillData::Type::HealthSteal)},
        {"Additional Health", ParsedToProp(ParsedSkillData::Type::AdditionalHealth)},

        {"Energy Regeneration", ParsedToProp(ParsedSkillData::Type::EnergyRegen)},
        {"Energy Degeneration", ParsedToProp(ParsedSkillData::Type::EnergyDegen)},
        {"Energy Gain", ParsedToProp(ParsedSkillData::Type::EnergyGain)},
        {"Energy Loss", ParsedToProp(ParsedSkillData::Type::EnergyLoss)},
        {"Energy Steal", ParsedToProp(ParsedSkillData::Type::EnergySteal)},
        {"Energy Discount", ParsedToProp(ParsedSkillData::Type::EnergyDiscount)},

        {"Adrenaline Gain", ParsedToProp(ParsedSkillData::Type::AdrenalineGain)},
        {"Adrenaline Loss", ParsedToProp(ParsedSkillData::Type::AdrenalineLoss)},

        {"Bleeding", ParsedToProp(ParsedSkillData::Type::Bleeding)},
        {"Blind", ParsedToProp(ParsedSkillData::Type::Blind)},
        {"Burning", ParsedToProp(ParsedSkillData::Type::Burning)},
        {"Cracked Armor", ParsedToProp(ParsedSkillData::Type::CrackedArmor)},
        {"Crippled", ParsedToProp(ParsedSkillData::Type::Crippled)},
        {"Dazed", ParsedToProp(ParsedSkillData::Type::Dazed)},
        {"Deep Wound", ParsedToProp(ParsedSkillData::Type::DeepWound)},
        {"Disease", ParsedToProp(ParsedSkillData::Type::Disease)},
        {"Poison", ParsedToProp(ParsedSkillData::Type::Poison)},
        {"Weakness", ParsedToProp(ParsedSkillData::Type::Weakness)},

        {"Faster Activation", ParsedToProp(ParsedSkillData::Type::FasterActivation)},
        {"Slower Activation", ParsedToProp(ParsedSkillData::Type::SlowerActivation)},
        {"Faster Movement", ParsedToProp(ParsedSkillData::Type::FasterMovement)},
        {"Slower Movement", ParsedToProp(ParsedSkillData::Type::SlowerMovement)},
        {"Faster Recharge", ParsedToProp(ParsedSkillData::Type::FasterRecharge)},
        {"Slower Recharge", ParsedToProp(ParsedSkillData::Type::SlowerRecharge)},
        {"Faster Attacks", ParsedToProp(ParsedSkillData::Type::FasterAttacks)},
        {"Slower Attacks", ParsedToProp(ParsedSkillData::Type::SlowerAttacks)},
        {"Longer Duration", ParsedToProp(ParsedSkillData::Type::LongerDuration)},
        {"Shorter Duration", ParsedToProp(ParsedSkillData::Type::ShorterDuration)},
        {"More Damage", ParsedToProp(ParsedSkillData::Type::MoreDamage)},
        {"Less Damage", ParsedToProp(ParsedSkillData::Type::LessDamage)},
        {"More Healing", ParsedToProp(ParsedSkillData::Type::MoreHealing)},
        {"Less Healing", ParsedToProp(ParsedSkillData::Type::LessHealing)},
    };

    static constexpr std::span<const std::pair<std::string_view, SkillPropertyID::Type>> text_filter_targets = {filter_targets, n_text_filter_targets};
    static constexpr std::span<const std::pair<std::string_view, SkillPropertyID::Type>> number_filter_targets = {filter_targets + n_text_filter_targets, std::size(filter_targets) - n_text_filter_targets};

    std::string_view GetOpDescription(SkillPropertyID target, FilterOperator op)
    {
        // clang-format off
        switch (op) {
            case FilterOperator::Contain:            return target.IsRawNumberType() ? "must contain bitflags of" : "must contain";
            case FilterOperator::NotContain:         return target.IsRawNumberType() ? "must NOT contain bitflags of" : "must NOT contain";
            case FilterOperator::Equal:              return target.IsStringType() ? "must start with" : "must be equal to";
            case FilterOperator::NotEqual:           return target.IsStringType() ? "must NOT start with" : "must NOT be equal to";
            case FilterOperator::GreaterThanOrEqual: return "must be greater than or equal to";
            case FilterOperator::GreaterThan:        return "must be greater than";
            case FilterOperator::LessThanOrEqual:    return "must be less than or equal to";
            case FilterOperator::LessThan:           return "must be less than";
            default:                                 return "...";
        }
        // clang-format on
    }

    // union SortProp
    // {
    //     char str[16];
    //     double num[2];

    //     SortProp() = default;
    //     SortProp(SkillPropertyID id, CustomSkillData &custom_sd)
    //     {
    //         FixedVector<SkillProperty, 8> salloc;
    //         auto props = salloc.ref();
    //         GetSkillProperty(id, custom_sd, props);

    //         if (id.IsStringType())
    //         {
    //             if (props.size() > 0)
    //             {
    //                 auto str_view = props[0].GetStr();
    //                 std::strncpy(str, str_view.data(), sizeof(str));
    //             }
    //             else
    //             {
    //                 std::memset(str, 0, sizeof(str));
    //             }
    //         }
    //         else
    //         {
    //             num[0] = props.size() > 0 ? props[0].GetNumber() : 0;
    //             num[1] = num[0];
    //             for (uint32_t i = 1; i < props.size(); i++)
    //             {
    //                 auto val = props[i].GetNumber();
    //                 num[0] = std::min(num[0], val);
    //                 num[1] = std::max(num[1], val);
    //             }
    //         }
    //     }

    //     bool ContainsFullString() const
    //     {
    //         return str[sizeof(str) - 1] == '\0';
    //     }

    //     int32_t CompareStr(const SortProp &other) const
    //     {
    //         return std::strncmp(str, other.str, sizeof(str));
    //     }

    //     int32_t CompareNumAscending(SortProp &other) const
    //     {
    //         if (num[0] != other.num[0])
    //             return num[0] < other.num[0] ? -1 : 1;
    //         if (num[1] != other.num[1])
    //             return num[1] < other.num[1] ? -1 : 1;
    //         return 0;
    //     }

    //     int32_t CompareNumDescending(SortProp &other) const
    //     {
    //         if (num[1] != other.num[1])
    //             return num[1] < other.num[1] ? 1 : -1;
    //         if (num[0] != other.num[0])
    //             return num[0] < other.num[0] ? 1 : -1;
    //         return 0;
    //     }
    // };

    // void ApplyCommand(Command &command, std::span<uint16_t> filtered_skills)
    // {
    //     if (std::holds_alternative<SortCommand>(command.data))
    //     {
    //         // Sorting may be slow on debug builds but should be super fast on release builds
    //         // (I experienced 110 ms in debug build and 4 ms in release build for the same sorting operation)

    //         auto &sort_command = std::get<SortCommand>(command.data);
    //         if (sort_command.args.empty())
    //             return;

    //         // Prefetch some data to speed up sorting (this might actually not be needed for release builds)
    //         auto prefetched = std::array<SortProp, GW::Constants::SkillMax>{};
    //         auto first_target = sort_command.args[0].target;
    //         for (auto skill_id : filtered_skills)
    //         {
    //             auto &custom_sd = CustomSkillDataModule::GetCustomSkillData((GW::Constants::SkillID)skill_id);
    //             prefetched[skill_id] = SortProp(first_target, custom_sd);
    //         }

    //         auto Comparer = [&](const uint16_t a, const uint16_t b)
    //         {
    //             for (uint32_t i = 0; i < sort_command.args.size(); i++)
    //             {
    //                 auto &arg = sort_command.args[i];
    //                 auto target = arg.target;
    //                 bool is_negated = arg.is_negated;

    //                 auto &custom_sd_a = CustomSkillDataModule::GetCustomSkillData((GW::Constants::SkillID)a);
    //                 auto &custom_sd_b = CustomSkillDataModule::GetCustomSkillData((GW::Constants::SkillID)b);

    //                 bool is_string = target.IsStringType();
    //                 int32_t cmp = 0;
    //                 if (is_string)
    //                 {
    //                     if (i == 0)
    //                     {
    //                         // Do a cheap check on first 16 chars first
    //                         auto &prop_a = prefetched[a];
    //                         auto &prop_b = prefetched[b];
    //                         cmp = prop_a.CompareStr(prop_b);
    //                         if (cmp != 0)
    //                             return is_negated ? cmp > 0 : cmp < 0;
    //                         if (prop_a.ContainsFullString() && prop_b.ContainsFullString())
    //                             continue;
    //                     }

    //                     constexpr auto buffer_size = 8;
    //                     FixedVector<SkillProperty, buffer_size> salloc_a;
    //                     FixedVector<SkillProperty, buffer_size> salloc_b;
    //                     auto buffer_a = salloc_a.ref();
    //                     auto buffer_b = salloc_b.ref();
    //                     GetSkillProperty(target, custom_sd_a, buffer_a);
    //                     GetSkillProperty(target, custom_sd_b, buffer_b);

    //                     assert(buffer_a.size() == buffer_b.size());
    //                     for (uint32_t i = 0; i < buffer_a.size(); i++)
    //                     {
    //                         auto str_a = buffer_a[i].GetStr();
    //                         auto str_b = buffer_b[i].GetStr();

    //                         cmp = str_a.compare(str_b);
    //                         if (cmp != 0)
    //                             return is_negated ? cmp > 0 : cmp < 0;
    //                     }
    //                 }
    //                 else
    //                 {
    //                     auto prop_a = i == 0 ? prefetched[a] : SortProp(target, custom_sd_a);
    //                     auto prop_b = i == 0 ? prefetched[b] : SortProp(target, custom_sd_b);
    //                     cmp = is_negated ? prop_a.CompareNumDescending(prop_b)
    //                                      : prop_a.CompareNumAscending(prop_b);
    //                     if (cmp != 0)
    //                         return cmp < 0;
    //                 }
    //             }
    //             return false;
    //         };
    //         std::stable_sort(filtered_skills.begin(), filtered_skills.end(), Comparer);
    //     }
    //     else
    //     {
    //         SOFT_ASSERT(false, L"Invalid command type");
    //     }
    // }

    void CloseBooks()
    {
        bool all_closed = true;
        for (auto &book : books)
        {
            bool is_closed = !book->is_opened;
            all_closed &= is_closed;
        }

        auto it = std::remove_if(
            books.begin() + all_closed, books.end(),
            [](const std::unique_ptr<BookState> &book)
            {
                return !book->is_opened;
            }
        );
        books.erase(it, books.end());

        if (all_closed)
        {
            books[0]->is_opened = true;
            UpdateManager::open_skill_book = false;
        }
    }

    void Update()
    {
#ifdef _DEBUG
        DebugDisplay::PushToDisplay("Skill Book selected agent_id", focused_agent_id);
#endif

        CloseBooks();

        for (auto &book : books)
        {
            book->Update();
        }

        UpdateSkillDragging();
    };

    void Draw(IDirect3DDevice9 *device)
    {
        // Books may be added in book->Draw().
        size_t n_books_before_draw = books.size();
        for (size_t i = 0; i < n_books_before_draw; ++i)
        {
            auto &book = books[i];
            book->Draw(device, i);
        }
    }
}