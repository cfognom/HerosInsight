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

#include <attribute_or_title.h>
#include <attribute_store.h>
#include <capacity_hints.h>
#include <constants.h>
#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <debug.h>
#include <debug_display.h>
#include <filtering.h>
#include <imgui_ext.h>
#include <matcher.h>
#include <party_data.h>
#include <rich_text.h>
#include <settings.h>
#include <span_vector.h>
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
    struct BookState;

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

        auto skills = GW::SkillbarMgr::GetSkills();
        auto cskills = CustomSkillDataModule::GetSkills();

        auto Comparer = [&text_provider, skills, cskills](uint16_t a, uint16_t b)
        {
            std::strong_ordering cmp;

            auto &a_skill = skills[a];
            auto &b_skill = skills[b];
            // If the skill is a pvp version, refer to the pve version
            auto a_pve_id = a_skill.IsPvP() ? (uint16_t)a_skill.skill_id_pvp : a;
            auto b_pve_id = b_skill.IsPvP() ? (uint16_t)b_skill.skill_id_pvp : b;
            auto &a_pve_skill = skills[a_pve_id];
            auto &b_pve_skill = skills[b_pve_id];
            if (a_pve_id == b_pve_id)
            {
                cmp = a_skill.IsPvP() <=> b_skill.IsPvP(); // Put pve version before pvp version
                if (cmp != 0)
                    return cmp < 0;
            }
            else
            {
                auto &a_pve_cskill = cskills[a_pve_id];
                auto &b_pve_cskill = cskills[b_pve_id];

                auto ca = a_pve_cskill.context;
                auto cb = b_pve_cskill.context;
                cmp = ca <=> cb;
                if (cmp != 0)
                    return cmp < 0;

                auto pa = (uint32_t)a_pve_skill.profession - 1; // We do this so that None comes last
                auto pb = (uint32_t)b_pve_skill.profession - 1;
                cmp = pa <=> pb;
                if (cmp != 0)
                    return cmp < 0;

                auto attr_a = a_pve_cskill.attribute.value;
                auto attr_b = b_pve_cskill.attribute.value;
                cmp = attr_a <=> attr_b;
                if (cmp != 0)
                    return cmp < 0;

                // Skills with neither profession nor attribute/title are sorted by type and campaign
                if (a_pve_skill.profession == GW::Constants::ProfessionByte::None &&
                    a_pve_cskill.attribute.IsNone())
                {
                    assert(b_pve_skill.profession == GW::Constants::ProfessionByte::None);
                    assert(b_pve_cskill.attribute.IsNone());

                    auto ty_a = a_pve_cskill.GetTypeString();
                    auto ty_b = b_pve_cskill.GetTypeString();
                    cmp = ty_a <=> ty_b;
                    if (cmp != 0)
                        return cmp < 0;

                    auto ca = a_pve_skill.campaign;
                    auto cb = b_pve_skill.campaign;
                    cmp = ca <=> cb;
                    if (cmp != 0)
                        return cmp < 0;
                }

                auto n_a = text_provider.GetName((GW::Constants::SkillID)a_pve_id);
                auto n_b = text_provider.GetName((GW::Constants::SkillID)b_pve_id);
                cmp = n_a <=> n_b;
                if (cmp != 0)
                    return cmp < 0;
            }

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
        AoE,
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
    BookState *book_that_pressed_help = nullptr;
    auto feedback_checker = SettingsGuard().Access().skill_book.feedback.GetChangeChecker();

    int8_t GetFocusedAgentAttrLvl(AttributeOrTitle id)
    {
        auto agent_id = focused_agent_id;
        auto custom_agent_data = CustomAgentDataModule::GetCustomAgentData(agent_id);
        return custom_agent_data.GetOrEstimateAttribute(id);
    }

    struct AttributeSource
    {
        enum struct Type : int
        {
            ZeroToFifteen,
            FromAgent,
            Manual,
        };

        AttributeSource() : type(Type::ZeroToFifteen), value(-1) {}

        Type type;
        size_t value;

        int8_t GetAttrLvl(AttributeOrTitle id) const
        {
            switch (this->type)
            {
                case Type::FromAgent:
                {
                    // auto agent_id = this->value;
                    return GetFocusedAgentAttrLvl(id);
                }
                default:
                case Type::ZeroToFifteen:
                case Type::Manual:
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
        int8_t attr_lvl;
        SkillTooltipProvider(GW::Constants::SkillID skill_id, int8_t attr_lvl) : skill_id(skill_id), attr_lvl(attr_lvl) {}
        void DrawTooltip(uint32_t tooltip_id)
        {
            auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);
            const auto attr_str = custom_sd.GetAttributeStr();

            ImGui::BeginTooltip();

            auto draw_list = ImGui::GetWindowDrawList();
            const auto init_cursor_ss = ImGui::GetCursorScreenPos();
            auto cursor = init_cursor_ss;

            auto text_height = ImGui::GetTextLineHeight();

            enum Row
            {
                AttributeRow,
                ValueRow,
                DeltaRow,

                COUNT,
            };

            struct Cell
            {
                union
                {
                    const char *str;
                    char chars[sizeof(str)];
                };
                float width;
            };

            constexpr uint32_t attr_lvls = 22;
            constexpr uint32_t rows = Row::COUNT;
            constexpr uint32_t cols = 1 + attr_lvls;
            constexpr uint32_t cell_count = rows * cols;
            std::array<Cell, cell_count> cells;
            std::array<int32_t, attr_lvls - 1> deltas;

            auto skill_param = custom_sd.GetSkillParam(tooltip_id);

            { // Assign strings
                cells[AttributeRow * cols + 0].str = attr_str.data();
                cells[ValueRow * cols + 0].str = "Value";
                cells[DeltaRow * cols + 0].str = "Change";

                int32_t prev_value = 0;
                for (size_t attr_lvl = 0; attr_lvl < attr_lvls; ++attr_lvl)
                {
                    auto value = skill_param.Resolve(attr_lvl);
                    auto delta = (int32_t)value - prev_value;
                    prev_value = value;

                    auto x = 1 + attr_lvl;

                    auto &attr_lvl_cell = cells[AttributeRow * cols + x];
                    auto &value_cell = cells[ValueRow * cols + x];
                    auto &delta_cell = cells[DeltaRow * cols + x];

                    constexpr size_t buffer_size = sizeof(Cell::chars);
                    snprintf(attr_lvl_cell.chars, buffer_size, "%d", attr_lvl);
                    snprintf(value_cell.chars, buffer_size, "%d", value);
                    if (attr_lvl == 0 || delta == 0)
                    {
                        delta_cell.chars[0] = '-';
                        delta_cell.chars[1] = '\0';
                    }
                    else
                    {
                        snprintf(delta_cell.chars, buffer_size, "%+d", delta);
                    }
                    if (attr_lvl > 0)
                    {
                        deltas[attr_lvl - 1] = delta;
                    }
                }
            }

            int32_t majority_delta; // The most common delta
            int32_t good_delta = std::numeric_limits<int32_t>::min();
            int32_t bad_delta = std::numeric_limits<int32_t>::max();
            { // Figure out majority, good and bad delta
                size_t majority_delta_count = 0;
                for (auto delta : deltas)
                {
                    bad_delta = std::min(bad_delta, delta);
                    good_delta = std::max(good_delta, delta);

                    // Boyer–Moore Voting Algorithm
                    if (majority_delta_count == 0)
                        majority_delta = delta;
                    majority_delta_count += (delta == majority_delta) ? 1 : -1;
                }
                if (skill_param.IsDecreasing())
                {
                    std::swap(bad_delta, good_delta);
                }
            }
            SOFT_ASSERT(majority_delta == bad_delta || majority_delta == good_delta, L"Boyer–Moore Voting Algorithm invariant broken");

            struct Style
            {
                ImFont *font;
                ImU32 color;
            };

            auto GetStyle = [&](size_t col, size_t row)
            {
                Style style{
                    .font = Constants::Fonts::gw_font_16,
                    .color = IM_COL32_WHITE
                };

                int32_t attr_lvl = (int32_t)col - 1;
                bool is_special = attr_lvl == 0 || attr_lvl == 12 || attr_lvl == 15;
                switch (row)
                {
                    case AttributeRow:
                    {
                        if (is_special)
                        {
                            style.color = Constants::Colors::highlight;
                            style.font = Constants::Fonts::skill_thick_font_15;
                        }
                        break;
                    }
                    case ValueRow:
                    {
                        if (is_special)
                        {
                            style.font = Constants::Fonts::skill_thick_font_15;
                        }
                        style.color = Constants::GWColors::skill_dynamic_green;
                        break;
                    }
                    case DeltaRow:
                    {
                        style.color = Constants::GWColors::skill_dull_gray;
                        if (col >= 2 && (good_delta != bad_delta))
                        {
                            auto delta = deltas[col - 2];
                            if (delta == good_delta)
                            {
                                style.color = Constants::GWColors::heal_blue;
                            }
                        }
                        break;
                    }
                }

                return style;
            };

            // Calc widths
            for (size_t i = 0; i < cell_count; ++i)
            {
                auto y = i / cols;
                auto x = i % cols;
                auto &cell = cells[i];

                auto style = GetStyle(x, y);
                ImGui::PushFont(style.font);
                auto text = x == 0 ? cell.str : cell.chars;
                cell.width = ImGui::CalcTextSize(text).x;
                ImGui::PopFont();
            }

            float names_max_width = 0.f;
            { // Calc names_max_width
                for (size_t y = 0; y < rows; ++y)
                {
                    names_max_width = std::max(names_max_width, cells[y * cols + 0].width);
                }
            }

            float values_max_width = 0.f;
            { // Calc values_max_width
                for (size_t x = 1; x < cols; ++x)
                {
                    for (size_t y = 0; y < rows; ++y)
                    {
                        values_max_width = std::max(values_max_width, cells[y * cols + x].width);
                    }
                }
            }

            constexpr auto x_padding = 3;
            constexpr auto selector_box_padding = 2;
            constexpr auto separator_line_overextend = 1;
            constexpr auto separator_line_thickness = 1;

            for (size_t x = 0; x < cols; ++x)
            {
                auto col_cursor = cursor;
                cursor.x += x_padding;
                for (size_t y = 0; y < rows; ++y)
                {
                    auto &cell = cells[y * cols + x];
                    auto style = GetStyle(x, y);
                    ImGui::PushFont(style.font);
                    auto color = ImGui::GetColorU32(style.color);
                    auto c = cursor;
                    c.y += y * text_height;
                    if (x == 0)
                    {
                        c.x += (names_max_width - cell.width) / 2;
                        auto &name = cell.str;
                        draw_list->AddText(c, color, name);
                    }
                    else
                    {
                        c.x += (values_max_width - cell.width) / 2;
                        draw_list->AddText(c, color, cell.chars);
                    }
                    ImGui::PopFont();
                }
                cursor.x += x == 0 ? names_max_width : values_max_width;
                cursor.x += x_padding;

                if (x > 0)
                {
                    auto attr_lvl = x - 1;
                    if (attr_lvl == this->attr_lvl)
                    {
                        // Draw selector box around current attribute
                        auto min = col_cursor + ImVec2(x_padding - selector_box_padding, -selector_box_padding);
                        auto max = cursor + ImVec2(selector_box_padding - x_padding, rows * text_height + selector_box_padding);
                        draw_list->AddRect(min, max, ImGui::GetColorU32(Constants::Colors::highlight));
                    }
                }

                if (x + 1 < cols)
                {
                    // Draw separator
                    auto start = cursor + ImVec2(0, -separator_line_overextend);
                    auto end = cursor + ImVec2(0, rows * text_height + separator_line_overextend);
                    draw_list->AddLine(start, end, ImGui::GetColorU32(ImGuiCol_Border), separator_line_thickness);
                    cursor.x += separator_line_thickness;
                }
            }

            const auto min = init_cursor_ss;
            const auto max = cursor + ImVec2(0, rows * text_height);
            ImRect rect(min, max);

            ImGui::ItemSize(rect);
            ImGui::ItemAdd(rect, 0);

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

        template <auto Func, auto Unit, auto Icon>
        static Text::StringTemplateAtom NumberAndIcon(Text::StringTemplateAtom::Builder &b, size_t skill_id, void *)
        {
            auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
            auto value = (cskill.*Func)();
            if (value == 0)
                return {};

            FixedVector<Text::StringTemplateAtom, 4> args;

            if (value < 0)
            {
                value = -value;
                args.push_back(b.Char('-'));
            }

            args.push_back(b.MixedNumber(value));
            if constexpr (Unit != nullptr)
            {
                args.push_back(b.Chars(*Unit));
            }
            args.push_back(b.ExplicitString(*Icon));

            return b.ExplicitSequence(args);
        }

        template <auto Func>
        static void GetSkillProp(SpanVector<char> &dst, size_t skill_id)
        {
            auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
            dst.Elements().append_range((cskill.*Func)());
        }

        void InitProps()
        {
            static std::string_view Percent = "%";

            static_props[(size_t)SkillProp::Energy].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetEnergy, nullptr, &RichText::Icons::EnergyOrb>);
            static_props[(size_t)SkillProp::Recharge].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetRecharge, nullptr, &RichText::Icons::Recharge>);

            static_props[(size_t)SkillProp::Upkeep].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetUpkeep, nullptr, &RichText::Icons::Upkeep>);
            static_props[(size_t)SkillProp::Overcast].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetOvercast, nullptr, &RichText::Icons::Overcast>);
            static_props[(size_t)SkillProp::Sacrifice].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetSacrifice, &Percent, &RichText::Icons::Sacrifice>);
            static_props[(size_t)SkillProp::Activation].SetupIncremental(nullptr, NumberAndIcon<&CustomSkillData::GetActivation, nullptr, &RichText::Icons::Activation>);

            static_props[(size_t)SkillProp::Name].SetupIncremental(
                nullptr,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *) -> Text::StringTemplateAtom
                {
                    auto &text_provider = Text::GetTextProvider(GW::Constants::Language::English);
                    return text_provider.MakeSkillName(b, (GW::Constants::SkillID)skill_id);
                }
            );

            static_props[(size_t)SkillProp::Type].PopulateItems("SkillBookProp_Type", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetTypeString>);
            static_props[(size_t)SkillProp::Attribute].PopulateItems("SkillBookProp_Attribute", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetAttributeStr>);
            static_props[(size_t)SkillProp::Profession].PopulateItems("SkillBookProp_Profession", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetProfessionStr>);
            static_props[(size_t)SkillProp::Campaign].PopulateItems("SkillBookProp_Campaign", GW::Constants::SkillMax, GetSkillProp<&CustomSkillData::GetCampaignStr>);
            static_props[(size_t)SkillProp::Id].SetupIncremental(
                nullptr,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *) -> Text::StringTemplateAtom
                {
                    return b.Number((float)skill_id);
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
                        number_and_icon.push_back(b.Tag(RichText::ColorTag(IM_COL32(255, 255, 120, 255))));
                    }
                    number_and_icon.push_back(b.MixedNumber(skill.aftercast));
                    if (!is_normal_aftercast)
                    {
                        number_and_icon.push_back(b.Char('*'));
                        number_and_icon.push_back(b.Tag(RichText::ColorTag(NULL)));
                    }
                    number_and_icon.push_back(b.ExplicitString(RichText::Icons::Aftercast));

                    return b.ExplicitSequence(number_and_icon);
                }
            );

            static_props[(size_t)SkillProp::AoE].SetupIncremental(
                nullptr,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *) -> Text::StringTemplateAtom
                {
                    FixedVector<Utils::Range, 4> ranges;
                    auto &cskill = CustomSkillDataModule::GetSkills()[skill_id];
                    cskill.GetRanges(ranges);
                    FixedVector<Text::StringTemplateAtom, 32> range_atoms;
                    for (size_t i = 0; i < ranges.size(); ++i)
                    {
                        auto range = ranges[i];
                        range_atoms.push_back(b.Number((float)(std::underlying_type_t<Utils::Range>)range));
                        auto range_name = Utils::GetRangeStr(range);
                        if (range_name.has_value())
                        {
                            range_atoms.push_back(b.Chars(" ("));
                            range_atoms.push_back(b.ExplicitString(range_name.value()));
                            range_atoms.push_back(b.Char(')'));
                        }

                        if (i < ranges.size() - 1)
                            range_atoms.push_back(b.Chars(", "));
                    }
                    return b.ExplicitSequence(range_atoms);
                }
            );
        }

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

            std::string_view capacity_hint_key = "meta_prop_names";
            meta_prop_names.arena.ReserveFromHint(capacity_hint_key);
            meta_propsets.reserve(meta_prop_names.arena.SpanCount());

            SetupMetaProp("", ALL_PROPS); // Must be first
            SetupMetaProp("Meta", CreatePropset(SkillProp::COUNT));
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
            SetupMetaProp("Attribute", CreatePropset(SkillProp::Attribute));
            SetupMetaProp("Profession", CreatePropset(SkillProp::Profession));
            SetupMetaProp("Campaign", CreatePropset(SkillProp::Campaign));
            SetupMetaProp("AoE", CreatePropset(SkillProp::AoE));

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
        enum struct Ruleset
        {
            Mixed,
            PvE,
            PvP,
        };
        enum struct Scope : int
        {
            Invested,
            Equipable,
            ForProfessions,
            Default,
            AddTemporary,
            AddNPC,
            AddMisc,
            AddArchived,
            COUNT,
        };
        AttributeSource attr_src;
        Ruleset ruleset = Ruleset::Mixed;
        Scope scope = Scope::Default;
        int attr_lvl_slider = 12;

        bool use_exact_adrenaline = false;
        bool prefer_concise_descriptions = false;
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
            dynamic_props.reserve(8);
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

                    return b.ExplicitSequence(
                        {b.MixedNumber(adrenaline_strikes, adrenaline_units, 25),
                         b.ExplicitString(RichText::Icons::Adrenaline)}
                    );
                }
            );
            dynamic_props[SkillProp::Tag].SetupIncremental(
                &settings,
                +[](Text::StringTemplateAtom::Builder &b, size_t skill_id, void *data) -> Text::StringTemplateAtom
                {
                    auto skills = GW::SkillbarMgr::GetSkills();
                    auto cskills = CustomSkillDataModule::GetSkills();
                    auto &skill = skills[skill_id];
                    auto &cskill = cskills[skill_id];
                    auto skill_id_to_check = skill.IsPvP() ? skill.skill_id_pvp : skill.skill_id;
                    auto &pve_skill = skills[(uint32_t)skill_id_to_check];
                    auto &pve_cskill = cskills[(uint32_t)skill_id_to_check];

                    auto &tags = cskill.tags;
                    bool is_unlocked = GW::SkillbarMgr::GetIsSkillUnlocked(skill_id_to_check);
                    bool is_learned = Utils::IsSkillLearned(pve_skill, focused_agent_id);
                    bool is_equipable = Utils::IsSkillEquipable(skill, focused_agent_id);
                    bool is_locked = tags.Unlockable && !is_unlocked;
                    bool is_learnable = Utils::IsSkillLearnable(pve_cskill, focused_agent_id);
                    bool is_unlearned = is_learnable && !is_learned;

                    FixedVector<Text::StringTemplateAtom, 16> args;

                    auto PushTag = [&](std::string_view str, ImU32 color = NULL, std::string_view icon = {})
                    {
                        if (color != NULL)
                            args.push_back(b.Tag(RichText::ColorTag(color)));

                        args.push_back(b.ExplicitString(str));

                        if (color != NULL)
                            args.push_back(b.Tag(RichText::ColorTag(NULL)));

                        if (!icon.empty())
                            args.push_back(b.ExplicitString(icon));

                        args.push_back(b.ExplicitString(", "));
                    };

                    constexpr auto soft_red = IM_COL32(255, 100, 100, 255);
                    constexpr auto soft_green = IM_COL32(143, 255, 143, 255);

                    // clang-format off
                    if (tags.PvEOnly)          PushTag("PvE-only");
                    if (is_unlocked)           PushTag("Unlocked" , soft_green);
                    if (is_locked)             PushTag("Locked"   , soft_red);
                    if (is_learned)            PushTag("Learned"  , soft_green);
                    if (is_unlearned)          PushTag("Unlearned", soft_red);
                    if (tags.Temporary)        PushTag("Temporary");
                    if (is_equipable)          PushTag("Equipable", IM_COL32(100, 255, 255, 255));
                    
                    if (cskill.context != Utils::SkillContext::Null)
                        PushTag(Utils::GetSkillContextString(cskill.context));

                    if (tags.Archived)         PushTag("Archived");
                    if (tags.EffectOnly)       PushTag("Effect-only");
                    if (tags.PvPOnly)          PushTag("PvP-only");
                    if (tags.PvEVersion)       PushTag("PvE Version");
                    if (tags.PvPVersion)       PushTag("PvP Version");
                    if (tags.Bounty)           PushTag("Bounty");

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
                        args.pop_back();
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
        constexpr static size_t MaxSpanCount() { return GW::Constants::SkillMax; }
        constexpr static size_t PropCount() { return PROP_COUNT; }
        size_t MetaCount() const { return ts.meta_propsets.size(); }

        LoweredText GetMetaName(size_t meta) { return ts.meta_prop_names.Get(meta); }
        BitView GetMetaPropset(size_t meta) const { return ts.meta_propsets[meta]; }

        Filtering::IncrementalProp *GetProperty(size_t prop) { return props[prop]; }
    };

    bool is_dragging = false;
    bool IsDragging() { return is_dragging; }
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
            case Utils::GetSkillFrameResult::Error::SkillsAndAttributesNotOpened:
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
        ImGuiWindow *imgui_window = nullptr;

        Text::Provider &text_provider = Text::GetTextProvider(GW::Constants::Language::English);

        enum struct DirtyFlags : uint32_t
        {
            None = 0,
            Query = 1 << 0,
            SkillList = 1 << 1,
            Props = 1 << 2,

            ALL = Query | SkillList | Props,
        };

        DirtyFlags dirty_flags = DirtyFlags::ALL;
        bool first_draw = true;

        struct ScrollTracking
        {
            std::vector<VariableSizeClipper::Position> scroll_positions;
            SpanVector<char> input_text_history;
            size_t current_index = 0;

            ScrollTracking() { input_text_history.push_back(std::string_view("")); }

            void UpdateScrollTracking(std::string_view input_text, VariableSizeClipper &clipper)
            {
                // Save current scroll
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
                current_index = std::distance(input_text_history.begin(), it); // this may increment index
                if (it == input_text_history.end() ||
                    std::string_view(*it) != input_text) // Not found
                {
                    // Discard diverging history
                    scroll_positions.resize(current_index);
                    input_text_history.resize(current_index);
                    input_text_history.push_back(input_text);
                    // Reset scroll
                    clipper.Reset();
                }
                else
                {
                    // Restore old scroll
                    auto old_scroll = scroll_positions[current_index];
                    clipper.SetScroll(old_scroll);
                }
            }
        };
        ScrollTracking scroll_tracking;
        char input_text[1024] = {'\0'};

        Filtering::Feedback feedback;

        VariableSizeClipper clipper{};

        void Update()
        {
            if (dirty_flags != DirtyFlags::None)
            {
                UpdateQuery();
            }
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
            auto button_pos = ImVec2(window->Size.x - button_side * 2, button_padding);

            ImGui::PushClipRect(title_bar_rect.Min, title_bar_rect.Max, false);
            ImGui::SetCursorPos(button_pos);
            if (ImGui::Button("##DupeButton", button_size))
            {
                auto book = AddBook();
                book->CopyStateFrom(*this);
                book->init_dims = {window->Pos + ImVec2(32, 32), window->Size};
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

        void DrawScopeSlider()
        {
            if (ImGui::SliderInt("Scope", (int *)&settings.scope, 0, (int)BookSettings::Scope::COUNT - 1, ""))
            {
                dirty_flags |= DirtyFlags::SkillList;
            }
            std::string_view explanation;
            switch (settings.scope)
            {
                case BookSettings::Scope::Invested:
                    explanation = "Showing equipable and invested.";
                    break;
                case BookSettings::Scope::Equipable:
                    explanation = "Showing equipable.";
                    break;
                case BookSettings::Scope::ForProfessions:
                    explanation = "Showing for professions.";
                    break;
                case BookSettings::Scope::Default:
                    explanation = "Showing normal.";
                    break;
                case BookSettings::Scope::AddTemporary:
                    explanation = "Showing normal and temporary.";
                    break;
                case BookSettings::Scope::AddNPC:
                    explanation = "Showing normal, temporary, and NPC.";
                    break;
                case BookSettings::Scope::AddMisc:
                    explanation = "Showing normal, temporary, NPC, and misc.";
                    break;
                case BookSettings::Scope::AddArchived:
                    explanation = "Showing normal, temporary, NPC, misc, and archived.";
                    break;
            }
            // ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
            ImGui::TextUnformatted(explanation.data());
            // ImGui::PopStyleColor();
        }

        void DrawCheckboxes()
        {
            // dirty_flags |= ImGui::Checkbox("Show exact adrenaline", &settings.use_exact_adrenaline) ? DirtyFlags::Props : DirtyFlags::None; // Disabled, is it really that useful?
            ImGui::Checkbox("Prefer concise descriptions", &settings.prefer_concise_descriptions);
        }

        void DrawRulesetSelection()
        {
            const char *options[] = {"Mixed", "PvE", "PvP"};
            if (ImGuiExt::RadioArray("Skill Ruleset", (int *)&settings.ruleset, options, IM_ARRAYSIZE(options)))
            {
                dirty_flags |= DirtyFlags::SkillList;
            }
        }

        void DrawAttributeModeSelection()
        {
            const char *options[] = {"(0...15)", "Character's", "Manual"};
            if (ImGuiExt::RadioArray("Attributes", (int *)&settings.attr_src.type, options, IM_ARRAYSIZE(options)))
            {
                dirty_flags |= DirtyFlags::Props;
                switch (settings.attr_src.type)
                {
                    case AttributeSource::Type::ZeroToFifteen:
                        settings.attr_src.value = -1;
                        break;
                    case AttributeSource::Type::FromAgent:
                        break;
                    case AttributeSource::Type::Manual:
                        settings.attr_src.value = settings.attr_lvl_slider;
                        break;
                }
            }

            if (settings.attr_src.type == AttributeSource::Type::Manual)
            {
                if (ImGui::SliderInt("Attribute level", &settings.attr_lvl_slider, 0, 21))
                {
                    dirty_flags |= DirtyFlags::Props;
                    settings.attr_src.value = settings.attr_lvl_slider;
                }
            }
        }

        void InitFilterList()
        {
            uint32_t prof_mask;
            if (settings.scope == BookSettings::Scope::ForProfessions)
            {
                prof_mask = Utils::GetProfessionMask(focused_agent_id);
            }

            auto skills = GW::SkillbarMgr::GetSkills();
            auto cskills = CustomSkillDataModule::GetSkills();

            filtered_skills.clear();
            for (auto skill_id_16 : base_skills)
            {
                auto &skill = skills[skill_id_16];
                auto &cskill = cskills[skill_id_16];

                if (settings.ruleset == BookSettings::Ruleset::PvP)
                {
                    if (cskill.tags.PvEOnly || cskill.tags.PvEVersion)
                        continue;
                }
                else if (settings.ruleset == BookSettings::Ruleset::PvE)
                {
                    if (cskill.tags.PvPOnly || cskill.tags.PvPVersion)
                        continue;
                }

                if ((int)settings.scope < (int)BookSettings::Scope::AddTemporary)
                {
                    if (cskill.tags.Temporary)
                        continue;
                }
                if ((int)settings.scope < (int)BookSettings::Scope::AddArchived)
                {
                    if (cskill.tags.Archived)
                        continue;
                }
                if ((int)settings.scope < (int)BookSettings::Scope::AddNPC)
                {
                    if (cskill.tags.MonsterSkill ||
                        cskill.tags.EnvironmentSkill)
                        continue;
                }
                if ((int)settings.scope < (int)BookSettings::Scope::AddMisc)
                {
                    if (skill.type == GW::Constants::SkillType::Disguise ||
                        skill_id_16 == GW::Constants::SkillID::Tonic_Tipsiness)
                        continue;
                }
                if ((int)settings.scope < (int)BookSettings::Scope::AddMisc)
                {
                    if (cskill.tags.Bounty)
                        continue;
                }

                if ((int)settings.scope <= (int)BookSettings::Scope::Invested)
                {
                    if (GetFocusedAgentAttrLvl(cskill.attribute) == 0)
                        continue;
                }

                if ((int)settings.scope <= (int)BookSettings::Scope::Equipable)
                {
                    if (!Utils::IsSkillEquipable(skill, focused_agent_id))
                        continue;
                }
                else if ((int)settings.scope <= (int)BookSettings::Scope::ForProfessions)
                {
                    auto skill_prof_mask = 1 << (uint32_t)cskill.skill->profession;
                    if ((prof_mask & skill_prof_mask) == 0)
                        continue;
                }

                filtered_skills.push_back(skill_id_16);
            }
        }

        void UpdateFeedback(int feedback_style)
        {
            bool verbose = feedback_style == (int)Settings::SkillBook::FeedbackSetting::Detailed;
            filter_device.GetFeedback(query, feedback, verbose);
        }

        void UpdateQuery()
        {
            if (Utils::HasAnyFlag(dirty_flags, DirtyFlags::Props))
            {
                adapter.RefreshDynamicProps(this->settings.attr_src);
                dirty_flags |= DirtyFlags::Query;
            }
            if (Utils::HasAnyFlag(dirty_flags, DirtyFlags::SkillList))
            {
                clipper.Reset();
                scroll_tracking = ScrollTracking();
                dirty_flags |= DirtyFlags::Query;
            }
            if (Utils::HasAnyFlag(dirty_flags, DirtyFlags::Query))
            {
                auto input_text_view = std::string_view(input_text, strlen(input_text));
                scroll_tracking.UpdateScrollTracking(input_text_view, clipper);
                filter_device.ParseQuery(input_text_view, query);

                InitFilterList();
                this->filter_device.RunQuery(this->query, this->filtered_skills);

                UpdateFeedback(feedback_checker.last_value);
            }
            dirty_flags = DirtyFlags::None;
        }

        template <typename DrawContent>
        void DrawProperty(DrawContent &&draw_content, size_t prop_id, bool is_hidden_header = false)
        {
            auto meta_names = filter_device.CalcPropResult(query, prop_id);
            std::string_view header{};
            if (!is_hidden_header)
            {
                std::string_view span = meta_names.text;
                header = span.substr(0, span.find(','));
            }

            auto DrawTooltip = [](std::string_view text, std::span<uint16_t> highlighting)
            {
                if (text.empty())
                    return;

                ImGui::BeginTooltip();
                auto window = ImGui::GetCurrentWindow();
                if (window->BeginCount == 1) // We only draw a tooltip if it wasn't written already
                {
                    ImGui::PushFont(Constants::Fonts::gw_font_16);
                    ImGui::PushStyleColor(ImGuiCol_Text, 0xff64ffff);
                    text_drawer.DrawRichText(text, 0, -1, highlighting);
                    ImGui::PopStyleColor();
                    ImGui::PopFont();
                }
                ImGui::EndTooltip();
            };

            text_drawer.DrawRichText(header, 0, -1, meta_names.hl);
            bool is_header_hovered = ImGui::IsItemHovered();
            if (is_header_hovered)
            {
                auto meta_prop_id = adapter.PropCount();
                auto meta_meta_names = filter_device.CalcPropResult(query, meta_prop_id);
                DrawTooltip(meta_meta_names.text, meta_meta_names.hl);
            }
            ImGui::SameLine(0, 0);

            if (!is_hidden_header)
            {
                ImGui::TextUnformatted(": ");
                ImGui::SameLine(0, 0);
            }

            draw_content();
            if (!is_header_hovered && ImGui::IsItemHovered())
            {
                DrawTooltip(meta_names.text, meta_names.hl);
            }
        }

        void DrawProperty(size_t prop_id, size_t skill_id, float wrapping_min, float wrapping_max, bool is_hidden_header = false)
        {
            auto content = filter_device.CalcItemResult(query, prop_id, skill_id);
            if (content.text.empty())
                return;

            DrawProperty(
                [&]()
                {
                    text_drawer.DrawRichText(content.text, wrapping_min, wrapping_max, content.hl);
                },
                prop_id,
                is_hidden_header
            );
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
                size_t pos_from_right;
            };

            static constexpr Layout layout[]{
                {SkillProp::Overcast, 4},
                {SkillProp::Sacrifice, 4},
                {SkillProp::Upkeep, 4},
                {SkillProp::Energy, 3},
                {SkillProp::Adrenaline, 3},
                {SkillProp::Activation, 2},
                {SkillProp::Recharge, 1},
                {SkillProp::Aftercast, 0},
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
                const auto stat_line_cursor = ImVec2(current_x, base_pos_y + 1);
                ImGui::SetCursorPos(stat_line_cursor);

                DrawProperty(
                    [&]()
                    {
                        text_drawer.DrawTextSegments(segments, 0, -1);
                    },
                    (size_t)l.id,
                    true
                );

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
                    FixedVector<RichText::TextSegment, 32> segments;
                    text_drawer.MakeTextSegments(r.text, segments, r.hl);
                    DrawProperty(
                        [&]()
                        {
                            text_drawer.DrawTextSegments(segments, wrapping_min, wrapping_max);
                        },
                        (size_t)SkillProp::Name,
                        true
                    );

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
                    DrawProperty((size_t)SkillProp::Type, (size_t)skill_id, wrapping_min, wrapping_max, true);
                }

                { // Draw skill tags
                    ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
                    DrawProperty((size_t)SkillProp::Tag, (size_t)skill_id, wrapping_min, wrapping_max, true);
                    ImGui::PopStyleColor();
                }

                ImGui::PopStyleVar();
                ImGui::EndGroup();
            }

            ImGui::EndGroup();
        }

        void MarkQueryDirty()
        {
            dirty_flags |= DirtyFlags::Query;
        }

        void SetInputText(std::string_view text)
        {
            auto size = std::min(text.size(), sizeof(input_text) - 1);
            std::memcpy(input_text, text.data(), size);
            input_text[size] = '\0';
            MarkQueryDirty();
        }

        void DrawSearchBox()
        {
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();

            ImGui::InputTextWithHint(
                "##SearchBox",
                "Search for a skill...",
                input_text, sizeof(input_text),
                ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_AutoSelectAll,
                [](ImGuiInputTextCallbackData *data)
                {
                    auto &book = *static_cast<BookState *>(data->UserData);
                    book.MarkQueryDirty();

                    return 0;
                },
                this
            );

            if (SettingsGuard{}.Access().skill_book.show_help_button.value)
            {
                ImGui::SameLine();
                auto pos = ImGui::GetCursorScreenPos();
                if (ImGuiExt::Button("Help!"))
                {
                    book_that_pressed_help = book_that_pressed_help ? nullptr : this;
                }
                // ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y - 1));
                // ImGui::TextUnformatted("(?)");
                // if (ImGui::IsItemHovered())
                // {
                //     ImGui::BeginTooltip();
                //     DrawSearchHelp();
                //     ImGui::EndTooltip();
                // }
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

            if (feedback_checker.last_value != (int)Settings::SkillBook::FeedbackSetting::Hidden)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);

                // ImGui::TextUnformatted("Feedback:");
                // ImGui::SameLine(0, 0);
                auto window = ImGui::GetCurrentWindow();
                auto wrapping_max = window->WorkRect.GetWidth();
                if (!feedback.filter_feedback.empty())
                {
                    text_drawer.DrawRichText(feedback.filter_feedback, 0, wrapping_max);
                }
                if (!feedback.command_feedback.empty())
                {
                    ImGui::Spacing();
                    text_drawer.DrawRichText(feedback.command_feedback, 0, wrapping_max);
                }

                ImGui::PopStyleColor();
            }
        }

        void DrawDescription(GW::Constants::SkillID skill_id, float work_width)
        {
            auto &cskill = CustomSkillDataModule::GetSkills()[(size_t)skill_id];
            auto attr_lvl = settings.attr_src.GetAttrLvl(cskill.attribute);
            auto tt_provider = SkillTooltipProvider(skill_id, attr_lvl);
            text_drawer.tooltip_provider = &tt_provider;

            auto main_prop = settings.prefer_concise_descriptions ? SkillProp::Concise : SkillProp::Description;
            auto alt_prop = settings.prefer_concise_descriptions ? SkillProp::Description : SkillProp::Concise;

            auto main_r = filter_device.CalcItemResult(query, (size_t)main_prop, (size_t)skill_id);
            auto alt_r = filter_device.CalcItemResult(query, (size_t)alt_prop, (size_t)skill_id);

            DrawProperty(
                [&]
                {
                    text_drawer.DrawRichText(main_r.text, 0, work_width, main_r.hl);
                },
                (size_t)main_prop,
                true
            );

            bool draw_alt = alt_r.hl.size() > main_r.hl.size();
            if (!draw_alt)
            {
                auto len = std::min(main_r.hl.size(), alt_r.hl.size());
                assert(len % 2 == 0);
                for (size_t i = 0; i < len; i += 2)
                {
                    std::string_view main_hl_text = ((std::string_view)main_r.text).substr(main_r.hl[i], main_r.hl[i + 1] - main_r.hl[i]);
                    std::string_view alt_hl_text = ((std::string_view)alt_r.text).substr(alt_r.hl[i], alt_r.hl[i + 1] - alt_r.hl[i]);
                    if (main_hl_text != alt_hl_text)
                    {
                        draw_alt = true;
                        break;
                    }
                }
            }

            if (draw_alt)
            {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, Constants::Colors::notify);
                ImGui::TextUnformatted(settings.prefer_concise_descriptions ? "Additional matches in full description: " : "Additional matches in concise description: ");
                ImGui::PopStyleColor();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3);
                // ImGui::SameLine(0, 0);
                // auto wrapping_min = ImGui::GetCursorPosX();

                DrawProperty(
                    [&]
                    {
                        text_drawer.DrawRichText(alt_r.text, 0, work_width, alt_r.hl);
                    },
                    (size_t)alt_prop,
                    true
                );
            }
        }

        void DrawSkillFooter(CustomSkillData &custom_sd, float work_width)
        {
            auto &skill = *custom_sd.skill;
            auto skill_id = custom_sd.skill_id;
            ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            struct Layout
            {
                SkillProp id;
            };

            static constexpr Layout layout[]{
                {SkillProp::Attribute},
                {SkillProp::Profession},
                {SkillProp::Campaign},
                {SkillProp::AoE},
            };

            for (auto &l : layout)
            {
                DrawProperty((size_t)l.id, (size_t)skill_id, 0, work_width, false);
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
                DrawProperty(
                    [&]()
                    {
                        text_drawer.DrawRichText(r.text, 0, -1, r.hl);
                    },
                    (size_t)SkillProp::Id, true
                );
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(1.f);
            }
        }

        static void MakeBookName(OutBuf<char> name, size_t book_index)
        {
            name.AppendString("Skill Book [Ctrl + K]");
            if (book_index > 0)
            {
                name.AppendFormat(" ({})", book_index);
            }

            name.push_back('\0');
        }

        void DrawFocusedCharacterInfo()
        {
            auto wname = Utils::GetAgentName(focused_agent_id, L"?");
            auto name = Utils::WStrToStr(wname.c_str());

            if (SettingsGuard().Access().skill_book.show_focused_character.value)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
                ImGui::Text("Focused character: %s", name.c_str());
                ImGui::PopStyleColor();
            }
        }

        bool Draw(IDirect3DDevice9 *device, size_t book_index)
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

            bool is_open = true;
            ImGuiExt::WindowScope hi_wnd(name.data(), &is_open);
            if (hi_wnd.begun)
            {
                ImGui::PushFont(Constants::Fonts::window_name_font);
                DrawDupeButton();
                ImGui::PopFont();

                this->imgui_window = ImGui::GetCurrentWindow();

                DrawCheckboxes();
                DrawRulesetSelection();
                DrawScopeSlider();
                ImGui::Spacing();
                DrawFocusedCharacterInfo();
                DrawAttributeModeSelection();
                DrawSearchBox();
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
                auto n_skills = filtered_skills.size();
                ImGui::Text("%s results", std::to_string(n_skills).c_str());
                ImGui::PopStyleColor();
                ImGui::Separator();

                if (ImGui::BeginChild("SkillList", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove))
                {
                    auto est_item_height = 128.f;
                    // auto est_item_height = 1.f;

                    auto draw_list = ImGui::GetWindowDrawList();

                    auto DrawItem = [&](uint32_t i)
                    {
                        const auto skill_id = static_cast<GW::Constants::SkillID>(filtered_skills[i]);
                        auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);

                        const auto initial_screen_cursor = ImGui::GetCursorScreenPos();
                        const auto initial_cursor = ImGui::GetCursorPos();
                        const auto window = ImGui::GetCurrentWindow();
                        const auto work_width = window->WorkRect.GetWidth();

                        bool is_equipable = Utils::IsSkillEquipable(*custom_sd.skill, focused_agent_id);

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

                    bool snap_to_item = SettingsGuard().Access().general.scroll_snap_to_item.value;
                    clipper.Draw(n_skills, est_item_height, snap_to_item, DrawItem);
                }
                ImGui::EndChild();
            }

            Update();

            return is_open;
        }
    };

    void DrawHelpContent(BookState *book)
    {
        constexpr float wrap_min = -1;
        constexpr float wrap_max = 800;

        constexpr auto DrawTable = [](const auto &cells)
        {
            constexpr auto elems = std::size(cells);
            constexpr auto cols = 2;
            constexpr auto rows = elems / cols;
            constexpr auto padding = 5;

            float widths[elems];

            for (size_t i = 0; i < elems; i++)
            {
                FixedVector<RichText::TextSegment, 32> segments;
                text_drawer.MakeTextSegments(cells[i], segments);
                widths[i] = RichText::CalcTextSegmentsWidth(segments);
            }

            float col_widths[cols]{0, 0};

            for (size_t x = 0; x < cols; ++x)
            {
                col_widths[x] = 0;
                for (size_t y = 0; y < rows; ++y)
                {
                    col_widths[x] = std::max(col_widths[x], widths[y * cols + x]);
                }
            }

            auto init_cursor = ImGui::GetCursorScreenPos();
            auto cursor = init_cursor;

            for (size_t y = 0; y < rows; ++y)
            {
                cursor.x = init_cursor.x;
                for (size_t x = 0; x < cols; ++x)
                {
                    cursor.x += padding;
                    auto cell = cells[y * cols + x];
                    auto width = widths[y * cols + x];
                    auto write_cursor = cursor;
                    if (x == 0)
                    {
                        auto offset = (col_widths[x] - width) / 2;
                        write_cursor.x += offset;
                    }
                    ImGui::SetCursorScreenPos(write_cursor);
                    text_drawer.DrawRichText(cell, -1, wrap_max);
                    cursor.x += col_widths[x] + padding;
                }
                cursor.y += ImGui::GetTextLineHeight();
            }
        };

        text_drawer.DrawRichText(
            "Studying and trying the examples below should help you understand most of the search-syntax. "
            "In case you need more help you can also check out the more rigid definitions further below. "
            "Those definitions are much less intuitive but also much more precise.",
            wrap_min, wrap_max
        );

        ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
        ImGui::PushFont(Constants::Fonts::skill_name_font);
        ImGui::Text("Examples");
        ImGui::PopFont();
        ImGui::PopStyleColor();

        auto button_counter = 0;
        auto DrawExample = [&](std::string_view query_body, std::string_view description)
        {
            if (book)
            {
                FixedVector<char, 32> buffer;
                buffer.AppendFormat("Try it##{}", button_counter++);
                buffer.push_back('\0');
                if (ImGuiExt::Button(buffer.data()))
                {
                    book->SetInputText(query_body);
                }
                ImGui::SameLine();
            }
            auto text = std::format("\"{}\" <c=@skilldull>{}</c>", query_body, description);
            text_drawer.DrawRichText(text, wrap_min, wrap_max);
            ImGui::Spacing();
        };
        DrawExample("locked", "Finds skills you have yet to unlock");
        DrawExample("health regen", "Finds skills giving health regeneration");
        DrawExample("knock...down", "Finds skills related to knock down");
        DrawExample("recharge: <9", "Finds skills having less than 9 seconds recharge time");
        DrawExample("campaign!: prophecies", "Finds non-prophecies skills");
        DrawExample("type: stance & prof: mes|derv", "Finds mesmer or dervish stances");
        DrawExample("type: attack & aoe /sort prof, aoe!", "Finds AoE attack skills. Sorts ascending by profession then descending by AoE.");
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
        ImGui::PushFont(Constants::Fonts::skill_name_font);
        ImGui::Text("Definitions");
        ImGui::PopFont();
        ImGui::PopStyleColor();
        text_drawer.DrawRichText("The text typed in the search box is known as a <c=@skilldyn>query</c>.");

        ImGui::Bullet();
        text_drawer.DrawRichText(
            "A <c=@skilldyn>query</c> consists of one or more <c=@skilldyn>statements</c>, separated by <c=@skilldyn>&</c>.",
            wrap_min, wrap_max
        );
        ImGui::Bullet();
        text_drawer.DrawRichText(
            "A <c=@skilldyn>statement</c> consists of an optional <c=@skilldyn>filter</c> and zero or more <c=@skilldyn>commands</c>.",
            wrap_min, wrap_max
        );
        ImGui::Bullet();
        text_drawer.DrawRichText(
            "A <c=@skilldyn>filter</c> has two parts: an optional <c=@skilldyn>target</c>, "
            "which specifies which skill property to search and a <c=@skilldyn>body</c>, which defines what to match. "
            "<c=@skilldull>(You can discover valid targets by hovering over a skill's property in the skill book. "
            "For example, hovering over a skill's name shows \"<c=#64ffff>Name</c>\", which can be used as a filter target.)</c>",
            wrap_min, wrap_max
        );

        ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
        ImGui::Text("Special control characters");
        ImGui::PopStyleColor();
        ImGui::Text("These characters are used to control the overall structure of the query.");
        // clang-format off
        constexpr std::string_view control_chars[] = {
            "<c=@skilldyn>:</c>", "Used as the separator between a filter's target and body.",
            "<c=@skilldyn>!</c>", "\"Not\". When put before the separator, negates the filter, turning it into an exclusion filter.",
            "<c=@skilldyn>&</c>", "\"And\". Used to combine multiple statements in a query",
            "<c=@skilldyn>|</c>", "\"Or\". Used to specify multiple options in a filter's body",
            "<c=@skilldyn>/</c>", "Start of a command, follow up by the command name and its arguments",
        };
        // clang-format on
        DrawTable(control_chars);
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
        ImGui::Text("Special commands");
        ImGui::PopStyleColor();
        ImGui::Text("These commands can be used to modify how the results are presented.");
        // clang-format off
        constexpr std::string_view commands[] = {
            "<c=@skilldyn>/sort</c>", "Sorts the results according to the comma-separated list of targets following it. Append <c=@skilldyn>!</c> to reverse the sort order.",
        };
        // clang-format on
        DrawTable(commands);
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
        ImGui::Text("Special keywords");
        ImGui::PopStyleColor();
        ImGui::Text("These can be used in a filter's body to specify more complex patterns.");
        // clang-format off
        constexpr std::string_view keywords[] = {
            "(Space)", "Optionally skip to the next word.",
            "<c=@skilldyn>..</c>", "Optionally skip non-space characters.",
            "<c=@skilldyn>...</c>", "Optionally skip anything.",
            "<c=@skilldyn>#</c>", "Matches any number.",
        };
        // clang-format on
        DrawTable(keywords);
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
        ImGui::Text("Special number prefixes");
        ImGui::PopStyleColor();
        ImGui::Text("When put right before a number, they change how the number is matched.");
        // clang-format off
        constexpr std::string_view number_prefixes[] = {
            "<c=@skilldyn><</c>", "\"Less than\".",
            "<c=@skilldyn>></c>", "\"Greater than\".",
            "<c=@skilldyn><=</c>", "\"Less than or equal to\".",
            "<c=@skilldyn>>=</c>", "\"Greater than or equal to\".",
        };
        // clang-format on
        DrawTable(number_prefixes);
    }

    GW::HookEntry attribute_update_entry;
    GW::HookEntry select_hero_entry;

    void AttributeUpdatedCallback(GW::HookStatus *, const GW::Packet::StoC::AttributeUpdatePacket *packet)
    {
        auto packet_agent_id = packet->agent_id;

        if (packet_agent_id == focused_agent_id)
        {
            for (auto &book : books)
            {
                if (book->settings.attr_src.type == AttributeSource::Type::FromAgent)
                {
                    // auto attr = AttributeOrTitle((GW::Constants::AttributeByte)p->attribute);
                    // attributes[attr.value] = p->value;
                    book->dirty_flags |= BookState::DirtyFlags::Props;
                    // book->FetchDescriptions();
                }
            }
        }
    }
    void TitleUpdateCallback(GW::HookStatus *, const GW::Packet::StoC::UpdateTitle *packet)
    {
        for (auto &book : books)
        {
            if (book->settings.attr_src.type == AttributeSource::Type::FromAgent)
            {
                // auto attr = AttributeOrTitle((GW::Constants::TitleID)packet->title_id);
                // attributes[attr.value] = packet->new_value;
                book->dirty_flags |= BookState::DirtyFlags::Props;
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
                book->dirty_flags |= BookState::DirtyFlags::Props;
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
            book->dirty_flags |= BookState::DirtyFlags::Props;
        }
    }

    void ProfessionUpdatedCallback(GW::HookStatus *, GW::UI::UIMessage msg, void *wparam, void *lparam)
    {
        auto p = (GW::UI::UIPacket::kProfessionUpdated *)wparam;
        if (p->agent_id == focused_agent_id)
        {
            for (auto &book : books)
            {
                book->dirty_flags |= BookState::DirtyFlags::Props;
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
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AttributeUpdatePacket>(&attribute_update_entry, AttributeUpdatedCallback);
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

    void Update()
    {
        auto &feedback = SettingsGuard().Access().skill_book.feedback;
        if (feedback_checker.Changed(feedback))
        {
            for (auto &book : books)
            {
                book->UpdateFeedback(feedback.value);
            }
        }

        UpdateSkillDragging();
    };

    void Draw(IDirect3DDevice9 *device)
    {
        // Reverse loop because books may be added in book->Draw() + we handle removal.
        for (size_t i = books.size(); i-- > 0;)
        {
            auto &book = books[i];
            bool is_open = book->Draw(device, i);
            if (!is_open)
            {
                if (books.size() > 1)
                {
                    auto dst_wind = books[i]->imgui_window;
                    for (size_t j = i; ++j < books.size();)
                    {
                        auto src_wind = books[j]->imgui_window;
                        if (dst_wind && src_wind)
                        {
                            dst_wind->Pos = src_wind->Pos;
                            dst_wind->Size = src_wind->Size;
                            dst_wind->Collapsed = src_wind->Collapsed;
                        }
                        dst_wind = src_wind;
                    }
                    books.erase(books.begin() + i);
                }
                else
                {
                    UpdateManager::open_skill_book = false;
                }
            }
        }

        if (book_that_pressed_help)
        {
            bool is_open = true;
            ImGuiExt::WindowScope hi_wnd("Search-syntax help", &is_open);
            if (hi_wnd.begun)
            {
                DrawHelpContent(book_that_pressed_help);
            }
            if (!is_open)
            {
                book_that_pressed_help = nullptr;
            }
        }
    }
}