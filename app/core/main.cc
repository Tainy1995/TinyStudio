#include <Windows.h>
#include <crtdbg.h>
#include "core-engine.h"


int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE prevInstance,
	_In_ LPSTR cmdLine, _In_ int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	CoreEngine engine;
	engine.SetAppInstance(hInstance);
	return engine.StartUpCoreEngine();
}
