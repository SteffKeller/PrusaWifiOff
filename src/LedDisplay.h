#pragma once
#include <M5Atom.h>

void clearMatrix();
void drawI(uint32_t col);
void showAutoOffEnabledBase();   // blaues X (Auto ON)
void showAutoOffDisabled();      // grünes I (Auto OFF)
void drawProgressBar(uint8_t filledRows);
