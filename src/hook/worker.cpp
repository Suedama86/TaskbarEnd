#include "worker.h"
#include "shared_state.h"
#include "input_hooks.h"
#include "target.h"
#include "end_button.h"
#include "terminate.h"
#include "log.h"

static HANDLE g_unloadThread = nullptr;

static void DoKill()
{
    TeExecuteEnd(TeState().current, TeState().hover);
}

static bool OverTaskbar(POINT pt)
{
    HWND hwnd = WindowFromPoint(pt);
    for (int i = 0; i < 8 && hwnd; ++i) {
        wchar_t cls[64] = L"";
        GetClassNameW(hwnd, cls, 64);
        if (wcsstr(cls, L"MSTask") || wcsstr(cls, L"Shell_Tray") ||
            wcsstr(cls, L"Shell_SecondaryTray"))
            return true;
        hwnd = GetParent(hwnd);
    }
    return false;
}

static bool RestoreHover(POINT click)
{
    auto& h = TeState().hover;
    if (!TeTargetFresh(h, TE_HOVER_TIMEOUT_MS)) return false;
    if (!TeSameIconSlot(click, h.pt)) {
        TeLog(L"stale hover slot (hover x=%d click x=%d)", h.pt.x, click.x);
        return false;
    }
    TeState().current = h;
    TeState().current.pt = click;
    TeState().current.tick = GetTickCount();
    TeLog(L"restored hover '%s' pid=%lu", h.name, h.pid);
    return true;
}

static DWORD WINAPI WorkerProc(LPVOID)
{
    auto& st = TeState();
    TeLog(L"worker start tid=%lu", GetCurrentThreadId());
    TeEnsureUia();

    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    DWORD trayTid = tray ? GetWindowThreadProcessId(tray, nullptr) : 0;
    TeInstallInputHooks(trayTid, st.worker);

    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, L"Global\\TaskbarEnd_Ready");
    if (ready) { SetEvent(ready); CloseHandle(ready); }

    while (st.running) {
        DWORD slice = TeEndButtonVisible() ? 15 : 100;
        DWORD wait = MsgWaitForMultipleObjects(1, &st.unloadEvent, FALSE, slice, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0) break;

        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { st.running = false; break; }
            if (msg.message == TE_WM_DISMISS) {
                TeHideEndButton();
                TeClearTarget(st.current);
                continue;
            }
            if (msg.message == TE_WM_SHOW_END) {
                POINT pt = { (LONG)msg.wParam, (LONG)msg.lParam };
                st.clickPt = pt;
                TeLog(L"right-click (%d,%d)", pt.x, pt.y);
                TeIdentifySafe(pt, true, st.current);
                if (!TeTargetFresh(st.current, TE_TARGET_TIMEOUT_MS))
                    RestoreHover(pt);
                if (st.current.pid || st.current.hwnd)
                    st.current.pt = pt;
                TeShowEndButton(pt, st.current, DoKill);
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Left click outside the button cancels it. Polled on this thread so
        // Explorer never hosts a mouse hook (that restarted the taskbar).
        {
            static bool prevLeft = false;
            bool left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (TeEndButtonVisible() && left && !prevLeft) {
                POINT pt;
                GetCursorPos(&pt);
                if (!TeEndButtonHitTest(pt)) {
                    TeHideEndButton();
                    TeClearTarget(st.current);
                }
            }
            prevLeft = left;
        }

        // Hover cache (light identify), skip while button is up.
        if (!TeEndButtonVisible()) {
            POINT pt;
            GetCursorPos(&pt);
            if (OverTaskbar(pt)) {
                static DWORD last = 0;
                DWORD now = GetTickCount();
                if (now - last > 500) {
                    last = now;
                    TeTarget tmp;
                    TeIdentifySafe(pt, false, tmp);
                    if (tmp.pid || tmp.closeOnly) {
                        st.hover = tmp;
                        st.hover.pt = pt;
                    } else if (st.hover.pid && !TeSameIconSlot(pt, st.hover.pt)) {
                        TeClearTarget(st.hover);
                    }
                }
            }
        }
    }

    TeRemoveInputHooks();
    TeHideEndButton();
    TeShutdownUia();
    TeLog(L"worker exit");
    return 0;
}

static DWORD WINAPI UnloadProc(LPVOID)
{
    WaitForSingleObject(TeState().unloadEvent, INFINITE);
    TeLog(L"unload signaled");
    TeState().running = false;
    if (TeState().worker) {
        PostThreadMessageW(GetThreadId(TeState().worker), WM_QUIT, 0, 0);
        WaitForSingleObject(TeState().worker, 5000);
    }
    TeRemoveInputHooks();
    TeHideEndButton();
    HMODULE mod = TeState().module;
    CloseHandle(TeState().unloadEvent);
    TeState().unloadEvent = nullptr;
    FreeLibraryAndExitThread(mod, 0);
    return 0;
}

bool TeStartWorker(HMODULE self)
{
    auto& st = TeState();
    st.module = self;
    st.running = true;
    st.unloadEvent = CreateEventW(nullptr, TRUE, FALSE, L"Global\\TaskbarEnd_Unload");
    if (!st.unloadEvent) return false;
    // If event already existed from a previous load, reset it.
    ResetEvent(st.unloadEvent);

    st.worker = CreateThread(nullptr, 0, WorkerProc, nullptr, 0, nullptr);
    g_unloadThread = CreateThread(nullptr, 0, UnloadProc, nullptr, 0, nullptr);
    return st.worker != nullptr;
}

void TeStopWorker()
{
    auto& st = TeState();
    st.running = false;
    if (st.unloadEvent) SetEvent(st.unloadEvent);
}

HANDLE TeWorkerThread()
{
    return TeState().worker;
}
