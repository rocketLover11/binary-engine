#include "../shared/interface.h"
#include "../shared/eiface.h"
#include "../shared/mbox.h"

#include <Windows.h>

#include <cstdio>

int main()
{
	HMODULE mod = LoadLibraryA("engine.dll");
	if (!mod)
	{
		ErrorMessage("Failed to load engine.dll (error {})", GetLastError());
		return 1;
	}

	auto factory = reinterpret_cast<CreateInterfaceFn>(GetProcAddress(mod, "CreateInterface"));
	auto* engine = factory ? static_cast<IEngine*>(factory(ENGINE_VERSION, nullptr)) : nullptr;
	if (!engine)
	{
		ErrorMessage("engine.dll does not provide {}\n", ENGINE_VERSION);
		return 2;
	}

	const int rc = engine->Run();
	FreeLibrary(mod);
	return rc;
}