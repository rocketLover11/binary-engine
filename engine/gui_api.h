#pragma once

#include "../shared/giface.h"

bool GuiCvarCheckbox(const char* label, const char* cvarName);
bool GuiCvarSlider(const char* label, const char* cvarName, float lo, float hi);

class GuiApi final : public IGui
{
public:
	bool Begin(const char* title, bool* open, unsigned flags) override;
	void End() override;
	void SetNextWindowPos(float x, float y, float pivotX, float pivotY, bool always) override;
	void SetNextWindowSize(float w, float h, bool always) override;
	void SetNextWindowBgAlpha(float alpha) override;
	void GetDisplaySize(float* w, float* h) override;

	void Text(const char* text) override;
	void TextColored(unsigned rgba, const char* text) override;
	void Separator() override;
	void Spacing() override;
	void SameLine() override;

    bool Button(const char* label) override;
    bool Checkbox(const char* label, bool* value) override;
    bool SliderFloat(const char* label, float* value, float lo, float hi) override;
    bool SliderInt(const char* label, int* value, int lo, int hi) override;
    bool InputText(const char* label, char* buffer, size_t bufferSize) override;
    void ProgressBar(float fraction, float width, float height, const char* overlay) override;
    bool CollapsingHeader(const char* label) override;
    bool BeginTabBar(const char* id) override;
    void EndTabBar() override;
    bool BeginTabItem(const char* label) override;
    void EndTabItem() override;

    bool CvarCheckbox(const char* label, const char* cvarName) override;
    bool CvarSlider(const char* label, const char* cvarName, float lo, float hi) override;

    void HudRect(float x0, float y0, float x1, float y1, unsigned rgba) override;
    void HudText(float x, float y, unsigned rgba, const char* text) override;

    bool CursorFree() override { return m_cursorFree; }
    void RequestCursor() override { m_cursorRequested = true; }

    bool TakeCursorRequest() { const bool r = m_cursorRequested; m_cursorRequested = false; return r; }
    void SetCursorFree(bool free) { m_cursorFree = free; }

private:
    bool m_cursorRequested = false;
    bool m_cursorFree = false;
};