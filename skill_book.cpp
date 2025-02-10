#include <Windows.h>
#include <bitset>
#include <codecvt>
#include <d3d9.h>
#include <filesystem>
#include <future>
#include <iostream>
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
#include <constants.h>
#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <debug.h>
#include <debug_display.h>
#include <party_data.h>
#include <texture_module.h>
#include <update_manager.h>
#include <utils.h>
#include <variable_size_clipper.h>

#include "skill_book.h"

#ifdef _DEBUG
// #define _ARTIFICIAL_DELAY 1
#endif

// This adds a "skill book" window where the user can search and filter all skills in the game.
namespace HerosInsight::SkillBook
{
    enum struct AttributeMode : uint32_t
    {
        Null,
        Generic,
        Characters,
        Manual
    };

    struct BookState
    {
        uint32_t hero_index = 0;
        std::array<int8_t, AttributeOrTitle::Count> attributes = {};
        AttributeMode attribute_mode = AttributeMode::Generic;
        int attr_lvl_slider = 0;
        std::vector<uint16_t> filtered_skills;                                // skill ids
        std::unordered_map<uint32_t, std::vector<uint16_t>> highlighting_map; // key: skill id, value: start/stop offsets

        int8_t GetAttribute(AttributeOrTitle attr) const
        {
            // clang-format off
            switch (attribute_mode) {
                case AttributeMode::Generic: return -1;
                case AttributeMode::Manual: return attr_lvl_slider;
                case AttributeMode::Characters:
                    if (attr.IsNone())
                        return 0;
                    return attributes[attr.value];

                default:
                    SOFT_ASSERT(false, L"Invalid attribute mode");
                    return -1;
            }
            // clang-format on
        }
    };

    BookState state;         // The state currently in effect
    BookState pending_state; // The state that is being edited
    bool state_dirty = true; // Set this to attempt to apply the pending state

    std::vector<uint16_t> base_skills; // skill ids
    bool prefer_concise_descriptions = false;
    bool include_pve_only_skills = true;
    bool include_temporary_skills = false;
    bool include_npc_skills = false;
    bool include_archived_skills = false;
    bool include_disguises = false;
    char text_input[256];

    GW::HookEntry attribute_update_entry;
    void AttributeUpdateCallback(GW::HookStatus *, const GW::Packet::StoC::PacketBase *packet)
    {
        if (pending_state.attribute_mode == AttributeMode::Characters)
        {
            const auto p = reinterpret_cast<const GW::Packet::StoC::AttributeUpdatePacket *>(packet);
            auto target_agent_id = GW::Agents::GetHeroAgentID(pending_state.hero_index);
            auto packet_agent_id = p->agent_id;
            if (packet_agent_id == target_agent_id)
            {
                // auto attr = AttributeOrTitle((GW::Constants::AttributeByte)p->attribute);
                // pending_state.attributes[attr.value] = p->value;
                state_dirty = true;
            }
        }
    }
    void TitleUpdateCallback(GW::HookStatus *, const GW::Packet::StoC::UpdateTitle *packet)
    {
        if (pending_state.attribute_mode == AttributeMode::Characters)
        {
            // auto attr = AttributeOrTitle((GW::Constants::TitleID)packet->title_id);
            // pending_state.attributes[attr.value] = packet->new_value;
            state_dirty = true;
        }
    }

    void Initialize()
    {
        GW::StoC::RegisterPacketCallback(&attribute_update_entry, GAME_SMSG_AGENT_UPDATE_ATTRIBUTE, &AttributeUpdateCallback);
        GW::StoC::RegisterPacketCallback<GW::Packet::StoC::UpdateTitle>(&attribute_update_entry, &TitleUpdateCallback);
    }

    void Terminate()
    {
        GW::StoC::RemoveCallbacks(&attribute_update_entry);
    }

    uint8_t GetSelectedHeroIndex()
    {
        if (!UpdateManager::s_FrameArray)
            return 0;

        for (auto frame : *UpdateManager::s_FrameArray)
        {
            if (!Utils::IsFrameValid(frame))
                continue;

            if (!frame->tooltip_info || !frame->tooltip_info->payload)
                continue;

            auto str = (wchar_t *)frame->tooltip_info->payload;
            auto str_view = std::wstring_view(str, frame->tooltip_info->payload_len);
            if (str_view.compare(L"Solos Animus") == 0)
                Utils::FormatToChat(L"Solos Animus");
        }

        return 0;
    }

    const float adrenaline_den5_offset_x = 12.f;
    const float adrenaline_slash_offset_x = -5.f;
    bool use_precise_adrenaline = false;
    void DrawSlash25()
    {
        ImGui::PushFont(Constants::Fonts::skill_thick_font_15);
        const auto text_height15 = ImGui::GetTextLineHeight();
        const auto str = "½";
        const auto size = ImGui::CalcTextSize(str);
        const auto min = ImGui::GetCursorScreenPos();
        const auto max = min + size;
        const auto after1_x = 4;
        const auto after1_y = 7;
        auto clip_rect = ImRect(ImVec2(min.x, min.y + after1_y), max);
        ImGui::RenderTextClipped(min, max, str, nullptr, &size, ImVec2(0, 0), &clip_rect);
        clip_rect = ImRect(ImVec2(min.x + after1_x, min.y), ImVec2(max.x, min.y + after1_y));
        ImGui::RenderTextClipped(min, max, str, nullptr, &size, ImVec2(0, 0), &clip_rect);
        ImGui::PushFont(Constants::Fonts::skill_thick_font_9);
        const auto text_height9 = ImGui::GetTextLineHeight();
        auto bb = ImRect(min, max);
        const auto str5 = "5";
        const auto size5 = ImGui::CalcTextSize(str5);
        bb.Max.x += size5.x;
        ImGui::RenderText(min + ImVec2(adrenaline_den5_offset_x, 4), str5);
        ImGui::PopFont();
        ImGui::PopFont();

        ImGui::ItemSize(bb);
        ImGui::ItemAdd(bb, 0);
    }

    float CalcAdrenalineValueWidth(uint32_t value)
    {
        uint32_t wholes = value / 25;
        uint32_t parts = value % 25;

        float width = 0;

        if (!use_precise_adrenaline && parts > 0)
        {
            wholes++;
            parts = 0;
        }

        char buffer[16];
        if (wholes > 0)
        {
            sprintf(buffer, "%d", wholes);
            ImGui::PushFont(Constants::Fonts::skill_thick_font_15);
            width += ImGui::CalcTextSize(buffer).x;
            ImGui::PopFont();
        }

        if (parts > 0)
        {
            sprintf(buffer, "%d", parts);
            ImGui::PushFont(Constants::Fonts::skill_thick_font_9);
            width += ImGui::CalcTextSize(buffer).x;
            width += adrenaline_slash_offset_x + adrenaline_den5_offset_x;
            width += ImGui::CalcTextSize("5").x;
            ImGui::PopFont();
        }

        width += 2; // Padding

        return width;
    }

    void DrawAdrenalineValue(uint32_t value)
    {
        ImGui::BeginGroup();

        uint32_t wholes = value / 25;
        uint32_t parts = value % 25;

        if (!use_precise_adrenaline && parts > 0)
        {
            wholes++;
            parts = 0;
        }

        if (wholes > 0)
        {
            ImGui::Text("%d", wholes);
        }

        if (wholes > 0 && parts > 0)
            ImGui::SameLine(0, 0);

        if (parts > 0)
        {
            // Render the numerator
            ImGui::PushFont(Constants::Fonts::skill_thick_font_9);
            ImGui::Text("%d", parts);
            ImGui::PopFont();

            // Render the denominator
            ImGui::SameLine(0, 0); // No spacing between numerator and slash
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + adrenaline_slash_offset_x);
            DrawSlash25();
        }

        ImGui::EndGroup();
    }

    void FormatActivation(char *buffer, uint32_t len, float value)
    {
        float intpart;
        float activation_fract = std::modf(value, &intpart);
        uint32_t activation_int = static_cast<uint32_t>(intpart);

        const auto thresh = 0.001f;
        uint32_t match_id = 0;
        if ((match_id++, std::abs(activation_fract - 0.25f)) < thresh ||
            (match_id++, std::abs(activation_fract - 0.5f)) < thresh ||
            (match_id++, std::abs(activation_fract - 0.75f)) < thresh)
        {
            if (activation_int)
            {
                auto written = snprintf(buffer, len, "%d", activation_int);
                assert(written >= 0 && written < len);
                buffer += written;
                len -= written;
            }

            const char *str = nullptr;
            // clang-format off
            switch (match_id) {
                case 1: str = "¼"; break;
                case 2: str = "½"; break;
                case 3: str = "¾"; break;
            }
            // clang-format on

            auto result = snprintf(buffer, len, str);
            assert(result >= 0 && result < len);
        }
        else
        {
            auto result = snprintf(buffer, len, "%g", value);
            assert(result >= 0 && result < len);
        }
    }

    void DrawSkillStats(const CustomSkillData &custom_sd)
    {
        ImGui::PushFont(Constants::Fonts::skill_thick_font_15);

        const float width_per_stat = 45;

        const auto draw_list = ImGui::GetWindowDrawList();

        ImGui::SameLine();
        float max_pos_x = ImGui::GetWindowContentRegionWidth() - 20;
        auto cursor_start_pos = ImGui::GetCursorPos();
        float min_pos_x = cursor_start_pos.x;
        float base_pos_y = cursor_start_pos.y + 1;

        ImGui::SetCursorPosY(base_pos_y);

        const auto icon = TextureModule::LoadTextureFromFileId(TextureModule::KnownFileIDs::UI_SkillStatsIcons);
        auto DrawIcon = [&](uint32_t i, float text_width, const uint32_t icon_atlas_index, const ImVec4 tint = ImVec4(1, 1, 1, 1))
        {
            ImVec2 icon_size = ImVec2(16, 16);

            float start_x = max_pos_x - i * width_per_stat - icon_size.x - 2 - text_width;
            float current_x = start_x > min_pos_x ? start_x : min_pos_x;
            const auto text_cursor = ImVec2(current_x, base_pos_y + 4);
            current_x += text_width;
            if (icon && *icon)
            {
                current_x -= 1;
                ImGui::SetCursorPos(ImVec2(current_x, base_pos_y - 2));
                ImVec2 uv0, uv1;
                TextureModule::GetImageUVsInAtlas(*icon, icon_size, icon_atlas_index, uv0, uv1);
                icon_size = ImVec2(23, 23);
                ImGui::Image(*icon, icon_size, uv0, uv1, tint);
                current_x += icon_size.x;
            }
            current_x += 5;
            min_pos_x = current_x;
            ImGui::SetCursorPos(text_cursor);
        };

        if (auto overcast = custom_sd.GetOvercast())
        {
            char str[4];
            snprintf(str, sizeof(str), "%d", overcast);
            const auto text_width = ImGui::CalcTextSize(str).x;
            DrawIcon(4, text_width, 10);
            ImGui::TextUnformatted(str);
        }

        if (auto sacrifice = custom_sd.GetSacrifice())
        {
            char str[8];
            snprintf(str, sizeof(str), "%d%%", sacrifice);
            const auto text_width = ImGui::CalcTextSize(str).x;
            DrawIcon(4, text_width, 7);
            ImGui::TextUnformatted(str);
        }

        if (custom_sd.tags.Maintained)
        {
            const auto str = "-1";
            const auto text_width = ImGui::CalcTextSize(str).x;
            DrawIcon(4, text_width, 0);
            ImGui::TextUnformatted(str);
        }

        if (auto energy = custom_sd.GetEnergy())
        {
            char str[4];
            sprintf(str, "%d", energy);
            const auto text_width = ImGui::CalcTextSize(str).x;
            DrawIcon(3, text_width, 1);
            ImGui::TextUnformatted(str);
        }

        if (auto adrenaline = custom_sd.GetAdrenaline())
        {
            const auto text_width = CalcAdrenalineValueWidth(adrenaline);
            DrawIcon(3, text_width, 4);
            // ImGui::TextUnformatted(str);
            DrawAdrenalineValue(adrenaline);
        }

        if (auto activation = custom_sd.GetActivation())
        {
            char str[16];
            FormatActivation(str, sizeof(str), activation);
            const auto text_width = ImGui::CalcTextSize(str).x;
            DrawIcon(2, text_width, 2);
            ImGui::TextUnformatted(str);
        }

        if (auto recharge = custom_sd.GetRecharge())
        {
            char str[16];
            sprintf(str, "%d", recharge);
            const auto text_width = ImGui::CalcTextSize(str).x;
            DrawIcon(1, text_width, 3);
            ImGui::TextUnformatted(str);
        }

        if (auto aftercast = custom_sd.GetAftercast())
        {
            auto activation = custom_sd.GetActivation();
            const bool is_normal_aftercast = (activation > 0 && aftercast == 0.75f) ||
                                             (activation == 0 && aftercast == 0);
            char str[16];
            FormatActivation(str, sizeof(str), aftercast);
            if (is_normal_aftercast)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.2f);
            }
            const auto text_width = ImGui::CalcTextSize(str).x;
            DrawIcon(0, text_width, 2, ImVec4(0.3f, 0.33f, 0.7f, 1.f));
            ImGui::TextUnformatted(str);
            if (is_normal_aftercast)
            {
                ImGui::PopStyleVar();
            }
        }

        ImGui::PopFont();
    }

    enum struct FilterJoin
    {
        None,
        And,
        Or,
    };

    constexpr uint32_t MAX_TAGS = 16;
    enum struct SkillPropertyType
    {
        None,
        Unknown,

        // String
        TEXT,

        Name,
        SkillType,
        TAGS,
        TAG_END = TAGS + MAX_TAGS,
        Description,
        Concise,
        Attribute,
        Profession,
        Campaign,

        TEXT_END,

        // Number
        NUMBER,

        Overcast,
        Adrenaline,
        Sacrifice,
        Energy,
        Activation,
        Recharge,
        Aftercast,
        Range,
        ID,
        PARSED,
        PARSED_END = PARSED + (uint32_t)ParsedSkillData::Type::COUNT,

        // Raw struct data
        RAW,

        u8,
        u16,
        u32,
        f32,

        NUMBER_END,
    };

    struct SkillPropertyID
    {
        SkillPropertyType type;
        uint8_t byte_offset;

        bool IsStringType() const
        {
            return type >= SkillPropertyType::TEXT && type < SkillPropertyType::TEXT_END;
        }

        bool IsNumberType() const
        {
            return type >= SkillPropertyType::NUMBER && type < SkillPropertyType::NUMBER_END;
        }

        bool IsRawNumberType() const
        {
            return type >= SkillPropertyType::RAW && type < SkillPropertyType::NUMBER_END;
        }

        std::string ToString() const
        {
            if (type >= SkillPropertyType::PARSED && type < SkillPropertyType::PARSED_END)
            {
                ParsedSkillData temp = {};
                temp.type = static_cast<ParsedSkillData::Type>((uint32_t)type - (uint32_t)SkillPropertyType::PARSED);
                return std::string(temp.ToStr());
            }

            if (IsRawNumberType())
            {
                std::string_view type_str;
                // clang-format off
                switch (type)
                {
                    case SkillPropertyType::u8: type_str = "u8"; break;
                    case SkillPropertyType::u16: type_str = "u16"; break;
                    case SkillPropertyType::u32: type_str = "u32"; break;
                    case SkillPropertyType::f32: type_str = "f32"; break;
                    default: type_str = "..."; break;
                }
                // clang-format on
                return std::format("Data at offset {} as {}", byte_offset, type_str);
            }

            // clang-format off
            switch (type) {
                case SkillPropertyType::TEXT:         return "Text";         break;
                case SkillPropertyType::Name:         return "Name";         break;
                case SkillPropertyType::SkillType:    return "Type";         break;
                case SkillPropertyType::TAGS:         return "Tag";          break;
                case SkillPropertyType::Description:  return "Description";  break;
                case SkillPropertyType::Attribute:    return "Attribute";    break;
                case SkillPropertyType::Profession:   return "Profession";   break;
                case SkillPropertyType::Campaign:     return "Campaign";     break;

                case SkillPropertyType::Overcast:     return "Overcast";     break;
                case SkillPropertyType::Adrenaline:   return "Adrenaline";   break;
                case SkillPropertyType::Sacrifice:    return "Sacrifice";    break;
                case SkillPropertyType::Energy:       return "Energy";       break;
                case SkillPropertyType::Activation:   return "Activation";   break;
                case SkillPropertyType::Recharge:     return "Recharge";     break;
                case SkillPropertyType::Aftercast:    return "Aftercast";    break;
                case SkillPropertyType::Range:        return "Range";        break;
                case SkillPropertyType::ID:           return "ID";           break;

                default:                             return "...";          break;
            }
            // clang-format on
        }
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

    struct Filter
    {
        FilterJoin join;
        SkillPropertyID target;
        FilterOperator op;
        std::vector<std::string_view> str_values;
        std::vector<double> num_values;
        const char *start;
        const char *end;
        const char *value_start;
        const char *value_end;

        bool IsValid() const
        {
            return ((target.IsNumberType() && !num_values.empty()) ||
                       (target.IsStringType() && !str_values.empty())) &&
                   op > FilterOperator::Unknown;
        }
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

    bool SkipWhitespace(char *&p, char *end)
    {
        auto start = p;
        while (p < end && *p == ' ')
            p++;

        return p != start;
    }

    void SkipOneWhitespace(char *&p, char *end)
    {
        if (p < end && *p == ' ')
            p++;
    }

    void SkipWhitespaceBackwards(char *start, char *&p)
    {
        while (p > start && *(p - 1) == ' ')
            p--;
    }

    void SkipOneWhitespaceBackwards(const char *start, char *&p)
    {
        if (p > start && *(p - 1) == ' ')
            p--;
    }

    void SkipUntil(char *&p, char *end, char c)
    {
        while (p < end && *p != c)
            p++;
    }

    bool IsSeparator(char c)
    {
        return c == '|' || c == '&' || c == '#';
    }

    void SkipUntilStatementEnd(char *&p, char *end)
    {
        auto p_start = p;
        while (p < end)
        {
            if (IsSeparator(*p))
                return;
            p++;
        }
    }

    bool TryParseRawFilterTarget(char *&p, char *end, SkillPropertyID &target)
    {
        auto p_copy = p;
        if (!Utils::TryRead("0x", p_copy, end))
            return false;

        target.type = SkillPropertyType::RAW;
        target.byte_offset = 0;

        char *hex_start = p_copy;
        auto offset_ul = std::strtoul(hex_start, &p_copy, 16);
        if (offset_ul >= sizeof(GW::Skill))
            return false;
        target.byte_offset = static_cast<uint8_t>(offset_ul);

        SkipWhitespace(p_copy, end);

        bool is_float = false;
        if (Utils::TryRead('U', p_copy, end))
            ;
        else if (Utils::TryRead('F', p_copy, end))
            is_float = true;
        else
        {
            p = p_copy;
            return true;
        }

        uint32_t eq_len = 0;
        if (Utils::TryRead('8', p_copy, end))
        {
            if (is_float || target.byte_offset > sizeof(GW::Skill) - sizeof(uint8_t))
                return false;
            else
                target.type = SkillPropertyType::u8;
        }
        else if (!eq_len && (eq_len = Utils::TryReadPartial("16", p_copy, end)) == 2)
        {
            if (is_float || target.byte_offset > sizeof(GW::Skill) - sizeof(uint16_t))
                return false;
            else
                target.type = SkillPropertyType::u16;
        }
        else if (!eq_len && (eq_len = Utils::TryReadPartial("32", p_copy, end)) == 2)
        {
            if (target.byte_offset > sizeof(GW::Skill) - sizeof(uint32_t))
                return false;
            else
                target.type = is_float ? SkillPropertyType::f32 : SkillPropertyType::u32;
        }
        else
        {
            p = p_copy;
            return true;
        }

        p = p_copy;
        return true;
    }

    constexpr SkillPropertyType FromParam(ParsedSkillData::Type type)
    {
        return static_cast<SkillPropertyType>((uint32_t)SkillPropertyType::PARSED + (uint32_t)type);
    }

    // Must not contain any whitespace
    static constexpr uint32_t n_text_filter_targets = 8; // Must match the array below
    static constexpr std::pair<std::string_view, SkillPropertyType> filter_targets[] = {
        // Text

        {"Text", SkillPropertyType::TEXT},
        {"Name", SkillPropertyType::Name},
        {"Type", SkillPropertyType::SkillType},
        {"Tag", SkillPropertyType::TAGS},
        {"Description", SkillPropertyType::Description},
        {"Attribute", SkillPropertyType::Attribute},
        {"Profession", SkillPropertyType::Profession},
        {"Campaign", SkillPropertyType::Campaign},

        // Number

        {"Overcast", SkillPropertyType::Overcast},
        {"Adrenaline", SkillPropertyType::Adrenaline},
        {"Sacrifice", SkillPropertyType::Sacrifice},
        {"Energy", SkillPropertyType::Energy},
        {"Activation", SkillPropertyType::Activation},
        {"Recharge", SkillPropertyType::Recharge},
        {"Aftercast", SkillPropertyType::Aftercast},
        {"Range", SkillPropertyType::Range},
        {"ID", SkillPropertyType::ID},

        {"Duration", FromParam(ParsedSkillData::Type::Duration)},
        {"Disable", FromParam(ParsedSkillData::Type::Disable)},
        {"Level", FromParam(ParsedSkillData::Type::Level)},
        {"Damage", FromParam(ParsedSkillData::Type::Damage)},
        {"Healing", FromParam(ParsedSkillData::Type::Heal)},
        {"Armor", FromParam(ParsedSkillData::Type::ArmorChange)},

        {"ConditionsRemoved", FromParam(ParsedSkillData::Type::ConditionsRemoved)},
        {"HexesRemoved", FromParam(ParsedSkillData::Type::HexesRemoved)},
        {"EnchantmentsRemoved", FromParam(ParsedSkillData::Type::EnchantmentsRemoved)},

        {"HealthPips", FromParam(ParsedSkillData::Type::HealthPips)},
        {"HealthGain", FromParam(ParsedSkillData::Type::HealthGain)},
        {"HealthLoss", FromParam(ParsedSkillData::Type::HealthLoss)},
        {"HealthSteal", FromParam(ParsedSkillData::Type::HealthSteal)},
        {"MaxHealth", FromParam(ParsedSkillData::Type::MaxHealthAdd)},

        {"EnergyPips", FromParam(ParsedSkillData::Type::EnergyPips)},
        {"EnergyGain", FromParam(ParsedSkillData::Type::EnergyGain)},
        {"EnergyLoss", FromParam(ParsedSkillData::Type::EnergyLoss)},
        {"EnergySteal", FromParam(ParsedSkillData::Type::EnergySteal)},
        {"EnergyDiscount", FromParam(ParsedSkillData::Type::EnergyDiscount)},

        {"AdrenalineGain", FromParam(ParsedSkillData::Type::AdrenalineGain)},
        {"AdrenalineLoss", FromParam(ParsedSkillData::Type::AdrenalineLoss)},

        {"Bleeding", FromParam(ParsedSkillData::Type::Bleeding)},
        {"Blind", FromParam(ParsedSkillData::Type::Blind)},
        {"Burning", FromParam(ParsedSkillData::Type::Burning)},
        {"CrackedArmor", FromParam(ParsedSkillData::Type::CrackedArmor)},
        {"Crippled", FromParam(ParsedSkillData::Type::Crippled)},
        {"Dazed", FromParam(ParsedSkillData::Type::Dazed)},
        {"DeepWound", FromParam(ParsedSkillData::Type::DeepWound)},
        {"Disease", FromParam(ParsedSkillData::Type::Disease)},
        {"Poison", FromParam(ParsedSkillData::Type::Poison)},
        {"Weakness", FromParam(ParsedSkillData::Type::Weakness)},

        {"ActivationModifier", FromParam(ParsedSkillData::Type::ActivationTimeAdd)},
        {"MoveSpeedModifier%", FromParam(ParsedSkillData::Type::MovementSpeedMod)},
        {"RechargeModifier", FromParam(ParsedSkillData::Type::RechargeTimeAdd)},
        {"RechargeModifier%", FromParam(ParsedSkillData::Type::RechargeTimeMod)},
        {"AttackTimeModifier%", FromParam(ParsedSkillData::Type::AttackTimeMod)},
        {"DamageModifier", FromParam(ParsedSkillData::Type::DamageReduction)},
        {"DamageModifier%", FromParam(ParsedSkillData::Type::DamageMod)},
        {"DurationModifier%", FromParam(ParsedSkillData::Type::DurationMod)},
        {"HealingModifier%", FromParam(ParsedSkillData::Type::HealMod)},
    };

    static constexpr std::span<const std::pair<std::string_view, SkillPropertyType>> text_filter_targets = {filter_targets, n_text_filter_targets};
    static constexpr std::span<const std::pair<std::string_view, SkillPropertyType>> number_filter_targets = {filter_targets + n_text_filter_targets, std::size(filter_targets) - n_text_filter_targets};

    uint32_t MatchIdent(std::string_view ident, std::string_view shorthand, uint32_t i, uint32_t s)
    {
        if (s >= shorthand.size() || i >= ident.size())
            return 0;

        auto opt_a = 0;
        if (tolower(ident[i]) == tolower(shorthand[s]))
        {
            opt_a = 1 + MatchIdent(ident, shorthand, i + 1, s + 1);
        }

        auto next_subword_index = i + 1;
        while (next_subword_index < ident.size() &&
               ident[next_subword_index] >= 'a' &&
               ident[next_subword_index] <= 'z')
            next_subword_index++;

        bool has_next_subword = next_subword_index < ident.size();

        auto opt_b = 0;
        if (has_next_subword)
        {
            opt_b = MatchIdent(ident, shorthand, next_subword_index, s);
        }

        return std::max(opt_a, opt_b);
    }

    SkillPropertyID ParseFilterTarget(char *&p, char *end, bool must_match_whole_word = true)
    {
        SkillPropertyID target = {};

        if (TryParseRawFilterTarget(p, end, target))
        {
            return target;
        }

        auto view = std::string_view(p, end - p);

        uint32_t best_match_count = 0;
        int32_t best_match_index = -1;
        char *best_match_end = p;
        for (uint32_t i = 0; i < std::size(filter_targets); i++)
        {
            auto &pair = filter_targets[i];
            auto ident = pair.first;
            auto &type = pair.second;

            auto match_count = MatchIdent(ident, view, 0, 0);
            char *match_end = p + match_count;
            if (must_match_whole_word && (match_end < end && std::isalpha(*match_end)))
                continue;
            if (match_count > best_match_count)
            {
                target.type = type;
                best_match_count = match_count;
                best_match_index = i;
                best_match_end = match_end;
            }
            // else if (best_match_index != -1 && match_count == best_match_count)
            // {
            //     auto old_ident = filter_targets[best_match_index].first;
            //     if (!ident.contains(old_ident))
            //     {
            //         target.type = SkillPropertyType::None;
            //         best_match_index = -1;
            //         best_match_end = p;
            //     }
            // }
        }

        p = best_match_end;

        return target;
    }

    static constexpr std::pair<std::string_view, FilterOperator> filter_operators[] = {
        {":", FilterOperator::Contain},
        {"!:", FilterOperator::NotContain},
        {"=", FilterOperator::Equal},
        {"!=", FilterOperator::NotEqual},
        {">=", FilterOperator::GreaterThanOrEqual},
        {">", FilterOperator::GreaterThan},
        {"<=", FilterOperator::LessThanOrEqual},
        {"<", FilterOperator::LessThan},
    };

    static constexpr std::span<const std::pair<std::string_view, FilterOperator>> text_filter_operators = {filter_operators, 4};
    static constexpr std::span<const std::pair<std::string_view, FilterOperator>> number_filter_operators = {filter_operators + 2, 6};
    static constexpr std::span<const std::pair<std::string_view, FilterOperator>> adv_filter_operators = {filter_operators, 2};

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

    FilterOperator ParseFilterOperator(SkillPropertyID target, char *&p, char *end)
    {
        if (p == end)
            return FilterOperator::None;

        if (Utils::TryRead("==", p, end)) // Special case for programmers habit of using == for equality instead of =
            return FilterOperator::Equal;

        auto operators = target.IsStringType()
                             ? text_filter_operators
                         : target.IsRawNumberType()
                             ? filter_operators
                             : number_filter_operators;

        for (const auto &[op, op_enum] : operators)
        {
            if (Utils::TryRead(op, p, end))
                return op_enum;
        }

        return FilterOperator::None;
    }

    bool TryParseFilterTargetAndOperator(char *&p, char *end, Filter &filter)
    {
        auto p_copy = p;

        SkipWhitespace(p_copy, end);

        filter.target = ParseFilterTarget(p_copy, end);
        if (filter.target.type == SkillPropertyType::None)
        {
            filter.target = {SkillPropertyType::TEXT, 0};
        }

        SkipWhitespace(p_copy, end);

        filter.op = ParseFilterOperator(filter.target, p_copy, end);
        if (filter.op == FilterOperator::None)
        {
            if (filter.target.IsRawNumberType())
            {
                filter.op = FilterOperator::Unknown;
            }
            else
            {
                filter.target = {SkillPropertyType::TEXT, 0};
                filter.op = FilterOperator::Contain;
                return true;
            }
        }

        SkipOneWhitespace(p_copy, end);

        p = p_copy;
        return true;
    }

    bool TryParseFilter(char *&p, char *end, Filter &filter)
    {
        filter.start = p;
        bool is_invalid = false;

        if (p == end)
            return false;

        SkipWhitespace(p, end);

        if (Utils::TryRead('|', p, end))
        {
            filter.join = FilterJoin::Or;
        }
        else if (Utils::TryRead('&', p, end))
        {
            filter.join = FilterJoin::And;
        }
        else
        {
            p = (char *)filter.start;
            filter.join = FilterJoin::None;
        }

        if (filter.join != FilterJoin::None)
            SkipWhitespace(p, end);

        TryParseFilterTargetAndOperator(p, end, filter);

        filter.value_start = p;
        filter.end = end;

        while (true)
        {
            char *str_start;
            size_t str_len;
            if (filter.target.IsNumberType())
            {
                SkipWhitespace(p, end);
                str_start = p;
                double value = strtod(str_start, &p);
                str_len = p - str_start;
                bool success = (p == end || *p == ' ' || *p == '/' || IsSeparator(*p));
                SkipWhitespace(p, end);

                if (str_len > 0)
                    filter.num_values.push_back(value);
                else if (filter.num_values.empty())
                    success = false;

                if (!success)
                {
                    break;
                }
            }
            else
            {
                str_start = p;
                SkipUntil(p, end, '/');
                str_len = p - str_start;
            }

            filter.str_values.push_back(std::string_view(str_start, str_len));

            if (!Utils::TryRead('/', p, end))
                break;
        }
        filter.value_end = p;

        p = (char *)filter.end;

        if (p == filter.start)
            return false;

        return true;
    }

    bool TryParseCommand(char *&p, char *end, Command &command)
    {
        auto p_copy = p;

        SkipWhitespace(p_copy, end);

        if (!Utils::TryRead('#', p_copy, end))
            return false;

        if (Utils::TryRead("SORT", p_copy, end))
        {
            SortCommand sort_com = {};
            while (p_copy < end)
            {
                if (!SkipWhitespace(p_copy, end))
                    break;
                SortCommandArg sort_arg = {};
                auto start = p_copy;
                bool has_excl = Utils::TryRead('!', p_copy, end);
                sort_arg.target = ParseFilterTarget(p_copy, end);
                if (sort_arg.target.type == SkillPropertyType::None ||
                    sort_arg.target.type == SkillPropertyType::TEXT)
                    break;

                if (sort_arg.target.type == SkillPropertyType::ID)
                    sort_arg.is_negated = false;
                else
                    sort_arg.is_negated = sort_arg.target.IsStringType() ? has_excl : !has_excl; // We sort numbers by default in descending order

                sort_com.args.push_back(sort_arg);
            }
            command.data = sort_com;
        }
        else
        {
            return false;
        }

        p = p_copy;

        return true;
    }

    // Example input: "foe | prof= me|p & desc !: icy|ice & range !=4 & activation < 2 /sort re !e"
    void ParseInput(char *input, size_t filter_len, std::vector<Filter> &parsed_filters, std::vector<Command> &parsed_commands)
    {
        char *p = input;
        char *end = input + filter_len;

        // auto stmt_start = p;
        while (p < end)
        {
            auto stmt_start = p;
            p++; // Skip the separator
            SkipUntilStatementEnd(p, end);
            auto stmt_end = p;
            if (p != end)
                SkipOneWhitespaceBackwards(stmt_start, stmt_end);

            if (*stmt_start == '#')
            {
                if (Command command = {}; TryParseCommand(stmt_start, stmt_end, command))
                {
                    parsed_commands.push_back(command);
                }
            }
            else
            {
                if (Filter filter = {}; TryParseFilter(stmt_start, stmt_end, filter))
                {
                    parsed_filters.push_back(filter);
                }
            }
        }
    }

    struct SkillProperty
    {
        double GetNumber() const
        {
            return std::get<double>(val);
        }

        std::string_view GetStr() const
        {
            return std::get<std::string_view>(val);
        }

        bool IsString() const
        {
            return std::holds_alternative<std::string_view>(val);
        }

        SkillPropertyType type;
        std::variant<double, std::string_view> val;
    };

    uint32_t GetHighlightKey(GW::Constants::SkillID skill_id, SkillPropertyType target)
    {
        assert((uint32_t)skill_id < 0x10000);
        assert((uint32_t)target < 0x10000);
        return (((uint32_t)skill_id) << 16) | (uint32_t)target;
    }

    void GetSkillProperty(SkillPropertyID target, CustomSkillData &custom_sd, FixedArrayRef<SkillProperty> out)
    {
        auto &skill = *custom_sd.skill;
        auto attr_lvl = pending_state.GetAttribute(custom_sd.attribute);

        auto success = true;

        if (target.type >= SkillPropertyType::PARSED && target.type < SkillPropertyType::PARSED_END)
        {
            auto PushSkillParam = [&](ParsedSkillData::Type type)
            {
                FixedArray<ParsedSkillData, 8> salloc;
                auto pps = salloc.ref();
                custom_sd.GetParsedSkillParams(type, pps);
                if (pps.size() == 0)
                {
                    success &= out.try_push({target.type, 0.0});
                    return;
                }
                for (const auto &pp : pps)
                {
                    double sign = pp.is_negative ? -1.0 : 1.0;
                    if (attr_lvl == -1)
                    {
                        success &= out.try_push({target.type, sign * (double)pp.param.val0});
                        success &= out.try_push({target.type, sign * (double)pp.param.val15});
                    }
                    else
                    {
                        success &= out.try_push({target.type, sign * (double)pp.param.Resolve(attr_lvl)});
                    }
                }
            };
            const auto param_type = static_cast<ParsedSkillData::Type>((uint32_t)target.type - (uint32_t)SkillPropertyType::PARSED);
            PushSkillParam(param_type);
            SOFT_ASSERT(success);
            return;
        }

        auto PushTags = [&]()
        {
            FixedArray<SkillTag, MAX_TAGS> tags_salloc;
            auto tags = tags_salloc.ref();
            custom_sd.GetTags(tags);
            for (uint32_t i = 0; i < tags.size(); i++)
            {
                auto type = (SkillPropertyType)((uint32_t)SkillPropertyType::TAGS + i);
                success &= out.try_push({type, SkillTagToString(tags[i])});
            }
        };

        auto PushDesc = [&]()
        {
            success &= out.try_push({SkillPropertyType::Description, (std::string_view)custom_sd.TryGetDescription(false, attr_lvl)->str});
            success &= out.try_push({SkillPropertyType::Concise, (std::string_view)custom_sd.TryGetDescription(true, attr_lvl)->str});
        };

        const auto ptr = reinterpret_cast<const uint8_t *>(custom_sd.skill);

        // clang-format off
        switch (target.type) {
            case SkillPropertyType::TEXT: {
                success &= out.try_push({SkillPropertyType::Name, (std::string_view)*custom_sd.TryGetName()});
                success &= out.try_push({SkillPropertyType::SkillType, custom_sd.GetTypeString()});
                PushTags();
                PushDesc();
                success &= out.try_push({SkillPropertyType::Attribute, custom_sd.GetAttributeString()});
                success &= out.try_push({SkillPropertyType::Profession, custom_sd.GetProfessionString()});
                success &= out.try_push({SkillPropertyType::Campaign, custom_sd.GetCampaignString()});
                break;
            }
            case SkillPropertyType::Name:         success &= out.try_push({target.type, (std::string_view)*custom_sd.TryGetName()}); break;
            case SkillPropertyType::SkillType:    success &= out.try_push({target.type, custom_sd.GetTypeString()});                 break;
            case SkillPropertyType::TAGS:         PushTags();                                                                        break;
            case SkillPropertyType::Description:  PushDesc();                                                                        break;
            case SkillPropertyType::Attribute:    success &= out.try_push({target.type, custom_sd.GetAttributeString()});            break;
            case SkillPropertyType::Profession:   success &= out.try_push({target.type, custom_sd.GetProfessionString()});           break;
            case SkillPropertyType::Campaign:     success &= out.try_push({target.type, custom_sd.GetCampaignString()});             break;

            case SkillPropertyType::Overcast:     success &= out.try_push({target.type, (double)custom_sd.GetOvercast()});           break;
            case SkillPropertyType::Adrenaline: {
                // For adrenaline we push two values, the adrenaline strikes (what the user typically wants) and the raw adrenaline value, which is used when sorting.
                success &= out.try_push({SkillPropertyType::Adrenaline, (double)custom_sd.GetAdrenalineStrikes()});
                success &= out.try_push({SkillPropertyType::Adrenaline, (double)custom_sd.GetAdrenaline()});
                break;
            }
            case SkillPropertyType::Sacrifice:    success &= out.try_push({target.type, (double)custom_sd.GetSacrifice()});          break;
            case SkillPropertyType::Energy:       success &= out.try_push({target.type, (double)custom_sd.GetEnergy()});             break;
            case SkillPropertyType::Activation:   success &= out.try_push({target.type, custom_sd.GetActivation()});                 break;
            case SkillPropertyType::Recharge:     success &= out.try_push({target.type, (double)custom_sd.GetRecharge()});           break;
            case SkillPropertyType::Aftercast:    success &= out.try_push({target.type, custom_sd.GetAftercast()});                  break;
            case SkillPropertyType::Range: {
                FixedArray<Utils::Range, 4> ranges_salloc;
                auto ranges = ranges_salloc.ref();
                custom_sd.GetRanges(ranges);
                for (const auto &range : ranges)
                    success &= out.try_push({SkillPropertyType::Range, (double)range});
                break;
            }
            case SkillPropertyType::ID:           success &= out.try_push({target.type, (double)custom_sd.skill_id});                break;

            case SkillPropertyType::u8:           success &= out.try_push({target.type, (double)*(uint8_t  *)(ptr + target.byte_offset)}); break;
            case SkillPropertyType::u16:          success &= out.try_push({target.type, (double)*(uint16_t *)(ptr + target.byte_offset)}); break;
            case SkillPropertyType::u32:          success &= out.try_push({target.type, (double)*(uint32_t *)(ptr + target.byte_offset)}); break;
            case SkillPropertyType::f32:          success &= out.try_push({target.type, (double)*(float    *)(ptr + target.byte_offset)}); break;

            case SkillPropertyType::RAW:
            case SkillPropertyType::Unknown:
                break; // If unknown, it was explicitly requested, perhaps by an in-progress filter. Do nothing.

            default: SOFT_ASSERT(false, L"Unhandled SkillPropertyType {}", (uint32_t)target.type); break;
        }
        // clang-format on
        SOFT_ASSERT(success);
    }

    void AddHighlightRange(std::vector<uint16_t> &highlighting, uint16_t start, uint16_t end)
    {
        const auto count = highlighting.size();
        if (count == 0)
        {
            highlighting.push_back(start);
            highlighting.push_back(end);
            return;
        }

        const auto &prev_start = highlighting[count - 2];
        if (prev_start <= start) // Common case
        {
            auto &prev_end = highlighting[count - 1];
            if (prev_end < start)
            {
                highlighting.push_back(start);
                highlighting.push_back(end);
            }
            else
            {
                prev_end = end;
            }
            return;
        }

        // Find the insertion point using std::lower_bound
        auto it_start = std::lower_bound(highlighting.begin(), highlighting.end(), start, [](uint16_t a, uint16_t b)
            { return a < b; });

        auto index_start = std::distance(highlighting.begin(), it_start);
        bool is_intersecting_start = index_start & 1;

        // Find the insertion point using std::lower_bound
        auto it_end = std::lower_bound(highlighting.begin(), highlighting.end(), end, [](uint16_t a, uint16_t b)
            { return a < b; });

        auto index_end = std::distance(highlighting.begin(), it_end);
        bool is_intersecting_end = index_end & 1;

        auto removal_count = index_end - index_start;

        auto eit = highlighting.erase(it_start, it_end);

        if (!is_intersecting_end)
        {
            eit = highlighting.insert(eit, end);
        }
        if (!is_intersecting_start)
        {
            eit = highlighting.insert(eit, start);
        }
    }

    bool CheckPassesFilter(const Filter &filter, CustomSkillData &custom_sd)
    {
        bool negated = filter.op == FilterOperator::NotEqual ||
                       filter.op == FilterOperator::NotContain;

        if (filter.target.IsStringType())
        {
            bool matches_must_start_at_beginning = filter.op == FilterOperator::Equal ||
                                                   filter.op == FilterOperator::NotEqual;

            FixedArray<SkillProperty, 16> salloc;
            auto buffer = salloc.ref();
            GetSkillProperty(filter.target, custom_sd, buffer);

            bool str_found = false;
            for (auto prop : buffer)
            {
                const auto str = prop.GetStr();
                auto str_len = str.size();
                if (str_len == 0)
                {
                    // We treat empty strings as not matching any filter
                    continue;
                }

                auto &highlighting = pending_state.highlighting_map[GetHighlightKey(custom_sd.skill_id, prop.type)];

                for (auto filter_str : filter.str_values)
                {
                    auto filter_str_len = filter_str.size();

                    if (filter_str_len == 0)
                    {
                        // But if the filter string is empty, we treat it as a match
                        str_found = true;
                        continue;
                    }

                    if (matches_must_start_at_beginning)
                    {
                        if (Utils::StrCountEqual(str, filter_str) == filter_str_len)
                        {
                            str_found = true;
                            if (negated)
                                break;

                            AddHighlightRange(highlighting, 0, filter_str_len);
                        }
                    }
                    else
                    {
                        const auto str_start = str.data();
                        auto search_ptr = str_start;
                        const auto search_end = search_ptr + str_len;
                        std::string_view found;
                        while (Utils::StrFind(search_ptr, search_end, filter_str))
                        {
                            str_found = true;
                            if (negated)
                                break;

                            auto start_index = search_ptr - str_start;
                            auto end_index = start_index + filter_str_len;
                            AddHighlightRange(highlighting, start_index, end_index);
                            search_ptr += 1;
                        }
                    }
                }
            }

            return str_found ^ negated;
        }
        else if (filter.target.IsNumberType())
        {
            FixedArray<SkillProperty, 8> salloc;
            auto buffer = salloc.ref();
            GetSkillProperty(filter.target, custom_sd, buffer);

            bool cond_satisfied = false;

            for (auto prop : buffer)
            {
                auto skill_val = prop.GetNumber();
                for (auto filt_val : filter.num_values)
                {
                    // clang-format off
                    switch (filter.op) {
                        case FilterOperator::GreaterThan:        cond_satisfied |= skill_val >  filt_val; break;
                        case FilterOperator::LessThan:           cond_satisfied |= skill_val <  filt_val; break;
                        case FilterOperator::GreaterThanOrEqual: cond_satisfied |= skill_val >= filt_val; break;
                        case FilterOperator::LessThanOrEqual:    cond_satisfied |= skill_val <= filt_val; break;
                        
                        case FilterOperator::NotContain:
                        case FilterOperator::Contain:            cond_satisfied |= ((uint32_t)skill_val & (uint32_t)filt_val) == (uint32_t)filt_val; break;
                        
                        case FilterOperator::None:
                        case FilterOperator::NotEqual:
                        case FilterOperator::Equal:
                        default:                                 cond_satisfied |= skill_val == filt_val; break;
                    }
                    // clang-format on
                }
            }

            return cond_satisfied ^ negated;
        }
        else
        {
            SOFT_ASSERT(false, L"Invalid filter target type");
            return false;
        }
    }

    bool CheckPassesFilters(const std::vector<Filter> &filters, CustomSkillData &custom_sd)
    {
        bool passes = true;
        for (auto &filter : filters)
        {
            if (!filter.IsValid())
                continue;

            auto join = filter.join;
            if (join == FilterJoin::And ||
                join == FilterJoin::None)
            {
                if (passes == false)
                    return false;
                passes &= CheckPassesFilter(filter, custom_sd);
            }
            else if (join == FilterJoin::Or)
            {
                passes |= CheckPassesFilter(filter, custom_sd);
            }
        }

        return passes;
    }

    union SortProp
    {
        char str[16];
        double num[2];

        SortProp() = default;
        SortProp(SkillPropertyID id, CustomSkillData &custom_sd)
        {
            FixedArray<SkillProperty, 8> salloc;
            auto props = salloc.ref();
            GetSkillProperty(id, custom_sd, props);

            if (id.IsStringType())
            {
                if (props.size() > 0)
                {
                    auto str_view = props[0].GetStr();
                    std::strncpy(str, str_view.data(), sizeof(str));
                }
                else
                {
                    std::memset(str, 0, sizeof(str));
                }
            }
            else
            {
                num[0] = props.size() > 0 ? props[0].GetNumber() : 0;
                num[1] = num[0];
                for (uint32_t i = 1; i < props.size(); i++)
                {
                    auto val = props[i].GetNumber();
                    num[0] = std::min(num[0], val);
                    num[1] = std::max(num[1], val);
                }
            }
        }

        bool ContainsFullString() const
        {
            return str[sizeof(str) - 1] == '\0';
        }

        int32_t CompareStr(const SortProp &other) const
        {
            return std::strncmp(str, other.str, sizeof(str));
        }

        int32_t CompareNumAscending(SortProp &other) const
        {
            if (num[0] != other.num[0])
                return num[0] < other.num[0] ? -1 : 1;
            if (num[1] != other.num[1])
                return num[1] < other.num[1] ? -1 : 1;
            return 0;
        }

        int32_t CompareNumDescending(SortProp &other) const
        {
            if (num[1] != other.num[1])
                return num[1] < other.num[1] ? 1 : -1;
            if (num[0] != other.num[0])
                return num[0] < other.num[0] ? 1 : -1;
            return 0;
        }
    };

    void ApplyCommand(Command &command, std::span<uint16_t> filtered_skills)
    {
        if (std::holds_alternative<SortCommand>(command.data))
        {
            // Sorting may be slow on debug builds but should be super fast on release builds
            // (I experienced 110 ms in debug build and 4 ms in release build for the same sorting operation)

            auto &sort_command = std::get<SortCommand>(command.data);
            if (sort_command.args.empty())
                return;

            // Prefetch some data to speed up sorting (this might actually not be needed for release builds)
            auto prefetched = std::array<SortProp, GW::Constants::SkillMax>{};
            auto first_target = sort_command.args[0].target;
            for (auto skill_id : filtered_skills)
            {
                auto &custom_sd = CustomSkillDataModule::GetCustomSkillData((GW::Constants::SkillID)skill_id);
                prefetched[skill_id] = SortProp(first_target, custom_sd);
            }

            auto Comparer = [&](const uint16_t a, const uint16_t b)
            {
                for (uint32_t i = 0; i < sort_command.args.size(); i++)
                {
                    auto &arg = sort_command.args[i];
                    auto target = arg.target;
                    bool is_negated = arg.is_negated;

                    auto &custom_sd_a = CustomSkillDataModule::GetCustomSkillData((GW::Constants::SkillID)a);
                    auto &custom_sd_b = CustomSkillDataModule::GetCustomSkillData((GW::Constants::SkillID)b);

                    bool is_string = target.IsStringType();
                    int32_t cmp = 0;
                    if (is_string)
                    {
                        if (i == 0)
                        {
                            // Do a cheap check on first 16 chars first
                            auto &prop_a = prefetched[a];
                            auto &prop_b = prefetched[b];
                            cmp = prop_a.CompareStr(prop_b);
                            if (cmp != 0)
                                return is_negated ? cmp > 0 : cmp < 0;
                            if (prop_a.ContainsFullString() && prop_b.ContainsFullString())
                                continue;
                        }

                        constexpr auto buffer_size = 8;
                        FixedArray<SkillProperty, buffer_size> salloc_a;
                        FixedArray<SkillProperty, buffer_size> salloc_b;
                        auto buffer_a = salloc_a.ref();
                        auto buffer_b = salloc_b.ref();
                        GetSkillProperty(target, custom_sd_a, buffer_a);
                        GetSkillProperty(target, custom_sd_b, buffer_b);

                        assert(buffer_a.size() == buffer_b.size());
                        for (uint32_t i = 0; i < buffer_a.size(); i++)
                        {
                            auto str_a = buffer_a[i].GetStr();
                            auto str_b = buffer_b[i].GetStr();

                            cmp = str_a.compare(str_b);
                            if (cmp != 0)
                                return is_negated ? cmp > 0 : cmp < 0;
                        }
                    }
                    else
                    {
                        auto prop_a = i == 0 ? prefetched[a] : SortProp(target, custom_sd_a);
                        auto prop_b = i == 0 ? prefetched[b] : SortProp(target, custom_sd_b);
                        cmp = is_negated ? prop_a.CompareNumDescending(prop_b)
                                         : prop_a.CompareNumAscending(prop_b);
                        if (cmp != 0)
                            return cmp < 0;
                    }
                }
                return false;
            };
            std::stable_sort(filtered_skills.begin(), filtered_skills.end(), Comparer);
        }
        else
        {
            SOFT_ASSERT(false, L"Invalid command type");
        }
    }

    bool show_null_stats = false;
    bool snap_to_skill = true;
    void DrawCheckboxes()
    {
        // if (ImGui::CollapsingHeader("Options"))
        {
            ImGui::Columns(2, nullptr, false);

            state_dirty |= ImGui::Checkbox("Include PvE-only skills", &include_pve_only_skills);
            state_dirty |= ImGui::Checkbox("Include Temporary skills", &include_temporary_skills);
            state_dirty |= ImGui::Checkbox("Include NPC skills", &include_npc_skills);
            state_dirty |= ImGui::Checkbox("Include Archived skills", &include_archived_skills);
            state_dirty |= ImGui::Checkbox("Include Disguises", &include_disguises);

            ImGui::NextColumn();

            ImGui::Checkbox("Show precise adrenaline", &use_precise_adrenaline);
            ImGui::Checkbox("Show null stats", &show_null_stats);
            ImGui::Checkbox("Snap to skill", &snap_to_skill);
            ImGui::Checkbox("Prefer concise descriptions", &prefer_concise_descriptions);

            ImGui::Columns(1);
        }
    }

    Utils::RichString input_feedback;
    void UpdateFeedback(std::span<Filter> parsed_filters, std::span<Command> parsed_commands)
    {
        input_feedback.str.clear();
        input_feedback.color_changes.clear();
        for (auto &filter : parsed_filters)
        {
            if (!filter.IsValid())
            {
                input_feedback.color_changes.push_back({input_feedback.str.size(), Constants::GWColors::skill_dull_gray});
            }

            const auto target_name = filter.target.ToString();
            const auto op_desc = GetOpDescription(filter.target, filter.op).data();
            bool is_string = filter.target.IsStringType();
            bool is_number = filter.target.IsNumberType();

            if (filter.join == FilterJoin::Or)
            {
                input_feedback.str += "OR ";
            }
            else if (filter.join == FilterJoin::And)
            {
                input_feedback.str += "AND ";
            }

            if (is_string)
            {
                Utils::AppendFormatted(input_feedback.str, 64, "%s %s: ", target_name.data(), op_desc);
            }
            else if (is_number)
            {
                Utils::AppendFormatted(input_feedback.str, 128, "%s %s ", target_name.data(), op_desc);
            }
            else
            {
                input_feedback.str += "...";
            }

            auto value_str_len = filter.value_end - filter.value_start;

            const auto n_values = filter.str_values.size();
            for (uint32_t i = 0; i < n_values; i++)
            {
                auto filt_str = filter.str_values[i];
                if (is_number && filt_str.size() == 0)
                    filt_str = "...";
                auto kind = i == 0             ? 0
                            : i < n_values - 1 ? 1
                                               : 2;
                // clang-format off
                if (kind == 1)      input_feedback.str += ", ";
                else if (kind == 2) input_feedback.str += " or ";
                if (is_string)      input_feedback.str += "\"";
                                    input_feedback.str += filt_str;
                if (is_string)      input_feedback.str += "\"";
                // if (kind = 2)       input_feedback.str += ".";
                // clang-format on
            }

            if (n_values == 0 && is_number)
            {
                input_feedback.str += "...";
            }

            if (!filter.IsValid())
            {
                input_feedback.color_changes.push_back({input_feedback.str.size(), 0});
            }

            input_feedback.str += "\n";
        }

        for (auto &command : parsed_commands)
        {
            if (std::holds_alternative<SortCommand>(command.data))
            {
                auto &sort_command = std::get<SortCommand>(command.data);

                const auto n_values = sort_command.args.size();
                bool is_incomplete = n_values == 0;
                if (is_incomplete)
                {
                    input_feedback.color_changes.push_back({input_feedback.str.size(), Constants::GWColors::skill_dull_gray});
                }

                input_feedback.str += is_incomplete ? "Sort by ..." : "Sort";
                for (uint32_t i = 0; i < n_values; i++)
                {
                    const auto &arg = sort_command.args[i];
                    auto kind = i == 0             ? 0
                                : i < n_values - 1 ? 1
                                                   : 2;

                    // clang-format off
                    if (kind == 1)      input_feedback.str += ", then";
                    else if (kind == 2) input_feedback.str += " and then";
                    if (arg.is_negated) input_feedback.str += " descending by ";
                    else                input_feedback.str += " ascending by ";
                                        input_feedback.str += arg.target.ToString();
                    // clang-format on
                }

                if (is_incomplete)
                {
                    input_feedback.color_changes.push_back({input_feedback.str.size(), 0});
                }
            }
            input_feedback.str += "\n";
        }
    }

    bool TryInitBaseSkills()
    {
        if (!CustomSkillDataModule::is_initialized)
            return false;

        bool fail = false;
        for (uint16_t i = 0; i < GW::Constants::SkillMax; i++)
        {
            auto &custom_sd = CustomSkillDataModule::GetCustomSkillData((GW::Constants::SkillID)i);
            if (!custom_sd.TryGetName())
                fail = true;
        }
        if (fail)
            return false;

        for (uint16_t i = 1; i < GW::Constants::SkillMax; i++)
        {
            base_skills.push_back(i);
        }

        auto Comparer = [](uint16_t a, uint16_t b)
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

            auto camp_a = custom_sd_a.GetCampaignString();
            auto camp_b = custom_sd_b.GetCampaignString();
            if (camp_a != camp_b)
                return camp_a < camp_b;

            auto n_a = (std::string_view)*custom_sd_a.TryGetName();
            auto n_b = (std::string_view)*custom_sd_b.TryGetName();
            auto cmp = n_a.compare(n_b);
            if (cmp != 0)
                return cmp < 0;

            return a < b;
        };
        std::sort(base_skills.begin(), base_skills.end(), Comparer);

        return true;
    }

    std::vector<Filter> parsed_filters;
    std::vector<Command> parsed_commands;
    VariableSizeClipper clipper;
    bool TryUpdateFilter()
    {
#ifdef _TIMING
        auto start_timestamp = std::chrono::high_resolution_clock::now();
#endif

        if (base_skills.empty())
        {
            if (!TryInitBaseSkills())
                return false;
        }

        parsed_filters.clear();
        parsed_commands.clear();
        for (auto &[key, value] : pending_state.highlighting_map)
            value.clear(); // Instead of clearing the map we clear the vectors to avoid reallocations.
        pending_state.filtered_skills.clear();

        ParseInput(text_input, strlen(text_input), parsed_filters, parsed_commands);

        const auto target_agent_id = GW::Agents::GetHeroAgentID(pending_state.hero_index);

        if (pending_state.attribute_mode == AttributeMode::Characters)
        {
            auto custom_ad = CustomAgentDataModule::GetCustomAgentData(target_agent_id);
            for (uint32_t i = 0; i < pending_state.attributes.size(); i++)
            {
                auto attr_lvl = custom_ad.GetAttribute((AttributeOrTitle)i).value_or(0);
                pending_state.attributes[i] = attr_lvl;
            }
        }

        bool custom_sds_ready = true;
#ifdef _ARTIFICIAL_DELAY
        // Artificially fail 64 times to test long delays
        custom_sds_ready = false;
        static uint32_t artificial_fail_counter = 0;
        if ((++artificial_fail_counter % 64) == 0)
            custom_sds_ready = true;
#endif

        for (auto skill_id_short : base_skills)
        {
            const auto skill_id = static_cast<GW::Constants::SkillID>(skill_id_short);
            auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);

            if (!include_pve_only_skills)
            {
                if (custom_sd.tags.PvEOnly)
                    continue;
            }
            if (!include_temporary_skills)
            {
                if (custom_sd.tags.Temporary)
                    continue;
            }
            if (!include_archived_skills)
            {
                if (custom_sd.tags.Archived)
                    continue;
            }
            if (!include_npc_skills)
            {
                if (custom_sd.tags.MonsterSkill ||
                    custom_sd.tags.EnvironmentSkill)
                    continue;
            }
            if (!include_disguises)
            {
                if (custom_sd.skill->type == GW::Constants::SkillType::Disguise ||
                    custom_sd.skill_id == GW::Constants::SkillID::Tonic_Tipsiness)
                    continue;
            }

            auto attr_lvl = pending_state.GetAttribute(custom_sd.attribute);
            bool is_prepared = (custom_sd.TryGetName() != nullptr) &&
                               (custom_sd.TryGetPredecodedDescription(custom_sd.GetDescKey(false, attr_lvl)) != nullptr) &&
                               (custom_sd.TryGetPredecodedDescription(custom_sd.GetDescKey(true, attr_lvl)) != nullptr);

            if (!custom_sds_ready || !is_prepared)
            {
                custom_sds_ready = false;
                // Fail, but continue so that we load the strings needed later anyway.
                // After the loop we can return false.
                continue;
            }

            if (!CheckPassesFilters(parsed_filters, custom_sd))
            {
                continue;
            }

            pending_state.filtered_skills.push_back(static_cast<uint16_t>(skill_id));
        }

        if (!custom_sds_ready)
            return false;

#ifdef _TIMING
        auto timestamp_filtering = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp_filtering - start_timestamp).count();
        Utils::FormatToChat(L"Filtering took {} ms", duration);
#endif

        for (auto &command : parsed_commands)
        {
            ApplyCommand(command, pending_state.filtered_skills);
        }

#ifdef _TIMING
        auto timestamp_commands = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp_commands - timestamp_filtering).count();
        Utils::FormatToChat(L"Applying commands took {} ms", duration);
#endif

        UpdateFeedback(parsed_filters, parsed_commands);

        if (state.filtered_skills.size() > 0)
        {
            uint32_t max_count = std::min(state.filtered_skills.size(), pending_state.filtered_skills.size());
            max_count = std::min(max_count, clipper.visible_index_start + 1);
            uint32_t same_count = 0;
            while (same_count < max_count &&
                   pending_state.filtered_skills[same_count] == state.filtered_skills[same_count])
                same_count++;

            if (same_count > 1)
                clipper.SetScrollToIndex(same_count - 1);
            else
                clipper.Reset();

            // auto focused_skill = state.filtered_skills[clipper.visible_index_start];
            // auto it = std::find(pending_state.filtered_skills.begin(), pending_state.filtered_skills.end(), focused_skill);
            // if (it != pending_state.filtered_skills.end())
            // {
            //     auto index = std::distance(pending_state.filtered_skills.begin(), it);
            //     clipper.SetScrollToIndex(index);
            // }
            // else
            // {
            //     clipper.Reset();
            // }
        }

        // Recycle the heavy stuff and move in the new state
        auto old_filtered_skills = std::move(state.filtered_skills);
        auto old_highlighting_map = std::move(state.highlighting_map);
        state = std::move(pending_state);
        pending_state.filtered_skills = std::move(old_filtered_skills);
        pending_state.highlighting_map = std::move(old_highlighting_map);

        return true;
    }

    void Update()
    {
        GetSelectedHeroIndex();

        if (state_dirty)
        {
            if (TryUpdateFilter())
            {
                state_dirty = false;
            }
        }
    }

    void DrawAttributeModeSelection()
    {
        auto cursor_before = ImGui::GetCursorPos();
        ImGui::SetCursorPosY(cursor_before.y + 2);
        ImGui::TextUnformatted("Attributes");
        ImGui::SameLine();
        auto cursor_content_x = ImGui::GetCursorPosX();
        ImGui::SetCursorPosY(cursor_before.y);

        if (ImGui::RadioButton("(0...15)", pending_state.attribute_mode == AttributeMode::Generic))
        {
            pending_state.attribute_mode = AttributeMode::Generic;
            state_dirty = true;
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("Character's", pending_state.attribute_mode == AttributeMode::Characters))
        {
            pending_state.attribute_mode = AttributeMode::Characters;
            state_dirty = true;
        }

        ImGui::SameLine();
        if (ImGui::RadioButton("Manual", pending_state.attribute_mode == AttributeMode::Manual))
        {
            pending_state.attribute_mode = AttributeMode::Manual;
            state_dirty = true;
        }

        if (state.attribute_mode == AttributeMode::Manual)
        {
            // ImGui::SetCursorPosX(cursor_content_x);
            if (ImGui::SliderInt("Attribute level", &pending_state.attr_lvl_slider, 0, 21))
            {
                state_dirty = true;
            }
        }
    }

    void DrawSkillHeader(CustomSkillData &custom_sd, std::string_view name)
    {
        const auto skill_icon_size = 56;
        auto icon_cursor = ImGui::GetCursorPos();
        auto icon_cursor_ss = ImGui::GetCursorScreenPos();
        // bool is_equipable = GW::SkillbarMgr::GetIsSkillLearnt(custom_sd.skill_id);
        bool is_hovered = ImGui::IsMouseHoveringRect(icon_cursor_ss, icon_cursor_ss + ImVec2(skill_icon_size, skill_icon_size));
        bool is_effect = custom_sd.tags.EffectOnly;
        if (TextureModule::DrawSkill(*custom_sd.skill, icon_cursor_ss, skill_icon_size, is_effect, is_hovered) ||
            TextureModule::DrawSkill(*GW::SkillbarMgr::GetSkillConstantData(GW::Constants::SkillID::No_Skill), icon_cursor_ss, skill_icon_size, is_effect, is_hovered))
        {
            icon_cursor.y += skill_icon_size;
            ImGui::SameLine();
        }

        auto name_cursor = ImGui::GetCursorPos();
        auto content_max = ImGui::GetWindowContentRegionMax().x;
        auto cursor_x = ImGui::GetCursorPosX();

        // Draw skill name
        {
            ImGui::PushFont(Constants::Fonts::skill_name_font);
            auto name_color = custom_sd.tags.Archived ? Constants::GWColors::skill_dull_gray : Constants::GWColors::header_beige;
            ImGui::PushStyleColor(ImGuiCol_Text, name_color);
            auto &hightlighting = state.highlighting_map[GetHighlightKey(custom_sd.skill_id, SkillPropertyType::Name)];
            auto p = name.data();
            Utils::DrawMultiColoredText(p, p + name.size(), cursor_x, content_max, {}, hightlighting);

            // ImGui::SameLine();
            // // ImGui::SetWindowFontScale(0.7f);
            // char id_str[8];
            // snprintf(id_str, sizeof(id_str), "#%d", skill.skill_id);
            // // const auto id_str_size = ImGui::CalcTextSize(id_str.c_str());
            // // ImGui::SetCursorPosX(content_width - id_str_size.x - 4);
            // ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.3f));
            // ImGui::TextUnformatted(id_str);
            // ImGui::PopStyleColor();
            // ImGui::SetWindowFontScale(1.f);

            ImGui::PopStyleColor();
            ImGui::PopFont();
            auto size = ImGui::GetItemRectSize();

            if (custom_sd.tags.Archived)
            {
                const auto content_min = ImGui::GetCurrentWindow()->ContentRegionRect.Min;
                ImGui::GetWindowDrawList()->AddLine(content_min + ImVec2(name_cursor.x, name_cursor.y + size.y / 2),
                    content_min + ImVec2(name_cursor.x + size.x, name_cursor.y + size.y / 2),
                    Constants::GWColors::skill_dull_gray, 1.0f);
            }

            name_cursor.y += size.y;

            if (ImGui::GetIO().KeyCtrl)
            {
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted("Ctrl + Click to open wiki");
                    ImGui::EndTooltip();
                }

                const auto content_min = ImGui::GetCurrentWindow()->ContentRegionRect.Min;
                ImGui::GetWindowDrawList()->AddLine(content_min + ImVec2(name_cursor.x, name_cursor.y),
                    content_min + ImVec2(name_cursor.x + size.x, name_cursor.y),
                    Constants::GWColors::header_beige, 1.0f);
            }
        }

        // Draw skill stats
        DrawSkillStats(custom_sd);

        // Draw skill type
        {
            ImGui::SetCursorPos(name_cursor);
            auto &hightlighting = state.highlighting_map[GetHighlightKey(custom_sd.skill_id, SkillPropertyType::SkillType)];
            auto str = custom_sd.GetTypeString();
            auto p = str.data();
            Utils::DrawMultiColoredText(p, p + str.size(), cursor_x, content_max, {}, hightlighting);
            name_cursor.y += ImGui::GetItemRectSize().y + 2;
        }

        // Draw skill tags
        FixedArray<SkillTag, MAX_TAGS> tags_salloc;
        auto tags = tags_salloc.ref();
        custom_sd.GetTags(tags);
        if (show_null_stats || tags.size() > 0)
        {
            ImGui::SetCursorPos(name_cursor);
            for (uint32_t i = 0; i < tags.size(); i++)
            {
                auto &tag = tags[i];
                auto type = (SkillPropertyType)((uint32_t)SkillPropertyType::TAGS + i);
                auto &hightlighting = state.highlighting_map[GetHighlightKey(custom_sd.skill_id, type)];
                if (i > 0)
                {
                    ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted(", ");
                    ImGui::SameLine(0, 0);
                }
                auto str = SkillTagToString(tag);
                auto p = str.data();
                // ImGui::TextUnformatted(p);

                auto color = Constants::GWColors::skill_dull_gray;
                // clang-format off
                switch (tag) {
                    case SkillTag::Equipable:    color = ImColor(100, 255, 255); break;
                    case SkillTag::Unlocked:     color = Constants::GWColors::skill_dynamic_green; break;
                    case SkillTag::Locked:       color = ImColor(255, 100, 100); break; 
                }
                // clang-format on

                ImGui::PushStyleColor(ImGuiCol_Text, color);
                Utils::DrawMultiColoredText(p, p + str.size(), name_cursor.x, content_max, {}, hightlighting);
                ImGui::PopStyleColor();

                if (tag == SkillTag::MonsterSkill)
                {
                    auto packet = TextureModule::GetPacket_ImageInAtlas(TextureModule::KnownFileIDs::UI_SkillStatsIcons, ImVec2(16, 16), ImVec2(16, 16), 5);
                    if (packet.CanDraw())
                    {
                        ImGui::SameLine(0, 0);
                        packet.DrawOnWindow();
                    }
                }
            }

            name_cursor.y += ImGui::GetItemRectSize().y + 2;
        }

        icon_cursor.y = std::max(icon_cursor.y, name_cursor.y) + 3;
        ImGui::SetCursorPos(icon_cursor);
    }

    void DrawDescription(CustomSkillData &custom_sd, float content_width)
    {
        const auto attr_str = custom_sd.GetAttributeString();
        auto draw_param_tooltip = [&](uint32_t tooltip_id)
        {
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
                    text2 = "Number";
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
        };

        auto skill_id = custom_sd.skill_id;
        auto attr_lvl = state.GetAttribute(custom_sd.attribute);
        constexpr SkillPropertyType type[2] = {SkillPropertyType::Description, SkillPropertyType::Concise};
        auto &hightlighting = state.highlighting_map[GetHighlightKey(skill_id, type[prefer_concise_descriptions])];
        auto &hightlighting_other = state.highlighting_map[GetHighlightKey(skill_id, type[!prefer_concise_descriptions])];

        bool draw_full_desc = !prefer_concise_descriptions || !hightlighting.empty();

        // Should not be null because the description was used to filter the skills
        auto desc = *custom_sd.TryGetDescription(prefer_concise_descriptions, attr_lvl);
        auto desc_other = *custom_sd.TryGetDescription(!prefer_concise_descriptions, attr_lvl);

        desc.ImGuiRender(0, content_width, hightlighting, draw_param_tooltip);

        bool draw_other = hightlighting_other.size() > hightlighting.size();
        if (!draw_other)
        {
            auto len = std::min(hightlighting.size(), hightlighting_other.size());
            assert(len % 2 == 0);
            uint32_t i = 0;
            while (i < len)
            {
                std::string_view hl_a = ((std::string_view)desc.str).substr(hightlighting[i], hightlighting[i + 1] - hightlighting[i]);
                std::string_view hl_b = ((std::string_view)desc_other.str).substr(hightlighting_other[i], hightlighting_other[i + 1] - hightlighting_other[i]);
                i += 2;
                bool eq = (std::tolower(hl_a[0]) == std::tolower(hl_b[0])) &&
                          (hl_a.substr(1) == hl_b.substr(1));
                if (!eq)
                {
                    draw_other = true;
                    break;
                }
            }
        }

        if (draw_other)
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
            ImGui::TextUnformatted(prefer_concise_descriptions ? "Additional matches in full description: " : "Additional matches in concise description: ");
            ImGui::PopStyleColor();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3);
            // ImGui::SameLine(0, 0);
            // auto wrapping_min = ImGui::GetCursorPosX();

            desc_other.ImGuiRender(0, content_width, hightlighting_other, draw_param_tooltip);
        }
    }

    void DrawSkillFooter(CustomSkillData &custom_sd, float content_width)
    {
        auto &skill = *custom_sd.skill;
        ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        auto content_max = ImGui::GetWindowContentRegionMax().x;
        auto cursor_x = ImGui::GetCursorPosX();

        if (show_null_stats || !custom_sd.attribute.IsNone())
        {
            ImGui::TextUnformatted("Attribute: ");
            ImGui::SameLine();
            auto str = custom_sd.GetAttributeString();
            auto p = str.data();
            auto &highlighting = state.highlighting_map[GetHighlightKey(skill.skill_id, SkillPropertyType::Attribute)];
            Utils::DrawMultiColoredText(p, p + str.size(), cursor_x, content_max, {}, highlighting);
        }
        // if (show_null_stats || skill.title != 48)
        // {
        //     ImGui::TextUnformatted("Title: ");
        //     ImGui::SameLine();
        //     auto str = Utils::GetTitleString((GW::Constants::TitleID)skill.title);
        //     auto p = str.data();
        //     auto &highlighting = highlighting_map[GetHighlightKey(skill.skill_id, FilterTargetType::Attribute)];
        //     Utils::DrawMultiColoredText(p, p + str.size(), cursor_x, content_max, {}, highlighting);
        // }
        if (show_null_stats || skill.profession != GW::Constants::ProfessionByte::None)
        {
            ImGui::TextUnformatted("Profession: ");
            ImGui::SameLine();
            auto str = Utils::GetProfessionString(skill.profession);
            auto p = str.data();
            auto &highlighting = state.highlighting_map[GetHighlightKey(skill.skill_id, SkillPropertyType::Profession)];
            Utils::DrawMultiColoredText(p, p + str.size(), cursor_x, content_max, {}, highlighting);
        }
        if (show_null_stats || true)
        {
            ImGui::TextUnformatted("Campaign: ");
            ImGui::SameLine();
            auto str = Utils::GetCampaignString(skill.campaign);
            auto p = str.data();
            auto &highlighting = state.highlighting_map[GetHighlightKey(skill.skill_id, SkillPropertyType::Campaign)];
            Utils::DrawMultiColoredText(p, p + str.size(), cursor_x, content_max, {}, highlighting);
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

        FixedArray<Utils::Range, 4> ranges_salloc;
        auto ranges = ranges_salloc.ref();
        custom_sd.GetRanges(ranges);
        if (show_null_stats || ranges.size() > 0)
        {
            for (auto range : ranges)
            {
                ImGui::Text("Range: %d", (uint32_t)range);
                auto range_name = Utils::GetRangeStr(range);
                if (range_name)
                {
                    ImGui::SameLine();
                    ImGui::Text(" (%s)", range_name.value());
                }
            }
        }

        auto attr_lvl = state.GetAttribute(custom_sd.attribute);
        // auto duration = custom_sd.GetDuration();
        for (auto &pd : custom_sd.parsed_data)
        {
            pd.ImGuiRender(attr_lvl);
        }
        // if (show_null_stats || !duration.IsNull())
        // {
        //     duration.ImGuiRender("Duration", attr_lvl);
        // }
        // if (show_null_stats || !custom_sd.health_regen.IsNull())
        // {
        //     custom_sd.health_regen.ImGuiRender("Health regeneration", attr_lvl);
        // }
        // if (show_null_stats || !custom_sd.health_degen.IsNull())
        // {
        //     custom_sd.health_degen.ImGuiRender("Health degeneration", attr_lvl);
        // }
        // if (show_null_stats || !custom_sd.health_gain.IsNull())
        // {
        //     custom_sd.health_gain.ImGuiRender("Health gain", attr_lvl);
        // }
        // if (show_null_stats || !custom_sd.health_loss.IsNull())
        // {
        //     custom_sd.health_loss.ImGuiRender("Health loss", attr_lvl);
        // }
        // if (show_null_stats || !custom_sd.energy_regen.IsNull())
        // {
        //     custom_sd.energy_regen.ImGuiRender("Energy regeneration", attr_lvl);
        // }
        // if (show_null_stats || !custom_sd.energy_degen.IsNull())
        // {
        //     custom_sd.energy_degen.ImGuiRender("Energy degeneration", attr_lvl);
        // }
        // if (show_null_stats || !custom_sd.energy_gain.IsNull())
        // {
        //     custom_sd.energy_gain.ImGuiRender("Energy gain", attr_lvl);
        // }
        // if (show_null_stats || !custom_sd.energy_loss.IsNull())
        // {
        //     custom_sd.energy_loss.ImGuiRender("Energy loss", attr_lvl);
        // }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        { // Draw skill id
            ImGui::SetWindowFontScale(0.7f);
            const auto id_str = std::to_string((uint32_t)skill.skill_id);
            const auto id_str_size = ImGui::CalcTextSize(id_str.c_str());
            ImGui::SetCursorPosX(content_width - id_str_size.x - 4);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.3f));
            ImGui::TextUnformatted(id_str.c_str());
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.f);
        }
    }

    unsigned int countSetBits(unsigned int n)
    {
        unsigned int count = 0;
        while (n)
        {
            n &= (n - 1);
            count++;
        }
        return count;
    }

    bool CalcIdentMinChars(SkillPropertyType target_type, std::string_view ident, FixedArrayRef<char> char_buffer, FixedArrayRef<uint32_t> boundaries)
    {
        FixedArray<std::string_view, 8> words_alloc;
        auto words = words_alloc.ref();
        Utils::CamelSplit(ident, words);
        FixedArray<uint32_t, 8> words_len_alloc;
        auto word_lengths = words_len_alloc.ref();
        word_lengths.set_size(words.size());

        auto Check = [&]()
        {
            char_buffer.clear();
            for (uint32_t i = 0; i < words.size(); i++)
            {
                auto len = word_lengths[i];
                char_buffer.PushFormat("%.*s", len, words[i].data());
            }

            auto p = char_buffer.data();
            auto target = ParseFilterTarget(p, char_buffer.data_end());
            return target.type == target_type;
        };

        std::function<bool(uint32_t, std::span<uint32_t>)> DistChars =
            [&](uint32_t n_chars, std::span<uint32_t> words_len)
        {
            uint32_t n_words = words_len.size();
            if (n_words > 1)
            {
                for (uint32_t i = 0; i <= n_chars; i++)
                {
                    auto n_chars_in_first = (i + 1) % (n_chars + 1);
                    auto n_chars_in_rest = n_chars - n_chars_in_first;
                    words_len[0] = n_chars_in_first;
                    auto success = DistChars(n_chars_in_rest, std::span<uint32_t>(words_len.data() + 1, n_words - 1));
                    if (success)
                    {
                        return true;
                    }
                }
                return false;
            }
            else
            {
                words_len[0] = n_chars;
                return Check();
            }
        };

        uint32_t req_len = 0;
        while (req_len < ident.size())
        {
            req_len++;
            // Iterate all possible ways to assign 'req_len' chars to the words
            // and check if any of them match the target type.
            bool success = DistChars(req_len, word_lengths);
            if (success)
            {
                boundaries.clear();
                for (uint32_t i = 0; i < words.size(); i++)
                {
                    if (word_lengths[i] == 0)
                        continue;

                    auto start_in_ident = words[i].data() - ident.data();
                    auto end_in_ident = start_in_ident + word_lengths[i];
                    boundaries.try_push(start_in_ident);
                    boundaries.try_push(end_in_ident);
                }
                return true;
            }
        }

        return false;
    };

    void DrawFilterTooltip()
    {
        ImGui::BeginTooltip();

        auto width = 600;
        std::string_view text;

        { // General info

            text = "Here you can narrow down the results by using a filter.";
            Utils::DrawMultiColoredText(text.data(), nullptr, 0, width);

            text = "A filter requires a target, an operator and one or more values. "
                   "If you do not specify target or operator, the target will be 'Text', and the operator will be ':'. "
                   "This will essentially check if any text of the skill contains the provided string.";
            Utils::DrawMultiColoredText(text.data(), nullptr, 0, width);

            text = "Multiple values can be specified for a filter by separating them with '/', "
                   "then ANY of them must be satisfied for that filter to match.";
            Utils::DrawMultiColoredText(text.data(), nullptr, 0, width);

            text = "You can use '&' (AND) or '|' (OR) to combine multiple filters.";
            Utils::DrawMultiColoredText(text.data(), nullptr, 0, width);

            text = "Once you are satisified with the filters, you may want to sort the results. "
                   "This can be achieved by typing the special command \"#sort\" followed by a "
                   "whitespace-separated list of targets to sort by. The results will be sorted by the first target, "
                   "then by the second, and so on. To invert the order prepend a '!' character before the target";
            Utils::DrawMultiColoredText(text.data(), nullptr, 0, width);

            text = "If you are unsure how to proceed, take a look at the examples below.";
            Utils::DrawMultiColoredText(text.data(), nullptr, 0, width);
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

        FixedArray<char, 128> char_buffer_alloc;
        FixedArray<Utils::ColorChange, 8> col_buffer_alloc;
        FixedArray<uint32_t, 8> boundaries_alloc;
        auto char_buffer = char_buffer_alloc.ref();
        auto col_buffer = col_buffer_alloc.ref();
        auto boundaries = boundaries_alloc.ref();
        ImU32 req_color = IM_COL32(0, 255, 0, 255);

        { // Target and operator info
            ImGui::Columns(3, "mycolumns", false);

            auto DrawTargets = [&](std::string_view type, const auto &target_array)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::header_beige);
                ImGui::Text("%s targets:", type);
                ImGui::PopStyleColor();
                uint32_t i = 0;
                for (auto &[ident, target_type] : target_array)
                {
                    if (i++ == 25)
                        ImGui::NextColumn();
                    bool success = CalcIdentMinChars(target_type, ident, char_buffer, boundaries);
                    assert(success);

                    assert(boundaries.size() % 2 == 0);
                    col_buffer.clear();
                    for (int32_t i = 0; i < boundaries.size(); i += 2)
                    {
                        col_buffer.try_push({boundaries[i] + 2, req_color});
                        col_buffer.try_push({boundaries[i + 1] + 2, 0});
                    }

                    char_buffer.clear();
                    char_buffer.PushFormat("- %s", ident.data());
                    Utils::DrawMultiColoredText(char_buffer.data(), char_buffer.data_end(), 0, width, col_buffer);
                }
            };

            auto DrawOperators = [&](std::string_view type, const auto &op_array, SkillPropertyType prop_type)
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
                    Utils::DrawMultiColoredText(char_buffer.data(), char_buffer.data_end(), 0, width, col_buffer);
                }
            };

            DrawTargets("Text", text_filter_targets);
            ImGui::Spacing();
            DrawOperators("Text", text_filter_operators, SkillPropertyType::TEXT);
            ImGui::Spacing();
            DrawOperators("Number", number_filter_operators, SkillPropertyType::NUMBER);

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
            text = "The highlighted characters indicate the minimum required to uniquely identify it.";
            Utils::DrawMultiColoredText(text.data(), nullptr, 0, width / 3);
            ImGui::PopStyleColor();

            ImGui::NextColumn();
            DrawTargets("Number", number_filter_targets);

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
                for (auto &[ident, op] : adv_filter_operators)
                {
                    const auto desc = GetOpDescription(SkillPropertyID{SkillPropertyType::RAW, 0}, op);
                    char_buffer.clear();
                    char_buffer.PushFormat("- %s %s", ident.data(), desc.data());
                    col_buffer.try_push({2, req_color});
                    col_buffer.try_push({2 + ident.size(), 0});
                    Utils::DrawMultiColoredText(char_buffer.data(), char_buffer.data_end(), 0, width, col_buffer);
                }
            }
        }

        ImGui::EndTooltip();
    }

    int FilterCallback(ImGuiInputTextCallbackData *data)
    {
        state_dirty = true;

        return 0;
    }

    void DrawSearchBox()
    {
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint(
            "",
            "Search for a skill...",
            text_input, sizeof(text_input),
            ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_AutoSelectAll,
            FilterCallback,
            nullptr);
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1);
        ImGui::TextUnformatted("(?)");
        if (ImGui::IsItemHovered())
        {
            DrawFilterTooltip();
        }

        input_feedback.ImGuiRender(0, ImGui::GetWindowContentRegionMax().x);
    }

    bool first_draw = true;
    void Draw(IDirect3DDevice9 *device)
    {
        if (first_draw)
        {
            auto vpw = GW::Render::GetViewportWidth();
            auto vph = GW::Render::GetViewportHeight();
            const auto window_width = 600;
            const auto offset_from_top = ImGui::GetFrameHeight();
            ImGui::SetNextWindowPos(ImVec2(vpw - window_width, offset_from_top), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(window_width, vph - offset_from_top), ImGuiCond_FirstUseEver);
            first_draw = false;
        }

        if (auto _ = WINDOW_ALPHA_SCOPE(); ImGui::Begin("Skill Book (Ctrl + K)", &UpdateManager::open_skill_book, UpdateManager::GetWindowFlags()))
        {
            auto n_skills = state.filtered_skills.size();

            DrawCheckboxes();
            DrawAttributeModeSelection();
            DrawSearchBox();

            ImGui::PushStyleColor(ImGuiCol_Text, Constants::GWColors::skill_dull_gray);
            ImGui::Text("%s results", std::to_string(n_skills).c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();

            if (ImGui::BeginChild("SkillList"))
            {
                auto est_item_height = 128.f;
                // auto est_item_height = 1.f;

                auto draw_list = ImGui::GetWindowDrawList();

                auto DrawItem = [&](uint32_t i)
                {
                    const auto skill_id = static_cast<GW::Constants::SkillID>(state.filtered_skills[i]);
                    auto &custom_sd = CustomSkillDataModule::GetCustomSkillData(skill_id);

                    const auto initial_screen_cursor = ImGui::GetCursorScreenPos();
                    const auto initial_cursor = ImGui::GetCursorPos();
                    const auto content_width = ImGui::GetWindowContentRegionWidth();

                    // Should not be null because the name was used to filter the skills
                    std::string_view name = *custom_sd.TryGetName();
                    DrawSkillHeader(custom_sd, name);
                    DrawDescription(custom_sd, content_width);
                    ImGui::Spacing();
                    DrawSkillFooter(custom_sd, content_width);
                    auto final_screen_cursor = ImGui::GetCursorScreenPos();

                    auto entry_screen_max = final_screen_cursor;
                    entry_screen_max.x += content_width;

                    if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(initial_screen_cursor, entry_screen_max))
                    {
                        if (ImGui::IsMouseClicked(0))
                        {
                            if (ImGui::GetIO().KeyCtrl)
                            {
                                Utils::OpenWikiPage(name);
                            }
                            else
                            {
                                UpdateManager::RequestSkillDragging(skill_id);
                            }
                        }
#ifdef _DEBUG
                        Debug::SetHoveredSkill(skill_id);
#endif
                    }

                    ImGui::Separator();
                };

                clipper.Draw(n_skills, est_item_height, snap_to_skill, DrawItem);
            }
            ImGui::EndChild();
        }

        ImGui::End();
    }
}