#pragma once

#include <string>

class MainMenu
{
public:
	bool IsOpen() const { return m_open; }
	void Open();
	void Close();
	void HandleEscape();
	void Draw();

private:
	enum class Page { Main, Settings };

	void DrawMain();
	void DrawSettings();
	void DrawVideo();
	void DrawMouse();
	void DrawControls();

	bool m_open = false;
	Page m_page = Page::Main;
	std::string m_waitingAction;
};