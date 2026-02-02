/**
 * @file WebUi.cpp
 * @brief Web interface implementation serving files from LittleFS filesystem
 * 
 * Provides a responsive web UI with "glass morphism" dark theme styled
 * in Prusa orange (#F96831). Features live AJAX updates every 500ms for
 * status monitoring without page reloads.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <M5Atom.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiManager.h>
#include "WebUi.h"
#include "LedDisplay.h"

// External WiFiManager from main.cpp
extern WiFiManager wifiManager;

// External state from main.cpp
extern bool autoPowerOffEnabled;
extern bool offTimerRunning;
extern bool reportValid;
extern bool reportRelay;
extern float reportPower;
extern float reportWs;
extern float reportTemperature;
extern String reportBootId;
extern float reportEnergyBoot;
extern uint32_t reportTimeBoot;
extern String relayIpAddress;
extern uint32_t consecutiveErrors;
extern uint32_t lastReportPollMs;

// Power logging constants and externals
#define MAX_LOG_ENTRIES 500  // Must match main.cpp definition

struct PowerLogEntry {
  uint32_t timestamp;
  float power;
  float energy;
  float cost;
};
extern PowerLogEntry powerLog[];
extern size_t powerLogCount;
extern size_t powerLogIndex;
extern bool loggingEnabled;
extern uint32_t loggingStartMs;

// Tariff settings externals
extern float tariffHigh;
extern float tariffLow;
extern String currency;
extern int tariffSwitchHour;
extern int tariffSwitchEndHour;

// Auto-logging settings externals
extern bool autoLogEnabled;
extern float autoLogThreshold;
extern uint32_t autoLogDebounce;
extern uint32_t autoLogAboveMs;
extern uint32_t autoLogBelowMs;

// Authentication credentials (stored in NVS)
String authUsername = "admin";
String authPassword = "prusa";

// External control functions from main.cpp
void sendOff();
void sendOn();
void sendToggle();
void updateReportStatus();
void startLogging();
void stopLogging();
void clearLog();
void saveTariffSettings();

/**
 * @brief Check HTTP Basic Authentication
 * @return true if authenticated, false otherwise
 */
bool checkAuth() {
    if (!server.authenticate(authUsername.c_str(), authPassword.c_str())) {
        server.requestAuthentication();
        return false;
    }
    return true;
}

/**
 * @brief Get MIME type from file extension
 * @param filename Name of file
 * @return MIME type string
 */
String getContentType(String filename) {
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".json")) return "application/json";
    else if (filename.endsWith(".png")) return "image/png";
    else if (filename.endsWith(".jpg")) return "image/jpeg";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
}

/**
 * @brief Serve file from LittleFS filesystem
 * @param path File path to serve
 * @return true if file was served successfully
 */
bool handleFileRead(String path) {
    Serial.println("handleFileRead: " + path);
    
    if (path.endsWith("/")) {
        path += "index.html";
    }
    
    String contentType = getContentType(path);
    
    if (SPIFFS.exists(path)) {
        File file = SPIFFS.open(path, "r");
        server.streamFile(file, contentType);
        file.close();
        return true;
    }
    
    Serial.println("File not found: " + path);
    return false;
}


void startWebServer()
{
    // Load authentication credentials from NVS
    Preferences prefs;
    prefs.begin("coreone", true); // Read-only
    authUsername = prefs.getString("auth_user", "admin");
    authPassword = prefs.getString("auth_pass", "prusa");
    prefs.end();
    Serial.println("Auth enabled - User: " + authUsername);

    // Serve static files from LittleFS
    server.onNotFound([]() {
        if (!checkAuth()) return;
        if (!handleFileRead(server.uri())) {
            server.send(404, "text/plain", "404: Not Found");
        }
    });

    server.on("/", HTTP_GET, []()
              { 
        if (!checkAuth()) return;
        handleFileRead("/index.html"); });

    server.on("/api/status", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        uint32_t now = millis();
        bool timer = offTimerRunning;
        uint32_t remaining = 0;
        if (timer) {
          uint32_t elapsed = now - offTimerStart;
          remaining = (elapsed >= offDelayMs) ? 0 : (offDelayMs - elapsed);
        }

        uint32_t timerMinutes = offDelayMs / 60000UL;
        String deviceIp = WiFi.localIP().toString();
        String wifiSSID = WiFi.SSID();

        String json = "{";
        json += "\"auto_mode\":"     + String(autoPowerOffEnabled ? "true" : "false") + ",";
        json += "\"timer\":"         + String(timer ? "true" : "false") + ",";
        json += "\"remaining_ms\":"  + String(remaining) + ",";
        json += "\"total_ms\":"      + String(offDelayMs) + ",";
        json += "\"timer_minutes\":" + String(timerMinutes) + ",";
        json += "\"report_valid\":"  + String(reportValid ? "true" : "false") + ",";
        json += "\"relay\":"         + String(reportRelay ? "true" : "false") + ",";
        json += "\"power\":"         + String(reportPower, 2) + ",";
        json += "\"ws\":"            + String(reportWs, 2) + ",";
        json += "\"temperature\":"   + String(reportTemperature, 2) + ",";
        json += "\"energy_boot\":"   + String(reportEnergyBoot, 2) + ",";
        json += "\"time_boot\":"     + String(reportTimeBoot) + ",";
        json += "\"boot_id\":\""     + reportBootId + "\",";
        json += "\"relay_ip\":\""    + relayIpAddress + "\",";
        json += "\"device_ip\":\""   + deviceIp + "\",";
        json += "\"wifi_ssid\":\""   + wifiSSID + "\"";
        json += "}";
        server.send(200, "application/json", json); });

    server.on("/api/mode", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        autoPowerOffEnabled = !autoPowerOffEnabled;
        offTimerRunning = false;
        if (autoPowerOffEnabled) {
          showAutoOffEnabledBase();
        } else {
          showAutoOffDisabled();
        }
        server.send(200, "text/plain", autoPowerOffEnabled ? "auto_mode=ON" : "auto_mode=OFF"); });

    server.on("/api/off_now", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        offTimerRunning = false;
        sendOff();
        clearMatrix();
        M5.dis.fillpix(0x000000);
        drawI(0xFF0000);
        server.send(200, "text/plain", "off_now=OK"); });

    server.on("/api/on_now", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        sendOn();
        server.send(200, "text/plain", "on_now=OK"); });

    server.on("/api/toggle", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        sendToggle();
        server.send(200, "text/plain", "toggle=OK"); });

    server.on("/api/set_timer", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        if (!server.hasArg("minutes")) {
          server.send(400, "text/plain", "missing minutes");
          return;
        }
        int minutes = server.arg("minutes").toInt();
        if (minutes < 1) minutes = 1;
        if (minutes > 240) minutes = 240;

        offDelayMs = (uint32_t)minutes * 60UL * 1000UL;

        Preferences prefs;
        prefs.begin("coreone", false);
        prefs.putUInt("off_delay_ms", offDelayMs); 
        prefs.end();
        Serial.println("store"+ String(offDelayMs)  );

        server.send(200, "text/plain", "ok"); });

    server.on("/api/set_relay_ip", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        if (!server.hasArg("ip")) {
          server.send(400, "text/plain", "missing ip");
          return;
        }
        String newIp = server.arg("ip");
        newIp.trim();
        
        if (newIp.length() < 7 || newIp.length() > 15) {
          server.send(400, "text/plain", "invalid ip format");
          return;
        }

        relayIpAddress = newIp;

        Preferences prefs;
        prefs.begin("coreone", false);
        prefs.putString("relay_ip", relayIpAddress);
        prefs.end();
        Serial.println("Stored relay IP: " + relayIpAddress);

        // Reset error counter and force immediate status poll
        consecutiveErrors = 0;
        lastReportPollMs = 0;
        updateReportStatus();

        server.send(200, "text/plain", "ok"); });

    server.on("/api/set_auth", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        if (!server.hasArg("user") || !server.hasArg("pass")) {
          server.send(400, "text/plain", "missing user or pass");
          return;
        }
        String newUser = server.arg("user");
        String newPass = server.arg("pass");
        newUser.trim();
        newPass.trim();
        
        if (newUser.length() < 3 || newPass.length() < 4) {
          server.send(400, "text/plain", "username min 3 chars, password min 4 chars");
          return;
        }

        authUsername = newUser;
        authPassword = newPass;

        Preferences prefs;
        prefs.begin("coreone", false);
        prefs.putString("auth_user", authUsername);
        prefs.putString("auth_pass", authPassword);
        prefs.end();
        Serial.println("Updated auth - User: " + authUsername);

        server.send(200, "text/plain", "ok"); });

    server.on("/api/reset_wifi", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        Serial.println("WiFi reset requested via web UI");
        server.send(200, "text/plain", "Resetting WiFi settings and restarting...");
        delay(500);
        wifiManager.resetSettings();
        delay(1000);
        ESP.restart(); });

    // Power logging API endpoints
    server.on("/api/log_start", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        startLogging();
        server.send(200, "text/plain", "logging started"); });

    server.on("/api/log_stop", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        stopLogging();
        server.send(200, "text/plain", "logging stopped"); });

    server.on("/api/log_clear", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        clearLog();
        server.send(200, "text/plain", "log cleared"); });

    server.on("/api/log_status", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        String json = "{";
        json += "\"enabled\":" + String(loggingEnabled ? "true" : "false") + ",";
        json += "\"count\":" + String(powerLogCount) + ",";
        json += "\"max\":" + String(MAX_LOG_ENTRIES) + ",";
        json += "\"duration_ms\":" + String(loggingEnabled ? (millis() - loggingStartMs) : 0);
        json += "}";
        server.send(200, "application/json", json); });

    server.on("/api/log_data", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        
        if (powerLogCount == 0) {
          server.send(200, "application/json", "{\"timestamps\":[],\"power\":[],\"energy\":[],\"cost\":[]}");
          return;
        }

        String json = "{\"timestamps\":[";
        
        // Build timestamps array
        size_t startIdx = (powerLogCount < MAX_LOG_ENTRIES) ? 0 : powerLogIndex;
        for (size_t i = 0; i < powerLogCount; i++) {
          size_t idx = (startIdx + i) % MAX_LOG_ENTRIES;
          if (i > 0) json += ",";
          json += String(powerLog[idx].timestamp);
        }
        json += "],\"power\":[";
        
        // Build power array
        for (size_t i = 0; i < powerLogCount; i++) {
          size_t idx = (startIdx + i) % MAX_LOG_ENTRIES;
          if (i > 0) json += ",";
          json += String(powerLog[idx].power, 2);
        }
        json += "],\"energy\":[";
        
        // Build energy array
        for (size_t i = 0; i < powerLogCount; i++) {
          size_t idx = (startIdx + i) % MAX_LOG_ENTRIES;
          if (i > 0) json += ",";
          json += String(powerLog[idx].energy, 3);
        }
        json += "],\"cost\":[";
        
        // Build cost array
        for (size_t i = 0; i < powerLogCount; i++) {
          size_t idx = (startIdx + i) % MAX_LOG_ENTRIES;
          if (i > 0) json += ",";
          json += String(powerLog[idx].cost, 4);
        }
        json += "]}";
        
        server.send(200, "application/json", json); });

    // Tariff settings API endpoints
    server.on("/api/tariff_get", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        String json = "{";
        json += "\"high\":" + String(tariffHigh, 4) + ",";
        json += "\"low\":" + String(tariffLow, 4) + ",";
        json += "\"currency\":\"" + currency + "\",";
        json += "\"start_hour\":" + String(tariffSwitchHour) + ",";
        json += "\"end_hour\":" + String(tariffSwitchEndHour);
        json += "}";
        server.send(200, "application/json", json); });

    server.on("/api/tariff_set", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        
        bool changed = false;
        
        if (server.hasArg("high")) {
          tariffHigh = server.arg("high").toFloat();
          changed = true;
        }
        if (server.hasArg("low")) {
          tariffLow = server.arg("low").toFloat();
          changed = true;
        }
        if (server.hasArg("currency")) {
          currency = server.arg("currency");
          currency.trim();
          if (currency.length() > 10) currency = currency.substring(0, 10);
          changed = true;
        }
        if (server.hasArg("start")) {
          tariffSwitchHour = server.arg("start").toInt();
          if (tariffSwitchHour < 0) tariffSwitchHour = 0;
          if (tariffSwitchHour > 23) tariffSwitchHour = 23;
          changed = true;
        }
        if (server.hasArg("end")) {
          tariffSwitchEndHour = server.arg("end").toInt();
          if (tariffSwitchEndHour < 0) tariffSwitchEndHour = 0;
          if (tariffSwitchEndHour > 23) tariffSwitchEndHour = 23;
          changed = true;
        }
        
        if (changed) {
          saveTariffSettings();
          server.send(200, "text/plain", "tariff settings saved");
        } else {
          server.send(400, "text/plain", "no parameters provided");
        } });

    // Auto-logging settings endpoints
    server.on("/api/autolog_get", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        String json = "{";
        json += "\"enabled\":" + String(autoLogEnabled ? "true" : "false") + ",";
        json += "\"threshold\":" + String(autoLogThreshold, 1) + ",";
        json += "\"debounce\":" + String(autoLogDebounce);
        json += "}";
        server.send(200, "application/json", json); });

    server.on("/api/autolog_set", HTTP_GET, []()
              {
        if (!checkAuth()) return;
        
        bool changed = false;
        
        if (server.hasArg("enabled")) {
          autoLogEnabled = (server.arg("enabled") == "true" || server.arg("enabled") == "1");
          changed = true;
        }
        if (server.hasArg("threshold")) {
          autoLogThreshold = server.arg("threshold").toFloat();
          if (autoLogThreshold < 0.1f) autoLogThreshold = 0.1f;
          if (autoLogThreshold > 500.0f) autoLogThreshold = 500.0f;
          changed = true;
        }
        if (server.hasArg("debounce")) {
          autoLogDebounce = server.arg("debounce").toInt();
          if (autoLogDebounce < 5) autoLogDebounce = 5;
          if (autoLogDebounce > 300) autoLogDebounce = 300;
          changed = true;
        }
        
        if (changed) {
          Preferences prefs;
          prefs.begin("coreone", false);
          prefs.putBool("autolog_en", autoLogEnabled);
          prefs.putFloat("autolog_th", autoLogThreshold);
          prefs.putUInt("autolog_db", autoLogDebounce);
          prefs.end();
          
          Serial.printf("Auto-logging settings saved: %s, %.1fW, %us\n",
                       autoLogEnabled ? "ON" : "OFF", autoLogThreshold, autoLogDebounce);
          
          // Reset debounce timers
          autoLogAboveMs = 0;
          autoLogBelowMs = 0;
          
          server.send(200, "text/plain", "autolog settings saved");
        } else {
          server.send(400, "text/plain", "no parameters provided");
        } });

    server.begin();
    Serial.println("HTTP server started");
}
