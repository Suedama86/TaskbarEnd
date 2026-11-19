#include "end_button.h"
#include "shared_state.h"
#include "log.h"
#include <cwchar>

static HWND g_btn = nullptr;
static HFONT g_font = nullptr;
static void (*g_onClick)() = nullptr;
static HMODULE g_mod = nullptr;
static const wchar_t* kClass = L"TaskbarEnd_Btn";
static constexpr int kW = 168;
static constexpr int kH = 32;

const wchar_t* TeEndTaskLabel()
{
    wchar_t locale[16] = L"";
    if (GetUserDefaultLocaleName(locale, 16) > 0 && _wcsnicmp(locale, L"sv", 2) == 0)
        return L"Avsluta aktivitet";
    return L"End task";
}

static LRESULT CALLBACK BtnProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(196, 43, 28));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        FrameRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        HFONT old = (HFONT)SelectObject(hdc, g_font ? g_font : GetStockObject(DEFAULT_GUI_FONT));
        DrawTextW(hdc, TeEndTaskLabel(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONUP:
        TeHideEndButton();
        if (g_onClick) g_onClick();
        return 0;
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        TeHideEndButton();
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            TeHideEndButton();
            KillTimer(hwnd, 1);
        }
        return 0;
    case WM_DESTROY:
        if (g_btn == hwnd) g_btn = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void TeHideEndButton()
{
    if (g_btn) {
        DestroyWindow(g_btn);
        g_btn = nullptr;
    }
}

bool TeEndButtonVisible()
{
    return g_btn != nullptr;
}

bool TeEndButtonHitTest(POINT pt)
{
    if (!g_btn || !IsWindow(g_btn)) return false;
    RECT rc = {};
    if (!GetWindowRect(g_btn, &rc)) return false;
    return PtInRect(&rc, pt) != FALSE;
}

void TeShowEndButton(POINT pt, const TeTarget& target, void (*onClick)())
{
    TeHideEndButton();
    g_onClick = onClick;
    g_mod = (HMODULE)GetModuleHandleW(L"TaskbarEndHook.dll");
    if (!g_mod) g_mod = GetModuleHandleW(nullptr);

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = BtnProc;
        wc.hInstance = g_mod;
        wc.lpszClassName = kClass;
        wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
        RegisterClassExW(&wc);
        registered = true;
    }
    if (!g_font) {
        g_font = CreateFontW(15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    }

    int x = pt.x - kW - 24;
    int y = pt.y - 200;
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    if (GetMonitorInfoW(mon, &mi)) {
        if (x < mi.rcWork.left) x = pt.x + 16;
        if (x + kW > mi.rcWork.right) x = mi.rcWork.right - kW - 4;
        if (y < mi.rcWork.top) y = mi.rcWork.top + 8;
        if (y + kH > mi.rcWork.bottom) y = mi.rcWork.bottom - kH - 4;
    }

    g_btn = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClass, TeEndTaskLabel(),
        WS_POPUP | WS_VISIBLE,
        x, y, kW, kH,
        nullptr, nullptr, g_mod, nullptr);
    if (g_btn) {
        SetWindowPos(g_btn, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        SetTimer(g_btn, 1, TE_TARGET_TIMEOUT_MS, nullptr);
        TeLog(L"button @(%d,%d) pid=%lu", x, y, target.pid);
    }
}
