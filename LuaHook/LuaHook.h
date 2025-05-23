#pragma once

#include <windows.h>
#include <string>
#include <memory>
#include <functional>

class CLuaShared;

// Lua function typedefs
using luaL_loadstring_fn = int(__cdecl*)(void* L, const char* s);
using lua_pcall_fn = int(__cdecl*)(void* L, int nargs, int nresults, int errfunc);
using CreateInterfaceFn = void* (*)(const char* name, int* ret);

namespace LuaConstants {
    constexpr int LUA_MULTRET = -1;
    constexpr int LUA_OK = 0;
    constexpr const char* LUA_SHARED_DLL = "lua_shared.dll";
    constexpr const char* CREATE_INTERFACE = "CreateInterface";
    constexpr const char* LUASHARED_INTERFACE = "LUASHARED003";
    constexpr const char* SCRIPT_FILENAME = "hook.lua";
    constexpr DWORD POLL_INTERVAL_MS = 500;
    constexpr DWORD MODULE_WAIT_INTERVAL_MS = 50;
}

class Logger {
public:
    static void Initialize();
    static void Info(const std::string& message);
    static void Error(const std::string& message);
    static void Success(const std::string& message);
    
private:
    static bool consoleInitialized;
};

class ScriptManager {
public:
    bool LoadScript(const std::string& modulePath);
    const std::string& GetScript() const { return scriptContent; }
    bool IsLoaded() const { return loaded; }
    
private:
    std::string scriptContent;
    bool loaded = false;
    
    std::string GetScriptPath(const std::string& modulePath);
    std::string ReadFileContent(const std::string& path);
};

class LuaInterface {
public:
    struct Functions {
        luaL_loadstring_fn loadstring = nullptr;
        lua_pcall_fn pcall = nullptr;
    };
    
    bool Initialize(HMODULE luaModule);
    bool ExecuteScript(void* luaState, const std::string& script);
    
private:
    Functions functions;
};

class LuaInjector {
public:
    LuaInjector(HMODULE moduleHandle);
    void Run();
    
private:
    HMODULE moduleHandle;
    HMODULE luaModule = nullptr;
    CLuaShared* luaShared = nullptr;
    LuaInterface luaInterface;
    ScriptManager scriptManager;
    void* lastClientInterface = nullptr;
    
    bool WaitForLuaModule();
    bool InitializeLuaShared();
    bool LoadScript();
    void InjectIntoInterface(void* luaInterfacePtr);
    void* ExtractLuaState(void* interfacePtr);
    void PollForInterfaceChanges();
};