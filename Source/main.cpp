#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Windowsx.h>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iterator>

#include <string>

#include <GWCA/GWCA.h>
#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/NPC.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/AgentContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/GadgetContext.h>
#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/PreGameContext.h>

#include <GWCA/Constants/AgentIDs.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/Constants/Maps.h>
#include <GWCA/Constants/Skills.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/GameContainers/List.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/Module.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Packets/StoC.h>

#include <capacity_hints.h>
#include <constants.h>
#include <crash_handling.h>
#include <imgui.h>
#include <imgui_customize.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <skill_book.h>

#include <update_manager.h>

// We can forward declare, because we won't use it
struct IDirect3DDevice9;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static long prevWndProc = 0;
static volatile bool running = true;

// Must be called from the render thread
void TerminateFromRenderThread();

static LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    auto CheckKeyBinds = [&]() -> bool
    {
        constexpr auto size = std::size(HerosInsight::UpdateManager::key_bindings);
        static uint32_t pressed_bits = 0; // global state of keybindings
        static_assert(size <= 32, "Too many key bindings");
        for (size_t i = 0; i < size; i++)
        {
            auto &[state, modifier, key_binding] = HerosInsight::UpdateManager::key_bindings[i];

            uint32_t mask = 1 << i;

            if (modifier)
            {
                bool modifier_down = GetAsyncKeyState(modifier) & 0x8000;
                // If the modifier is not pressed, clear the state
                if (!modifier_down)
                {
                    pressed_bits &= ~mask;
                    continue;
                }
            }

            if (wParam != key_binding)
                continue;

            if (Message == WM_KEYDOWN && !(pressed_bits & mask))
            {
                *state = !*state;
                pressed_bits |= mask;
                return true; // Block input to the game
            }
            else if (Message == WM_KEYUP)
            {
                pressed_bits &= ~mask;
            }
        }
        return false;
    };

    static bool right_mouse_down = false;

    if (Message == WM_RBUTTONDOWN)
        right_mouse_down = true;
    if (Message == WM_RBUTTONDBLCLK)
        right_mouse_down = true;
    if (Message == WM_RBUTTONUP)
        right_mouse_down = false;

    ImGuiIO &io = ImGui::GetIO();

    ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam);

    //
    // This switch is used to determine whether we need to forward the input to Guild Wars.
    //
    bool is_up = false;
    switch (Message)
    {
        // Send button up mouse events to everything, to avoid being stuck on mouse-down
        case WM_LBUTTONUP:
            break;

        // Other mouse events:
        // - If right mouse down, leave it to gw
        // - ImGui first (above), if WantCaptureMouse that's it
        // - Toolbox module second (e.g.: minimap), if captured, that's it
        // - otherwise pass to gw
        case WM_MOUSEMOVE:
            break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        {
            if (!right_mouse_down && io.WantCaptureMouse)
                return true;
            break;
        }

        // keyboard messages
        case WM_KEYUP:
        case WM_SYSKEYUP:
            is_up = true;
            [[fallthrough]];
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_IME_CHAR:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONDBLCLK:
        case WM_XBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONUP:
            if (CheckKeyBinds())
                return true;

            if (io.WantTextInput)
            {
                if (is_up)
                    break; // if imgui wants them, send to imgui (above) and to gw
                else
                    return true; // if imgui wants them, send just to imgui (above)
            }

            // note: capturing those events would prevent typing if you have a hotkey assigned to normal letters.
            // We may want to not send events to toolbox if the player is typing in-game
            // Otherwise, we may want to capture events.
            // For that, we may want to only capture *successfull* hotkey activations.
            break;

        case WM_SIZE:
            // ImGui doesn't need this, it reads the viewport size directly
            break;

        default:
            break;
    }

    return CallWindowProc((WNDPROC)prevWndProc, hWnd, Message, wParam, lParam);
}

static LRESULT CALLBACK SafeWndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    __try
    {
        return WndProc(hWnd, Message, wParam, lParam);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return CallWindowProc(reinterpret_cast<WNDPROC>(prevWndProc), hWnd, Message, wParam, lParam);
    }
}

// This function attempts to mark regions of the screen where rendering should be disabled.
// For example, the current tooltip frame, the current dragged skill frame, etc.
// It returns a stateblock that can be used to restore the previous state, or a nullptr if nothing was done.
IDirect3DStateBlock9 *TryPrepareStencil(IDirect3DDevice9 *device)
{
    HerosInsight::FixedVector<ImRect, 2> holes;

    auto dragged_skill_frame = HerosInsight::Utils::GetDraggedSkillFrame();
    if (dragged_skill_frame &&
        dragged_skill_frame->IsVisible())
    {
        auto rect = HerosInsight::Utils::GetFrameRect(*dragged_skill_frame);
        holes.try_push(rect);
    }

    auto tt_frame = HerosInsight::Utils::GetTooltipFrame();
    if (tt_frame && (HerosInsight::SkillBook::IsDragging() || !ImGui::GetIO().WantCaptureMouse))
    {
        auto rect = HerosInsight::Utils::GetFrameRect(*tt_frame);
        rect.Min.y -= 1.f;
        rect.Max.y -= 1.f; // Looks better
        holes.try_push(rect);
    }

    if (holes.size() == 0)
        return nullptr;

    return HerosInsight::Utils::PrepareStencilHoles(device, holes);
}

static void OnRender(void *data)
{
    auto helper = *static_cast<GW::Render::Helper *>(data);
    auto device = helper.device;

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX9_NewFrame();
    ImGui::NewFrame();

    HerosInsight::UpdateManager::Draw(device);

    auto old_state = TryPrepareStencil(device);

    ImGui::Render();
    ImDrawData *draw_data = ImGui::GetDrawData();
    ImGui_ImplDX9_RenderDrawData(draw_data);

    if (old_state)
    {
        old_state->Apply();
        old_state->Release();
    }
}

static void OnUpdate(void *data)
{
    HerosInsight::UpdateManager::Update();
}

static void OnRender_CheckPermission(void *data)
{
    static bool is_modding_allowed = false;
    if (GW::Map::GetIsMapLoaded())
    {
        is_modding_allowed = HerosInsight::Utils::IsModdingAllowed();

        static bool hooks_enabled = false;
        if (is_modding_allowed && !hooks_enabled)
        {
            GW::EnableHooks();
            hooks_enabled = true;
        }
        else if (!is_modding_allowed && hooks_enabled)
        {
            GW::Chat::WriteChat(GW::Chat::CHANNEL_MODERATOR, L"Hero's Insight: Modding is not allowed in this zone. Hero's Insight has been disabled and will be re-enabled when you leave this area.");
            GW::DisableHooks();
            GW::EnableRenderHooks();
            hooks_enabled = false;
        }
    }

    if (is_modding_allowed)
    {
        OnRender(data);
    }

    if (!HerosInsight::UpdateManager::open_main_menu
#ifdef _DEBUG
        || ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(VK_END) & 0x8000))
#endif
    )
    {
        TerminateFromRenderThread();
    }
}

struct WindowOverride
{
    HWND hWnd;
    WindowOverride(HWND hWnd, LONG_PTR wndProc)
        : hWnd(hWnd)
    {
        prevWndProc = SetWindowLongPtr(hWnd, GWL_WNDPROC, wndProc);
    }
    ~WindowOverride()
    {
        SetWindowLongPtr(hWnd, GWL_WNDPROC, prevWndProc);
    }
};

struct ImGuiScope
{
    struct Context
    {
        Context() { ImGui::CreateContext(); }
        ~Context() { ImGui::DestroyContext(); }
    };
    struct ImplWin32
    {
        ImplWin32(HWND hWnd) { ImGui_ImplWin32_Init(hWnd); }
        ~ImplWin32() { ImGui_ImplWin32_Shutdown(); }
    };
    struct ImplDx9
    {
        ImplDx9(IDirect3DDevice9 *device) { ImGui_ImplDX9_Init(device); }
        ~ImplDx9() { ImGui_ImplDX9_Shutdown(); }
    };
    Context context;
    ImplWin32 implWin32;
    ImplDx9 implDx9;
    ImGuiScope(HWND hWnd, IDirect3DDevice9 *device)
        : context(),
          implWin32(hWnd),
          implDx9(device)
    {
        HerosInsight::ImGuiCustomize::Init();
    }
};

struct UpdateManagerScope
{
    UpdateManagerScope() { HerosInsight::UpdateManager::Initialize(); }
    ~UpdateManagerScope() { HerosInsight::UpdateManager::Terminate(); }
};

struct RenderThreadScope
{
    WindowOverride windowOverride;
    ImGuiScope imguiScope;
    UpdateManagerScope updateManagerScope;
    GW::HookEntry game_loop_callback_entry;

    RenderThreadScope(HWND hWnd, IDirect3DDevice9 *device)
        : windowOverride(hWnd, reinterpret_cast<LONG_PTR>(SafeWndProc)),
          imguiScope(hWnd, device),
          updateManagerScope()
    {
        GW::Render::SetRenderCallback(
            [](GW::Render::Helper helper, void *)
            {
                if (!HerosInsight::CrashHandling::SafeCall(&OnRender_CheckPermission, &helper))
                {
                    TerminateFromRenderThread();
                }
            }
        );

        GW::Render::SetResetCallback(
            [](GW::Render::Helper helper, void *)
            {
                ImGui_ImplDX9_InvalidateDeviceObjects();
            }
        );

        GW::GameThread::RegisterGameThreadCallback(
            &game_loop_callback_entry,
            [](GW::HookStatus *)
            {
                if (!HerosInsight::CrashHandling::SafeCall(&OnUpdate))
                {
                    TerminateFromRenderThread();
                }
            }
        );
        HerosInsight::CrashHandling::msg_overload = L"";
    }
    ~RenderThreadScope()
    {
        GW::DisableHooks();
        GW::Render::SetRenderCallback(nullptr);
        GW::GameThread::RemoveGameThreadCallback(&game_loop_callback_entry);
    }
};
std::optional<RenderThreadScope> render_thread_scope = std::nullopt;

void TerminateFromRenderThread()
{
    if (running)
    {
        GW::DisableHooks();
        GW::Render::SetRenderCallback(nullptr);
        running = false;
        if (render_thread_scope.has_value())
        {
            render_thread_scope.reset();
        }
    }
}

struct CapacityHintsScope
{
    CapacityHintsScope() { HerosInsight::CapacityHints::LoadHints(); }
    ~CapacityHintsScope() { HerosInsight::CapacityHints::SaveHints(); }
};

struct GWCAScope
{
    GWCAScope() { GW::Initialize(); }
    ~GWCAScope() { GW::Terminate(); }
};

struct MainScope
{
    GWCAScope gwca_scope;
    CapacityHintsScope capacity_hints_scope;
    HWND hWnd;
    MainScope()
        : gwca_scope(),
          capacity_hints_scope()
    {
        hWnd = GW::MemoryMgr::GetGWWindowHandle();

        // We temporarily set the render callback to initialize render_scope, since all initialization must happen in the game thread ???
        GW::Render::SetRenderCallback(
            [](GW::Render::Helper helper, void *user_arg)
            {
                // This is call from within the game thread and all operation should be done here. (Although, Note that: GW::GameThread::IsInGameThread() == false)
                // You can't freeze this thread, so no blocking operation or at your own risk.
                struct Param
                {
                    MainScope *main;
                    GW::Render::Helper helper;
                } param;
                param.main = static_cast<MainScope *>(user_arg);
                param.helper = helper;

                constexpr auto Init = [](void *user_arg)
                {
                    auto param = *static_cast<Param *>(user_arg);
                    render_thread_scope.emplace(param.main->hWnd, param.helper.device);
                };

                if (!HerosInsight::CrashHandling::SafeCall(Init, &param))
                {
                    // Failed to initialize
                    TerminateFromRenderThread();
                }
            },
            this
        );
        GW::EnableRenderHooks();

        while (running)
        {
            Sleep(100);
        }

        // Hooks are disabled from Guild Wars thread (safely), so we just make sure we exit the last hooks
        while (GW::HookBase::GetInHookCount())
            Sleep(16);

        // We can't guarantee that the code in Guild Wars thread isn't still in the trampoline, but
        // practically a short sleep is fine.
        Sleep(16);
    }
};

static DWORD WINAPI ThreadProc(LPVOID lpModule)
{
    // This is a new thread so you should only initialize GWCA and setup the hook on the game thread.
    // When the game thread hook is setup (i.e. SetRenderCallback), you should do the next operations
    // on the game from within the game thread.

    HMODULE hModule = static_cast<HMODULE>(lpModule);

    HerosInsight::CrashHandling::msg_overload =
        L"Hero's Insight failed to initialize."
        L"\n\n"
        L"If this happened after a game update, it means the mod is incompatible with this Guild Wars build. "
        L"Please wait until the mod developer has fixed the mod and then try again.";

    if (!HerosInsight::CrashHandling::SafeCall(
            [](void *data)
            {
                MainScope main_scope{};
                // Destructors are called here
            }
        ))
    {
    }
    FreeLibraryAndExitThread(hModule, EXIT_SUCCESS);
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    DisableThreadLibraryCalls(hModule);

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        HANDLE handle = CreateThread(0, 0, ThreadProc, hModule, 0, 0);
        CloseHandle(handle);
    }

    return TRUE;
}