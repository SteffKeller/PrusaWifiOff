#include <Arduino.h>
#include "ButtonMode.h"

ModeClickEvent chkModeButton() {
  static bool  lastStable     = HIGH;
  static bool  lastRaw        = HIGH;
  static unsigned long lastDebounce = 0;

  static uint8_t  clickCount        = 0;
  static unsigned long firstReleaseTime = 0;

  unsigned long now = millis();
  bool reading = digitalRead(INPUT_PIN_MODE);

  // Entprellung
  if (reading != lastRaw) {
    lastDebounce = now;
    lastRaw = reading;
  }
  if (now - lastDebounce < DEBOUNCE_MS) {
    return ModeNone;
  }

  // Flanke auf stabilen Pegel
  if (reading != lastStable) {
    lastStable = reading;

    // nur Release (HIGH) zählt als Klick
    if (reading == HIGH) {
      clickCount++;
      if (clickCount == 1) {
        firstReleaseTime = now;
      } else if (clickCount == 2) {
        if (now - firstReleaseTime <= DOUBLE_CLICK_MS) {
          clickCount = 0;
          return ModeDoubleClick;
        } else {
          // zu langsam -> als neuer erster Klick
          clickCount = 1;
          firstReleaseTime = now;
        }
      }
    }
  }

  // Single-Click, wenn Fenster vorbei und nur 1 Klick
  if (clickCount == 1 && (now - firstReleaseTime > DOUBLE_CLICK_MS)) {
    clickCount = 0;
    return ModeSingleClick;
  }

  return ModeNone;
}
