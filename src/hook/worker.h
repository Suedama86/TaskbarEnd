#pragma once
#include <windows.h>

#define TE_WM_SHOW_END (WM_APP + 0x71)
#define TE_WM_DISMISS  (WM_APP + 0x72)

bool TeStartWorker(HMODULE self);
void TeStopWorker();
HANDLE TeWorkerThread();
