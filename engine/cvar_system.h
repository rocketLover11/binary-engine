#pragma once

#include "../shared/convar.h"

#include <cctype>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

inline std::string ToLowerAscii(const char* s)
{
	std::string r(s ? s : "");
	for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return r;
}

struct IConsoleSink
{
	virtual void OnPrint(ConLevel level, const char* line) = 0;
};

struct ConsoleLine
{
	ConLevel level;
	std::string text;
};

class CvarSystem final : public ICvar
{
public:
	CvarSystem() = default;
	CvarSystem(const CvarSystem&) = delete;
	CvarSystem& operator=(const CvarSystem&) = delete;

	void RegisterCommand(ConCommandBase* cmd) override;
	void UnregisterCommand(ConCommandBase* cmd) override;
	ConCommandBase* FindCommand(const char* name) override;
	void Print(ConLevel level, const char* text) override;
	void AddText(const char* text) override;
	void ExecuteNow(const char* text) override;

	void ExecuteBuffer();
	bool UserSet(ConVar* var, const char* value);
	std::vector<ConCommandBase*> Sorted(bool includeHidden);
	void CompleteName(const char* prefix, std::vector<std::string>& out);

	void AddSink(IConsoleSink* sink);
	void RemoveSink(IConsoleSink* sink);
	const std::deque<ConsoleLine>& History() const { return m_history; }

	void SetBaseDir(const std::filesystem::path& dir);
	bool ExecFile(const char* name, bool quietIfMissing = false);
	bool WriteConfig();

	using ConfigWriter = void (*)(std::string& out);
	void SetConfigWriter(ConfigWriter fn) { m_configWriter = fn; }

private:
	void RunCommandLine(const std::string& line);
	bool CheatsEnabled();
	void EmitLine(ConLevel level, std::string line);
	void Logf(ConLevel level, const char* fmt, ...);

	static constexpr size_t kMaxHistory = 4096;

	std::recursive_mutex m_mutex;
	std::unordered_map<std::string, ConCommandBase*> m_commands;
	std::vector<IConsoleSink*> m_sinks;
	std::deque<ConsoleLine> m_history;
	std::string m_buffer;
	std::filesystem::path m_cfgDir;
	int m_execDepth = 0;
	ConfigWriter m_configWriter = nullptr;
};

CvarSystem& Cvars();