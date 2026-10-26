#include "cvar_system.h"

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iterator>
#include <system_error>

CvarSystem& Cvars()
{
	static CvarSystem instance;
	return instance;
}

void CvarSystem::RegisterCommand(ConCommandBase* cmd)
{
	std::lock_guard lock(m_mutex);
	auto result = m_commands.try_emplace(ToLowerAscii(cmd->GetName()), cmd);
	if (!result.second && result.first->second != cmd) Logf(ConLevel::Warning, "Console name \"%s\" is already registered; ignoring the duplicate", cmd->GetName());
}

void CvarSystem::UnregisterCommand(ConCommandBase* cmd)
{
	std::lock_guard lock(m_mutex);
	auto it = m_commands.find(ToLowerAscii(cmd->GetName()));
	if (it != m_commands.end() && it->second == cmd) m_commands.erase(it);
}

ConCommandBase* CvarSystem::FindCommand(const char* name)
{
	if (!name) return nullptr;
	std::lock_guard lock(m_mutex);
	auto it = m_commands.find(ToLowerAscii(name));
	return it == m_commands.end() ? nullptr : it->second;
}

std::vector<ConCommandBase*> CvarSystem::Sorted(bool includeHidden)
{
	std::lock_guard lock(m_mutex);
	std::vector<ConCommandBase*> v;
	v.reserve(m_commands.size());
	for (auto& kv : m_commands) if (includeHidden || !kv.second->HasFlag(FCVAR_HIDDEN)) v.push_back(kv.second);
	std::sort(v.begin(), v.end(), [](const ConCommandBase* a, const ConCommandBase* b)
	{
		return _stricmp(a->GetName(), b->GetName()) < 0;
	});
	return v;
}

void CvarSystem::CompleteName(const char* prefix, std::vector<std::string>& out)
{
	out.clear();
	const std::string p = ToLowerAscii(prefix);
	for (ConCommandBase* c : Sorted(false)) if (ToLowerAscii(c->GetName()).rfind(p, 0) == 0) out.emplace_back(c->GetName());
}

void CvarSystem::AddSink(IConsoleSink* sink)
{
	std::lock_guard lock(m_mutex);
	m_sinks.push_back(sink);
}

void CvarSystem::RemoveSink(IConsoleSink* sink)
{
	std::lock_guard lock(m_mutex);
	m_sinks.erase(std::remove(m_sinks.begin(), m_sinks.end(), sink), m_sinks.end());
}

void CvarSystem::Print(ConLevel level, const char* text)
{
	if (!text) return;
	std::lock_guard lock(m_mutex);
	const char* start = text;
	for (const char* p = text; ; ++p)
	{
		if (*p == '\n' || *p == '\0')
		{
			if (*p == '\n' || p != start) EmitLine(level, std::string(start, p));
			if (!*p) break;
			start = p + 1;
		}
	}
}

void CvarSystem::EmitLine(ConLevel level, std::string line)
{
	for (IConsoleSink* s : m_sinks) s->OnPrint(level, line.c_str());
	m_history.push_back({ level, std::move(line) });
	if (m_history.size() > kMaxHistory) m_history.pop_front();
}

void CvarSystem::Logf(ConLevel level, const char* fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	std::vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	Print(level, buf);
}

void CvarSystem::AddText(const char* text)
{
	if (!text) return;
	std::lock_guard lock(m_mutex);
	m_buffer += text;
	m_buffer += '\n';
}

void CvarSystem::ExecuteBuffer()
{
	for (int pass = 0; pass < 32; ++pass)
	{
		std::string text;
		{
			std::lock_guard lock(m_mutex);
			if (m_buffer.empty()) return;
			text.swap(m_buffer);
		}
		ExecuteNow(text.c_str());
	}
}

void CvarSystem::ExecuteNow(const char* text)
{
	if (!text) return;

	std::string cur;
	bool inQuote = false;
	const char* p = text;
	for (;;)
	{
		char c = *p;
		if (!inQuote && c == '/' && p[1] == '/')
		{
			while (*p && *p != '\n') ++p;
			c = *p;
		}
		if (c == '\r')
		{
			++p;
			continue;
		}
		if (c == '\0' || c == '\n' || (!inQuote && c == ';'))
		{
			RunCommandLine(cur);
			cur.clear();
			inQuote = false;
			if (c == '\0') break;
			++p;
			continue;
		}
		if (c == '"') inQuote = !inQuote;
		cur.push_back(c);
		++p;
	}
}

void CvarSystem::RunCommandLine(const std::string& line)
{
	std::lock_guard lock(m_mutex);

	std::string buf;
	std::vector<size_t> starts;
	size_t argStringPos = std::string::npos;
	const char* s = line.c_str();
	const char* p = s;
	while (*p)
	{
		while (*p == ' ' || *p == '\t') ++p;
		if (!*p) break;
		if (starts.size() == 1) argStringPos = static_cast<size_t>(p - s);
		starts.push_back(buf.size());
		if (*p == '"')
		{
			++p;
			while (*p && *p != '"') buf.push_back(*p++);
			if (*p == '"') ++p;
		}
		else
		{
			while (*p && *p != ' ' && *p != '\t') buf.push_back(*p++);
		}
		buf.push_back('\0');
	}
	if (starts.empty()) return;

	std::string argString;
	if (argStringPos != std::string::npos)
	{
		argString = line.substr(argStringPos);
		while (!argString.empty() && (argString.back() == ' ' || argString.back() == '\t')) argString.pop_back();
	}

	int n = static_cast<int>(starts.size());
	if (n > CommandArgs::kMaxArgs) n = CommandArgs::kMaxArgs;

	CommandArgs args;
	args.argc = n;
	for (int i = 0; i < n; ++i) args.argv[i] = buf.c_str() + starts[static_cast<size_t>(i)];
	args.argString = argString.c_str();

	ConCommandBase* base = FindCommand(args.argv[0]);
	if (!base)
	{
		Logf(ConLevel::Warning, "Unknown command \"%s\"", args.argv[0]);
		return;
	}

	if (base->IsCommand())
	{
		if (base->HasFlag(FCVAR_CHEAT) && !CheatsEnabled())
		{
			Logf(ConLevel::Warning, "\"%s\" is cheat-protected. Set sv_cheats 1 first.", base->GetName());
			return;
		}
		static_cast<ConCommand*>(base)->Execute(args);
		return;
	}

	auto* var = static_cast<ConVar*>(base);
	if (n == 1)
	{
		Logf(ConLevel::Msg, "\"%s\" = \"%s\" (default \"%s\")%s%s", var->GetName(), var->GetString(), var->GetDefault(), *var->GetHelp() ? " - " : "", var->GetHelp());
		return;
	}
	UserSet(var, args.argv[1]);
}

bool CvarSystem::CheatsEnabled()
{
	auto it = m_commands.find("sv_cheats");
	if (it == m_commands.end() || it->second->IsCommand()) return false;
	return static_cast<ConVar*>(it->second)->GetBool();
}

bool CvarSystem::UserSet(ConVar* var, const char* value)
{
	std::lock_guard lock(m_mutex);
	if (var->HasFlag(FCVAR_READONLY))
	{
		Logf(ConLevel::Warning, "\"%s\" is read-only.", var->GetName());
		return false;
	}
	if (var->HasFlag(FCVAR_CHEAT) && !CheatsEnabled())
	{
		Logf(ConLevel::Warning, "\"%s\" is cheat-protected. Set sv_cheats 1 first.", var->GetName());
		return false;
	}
	var->SetValue(value);
	return true;
}

void CvarSystem::SetBaseDir(const std::filesystem::path& dir) { m_cfgDir = dir / "cfg"; }

bool CvarSystem::ExecFile(const char* name, bool quietIfMissing)
{
	namespace fs = std::filesystem;
	fs::path rel = name ? name : "";
	if (!rel.has_extension()) rel += ".cfg";
	if (rel.is_absolute() || rel.string().find("..") != std::string::npos)
	{
		Logf(ConLevel::Warning, "exec: \"%s\" must be a plain file name inside the cfg folder", name);
		return false;
	}

	const fs::path full = m_cfgDir / rel;
	std::ifstream f(full, std::ios::binary);
	if (!f)
	{
		if (!quietIfMissing) Logf(ConLevel::Warning, "exec: couldn't open \"%s\"", full.string().c_str());
		return false;
	}
	if (m_execDepth >= 8)
	{
		Logf(ConLevel::Warning, "exec: nested too deeply (is a cfg file executing itself?)");
		return false;
	}

	std::string text{ std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
	if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
		static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
		text.erase(0, 3);

	++m_execDepth;
	ExecuteNow(text.c_str());
	--m_execDepth;
	return true;
}

bool CvarSystem::WriteConfig()
{
	std::error_code ec;
	std::filesystem::create_directories(m_cfgDir, ec);

	std::ofstream f(m_cfgDir / "config.cfg", std::ios::trunc);
	if (!f) return false;

	f << "// Written automatically on exit. Put hand-written settings in autoexec.cfg.\n";
	for (ConCommandBase* c : Sorted(true))
	{
		if (c->IsCommand() || !c->HasFlag(FCVAR_ARCHIVE)) continue;
		const auto* v = static_cast<const ConVar*>(c);
		f << v->GetName() << " \"" << v->GetString() << "\"\n";
	}

	if (m_configWriter)
	{
		std::string extra;
		m_configWriter(extra);
		f << extra;
	}

	return static_cast<bool>(f);
}