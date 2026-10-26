#include "gui.h"

#include "../shared/convar.h"
#include "cvar_system.h"

#include "../third_party/imgui-1.91.9/imgui_impl_win32.h"
#include "../third_party/imgui-1.91.9/imgui_impl_dx11.h"

#include <algorithm>
#include <cctype>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static ConVar r_showstats("r_showstats", "1", FCVAR_ARCHIVE, "Show the statistics overlay", 0.0f, 1.0f);

bool Gui::Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	ImGui::StyleColorsDark();

	const float scale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
	ImGui::GetStyle().ScaleAllSizes(scale);
	io.FontGlobalScale = scale;

	if (!ImGui_ImplWin32_Init(hwnd))
	{
		ImGui::DestroyContext();
		return false;
	}
	if (!ImGui_ImplDX11_Init(device, context))
	{
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return false;
	}
	m_initialized = true;
	return true;
}

void Gui::Shutdown()
{
	if (!m_initialized) return;
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	m_initialized = false;
}

LRESULT Gui::MessageHook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (!ImGui::GetCurrentContext()) return 0;
	return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
}

void Gui::NewFrame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void Gui::Render()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void Gui::DrawPanels(const GuiStats& stats)
{
	DrawStats(stats);
	DrawConsole();
}

bool Gui::WantsKeyboard() const
{
	return m_consoleOpen || (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard);
}

bool Gui::WantsMouse() const
{
	return ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;
}

void Gui::SetMouseEnabled(bool enabled)
{
	if (!ImGui::GetCurrentContext()) return;
	ImGuiIO& io = ImGui::GetIO();
	if (enabled) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	else io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
}

void Gui::ToggleConsole()
{
	m_consoleOpen = !m_consoleOpen;
	if (m_consoleOpen)
	{
		m_focusInput = true;
		m_scrollToBottom = true;
	}
}

void Gui::DrawStats(const GuiStats& s)
{
	if (!r_showstats.GetBool()) return;

	const ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.45f);

	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
	if (ImGui::Begin("##stats", nullptr, flags))
	{
		ImGui::Text("%.0f fps (%.2f ms)", s.fps, s.frameMs);
		ImGui::Text("pos   %.0f %.0f %.0f", s.camPos[0], s.camPos[1], s.camPos[2]);
		ImGui::Text("angle %.1f %.1f", s.pitchDeg, s.yawDeg);
	}
	ImGui::End();
}

void Gui::DrawConsole()
{
	if (!m_consoleOpen) return;

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(display.x, display.y * 0.45f));
	ImGui::SetNextWindowBgAlpha(0.88f);

	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Console", nullptr, flags)) {
		const float footer = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("##log", ImVec2(0.0f, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
		{
			const std::deque<ConsoleLine>& lines = Cvars().History();

			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(lines.size()));
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
				{
					const ConsoleLine& line = lines[static_cast<size_t>(i)];
					ImVec4 col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
					if (line.level == ConLevel::Warning)    col = ImVec4(1.0f, 0.85f, 0.30f, 1.0f);
					else if (line.level == ConLevel::Error) col = ImVec4(1.0f, 0.40f, 0.40f, 1.0f);
					ImGui::PushStyleColor(ImGuiCol_Text, col);
					ImGui::TextUnformatted(line.text.c_str());
					ImGui::PopStyleColor();
				}
			}
			clipper.End();

			if (m_scrollToBottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
			m_scrollToBottom = false;
		}
		ImGui::EndChild();
		ImGui::Separator();

		ImGui::SetNextItemWidth(-1.0f);
		if (m_focusInput)
		{
			ImGui::SetKeyboardFocusHere();
			m_focusInput = false;
		}

		const ImGuiInputTextFlags tf = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCharFilter;
		if (ImGui::InputText("##input", m_input, sizeof m_input, tf, &Gui::InputCallback, this))
		{
			Submit();
			m_focusInput = true;
		}
	}
	ImGui::End();
}

void Gui::Submit()
{
	std::string cmd = m_input;
	m_input[0] = '\0';

	const size_t first = cmd.find_first_not_of(" \t");
	if (first == std::string::npos) return;
	cmd = cmd.substr(first, cmd.find_last_not_of(" \t") - first + 1);

	if (m_history.empty() || m_history.back() != cmd) m_history.push_back(cmd);
	m_histPos = -1;

	Cvars().Print(ConLevel::Msg, ("> " + cmd).c_str());
	Cvars().AddText(cmd.c_str());
	m_scrollToBottom = true;
}

int Gui::InputCallback(ImGuiInputTextCallbackData* d)
{
	Gui* self = static_cast<Gui*>(d->UserData);

	switch (d->EventFlag) {
	case ImGuiInputTextFlags_CallbackCharFilter:
		return (d->EventChar == '`' || d->EventChar == '~') ? 1 : 0;

	case ImGuiInputTextFlags_CallbackCompletion:
	{
		if (d->CursorPos != d->BufTextLen) return 0;
		const std::string cur(d->Buf, static_cast<size_t>(d->CursorPos));
		if (cur.empty() || cur.find(' ') != std::string::npos) return 0;

		std::vector<std::string> matches;
		Cvars().CompleteName(cur.c_str(), matches);
		if (matches.empty()) return 0;

		std::string completed;
		if (matches.size() == 1) completed = matches[0] + " ";
		else
		{
			size_t common = matches[0].size();
			for (const std::string& m : matches)
			{
				size_t i = 0;
				while (i < common && i < m.size() && std::tolower(static_cast<unsigned char>(m[i])) == std::tolower(static_cast<unsigned char>(matches[0][i]))) ++i;
				common = i;
			}
			completed = matches[0].substr(0, common);

			std::string list;
			const size_t shown = std::min<size_t>(matches.size(), 24);
			for (size_t i = 0; i < shown; ++i)
			{
				list += matches[i];
				list += '\n';
			}
			if (matches.size() > shown) list += "...\n";
			Cvars().Print(ConLevel::Msg, list.c_str());
		}
		if (completed.size() >= cur.size())
		{
			d->DeleteChars(0, d->BufTextLen);
			d->InsertChars(0, completed.c_str());
		}
		return 0;
	}

	case ImGuiInputTextFlags_CallbackHistory:
	{
		const int size = static_cast<int>(self->m_history.size());
		const int prev = self->m_histPos;
		if (d->EventKey == ImGuiKey_UpArrow)
		{
			if (self->m_histPos == -1) self->m_histPos = size - 1;
			else if (self->m_histPos > 0) --self->m_histPos;
		}
		else if (d->EventKey == ImGuiKey_DownArrow)
		{
			if (self->m_histPos != -1 && ++self->m_histPos >= size) self->m_histPos = -1;
		}
		if (prev != self->m_histPos)
		{
			const char* text = self->m_histPos >= 0 ? self->m_history[static_cast<size_t>(self->m_histPos)].c_str() : "";
			d->DeleteChars(0, d->BufTextLen);
			d->InsertChars(0, text);
		}
		return 0;
	}
	}
	return 0;
}