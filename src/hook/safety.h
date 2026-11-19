#pragma once
#include <windows.h>

bool TeIsProtectedProcessName(const wchar_t* exe);
bool TeIsProtectedPid(DWORD pid);
bool TeIsFrameHostProcessName(const wchar_t* exe);
bool TeIsShellChromeHwnd(HWND hwnd);
bool TeIsExplorerHostedClass(const wchar_t* cls);
bool TeGetProcessExeName(DWORD pid, wchar_t* exe, size_t cch);
