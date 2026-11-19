#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cwchar>
#include "log.h"

static DWORD FindExplorerPid()
{
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!tray) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(tray, &pid);
    return pid;
}

static bool Inject(DWORD pid, const wchar_t* dllPath)
{
    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!proc) {
        wprintf(L"OpenProcess failed: %lu\n", GetLastError());
        return false;
    }
    size_t bytes = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        CloseHandle(proc);
        return false;
    }
    WriteProcessMemory(proc, remote, dllPath, bytes, nullptr);
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto load = (LPTHREAD_START_ROUTINE)GetProcAddress(k32, "LoadLibraryW");
    HANDLE th = CreateRemoteThread(proc, nullptr, 0, load, remote, 0, nullptr);
    bool ok = false;
    if (th) {
        WaitForSingleObject(th, 10000);
        DWORD code = 0;
        GetExitCodeThread(th, &code);
        ok = code != 0;
        CloseHandle(th);
    }
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(proc);
    return ok;
}

static bool Unload()
{
    HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\TaskbarEnd_Unload");
    if (!ev) {
        wprintf(L"DLL not loaded (no unload event)\n");
        return false;
    }
    SetEvent(ev);
    CloseHandle(ev);
    // Wait until ready event goes away / give hook time to free
    Sleep(800);
    return true;
}

int wmain(int argc, wchar_t** argv)
{
    TeLogInit();
    bool doUnload = argc > 1 && (_wcsicmp(argv[1], L"/unload") == 0 || _wcsicmp(argv[1], L"-unload") == 0);

    wprintf(L"TaskbarEnd injector\n\n");
    if (doUnload) {
        wprintf(L"Signaling unload...\n");
        Unload();
        return 0;
    }

    wchar_t dll[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, dll, MAX_PATH);
    wchar_t* slash = wcsrchr(dll, L'\\');
    if (slash) wcscpy_s(slash + 1, MAX_PATH - (slash - dll + 1), L"TaskbarEndHook.dll");

    if (GetFileAttributesW(dll) == INVALID_FILE_ATTRIBUTES) {
        wprintf(L"Missing DLL: %s\n", dll);
        return 1;
    }

    DWORD pid = FindExplorerPid();
    if (!pid) {
        wprintf(L"explorer Shell_TrayWnd not found\n");
        return 1;
    }
    wprintf(L"explorer.exe PID: %lu\n", pid);
    if (!Inject(pid, dll)) {
        wprintf(L"Injection failed\n");
        return 1;
    }
    wprintf(L"SUCCESS: DLL injected\n");
    wprintf(L"Use TaskbarEndInjector.exe /unload to unload\n");
    return 0;
}
