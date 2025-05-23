#include <windows.h>
#include <iostream>
#include <string>
#include <fstream>
#include <tlhelp32.h>

// Global log stream
static std::ofstream g_log;

// Simple logging helper
void Log(const std::string &msg) {
    if (g_log.is_open()) {
        g_log << msg << std::endl;
        g_log.flush();
    }
}

/**
 * @brief Get the Process Id By Name
 * @param processName Name of the process (e.g., L"gmod.exe")
 * @return Process ID if found, 0 otherwise
 */
DWORD GetProcessIdByName(const std::wstring& processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        Log("[!] Failed to create process snapshot");
        return 0;
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(processEntry);

    if (!Process32First(snapshot, &processEntry)) {
        Log("[!] Failed to get first process from snapshot");
        CloseHandle(snapshot);
        return 0;
    }

    do {
        if (processEntry.szExeFile == processName) {
            DWORD pid = processEntry.th32ProcessID;
            CloseHandle(snapshot);
            return pid;
        }
    } while (Process32Next(snapshot, &processEntry));

    CloseHandle(snapshot);
    return 0;
}

int main()
{
    // Open log file in the same directory as the EXE
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exeDir(exePath);
    auto pos = exeDir.find_last_of(L"\\/");
    exeDir = exeDir.substr(0, pos);
    std::wstring logPath = exeDir + L"\\injector.log";
    g_log.open(logPath, std::ios::out | std::ios::app);
    if (!g_log.is_open()) {
        std::cerr << "[-] Could not open log file: " << std::string(logPath.begin(), logPath.end()) << std::endl;
    } else {
        Log("[+] Injector started");
    }

    SetConsoleTitle(L"Injector");
    std::cout << "[+] Waiting for gmod.exe" << std::endl;
    Log("[+] Waiting for gmod.exe");

    DWORD processId = 0;
    do {
        processId = GetProcessIdByName(L"gmod.exe");
        Sleep(100);
    } while (!processId);

    std::cout << "[+] gmod.exe found (PID: " << processId << ")" << std::endl;
    Log(std::string("[+] gmod.exe found (PID: ") + std::to_string(processId) + ")");

    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, currentDir);
    std::wstring dllPath = std::wstring(currentDir) + L"\\LuaHook.dll";
    std::ifstream dllFile(dllPath);
    if (!dllFile.good()) {
        std::cerr << "[-] Failed to open DLL: " << std::string(dllPath.begin(), dllPath.end()) << std::endl;
        Log(std::string("[-] Failed to open DLL: ") + std::string(dllPath.begin(), dllPath.end()));
        return 1;
    }
    dllFile.close();
    Log(std::string("[+] DLL path verified: ") + std::string(dllPath.begin(), dllPath.end()));

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) {
        std::cerr << "[-] Failed to open process" << std::endl;
        Log("[-] Failed to open process");
        return 1;
    }
    Log("[+] Opened process handle");

    size_t sizeBytes = (dllPath.length() + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(hProcess, NULL, sizeBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!remoteMem) {
        std::cerr << "[-] Failed to allocate memory in remote process" << std::endl;
        Log("[-] Failed to allocate memory in remote process");
        CloseHandle(hProcess);
        return 1;
    }
    Log("[+] Allocated remote memory");

    if (!WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), sizeBytes, NULL)) {
        std::cerr << "[-] Failed to write process memory" << std::endl;
        Log("[-] Failed to write process memory");
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }
    Log("[+] Wrote DLL path to remote process memory");

    auto loadLibAddr = reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    if (!loadLibAddr) {
        std::cerr << "[-] Failed to get LoadLibraryW address" << std::endl;
        Log("[-] Failed to get LoadLibraryW address");
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }
    Log("[+] Retrieved LoadLibraryW address");

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
                                        reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibAddr),
                                        remoteMem, 0, NULL);
    if (!hThread) {
        std::cerr << "[-] Failed to create remote thread" << std::endl;
        Log("[-] Failed to create remote thread");
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }
    Log("[+] Remote thread created");

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    Log("[+] DLL injected and remote memory freed");

    CloseHandle(hProcess);
    std::cout << "[+] Process handle closed" << std::endl;
    Log("[+] Process handle closed");

    return 0;
}
