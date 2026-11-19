#pragma once
#include <windows.h>
#include "target.h"

void TeShowEndButton(POINT pt, const TeTarget& target, void (*onClick)());
void TeHideEndButton();
bool TeEndButtonVisible();
bool TeEndButtonHitTest(POINT pt);
const wchar_t* TeEndTaskLabel();
