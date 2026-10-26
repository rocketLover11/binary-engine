#pragma once

#include <Windows.h>

#include <string>
#include <format>

template <typename... Args>
inline void ErrorMessage(std::format_string<Args...> fmt, Args&&... args)
{
	std::string msg = std::format(fmt, std::forward<Args>(args)...);
	MessageBoxA(nullptr, msg.c_str(), "Error", MB_OK | MB_ICONERROR);
}