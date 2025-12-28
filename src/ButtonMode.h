#ifndef BUTTONMODE_H
#pragma once
#include <Arduino.h>

constexpr int INPUT_PIN_MODE = 39;
constexpr uint32_t DEBOUNCE_MS     = 60;
constexpr uint32_t DOUBLE_CLICK_MS = 250;

enum ModeClickEvent {
  ModeNone,
  ModeSingleClick,
  ModeDoubleClick
};

ModeClickEvent chkModeButton();
#endif // BUTTONMODE_H