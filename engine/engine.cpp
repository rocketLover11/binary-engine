#include "../shared/interface.h"
#include "../shared/eiface.h"
#include "../shared/gamedll.h"
#include "../shared/convar.h"
#include "window.h"
#include "d3d11_device.h"
#include "camera.h"
#include "renderer.h"
#include "gui.h"
#include "gui_api.h"
#include "main_menu.h"
#include "input.h"
#include "cvar_system.h"
#include "std_console.h"

#include <Windows.h>
#include <timeapi.h>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

    std::filesystem::path ModuleDir()
    {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&ModuleDir), &self);
        wchar_t buf[MAX_PATH * 2] = {};
        GetModuleFileNameW(self, buf, _countof(buf));
        return std::filesystem::path(buf).parent_path();
    }

    template <class T>
    T* LoadGameModule(const char* dll, const char* version, HMODULE& outModule)
    {
        outModule = LoadLibraryA(dll);
        if (!outModule)
        {
            ConError("Could not load %s (error %lu)\n", dll, GetLastError());
            return nullptr;
        }
        auto fn = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(outModule, "CreateInterface"));
        if (!fn)
        {
            ConError("%s has no CreateInterface export\n", dll);
            return nullptr;
        }
        int rc = 0;
        void* p = fn(version, &rc);
        if (!p || rc != 0)
        {
            ConError("%s does not provide %s\n", dll, version);
            return nullptr;
        }
        return static_cast<T*>(p);
    }

    void WriteBinds(std::string& out) { Input().AppendBindConfig(out); }

    constexpr float kMouseYaw = 0.022f;

}

static bool g_quitRequested = false;
static bool g_applyVideo = false;

CON_COMMAND(quit, "quit: exit the engine") { g_quitRequested = true; }
CON_COMMAND(vid_apply, "vid_apply: apply fullscreen and windowed size") { g_applyVideo = true; }

static ConVar vid_vsync("vid_vsync", "1", FCVAR_ARCHIVE, "Enable vertical sync", 0.0f, 1.0f);
static ConVar vid_fullscreen("vid_fullscreen", "0", FCVAR_ARCHIVE, "Borderless fullscreen (takes effect on vid_apply)", 0.0f, 1.0f);
static ConVar vid_width("vid_width", "1280", FCVAR_ARCHIVE, "Windowed client width (vid_apply)", 640.0f, 7680.0f);
static ConVar vid_height("vid_height", "720", FCVAR_ARCHIVE, "Windowed client height (vid_apply)", 480.0f, 4320.0f);
static ConVar host_timescale("host_timescale", "1", FCVAR_CHEAT, "Scale simulation speed", 0.01f, 10.0f);
static ConVar r_fov("r_fov", "90", FCVAR_ARCHIVE, "Horizontal field of view in degrees", 60.0f, 120.0f);
static ConVar sensitivity("sensitivity", "3", FCVAR_ARCHIVE, "Mouse sensitivity", 0.1f, 20.0f);
static ConVar cam_speed("cam_speed", "300", FCVAR_ARCHIVE, "Free camera speed in units per second", 10.0f, 5000.0f);

class Engine final : public IEngine
{
public:
    int Run() override;
};

BE_EXPORT void* CreateInterface(const char* name, int* returnCode)
{
    static Engine engine;
    if (name)
    {
        void* result = nullptr;
        if (std::strcmp(name, ENGINE_VERSION) == 0) result = static_cast<IEngine*>(&engine);
        else if (std::strcmp(name, CVAR_INTERFACE_VERSION) == 0) result = static_cast<ICvar*>(&Cvars());
        if (result)
        {
            if (returnCode) *returnCode = 0;
            return result;
        }
    }
    if (returnCode) *returnCode = 1;
    return nullptr;
}

int Engine::Run()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    timeBeginPeriod(1);

    Cvars().SetBaseDir(ModuleDir());
    Cvars().SetConfigWriter(&WriteBinds);
    StdConsole console;
    console.Init(&Cvars());
    ConVar_Register(&CreateInterface);
    ConMsg("Binary engine starting\n");

    Window window;
    if (!window.Create(L"Binary v0.0.3", 1280, 720))
    {
        ConError("Window creation failed\n");
        return 1;
    }

#ifdef _DEBUG
    const bool debugLayer = true;
#else
    const bool debugLayer = false;
#endif

    D3D11Device device;
    if (!device.Init(window.Handle(), window.Width(), window.Height(), debugLayer)) return 1;

    Renderer renderer;
    if (!renderer.Init(device, ModuleDir() / "shaders")) return 1;

    Gui gui;
    if (!gui.Init(window.Handle(), device.Device(), device.Context()))
    {
        ConError("GUI initialization failed\n");
        return 1;
    }
    window.SetMessageHook(&Gui::MessageHook);

    GuiApi guiApi;
    MainMenu menu;

    HMODULE serverMod = nullptr, clientMod = nullptr;
    auto* server = LoadGameModule<IServerGameDLL>("server.dll", SERVER_GAMEDLL_VERSION, serverMod);
    auto* client = LoadGameModule<IClientGameDLL>("client.dll", CLIENT_GAMEDLL_VERSION, clientMod);
    if (!server || !client) return 1;

    if (!server->Init(&CreateInterface) || !client->Init(&CreateInterface)) return 1;
    server->LevelInit("test");

    Input().SetDefaults();
    Cvars().ExecFile("config", true);
    Cvars().ExecFile("autoexec", true);
    g_applyVideo = true;

    constexpr double tickInterval = 1.0 / 66.0;
    LARGE_INTEGER freq, prev;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    double accumulator = 0.0;
    double totalTime = 0.0;
    float fpsSmooth = 60.0f;
    Camera camera;
    std::vector<KeyEvent> events;

    while (window.Pump() && !g_quitRequested)
    {
        console.Poll();
        Cvars().ExecuteBuffer();
        if (g_quitRequested) break;

        if (g_applyVideo)
        {
            g_applyVideo = false;
            window.SetFullscreen(vid_fullscreen.GetBool());
            if (!window.IsFullscreen()) window.SetClientSize(vid_width.GetInt(), vid_height.GetInt());
            window.SetMouseLook(false);
        }

        if (window.ConsumeResize()) device.Resize(window.Width(), window.Height());

        if (window.Minimized())
        {
            Sleep(50);
            QueryPerformanceCounter(&prev);
            continue;
        }

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double realDt = static_cast<double>(now.QuadPart - prev.QuadPart) / static_cast<double>(freq.QuadPart);
        prev = now;
        if (realDt > 0.25) realDt = 0.25;
        const double simDt = realDt * static_cast<double>(host_timescale.GetFloat());
        totalTime += simDt;
        accumulator += simDt;
        if (realDt > 0.0) fpsSmooth += (static_cast<float>(1.0 / realDt) - fpsSmooth) * 0.05f;

        gui.NewFrame();

        const bool cursorWanted = guiApi.TakeCursorRequest();
        auto uiBlocksKeys = [&]() { return gui.ConsoleOpen() || menu.IsOpen() || gui.WantsKeyboard(); };

        window.TakeKeyEvents(events);
        for (const KeyEvent& e : events)
        {
            if (Input().Capturing())
            {
                Input().Capture(e.key, e.down);
                continue;
            }

            if (e.down && e.key == VK_ESCAPE)
            {
                if (gui.ConsoleOpen()) gui.CloseConsole();
                else menu.HandleEscape();
                continue;
            }
            if (e.down && e.key == VK_OEM_3)
            {
                gui.ToggleConsole();
                continue;
            }

            Input().OnKey(e.key, e.down, uiBlocksKeys());
        }
        if (uiBlocksKeys() || !window.Focused()) Input().ReleaseAll();   // nothing stays stuck

        int mouseDx = 0, mouseDy = 0;
        window.ConsumeMouseDelta(mouseDx, mouseDy);

        const bool uiWantsCursor = gui.ConsoleOpen() || menu.IsOpen() || cursorWanted;
        const bool wantLook = window.Focused() && !uiWantsCursor;
        window.SetMouseLook(wantLook);
        gui.SetMouseEnabled(!wantLook);
        guiApi.SetCursorFree(!wantLook);

        camera.fovDegrees = r_fov.GetFloat();
        if (window.MouseLook()) camera.Look(static_cast<float>(mouseDx), static_cast<float>(mouseDy), sensitivity.GetFloat() * kMouseYaw);

        {
            const float step = cam_speed.GetFloat() * (g_buttons.sprint ? 3.0f : 1.0f) * static_cast<float>(realDt);
            const float f = (g_buttons.forward ? 1.0f : 0.0f) - (g_buttons.back ? 1.0f : 0.0f);
            const float r = (g_buttons.right ? 1.0f : 0.0f) - (g_buttons.left ? 1.0f : 0.0f);
            const float u = (g_buttons.up ? 1.0f : 0.0f) - (g_buttons.down ? 1.0f : 0.0f);
            camera.Move(f * step, r * step, u * step);
        }

        while (accumulator >= tickInterval)
        {
            server->GameFrame(true);
            accumulator -= tickInterval;
        }
        const float alpha = static_cast<float>(accumulator / tickInterval);
        client->Frame(static_cast<float>(simDt), alpha);

        const float clear[4] = { 0.10f, 0.12f, 0.18f, 1.0f };
        device.BeginFrame(clear);
        renderer.RenderScene(camera, static_cast<float>(totalTime));

        GuiStats stats;
        stats.fps = fpsSmooth;
        stats.frameMs = static_cast<float>(realDt * 1000.0);
        stats.camPos[0] = camera.position.x;
        stats.camPos[1] = camera.position.y;
        stats.camPos[2] = camera.position.z;
        stats.yawDeg = std::fmod(DirectX::XMConvertToDegrees(camera.yaw), 360.0f);
        stats.pitchDeg = DirectX::XMConvertToDegrees(camera.pitch);

        gui.DrawStats(stats);
        client->DrawGui(&guiApi);
        menu.Draw();
        gui.DrawConsole();
        gui.Render();

        device.EndFrame(vid_vsync.GetBool());
    }

    window.SetMessageHook(nullptr);
    Cvars().WriteConfig();
    client->Shutdown();
    server->Shutdown();
    ConVar_Unregister();
    gui.Shutdown();
    renderer.Shutdown();
    device.Shutdown();
    window.Destroy();
    FreeLibrary(clientMod);
    FreeLibrary(serverMod);
    timeEndPeriod(1);
    return 0;
}