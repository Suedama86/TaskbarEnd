#pragma once
#include <windows.h>

void TeEnsureUia();
void TeShutdownUia();

struct TeTarget {
    DWORD pid = 0;
    HWND hwnd = nullptr;
    wchar_t name[256] = {};
    DWORD tick = 0;
    POINT pt = {};
    bool closeOnly = false; // WM_CLOSE only (never TerminateProcess)
};

void TeClearTarget(TeTarget& t);
bool TeTargetFresh(const TeTarget& t, DWORD timeoutMs);
bool TeSameIconSlot(POINT a, POINT b);

// heavy=true: right-click / kill path. heavy=false: hover cache (lighter).
void TeIdentifyAtPoint(POINT pt, bool heavy, TeTarget& out);

// SEH wrapper — UIA can fault under shell injectors.
void TeIdentifySafe(POINT pt, bool heavy, TeTarget& out);
