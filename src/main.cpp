/**
 * @file main.cpp
 * @brief M5Stack Atom relay controller with automatic power-off functionality
 * 
 * Monitors an external signal on GPIO 23 (printer status) and automatically
 * powers off an HTTP-controlled relay after a configurable delay when the
 * signal goes low. Features web UI control, LED status display, and button input.
 * 
 * @author Steff8583
 * @date 02.01.2026
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <M5Atom.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiManager.h>
#include "LedDisplay.h"
#include "ButtonMode.h"
#include "WebUi.h"

WiFiManager wifiManager; ///< WiFiManager for captive portal configuration

constexpr int INPUT_PIN = 23; ///< GPIO pin for external signal monitoring (printer status)

String relayIpAddress = "192.168.188.44"; ///< Configurable relay device IP address (stored in NVS)

/**
 * @brief Build URL for relay toggle endpoint
 * @return Complete URL string for toggling relay state
 */
String getUrlToggle() { return "http://" + relayIpAddress + "/toggle"; }

/**
 * @brief Build URL for relay OFF command
 * @return Complete URL string for turning relay off
 */
String getUrlOff()    { return "http://" + relayIpAddress + "/relay?state=0"; }

/**
 * @brief Build URL for relay ON command
 * @return Complete URL string for turning relay on
 */
String getUrlOn()     { return "http://" + relayIpAddress + "/relay?state=1"; }

/**
 * @brief Build URL for relay status report endpoint
 * @return Complete URL string for fetching relay JSON status
 */
String getUrlReport() { return "http://" + relayIpAddress + "/report"; }

WebServer server(80); ///< HTTP server on port 80 for web UI

// Input signal debouncing state
bool     lastState        = HIGH;      ///< Last stable state of INPUT_PIN
uint32_t lastChangeMs     = 0;         ///< Timestamp of last state change

// Auto power-off timer state
bool     autoPowerOffEnabled = false;  ///< Auto power-off mode enabled flag
bool     offTimerRunning     = false;  ///< Timer countdown active flag
uint32_t offTimerStart       = 0;      ///< Timestamp when timer started

// Relay status report from HTTP polling
bool     reportValid       = false;    ///< Report data is valid and up-to-date
bool     reportRelay       = false;    ///< Relay state (ON/OFF) from report
float    reportPower       = 0.0f;     ///< Current power consumption in watts
float    reportWs          = 0.0f;     ///< Watt-seconds energy measurement
float    reportTemperature = 0.0f;     ///< Device temperature in Celsius
String   reportBootId      = "";       ///< Unique boot ID from relay device
float    reportEnergyBoot  = 0.0f;     ///< Energy consumed since boot
uint32_t reportTimeBoot    = 0;        ///< Time since boot in seconds

uint32_t lastReportPollMs  = 0;        ///< Timestamp of last status poll
constexpr uint32_t REPORT_POLL_INTERVAL_MS = 5000; ///< Poll relay every 5 seconds

Preferences prefs;                     ///< ESP32 NVS preferences storage
uint32_t offDelayMs = 10UL * 60UL * 1000UL; ///< Auto-off delay (default 10 minutes)

// Forward declarations
void ensureWifi();
void sendGet(const String& url);
void sendOff();
void sendOn();
void sendToggle();
void updateReportStatus();

/**
 * @brief Send HTTP GET request to relay device
 * @param url Complete URL string to fetch
 * @note Only sends if WiFi is connected, logs HTTP response code to Serial
 */
void sendGet(const String& url) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(url);
  http.setTimeout(2000); // 2 second timeout
  int code = http.GET();
  Serial.printf("GET %s -> HTTP %d\n", url.c_str(), code);
  http.end();
}

/**
 * @brief Send relay OFF command
 */
void sendOff()    { sendGet(getUrlOff());    }

/**
 * @brief Send relay ON command
 */
void sendOn()     { sendGet(getUrlOn());     }

/**
 * @brief Send relay toggle command
 */
void sendToggle() { sendGet(getUrlToggle()); }

/**
 * @brief Ensure WiFi connection is active, reconnect if needed
 * @note Uses WiFiManager to handle connection and configuration
 */
void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("WiFi disconnected, attempting reconnect...");
  WiFi.mode(WIFI_STA);
  WiFi.reconnect();

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi reconnected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi reconnect failed. Reset device or press button to reconfigure.");
  }
}

/**
 * @brief Poll relay device for status report via HTTP GET
 * @note Fetches JSON from /report endpoint, updates global report* variables
 * @note Sets reportValid to false on connection or parsing errors
 */
void updateReportStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    reportValid = false;
    return;
  }

  HTTPClient http;
  http.begin(getUrlReport());
  http.setTimeout(2000); // 2 second timeout
  int code = http.GET();
  if (code != 200) {
    Serial.printf("REPORT GET -> HTTP %d\n", code);
    http.end();
    reportValid = false;
    return;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("REPORT JSON parse failed: ");
    Serial.println(err.c_str());
    reportValid = false;
    return;
  }

  reportPower       = doc["power"] | 0.0f;
  reportWs          = doc["Ws"] | 0.0f;
  reportRelay       = doc["relay"] | false;
  reportTemperature = doc["temperature"] | 0.0f;
  reportBootId      = doc["boot_id"] | "";
  reportEnergyBoot  = doc["energy_since_boot"] | 0.0f;
  reportTimeBoot    = doc["time_since_boot"] | 0;

  reportValid       = true;
  Serial.println("REPORT updated");
}

/**
 * @brief Arduino setup - initialize hardware, load settings, connect WiFi
 * @note Loads offDelayMs and relayIpAddress from NVS preferences
 * @note Configures GPIO pins with pullups, starts web server
 */
void setup() {
  Serial.begin(115200);
  delay(200);
  M5.begin(true, false, true);

  // Initialize SPIFFS filesystem
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed!");
  } else {
    Serial.println("SPIFFS mounted successfully");
  }

  prefs.begin("coreone", false);  // Namespace
  offDelayMs = prefs.getUInt("off_delay_ms", offDelayMs); // Load from NVS, use default if not set
  relayIpAddress = prefs.getString("relay_ip", relayIpAddress); // Load relay IP
  prefs.end();

  Serial.println("Load offDelayMs: " + String(offDelayMs));
  Serial.println("Load relayIpAddress: " + relayIpAddress);

  pinMode(INPUT_PIN,      INPUT_PULLUP);
  pinMode(INPUT_PIN_MODE, INPUT_PULLUP);

  // WiFiManager setup
  Serial.println("Starting WiFi configuration...");
  
  // Set custom AP name and timeout
  wifiManager.setConfigPortalTimeout(180); // 3 minutes timeout
  wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
    Serial.println("Entered config mode");
    Serial.println("AP Name: " + String(myWiFiManager->getConfigPortalSSID()));
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
    // Show blue pattern on LED to indicate config mode
    clearMatrix();
    for (int i = 0; i < 25; i++) {
      M5.dis.drawpix(i, 0x0000FF);
    }
  });

  // Auto-connect or start config portal
  if (!wifiManager.autoConnect("M5Stack-AutoOff")) {
    Serial.println("Failed to connect and timeout occurred");
    // Show red LED for failed connection
    clearMatrix();
    for (int i = 0; i < 25; i++) {
      M5.dis.drawpix(i, 0xFF0000);
    }
    delay(3000);
    ESP.restart();
  }

  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  lastState = digitalRead(INPUT_PIN);

  autoPowerOffEnabled = false;
  showAutoOffDisabled();

  startWebServer();
}

/**
 * @brief Main loop - handle web requests, button input, timer logic, LED updates
 * @note Polls relay status every 5 seconds
 * @note Updates LED matrix based on timer state and progress
 * @note Monitors INPUT_PIN for signal changes to trigger auto-off timer
 */
void loop() {
  ensureWifi();
  server.handleClient();

  uint32_t now = millis();

  if (now - lastReportPollMs >= REPORT_POLL_INTERVAL_MS) {
    lastReportPollMs = now;
    updateReportStatus();
  }

  ModeClickEvent evt = chkModeButton();
  if (evt == ModeSingleClick) {
    Serial.println("Mode SINGLE-CLICK -> toggle auto mode");
    offTimerRunning = false;
    lastState = digitalRead(INPUT_PIN);
    autoPowerOffEnabled = !autoPowerOffEnabled;

    // Update LED display immediately based on mode and relay state
    if (autoPowerOffEnabled) {
      if (reportValid && !reportRelay) {
        showAutoOffEnabledRed();  // Red X when auto-off enabled and relay is OFF
      } else {
        showAutoOffEnabledBase(); // Blue X when auto-off enabled and relay is ON
      }
    } else {
      if (reportValid && !reportRelay) {
        showAutoOffDisabledRed(); // Red I when auto-off disabled and relay is OFF
      } else {
        showAutoOffDisabled();    // Green I when auto-off disabled and relay is ON
      }
    }
  } else if (evt == ModeDoubleClick) {
    Serial.println("Mode DOUBLE-CLICK -> toggle relay");
    offTimerRunning = false;
    sendToggle();

    clearMatrix();
    if (autoPowerOffEnabled) {
      showAutoOffEnabledBase();
    } else {
      showAutoOffDisabled();
    }
  } else if (evt == ModeLongPress) {
    Serial.println("Mode LONG-PRESS -> Reset WiFi settings and restart");
    clearMatrix();
    // Show magenta/purple pattern for WiFi reset
    for (int i = 0; i < 25; i++) {
      M5.dis.drawpix(i, 0xFF00FF);
    }
    wifiManager.resetSettings();
    delay(2000);
    ESP.restart();
  }

  if (autoPowerOffEnabled) {
    bool s = digitalRead(INPUT_PIN);

    if (s != lastState && (now - lastChangeMs) > DEBOUNCE_MS) {
      lastChangeMs = now;
      lastState = s;

      if (s == LOW) {
        offTimerRunning = true;
        offTimerStart   = now;
      } else {
        offTimerRunning = false;
        // Update LED based on relay state when signal goes HIGH
        if (reportValid && !reportRelay) {
          showAutoOffEnabledRed();  // Red X if relay is OFF
        } else {
          showAutoOffEnabledBase(); // Blue X if relay is ON
        }
      }
    }
  } else {
    if (offTimerRunning) {
      offTimerRunning = false;
    }
  }

  if (offTimerRunning) {
    uint32_t elapsed = now - offTimerStart;
    if (elapsed >= offDelayMs ) {
      offTimerRunning = false;
      sendOff();
      clearMatrix();
      M5.dis.fillpix(0x330000);
      drawI(0xFF0000);
    } else {
      float progress = (float)elapsed / (float)offDelayMs ;
      if (progress < 0.0f) progress = 0.0f;
      if (progress > 1.0f) progress = 1.0f;

      uint8_t filledRows = (uint8_t)(progress * 5.0f + 0.999f);
      if (filledRows > 5) filledRows = 5;

      drawProgressBar(filledRows);
    }
  } else {
    // When timer is not running, update LED based on current relay state
    static bool lastReportRelay = false;
    static bool lastReportValid = false;
    
    // Only update LED if relay state or report validity changed
    if (reportRelay != lastReportRelay || reportValid != lastReportValid) {
      lastReportRelay = reportRelay;
      lastReportValid = reportValid;
      
      if (autoPowerOffEnabled) {
        if (reportValid && !reportRelay) {
          showAutoOffEnabledRed();  // Red X when relay is OFF
        } else {
          showAutoOffEnabledBase(); // Blue X when relay is ON or report invalid
        }
      } else {
        if (reportValid && !reportRelay) {
          showAutoOffDisabledRed(); // Red I when relay is OFF
        } else {
          showAutoOffDisabled();    // Green I when relay is ON or report invalid
        }
      }
    }
  }

  delay(5);
}
