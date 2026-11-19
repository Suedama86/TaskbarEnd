#pragma once
#include <windows.h>

// Install lightweight hooks only. Never run UIA here.
bool TeInstallInputHooks(DWORD trayThreadId, HANDLE workerThread);
void TeRemoveInputHooks();
