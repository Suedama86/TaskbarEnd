#include "log.h"
#include <cstdio>
#include <cwchar>
#include <cstdarg>

static CRITICAL_SECTION g_logCs;
static bool g_logReady = false;

void TeLogInit()
{
    if (g_logReady) return;
    InitializeCriticalSection(&g_logCs);
    g_logReady = true;
}

void TeLog(const wchar_t* fmt, ...)
{
    if (!g_logReady) TeLogInit();

    wchar_t line[1024];
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    int n = swprintf_s(line, L"[%02d:%02d:%02d.%03d] ",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    if (n < 0) return;

    va_list ap;
    va_start(ap, fmt);
    vswprintf_s(line + n, _countof(line) - n, fmt, ap);
    va_end(ap);

    EnterCriticalSection(&g_logCs);
    wchar_t path[MAX_PATH] = L"";
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"TaskbarEnd.log");
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, line, (DWORD)(wcslen(line) * sizeof(wchar_t)), &written, nullptr);
        WriteFile(h, L"\r\n", 4, &written, nullptr);
        CloseHandle(h);
    }
    LeaveCriticalSection(&g_logCs);
}
