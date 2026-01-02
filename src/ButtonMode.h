/**
 * @file ButtonMode.h
 * @brief Button input handling with debounce and click detection
 * 
 * Provides single and double-click detection on GPIO 39 with configurable
 * debounce timing and double-click window.
 */

#ifndef BUTTONMODE_H
#pragma once
#include <Arduino.h>

constexpr int INPUT_PIN_MODE = 39;         ///< GPIO pin for mode button input
constexpr uint32_t DEBOUNCE_MS = 60;       ///< Debounce time in milliseconds
constexpr uint32_t DOUBLE_CLICK_MS = 250;  ///< Double-click detection window in ms

/**
 * @brief Button click event types
 */
enum ModeClickEvent {
  ModeNone,          ///< No click detected
  ModeSingleClick,   ///< Single click detected
  ModeDoubleClick    ///< Double click detected
};

/**
 * @brief Check mode button for click events
 * @return ModeClickEvent indicating detected click type
 * @note Call repeatedly in loop(), maintains internal state for debouncing
 * @note Only button release (rising edge to HIGH) counts as a click
 */
ModeClickEvent chkModeButton();
#endif // BUTTONMODE_H