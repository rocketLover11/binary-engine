#include "gui_api.h"
#include "../shared/convar.h"
#include "cvar_system.h"

#include "../third_party/imgui-1.91.9/imgui.h"

#include <cstdio>

namespace
{
	
	ImU32 ToCol(unsigned c)
	{
		return IM_COL32((c >> 24) & 255, (c >> 16) & 255, (c >> 8) & 255, c & 255);
	}

	ConVar* FindVar(const char* name)
	{
		ConCommandBase* b = Cvars().FindCommand(name);
		return (b && !b->IsCommand()) ? static_cast<ConVar*>(b) : nullptr;
	}

	bool Locked(const ConVar* v)
	{
		if (v->HasFlag(FCVAR_READONLY)) return true;
		if (v->HasFlag(FCVAR_CHEAT))
		{
			const ConVar* cheats = FindVar("sv_cheats");
			return !(cheats && cheats->GetBool());
		}
		return false;
	}

}

bool GuiCvarCheckbox(const char* label, const char* cvarName)
{
	ConVar* v = FindVar(cvarName);
	if (!v)
	{
		ImGui::TextDisabled("%s (missing cvar %s)", label, cvarName);
		return false;
	}

	const bool locked = Locked(v);
	bool value = v->GetBool();
	ImGui::BeginDisabled(locked);
	const bool changed = ImGui::Checkbox(label, &value);
	ImGui::EndDisabled();
	if (locked && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Locked: cheat-protected (sv_cheats 1) or read-only");
	if (changed) Cvars().UserSet(v, value ? "1" : "0");
	return changed;
}

bool GuiCvarSlider(const char* label, const char* cvarName, float lo, float hi)
{
    ConVar* v = FindVar(cvarName);
    if (!v) { ImGui::TextDisabled("%s (missing cvar %s)", label, cvarName); return false; }

    const bool locked = Locked(v);
    float value = v->GetFloat();
    const char* fmt = (hi - lo) >= 50.0f ? "%.0f" : "%.2f";

    ImGui::BeginDisabled(locked);
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15.0f);
    const bool changed = ImGui::SliderFloat(label, &value, lo, hi, fmt);
    ImGui::EndDisabled();
    if (locked && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Locked: cheat-protected (sv_cheats 1) or read-only");
    if (changed)
    {
        char buf[48];
        std::snprintf(buf, sizeof buf, "%g", static_cast<double>(value));
        Cvars().UserSet(v, buf);
    }
    return changed;
}

bool GuiApi::Begin(const char* title, bool* open, unsigned f)
{
    ImGuiWindowFlags w = ImGuiWindowFlags_NoSavedSettings;
    if (f & GUIWIN_NO_TITLEBAR) w |= ImGuiWindowFlags_NoTitleBar;
    if (f & GUIWIN_NO_RESIZE) w |= ImGuiWindowFlags_NoResize;
    if (f & GUIWIN_NO_MOVE) w |= ImGuiWindowFlags_NoMove;
    if (f & GUIWIN_AUTO_RESIZE) w |= ImGuiWindowFlags_AlwaysAutoResize;
    if (f & GUIWIN_NO_BACKGROUND) w |= ImGuiWindowFlags_NoBackground;
    if (f & GUIWIN_NO_INPUTS) w |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing;
    if (f & GUIWIN_NO_COLLAPSE) w |= ImGuiWindowFlags_NoCollapse;
    return ImGui::Begin(title, open, w);
}

void GuiApi::End() { ImGui::End(); }

void GuiApi::SetNextWindowPos(float x, float y, float px, float py, bool always)
{
    ImGui::SetNextWindowPos(ImVec2(x, y), always ? ImGuiCond_Always : ImGuiCond_FirstUseEver, ImVec2(px, py));
}

void GuiApi::SetNextWindowSize(float w, float h, bool always)
{
    ImGui::SetNextWindowSize(ImVec2(w, h), always ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
}

void GuiApi::SetNextWindowBgAlpha(float alpha) { ImGui::SetNextWindowBgAlpha(alpha); }

void GuiApi::GetDisplaySize(float* w, float* h)
{
    const ImVec2 d = ImGui::GetIO().DisplaySize;
    if (w) *w = d.x;
    if (h) *h = d.y;
}

void GuiApi::Text(const char* text) { ImGui::TextUnformatted(text); }

void GuiApi::TextColored(unsigned rgba, const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ToCol(rgba));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void GuiApi::Separator() { ImGui::Separator(); }
void GuiApi::Spacing() { ImGui::Spacing(); }
void GuiApi::SameLine() { ImGui::SameLine(); }

bool GuiApi::Button(const char* label) { return ImGui::Button(label); }
bool GuiApi::Checkbox(const char* label, bool* value) { return ImGui::Checkbox(label, value); }
bool GuiApi::SliderFloat(const char* label, float* v, float lo, float hi) { return ImGui::SliderFloat(label, v, lo, hi, "%.2f"); }
bool GuiApi::SliderInt(const char* label, int* v, int lo, int hi) { return ImGui::SliderInt(label, v, lo, hi); }
bool GuiApi::InputText(const char* label, char* buffer, size_t size) { return ImGui::InputText(label, buffer, size); }

void GuiApi::ProgressBar(float fraction, float width, float height, const char* overlay)
{
    ImGui::ProgressBar(fraction, ImVec2(width, height), overlay);
}

bool GuiApi::CollapsingHeader(const char* label) { return ImGui::CollapsingHeader(label); }
bool GuiApi::BeginTabBar(const char* id) { return ImGui::BeginTabBar(id); }
void GuiApi::EndTabBar() { ImGui::EndTabBar(); }
bool GuiApi::BeginTabItem(const char* l) { return ImGui::BeginTabItem(l); }
void GuiApi::EndTabItem() { ImGui::EndTabItem(); }

bool GuiApi::CvarCheckbox(const char* label, const char* cvar) { return GuiCvarCheckbox(label, cvar); }
bool GuiApi::CvarSlider(const char* label, const char* cvar, float lo, float hi) { return GuiCvarSlider(label, cvar, lo, hi); }

void GuiApi::HudRect(float x0, float y0, float x1, float y1, unsigned rgba)
{
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), ToCol(rgba));
}

void GuiApi::HudText(float x, float y, unsigned rgba, const char* text)
{
    ImGui::GetBackgroundDrawList()->AddText(ImVec2(x, y), ToCol(rgba), text);
}