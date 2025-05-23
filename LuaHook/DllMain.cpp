#include <windows.h>
#include <thread>
#include <memory>
#include "LuaHook.h"

static std::unique_ptr<std::thread> g_injectorThread;

DWORD WINAPI InjectorThreadProc(LPVOID param) {
    HMODULE moduleHandle = reinterpret_cast<HMODULE>(param);
    LuaInjector injector(moduleHandle);
    injector.Run();
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hInstance);
        CreateThread(nullptr, 0, InjectorThreadProc, hInstance, 0, nullptr);
    }
    
    return TRUE;
}