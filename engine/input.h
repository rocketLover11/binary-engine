#pragma once

#include "keys.h"

#include <string>
#include <vector>

struct InputButtons
{
	bool forward = false;
	bool back = false;
	bool left = false;
	bool right = false;
	bool up = false;
	bool down = false;
	bool sprint = false;
};
extern InputButtons g_buttons;

class InputSystem
{
public:
	static int KeyFromName(const char* name);
	static std::string NameFromKey(int key);
	static bool IsReserved(int key);

	bool Bind(int key, const char* command);
	void Unbind(int key);
	void UnbindAll();
	void UnbindCommand(const char* command);
	const std::string& Binding(int key) const;
	int FindKeyFor(const char* command) const;
	std::vector<int> BoundKeys() const;
	void SetDefaults();

	void OnKey(int key, bool down, bool blocked);
	void ReleaseAll();

	void StartCapture();
	void CancelCapture();
	bool Capturing() const { return m_capturing; }
	void Capture(int key, bool down);
	bool ConsumeCaptured(int& key);

	void AppendBindConfig(std::string& out) const;

private:
	std::string m_binds[kKeyCount];
	std::string m_heldRelease[kKeyCount];

	bool m_capturing = false;
	bool m_captureDone = false;
	int m_capturedKey = -1;
};

InputSystem& Input();