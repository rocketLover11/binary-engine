#pragma once

#include <cstddef>
#include <cstdarg>
#include <cstdio>

enum GuiWindowFlags : unsigned
{
	GUIWIN_NONE = 0,
	GUIWIN_NO_TITLEBAR = 1u << 0,
	GUIWIN_NO_RESIZE = 1u << 1,
	GUIWIN_NO_MOVE = 1u << 2,
	GUIWIN_AUTO_RESIZE = 1u << 3,
	GUIWIN_NO_BACKGROUND = 1u << 4,
	GUIWIN_NO_INPUTS = 1u << 5,
	GUIWIN_NO_COLLAPSE = 1u << 6,
};

constexpr unsigned GuiRGBA(unsigned r, unsigned g, unsigned b, unsigned a = 255)
{
	return (r << 24) | (g << 16) | (b << 8) | a;
}

struct IGui
{
	virtual bool Begin(const char* title, bool* open, unsigned flags) = 0;
	virtual void End() = 0;
	virtual void SetNextWindowPos(float x, float y, float pivotX, float pivotY, bool always) = 0;
	virtual void SetNextWindowSize(float w, float h, bool always) = 0;
	virtual void SetNextWindowBgAlpha(float alpha) = 0;
	virtual void GetDisplaySize(float* w, float* h) = 0;

	virtual void Text(const char* text) = 0;
	virtual void TextColored(unsigned rgba, const char* text) = 0;
	virtual void Separator() = 0;
	virtual void Spacing() = 0;
	virtual void SameLine() = 0;

	virtual bool Button(const char* label) = 0;
	virtual bool Checkbox(const char* label, bool* value) = 0;
	virtual bool SliderFloat(const char* label, float* value, float lo, float hi) = 0;
	virtual bool SliderInt(const char* label, int* value, int lo, int hi) = 0;
	virtual bool InputText(const char* label, char* buffer, size_t bufferSize) = 0;
	virtual void ProgressBar(float fraction, float width, float height, const char* overlay) = 0;
	virtual bool CollapsingHeader(const char* label) = 0;
	virtual bool BeginTabBar(const char* id) = 0;
	virtual void EndTabBar() = 0;
	virtual bool BeginTabItem(const char* label) = 0;
	virtual void EndTabItem() = 0;

	virtual bool CvarCheckbox(const char* label, const char* cvarName) = 0;
	virtual bool CvarSlider(const char* label, const char* cvarName, float lo, float hi) = 0;

	virtual void HudRect(float x0, float y0, float x1, float y1, unsigned rgba) = 0;
	virtual void HudText(float x, float y, unsigned rgba, const char* text) = 0;
	
	virtual bool CursorFree() = 0;
	virtual void RequestCursor() = 0;
};

inline void GuiTextf(IGui* gui, const char* fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	std::vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	gui->Text(buf);
}