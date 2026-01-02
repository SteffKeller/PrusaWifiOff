/**
 * @file ButtonMode.cpp
 * @brief Implementation of button click detection with debouncing
 */

#include <Arduino.h>
#include "ButtonMode.h"

ModeClickEvent chkModeButton() {
  static bool  lastStable     = HIGH;          // Last stable button state
  static bool  lastRaw        = HIGH;          // Last raw reading for debounce
  static unsigned long lastDebounce = 0;       // Timestamp for debounce

  static uint8_t  clickCount        = 0;       // Number of clicks detected
  static unsigned long firstReleaseTime = 0;   // Timestamp of first click release

  unsigned long now = millis();
  bool reading = digitalRead(INPUT_PIN_MODE);

  // Debouncing - start debounce timer on any change
  if (reading != lastRaw) {
    lastDebounce = now;
    lastRaw = reading;
  }
  if (now - lastDebounce < DEBOUNCE_MS) {
    return ModeNone;
  }

  // Detect stable edge transition
  if (reading != lastStable) {
    lastStable = reading;

    // Only button release (HIGH) counts as a click
    if (reading == HIGH) {
      clickCount++;
      if (clickCount == 1) {
        firstReleaseTime = now;
      } else if (clickCount == 2) {
        if (now - firstReleaseTime <= DOUBLE_CLICK_MS) {
          clickCount = 0;
          return ModeDoubleClick;
        } else {
          // Too slow - treat as new first click
          clickCount = 1;
          firstReleaseTime = now;
        }
      }
    }
  }

  // Single-click when double-click window expires with only 1 click
  if (clickCount == 1 && (now - firstReleaseTime > DOUBLE_CLICK_MS)) {
    clickCount = 0;
    return ModeSingleClick;
  }

  return ModeNone;
}
