#include "terminate.h"
#include "shared_state.h"
#include "safety.h"
#include "end_button.h"
#include "target.h"
#include "log.h"
#include <tlhelp32.h>
#include <shlwapi.h>

static void KillFamily(DWORD mainPid)
{
    wchar_t mainExe[128] = L"";
    TeGetProcessExeName(mainPid, mainExe, _countof(mainExe));
    if (!mainExe[0] || TeIsProtectedProcessName(mainExe)) return;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == mainPid) continue;
            if (pe.th32ParentProcessID != mainPid) continue;
            if (TeIsProtectedProcessName(pe.szExeFile)) continue;
            if (_wcsicmp(pe.szExeFile, mainExe) != 0) {
                // also kill same-name siblings under same parent tree later — keep simple
                wchar_t child[128] = L"";
                wcsncpy_s(child, pe.szExeFile, _TRUNCATE);
                wchar_t* dot = wcsrchr(child, L'.');
                if (dot) *dot = L'\0';
                wchar_t mainNo[128] = L"";
                wcsncpy_s(mainNo, mainExe, _TRUNCATE);
                wchar_t* d2 = wcsrchr(mainNo, L'.');
                if (d2) *d2 = L'\0';
                if (!StrStrIW(child, mainNo) && !StrStrIW(mainNo, child))
                    continue;
            }
            HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
            if (hp) {
                TerminateProcess(hp, 1);
                CloseHandle(hp);
                TeLog(L"family kill pid=%lu", pe.th32ProcessID);
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}

void TeExecuteEnd(TeTarget& target, TeTarget& hover)
{
    TeHideEndButton();

    if (!TeTargetFresh(target, TE_TARGET_TIMEOUT_MS) && !target.hwnd) {
        TeLog(L"no valid target");
        TeClearTarget(target);
        TeClearTarget(hover);
        return;
    }

    if (target.closeOnly) {
        if (target.hwnd && IsWindow(target.hwnd) && !TeIsShellChromeHwnd(target.hwnd)) {
            TeLog(L"WM_CLOSE shell-hosted '%s' hwnd=%p", target.name, target.hwnd);
            PostMessageW(target.hwnd, WM_CLOSE, 0, 0);
        } else {
            TeLog(L"refuse closeOnly chrome hwnd=%p", target.hwnd);
        }
        TeClearTarget(target);
        TeClearTarget(hover);
        return;
    }

    DWORD pid = target.pid;
    if (target.hwnd) {
        DWORD root = 0;
        GetWindowThreadProcessId(target.hwnd, &root);
        DWORD uwp = 0;
        // Re-resolve AFH → real app if needed
        wchar_t exe[128] = L"";
        if (TeGetProcessExeName(root ? root : pid, exe, _countof(exe)) && TeIsFrameHostProcessName(exe)) {
            // Identify path already prefers UWP; keep pid as stored.
        }
        if (root && !TeIsProtectedPid(root) && !TeIsFrameHostProcessName(exe))
            pid = root;
        (void)uwp;
    }

    if (pid == GetCurrentProcessId() || TeIsProtectedPid(pid)) {
        if (target.hwnd && !TeIsShellChromeHwnd(target.hwnd)) {
            wchar_t cls[64] = L"";
            GetClassNameW(target.hwnd, cls, 64);
            if (TeIsExplorerHostedClass(cls)) {
                PostMessageW(target.hwnd, WM_CLOSE, 0, 0);
                TeLog(L"fallback WM_CLOSE explorer-hosted");
                TeClearTarget(target);
                TeClearTarget(hover);
                return;
            }
        }
        TeLog(L"refuse protected pid=%lu", pid);
        TeClearTarget(target);
        TeClearTarget(hover);
        return;
    }

    TeLog(L"end '%s' pid=%lu hwnd=%p", target.name, pid, target.hwnd);

    if (target.hwnd && IsWindow(target.hwnd) && !TeIsShellChromeHwnd(target.hwnd)) {
        PostMessageW(target.hwnd, WM_CLOSE, 0, 0);
        HWND root = GetAncestor(target.hwnd, GA_ROOT);
        if (root && root != target.hwnd && !TeIsShellChromeHwnd(root))
            PostMessageW(root, WM_CLOSE, 0, 0);
    }

    HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hp) {
        TerminateProcess(hp, 1);
        CloseHandle(hp);
        TeLog(L"terminated pid=%lu", pid);
    } else {
        TeLog(L"OpenProcess failed %lu", GetLastError());
    }
    KillFamily(pid);

    TeClearTarget(target);
    TeClearTarget(hover);
}
