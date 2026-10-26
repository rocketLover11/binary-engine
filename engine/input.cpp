#include "input.h"
#include "../shared/convar.h"
#include "cvar_system.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>

InputButtons g_buttons;

InputSystem& Input()
{
	static InputSystem instance;
	return instance;
}

namespace
{
	
	struct KeyName
	{
		int vk;
		const char* name;
	};

	const KeyName kKeys[] =
	{
		{ VK_TAB, "tab" },{ VK_RETURN, "enter" },{ VK_SPACE, "space" },{ VK_BACK, "backspace" },
		{ VK_ESCAPE, "escape" },{ VK_CAPITAL, "capslock" },
		{ VK_SHIFT, "shift" },{ VK_CONTROL, "ctrl" },{ VK_MENU, "alt" },
		{ VK_UP, "uparrow" },{ VK_DOWN, "downarrow" },{ VK_LEFT, "leftarrow" },{ VK_RIGHT, "rightarrow" },
		{ VK_INSERT, "ins" },{ VK_DELETE, "del" },{ VK_HOME, "home" },{ VK_END, "end" },
		{ VK_PRIOR, "pgup" },{ VK_NEXT, "pgdn" },
		{ VK_LBUTTON, "mouse1" },{ VK_RBUTTON, "mouse2" },{ VK_MBUTTON, "mouse3" },
		{ VK_XBUTTON1, "mouse4" },{ VK_XBUTTON2, "mouse5" },
		{ kKeyWheelUp, "mwheelup" },{ kKeyWheelDown, "mwheeldown" },
		{ VK_OEM_MINUS, "-" },{ VK_OEM_PLUS, "=" },{ VK_OEM_4, "[" },{ VK_OEM_6, "]" },
		{ VK_OEM_1, "semicolon" },{ VK_OEM_7, "'" },{ VK_OEM_COMMA, "," },{ VK_OEM_PERIOD, "." },
		{ VK_OEM_2, "/" },{ VK_OEM_5, "\\" },{ VK_OEM_3, "`" },
	};

	const std::string kEmpty;

	bool AllDigits(const char* s)
	{
		if (!*s) return false;
		for (; *s; ++s) if (!std::isdigit(static_cast<unsigned char>(*s))) return false;
		return true;
	}

}

int InputSystem::KeyFromName(const char* name)
{
	if (!name || !*name) return -1;
	const std::string n = ToLowerAscii(name);

	if (n.size() == 1 && std::isalnum(static_cast<unsigned char>(n[0]))) return std::toupper(static_cast<unsigned char>(n[0]));
	if (n[0] == 'f' && AllDigits(n.c_str() + 1))
	{
		const int num = std::atoi(n.c_str() + 1);
		if (num >= 1 && num <= 12) return VK_F1 + num - 1;
	}
	if (n.rfind("numpad", 0) == 0 && n.size() == 7 && std::isdigit(static_cast<unsigned char>(n[6]))) return VK_NUMPAD0 + (n[6] - '0');
	for (const KeyName& k : kKeys) if (n == k.name) return k.vk;
	return -1;
}

std::string InputSystem::NameFromKey(int key)
{
	if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) return std::string(1, static_cast<char>(std::tolower(key)));
	if (key >= VK_F1 && key <= VK_F12) return "f" + std::to_string(key - VK_F1 + 1);
	if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) return "numpad" + std::to_string(key - VK_NUMPAD0);
	for (const KeyName& k : kKeys) if (k.vk == key) return k.name;
	char buf[24];
	std::snprintf(buf, sizeof buf, "key%d", key);
	return buf;
}

bool InputSystem::IsReserved(int key) { return key == VK_ESCAPE || key == VK_OEM_3; }

bool InputSystem::Bind(int key, const char* command)
{
	if (key < 0 || key >= kKeyCount || IsReserved(key)) return false;
	m_binds[key] = command ? command : "";
	return true;
}

void InputSystem::Unbind(int key)
{
	if (key >= 0 && key < kKeyCount) m_binds[key].clear();
}

void InputSystem::UnbindAll()
{
	for (std::string& b : m_binds) b.clear();
}

void InputSystem::UnbindCommand(const char* command)
{
	for (std::string& b : m_binds) if (b == command) b.clear();
}

const std::string& InputSystem::Binding(int key) const
{
	return (key >= 0 && key < kKeyCount) ? m_binds[key] : kEmpty;
}

int InputSystem::FindKeyFor(const char* command) const
{
	for (int k = 0; k < kKeyCount; ++k) if (m_binds[k] == command) return k;
	return -1;
}

std::vector<int> InputSystem::BoundKeys() const
{
	std::vector<int> keys;
	for (int k = 0; k < kKeyCount; ++k) if (!m_binds[k].empty()) keys.push_back(k);
	return keys;
}

void InputSystem::SetDefaults()
{
	UnbindAll();
	Bind('W', "+forward");
	Bind('S', "+back");
	Bind('A', "+moveleft");
	Bind('D', "+moveright");
	Bind('E', "+moveup");
	Bind('Q', "+movedown");
	Bind(VK_SHIFT, "+sprint");
}

void InputSystem::AppendBindConfig(std::string& out) const
{
	out += "unbindall\n";
	for (int k = 0; k < kKeyCount; ++k)
	{
		if (m_binds[k].empty()) continue;
		out += "bind " + NameFromKey(k) + " \"" + m_binds[k] + "\"\n";
	}
}

void InputSystem::OnKey(int key, bool down, bool blocked)
{
	if (key < 0 || key >= kKeyCount) return;

	if (down)
	{
		if (blocked || m_binds[key].empty()) return;
		const std::string cmd = m_binds[key];
		if (cmd[0] == '+')
		{
			size_t end = cmd.find_first_of(" ;\t");
			if (end == std::string::npos) end = cmd.size();
			m_heldRelease[key] = "-" + cmd.substr(1, end - 1);
		}
		Cvars().ExecuteNow(cmd.c_str());
	}
	else if (!m_heldRelease[key].empty())
	{
		const std::string rel = m_heldRelease[key];
		m_heldRelease[key].clear();
		Cvars().ExecuteNow(rel.c_str());
	}
}

void InputSystem::ReleaseAll()
{
	for (int k = 0; k < kKeyCount; ++k) if (!m_heldRelease[k].empty()) OnKey(k, false, false);
}

void InputSystem::StartCapture()
{
	m_capturing = true;
	m_captureDone = false;
	m_capturedKey = -1;
}

void InputSystem::CancelCapture()
{
	m_capturing = false;
	m_captureDone = false;
	m_capturedKey = -1;
}

void InputSystem::Capture(int key, bool down)
{
	if (!m_capturing || !down) return;
	if (key == VK_OEM_3 || key == VK_LBUTTON) return;
	m_capturing = false;
	m_captureDone = true;
	m_capturedKey = (key == VK_ESCAPE) ? -1 : key;
}

bool InputSystem::ConsumeCaptured(int& key)
{
	if (!m_captureDone) return false;
	key = m_capturedKey;
	m_captureDone = false;
	return true;
}

CON_COMMAND(bind, "bind <key> [command]: bind a key to a command (or show its binding)")
{
	if (args.Argc() < 2)
	{
		ConMsg("usage: bind <key> \"<command>\"\n");
		return;
	}

	const int key = InputSystem::KeyFromName(args.Argv(1));
	if (key < 0)
	{
		ConWarning("bind: unknown key \"%s\"\n", args.Argv(1));
		return;
	}

	if (args.Argc() == 2)
	{
		const std::string& b = Input().Binding(key);
		if (b.empty()) ConMsg("\"%s\" is not bound\n", args.Argv(1));
		else ConMsg("\"%s\" = \"%s\"\n", args.Argv(1), b.c_str());
		return;
	}

	std::string cmd;
	for (int i = 2; i < args.Argc(); ++i)
	{
		if (i > 2) cmd += ' ';
		cmd += args.Argv(i);
	}
	if (!Input().Bind(key, cmd.c_str())) ConWarning("bind: \"%s\" can't be bound\n", args.Argv(1));
}

CON_COMMAND(unbind, "unbind <key>: remove a key's binding")
{
	if (args.Argc() < 2)
	{
		ConMsg("usage: unbind <key>\n");
		return;
	}
	const int key = InputSystem::KeyFromName(args.Argv(1));
	if (key < 0)
	{
		ConWarning("unbind: unknown key \"%s\"\n", args.Argv(1));
		return;
	}
	Input().Unbind(key);
}

CON_COMMAND(unbindall, "unbindall: remove every binding") { Input().UnbindAll(); }

CON_COMMAND(bind_defaults, "bind_defaults: restore the default key bindings") { Input().SetDefaults(); }

CON_COMMAND(bindlist, "bindlist: list every binding")
{
	int count = 0;
	for (int k : Input().BoundKeys())
	{
		ConMsg("%-12s \"%s\"\n", InputSystem::NameFromKey(k).c_str(), Input().Binding(k).c_str());
		++count;
	}
	ConMsg("%d binding(s)\n", count);
}

#define BUTTON_COMMANDS(name, field) \
	static void name##_down(const CommandArgs&) { g_buttons.field = true; } \
	static void name##_up(const CommandArgs&) { g_buttons.field = false; } \
	static ConCommand name##_down_cmd("+" #name, &name##_down, "held while the key is down", FCVAR_HIDDEN); \
	static ConCommand name##_up_cmd("-" #name, &name##_up, "released when the key goes up", FCVAR_HIDDEN);

BUTTON_COMMANDS(forward, forward)
BUTTON_COMMANDS(back, back)
BUTTON_COMMANDS(moveleft, left)
BUTTON_COMMANDS(moveright, right)
BUTTON_COMMANDS(moveup, up)
BUTTON_COMMANDS(movedown, down)
BUTTON_COMMANDS(sprint, sprint)