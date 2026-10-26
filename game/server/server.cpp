#include "../../shared/gamedll.h"
#include "../../shared/convar.h"

static unsigned g_tick = 0;

static void OnGravityChanged(ConVar* var, const char* oldValue)
{
    ConMsg("sv_gravity changed: %s -> %s\n", oldValue, var->GetString());
}

static ConVar sv_gravity("sv_gravity", "800", FCVAR_ARCHIVE | FCVAR_REPLICATED, "World gravity in units/s^2", 0.0f, 10000.0f, &OnGravityChanged);
static ConVar sv_report_interval("sv_report_interval", "0", FCVAR_NONE, "Print the tick number every N seconds (0 = off)", 0.0f, 3600.0f);

CON_COMMAND(sv_status, "sv_status: show the server tick counter")
{
    ConMsg("server tick = %u\n", g_tick);
}

class ServerGameDLL final : public IServerGameDLL
{
public:
    bool Init(CreateInterfaceFn engineFactory) override
    {
        if (!ConVar_Register(engineFactory)) return false;
        ConMsg("[server] Initialized\n");
        return true;
    }
    
    void LevelInit(const char* mapName) override { ConMsg("[server] Level requested: %s\n", mapName); }
    
    void GameFrame(bool) override
    {
        ++g_tick;
        const int interval = sv_report_interval.GetInt();
        if (interval > 0 && g_tick % (66u * static_cast<unsigned>(interval)) == 0)
            ConMsg("[server] tick %u\n", g_tick);
    }

    void Shutdown() override
    {
        ConMsg("[server] Shutting down...\n");
        ConVar_Unregister();
    }
};

EXPOSE_INTERFACE(ServerGameDLL, IServerGameDLL, SERVER_GAMEDLL_VERSION)