#pragma once

#include "cvar_system.h"

#include <Windows.h>

#include <string>
#include <vector>

class StdConsole final : public IConsoleSink
{
public:
	~StdConsole();

	bool Init(CvarSystem* cvars);
	void Shutdown();
	void Poll();

	void OnPrint(ConLevel level, const char* line) override;

private:
	void Write(const std::wstring& s);
	void Redraw();
	void Submit();
	void HandleTab();
	void History(int dir);

	CvarSystem* m_cvars = nullptr;
	HANDLE m_in = nullptr;
	HANDLE m_out = nullptr;
	DWORD m_oldInMode = 0;
	DWORD m_oldOutMode = 0;
	bool m_enabled = false;
	bool m_vt = false;
	bool m_promptVisible = false;

	std::wstring m_line;
	size_t m_drawnLen = 0;
	std::vector<std::wstring> m_history;
	int m_histPos = 0;
};