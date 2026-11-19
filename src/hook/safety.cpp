#include "safety.h"
#include <tlhelp32.h>
#include <cwchar>

bool TeIsProtectedProcessName(const wchar_t* exe)
{
    if (!exe || !exe[0]) return true;
    static const wchar_t* kBlocked[] = {
        L"explorer.exe", L"dwm.exe", L"csrss.exe", L"winlogon.exe",
        L"services.exe", L"lsass.exe", L"smss.exe", L"wininit.exe",
        L"System", L"Registry",
    };
    for (auto* b : kBlocked) {
        if (_wcsicmp(exe, b) == 0) return true;
    }
    return false;
}

bool TeIsFrameHostProcessName(const wchar_t* exe)
{
    return exe && (_wcsicmp(exe, L"ApplicationFrameHost.exe") == 0 ||
                   _wcsicmp(exe, L"TextInputHost.exe") == 0);
}

bool TeGetProcessExeName(DWORD pid, wchar_t* exe, size_t cch)
{
    if (!exe || !cch) return false;
    exe[0] = L'\0';
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    bool ok = false;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                wcsncpy_s(exe, cch, pe.szExeFile, _TRUNCATE);
                ok = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return ok;
}

bool TeIsProtectedPid(DWORD pid)
{
    wchar_t exe[128] = L"";
    if (!TeGetProcessExeName(pid, exe, _countof(exe))) return false;
    return TeIsProtectedProcessName(exe);
}

bool TeIsShellChromeHwnd(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd)) return true;
    wchar_t cls[64] = L"";
    GetClassNameW(hwnd, cls, 64);
    return wcsstr(cls, L"Shell_Tray") || wcsstr(cls, L"Shell_SecondaryTray") ||
           wcsstr(cls, L"Progman") || wcsstr(cls, L"WorkerW") ||
           wcsstr(cls, L"MSTask") || wcsstr(cls, L"ForegroundStaging") ||
           wcsstr(cls, L"TaskList") || wcsstr(cls, L"DV2ControlHost");
}

bool TeIsExplorerHostedClass(const wchar_t* cls)
{
    return cls && (_wcsicmp(cls, L"CabinetWClass") == 0 ||
                   _wcsicmp(cls, L"ExploreWClass") == 0 ||
                   wcscmp(cls, L"#32770") == 0);
}
