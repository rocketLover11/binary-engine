#pragma once

#include "../third_party/imgui-1.91.9/imgui.h"

#include <Windows.h>
#include <d3d11.h>

#include <string>
#include <vector>

struct GuiStats
{
	float fps = 0.0f;
	float frameMs = 0.0f;
	float camPos[3] = {};
	float yawDeg = 0.0f;
	float pitchDeg = 0.0f;
};

class Gui
{
public:
	bool Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
	void Shutdown();

	static LRESULT MessageHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

	void NewFrame();
	void DrawPanels(const GuiStats& stats);
	void Render();

	void ToggleConsole();
	void CloseConsole() { m_consoleOpen = false; }
	bool ConsoleOpen() const { return m_consoleOpen; }

	bool WantsKeyboard() const;
	bool WantsMouse() const;
	void SetMouseEnabled(bool enabled);

	void DrawConsole();
	void DrawStats(const GuiStats& stats);

private:
	void Submit();
	static int InputCallback(ImGuiInputTextCallbackData* data);

	bool m_initialized = false;
	bool m_consoleOpen = false;
	bool m_focusInput = false;
	bool m_scrollToBottom = false;
	char m_input[256] = {};
	std::vector<std::string> m_history;
	int m_histPos = -1;
};