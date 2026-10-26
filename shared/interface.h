#pragma once

#include <cstring>

#define BE_EXPORT extern "C" __declspec(dllexport)

using CreateInterfaceFn = void* (*)(const char* name, int* returnCode);

#define EXPOSE_INTERFACE(Class, Iface, Version) \
	BE_EXPORT void* CreateInterface(const char* name, int* returnCode) \
	{ \
		static Class instance; \
		if (name && std::strcmp(name, Version) == 0) \
		{ \
			if (returnCode) *returnCode = 0; \
			return static_cast<Iface*>(&instance); \
		} \
		if (returnCode) *returnCode = 1; \
		return nullptr; \
	}
