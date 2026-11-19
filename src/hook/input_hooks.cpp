#include "input_hooks.h"
#include "worker.h"
#include "shared_state.h"
#include "log.h"

static HHOOK g_getMsg = nullptr;
static HHOOK g_kbd = nullptr;
static HANDLE g_worker = nullptr;

static bool IsTaskbarClass(const wchar_t* cls)
{
    return wcsstr(cls, L"MSTask") || wcsstr(cls, L"Shell_Tray") ||
           wcsstr(cls, L"ReBar") || wcsstr(cls, L"WorkerW");
}

// CRITICAL: keep this hook tiny — no UIA/COM (restarts explorer).
static LRESULT CALLBACK GetMsgProc(int code, WPARAM wp, LPARAM lp)
{
    if (code >= 0 && wp == PM_REMOVE) {
        MSG* msg = (MSG*)lp;
        if (msg->message == WM_RBUTTONDOWN) {
            wchar_t cls[64] = L"";
            GetClassNameW(msg->hwnd, cls, 64);
            if (IsTaskbarClass(cls) && g_worker) {
                TeState().clickPt = msg->pt;
                PostThreadMessageW(GetThreadId(g_worker), TE_WM_SHOW_END,
                    (WPARAM)(INT_PTR)msg->pt.x, (LPARAM)(INT_PTR)msg->pt.y);
            }
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

static LRESULT CALLBACK KbdProc(int code, WPARAM wp, LPARAM lp)
{
    // Never destroy windows inside a hook — that restarts Explorer.
    if (code >= 0 && wp == WM_KEYDOWN && g_worker) {
        auto* kb = (KBDLLHOOKSTRUCT*)lp;
        if (kb->vkCode == VK_ESCAPE)
            PostThreadMessageW(GetThreadId(g_worker), TE_WM_DISMISS, 0, 0);
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

bool TeInstallInputHooks(DWORD trayThreadId, HANDLE workerThread)
{
    g_worker = workerThread;
    if (trayThreadId) {
        g_getMsg = SetWindowsHookExW(WH_GETMESSAGE, GetMsgProc, nullptr, trayThreadId);
        TeLog(L"WH_GETMESSAGE tid=%lu hook=%p", trayThreadId, g_getMsg);
    }
    g_kbd = SetWindowsHookExW(WH_KEYBOARD_LL, KbdProc, nullptr, 0);
    TeLog(L"WH_KEYBOARD_LL hook=%p", g_kbd);
    return g_getMsg || g_kbd;
}

void TeRemoveInputHooks()
{
    if (g_getMsg) { UnhookWindowsHookEx(g_getMsg); g_getMsg = nullptr; }
    if (g_kbd) { UnhookWindowsHookEx(g_kbd); g_kbd = nullptr; }
    g_worker = nullptr;
}
