#include "target.h"
#include "shared_state.h"
#include "safety.h"
#include "log.h"
#include <UIAutomation.h>
#include <OleAcc.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <cwctype>

#pragma comment(lib, "oleacc.lib")

static IUIAutomation* g_uia = nullptr;

void TeEnsureUia()
{
    if (g_uia) return;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
        IID_IUIAutomation, (void**)&g_uia);
}

void TeShutdownUia()
{
    if (g_uia) { g_uia->Release(); g_uia = nullptr; }
    CoUninitialize();
}

void TeClearTarget(TeTarget& t)
{
    t = {};
}

bool TeTargetFresh(const TeTarget& t, DWORD timeoutMs)
{
    if (!(t.pid || t.hwnd)) return false;
    return (GetTickCount() - t.tick) <= timeoutMs;
}

bool TeSameIconSlot(POINT a, POINT b)
{
    return abs((int)(a.x - b.x)) <= TE_SLOT_PX && abs((int)(a.y - b.y)) <= 64;
}

static bool WeakName(const wchar_t* name)
{
    if (!name || !name[0]) return true;
    if (StrStrIW(name, L"Running applications") || StrStrIW(name, L"Pinned") ||
        StrStrIW(name, L"Task View") || StrStrIW(name, L"Show desktop") ||
        StrStrIW(name, L"Taskbar") || StrStrIW(name, L"Cortana") ||
        StrStrIW(name, L"Type here to search") ||
        StrStrIW(name, L"xla mellan") || StrStrIW(name, L"rogram som k") ||
        StrStrIW(name, L"ktiva program") || StrStrIW(name, L"isa skrivbord") ||
        StrStrIW(name, L"ktivitetsf") || StrStrIW(name, L"kriv h") ||
        StrStrIW(name, L"eddelandef") || StrStrIW(name, L"Notification") ||
        _wcsicmp(name, L"Start") == 0)
        return true;

    auto desktop = [](const wchar_t* n, const wchar_t* prefix) {
        size_t len = wcslen(prefix);
        if (_wcsnicmp(n, prefix, len) != 0) return false;
        const wchar_t* p = n + len;
        while (*p == L' ') ++p;
        if (!*p) return true;
        if (!iswdigit(*p)) return false;
        while (iswdigit(*p)) ++p;
        while (*p == L' ') ++p;
        return !*p;
    };
    if (desktop(name, L"Skrivbord") || desktop(name, L"Desktop"))
        return true;

    const wchar_t* p = name;
    while (*p == L' ') ++p;
    if (!iswdigit(*p)) return false;
    while (iswdigit(*p)) ++p;
    while (*p == L' ') ++p;
    return StrStrIW(p, L"aktivt") == p || StrStrIW(p, L"running") == p ||
           StrStrIW(p, L"window") == p;
}

static void NormalizeName(wchar_t* name)
{
    static const wchar_t* prefixes[] = {
        L"Snabblista för ", L"Jump List for ", L"Jump list for ",
    };
    for (auto* pref : prefixes) {
        size_t n = wcslen(pref);
        if (_wcsnicmp(name, pref, n) == 0) {
            wchar_t tmp[256];
            wcsncpy_s(tmp, name + n, _TRUNCATE);
            wcscpy_s(name, 256, tmp);
            break;
        }
    }
    wchar_t* dash = wcsrchr(name, L'-');
    if (dash && dash > name) {
        const wchar_t* after = dash + 1;
        while (*after == L' ') ++after;
        if (iswdigit(*after) &&
            (StrStrIW(after, L"fönster") || StrStrIW(after, L"window") ||
             StrStrIW(after, L"körs") || StrStrIW(after, L"running"))) {
            while (dash > name && *(dash - 1) == L' ') --dash;
            *dash = L'\0';
        }
    }
    dash = wcsrchr(name, L'-');
    if (dash && dash > name) {
        const wchar_t* app = dash + 1;
        while (*app == L' ') ++app;
        if (app[0] && !WeakName(app)) {
            wchar_t tmp[256];
            wcsncpy_s(tmp, app, _TRUNCATE);
            wcscpy_s(name, 256, tmp);
        }
    }
}

static void BestToken(const wchar_t* src, wchar_t* dst, int max)
{
    dst[0] = L'\0';
    size_t best = 0;
    const wchar_t* p = src;
    while (*p) {
        while (*p && (iswspace(*p) || wcschr(L"-+_.,;()[]{}", *p))) ++p;
        const wchar_t* start = p;
        while (*p && !iswspace(*p) && !wcschr(L"-+_.,;()[]{}", *p)) ++p;
        size_t len = (size_t)(p - start);
        if (len < 4 || len >= (size_t)max || len <= best) continue;
        wchar_t tok[128] = L"";
        if (len >= _countof(tok)) continue;
        wcsncpy_s(tok, start, len);
        if (_wcsicmp(tok, L"Taskbar") == 0 || _wcsicmp(tok, L"Window") == 0 ||
            _wcsicmp(tok, L"Windows") == 0 || _wcsicmp(tok, L"Microsoft") == 0 ||
            _wcsicmp(tok, L"Application") == 0 || _wcsicmp(tok, L"Program") == 0 ||
            _wcsicmp(tok, L"Launcher") == 0 || StrStrIW(tok, L"fönster") ||
            StrStrIW(tok, L"aktiv") || _wcsicmp(tok, L"Skrivbord") == 0 ||
            _wcsicmp(tok, L"Desktop") == 0)
            continue;
        wcsncpy_s(dst, max, tok, _TRUNCATE);
        best = len;
    }
}

static bool NameFromOverlay(POINT pt, wchar_t* out, size_t cch)
{
    TeEnsureUia();
    HWND best = nullptr;
    LONG bestDist = LONG_MAX;
    HWND overlay = nullptr;
    while ((overlay = FindWindowExW(nullptr, overlay, L"TaskListOverlayWnd", nullptr))) {
        if (!IsWindowVisible(overlay)) continue;
        RECT rc = {};
        GetWindowRect(overlay, &rc);
        LONG dist = abs((int)(((rc.left + rc.right) / 2) - pt.x));
        if (dist < bestDist && dist <= 80) {
            bestDist = dist;
            best = overlay;
        }
    }
    if (!best || !g_uia) return false;

    RECT rc = {};
    GetWindowRect(best, &rc);
    const POINT samples[] = {
        { (rc.left + rc.right) / 2, rc.top + 12 },
        { (rc.left + rc.right) / 2, rc.top + 28 },
        { (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2 },
    };
    for (auto s : samples) {
        IUIAutomationElement* el = nullptr;
        if (g_uia->ElementFromPoint(s, &el) != S_OK || !el) continue;
        BSTR n = nullptr;
        if (el->get_CurrentName(&n) == S_OK && n && SysStringLen(n) > 0 && !WeakName(n)) {
            wcsncpy_s(out, cch, n, _TRUNCATE);
            SysFreeString(n);
            el->Release();
            TeLog(L"overlay name='%s'", out);
            return true;
        }
        if (n) SysFreeString(n);
        el->Release();
    }
    return false;
}

static bool NameFromMsaa(POINT pt, wchar_t* out, size_t cch)
{
    IAccessible* acc = nullptr;
    VARIANT var = { VT_I4 };
    if (AccessibleObjectFromPoint(pt, &acc, &var) != S_OK || !acc) return false;
    bool ok = false;
    IAccessible* cur = acc;
    for (int d = 0; d < 8; ++d) {
        VARIANT id = { VT_I4 };
        id.lVal = (d == 0) ? var.lVal : CHILDID_SELF;
        BSTR name = nullptr;
        if (cur->get_accName(id, &name) == S_OK && name && SysStringLen(name) > 0 && !WeakName(name)) {
            wcsncpy_s(out, cch, name, _TRUNCATE);
            ok = true;
            SysFreeString(name);
            break;
        }
        if (name) SysFreeString(name);
        IDispatch* parentDisp = nullptr;
        if (cur->get_accParent(&parentDisp) != S_OK || !parentDisp) break;
        IAccessible* parent = nullptr;
        if (parentDisp->QueryInterface(IID_IAccessible, (void**)&parent) != S_OK || !parent) {
            parentDisp->Release();
            break;
        }
        parentDisp->Release();
        if (cur != acc) cur->Release();
        cur = parent;
    }
    if (cur != acc) cur->Release();
    acc->Release();
    return ok;
}

static DWORD FindUwpPidByTitle(const wchar_t* title, DWORD skipPid)
{
    if (!title || !title[0]) return 0;
    struct Ctx { const wchar_t* title; DWORD skip; DWORD best; } ctx = { title, skipPid, 0 };
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        auto* c = (Ctx*)lp;
        wchar_t t[512] = L"";
        GetWindowTextW(hwnd, t, 512);
        if (!t[0] || !(StrStrIW(t, c->title) || StrStrIW(c->title, t))) return TRUE;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (!pid || pid == c->skip || pid == GetCurrentProcessId()) return TRUE;
        wchar_t exe[128] = L"";
        TeGetProcessExeName(pid, exe, _countof(exe));
        if (TeIsProtectedProcessName(exe) || TeIsFrameHostProcessName(exe)) return TRUE;
        wchar_t cls[128] = L"";
        GetClassNameW(hwnd, cls, 128);
        c->best = pid;
        if (StrStrIW(cls, L"CoreWindow")) return FALSE;
        return TRUE;
    }, (LPARAM)&ctx);
    return ctx.best;
}

static DWORD ResolveAppPid(HWND hwnd, DWORD fallback, const wchar_t* title)
{
    if (hwnd) {
        DWORD root = 0;
        GetWindowThreadProcessId(hwnd, &root);
        struct Child { DWORD root; DWORD best; } ch = { root, 0 };
        EnumChildWindows(hwnd, [](HWND child, LPARAM lp) -> BOOL {
            auto* s = (Child*)lp;
            DWORD pid = 0;
            GetWindowThreadProcessId(child, &pid);
            if (!pid || pid == s->root) return TRUE;
            wchar_t exe[128] = L"";
            TeGetProcessExeName(pid, exe, _countof(exe));
            if (TeIsProtectedProcessName(exe) || TeIsFrameHostProcessName(exe)) return TRUE;
            s->best = pid;
            return FALSE;
        }, (LPARAM)&ch);
        if (ch.best) return ch.best;

        wchar_t exe[128] = L"";
        if (TeGetProcessExeName(root, exe, _countof(exe)) && TeIsFrameHostProcessName(exe)) {
            wchar_t wt[512] = L"";
            if (title && title[0]) wcsncpy_s(wt, title, _TRUNCATE);
            else GetWindowTextW(hwnd, wt, 512);
            DWORD uwp = FindUwpPidByTitle(wt, root);
            if (uwp) return uwp;
        }
        if (root) return root;
    }
    if (fallback) {
        wchar_t exe[128] = L"";
        if (TeGetProcessExeName(fallback, exe, _countof(exe)) && TeIsFrameHostProcessName(exe)) {
            DWORD uwp = FindUwpPidByTitle(title, fallback);
            if (uwp) return uwp;
        }
    }
    return fallback;
}

struct MatchCtx {
    const wchar_t* name;
    const wchar_t* token;
    DWORD selfPid;
    HWND hwnd = nullptr;
    DWORD pid = 0;
    HWND shellHwnd = nullptr;
    DWORD shellPid = 0;
};

static bool TitleHits(const wchar_t* title, const wchar_t* cls, MatchCtx* m)
{
    if (title[0] && (StrStrIW(title, m->name) || StrStrIW(m->name, title))) return true;
    if (cls && cls[0] && (StrStrIW(cls, m->name) || StrStrIW(m->name, cls))) return true;
    if (m->token && m->token[0] &&
        ((title[0] && StrStrIW(title, m->token)) || (cls && cls[0] && StrStrIW(cls, m->token))))
        return true;
    return false;
}

static BOOL CALLBACK EnumMatch(HWND hwnd, LPARAM lp)
{
    auto* m = (MatchCtx*)lp;
    wchar_t title[512] = L"";
    GetWindowTextW(hwnd, title, 512);
    if (!title[0]) return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return TRUE;
    wchar_t cls[256] = L"";
    GetClassNameW(hwnd, cls, 256);
    if (!TitleHits(title, cls, m)) return TRUE;

    wchar_t exe[128] = L"";
    TeGetProcessExeName(pid, exe, _countof(exe));

    if (_wcsicmp(exe, L"explorer.exe") == 0 || pid == m->selfPid) {
        if (TeIsExplorerHostedClass(cls) && !m->shellHwnd) {
            m->shellHwnd = hwnd;
            m->shellPid = pid;
        }
        return TRUE;
    }
    if (TeIsFrameHostProcessName(exe)) {
        if (!m->hwnd) { m->hwnd = hwnd; m->pid = pid; }
        return TRUE;
    }
    if (TeIsProtectedProcessName(exe)) return TRUE;

    m->hwnd = hwnd;
    m->pid = pid;
    return FALSE;
}

void TeIdentifyAtPoint(POINT pt, bool /*heavy*/, TeTarget& out)
{
    TeClearTarget(out);
    wchar_t name[256] = L"";

    if (!NameFromOverlay(pt, name, _countof(name)))
        NameFromMsaa(pt, name, _countof(name));

    if (!name[0] || WeakName(name)) {
        out.tick = GetTickCount();
        out.pt = pt;
        return;
    }

    NormalizeName(name);
    if (WeakName(name)) {
        out.tick = GetTickCount();
        out.pt = pt;
        return;
    }

    wcsncpy_s(out.name, name, _TRUNCATE);
    wchar_t token[128] = L"";
    BestToken(name, token, 128);

    MatchCtx m = { out.name, token[0] ? token : nullptr, GetCurrentProcessId() };
    EnumWindows(EnumMatch, (LPARAM)&m);

    if (m.hwnd && m.pid) {
        out.hwnd = m.hwnd;
        out.pid = ResolveAppPid(m.hwnd, m.pid, out.name);
        out.closeOnly = false;
    } else if (m.shellHwnd) {
        out.hwnd = m.shellHwnd;
        out.pid = m.shellPid;
        out.closeOnly = true;
    }

    if (!out.pid && !out.closeOnly) {
        HWND exact = FindWindowW(nullptr, name);
        if (exact) {
            DWORD pid = 0;
            GetWindowThreadProcessId(exact, &pid);
            wchar_t exe[128] = L"";
            TeGetProcessExeName(pid, exe, _countof(exe));
            if (_wcsicmp(exe, L"explorer.exe") == 0) {
                wchar_t cls[64] = L"";
                GetClassNameW(exact, cls, 64);
                if (TeIsExplorerHostedClass(cls)) {
                    out.hwnd = exact;
                    out.pid = pid;
                    out.closeOnly = true;
                }
            } else if (pid && !TeIsProtectedProcessName(exe)) {
                out.hwnd = exact;
                out.pid = ResolveAppPid(exact, pid, name);
            }
        }
    }

    if (!out.closeOnly && out.pid) {
        wchar_t exe[128] = L"";
        if (TeGetProcessExeName(out.pid, exe, _countof(exe)) && _wcsicmp(exe, L"explorer.exe") == 0) {
            if (out.hwnd) {
                wchar_t cls[64] = L"";
                GetClassNameW(out.hwnd, cls, 64);
                if (TeIsExplorerHostedClass(cls)) out.closeOnly = true;
                else TeClearTarget(out);
            } else {
                TeClearTarget(out);
            }
        }
    }

    if (!out.closeOnly && (out.pid == GetCurrentProcessId() || TeIsProtectedPid(out.pid)))
        TeClearTarget(out);

    out.tick = GetTickCount();
    out.pt = pt;
    TeLog(L"target '%s' hwnd=%p pid=%lu closeOnly=%d",
        out.name, out.hwnd, out.pid, (int)out.closeOnly);
}

void TeIdentifySafe(POINT pt, bool heavy, TeTarget& out)
{
    __try {
        TeIdentifyAtPoint(pt, heavy, out);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        TeLog(L"identify fault 0x%08X", GetExceptionCode());
        TeClearTarget(out);
    }
}
