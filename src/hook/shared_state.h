#pragma once
#include <windows.h>
#include "target.h"

inline constexpr DWORD TE_TARGET_TIMEOUT_MS = 8000;
inline constexpr DWORD TE_HOVER_TIMEOUT_MS = 15000;
inline constexpr int TE_SLOT_PX = 48;

struct TeRuntime {
    HMODULE module = nullptr;
    HANDLE worker = nullptr;
    HANDLE unloadEvent = nullptr;
    volatile bool running = false;
    TeTarget current = {};
    TeTarget hover = {};
    POINT clickPt = {};
    void (*killFn)() = nullptr;
};

TeRuntime& TeState();
