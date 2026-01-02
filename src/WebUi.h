/**
 * @file WebUi.h
 * @brief Web interface declarations for HTTP control and monitoring
 * 
 * Provides embedded web UI with Bootstrap styling, live AJAX status updates,
 * and REST API endpoints for control and configuration.
 */

#pragma once
#include <WebServer.h>
#include <Preferences.h>

// Global state variables from main.cpp
extern WebServer server;          ///< HTTP server instance
extern bool autoPowerOffEnabled;  ///< Auto power-off mode enabled
extern bool offTimerRunning;      ///< Timer countdown active
extern bool reportValid;          ///< Relay report data is valid
extern bool reportRelay;          ///< Relay state from report
extern float reportPower;         ///< Current power in watts
extern float reportWs;            ///< Watt-seconds measurement
extern float reportTemperature;   ///< Device temperature
extern String reportBootId;       ///< Relay device boot ID
extern float reportEnergyBoot;    ///< Energy since boot
extern uint32_t reportTimeBoot;   ///< Time since boot (seconds)
extern uint32_t offDelayMs;       ///< Auto-off delay in milliseconds
extern uint32_t offTimerStart;    ///< Timer start timestamp
extern Preferences prefs;         ///< NVS preferences storage
extern String relayIpAddress;     ///< Configurable relay IP address

/**
 * @brief Initialize and start the HTTP web server
 * @note Registers all API endpoints and serves HTML UI
 * @note API endpoints:
 *   - GET / - Main HTML page
 *   - GET /api/status - JSON status (auto mode, timer, relay state, etc.)
 *   - GET /api/mode - Toggle auto power-off mode
 *   - GET /api/off_now - Power off relay immediately
 *   - GET /api/on_now - Power on relay immediately
 *   - GET /api/toggle - Toggle relay state
 *   - GET /api/set_timer?minutes=N - Set auto-off delay (1-240 minutes)
 *   - GET /api/set_relay_ip?ip=X.X.X.X - Set relay IP address
 */
void startWebServer();
