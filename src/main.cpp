/**
 * @file M5SwitchControl.ino
 * @brief M5Atom-based controller for a network switch with auto power-off mode,
 *        LED matrix progress display and Bootstrap-based web UI + relay report.
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <M5Atom.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "diveintothenet";
const char* WIFI_PASS = "dtn24steffshome67L";

constexpr int INPUT_PIN      = 23;  // external input (to GND, INPUT_PULLUP)
constexpr int INPUT_PIN_MODE = 39;  // mode button (Atom internal button)

constexpr uint32_t DEBOUNCE_MS     = 60;
constexpr uint32_t OFF_DELAY_MS    = 10UL * 1000UL;   // 10 seconds (test)
constexpr uint32_t DOUBLE_CLICK_MS = 400;             // double-click window

// -----------------------------------------------------------------------------
// Target switch URLs
// -----------------------------------------------------------------------------

const char* URL_TOGGLE = "http://192.168.188.44/toggle";
const char* URL_OFF    = "http://192.168.188.44/relay?state=0";
const char* URL_ON     = "http://192.168.188.44/relay?state=1";
const char* URL_REPORT = "http://192.168.188.44/report";

// -----------------------------------------------------------------------------
// Web server instance
// -----------------------------------------------------------------------------

WebServer server(80);

// -----------------------------------------------------------------------------
// State variables
// -----------------------------------------------------------------------------

bool     lastState        = HIGH;
uint32_t lastChangeMs     = 0;

/**
 * @brief Auto power off mode.
 * true  = auto power off enabled (uses input + timer)
 * false = auto power off disabled (manual mode)
 */
bool     autoPowerOffEnabled      = true;

bool     modeLastState            = HIGH;
uint32_t modeLastChangeMs         = 0;

uint8_t  modeClickCount           = 0;
uint32_t modeLastClickTime        = 0;

bool     offTimerRunning          = false;
uint32_t offTimerStart            = 0;

// -----------------------------------------------------------------------------
// Remote relay status (/report)
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// LED matrix helper functions
// -----------------------------------------------------------------------------

void clearMatrix() {
  M5.dis.clear();
}

/**
 * @brief Draw vertical "I" in center column, used for manual mode.
 */
void drawI(uint32_t col) {
  M5.dis.drawpix(2, 0, col);
  M5.dis.drawpix(2, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(2, 3, col);
  M5.dis.drawpix(2, 4, col);
}

/**
 * @brief Shows a blue "X" when auto power off is ENABLED.
 */
void showAutoOffEnabledBase() {
  clearMatrix();
  // dunkler Hintergrund
  M5.dis.fillpix(0x000000);

  uint32_t col = 0x0000FF;  // blue

  M5.dis.drawpix(0, 0, col);
  M5.dis.drawpix(1, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(3, 3, col);
  M5.dis.drawpix(4, 4, col);

  M5.dis.drawpix(4, 0, col);
  M5.dis.drawpix(3, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(1, 3, col);
  M5.dis.drawpix(0, 4, col);
}

/**
 * @brief Shows a green "I" when auto power off is DISABLED (manual mode).
 */
void showAutoOffDisabled() {
  clearMatrix();
  drawI(0x00FF00);
}

/**
 * @brief Draws an orange progress background behind the blue X
 *        (auto power off enabled) from bottom to top.
 *
 * The X stays blue, while the background rows under it fill up.
 *
 * @param filledRows Number of rows that should be filled (0..5).
 */
void drawProgressBar(uint8_t filledRows) {
  // Step 1: clear and draw base X in blue (auto ON)
  clearMatrix();
  uint32_t bgBase = 0x000000;   // dark background
  uint32_t xCol   = 0x0000FF;   // blue X
  uint32_t barCol = 0xFF8000;   // orange progress

  // fill full background dark blue first
  M5.dis.fillpix(bgBase);

  // Step 2: draw orange bars row by row from bottom (y=4) to top (y=0)
  for (int y = 4; y >= 0; --y) {
    bool fillRow = (4 - y) < filledRows;  // 0->y=4, 1->y=3, ...
    if (fillRow) {
      for (int x = 0; x < 5; ++x) {
        M5.dis.drawpix(x, y, barCol);
      }
    }
  }

  // Step 3: redraw the X in blue over the background
  M5.dis.drawpix(0, 0, xCol);
  M5.dis.drawpix(1, 1, xCol);
  M5.dis.drawpix(2, 2, xCol);
  M5.dis.drawpix(3, 3, xCol);
  M5.dis.drawpix(4, 4, xCol);

  M5.dis.drawpix(4, 0, xCol);
  M5.dis.drawpix(3, 1, xCol);
  M5.dis.drawpix(2, 2, xCol);
  M5.dis.drawpix(1, 3, xCol);
  M5.dis.drawpix(0, 4, xCol);
}

// -----------------------------------------------------------------------------
// HTTP client to external switch
// -----------------------------------------------------------------------------

void sendGet(const char* url) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  Serial.printf("GET %s -> HTTP %d\n", url, code);
  http.end();
}

void sendOff()    { sendGet(URL_OFF);    }
void sendOn()     { sendGet(URL_ON);     }
void sendToggle() { sendGet(URL_TOGGLE); }

// -----------------------------------------------------------------------------
// WiFi and web server helpers
// -----------------------------------------------------------------------------

void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

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

  StaticJsonDocument<256> doc;
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

// -----------------------------------------------------------------------------
// HTML page builder (Bootstrap UI)
// -----------------------------------------------------------------------------

String htmlPage() {
  String ip = WiFi.localIP().toString();

  String s;
  s += "<!doctype html><html lang='en'><head>";
  s += "<meta charset='utf-8'>";
  s += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  s += "<title>M5 Auto Power Off</title>";
  s += "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'>";
  s += "<link href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.css' rel='stylesheet'>";
  s += "<style>";
  s += "body{min-height:100vh;margin:0;";
  s += "background:radial-gradient(circle at top,#1f2937 0,#020617 55%,#000 100%);";
  s += "color:#e5e7eb;font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;}";
  s += ".card-shell{max-width:520px;margin:32px auto;padding:0 12px;}";
  s += ".glass-card{background:rgba(15,23,42,0.9);";
  s += "border-radius:18px;border:1px solid rgba(148,163,184,0.35);";
  s += "box-shadow:0 18px 45px rgba(0,0,0,0.65);backdrop-filter:blur(16px);}";
  s += ".card-header-gradient{border-bottom:1px solid rgba(148,163,184,0.35);";
  s += "background:linear-gradient(120deg,#0ea5e9,#22c55e,#eab308);}";
  s += ".chip{border-radius:999px;padding:4px 10px;font-size:0.78rem;}";
  s += ".btn-pill{border-radius:999px;font-weight:500;}";
  s += ".btn-icon i{margin-right:6px;}";
  s += ".status-label{font-size:0.8rem;color:#9ca3af;}";
  s += ".status-value{font-weight:600;}";
  s += ".progress{background:rgba(15,23,42,0.9);border-radius:999px;}";
  s += ".progress-bar{border-radius:999px;}";
  s += ".badge-soft{background:rgba(15,23,42,0.85);border:1px solid rgba(148,163,184,0.6);}";
  s += "a,button{outline:none!important;box-shadow:none!important;}";
  s += ".list-group-item{border-color:rgba(55,65,81,0.7);}";
  s += "</style>";
  s += "</head><body>";
  s += "<div class='card-shell'>";
  s += "  <div class='glass-card'>";
  s += "    <div class='card-header-gradient text-white px-4 py-3 d-flex align-items-center justify-content-between'>";
  s += "      <div>";
  s += "        <div class='fw-semibold'>M5 Auto Power Off</div>";
  s += "        <div class='small text-white-50'>Local ESP32 control & timer</div>";
  s += "      </div>";
  s += "      <div class='text-end'>";
  s += "        <div class='status-label'>Device IP</div>";
  s += "        <div class='status-value small'>" + ip + "</div>";
  s += "      </div>";
  s += "    </div>";

  s += "    <div class='px-4 pt-3 pb-4'>";
  s += "      <div class='row g-2 mb-3'>";
  s += "        <div class='col-6'>";
  s += "          <div class='badge-soft w-100 d-flex flex-column align-items-start px-3 py-2'>";
  s += "            <span class='status-label'>Auto power off</span>";
  s += "            <span id='modeBadge' class='status-value chip bg-secondary text-light mt-1'>?</span>";
  s += "          </div>";
  s += "        </div>";
  s += "        <div class='col-6'>";
  s += "          <div class='badge-soft w-100 d-flex flex-column align-items-start px-3 py-2'>";
  s += "            <span class='status-label'>Timer</span>";
  s += "            <span id='timerBadge' class='status-value chip bg-secondary text-light mt-1'>?</span>";
  s += "          </div>";
  s += "        </div>";
  s += "      </div>";

  s += "      <div class='d-flex justify-content-between align-items-center mb-1'>";
  s += "        <span class='status-label'>Remaining time</span>";
  s += "        <span id='timeRemaining' class='status-value small'>-</span>";
  s += "      </div>";
  s += "      <div class='progress mb-4' style='height:9px;'>";
  s += "        <div id='timerProgress' class='progress-bar bg-warning' role='progressbar' style='width:0%;'></div>";
  s += "      </div>";

  s += "      <div class='d-grid gap-2 mb-2'>";
  s += "        <button id='btnMode' class='btn btn-pill btn-sm btn-outline-info btn-icon' type='button'>";
  s += "          <i class='bi bi-clock-history'></i> Toggle auto power off";
  s += "        </button>";
  s += "        <button id='btnOffNow' class='btn btn-pill btn-sm btn-outline-danger btn-icon' type='button'>";
  s += "          <i class='bi bi-power'></i> Immediate OFF";
  s += "        </button>";
  s += "        <button id='btnOnNow' class='btn btn-pill btn-sm btn-outline-success btn-icon' type='button'>";
  s += "          <i class='bi bi-lightning-charge'></i> Relay ON";
  s += "        </button>";
  s += "        <button id='btnToggle' class='btn btn-pill btn-sm btn-outline-light btn-icon' type='button'>";
  s += "          <i class='bi bi-arrow-repeat'></i> Switch TOGGLE (test)";
  s += "        </button>";
  s += "      </div>";

  s += "      <div class='text-center mt-2'>";
  s += "        <small class='text-muted'>Updated live from M5 via JSON / AJAX</small>";
  s += "      </div>";
  s += "    </div>";
  s += "  </div>";

  // Relay report card
  s += "  <div class='glass-card mt-3'>";
  s += "    <div class='px-4 pt-3 pb-3'>";
  s += "      <div class='d-flex justify-content-between align-items-center mb-2'>";
  s += "        <div>";
  s += "          <div class='fw-semibold'>Relay status</div>";
  s += "          <div class='status-label'>Reported from 192.168.188.44</div>";
  s += "        </div>";
  s += "        <span id='relayBadge' class='chip bg-secondary text-light'>?</span>";
  s += "      </div>";
  s += "      <ul class='list-group list-group-flush small' style='background:transparent;border-radius:12px;'>";
  s += "        <li class='list-group-item bg-transparent text-light d-flex justify-content-between'>";
  s += "          <span>Power</span><span id='valPower'>-</span>";
  s += "        </li>";
  s += "        <li class='list-group-item bg-transparent text-light d-flex justify-content-between'>";
  s += "          <span>Ws</span><span id='valWs'>-</span>";
  s += "        </li>";
  s += "        <li class='list-group-item bg-transparent text-light d-flex justify-content-between'>";
  s += "          <span>Temperature</span><span id='valTemp'>-</span>";
  s += "        </li>";
  s += "        <li class='list-group-item bg-transparent text-light d-flex justify-content-between'>";
  s += "          <span>Energy since boot</span><span id='valEnergy'>-</span>";
  s += "        </li>";
  s += "        <li class='list-group-item bg-transparent text-light d-flex justify-content-between'>";
  s += "          <span>Time since boot</span><span id='valTimeBoot'>-</span>";
  s += "        </li>";
  s += "        <li class='list-group-item bg-transparent text-light d-flex justify-content-between'>";
  s += "          <span>Boot ID</span><span id='valBootId'>-</span>";
  s += "        </li>";
  s += "      </ul>";
  s += "      <div class='mt-2 text-end'>";
  s += "        <small id='valReportState' class='text-muted'>report: unknown</small>";
  s += "      </div>";
  s += "    </div>";
  s += "  </div>";

  s += "</div>"; // card-shell

  s += "<script>";
  s += "async function apiCall(path){try{await fetch(path);}catch(e){console.error(e);}}";
  s += "document.getElementById('btnMode').onclick=function(){apiCall('/api/mode');};";
  s += "document.getElementById('btnOffNow').onclick=function(){apiCall('/api/off_now');};";
  s += "document.getElementById('btnOnNow').onclick=function(){apiCall('/api/on_now');};";
  s += "document.getElementById('btnToggle').onclick=function(){apiCall('/api/toggle');};";

  s += "async function refreshStatus(){";
  s += "  try{const r=await fetch('/api/status');";
  s += "    if(!r.ok)return;const j=await r.json();";
  s += "    const autoMode=j.auto_mode;const tmr=j.timer;";
  s += "    const rem=j.remaining_ms;const total=j.total_ms;";
  s += "    const modeBadge=document.getElementById('modeBadge');";
  s += "    const tBadge=document.getElementById('timerBadge');";
  s += "    const timeSpan=document.getElementById('timeRemaining');";
  s += "    const bar=document.getElementById('timerProgress');";
  s += "    const relayBadge=document.getElementById('relayBadge');";
  s += "    const valPower=document.getElementById('valPower');";
  s += "    const valWs=document.getElementById('valWs');";
  s += "    const valTemp=document.getElementById('valTemp');";
  s += "    const valEnergy=document.getElementById('valEnergy');";
  s += "    const valTimeBoot=document.getElementById('valTimeBoot');";
  s += "    const valBootId=document.getElementById('valBootId');";
  s += "    const valReportState=document.getElementById('valReportState');";

  s += "    const rv=j.report_valid;";
  s += "    const relay=j.relay;";

  // auto power off badge
  s += "    modeBadge.textContent = autoMode ? 'ON' : 'OFF';";
  s += "    modeBadge.className = 'status-value chip ' + (autoMode ? 'bg-success' : 'bg-danger');";

  // timer badge
  s += "    tBadge.textContent = tmr ? 'RUNNING' : 'STOPPED';";
  s += "    tBadge.className = 'status-value chip ' + (tmr ? 'bg-warning text-dark' : 'bg-secondary');";

  // remaining + progress
  s += "    let txt='-';let pct=0;";
  s += "    if(tmr){";
  s += "      const sec=Math.max(0,Math.floor(rem/1000));";
  s += "      const m=Math.floor(sec/60);";
  s += "      const s=sec%60;";
  s += "      txt=(m.toString().padStart(2,'0')+':'+s.toString().padStart(2,'0'))+' min';";
  s += "      pct = Math.min(100, Math.max(0, 100*(total-rem)/total));";
  s += "    }";
  s += "    timeSpan.textContent=txt;";
  s += "    bar.style.width=pct+'%';";

  // report section
  s += "    valReportState.textContent = rv ? 'report: OK' : 'report: error';";
  s += "    if(rv){";
  s += "      relayBadge.textContent = relay ? 'ON' : 'OFF';";
  s += "      relayBadge.className = 'chip ' + (relay ? 'bg-success text-light' : 'bg-secondary text-light');";
  s += "      valPower.textContent  = j.power.toFixed(2) + ' W';";
  s += "      valWs.textContent     = j.ws.toFixed(2);";
  s += "      valTemp.textContent   = j.temperature.toFixed(2) + ' °C';";
  s += "      valEnergy.textContent = j.energy_boot.toFixed(2);";

  // time_boot in days/hours/minutes/seconds [web:344][web:350]
  s += "      let tb = j.time_boot;";
  s += "      let secs = tb;";
  s += "      const days = Math.floor(secs / 86400); secs %= 86400;";
  s += "      const hours = Math.floor(secs / 3600); secs %= 3600;";
  s += "      const mins = Math.floor(secs / 60); secs %= 60;";
  s += "      let tbStr = days+'d '+hours+'h '+mins+'m '+secs+'s';";
  s += "      valTimeBoot.textContent = tbStr;";

  s += "      valBootId.textContent = j.boot_id;";
  s += "    }else{";
  s += "      relayBadge.textContent = '?';";
  s += "      relayBadge.className = 'chip bg-secondary text-light';";
  s += "      valPower.textContent=valWs.textContent=valTemp.textContent='-';";
  s += "      valEnergy.textContent=valTimeBoot.textContent=valBootId.textContent='-';";
  s += "    }";
  s += "  }catch(e){console.error(e);}";
  s += "}";
  s += "setInterval(refreshStatus,500);";
  s += "refreshStatus();";
  s += "</script>";

  s += "</body></html>";
  return s;
}

// -----------------------------------------------------------------------------
// Web server setup
// -----------------------------------------------------------------------------

void startWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", htmlPage());
  });

  server.on("/api/status", HTTP_GET, []() {
    uint32_t now = millis();
    bool timer = offTimerRunning;
    uint32_t remaining = 0;
    if (timer) {
      uint32_t elapsed = now - offTimerStart;
      remaining = (elapsed >= OFF_DELAY_MS) ? 0 : (OFF_DELAY_MS - elapsed);
    }
    String json = "{";
    json += "\"auto_mode\":"     + String(autoPowerOffEnabled ? "true" : "false") + ",";
    json += "\"timer\":"         + String(timer ? "true" : "false") + ",";
    json += "\"remaining_ms\":"  + String(remaining) + ",";
    json += "\"total_ms\":"      + String(OFF_DELAY_MS) + ",";
    json += "\"report_valid\":"  + String(reportValid ? "true" : "false") + ",";
    json += "\"relay\":"         + String(reportRelay ? "true" : "false") + ",";
    json += "\"power\":"         + String(reportPower, 2) + ",";
    json += "\"ws\":"            + String(reportWs, 2) + ",";
    json += "\"temperature\":"   + String(reportTemperature, 2) + ",";
    json += "\"energy_boot\":"   + String(reportEnergyBoot, 2) + ",";
    json += "\"time_boot\":"     + String(reportTimeBoot) + ",";
    json += "\"boot_id\":\""     + reportBootId + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  // toggle auto power-off mode
  server.on("/api/mode", HTTP_GET, []() {
    autoPowerOffEnabled = !autoPowerOffEnabled;
    offTimerRunning = false;
    if (autoPowerOffEnabled) {
      showAutoOffEnabledBase();   // blue X
    } else {
      showAutoOffDisabled();      // green I
    }
    server.send(200, "text/plain", autoPowerOffEnabled ? "auto_mode=ON" : "auto_mode=OFF");
  });

  server.on("/api/off_now", HTTP_GET, []() {
    offTimerRunning = false;
    sendOff();
    clearMatrix();
    M5.dis.fillpix(0x330000);
    drawI(0xFF0000);  // red I to indicate OFF
    server.send(200, "text/plain", "off_now=OK");
  });

  server.on("/api/on_now", HTTP_GET, []() {
    sendOn();
    server.send(200, "text/plain", "on_now=OK");
  });

  server.on("/api/toggle", HTTP_GET, []() {
    sendToggle();
    server.send(200, "text/plain", "toggle=OK");
  });

  server.begin();
  Serial.println("HTTP server started");
}

// -----------------------------------------------------------------------------
// Arduino setup & loop
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);

  M5.begin(true, false, true);

  pinMode(INPUT_PIN,      INPUT_PULLUP);
  pinMode(INPUT_PIN_MODE, INPUT_PULLUP);

  ensureWifi();

  lastState     = digitalRead(INPUT_PIN);
  modeLastState = digitalRead(INPUT_PIN_MODE);

  autoPowerOffEnabled = true;
  showAutoOffEnabledBase();  // blue X

  startWebServer();
}

void loop() {
  ensureWifi();
  server.handleClient();

  uint32_t now = millis();

  // Periodically poll /report on the relay device
  if (now - lastReportPollMs >= REPORT_POLL_INTERVAL_MS) {
    lastReportPollMs = now;
    updateReportStatus();
  }

  // 1) Mode button: debounced + single/double click handling
  bool modeReading = digitalRead(INPUT_PIN_MODE);
  if (modeReading != modeLastState && (now - modeLastChangeMs) > DEBOUNCE_MS) {
    modeLastChangeMs = now;
    modeLastState = modeReading;

    if (modeReading == LOW) {
      // Button press detected
      if (now - modeLastClickTime > DOUBLE_CLICK_MS) {
        modeClickCount = 1;
        modeLastClickTime = now;
      } else {
        modeClickCount++;
      }

      if (modeClickCount == 2) {
        // Double-click: immediate toggle on relay
        Serial.println("Mode DOUBLE-CLICK -> toggle relay");
        offTimerRunning = false;
        sendToggle();
        clearMatrix();
        M5.dis.fillpix(0x000000);
        // short blue X flash
        showAutoOffEnabledBase();
        modeClickCount = 0;
        modeLastClickTime = 0;
      } else {
        // Single-click: toggle auto power-off mode
        autoPowerOffEnabled = !autoPowerOffEnabled;
        offTimerRunning = false;
        if (autoPowerOffEnabled) {
          showAutoOffEnabledBase(); // blue X
        } else {
          showAutoOffDisabled();    // green I
        }
      }
    }
  }

  // reset single-click window if time elapsed without second click
  if (modeClickCount == 1 && (now - modeLastClickTime > DOUBLE_CLICK_MS)) {
    modeClickCount = 0;
    modeLastClickTime = 0;
  }

  // 2) External input handling (only if auto power off is enabled)
  if (autoPowerOffEnabled) {
    bool s = digitalRead(INPUT_PIN);

    if (s != lastState && (now - lastChangeMs) > DEBOUNCE_MS) {
      lastChangeMs = now;
      lastState = s;

      if (s == LOW) {
        // Start auto power off timer
        offTimerRunning = true;
        offTimerStart   = now;
      } else {
        // Abort timer, stay in auto mode (blue X)
        offTimerRunning = false;
        showAutoOffEnabledBase();
      }
    }
  } else {
    // In manual mode, ensure timer is not running
    if (offTimerRunning) {
      offTimerRunning = false;
    }
  }

  // 3) Auto power-off timer + LED matrix progress bar
  if (offTimerRunning) {
    uint32_t elapsed = now - offTimerStart;
    if (elapsed >= OFF_DELAY_MS) {
      offTimerRunning = false;
      sendOff();
      clearMatrix();
      M5.dis.fillpix(0x330000);
      drawI(0xFF0000);   // red I when turned off
    } else {
      float progress = (float)elapsed / (float)OFF_DELAY_MS;
      if (progress < 0.0f) progress = 0.0f;
      if (progress > 1.0f) progress = 1.0f;

      uint8_t filledRows = (uint8_t)(progress * 5.0f + 0.999f);
      if (filledRows > 5) filledRows = 5;

      drawProgressBar(filledRows);
    }
  }

  delay(5);
}
