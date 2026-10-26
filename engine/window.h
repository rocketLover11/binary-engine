#pragma once

#include <Windows.h>

#include <vector>

struct KeyEvent
{
	int key;
	bool down;
};

class Window
{
public:
	using MessageHook = LRESULT(*)(HWND, UINT, WPARAM, LPARAM);

	bool Create(const wchar_t* title, int w, int h);
	void Destroy();

	bool Pump();

	void SetMessageHook(MessageHook hook) { m_hook = hook; }

	HWND Handle() const { return m_hwnd; }
	int Width() const { return m_w; }
	int Height() const { return m_h; }
	bool Minimized() const { return m_minimized; }
	bool Focused() const { return m_focused; }

	bool ConsumeResize()
	{
		bool r = m_resized;
		m_resized = false;
		return r;
	}

	void SetFullscreen(bool on);
	bool IsFullscreen() const { return m_fullscreen; }
	void SetClientSize(int w, int h);

	bool KeyDown(int vk) const { return vk >= 0 && vk < 256 && m_keys[vk]; }
	
	void TakeKeyEvents(std::vector<KeyEvent>& out)
	{
		out.clear();
		out.swap(m_events);
	}

	void ConsumeMouseDelta(int& dx, int& dy)
	{
		dx = m_dx;
		dy = m_dy;
		m_dx = m_dy = 0;
	}

	void SetMouseLook(bool on);
	bool MouseLook() const { return m_look; }

private:
	static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
	LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);
	void PushKey(int key, bool down, bool repeat);

	HWND m_hwnd = nullptr;
	MessageHook m_hook = nullptr;
	int m_w = 0, m_h = 0;
	bool m_quit = false;
	bool m_resized = false;
	bool m_minimized = false;
	bool m_focused = false;
	bool m_look = false;
	bool m_fullscreen = false;
	WINDOWPLACEMENT m_placement{};
	bool m_keys[256] = {};
	std::vector<KeyEvent> m_events;
	int  m_dx = 0, m_dy = 0;
};