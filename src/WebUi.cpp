#include <Arduino.h>
#include <WiFi.h>
#include <M5Atom.h>
#include "WebUi.h"
#include "LedDisplay.h" // für clearMatrix, drawI, showAutoOff*


// Globale States aus main.cpp:
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

// Funktionen aus main.cpp:
void sendOff();
void sendOn();
void sendToggle();

String htmlPage()
{
    String ip = WiFi.localIP().toString();

    String s;
    s += "<!doctype html><html lang='en'><head>";
    s += "<meta charset='utf-8'>";
    s += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    s += "<title>Core One Auto Power Off</title>";
    s += "<link href='https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css' rel='stylesheet'>";
    s += "<link href='https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.css' rel='stylesheet'>";
    s += "<style>";
    s += "body{min-height:100vh;margin:0;";
    s += "background:radial-gradient(circle at top,#111827 0,#020617 55%,#000 100%);";
    s += "color:#e5e7eb;font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;}";
    s += ".card-shell{max-width:520px;margin:32px auto;padding:0 12px;}";
    s += ".glass-card{background:rgba(15,23,42,0.9);";
    s += "border-radius:18px;border:1px solid rgba(148,163,184,0.35);";
    s += "box-shadow:0 18px 45px rgba(0,0,0,0.65);backdrop-filter:blur(16px);}";
    s += ".card-header-gradient{border-bottom:1px solid rgba(148,163,184,0.35);";
    s += "background:linear-gradient(120deg,#F96831,#F96831,#F5F5F5);}"; // Prusa [web:400][web:419]
    s += ".chip{border-radius:999px;padding:4px 10px;font-size:0.78rem;}";
    s += ".btn-pill{border-radius:999px;font-weight:500;}";
    s += ".btn-icon i{margin-right:6px;}";
    s += ".status-label{font-size:0.8rem;color:#9ca3af;}";
    s += ".status-value{font-weight:600;}";
    s += ".progress{background:rgba(15,23,42,0.9);border-radius:999px;}";
    s += ".progress-bar{border-radius:999px;background-color:#F96831;}";
    s += ".badge-soft{background:rgba(15,23,42,0.85);border:1px solid rgba(148,163,184,0.6);}";
    s += "a,button{outline:none!important;box-shadow:none!important;}";
    s += ".list-group-item{border-color:rgba(55,65,81,0.7);}";

    s += ".btn-outline-info{color:#F96831;border-color:#F96831;}";
    s += ".btn-outline-info:hover{background-color:#F96831;color:#000;border-color:#F96831;}";
    s += ".btn-outline-danger{color:#F96831;border-color:#F96831;}";
    s += ".btn-outline-danger:hover{background-color:#F96831;color:#000;border-color:#F96831;}";
    s += ".btn-outline-success{color:#F96831;border-color:#F96831;}";
    s += ".btn-outline-success:hover{background-color:#F96831;color:#000;border-color:#F96831;}";
    s += ".btn-outline-light{color:#F5F5F5;border-color:#F5F5F5;}";
    s += ".btn-outline-light:hover{background-color:#F5F5F5;color:#111827;border-color:#F5F5F5;}";

    s += ".chip-auto-on{background-color:#F96831;color:#111827;}";
    s += ".chip-auto-off{background-color:#111827;color:#F5F5F5;border:1px solid #4b5563;}";

    s += "</style>";
    s += "</head><body>";
    s += "<div class='card-shell'>";
    s += "  <div class='glass-card'>";
    s += "    <div class='card-header-gradient text-white px-4 py-3 d-flex align-items-center justify-content-between'>";
    s += "      <div>";
    s += "        <div class='fw-semibold'>Core One Auto Power Off</div>";
    s += "        <div class='small text-white-50'>Local M5Stack control & timer</div>";
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
    s += "            <span id='modeBadge' class='status-value chip chip-auto-off mt-1'>?</span>";
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
    s += "        <div id='timerProgress' class='progress-bar' role='progressbar' style='width:0%;'></div>";
    s += "      </div>";

    s += "      <div class='d-grid gap-2 mb-2'>";
    s += "        <button id='btnMode' class='btn btn-pill btn-sm btn-outline-light btn-icon' type='button'>";
    s += "          <i class='bi bi-clock-history'></i> Toggle auto power off";
    s += "        </button>";
    s += "        <button id='btnOffNow' class='btn btn-pill btn-sm btn-outline-light btn-icon' type='button'>";
    s += "          <i class='bi bi-power'></i> Power OFF";
    s += "        </button>";
    s += "        <button id='btnOnNow' class='btn btn-pill btn-sm btn-outline-light btn-icon' type='button'>";
    s += "          <i class='bi bi-lightning-charge'></i> Power ON";
    s += "        </button>";
    s += "        <button id='btnToggle' class='btn btn-pill btn-sm btn-outline-light btn-icon' type='button'>";
    s += "          <i class='bi bi-arrow-repeat'></i> Power TOGGLE";
    s += "        </button>";
    s += "      </div>";
    s += "      <div class='text-center mt-2'>";
    s += "        <small class='text-muted'>Updated live from M5 via JSON / AJAX</small>";
    s += "      </div>";
    s += "    </div>";
    s += "  </div>";

    // Slider
    s += "  <div class='glass-card mt-3'>";
    s += "    <div class='px-4 pt-3 pb-3'>";
    s += "      <div class='mb-3'>";
    s += "        <label class='status-label'>Auto-off Timer (min)</label>";
    s += "        <input id='timerSlider' type='range' class='form-range' min='1' max='240' value='10'>";
    s += "        <div class='small'><span id='timerSliderValue'>10</span> min</div>";
    s += "      </div>";
    s += "    </div>";
    s += "  </div>";

    // IP Configuration Card
    s += "  <div class='glass-card mt-3'>";
    s += "    <div class='px-4 pt-3 pb-3'>";
    s += "      <div class='mb-3'>";
    s += "        <label class='status-label'>Relay IP Address</label>";
    s += "        <div class='input-group input-group-sm'>";
    s += "          <input id='relayIpInput' type='text' class='form-control' placeholder='192.168.188.44' value='' style='background:rgba(15,23,42,0.9);border:1px solid rgba(148,163,184,0.6);color:#e5e7eb;'>";
    s += "          <button id='btnSaveIp' class='btn btn-outline-light btn-sm' type='button' style='border-radius:0 999px 999px 0;'>";
    s += "            <i class='bi bi-check-lg'></i> Save";
    s += "          </button>";
    s += "        </div>";
    s += "        <div class='small text-muted mt-1'>Configure target relay device IP</div>";
    s += "      </div>";
    s += "    </div>";
    s += "  </div>";

    // Relay status card
    s += "  <div class='glass-card mt-3'>";
    s += "    <div class='px-4 pt-3 pb-3'>";
    s += "      <div class='d-flex justify-content-between align-items-center mb-2'>";
    s += "        <div>";
    s += "          <div class='fw-semibold'>Relay status</div>";
    s += "          <div class='status-label'>Reported from <span id='relayIpDisplay'>-</span></div>";
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
    s += "      </ul>";
    s += "      <div class='mt-2 text-end'>";
    s += "        <small id='valReportState' class='text-muted'>report: unknown</small>";
    s += "      </div>";
    s += "    </div>";
    s += "  </div>";

    s += "</div>";

    // JS
    s += "<script>";
    s += "async function apiCall(path){try{await fetch(path);}catch(e){console.error(e);}}";
    s += "document.getElementById('btnMode').onclick=function(){apiCall('/api/mode');};";
    s += "document.getElementById('btnOffNow').onclick=function(){apiCall('/api/off_now');};";
    s += "document.getElementById('btnOnNow').onclick=function(){apiCall('/api/on_now');};";
    s += "document.getElementById('btnToggle').onclick=function(){apiCall('/api/toggle');};";

    // IP Save button
    s += "document.getElementById('btnSaveIp').onclick=async function(){";
    s += "  const ip=document.getElementById('relayIpInput').value.trim();";
    s += "  if(!ip){alert('Please enter a valid IP address');return;}";
    s += "  try{await fetch('/api/set_relay_ip?ip='+encodeURIComponent(ip));}catch(e){console.error(e);}";
    s += "};";

    // Slider-Setup
    s += "const slider=document.getElementById('timerSlider');";
    s += "const sliderVal=document.getElementById('timerSliderValue');";
    s += "slider.oninput=()=>{sliderVal.textContent=slider.value;};";
    s += "slider.onchange=async()=>{";
    s += "  try{await fetch('/api/set_timer?minutes='+slider.value);}catch(e){console.error(e);}";
    s += "};";

    // Status-Refresh
    s += "async function refreshStatus(){";
    s += "  try{const r=await fetch('/api/status');";
    s += "    if(!r.ok)return;const j=await r.json();";
    s += "    const autoMode=j.auto_mode;const tmr=j.timer;";
    s += "    const rem=j.remaining_ms;const total=j.total_ms;";
    s += "    const timerMin=j.timer_minutes;";
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
    s += "    const relayIpInput=document.getElementById('relayIpInput');";
    s += "    const relayIpDisplay=document.getElementById('relayIpDisplay');";

    s += "    const rv=j.report_valid;";
    s += "    const relay=j.relay;";

    s += "    if(j.relay_ip){";
    s += "      if(document.activeElement!==relayIpInput){";
    s += "        relayIpInput.value=j.relay_ip;";
    s += "      }";
    s += "      relayIpDisplay.textContent=j.relay_ip;";
    s += "    }";

    s += "    modeBadge.textContent = autoMode ? 'ON' : 'OFF';";
    s += "    modeBadge.className = 'status-value chip ' + (autoMode ? 'chip-auto-on' : 'chip-auto-off');";

    s += "    tBadge.textContent = tmr ? 'RUNNING' : 'STOPPED';";
    s += "    tBadge.className = 'status-value chip ' + (tmr ? 'bg-warning text-dark' : 'bg-secondary');";

    s += "    let txt='-';let pct=0;";
    s += "    if(tmr){";
    s += "      const sec=Math.max(0,Math.floor(rem/1000));";
    s += "      const m=Math.floor(sec/60);";
    s += "      const s2=sec%60;";
    s += "      txt=(m.toString().padStart(2,'0')+':'+s2.toString().padStart(2,'0'))+' min';";
    s += "      pct = Math.min(100, Math.max(0, 100*(total-rem)/total));";
    s += "    }";
    s += "    timeSpan.textContent=txt;";
    s += "    bar.style.width=pct+'%';";

    // Slider sync
    s += "    if(typeof timerMin === 'number' && !isNaN(timerMin)){";
    s += "      slider.value = timerMin;";
    s += "      sliderVal.textContent = timerMin;";
    s += "    }";

    s += "    valReportState.textContent = rv ? 'report: OK' : 'report: error';";
    s += "    if(rv){";
    s += "      relayBadge.textContent = relay ? 'ON' : 'OFF';";
    s += "      relayBadge.className = 'chip ' + (relay ? 'bg-success text-light' : 'bg-secondary text-light');";
    s += "      valPower.textContent  = j.power.toFixed(2) + ' W';";
    s += "      valWs.textContent     = j.ws.toFixed(2);";
    s += "      valTemp.textContent   = j.temperature.toFixed(2) + ' °C';";
    s += "      valEnergy.textContent = j.energy_boot.toFixed(2);";

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

void startWebServer()
{
    server.on("/", HTTP_GET, []()
              { server.send(200, "text/html", htmlPage()); });

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
        json += "\"relay_ip\":\""    + relayIpAddress + "\"";
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
        prefs.putUInt("off_delay_ms", offDelayMs);  // Key ohne Leerzeichen
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

        server.send(200, "text/plain", "ok"); });

    server.begin();
    Serial.println("HTTP server started");
}
