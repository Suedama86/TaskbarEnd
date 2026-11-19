#include <windows.h>
#include "worker.h"
#include "log.h"

static bool IsExplorer()
{
    wchar_t path[MAX_PATH] = L"";
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* n = wcsrchr(path, L'\\');
    return _wcsicmp(n ? n + 1 : path, L"explorer.exe") == 0;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(mod);
        if (!IsExplorer()) return TRUE;
        TeLogInit();
        TeLog(L"==============================================");
        TeLog(L"TaskbarEndHook loaded");
        TeStartWorker(mod);
    } else if (reason == DLL_PROCESS_DETACH) {
        TeLog(L"TaskbarEndHook detach");
    }
    return TRUE;
}
