#include <GWCA/Constants/Constants.h>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Utilities/Hook.h>
#include <GWCA/Utilities/Hooker.h>

#include <custom_agent_data.h>
#include <custom_skill_data.h>
#include <damage_display.h>
#include <debug.h>
#include <debug_display.h>
#include <debug_ui.h>
#include <effect_initiator.h>
#include <effect_tracking.h>
#include <encstr_debugger.h>
#include <hero_ai.h>
#include <packet_stepper.h>
#include <party_data.h>
#include <settings.h>
#include <skill_book.h>
#include <texture_module.h>
#include <texture_viewer.h>
#include <version.h>
#include <worldspaceUI.h>

#include "update_manager.h"
#include <imgui_ext.h>
#include <utils.h>

namespace HerosInsight
{
#ifdef _DEBUG
    const bool LOG_GWCA_TO_CHAT = true;
#else
    const bool LOG_GWCA_TO_CHAT = false;
#endif

    uint32_t UpdateManager::frame_id = -1;
    DWORD UpdateManager::start_gw_ms = 0;
    DWORD UpdateManager::elapsed_ms = 0;
    float UpdateManager::delta_seconds = 0.0f;
    float UpdateManager::elapsed_seconds = 0.0f;

    DWORD UpdateManager::render_elapsed_ms = 0;
    float UpdateManager::render_delta_seconds = 0.0f;

#ifdef _DEBUG
    bool UpdateManager::enable_ui_debug = false;
    bool UpdateManager::open_debug = true;
    bool UpdateManager::open_texture_viewer = false;
    bool UpdateManager::open_encstr_debugger = false;
#else
#endif
    bool UpdateManager::open_skill_book = false;
    bool UpdateManager::open_main_menu = true;
    bool UpdateManager::open_settings = false;
#ifdef EXPERIMENTAL_FEATURES
    bool UpdateManager::open_damage = true;
#endif

    enum struct GameState
    {
        Null,
        Loading,
        InOutpost,
        InExplorable,
        InCinematic,
        Observing
    };

    GameState prev_game_state = GameState::Loading;
    GameState game_state = GameState::Loading;

    GameState GetGameState()
    {
        const auto instance_type = GW::Map::GetInstanceType();
        if (!GW::Map::GetIsMapLoaded() ||
            instance_type == GW::Constants::InstanceType::Loading ||
            GW::PartyMgr::GetPartyInfo() == nullptr)
            return GameState::Loading;

        if (GW::Map::GetIsInCinematic())
            return GameState::InCinematic;

        if (GW::Map::GetIsObserving())
            return GameState::Observing;

        if (instance_type == GW::Constants::InstanceType::Explorable)
            return GameState::InExplorable;

        if (instance_type == GW::Constants::InstanceType::Outpost)
            return GameState::InOutpost;

        return GameState::Null;
    }

    GW::HookEntry log_context;
    void LogHandler(
        void *context,
        GW::LogLevel level,
        const char *msg,
        const char *file,
        unsigned int line,
        const char *function
    )
    {
        if (level == GW::LogLevel::LEVEL_INFO)
        {
            HerosInsight::Utils::FormatToChat(L"GWCA: {}", HerosInsight::Utils::StrToWStr(msg));
            // HerosInsight::DebugDisplay::PushToDisplay(HerosInsight::Utils::StrToWStr(msg));
        }
    }

    void UpdateManager::Initialize()
    {
#ifdef _DEBUG
        HerosInsight::Debug::Initialize();
#endif

#ifdef EXPERIMENTAL_FEATURES
        HerosInsight::PacketReader::Initialize();
        HerosInsight::PacketStepper::Initialize();
        HerosInsight::EffectInitiator::Initialize();
        HerosInsight::EnergyDisplay::Initialize();
#endif

        HerosInsight::CustomAgentDataModule::Initialize();
        HerosInsight::CustomSkillDataModule::Initialize();
        HerosInsight::SkillBook::Initialize();

        if (LOG_GWCA_TO_CHAT)
        {
            GW::RegisterLogHandler(&LogHandler, &log_context);
        }
        GW::Chat::WriteChat(GW::Chat::CHANNEL_MODERATOR, L"Hero's Insight: Initialized");
    }

    void UpdateManager::Terminate()
    {
#ifdef _DEBUG
        HerosInsight::Debug::Terminate();
#endif
        TextureModule::Terminate();
        HerosInsight::SkillBook::Terminate();
        HerosInsight::PacketReader::Terminate();
        HerosInsight::PacketStepper::Terminate();
        HerosInsight::EffectInitiator::Terminate();
        HerosInsight::EnergyDisplay::Terminate();
        GW::Chat::WriteChat(GW::Chat::CHANNEL_MODERATOR, L"Hero's Insight: Terminated");
    }

    void OnUpdate()
    {
        assert(game_state != GameState::Null);

        if (game_state != prev_game_state)
        {
            // Game state changed

            if (game_state == GameState::InExplorable)
            {
                // Entered explorable

#ifdef EXPERIMENTAL_FEATURES
                HerosInsight::PartyDataModule::Initialize();
#endif
            }

            if (prev_game_state == GameState::InExplorable)
            {
                // Exited explorable

#ifdef EXPERIMENTAL_FEATURES
                HerosInsight::PartyDataModule::Terminate();
                HerosInsight::EffectTracking::Reset();
                HerosInsight::WorldSpaceUI::Reset();
#endif
            }
        }

        if (game_state == GameState::Loading ||
            game_state == GameState::InCinematic)
            return;

#ifdef _DEBUG
        if (UpdateManager::open_debug)
            HerosInsight::Debug::Update();
#endif
        if (UpdateManager::open_skill_book)
            HerosInsight::SkillBook::Update();

#ifdef EXPERIMENTAL_FEATURES
        HerosInsight::EnergyDisplay::Update();
#endif

        if (game_state != GameState::Observing)
        {
            // Not observing

            if (game_state == GameState::InExplorable)
            {
#ifdef EXPERIMENTAL_FEATURES
                HerosInsight::PartyDataModule::Update();
                HerosInsight::EffectTracking::Update();
                HerosInsight::WorldSpaceUI::Update();
#endif
            }
            else if (game_state == GameState::InOutpost)
            {
            }
        }
    }

    bool first_draw = true;
    void DrawMenu()
    {
        if (!UpdateManager::open_main_menu)
            return;

        auto &style = ImGui::GetStyle();

        if (first_draw)
        {
            auto vpw = GW::Render::GetViewportWidth();
            auto vph = GW::Render::GetViewportHeight();
            float window_width = 600;
            auto init_pos = ImVec2(vpw - 400, 0);
            ImGui::SetNextWindowPos(init_pos, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
            first_draw = false;
        }

        const auto flags = 0                           //
                           | ImGuiWindowFlags_NoResize //
                                                       // | ImGuiWindowFlags_AlwaysAutoResize //
                                                       // | ImGuiWindowFlags_NoCollapse       //
            /**/;

        auto current_font = ImGui::GetFont();
        ImGui::PushFont(Constants::Fonts::window_name_font);

        auto window_name = "Hero's Insight - Menu";
        auto window_name_width = ImGui::CalcTextSize(window_name).x //
                                 + style.WindowPadding.x * 2.0f     //
                                 + style.FramePadding.x * 2.0f      //
                                 + 10.0f;                           // For good measure :)

        ImGui::SetNextWindowSize(
            ImVec2(0.0f, 0.0f),
            ImGuiCond_Always
        );
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(window_name_width, 0.0f),
            ImVec2(FLT_MAX, FLT_MAX)
        );

        const auto window_menu_button_position = style.WindowMenuButtonPosition;
        style.WindowMenuButtonPosition = ImGuiDir_None;
        {
            bool visible_content = ImGui::Begin(window_name, &UpdateManager::open_main_menu, flags);
            style.WindowMenuButtonPosition = window_menu_button_position;

            const auto window = ImGui::GetCurrentWindow();
            const bool is_collapsed = window->Collapsed;
            const auto window_min = window->Pos;
            const auto window_size = window->Size;
            const auto window_max = window_min + window_size;
            const bool is_hovered = ImGui::IsMouseHoveringRect(window_min, window_max, false);
            window->Collapsed = !is_hovered;
            if (is_hovered)
            {
                ImGui::FocusWindow(window);
            }
            if (visible_content)
            {
                ImGui::PushFont(current_font);
#ifdef _DEBUG
                ImGui::TextUnformatted("Windows");
                if (ImGui::Checkbox("Debug UI", &UpdateManager::enable_ui_debug))
                {
                    if (UpdateManager::enable_ui_debug)
                        HerosInsight::DebugUI::EnableUIMessageLogging();
                    else
                        HerosInsight::DebugUI::DisableUIMessageLogging();
                }
                ImGui::Checkbox("Debug Display", &UpdateManager::open_debug);
                ImGui::Checkbox("Texture Viewer", &UpdateManager::open_texture_viewer);
                ImGui::Checkbox("Encoded String Debugger", &UpdateManager::open_encstr_debugger);
#endif
                ImGui::Checkbox("Skill Book", &UpdateManager::open_skill_book);
                Utils::ImGuiDisabledCheckboxWithTooltip("Party statistics", "This feature is not yet stabilized.");
                Utils::ImGuiDisabledCheckboxWithTooltip("Effect UI", "This feature is not yet stabilized.");
                ImGui::Checkbox("Settings", &UpdateManager::open_settings);
#ifdef EXPERIMENTAL_FEATURES
                ImGui::Checkbox("Damage Display", &DamageDisplay::enabled);
#endif
                // const auto content_size = ImGui::GetItemRectMax();
                // window->Size = content_size - ImVec2(0, 50);

                ImGui::Spacing();

                if (ImGuiExt::Button("Open Github"))
                {
                    Utils::OpenURL("https://github.com/cfognom/HerosInsight");
                }

                ImGui::Text("Version: %s", HEROSINSIGHT_VERSION_STRING);
#ifdef _DEBUG
                ImGui::TextUnformatted("Alt + End => Terminate addon");
#endif
                ImGui::PopFont();
            }
            ImGui::End();
        }
        ImGui::PopFont();
    }

    void UpdateManager::Draw(IDirect3DDevice9 *device)
    {
        assert(game_state != GameState::Null);

        const auto gw_ms = GW::MemoryMgr::GetSkillTimer();
        UpdateManager::render_delta_seconds = static_cast<float>(gw_ms - UpdateManager::render_elapsed_ms) / 1000.f;
        UpdateManager::render_elapsed_ms = gw_ms;

        TextureModule::DxUpdate(device);

        bool hide = game_state == GameState::Loading ||
                    game_state == GameState::InCinematic ||
                    GW::UI::GetIsWorldMapShowing();
        auto bg_draw_list = ImGui::GetBackgroundDrawList();

        if (hide)
        {
            // Instead of returning early to hide the UI,
            // we draw everything with alpha = 0 to avoid triggering
            // IsWindowAppearing etc.
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.f);
            bg_draw_list->PushClipRect(ImVec2(0, 0), ImVec2(0, 0), true); // Clip everything in bg_draw_list
        }

#ifdef EXPERIMENTAL_FEATURES
        HerosInsight::EnergyDisplay::Draw(device);

        if (game_state == GameState::InExplorable)
        {
            HerosInsight::WorldSpaceUI::Draw(device);
            if (DamageDisplay::enabled)
                HerosInsight::DamageDisplay::Draw(device);
        }
#endif

#ifdef _DEBUG
        if (open_texture_viewer)
            TextureViewer::Draw();
        if (open_encstr_debugger)
            HerosInsight::EncstrDebugger::Draw();
        if (enable_ui_debug)
            HerosInsight::DebugUI::Draw(device);
        if (open_debug)
            HerosInsight::DebugDisplay::Draw(device);
#endif
        if (open_skill_book)
            HerosInsight::SkillBook::Draw(device);
        if (open_settings)
            HerosInsight::Settings::Draw(device);

        DrawMenu();

        if (hide)
        {
            ImGui::PopStyleVar();
            bg_draw_list->PopClipRect();
        }
    }

    void UpdateManager::Update()
    {
        UpdateManager::frame_id++; // We increment frame_id at the beginning of the frame so that any draw updates that happen after this use the same frame_id
        const auto gw_ms = GW::MemoryMgr::GetSkillTimer();

        if (UpdateManager::frame_id == 0)
        {
            UpdateManager::start_gw_ms = gw_ms;
        }
        const auto prev_elapsed_ms = UpdateManager::elapsed_ms;
        const auto elapsed_ms = gw_ms - UpdateManager::start_gw_ms;
        const auto elapsed_seconds = static_cast<float>(elapsed_ms) / 1000.f;
        const auto delta_ms = elapsed_ms - prev_elapsed_ms;
        const auto delta_seconds = static_cast<float>(delta_ms) / 1000.f;

        UpdateManager::elapsed_ms = elapsed_ms;
        UpdateManager::delta_seconds = delta_seconds;
        UpdateManager::elapsed_seconds = elapsed_seconds;
        game_state = GetGameState();

        OnUpdate();

        prev_game_state = game_state;
    }
}