#include <WiFi.h>
#include <HTTPClient.h>
#include <M5Atom.h>
#include <WebServer.h>

const char* WIFI_SSID = "diveintothenet";
const char* WIFI_PASS = "dtn24steffshome67L";

constexpr int INPUT_PIN = 23;              // dein Eingang
constexpr int INPUT_PIN_OVERRIDE = 39;              // dein Eingang

constexpr uint32_t DEBOUNCE_MS      = 60;
constexpr uint32_t OFF_DELAY_MS     = 10UL * 1000UL;   // 10 Sekunden (Test)
constexpr uint32_t DOUBLE_CLICK_MS  = 400;             // Zeitfenster für Doppelklick

// -----------------------------------------------------------------------------
// Switch URLs
// -----------------------------------------------------------------------------
const char* URL_TOGGLE = "http://192.168.188.44/toggle";
const char* URL_OFF    = "http://192.168.188.44/relay?state=0";

// -----------------------------------------------------------------------------
// Webserver
// -----------------------------------------------------------------------------
WebServer server(80);

// -----------------------------------------------------------------------------
// Zustände
// -----------------------------------------------------------------------------
bool     lastState        = HIGH;
uint32_t lastChangeMs     = 0;

bool     overrideEnabled          = true;
bool     overrideLastState        = HIGH;
uint32_t overrideLastChangeMs     = 0;

uint8_t  overrideClickCount       = 0;
uint32_t overrideLastClickTime    = 0;

bool     offTimerRunning = false;
uint32_t offTimerStart   = 0;

// -----------------------------------------------------------------------------
// Anzeige
// -----------------------------------------------------------------------------
void clearMatrix() { M5.dis.clear(); }

void drawI(uint32_t col) {
  M5.dis.drawpix(2, 0, col);
  M5.dis.drawpix(2, 1, col);
  M5.dis.drawpix(2, 2, col);
  M5.dis.drawpix(2, 3, col);
  M5.dis.drawpix(2, 4, col);
}

void showOverrideOn() { // rotes X
  clearMatrix();
  // dunkler Hintergrund
  M5.dis.fillpix(0x000000);

  uint32_t col = 0x0000FF;

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

void showOverrideOffBase() { // grünes I, keine Progressbar
  clearMatrix();
  drawI(0x00FF00);
}

// Progressbar: Zeilen von unten nach oben orange füllen, I bleibt grün
void drawProgressBar(uint8_t filledRows) {
  uint32_t orange = 0xFF8000;

  for (int y = 4; y >= 0; --y) {
    bool fillRow = (4 - y) < filledRows;
    for (int x = 0; x < 5; ++x) {
      if (fillRow) {
        M5.dis.drawpix(x, y, orange);
      } else {
        if (x != 2) M5.dis.drawpix(x, y, 0x000000);
      }
    }
  }
  drawI(0x00FF00);
}

// -----------------------------------------------------------------------------
// Netzwerk zu deinem Switch
// -----------------------------------------------------------------------------
void sendGet(const char* url) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  Serial.printf("GET %s -> HTTP %d\n", url, code);
  http.end();
}

void sendOff()    { sendGet(URL_OFF); }
void sendToggle() { sendGet(URL_TOGGLE); }

// -----------------------------------------------------------------------------
// WiFi + Webserver
// -----------------------------------------------------------------------------
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

// HTML mit Bootstrap + JS für Live-Status [web:251][web:266]
String htmlPage() {
  String ip = WiFi.localIP().toString();

  String s;
  s += "<!doctype html><html lang='en'><head>";
  s += "<meta charset='utf-8'>";
  s += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  s += "<title>M5 Switch Control</title>";
  s += "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'>";
  s += "<link href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.css' rel='stylesheet'>"; // icons [web:295][web:303]
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
  s += "</style>";
  s += "</head><body>";
  s += "<div class='card-shell'>";
  s += "  <div class='glass-card'>";
  s += "    <div class='card-header-gradient text-white px-4 py-3 d-flex align-items-center justify-content-between'>";
  s += "      <div>";
  s += "        <div class='fw-semibold'>M5 Switch Controller</div>";
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
  s += "            <span class='status-label'>Override</span>";
  s += "            <span id='overrideBadge' class='status-value chip bg-secondary text-light mt-1'>?</span>";
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
  s += "        <button id='btnOverride' class='btn btn-pill btn-sm btn-outline-info btn-icon' type='button'>";
  s += "          <i class='bi bi-slash-circle'></i> Toggle override";
  s += "        </button>";
  s += "        <button id='btnOffNow' class='btn btn-pill btn-sm btn-outline-danger btn-icon' type='button'>";
  s += "          <i class='bi bi-power'></i> Immediate OFF";
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
  s += "</div>";

  s += "<script>";
  s += "async function apiCall(path){try{await fetch(path);}catch(e){console.error(e);}}";
  s += "document.getElementById('btnOverride').onclick=function(){apiCall('/api/override');};";
  s += "document.getElementById('btnOffNow').onclick=function(){apiCall('/api/off_now');};";
  s += "document.getElementById('btnToggle').onclick=function(){apiCall('/api/toggle');};";

  s += "async function refreshStatus(){";
  s += "  try{const r=await fetch('/api/status');";
  s += "    if(!r.ok)return;const j=await r.json();";
  s += "    const ov=j.override;const tmr=j.timer;";
  s += "    const rem=j.remaining_ms;const total=j.total_ms;";
  s += "    const ovBadge=document.getElementById('overrideBadge');";
  s += "    const tBadge=document.getElementById('timerBadge');";
  s += "    const timeSpan=document.getElementById('timeRemaining');";
  s += "    const bar=document.getElementById('timerProgress');";

  // override pill
  s += "    ovBadge.textContent = ov ? 'ON' : 'OFF';";
  s += "    ovBadge.className = 'status-value chip ' + (ov ? 'bg-danger' : 'bg-success');";

  // timer pill
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
  s += "  }catch(e){console.error(e);}";
  s += "}";
  s += "setInterval(refreshStatus,500);";
  s += "refreshStatus();";
  s += "</script>";

  s += "</body></html>";
  return s;
}


// -----------------------------------------------------------------------------
// Webserver-Setup
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
    json += "\"override\":" + String(overrideEnabled ? "true" : "false") + ",";
    json += "\"timer\":"    + String(timer ? "true" : "false") + ",";
    json += "\"remaining_ms\":" + String(remaining) + ",";
    json += "\"total_ms\":" + String(OFF_DELAY_MS);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/api/override", HTTP_GET, []() {
    overrideEnabled = !overrideEnabled;
    offTimerRunning = false;
    if (overrideEnabled) showOverrideOn();
    else showOverrideOffBase();
    server.send(200, "text/plain", overrideEnabled ? "override=ON" : "override=OFF");
  });

  server.on("/api/off_now", HTTP_GET, []() {
    offTimerRunning = false;
    sendOff();
    clearMatrix();
    M5.dis.fillpix(0x330000);
    drawI(0xFF0000);
    server.send(200, "text/plain", "off_now=OK");
  });

  server.on("/api/toggle", HTTP_GET, []() {
    sendToggle();
    server.send(200, "text/plain", "toggle=OK");
  });

  server.begin();
  Serial.println("HTTP server started");
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  M5.begin(true, false, true);

  pinMode(INPUT_PIN,          INPUT_PULLUP);
  pinMode(INPUT_PIN_OVERRIDE, INPUT_PULLUP);

  ensureWifi();

  lastState         = digitalRead(INPUT_PIN);
  overrideLastState = digitalRead(INPUT_PIN_OVERRIDE);

  overrideEnabled = false;
  showOverrideOffBase();

  startWebServer();
}

// -----------------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------------
void loop() {
  ensureWifi();
  server.handleClient();

  uint32_t now = millis();

  // Override-Button physisch: Entprellung + Doppelklick
  bool ovReading = digitalRead(INPUT_PIN_OVERRIDE);
  if (ovReading != overrideLastState && (now - overrideLastChangeMs) > DEBOUNCE_MS) {
    overrideLastChangeMs = now;
    overrideLastState = ovReading;

    if (ovReading == LOW) {
      if (now - overrideLastClickTime > DOUBLE_CLICK_MS) {
        overrideClickCount = 1;
        overrideLastClickTime = now;
      } else {
        overrideClickCount++;
      }

      if (overrideClickCount == 2) {
        Serial.println("Override DOUBLE-CLICK -> sofort AUS senden");
        offTimerRunning = false;
        sendOff();
        clearMatrix();
        M5.dis.fillpix(0x330000);
        drawI(0xFF0000);
        overrideClickCount = 0;
        overrideLastClickTime = 0;
      } else {
        overrideEnabled = !overrideEnabled;
        offTimerRunning = false;
        if (overrideEnabled) showOverrideOn();
        else showOverrideOffBase();
      }
    }
  }

  if (overrideClickCount == 1 && (now - overrideLastClickTime > DOUBLE_CLICK_MS)) {
    overrideClickCount = 0;
    overrideLastClickTime = 0;
  }

  // Eingang 23 nur wenn Override AUS
  if (!overrideEnabled) {
    bool s = digitalRead(INPUT_PIN);

    if (s != lastState && (now - lastChangeMs) > DEBOUNCE_MS) {
      lastChangeMs = now;
      lastState = s;

      if (s == LOW) {
        offTimerRunning = true;
        offTimerStart   = now;
      } else {
        offTimerRunning = false;
        showOverrideOffBase();
      }
    }
  } else {
    if (offTimerRunning) offTimerRunning = false;
  }

  // Timer + Progressbar
  if (offTimerRunning) {
    uint32_t elapsed = now - offTimerStart;
    if (elapsed >= OFF_DELAY_MS) {
      offTimerRunning = false;
      sendOff();
      clearMatrix();
      M5.dis.fillpix(0x330000);
      drawI(0xFF0000);
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
