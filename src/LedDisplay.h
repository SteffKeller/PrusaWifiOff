/**
 * @file LedDisplay.h
 * @brief LED matrix display patterns for M5Stack Atom 5x5 RGB matrix
 * 
 * Provides visual feedback patterns:
 * - Green "I" (vertical line) = Auto-off DISABLED
 * - Blue "X" (diagonal cross) = Auto-off ENABLED
 * - Orange progress bar (bottom-up) = Timer countdown active
 * - Red "I" = Power-off command sent
 */

#pragma once
#include <M5Atom.h>

/**
 * @brief Clear entire LED matrix to black
 */
void clearMatrix();

/**
 * @brief Draw vertical "I" pattern in center column
 * @param col RGB color value (0xRRGGBB format)
 */
void drawI(uint32_t col);

/**
 * @brief Display blue "X" pattern (auto-off enabled)
 * @note Shows diagonal cross indicating auto power-off mode is active
 */
void showAutoOffEnabledBase();

/**
 * @brief Display red "X" pattern (auto-off enabled, relay OFF)
 * @note Shows red diagonal cross when auto power-off is enabled but relay is off
 */
void showAutoOffEnabledRed();

/**
 * @brief Display green "I" pattern (auto-off disabled)
 * @note Shows vertical line indicating auto power-off mode is inactive
 */
void showAutoOffDisabled();

/**
 * @brief Display red "I" pattern (auto-off disabled, relay OFF)
 * @note Shows red vertical line when auto power-off is disabled and relay is off
 */
void showAutoOffDisabledRed();

/**
 * @brief Draw orange progress bar from bottom up with blue X overlay
 * @param filledRows Number of rows to fill (0-5), fills from bottom
 * @note Used during timer countdown to show remaining time visually
 */
void drawProgressBar(uint8_t filledRows);
