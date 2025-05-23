#include "LuaHook.h"
#include "CLuaShared.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <iostream>

bool Logger::consoleInitialized = false;

void Logger::Initialize() {
    if (consoleInitialized) {
        return;
    }
    
    if (!AllocConsole()) {
        return;
    }
    
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    SetConsoleTitleA("Lua Injector Console");
    consoleInitialized = true;
}

void Logger::Info(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

void Logger::Error(const std::string& message) {
    std::cout << "[ERROR] " << message << std::endl;
}

void Logger::Success(const std::string& message) {
    std::cout << "[SUCCESS] " << message << std::endl;
}

bool ScriptManager::LoadScript(const std::string& modulePath) {
    std::string scriptPath = GetScriptPath(modulePath);
    Logger::Info("Script path: " + scriptPath);
    
    scriptContent = ReadFileContent(scriptPath);
    if (scriptContent.empty()) {
        Logger::Error("Failed to load script from: " + scriptPath);
        return false;
    }
    
    loaded = true;
    Logger::Success("Loaded script (" + std::to_string(scriptContent.size()) + " bytes)");
    return true;
}

std::string ScriptManager::GetScriptPath(const std::string& modulePath) {
    size_t lastSlash = modulePath.find_last_of("\\/");
    if (lastSlash == std::string::npos) {
        return LuaConstants::SCRIPT_FILENAME;
    }
    
    std::string directory = modulePath.substr(0, lastSlash);
    return directory + "\\" + LuaConstants::SCRIPT_FILENAME;
}

std::string ScriptManager::ReadFileContent(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool LuaInterface::Initialize(HMODULE luaModule) {
    functions.loadstring = reinterpret_cast<luaL_loadstring_fn>(
        GetProcAddress(luaModule, "luaL_loadstring")
    );
    
    functions.pcall = reinterpret_cast<lua_pcall_fn>(
        GetProcAddress(luaModule, "lua_pcall")
    );
    
    if (!functions.loadstring) {
        Logger::Error("Failed to find luaL_loadstring");
        return false;
    }
    
    if (!functions.pcall) {
        Logger::Error("Failed to find lua_pcall");
        return false;
    }
    
    Logger::Success("Lua functions resolved");
    return true;
}

bool LuaInterface::ExecuteScript(void* luaState, const std::string& script) {
    int loadResult = functions.loadstring(luaState, script.c_str());
    if (loadResult != LuaConstants::LUA_OK) {
        Logger::Error("luaL_loadstring failed");
        return false;
    }
    
    int execResult = functions.pcall(
        luaState, 
        0, 
        LuaConstants::LUA_MULTRET, 
        0
    );
    
    if (execResult != LuaConstants::LUA_OK) {
        Logger::Error("lua_pcall failed");
        return false;
    }
    
    Logger::Success("Script executed on lua_State: " + 
        std::to_string(reinterpret_cast<uintptr_t>(luaState)));
    return true;
}

LuaInjector::LuaInjector(HMODULE moduleHandle) : moduleHandle(moduleHandle) {}

void LuaInjector::Run() {
    Logger::Initialize();
    Logger::Info("Starting Lua injection process...");
    
    if (!WaitForLuaModule()) {
        return;
    }
    
    if (!InitializeLuaShared()) {
        return;
    }
    
    if (!luaInterface.Initialize(luaModule)) {
        return;
    }
    
    if (!LoadScript()) {
        return;
    }
    
    // Initial injection
    void* clientInterface = luaShared->GetLuaInterface(LUAINTERFACE_CLIENT);
    if (clientInterface) {
        InjectIntoInterface(clientInterface);
        lastClientInterface = clientInterface;
    }
    
    // Start polling for interface changes
    PollForInterfaceChanges();
}

bool LuaInjector::WaitForLuaModule() {
    Logger::Info("Waiting for " + std::string(LuaConstants::LUA_SHARED_DLL) + "...");
    
    while (true) {
        luaModule = GetModuleHandleA(LuaConstants::LUA_SHARED_DLL);
        if (luaModule) {
            break;
        }
        Sleep(LuaConstants::MODULE_WAIT_INTERVAL_MS);
    }
    
    Logger::Success(std::string(LuaConstants::LUA_SHARED_DLL) + " loaded at: " + 
        std::to_string(reinterpret_cast<uintptr_t>(luaModule)));
    return true;
}

bool LuaInjector::InitializeLuaShared() {
    auto CreateInterface = reinterpret_cast<CreateInterfaceFn>(
        GetProcAddress(luaModule, LuaConstants::CREATE_INTERFACE)
    );
    
    if (!CreateInterface) {
        Logger::Error("CreateInterface not found");
        return false;
    }
    
    luaShared = reinterpret_cast<CLuaShared*>(
        CreateInterface(LuaConstants::LUASHARED_INTERFACE, nullptr)
    );
    
    if (!luaShared) {
        Logger::Error("Failed to create CLuaShared interface");
        return false;
    }
    
    Logger::Success("CLuaShared interface created");
    return true;
}

bool LuaInjector::LoadScript() {
    char modulePath[MAX_PATH];
    GetModuleFileNameA(moduleHandle, modulePath, MAX_PATH);
    return scriptManager.LoadScript(std::string(modulePath));
}

void LuaInjector::InjectIntoInterface(void* luaInterfacePtr) {
    void* luaState = ExtractLuaState(luaInterfacePtr);
    if (!luaState) {
        Logger::Error("Failed to extract lua_State");
        return;
    }
    
    Logger::Info("Injecting into lua_State: " + 
        std::to_string(reinterpret_cast<uintptr_t>(luaState)));
    
    luaInterface.ExecuteScript(luaState, scriptManager.GetScript());
}

void* LuaInjector::ExtractLuaState(void* interfacePtr) {
    // lua_State is at offset sizeof(void*) in the interface
    void** vtablePtr = reinterpret_cast<void**>(interfacePtr);
    char* interfaceBytes = reinterpret_cast<char*>(interfacePtr);
    void** luaStatePtr = reinterpret_cast<void**>(interfaceBytes + sizeof(void*));
    return *luaStatePtr;
}

void LuaInjector::PollForInterfaceChanges() {
    Logger::Info("Starting interface polling...");
    
    while (true) {
        Sleep(LuaConstants::POLL_INTERVAL_MS);
        
        void* currentInterface = luaShared->GetLuaInterface(LUAINTERFACE_CLIENT);
        if (!currentInterface) {
            continue;
        }
        
        if (currentInterface == lastClientInterface) {
            continue;
        }
        
        Logger::Info("Detected new client interface");
        InjectIntoInterface(currentInterface);
        lastClientInterface = currentInterface;
    }
}