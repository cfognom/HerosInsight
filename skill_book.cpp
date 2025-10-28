#include <Windows.h>
#include <bitset>
#include <codecvt>
#include <d3d9.h>
#include <filesystem>
#include <future>
#include <iostream>
#include <optional>
#include <regex>
#include <span>
#include <string>
#include <variant>

#include <GWCA/GWCA.h>

#include <GWCA/Managers/AgentMgr.h>
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
#include <capacity_hints.h>
#include <catalog.h>
#include <constants.h>
#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <debug.h>
#include <debug_display.h>
#include <matcher.h>
#include <party_data.h>
#include <rich_text.h>
#include <string_arena.h>
#include <texture_module.h>
#include <update_manager.h>
#include <utils.h>
#include <variable_size_clipper.h>

#include "skill_book.h"

// #define ASYNC_FILTERING

#ifdef _DEBUG
// #define _ARTIFICIAL_DELAY 1
#endif

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

    bool TryInitBaseSkills()
    {
        bool fail = false;
        auto &text_provider = SkillTextProvider::GetInstance(GW::Constants::Language::English);
        if (!text_provider.IsReady())
            return false;

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

        return true;
    }

    enum struct AttributeMode : uint32_t
    {
        Null,
        Generic,
        Characters,
        Manual
    };

    enum struct SkillTextPropertyID : uint16_t
    {
        Name,
        Type,
        Tag,

        Upkeep,
        Energy,
        AdrenalineStrikes,
        Adrenaline,
        Overcast,
        Sacrifice,
        Activation,
        Recharge,
        Aftercast,

        Description,
        Concise,

        Attribute,
        Profession,
        Campaign,
        Range,
        Parsed,

        COUNT,
    };

    constexpr size_t PROP_COUNT = static_cast<size_t>(SkillTextPropertyID::COUNT);

    using SkillCatalog = Catalog<SkillTextPropertyID, GW::Constants::SkillMax>;

    uint32_t focused_agent_id = 0;
    std::array<int8_t, AttributeOrTitle::Count> attributes = {};

    static constexpr size_t SkillCount = GW::Constants::SkillMax - 1;
    static constexpr size_t DescriptionCount = SkillCount * 21 * 2;

    size_t GetDescriptionIndex(uint16_t skill_id, int8_t attribute_level, bool is_concise)
    {
        auto page = (attribute_level + 1) * 2 + is_concise;
        return SkillCount * page + skill_id + 1;
    }

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

        static constexpr GW::Vec2f SLASH_SIZE{13, 15};
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
    RichText::Drawer text_drawer{nullptr, &RichText::DefaultTextImageProvider::Instance(), &frac_provider};

    StringArena<char> prop_bundle_names;
    std::vector<SkillCatalog::T_propset> prop_bundles;

    struct PropBundleSection
    {
        uint32_t start_index = 0;
        uint32_t end_index = 0;
    };

    PropBundleSection footer_bundle_section;

    void SetupPropBundles()
    {
        auto SetupBundle = [&](std::string_view name, SkillCatalog::T_propset propset)
        {
            prop_bundle_names.append_range(name);
            prop_bundle_names.CommitWritten();

            prop_bundles.push_back(propset);
        };

        struct SectionRecordingScope
        {
            SectionRecordingScope(PropBundleSection *dst) : dst(dst)
            {
                dst->start_index = static_cast<uint32_t>(prop_bundles.size());
            }
            ~SectionRecordingScope()
            {
                dst->end_index = static_cast<uint32_t>(prop_bundles.size());
            }

        private:
            PropBundleSection *dst;
        };

        const std::string capacity_hint_key = "prop_bundle_names";
        prop_bundle_names.ReserveFromHint(capacity_hint_key);

        SetupBundle("Any", SkillCatalog::ALL_PROPS);
        SetupBundle("Name", SkillCatalog::MakePropset({SkillTextPropertyID::Name}));
        SetupBundle("Type", SkillCatalog::MakePropset({SkillTextPropertyID::Type}));
        SetupBundle("Tags", SkillCatalog::MakePropset({SkillTextPropertyID::Tag}));
        SetupBundle("Energy", SkillCatalog::MakePropset({SkillTextPropertyID::Energy}));
        SetupBundle("Recharge", SkillCatalog::MakePropset({SkillTextPropertyID::Recharge}));
        SetupBundle("Activation", SkillCatalog::MakePropset({SkillTextPropertyID::Activation}));
        SetupBundle("Aftercast", SkillCatalog::MakePropset({SkillTextPropertyID::Aftercast}));
        SetupBundle("Sacrifice", SkillCatalog::MakePropset({SkillTextPropertyID::Sacrifice}));
        SetupBundle("Overcast", SkillCatalog::MakePropset({SkillTextPropertyID::Overcast}));
        SetupBundle("Adrenaline", SkillCatalog::MakePropset({SkillTextPropertyID::AdrenalineStrikes}));
        SetupBundle("Upkeep", SkillCatalog::MakePropset({SkillTextPropertyID::Upkeep}));
        SetupBundle("Description", SkillCatalog::MakePropset({SkillTextPropertyID::Description, SkillTextPropertyID::Concise}));

        {
            SectionRecordingScope section(&footer_bundle_section);
            SetupBundle("Attribute", SkillCatalog::MakePropset({SkillTextPropertyID::Attribute}));
            SetupBundle("Profession", SkillCatalog::MakePropset({SkillTextPropertyID::Profession}));
            SetupBundle("Campaign", SkillCatalog::MakePropset({SkillTextPropertyID::Campaign}));
        }

        prop_bundle_names.StoreCapacityHint(capacity_hint_key);
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

    static std::unordered_map<SkillTextPropertyID, RichText::RichTextArena> static_props;

    template <typename Func>
    void FetchX(SkillTextPropertyID prop_id, Func &&func)
    {
        auto &prop = static_props[prop_id];
        prop.Reset();
        auto id = std::format("prop_{}", (size_t)prop_id);
        prop.ReserveFromHint(id);

        for (size_t i = 1; i < GW::Constants::SkillMax; ++i)
        {
            auto skill_id = static_cast<GW::Constants::SkillID>(i);
            auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);
            std::invoke(std::forward<Func>(func), prop, custom_sd);
            prop.CommitWrittenToIndex(i);
        }

        prop.StoreCapacityHint(id);
    }

    template <typename MemPtr>
    void FetchXMember(SkillTextPropertyID prop_id, MemPtr getter, std::optional<uint32_t> icon_id = std::nullopt)
    {
        static_assert(std::is_member_function_pointer_v<MemPtr>, "This overload is for member function pointers only.");

#define ACTUAL_CODE                                                         \
    if constexpr (std::is_integral_v<Ret> || std::is_floating_point_v<Ret>) \
    {                                                                       \
        auto value = static_cast<double>(std::invoke(getter, cskill));      \
        if (value != 0.0)                                                   \
        {                                                                   \
            dst.AppendWriteBuffer(32, DoubleWriter{value});                 \
            if (icon_id.has_value())                                        \
                dst.PushImageTag(icon_id.value());                          \
        }                                                                   \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        dst.append_range(std::invoke(getter, cskill));                      \
    }

        FetchX(
            prop_id,
            [getter, icon_id](RichText::RichTextArena &dst, CustomSkillData &cskill)
            {
                // Try const-qualified call first
                if constexpr (std::is_invocable_v<MemPtr, const CustomSkillData &>)
                {
                    using Ret = std::invoke_result_t<MemPtr, const CustomSkillData &>;
                    ACTUAL_CODE
                }
                else
                {
                    // Non-const member function
                    using Ret = std::invoke_result_t<MemPtr, CustomSkillData &>;
                    ACTUAL_CODE
                }
            }
        );
    }

    void FetchTags()
    {
        FetchX(
            SkillTextPropertyID::Tag,
            [](RichText::RichTextArena &dst, CustomSkillData &cskill)
            {
                auto &skill = *cskill.skill;
                auto &tags = cskill.tags;
                auto skill_id_to_check = skill.IsPvP() ? skill.skill_id_pvp : skill.skill_id;

                bool is_unlocked = GW::SkillbarMgr::GetIsSkillUnlocked(skill_id_to_check);
                bool is_equipable = Utils::IsSkillEquipable(skill, focused_agent_id);
                bool is_locked = tags.Unlockable && !is_unlocked;

                auto PushTag = [&](std::string_view str, ImU32 color = NULL, std::optional<size_t> image_id = std::nullopt)
                {
                    if (color != NULL)
                    {
                        dst.PushColoredText(color, str);
                    }
                    else
                    {
                        dst.PushText(str);
                    }

                    if (image_id.has_value())
                    {
                        dst.PushText(" ");
                        dst.PushImageTag(image_id.value());
                    }

                    dst.PushText(", ");
                };

                // clang-format off
                if (is_equipable)          PushTag("Equipable", IM_COL32(100, 255, 255, 255));
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
                if (tags.MonsterSkill)     PushTag("Monster Skill ", NULL, RichText::DefaultTextImageProvider::MonsterSkull);
                if (tags.SpiritAttack)     PushTag("Spirit Attack");
                
                if (tags.Maintained)       PushTag("Maintained");
                if (tags.ConditionSource)  PushTag("Condition Source");
                if (tags.ExploitsCorpse)   PushTag("Exploits Corpse");
                if (tags.Consumable)       PushTag("Consumable");
                if (tags.Celestial)        PushTag("Celestial");
                if (tags.Mission)          PushTag("Mission");
                if (tags.Bundle)           PushTag("Bundle");
                // clang-format on

                if (dst.GetWrittenSize() > 0)
                {
                    // Cut off the last ", "
                    dst.resize(dst.size() - 2);
                }
            }
        );
    }

    void FetchStaticProps()
    {
        FetchTags();

        FetchXMember(SkillTextPropertyID::Type, &CustomSkillData::GetTypeString);
        FetchXMember(SkillTextPropertyID::Attribute, &CustomSkillData::GetAttributeStr);
        FetchXMember(SkillTextPropertyID::Profession, &CustomSkillData::GetProfessionStr);
        FetchXMember(SkillTextPropertyID::Campaign, &CustomSkillData::GetCampaignStr);
        FetchXMember(SkillTextPropertyID::Upkeep, &CustomSkillData::GetUpkeep, RichText::DefaultTextImageProvider::Upkeep);
        FetchXMember(SkillTextPropertyID::Energy, &CustomSkillData::GetEnergy, RichText::DefaultTextImageProvider::EnergyOrb);
        FetchXMember(SkillTextPropertyID::AdrenalineStrikes, &CustomSkillData::GetAdrenalineStrikes, RichText::DefaultTextImageProvider::Adrenaline);
        FetchXMember(SkillTextPropertyID::Overcast, &CustomSkillData::GetOvercast, RichText::DefaultTextImageProvider::Overcast);
        FetchXMember(SkillTextPropertyID::Sacrifice, &CustomSkillData::GetSacrifice, RichText::DefaultTextImageProvider::Sacrifice);
        FetchXMember(SkillTextPropertyID::Recharge, &CustomSkillData::GetRecharge, RichText::DefaultTextImageProvider::Recharge);

        auto AppendFraction = [](RichText::RichTextArena &dst, double value)
        {
            float value_int;
            float value_fract = std::modf(value, &value_int);

            std::optional<RichText::FracTag> frac_tag = std::nullopt;
            if (value_fract == 0.25f)
                frac_tag = {1, 4};
            else if (value_fract == 0.5f)
                frac_tag = {1, 2};
            else if (value_fract == 0.75f)
                frac_tag = {3, 4};

            if (frac_tag.has_value())
            {
                if (value_int > 0.f)
                    dst.AppendWriteBuffer(32, DoubleWriter{value_int});
                dst.PushTag(RichText::TextTag(frac_tag.value()));
            }
            else
            {
                dst.AppendWriteBuffer(32, DoubleWriter{value});
            }
        };

        FetchX(
            SkillTextPropertyID::Adrenaline,
            [&](RichText::RichTextArena &dst, CustomSkillData &cskill)
            {
                auto adrenaline = cskill.GetAdrenaline();
                if (adrenaline == 0)
                    return;
                uint16_t adrenaline_div = adrenaline / 25;
                uint16_t adrenaline_rem = adrenaline % 25;
                if (adrenaline_div > 0)
                {
                    dst.AppendWriteBuffer(32, DoubleWriter{(double)adrenaline_div});
                }
                if (adrenaline_rem > 0)
                {
                    dst.PushTag(RichText::TextTag(RichText::FracTag{adrenaline_rem, 25}));
                }
                dst.PushImageTag(RichText::DefaultTextImageProvider::Adrenaline); }
        );

        FetchX(
            SkillTextPropertyID::Activation,
            [&](RichText::RichTextArena &dst, CustomSkillData &cskill)
            {
                auto activation = cskill.GetActivation();
                if (activation == 0.f)
                    return;
                AppendFraction(dst, activation);
                dst.PushImageTag(RichText::DefaultTextImageProvider::Activation);
            }
        );

        FetchX(
            SkillTextPropertyID::Aftercast,
            [&](RichText::RichTextArena &dst, CustomSkillData &cskill)
            {
                auto &skill = *cskill.skill;
                const bool is_normal_aftercast = (skill.activation > 0 && skill.aftercast == 0.75f) ||
                                                 (skill.activation == 0 && skill.aftercast == 0);
                if (skill.aftercast == 0.f && is_normal_aftercast)
                    return;

                if (!is_normal_aftercast)
                    dst.PushColorTag(IM_COL32(255, 255, 0, 255));
                AppendFraction(dst, skill.aftercast);
                if (!is_normal_aftercast)
                    dst.PushColorTag(NULL);
                dst.PushImageTag(RichText::DefaultTextImageProvider::Aftercast);
            }
        );

        FetchX(
            SkillTextPropertyID::Range,
            [](RichText::RichTextArena &dst, CustomSkillData &cskill)
            {
                Utils::Range buffer[4];
                std::span<Utils::Range> ranges(buffer);
                cskill.GetRanges(ranges);
                for (auto range : ranges)
                {
                    dst.AppendWriteBuffer(32, DoubleWriter{static_cast<double>(range)});
                    auto range_name = Utils::GetRangeStr(range);
                    if (range_name)
                    {
                        std::format_to(std::back_inserter(dst), std::string_view(" ({})"), range_name.value());
                    }
                }
            }
        );
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
        BookState()
        {
            FetchProperties();
        }

        struct Settings
        {
            AttributeMode attribute_mode = AttributeMode::Generic;
            int attr_lvl_slider = 0;

            bool include_pve_only_skills = true;
            bool include_temporary_skills = false;
            bool include_npc_skills = false;
            bool include_archived_skills = false;
            bool include_disguises = false;

            bool use_precise_adrenaline = false;
            bool prefer_concise_descriptions = false;
            bool limit_to_characters_professions = false;
            bool show_null_stats = false;
            bool snap_to_skill = true;
        };
        struct WindowDims
        {
            ImVec2 window_pos;
            ImVec2 window_size;
        };
        std::optional<WindowDims> init_dims = std::nullopt;
        Settings settings;
        SkillCatalog catalog{prop_bundle_names, prop_bundles};
        RichText::RichTextArena descs[2]{};
        std::vector<uint16_t> filtered_skills; // skill ids
        CatalogUtils::Query query;
        CatalogUtils::HLData hl_data{SkillCatalog::PROP_COUNT, prop_bundles.size(), GW::Constants::SkillMax};

        SkillTextProvider &text_provider = SkillTextProvider::GetInstance(GW::Constants::Language::English);

        bool is_opened = true;
        bool skills_dirty = true;
        bool request_state_update = true; // Set this to attempt to apply the pending state
        bool attrs_changed = false;
        bool state_update_in_progress = false;
        bool first_draw = true;

        struct ScrollTracking
        {
            std::vector<VariableSizeClipper::Position> scroll_positions;
            uint16_t old_input_text_len = 0;
            char input_text_history[1024] = {'\0'};

            std::string_view GetOldInputText() const
            {
                return std::string_view(input_text_history, old_input_text_len);
            }
        };
        ScrollTracking scroll_tracking;
        char input_text[1024] = {'\0'};

        std::string feedback;

        VariableSizeClipper clipper{};
        std::future<bool> update_filter_future = std::future<bool>();
        std::atomic<bool> cancel_state_update = false;

        void Duplicate(BookState &other)
        {
            other.settings = settings;
        }

        int8_t ResolveAttribute(AttributeOrTitle attr) const
        {
            // clang-format off
            switch (settings.attribute_mode) {
                case AttributeMode::Generic: return -1;
                case AttributeMode::Manual: return settings.attr_lvl_slider;
                case AttributeMode::Characters:
                    if (attr.IsNone())
                        return 0;
                    return attributes[attr.value];

                default:
                    SOFT_ASSERT(false, L"Invalid attribute mode");
                    return -1;
            }
            // clang-format on
        };

        void Update()
        {
            if (request_state_update)
            {
                request_state_update = false;
                if (update_filter_future.valid())
                {
                    // Cancel the previous update
                    cancel_state_update.store(true);
                    update_filter_future.get();
                }
                state_update_in_progress = true;
            }

            if (state_update_in_progress)
            {
                bool success = TryUpdateQuery();
                state_update_in_progress = !success;
            }
        }

        std::string_view GetBundleName(size_t bundle_id)
        {
            return std::string_view(catalog.prop_bundle_names.Get(bundle_id));
        }

        std::span<uint16_t> GetMetaHL(size_t bundle_id)
        {
            return hl_data.GetBundleHL(bundle_id);
        }

        std::string_view GetStr(SkillTextPropertyID prop, GW::Constants::SkillID skill_id)
        {
            auto prop_ptr = catalog.GetPropertyPtr(prop);
            if (prop_ptr == nullptr)
                return {};

            auto chars = prop_ptr->GetIndexed((size_t)skill_id);
            return std::string_view(chars);
        }

        std::span<uint16_t> GetHL(SkillTextPropertyID prop, GW::Constants::SkillID skill_id)
        {
            auto prop_ptr = catalog.GetPropertyPtr(prop);
            if (prop_ptr == nullptr)
                return {};

            auto span_id_opt = prop_ptr->GetSpanId((size_t)skill_id);
            if (!span_id_opt.has_value())
                return {};
            auto span_id = span_id_opt.value();

            return hl_data.GetPropHL((size_t)prop, span_id);
        }

        void FetchProperties()
        {
            assert(!static_props.empty());
            for (auto &[prop_id, prop] : static_props)
            {
                catalog.SetPropertyPtr(prop_id, &prop);
            }
            FetchNames();
            FetchDescriptions();
        }

        void FetchTags()
        {
            auto &prop = static_props[SkillTextPropertyID::Tag];
            this->catalog.SetPropertyPtr(SkillTextPropertyID::Tag, &prop);
        }

        void FetchNames()
        {
            auto &text_provider = SkillTextProvider::GetInstance(GW::Constants::Language::English);
            auto names_ptr = &text_provider.GetNames();
            this->catalog.SetPropertyPtr(SkillTextPropertyID::Name, names_ptr);
        }

        void FetchDescriptions()
        {
            assert(text_provider.IsReady());
            for (auto is_concise : {false, true})
            {
                auto &desc = descs[is_concise];
                auto id = is_concise ? "skill_descriptions" : "skill_concise_descriptions";
                desc.Reset();
                desc.ReserveFromHint(id);
                for (size_t i = 1; i < GW::Constants::SkillMax; ++i)
                {
                    auto skill_id = static_cast<GW::Constants::SkillID>(i);
                    auto &cskill = CustomSkillDataModule::GetCustomSkillData(skill_id);
                    auto attr_lvl = ResolveAttribute(cskill.attribute);
                    desc.AppendWriteBuffer(
                        512,
                        [&](std::span<char> &buffer)
                        {
                            text_provider.MakeDescription(skill_id, is_concise, attr_lvl, buffer);
                        }
                    );
                    desc.CommitWrittenToIndex(i);
                }
                desc.StoreCapacityHint(id);
                auto prop_id = is_concise ? SkillTextPropertyID::Concise : SkillTextPropertyID::Description;
                catalog.SetPropertyPtr(prop_id, &desc);
            }
        }

        // Draw's a button to duplicate the current book
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
                book->init_dims = {window->Pos + ImVec2(16, 16), window->Size};
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Open new book");
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

                skills_dirty |= ImGui::Checkbox("Include PvE-only skills", &settings.include_pve_only_skills);
                skills_dirty |= ImGui::Checkbox("Include Temporary skills", &settings.include_temporary_skills);
                skills_dirty |= ImGui::Checkbox("Include NPC skills", &settings.include_npc_skills);
                skills_dirty |= ImGui::Checkbox("Include Archived skills", &settings.include_archived_skills);
                skills_dirty |= ImGui::Checkbox("Include Disguises", &settings.include_disguises);

                ImGui::NextColumn();

                ImGui::Checkbox("Show precise adrenaline", &settings.use_precise_adrenaline);
                ImGui::Checkbox("Show null stats", &settings.show_null_stats);
                ImGui::Checkbox("Snap to skill", &settings.snap_to_skill);
                ImGui::Checkbox("Prefer concise descriptions", &settings.prefer_concise_descriptions);
                skills_dirty |= ImGui::Checkbox("Limit to character's professions", &settings.limit_to_characters_professions);

                ImGui::Columns(1);
            }
        }

        void UpdateScrollTracking(std::string_view input_text)
        {
            assert(input_text.size() < sizeof(scroll_tracking.input_text_history));
            auto old_input_text = this->scroll_tracking.GetOldInputText();
            size_t common_len = std::mismatch(input_text.begin(), input_text.end(), scroll_tracking.input_text_history).first - input_text.begin();
            auto target_scroll = clipper.GetTargetScroll();
            if (common_len < input_text.size())
            {
                scroll_tracking.scroll_positions.resize(common_len); // Discard old history
                auto dst = scroll_tracking.input_text_history + common_len;
                auto src = input_text.data() + common_len;
                auto size = input_text.size() - common_len;
                std::memcpy(dst, src, size);
                dst[size] = '\0';
                clipper.Reset();
            }
            else
            {
                // Restore old scroll
                if (input_text.size() < scroll_tracking.scroll_positions.size())
                {
                    auto old_scroll = scroll_tracking.scroll_positions[input_text.size()];
                    clipper.SetScroll(old_scroll);
                }
            }
            if (scroll_tracking.scroll_positions.size() < old_input_text.size() + 1)
                scroll_tracking.scroll_positions.resize(old_input_text.size() + 1, target_scroll); // Save new scroll
            scroll_tracking.scroll_positions[old_input_text.size()] = target_scroll;               // Overwrite old scroll
            scroll_tracking.old_input_text_len = input_text.size();
        }

        void DrawAttributeModeSelection()
        {
            auto cursor_before = ImGui::GetCursorPos();
            ImGui::SetCursorPosY(cursor_before.y + 2);
            ImGui::TextUnformatted("Attributes");
            ImGui::SameLine();
            ImGui::SetCursorPosY(cursor_before.y);

            if (ImGui::RadioButton("(0...15)", settings.attribute_mode == AttributeMode::Generic))
            {
                settings.attribute_mode = AttributeMode::Generic;
                request_state_update = true;
                FetchDescriptions();
            }

            ImGui::SameLine();
            if (ImGui::RadioButton("Character's", settings.attribute_mode == AttributeMode::Characters))
            {
                settings.attribute_mode = AttributeMode::Characters;
                request_state_update = true;
                FetchDescriptions();
            }

            ImGui::SameLine();
            if (ImGui::RadioButton("Manual", settings.attribute_mode == AttributeMode::Manual))
            {
                settings.attribute_mode = AttributeMode::Manual;
                request_state_update = true;
                FetchDescriptions();
            }

            if (settings.attribute_mode == AttributeMode::Manual)
            {
                if (ImGui::SliderInt("Attribute level", &settings.attr_lvl_slider, 0, 21))
                {
                    request_state_update = true;
                    FetchDescriptions();
                }
            }
        }

        DWORD state_update_start_timestamp = 0;
        bool TryUpdateQuery()
        {
#ifdef ASYNC_FILTERING
            if (!update_filter_future.valid())
#endif
            {
                if (state_update_start_timestamp == 0)
                    state_update_start_timestamp = GW::MemoryMgr::GetSkillTimer();

                if (base_skills.empty())
                {
                    if (!TryInitBaseSkills())
                        return false;
                }

                query.Clear();
                auto input_text_view = std::string_view(input_text, strlen(input_text));
                UpdateScrollTracking(input_text_view);
                CatalogUtils::ParseQuery(input_text_view, catalog.prop_bundle_names, query);

                hl_data.Reset();
                filtered_skills.clear();

                if (settings.attribute_mode == AttributeMode::Characters)
                {
                    auto custom_ad = CustomAgentDataModule::GetCustomAgentData(focused_agent_id);
                    for (uint32_t i = 0; i < attributes.size(); i++)
                    {
                        auto attr_lvl = custom_ad.GetAttribute((AttributeOrTitle)i).value_or(0);
                        attributes[i] = attr_lvl;
                    }
                }
            }

            uint32_t prof_mask;
            if (settings.limit_to_characters_professions)
            {
                prof_mask = Utils::GetProfessionMask(focused_agent_id);
            }

#ifdef _ARTIFICIAL_DELAY
            // Artificially fail 64 times to test long delays
            static uint32_t artificial_fail_counter = 0;
            if ((++artificial_fail_counter % 64) != 0)
                return false;
#endif

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

            auto filter_task = [this]()
            {
                this->catalog.RunQuery(this->query, this->filtered_skills, hl_data);

#ifdef _TIMING
                auto timestamp_filtering = GW::MemoryMgr::GetSkillTimer();
                auto duration = timestamp_filtering - state_update_start_timestamp;
                GW::GameThread::Enqueue(
                    [=]()
                    {
                        Utils::FormatToChat(L"Filtering took {} ms", duration);
                    }
                );
#endif

                // for (auto &command : parsed_commands)
                // {
                //     ApplyCommand(command, filtered_skills);
                // }

#ifdef _TIMING
                auto timestamp_commands = GW::MemoryMgr::GetSkillTimer();
                duration = timestamp_commands - timestamp_filtering;
                GW::GameThread::Enqueue(
                    [=]()
                    {
                        Utils::FormatToChat(L"Applying commands took {} ms", duration);
                    }
                );
#endif
                return true;
            };

#ifdef ASYNC_FILTERING
            update_filter_future = std::async(std::launch::async, filter_task);
            return false;
        }
        else
        {
            bool is_ready = update_filter_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
            if (!is_ready)
                return false;

            assert(update_filter_future.valid());
            bool success = update_filter_future.get();
#else
            bool success = filter_task();
#endif
            if (!success)
                return false;

            state_update_start_timestamp = 0;

            CatalogUtils::GetFeedback(query, catalog.prop_bundle_names, feedback);

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

            return true;
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
                SkillTextPropertyID id;
                size_t atlas_index;
                size_t pos_from_right;
            };

            Layout layout[]{
                {SkillTextPropertyID::Overcast, 10, 4},
                {SkillTextPropertyID::Sacrifice, 7, 4},
                {SkillTextPropertyID::Upkeep, 0, 4},
                {SkillTextPropertyID::Energy, 1, 3},
                {settings.use_precise_adrenaline ? SkillTextPropertyID::Adrenaline : SkillTextPropertyID::AdrenalineStrikes, 3, 3},
                {SkillTextPropertyID::Activation, 2, 2},
                {SkillTextPropertyID::Recharge, 1, 1},
                {SkillTextPropertyID::Aftercast, 2, 0},
            };

            for (const auto &l : layout)
            {
                const auto text = GetStr(l.id, skill_id);
                if (text.empty())
                    continue;
                const auto hl = GetHL(l.id, skill_id);
                RichText::TextSegment seg_alloc[16];
                std::span<RichText::TextSegment> seg_span = seg_alloc;
                text_drawer.MakeTextSegments(text, seg_span, hl);
                const auto text_width = RichText::CalcTextSegmentsWidth(seg_span);
                float start_x = max_pos_x - l.pos_from_right * width_per_stat - text_width;
                float current_x = std::max(start_x, min_pos_x);
                const auto ls_text_cursor = ImVec2(current_x, base_pos_y + 4);
                ImGui::SetCursorPos(ls_text_cursor);

                text_drawer.DrawTextSegments(seg_span, 0, -1);

                current_x += text_width;
                min_pos_x = current_x + 5;
            }

            ImGui::PopFont();
            ImGui::EndGroup();
        }

        void DrawSkillHeader(CustomSkillData &custom_sd)
        {
            ImGui::BeginGroup(); // Whole header

            const auto &skill = *custom_sd.skill;
            const auto skill_id = custom_sd.skill_id;

            { // Draw skill icon
                const auto skill_icon_size = 56;
                auto icon_cursor_ss = ImGui::GetCursorScreenPos();
                auto result = Utils::GetSkillFrame(custom_sd.skill_id);
                bool is_equipable = Utils::IsSkillEquipable(*custom_sd.skill, focused_agent_id);
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

                    auto name_str = GetStr(SkillTextPropertyID::Name, skill_id);
                    auto name_hl = GetHL(SkillTextPropertyID::Name, skill_id);
                    RichText::TextSegment seg_alloc[256];
                    std::span<RichText::TextSegment> seg_span = seg_alloc;
                    text_drawer.MakeTextSegments(name_str, seg_span, name_hl);
                    text_drawer.DrawTextSegments(seg_span, wrapping_min, wrapping_max);

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
                            for (size_t i = 0; i <= seg_span.size(); i++)
                            {
                                auto seg_width = i < seg_span.size() ? seg_span[i].width : std::numeric_limits<float>::max();
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
                    auto str = GetStr(SkillTextPropertyID::Type, skill_id);
                    auto hl = GetHL(SkillTextPropertyID::Type, skill_id);
                    text_drawer.DrawRichText(str, wrapping_min, wrapping_max, hl);
                }

                { // Draw skill tags
                    auto str = GetStr(SkillTextPropertyID::Tag, skill_id);
                    auto hl = GetHL(SkillTextPropertyID::Tag, skill_id);
                    ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
                    text_drawer.DrawRichText(str, wrapping_min, wrapping_max, hl);
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
                    book.request_state_update = true;

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

            if (state_update_start_timestamp)
            {
                DWORD now_timestamp = GW::MemoryMgr::GetSkillTimer();
                float elapsed_sec = (float)(now_timestamp - state_update_start_timestamp) / 1000.0f;

                if (elapsed_sec > 0.2f)
                {
                    // Draw spinning arrow
                    auto packet = TextureModule::GetPacket_ImageInAtlas(
                        TextureModule::KnownFileIDs::UI_SkillStatsIcons,
                        ImVec2(23, 23),
                        ImVec2(16, 16),
                        3,
                        ImVec2(0, 0),
                        ImVec2(0, 0),
                        ImColor(0.95f, 0.6f, 0.2f)
                    );
                    auto rads = 2.f * elapsed_sec * 6.28f;
                    auto draw_list = ImGui::GetWindowDrawList();
                    packet.AddToDrawList(draw_list, ImVec2(pos.x + 20, pos.y), rads);
                }
            }

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

            auto type = settings.prefer_concise_descriptions ? SkillTextPropertyID::Concise : SkillTextPropertyID::Description;
            auto alt_type = settings.prefer_concise_descriptions ? SkillTextPropertyID::Description : SkillTextPropertyID::Concise;

            auto main_desc = GetStr(type, skill_id);
            auto alt_desc = GetStr(alt_type, skill_id);
            auto main_hl = GetHL(type, skill_id);
            auto alt_hl = GetHL(alt_type, skill_id);

            text_drawer.DrawRichText(main_desc, 0, work_width, main_hl);

            bool draw_alt = alt_hl.size() > main_hl.size();
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

                text_drawer.DrawRichText(alt_desc, 0, work_width, alt_hl);
            }
        }

        void DrawSkillFooter(CustomSkillData &custom_sd, float work_width)
        {
            auto &skill = *custom_sd.skill;
            auto skill_id = custom_sd.skill_id;
            ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            for (auto bundle_id = footer_bundle_section.start_index; bundle_id < footer_bundle_section.end_index; ++bundle_id)
            {
                auto prop_set = catalog.prop_bundles[bundle_id];
                bool has_drawn_header = false;
                for (Utils::BitsetIterator it(prop_set); !it.IsDone(); it.Next())
                {
                    auto prop_id = (SkillTextPropertyID)it.index;
                    auto str = GetStr(prop_id, skill_id);
                    if (str.empty())
                        continue;

                    if (!has_drawn_header)
                    {
                        auto meta = GetBundleName(bundle_id);
                        auto meta_hl = GetMetaHL(bundle_id);
                        text_drawer.DrawRichText(meta, 0, work_width, meta_hl);
                        ImGui::SameLine();
                        text_drawer.DrawRichText(": ", 0, work_width);
                        ImGui::SameLine();
                        has_drawn_header = true;
                    }
                    auto str_hl = GetHL(prop_id, skill_id);
                    text_drawer.DrawRichText(str, 0, work_width, str_hl);
                }
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            { // Draw skill id
                ImGui::SetWindowFontScale(0.7f);
                FixedVector<char, 32> id_str;
                id_str.AppendIntToChars((uint32_t)skill.skill_id);
                const auto id_str_size = ImGui::CalcTextSize(id_str.data(), id_str.data_end());
                ImGui::SetCursorPosX(work_width - id_str_size.x - 4);
                ImVec4 color(1, 1, 1, 0.3f);
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(id_str.data(), id_str.data_end());
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(1.f);
            }
        }

        void MakeBookName(std::span<char> buf, size_t book_index)
        {
            SpanWriter<char> name_writer = buf;
            name_writer.AppendRange(std::string_view("Skill Book (Ctrl + K)"));
            if (book_index > 0)
            {
                name_writer.AppendFormat(" ({})", book_index);
            }

            // Add unique id: Hex-string of pointer to BookState
            name_writer.AppendRange(std::string_view("###"));
            name_writer.AppendIntToChars(reinterpret_cast<size_t>(this), 16);

            name_writer.push_back('\0');
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

            char name_buf[64];
            MakeBookName(name_buf, book_index);

            if (ImGui::Begin(name_buf, &is_opened, UpdateManager::GetWindowFlags()))
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
                    if (!text_provider.IsReady())
                    {
                        ImGui::Text("Loading...");
                    }
                    else
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

                            DrawSkillHeader(custom_sd);
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
                                        auto name_str = GetStr(SkillTextPropertyID::Name, skill_id);
                                        Utils::OpenWikiPage(name_str);
                                    }
                                    else
                                    {
                                        if (Utils::IsSkillEquipable(*custom_sd.skill, focused_agent_id))
                                        {
                                            UpdateManager::RequestSkillDragging(skill_id);
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
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
    };

    GW::HookEntry attribute_update_entry;
    GW::HookEntry select_hero_entry;
    constexpr GW::UI::UIMessage kLoadHeroSkillsMessage = (GW::UI::UIMessage)(0x10000000 | 395);
    constexpr GW::UI::UIMessage kSelectHeroMessage = (GW::UI::UIMessage)(0x10000000 | 432);
    constexpr GW::UI::UIMessage kChangeProfession = (GW::UI::UIMessage)(0x10000000 | 78);

    void AttributeUpdateCallback(GW::HookStatus *, const GW::Packet::StoC::PacketBase *packet)
    {
        const auto p = reinterpret_cast<const GW::Packet::StoC::AttributeUpdatePacket *>(packet);
        auto packet_agent_id = p->agent_id;

        for (auto &book : books)
        {
            if (book->settings.attribute_mode == AttributeMode::Characters)
            {
                if (packet_agent_id == focused_agent_id)
                {
                    // auto attr = AttributeOrTitle((GW::Constants::AttributeByte)p->attribute);
                    // attributes[attr.value] = p->value;
                    book->attrs_changed = true;
                    book->FetchDescriptions();
                }
            }
        }
    }
    void TitleUpdateCallback(GW::HookStatus *, const GW::Packet::StoC::UpdateTitle *packet)
    {
        for (auto &book : books)
        {
            if (book->settings.attribute_mode == AttributeMode::Characters)
            {
                // auto attr = AttributeOrTitle((GW::Constants::TitleID)packet->title_id);
                // attributes[attr.value] = packet->new_value;
                book->attrs_changed = true;
            }
        }
    }

    void LoadHeroSkillsCallback(GW::HookStatus *, GW::UI::UIMessage msg, void *wparam, void *lparam)
    {
        // This is called whenever a new set of skills are loaded in the deckbuilder
        assert(msg == kLoadHeroSkillsMessage);

        auto agent_id = (uint32_t)wparam;
        if (agent_id > 0)
        {
            focused_agent_id = agent_id;
            FetchTags(); // Why this here?
            for (auto &book : books)
            {
                book->FetchTags();
                book->request_state_update = true;
            }
        }
    }

    void SelectHeroCallback(GW::HookStatus *, GW::UI::UIMessage msg, void *wparam, void *lparam)
    {
        // This is called whenever the user selects a hero in either the deckbuilder or the inventory,
        // but it doesn't come with any params/info
        assert(msg == kSelectHeroMessage);

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
            book->request_state_update = true;
        }
    }

    void ChangeProfessionCallback(GW::HookStatus *, GW::UI::UIMessage msg, void *wparam, void *lparam)
    {
        auto p = (uint32_t *)wparam;
        auto agent_id = p[0];
        if (agent_id == focused_agent_id)
        {
            // auto primary = p[1];
            // auto secondary = p[2];
            for (auto &book : books)
            {
                book->request_state_update = true;
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

    void Initialize()
    {
        GW::StoC::RegisterPacketCallback(&attribute_update_entry, GAME_SMSG_AGENT_UPDATE_ATTRIBUTE, AttributeUpdateCallback);
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::UpdateTitle>(&attribute_update_entry, TitleUpdateCallback);

        GW::UI::RegisterUIMessageCallback(&select_hero_entry, kLoadHeroSkillsMessage, LoadHeroSkillsCallback);
        GW::UI::RegisterUIMessageCallback(&select_hero_entry, kSelectHeroMessage, SelectHeroCallback);
        GW::UI::RegisterUIMessageCallback(&select_hero_entry, GW::UI::UIMessage::kMapLoaded, MapLoadedCallback);
        GW::UI::RegisterUIMessageCallback(&select_hero_entry, kChangeProfession, ChangeProfessionCallback);

        ForceDeckbuilderCallbacks();

        SetupPropBundles();
        FetchStaticProps();
        AddBook(); // Add first book
    }

    void Terminate()
    {
        GW::StoC::RemoveCallbacks(&attribute_update_entry);

        GW::UI::RemoveUIMessageCallback(&select_hero_entry, kLoadHeroSkillsMessage);
        GW::UI::RemoveUIMessageCallback(&select_hero_entry, kSelectHeroMessage);
        GW::UI::RemoveUIMessageCallback(&select_hero_entry, GW::UI::UIMessage::kMapLoaded);
        GW::UI::RemoveUIMessageCallback(&select_hero_entry, kChangeProfession);
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

    uint32_t GetHighlightKey(SkillPropertyID::Type target, GW::Constants::SkillID skill_id = GW::Constants::SkillID::No_Skill)
    {
        assert((uint32_t)skill_id < 0x10000);
        assert((uint32_t)target < 0x10000);
        return (((uint32_t)skill_id) << 16) | (uint32_t)target;
    }

    template <class _Container>
    void GetFormattedHeader(const SkillPropertyID::Type type, std::back_insert_iterator<_Container> it)
    {
        auto prop_name = SkillPropertyID{type}.ToStr();
        std::format_to(it, "{}: ", prop_name);
    }

    void DrawFormattedHeader(const SkillPropertyID::Type type, GW::Constants::SkillID skill_id)
    {
        char header[32];

        // FixedString<32> header;
        // GetFormattedHeader(type, std::back_inserter(header));

        auto window = ImGui::GetCurrentWindow();
        auto work_width = window->WorkRect.GetWidth();
        auto cursor_x = ImGui::GetCursorPosX();

        // auto has_header_hl = active_state->highlighting_map[GetHighlightKey(type, skill_id)].did_match_header;
        // FixedVector<uint16_t, 2> header_hl;
        // if (has_header_hl)
        // {
        //     header_hl.push_back(0);
        //     header_hl.push_back(header.size() - 1);
        // }
        ImGui::PushFont(Constants::Fonts::gw_font_16);
        ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
        Utils::DrawMultiColoredText(header, cursor_x, work_width, {}, {});
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    void DrawFormattedHeaderTooltip(const SkillPropertyID::Type type, GW::Constants::SkillID skill_id)
    {
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            bool untouched = IsInitialCursorPos();
            if (untouched)
            {
                DrawFormattedHeader(type, skill_id);
            }
            ImGui::EndTooltip();
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

    std::string_view PopWord(std::string_view &str)
    {
        char *p = (char *)str.data();
        char *end = p + str.size();
        while (p < end && Utils::IsSpace(*p))
            ++p;

        char *word_start = p;
        bool is_alpha = Utils::IsAlpha(*p);
        ++p;
        if (is_alpha)
        {
            while (p < end && Utils::IsAlpha(*p))
                ++p;
        }

        str = std::string_view(p, end - p);
        return std::string_view(word_start, p - word_start);
    }

    SkillPropertyID ParseFilterTarget(std::string_view text)
    {
        SkillPropertyID target = {};

        // if (TryParseRawFilterTarget(p, end, target))
        // {
        //     return target;
        // }

        auto matcher = Matcher(text, true);

        size_t best_match_weight = std::numeric_limits<size_t>::max();
        // int32_t best_match_index = -1;
        for (size_t i = 0; i < std::size(filter_targets); ++i)
        {
            auto &pair = filter_targets[i];
            auto &type = pair.second;
            auto ident = SkillPropertyID{type, 0}.ToStr();

            bool match = matcher.Matches(ident, nullptr);
            if (match)
            {
                float match_weight = ident.size();
                if (match_weight < best_match_weight)
                {
                    best_match_weight = match_weight;
                    target.type = type;
                }
            }
        }

        return target;
    }

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