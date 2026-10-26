#include "main_menu.h"
#include "../shared/convar.h"
#include "cvar_system.h"
#include "input.h"
#include "gui_api.h"

#include "../third_party/imgui-1.91.9/imgui.h"
#include "../third_party/imgui-1.91.9/imgui_internal.h"

#include <cstdio>

namespace
{

	struct Action
	{
		const char* label;
		const char* command;
	};

	const Action kActions[] = {
		{ "Move forward", "+forward" },
		{ "Move back",    "+back" },
		{ "Move left",    "+moveleft" },
		{ "Move right",   "+moveright" },
		{ "Move up",      "+moveup" },
		{ "Move down",    "+movedown" },
		{ "Sprint",       "+sprint" },
	};

	bool IsListedAction(const std::string& command)
	{
		for (const Action& a : kActions) if (command == a.command) return true;
		return false;
	}

	ConVar* Var(const char* name)
	{
		ConCommandBase* b = Cvars().FindCommand(name);
		return (b && !b->IsCommand()) ? static_cast<ConVar*>(b) : nullptr;
	}

}

void MainMenu::Open()
{
	m_open = true;
	m_page = Page::Main;
}

void MainMenu::Close()
{
	m_open = false;
	m_waitingAction.clear();
	Input().CancelCapture();
}

void MainMenu::HandleEscape()
{
	if (!m_open) Open();
	else if (m_page == Page::Settings) m_page = Page::Main;
	else Close();
}

void MainMenu::Draw()
{
	if (!m_open) return;

	const ImGuiIO& io = ImGui::GetIO();
	const float fs = ImGui::GetFontSize();

	ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize, IM_COL32(0, 0, 0, 150));

	int captured = -1;
	if (Input().ConsumeCaptured(captured))
	{
		if (captured >= 0 && !m_waitingAction.empty())
		{
			Input().UnbindCommand(m_waitingAction.c_str());
			if (!Input().Bind(captured, m_waitingAction.c_str())) ConWarning("That key can't be bound\n");
		}
		m_waitingAction.clear();
	}

	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	if (m_page == Page::Main) ImGui::SetNextWindowSize(ImVec2(fs * 16.0f, 0.0f), ImGuiCond_Always);
	else ImGui::SetNextWindowSize(ImVec2(ImMin(fs * 40.0f, io.DisplaySize.x * 0.95f), ImMin(fs * 28.0f, io.DisplaySize.y * 0.90f)), ImGuiCond_Always);

	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Binary##menu", nullptr, flags))
	{
		if (m_page == Page::Main) DrawMain();
		else DrawSettings();
	}
	ImGui::End();
}

void MainMenu::DrawMain()
{
	const ImVec2 button(-1.0f, ImGui::GetFontSize() * 2.0f);
	if (ImGui::Button("Resume", button)) Close();
	if (ImGui::Button("Settings", button)) m_page = Page::Settings;
	ImGui::Spacing();
	if (ImGui::Button("Quit", button)) Cvars().AddText("quit");
}

void MainMenu::DrawSettings()
{
	const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;

	if (ImGui::BeginTabBar("##settings"))
	{
		if (ImGui::BeginTabItem("Video"))
		{
			if (ImGui::BeginChild("##video", ImVec2(0.0f, -footer))) DrawVideo();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Mouse"))
		{
			if (ImGui::BeginChild("##mouse", ImVec2(0.0f, -footer))) DrawMouse();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Controls"))
		{
			if (ImGui::BeginChild("##controls", ImVec2(0.0f, -footer))) DrawControls();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::Separator();
	if (ImGui::Button("Back")) m_page = Page::Main;
}

void MainMenu::DrawVideo()
{
	const float fs = ImGui::GetFontSize();

	GuiCvarCheckbox("Fullscreen (borderless)", "vid_fullscreen");

	ConVar* wv = Var("vid_width");
	ConVar* hv = Var("vid_height");
	ConVar* fv = Var("vid_fullscreen");
	if (wv && hv)
	{
		struct Res { int w, h; };
		static const Res kRes[] = { { 1280, 720 },{ 1600, 900 },{ 1920, 1080 },{ 2560, 1440 } };

		char preview[32];
		std::snprintf(preview, sizeof preview, "%d x %d", wv->GetInt(), hv->GetInt());

		ImGui::BeginDisabled(fv && fv->GetBool());
		ImGui::SetNextItemWidth(fs * 12.0f);
		if (ImGui::BeginCombo("Windowed size", preview))
		{
			for (const Res& r : kRes)
			{
				char item[32];
				std::snprintf(item, sizeof item, "%d x %d", r.w, r.h);
				if (ImGui::Selectable(item, r.w == wv->GetInt() && r.h == hv->GetInt()))
				{
					wv->SetInt(r.w);
					hv->SetInt(r.h);
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
	}

	if (ImGui::Button("Apply")) Cvars().AddText("vid_apply");
	ImGui::SameLine();
	ImGui::TextDisabled("Fullscreen and size change when you press Apply");

	ImGui::Separator();
	GuiCvarCheckbox("VSync", "vid_vsync");
	GuiCvarSlider("Field of view", "r_fov", 60.0f, 120.0f);
	GuiCvarCheckbox("Show statistics overlay", "r_showstats");
}

void MainMenu::DrawMouse()
{
	GuiCvarSlider("Sensitivity", "sensitivity", 0.1f, 20.0f);
	GuiCvarSlider("Camera speed", "cam_speed", 10.0f, 5000.0f);
	ImGui::TextDisabled("Sprint multiplies camera speed by 3");
}

void MainMenu::DrawControls()
{
	const float fs = ImGui::GetFontSize();

	if (m_waitingAction.empty()) ImGui::TextDisabled("Click a key to rebind it. Left click can't be bound here; use the console.");
	else ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.30f, 1.0f), "Press a key or mouse button... (Esc cancels)");
	ImGui::Spacing();

	if (ImGui::BeginTable("##binds", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
	{
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, fs * 11.0f);
		ImGui::TableSetupColumn("##clear", ImGuiTableColumnFlags_WidthFixed, fs * 4.5f);
		ImGui::TableHeadersRow();

		for (const Action& a : kActions)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(a.label);

			ImGui::TableSetColumnIndex(1);
			const bool waiting = (m_waitingAction == a.command);
			const int key = Input().FindKeyFor(a.command);
			std::string text = waiting ? "Press a key..."
				: (key >= 0 ? InputSystem::NameFromKey(key) : "Unbound");
			text += "###key";

			ImGui::PushID(a.command);
			if (waiting) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.50f, 0.10f, 1.0f));
			if (ImGui::Button(text.c_str(), ImVec2(-1.0f, 0.0f)))
			{
				m_waitingAction = a.command;
				Input().StartCapture();
			}
			if (waiting) ImGui::PopStyleColor();

			ImGui::TableSetColumnIndex(2);
			if (ImGui::SmallButton("Clear")) Input().UnbindCommand(a.command);
			ImGui::PopID();
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Toggle console");
		ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("`");
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Menu / back");
		ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("escape");

		ImGui::EndTable();
	}

	ImGui::Spacing();
	ImGui::SeparatorText("Other bindings");

	bool any = false;
	if (ImGui::BeginTable("##otherbinds", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH))
	{
		ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, fs * 8.0f);
		ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##unbind", ImGuiTableColumnFlags_WidthFixed, fs * 5.0f);

		for (int key : Input().BoundKeys())
		{
			const std::string cmd = Input().Binding(key);
			if (IsListedAction(cmd)) continue;
			any = true;

			ImGui::PushID(key);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(InputSystem::NameFromKey(key).c_str());
			ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(cmd.c_str());
			ImGui::TableSetColumnIndex(2);
			if (ImGui::SmallButton("Unbind")) Input().Unbind(key);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (!any) ImGui::TextDisabled("None. In the console: bind <key> \"<command>\"");

	ImGui::Spacing();
	if (ImGui::Button("Reset to defaults"))
	{
		Input().SetDefaults();
		m_waitingAction.clear();
		Input().CancelCapture();
	}
}