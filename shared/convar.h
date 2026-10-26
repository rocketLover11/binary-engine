#pragma once

#include "interface.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

constexpr unsigned FCVAR_NONE = 0;
constexpr unsigned FCVAR_ARCHIVE = 1u << 0;
constexpr unsigned FCVAR_CHEAT = 1u << 1;
constexpr unsigned FCVAR_HIDDEN = 1u << 2;
constexpr unsigned FCVAR_REPLICATED = 1u << 3;
constexpr unsigned FCVAR_READONLY = 1u << 4;

enum class ConLevel : int { Msg, Warning, Error };

struct CommandArgs
{
	static constexpr int kMaxArgs = 64;

	int argc = 0;
	const char* argv[kMaxArgs] = {};
	const char* argString = "";

	int Argc() const { return argc; }
	const char* Argv(int i) const { return (i >= 0 && i < argc) ? argv[i] : ""; }
	const char* ArgS() const { return argString; }
};

class ConCommandBase;

inline ConCommandBase* g_conModuleList = nullptr;

class ConCommandBase
{
public:
	virtual bool IsCommand() const = 0;

	const char* GetName() const { return m_name; }
	const char* GetHelp() const { return m_help; }
	unsigned GetFlags() const { return m_flags; }
	bool HasFlag(unsigned f) const { return (m_flags & f) != 0; }

	ConCommandBase* NextInModule() const { return m_nextInModule; }

protected:
	ConCommandBase(const char* name, const char* help, unsigned flags) : m_name(name), m_help(help ? help : ""), m_flags(flags)
	{
		m_nextInModule = g_conModuleList;
		g_conModuleList = this;
	}
	~ConCommandBase() = default;

private:
	const char* m_name;
	const char* m_help;
	unsigned m_flags;
	ConCommandBase* m_nextInModule = nullptr;
};

class ConVar final : public ConCommandBase
{
public:
	using ChangeFn = void (*)(ConVar* var, const char* oldValue);
	static constexpr int kMaxString = 256;

	ConVar(const char* name, const char* defaultValue, unsigned flags = FCVAR_NONE, const char* help = "", ChangeFn onChange = nullptr) : ConCommandBase(name, help, flags), m_default(defaultValue ? defaultValue : ""), m_onChange(onChange)
	{
		Assign(m_default, false);
	}

	ConVar(const char* name, const char* defaultValue, unsigned flags, const char* help, float minValue, float maxValue, ChangeFn onChange = nullptr) : ConCommandBase(name, help, flags), m_default(defaultValue ? defaultValue : ""), m_onChange(onChange), m_hasMin(true), m_hasMax(true), m_min(minValue), m_max(maxValue)
	{
		Assign(m_default, false);
	}

	bool IsCommand() const override { return false; }

	const char* GetString() const { return m_string; }
	float GetFloat() const { return m_float; }
	int GetInt() const { return m_int; }
	bool GetBool() const { return m_int != 0; }
	const char* GetDefault() const { return m_default; }

	void SetValue(const char* value) { Assign(value, true); }
	void SetFloat(float v)
	{
		char b[48];
		std::snprintf(b, sizeof b, "%g", static_cast<double>(v));
		Assign(b, true);
	}
	void SetInt(int v)
	{
		char b[32];
		std::snprintf(b, sizeof b, "%d", v);
		Assign(b, true);
	}
	void Reset() { Assign(m_default, true); }

private:
	void Assign(const char* value, bool notify)
	{
		char tmp[kMaxString];
		std::snprintf(tmp, sizeof tmp, "%s", value ? value : "");

		float f = static_cast<float>(std::atof(tmp));
		if ((m_hasMin && f < m_min) || (m_hasMax && f > m_max))
		{
			if (m_hasMin && f < m_min) f = m_min;
			if (m_hasMax && f > m_max) f = m_max;
			std::snprintf(tmp, sizeof tmp, "%g", static_cast<double>(f));
		}

		char old[kMaxString];
		std::snprintf(old, sizeof old, "%s", m_string);

		std::snprintf(m_string, sizeof m_string, "%s", tmp);
		m_float = f;
		m_int = static_cast<int>(f);

		if (notify && m_onChange && std::strcmp(old, m_string) != 0) m_onChange(this, old);
	}

	const char* m_default;
	ChangeFn m_onChange;
	bool m_hasMin = false;
	bool m_hasMax = false;
	float m_min = 0.0f;
	float m_max = 0.0f;
	float m_float = 0.0f;
	int m_int = 0;
	char m_string[kMaxString] = {};
};

class ConCommand final : public ConCommandBase
{
public:
	using Fn = void (*)(const CommandArgs& args);

	ConCommand(const char* name, Fn fn, const char* help = "", unsigned flags = FCVAR_NONE) : ConCommandBase(name, help, flags), m_fn(fn) {}

	bool IsCommand() const override { return true; }
	void Execute(const CommandArgs& args) const { if (m_fn) m_fn(args); }

private:
	Fn m_fn;
};

struct ICvar
{
	virtual void RegisterCommand(ConCommandBase* cmd) = 0;
	virtual void UnregisterCommand(ConCommandBase* cmd) = 0;
	virtual ConCommandBase* FindCommand(const char* name) = 0;

	virtual void Print(ConLevel level, const char* text) = 0;

	virtual void AddText(const char* text) = 0;
	virtual void ExecuteNow(const char* text) = 0;
};

#define CVAR_INTERFACE_VERSION "Cvar001"

inline ICvar* g_pCvar = nullptr;

inline bool ConVar_Register(CreateInterfaceFn factory)
{
	g_pCvar = static_cast<ICvar*>(factory(CVAR_INTERFACE_VERSION, nullptr));
	if (!g_pCvar) return false;
	for (ConCommandBase* c = g_conModuleList; c; c = c->NextInModule()) g_pCvar->RegisterCommand(c);
	return true;
}

inline void ConVar_Unregister()
{
	if (!g_pCvar) return;
	for (ConCommandBase* c = g_conModuleList; c; c = c->NextInModule()) g_pCvar->UnregisterCommand(c);
	g_pCvar = nullptr;
}

inline ConVar* ConVar_Find(const char* name)
{
	if (!g_pCvar) return nullptr;
	ConCommandBase* b = g_pCvar->FindCommand(name);
	return (b && !b->IsCommand()) ? static_cast<ConVar*>(b) : nullptr;
}

inline void ConPrintV(ConLevel level, const char* fmt, va_list ap)
{
	char buf[2048];
	std::vsnprintf(buf, sizeof buf, fmt, ap);
	if (g_pCvar) g_pCvar->Print(level, buf);
	else std::fputs(buf, stdout);
}
inline void ConMsg(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ConPrintV(ConLevel::Msg, fmt, ap);
	va_end(ap);
}
inline void ConWarning(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ConPrintV(ConLevel::Warning, fmt, ap);
	va_end(ap);
}
inline void ConError(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ConPrintV(ConLevel::Error, fmt, ap);
	va_end(ap);
}

#define CON_COMMAND_F(name, desc, flags) \
	static void name##_callback(const CommandArgs& args); \
	static ConCommand name##_command(#name, &name##_callback, desc, flags); \
	static void name##_callback([[maybe_unused]] const CommandArgs& args)

#define CON_COMMAND(name, desc) CON_COMMAND_F(name, desc, FCVAR_NONE)