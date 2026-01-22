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

// External control functions from main.cpp
void sendOff();
void sendOn();
void sendToggle();
void updateReportStatus();

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
    // Serve static files from LittleFS
    server.onNotFound([]() {
        if (!handleFileRead(server.uri())) {
            server.send(404, "text/plain", "404: Not Found");
        }
    });

    server.on("/", HTTP_GET, []()
              { handleFileRead("/index.html"); });

    server.on("/api/status", HTTP_GET, []()
              {
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
        offTimerRunning = false;
        sendOff();
        clearMatrix();
        M5.dis.fillpix(0x000000);
        drawI(0xFF0000);
        server.send(200, "text/plain", "off_now=OK"); });

    server.on("/api/on_now", HTTP_GET, []()
              {
        sendOn();
        server.send(200, "text/plain", "on_now=OK"); });

    server.on("/api/toggle", HTTP_GET, []()
              {
        sendToggle();
        server.send(200, "text/plain", "toggle=OK"); });

    server.on("/api/set_timer", HTTP_GET, []()
              {
        if (!server.hasArg("minutes")) {
          server.send(400, "text/plain", "missing minutes");
          return;
        }
        int minutes = server.arg("minutes").toInt();
        if (minutes < 1) minutes = 1;
        if (minutes > 240) minutes = 240;

        offDelayMs = (uint32_t)minutes * 60UL * 1000UL;

        prefs.begin("coreone", false);
        prefs.putUInt("off_delay_ms", offDelayMs); 
        prefs.end();
        Serial.println("store"+ String(offDelayMs)  );

        server.send(200, "text/plain", "ok"); });

    server.on("/api/set_relay_ip", HTTP_GET, []()
              {
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

        prefs.begin("coreone", false);
        prefs.putString("relay_ip", relayIpAddress);
        prefs.end();
        Serial.println("Stored relay IP: " + relayIpAddress);

        // Reset error counter and force immediate status poll
        consecutiveErrors = 0;
        lastReportPollMs = 0;
        updateReportStatus();

        server.send(200, "text/plain", "ok"); });

    server.on("/api/reset_wifi", HTTP_GET, []()
              {
        Serial.println("WiFi reset requested via web UI");
        server.send(200, "text/plain", "Resetting WiFi settings and restarting...");
        delay(500);
        wifiManager.resetSettings();
        delay(1000);
        ESP.restart(); });

    server.begin();
    Serial.println("HTTP server started");
}
