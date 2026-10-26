#include "std_console.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace
{

    std::wstring Widen(const char* s)
    {
        const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
        if (n <= 1) return {};
        std::wstring w(static_cast<size_t>(n - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
        return w;
    }

    std::string Narrow(const std::wstring& w)
    {
        if (w.empty()) return {};
        const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        std::string s(static_cast<size_t>(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
        return s;
    }

}

StdConsole::~StdConsole() { Shutdown(); }

bool StdConsole::Init(CvarSystem* cvars)
{
    m_cvars = cvars;
    m_cvars->AddSink(this);

    m_in = GetStdHandle(STD_INPUT_HANDLE);
    m_out = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD inMode = 0, outMode = 0;
    if (!m_in || m_in == INVALID_HANDLE_VALUE || !GetConsoleMode(m_in, &inMode))  return false;
    if (!m_out || m_out == INVALID_HANDLE_VALUE || !GetConsoleMode(m_out, &outMode)) return false;

    m_oldInMode = inMode;
    m_oldOutMode = outMode;

    SetConsoleMode(m_in, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS);
    m_vt = SetConsoleMode(m_out, outMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;

    m_enabled = true;
    return true;
}

void StdConsole::Shutdown()
{
    if (m_cvars)
    {
        m_cvars->RemoveSink(this);
        m_cvars = nullptr;
    }
    if (!m_enabled) return;

    if (m_promptVisible) Write(L"\r" + std::wstring(2 + m_drawnLen, L' ') + L"\r");
    SetConsoleMode(m_in, m_oldInMode);
    SetConsoleMode(m_out, m_oldOutMode);
    m_enabled = false;
    m_promptVisible = false;
}

void StdConsole::Write(const std::wstring& s)
{
    DWORD written = 0;
    WriteConsoleW(m_out, s.c_str(), static_cast<DWORD>(s.size()), &written, nullptr);
}

void StdConsole::Redraw()
{
    std::wstring out = L"\r> " + m_line;
    if (m_drawnLen > m_line.size())
    {
        out.append(m_drawnLen - m_line.size(), L' ');
        out += L"\r> " + m_line;
    }
    m_drawnLen = m_line.size();
    Write(out);
}

void StdConsole::OnPrint(ConLevel level, const char* line)
{
    if (!m_enabled)
    {
        std::fputs(line, stdout);
        std::fputc('\n', stdout);
        return;
    }

    std::wstring out;
    if (m_promptVisible) out += L"\r" + std::wstring(2 + m_drawnLen, L' ') + L"\r";
    if (m_vt && level == ConLevel::Warning) out += L"\x1b[33m";
    if (m_vt && level == ConLevel::Error)   out += L"\x1b[91m";
    out += Widen(line);
    if (m_vt && level != ConLevel::Msg) out += L"\x1b[0m";
    out += L"\r\n";
    if (m_promptVisible)
    {
        out += L"\r> " + m_line;
        m_drawnLen = m_line.size();
    }
    Write(out);
}

void StdConsole::Poll()
{
    if (!m_enabled) return;

    if (!m_promptVisible)
    {
        m_promptVisible = true;
        Redraw();
    }

    DWORD count = 0;
    while (GetNumberOfConsoleInputEvents(m_in, &count) && count > 0)
    {
        INPUT_RECORD rec[32];
        DWORD read = 0;
        if (!ReadConsoleInputW(m_in, rec, _countof(rec), &read) || read == 0) break;

        for (DWORD i = 0; i < read; ++i)
        {
            if (rec[i].EventType != KEY_EVENT || !rec[i].Event.KeyEvent.bKeyDown) continue;
            const KEY_EVENT_RECORD& k = rec[i].Event.KeyEvent;

            switch (k.wVirtualKeyCode) {
            case VK_RETURN: Submit(); break;
            case VK_TAB:    HandleTab(); break;
            case VK_UP:     History(-1); break;
            case VK_DOWN:   History(+1); break;
            case VK_ESCAPE: m_line.clear(); Redraw(); break;
            case VK_BACK:
                if (!m_line.empty())
                {
                    m_line.pop_back();
                    Redraw();
                }
                break;
            default:
                if (k.uChar.UnicodeChar >= 32 && k.uChar.UnicodeChar != 127)
                {
                    m_line.push_back(k.uChar.UnicodeChar);
                    Redraw();
                }
                break;
            }
        }
    }
}

void StdConsole::Submit()
{
    const std::wstring line = m_line;
    m_line.clear();

    Write(L"\r> " + line + L"\r\n");
    m_drawnLen = 0;

    if (!line.empty())
    {
        if (m_history.empty() || m_history.back() != line) m_history.push_back(line);
        m_cvars->AddText(Narrow(line).c_str());
    }
    m_histPos = static_cast<int>(m_history.size());
    Redraw();
}

void StdConsole::History(int dir)
{
    if (m_history.empty()) return;
    const int size = static_cast<int>(m_history.size());
    int pos = m_histPos + dir;
    if (pos < 0) pos = 0;
    if (pos >= size) 
    {
        m_histPos = size;
        m_line.clear();
    }
    else 
    {
        m_histPos = pos;
        m_line = m_history[static_cast<size_t>(pos)];
    }
    Redraw();
}

void StdConsole::HandleTab()
{
    const std::string cur = Narrow(m_line);
    if (cur.empty() || cur.find(' ') != std::string::npos) return;

    std::vector<std::string> matches;
    m_cvars->CompleteName(cur.c_str(), matches);
    if (matches.empty()) return;

    if (matches.size() == 1)
    {
        m_line = Widen(matches[0].c_str()) + L" ";
        Redraw();
        return;
    }

    size_t common = matches[0].size();
    for (const std::string& m : matches)
    {
        size_t i = 0;
        while (i < common && i < m.size() && std::tolower(static_cast<unsigned char>(m[i])) == std::tolower(static_cast<unsigned char>(matches[0][i]))) ++i;
        common = i;
    }
    if (common > cur.size()) m_line = Widen(matches[0].substr(0, common).c_str());

    std::string list;
    const size_t shown = std::min<size_t>(matches.size(), 24);
    for (size_t i = 0; i < shown; ++i)
    {
        list += matches[i];
        list += '\n';
    }
    if (matches.size() > shown) list += "...\n";
    m_cvars->Print(ConLevel::Msg, list.c_str());
    Redraw();
}