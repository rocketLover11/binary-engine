#include <string>

#include "../shared/convar.h"
#include "cvar_system.h"

static ConVar sv_cheats("sv_cheats", "0", FCVAR_NONE,
    "Allow commands and variables flagged as cheats");

static std::string FlagText(unsigned f)
{
    std::string s;
    if (f & FCVAR_ARCHIVE)    s += " archive";
    if (f & FCVAR_CHEAT)      s += " cheat";
    if (f & FCVAR_REPLICATED) s += " replicated";
    if (f & FCVAR_READONLY)   s += " readonly";
    return s.empty() ? s : " [" + s.substr(1) + "]";
}

static void PrintEntry(const ConCommandBase* c)
{
    const char* sep = *c->GetHelp() ? " - " : "";
    if (c->IsCommand()) {
        ConMsg("%-26s (command)%s%s%s\n", c->GetName(), FlagText(c->GetFlags()).c_str(), sep, c->GetHelp());
        return;
    }
    const auto* v = static_cast<const ConVar*>(c);
    ConMsg("%-26s = \"%s\"%s%s%s\n", v->GetName(), v->GetString(), FlagText(v->GetFlags()).c_str(), sep, v->GetHelp());
}

CON_COMMAND(echo, "echo <text>: print text to the console")
{
    std::string s;
    for (int i = 1; i < args.Argc(); ++i) {
        if (i > 1) s += ' ';
        s += args.Argv(i);
    }
    ConMsg("%s\n", s.c_str());
}

CON_COMMAND(help, "help [name]: show help for a command or variable")
{
    if (args.Argc() < 2) {
        ConMsg("Type a variable's name alone to see its value; add a value to set it.\n"
            "  cvarlist [prefix]   list commands and variables\n"
            "  find <text>         search names and descriptions\n"
            "  help <name>         describe one entry\n"
            "Tab completes command names, Up/Down browse history.\n");
        return;
    }
    ConCommandBase* c = g_pCvar->FindCommand(args.Argv(1));
    if (!c) { ConWarning("No such command or variable: \"%s\"\n", args.Argv(1)); return; }
    PrintEntry(c);
}

CON_COMMAND(cvarlist, "cvarlist [prefix]: list every command and variable")
{
    const std::string prefix = args.Argc() > 1 ? ToLowerAscii(args.Argv(1)) : std::string();
    int count = 0;
    for (ConCommandBase* c : Cvars().Sorted(false)) {
        if (ToLowerAscii(c->GetName()).rfind(prefix, 0) != 0) continue;
        PrintEntry(c);
        ++count;
    }
    ConMsg("%d entries\n", count);
}

CON_COMMAND(find, "find <text>: search names and descriptions")
{
    if (args.Argc() < 2) { ConMsg("usage: find <text>\n"); return; }
    const std::string needle = ToLowerAscii(args.Argv(1));
    int count = 0;
    for (ConCommandBase* c : Cvars().Sorted(false)) {
        if (ToLowerAscii(c->GetName()).find(needle) == std::string::npos &&
            ToLowerAscii(c->GetHelp()).find(needle) == std::string::npos)
            continue;
        PrintEntry(c);
        ++count;
    }
    ConMsg("%d match(es)\n", count);
}

CON_COMMAND(exec, "exec <file>: run a config file from the cfg folder")
{
    if (args.Argc() < 2) { ConMsg("usage: exec <file>\n"); return; }
    Cvars().ExecFile(args.Argv(1));
}

CON_COMMAND(toggle, "toggle <variable>: flip a 0/1 variable")
{
    if (args.Argc() < 2) { ConMsg("usage: toggle <variable>\n"); return; }
    ConVar* v = ConVar_Find(args.Argv(1));
    if (!v) { ConWarning("No such variable: \"%s\"\n", args.Argv(1)); return; }
    Cvars().UserSet(v, v->GetBool() ? "0" : "1");
}

CON_COMMAND(reset, "reset <variable>: restore a variable's default value")
{
    if (args.Argc() < 2)
    {
        ConMsg("usage: reset <variable>\n");
        return;
    }
    ConVar* v = ConVar_Find(args.Argv(1));
    if (!v)
    {
        ConWarning("No such variable: \"%s\"\n", args.Argv(1));
        return;
    }
    Cvars().UserSet(v, v->GetDefault());
}

CON_COMMAND(host_writeconfig, "host_writeconfig: save archived variables to cfg/config.cfg")
{
    if (Cvars().WriteConfig()) ConMsg("Saved cfg/config.cfg\n");
    else ConWarning("Could not write cfg/config.cfg\n");
}