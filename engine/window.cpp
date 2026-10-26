#include "window.h"
#include "keys.h"

static const wchar_t* kClassName = L"BinaryWindow";

bool Window::Create(const wchar_t* title, int w, int h)
{
	HINSTANCE inst = GetModuleHandleW(nullptr);

	HICON hAppIcon = LoadIconW(inst, MAKEINTRESOURCEW(101));

	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = &Window::WndProc;
	wc.hInstance = inst;
	wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wc.lpszClassName = kClassName;
	wc.hIcon = hAppIcon;
	wc.hIconSm = hAppIcon;
	RegisterClassExW(&wc);

	RECT r{ 0, 0, w, h };
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

	m_hwnd = CreateWindowExW(0, kClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, this);
	if (!m_hwnd) return false;

	RECT cr{};
	GetClientRect(m_hwnd, &cr);
	m_w = cr.right - cr.left;
	m_h = cr.bottom - cr.top;

	ShowWindow(m_hwnd, SW_SHOW);

	RAWINPUTDEVICE rid{};
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x02;
	rid.dwFlags = 0;
	rid.hwndTarget = m_hwnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));

	return true;
}

void Window::Destroy()
{
	SetMouseLook(false);
	if (m_hwnd)
	{
		DestroyWindow(m_hwnd);
		m_hwnd = nullptr;
	}
	UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
}

bool Window::Pump()
{
	MSG msg;
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT) m_quit = true;
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return !m_quit;
}

void Window::SetMouseLook(bool on)
{
	if (on == m_look) return;
	m_look = on;
	if (on)
	{
		RECT rc{};
		GetClientRect(m_hwnd, &rc);
		POINT c{ rc.right / 2, rc.bottom / 2 };
		ClientToScreen(m_hwnd, &c);
		SetCursorPos(c.x, c.y);
		const RECT pin{ c.x, c.y, c.x + 1, c.y + 1 };
		ClipCursor(&pin);
		ShowCursor(FALSE);
	}
	else
	{
		ClipCursor(nullptr);
		ShowCursor(TRUE);
	}
}

void Window::SetFullscreen(bool on)
{
	if (!m_hwnd || on == m_fullscreen) return;
	
	if (on)
	{
		m_placement.length = sizeof(m_placement);
		GetWindowPlacement(m_hwnd, &m_placement);

		MONITORINFO mi{};
		mi.cbSize = sizeof(mi);
		GetMonitorInfoW(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST), &mi);

		SetWindowLongPtrW(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowPos(m_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
	}
	else
	{
		SetWindowLongPtrW(m_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
		SetWindowPlacement(m_hwnd, &m_placement);
		SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
	m_fullscreen = on;
}

void Window::SetClientSize(int w, int h)
{
	if (!m_hwnd || m_fullscreen || w <= 0 || h <= 0) return;
	if (IsZoomed(m_hwnd)) ShowWindow(m_hwnd, SW_RESTORE);

	const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(m_hwnd, GWL_STYLE));
	RECT r{ 0, 0, w, h };
	AdjustWindowRectExForDpi(&r, style, FALSE, 0, GetDpiForWindow(m_hwnd));
	SetWindowPos(m_hwnd, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::PushKey(int key, bool down, bool repeat)
{
	if (key < 0 || key >= 256) return;
	if (down)
	{
		if (repeat) return;
		m_keys[key] = true;
	}
	else
	{
		m_keys[key] = false;
	}
	m_events.push_back({ key, down });
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	Window* self = nullptr;
	if (msg == WM_NCCREATE)
	{
		auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
		self = static_cast<Window*>(cs->lpCreateParams);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	}
	else
	{
		self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	}
	return self ? self->HandleMessage(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT Window::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (m_hook) if (const LRESULT handled = m_hook(hwnd, msg, wp, lp)) return handled;

	switch (msg)
	{
	case WM_SIZE:
		m_minimized = (wp == SIZE_MINIMIZED);
		if (!m_minimized)
		{
			m_w = LOWORD(lp);
			m_h = HIWORD(lp);
			m_resized = true;
		}
		return 0;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (msg == WM_SYSKEYDOWN && (wp == VK_F4 || wp == VK_SPACE)) break;
		PushKey(static_cast<int>(wp), true, (lp & (1 << 30)) != 0);
		return 0;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		PushKey(static_cast<int>(wp), false, false);
		return 0;
	case WM_SYSCHAR:
		if (wp == ' ') break;
		return 0;
	case WM_LBUTTONDOWN: PushKey(VK_LBUTTON, true, false);  return 0;
	case WM_LBUTTONUP:   PushKey(VK_LBUTTON, false, false); return 0;
	case WM_RBUTTONDOWN: PushKey(VK_RBUTTON, true, false);  return 0;
	case WM_RBUTTONUP:   PushKey(VK_RBUTTON, false, false); return 0;
	case WM_MBUTTONDOWN: PushKey(VK_MBUTTON, true, false);  return 0;
	case WM_MBUTTONUP:   PushKey(VK_MBUTTON, false, false); return 0;
	case WM_XBUTTONDOWN:
		PushKey(HIWORD(wp) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2, true, false);
		return TRUE;
	case WM_XBUTTONUP:
		PushKey(HIWORD(wp) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2, false, false);
		return TRUE;
	case WM_MOUSEWHEEL:
	{
		const int delta = GET_WHEEL_DELTA_WPARAM(wp);
		if (delta != 0)
		{
			const int key = delta > 0 ? kKeyWheelUp : kKeyWheelDown;
			m_events.push_back({ key, true });
			m_events.push_back({ key, false });
		}
		return 0;
	}
	case WM_SETFOCUS:
		m_focused = true;
		return 0;
	case WM_KILLFOCUS:
		m_focused = false;
		for (bool& k : m_keys) k = false;
		SetMouseLook(false);
		return 0;
	case WM_INPUT:
	{
		RAWINPUT ri{};
		UINT size = sizeof(ri);
		if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lp), RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1) && ri.header.dwType == RIM_TYPEMOUSE && !(ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
		{
			m_dx += ri.data.mouse.lLastX;
			m_dy += ri.data.mouse.lLastY;
		}
		return DefWindowProcW(hwnd, msg, wp, lp);
	}
	case WM_CLOSE:
		m_quit = true;
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wp, lp);
}