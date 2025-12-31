#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <M5Atom.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "LedDisplay.h"
#include "ButtonMode.h"
#include "WebUi.h"
#include "wifi_cred.h"


const char* WIFI_SSID = WIFI_SSID_CONFIG;
const char* WIFI_PASS = WIFI_PASS_CONFIG;

constexpr int INPUT_PIN      = 23;

const char* URL_TOGGLE = "http://192.168.188.44/toggle";
const char* URL_OFF    = "http://192.168.188.44/relay?state=0";
const char* URL_ON     = "http://192.168.188.44/relay?state=1";
const char* URL_REPORT = "http://192.168.188.44/report";

WebServer server(80);

bool     lastState        = HIGH;
uint32_t lastChangeMs     = 0;

bool     autoPowerOffEnabled      = false;
bool     offTimerRunning          = false;
uint32_t offTimerStart            = 0;

bool     reportValid       = false;
bool     reportRelay       = false;
float    reportPower       = 0.0f;
float    reportWs          = 0.0f;
float    reportTemperature = 0.0f;
String   reportBootId      = "";
float    reportEnergyBoot  = 0.0f;
uint32_t reportTimeBoot    = 0;

uint32_t lastReportPollMs  = 0;
constexpr uint32_t REPORT_POLL_INTERVAL_MS = 5000;

//
Preferences prefs;
uint32_t offDelayMs = 10UL * 60UL * 1000UL; // default 10 min

// Forward
void ensureWifi();
void sendGet(const char* url);
void sendOff();
void sendOn();
void sendToggle();
void updateReportStatus();

void sendGet(const char* url) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  Serial.printf("GET %s -> HTTP %d\n", url, code);
  http.end();
}
void sendOff()    { sendGet(URL_OFF);    }
void sendOn()     { sendGet(URL_ON);     }
void sendToggle() { sendGet(URL_TOGGLE); }

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect timeout.");
  }
}

void updateReportStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    reportValid = false;
    return;
  }

  HTTPClient http;
  http.begin(URL_REPORT);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("REPORT GET -> HTTP %d\n", code);
    http.end();
    reportValid = false;
    return;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(256);
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

void setup() {
  Serial.begin(115200);
  delay(200);
  M5.begin(true, false, true);

  prefs.begin("coreone", false);  // Namespace
  offDelayMs = prefs.getUInt("off_delay_ms", offDelayMs); // Default, falls nicht gesetzt
  prefs.end();

  Serial.println("Load " + String(offDelayMs));

  pinMode(INPUT_PIN,      INPUT_PULLUP);
  pinMode(INPUT_PIN_MODE, INPUT_PULLUP);

  ensureWifi();

  lastState = digitalRead(INPUT_PIN);

  autoPowerOffEnabled = false;
  showAutoOffDisabled();

  startWebServer();
}

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

    if (autoPowerOffEnabled) {
      showAutoOffEnabledBase();
    } else {
      showAutoOffDisabled();
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
        showAutoOffEnabledBase();
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
  }

  delay(5);
}
