#include "../../shared/gamedll.h"
#include "../../shared/convar.h"

static ConVar cl_interp("cl_interp", "0.1", FCVAR_ARCHIVE, "Entity interpolation delay in seconds", 0.0f, 0.5f);
static ConVar cl_showfps("cl_showfps", "0", FCVAR_NONE, "Print the frame rate once per second");
static ConVar cl_crosshair("cl_crosshair", "1", FCVAR_ARCHIVE, "Draw a crosshair", 0.0f, 1.0f);
static ConVar cl_crosshair_size("cl_crosshair_size", "8", FCVAR_ARCHIVE, "Crosshair arm length in pixels", 2.0f, 64.0f);
static ConVar cl_debugui("cl_debugui", "0", FCVAR_NONE, "Show the client debug window", 0.0f, 1.0f);

CON_COMMAND(cl_toggledebugui, "cl_toggledebugui: toggle the client debug window")
{
    cl_debugui.SetInt(cl_debugui.GetBool() ? 0 : 1);
}

class ClientGameDLL final : public IClientGameDLL
{
    float m_accum = 0.0f;
    int m_frames = 0;
    float m_frameMs = 0.0f;

public:
    bool Init(CreateInterfaceFn engineFactory) override
    {
        if (!ConVar_Register(engineFactory)) return false;
        ConMsg("[client] Initialized\n");
        return true;
    }

    void Frame(float frameTime, float) override
    {
        ++m_frames;
        m_accum += frameTime;
        if (m_accum >= 1.0f)
        {
            if (cl_showfps.GetBool()) ConMsg("[client] %d fps\n", m_frames);
            m_accum = 0.0f;
            m_frames = 0;
        }
    }

    void DrawGui(IGui* gui) override
    {
        float w = 0.0f, h = 0.0f;
        gui->GetDisplaySize(&w, &h);

        if (cl_crosshair.GetBool() && !gui->CursorFree())
        {
            const float cx = w * 0.5f, cy = h * 0.5f;
            const float s = cl_crosshair_size.GetFloat(), gap = 3.0f;
            const unsigned col = GuiRGBA(255, 255, 255, 210);
            gui->HudRect(cx - gap - s, cy - 1.0f, cx - gap, cy + 1.0f, col);
            gui->HudRect(cx + gap, cy - 1.0f, cx + gap + s, cy + 1.0f, col);
            gui->HudRect(cx - 1.0f, cy - gap - s, cx + 1.0f, cy - gap, col);
            gui->HudRect(cx - 1.0f, cy + gap, cx + 1.0f, cy + gap + s, col);
        }

        if (cl_debugui.GetBool())
        {
            bool open = true;
            gui->SetNextWindowPos(20.0f, 20.0f, 0.0f, 0.0f, false);
            if (gui->Begin("Client debug", &open, GUIWIN_AUTO_RESIZE))
            {
                GuiTextf(gui, "frame time: %.2f ms", static_cast<double>(m_frameMs));
                gui->Separator();
                gui->CvarSlider("Interp (s)", "cl_interp", 0.0f, 0.5f);
                gui->CvarCheckbox("Crosshair", "cl_crosshair");
                gui->CvarSlider("Crosshair size", "cl_crosshair_size", 2.0f, 64.0f);
                if (gui->Button("Reset interp")) g_pCvar->AddText("reset cl_interp");
            }
            gui->End();
            if (!open) cl_debugui.SetInt(0);
            gui->RequestCursor();
        }
    }

    void Shutdown() override
    {
        ConMsg("[client] Shutting down...\n");
        ConVar_Unregister();
    }
};

EXPOSE_INTERFACE(ClientGameDLL, IClientGameDLL, CLIENT_GAMEDLL_VERSION)