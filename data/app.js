/**
 * @file app.js
 * @brief Client-side JavaScript for M5Stack Atom web interface
 * @author Steff8583
 * @date 02.01.2026
 * 
 * Handles all client-side interactions including:
 * - API calls to control relay and timer
 * - Live status updates via polling
 * - UI state management and visual feedback
 * - WiFi configuration and reset
 * 
 * API Endpoints:
 * - GET /api/status - Fetch current state (polled every 500ms)
 * - GET /api/mode - Toggle auto power-off mode
 * - GET /api/off_now - Power off relay immediately
 * - GET /api/on_now - Power on relay
 * - GET /api/toggle - Toggle relay state
 * - GET /api/set_timer?minutes=X - Set timer duration
 * - GET /api/set_relay_ip?ip=X - Configure relay IP address
 * - GET /api/reset_wifi - Reset WiFi settings and restart
 */

// ============================================================================
// API Communication Functions
// ============================================================================

/**
 * @brief Send API request without waiting for response
 * @param {string} path - API endpoint path
 */
async function apiCall(path) {
  try {
    await fetch(path);
  } catch (e) {
    console.error(e);
  }
}

// ============================================================================
// Button Event Handlers
// ============================================================================

/**
 * Toggle auto power-off mode
 */
document.getElementById('btnMode').onclick = function() {
  apiCall('/api/mode');
};

/**
 * Power off relay immediately
 */
document.getElementById('btnOffNow').onclick = function() {
  apiCall('/api/off_now');
};

/**
 * Power on relay
 */
document.getElementById('btnOnNow').onclick = function() {
  apiCall('/api/on_now');
};

/**
 * Toggle relay state
 */
document.getElementById('btnToggle').onclick = function() {
  apiCall('/api/toggle');
};

// ============================================================================
// Configuration Handlers
// ============================================================================

/**
 * Save relay IP address to device NVS storage
 */
document.getElementById('btnSaveIp').onclick = async function() {
  const ip = document.getElementById('relayIpInput').value.trim();
  if (!ip) {
    alert('Please enter a valid IP address');
    return;
  }
  try {
    await fetch('/api/set_relay_ip?ip=' + encodeURIComponent(ip));
  } catch (e) {
    console.error(e);
  }
};

/**
 * Reset WiFi settings and restart device into config portal mode
 * Device will create "M5Stack-AutoOff" AP for reconfiguration
 */
document.getElementById('btnResetWifi').onclick = async function() {
  if (!confirm('This will reset WiFi settings and restart the device. You will need to reconnect to the "M5Stack-AutoOff" access point to reconfigure. Continue?')) {
    return;
  }
  try {
    await fetch('/api/reset_wifi');
    alert('Device is restarting. Please connect to "M5Stack-AutoOff" WiFi network to reconfigure.');
  } catch (e) {
    console.error(e);
  }
};

// ============================================================================
// Timer Configuration Slider
// ============================================================================

/**
 * Timer delay slider (1-240 minutes)
 * Updates display value on input, saves to device on change
 */
const slider = document.getElementById('timerSlider');
const sliderVal = document.getElementById('timerSliderValue');
slider.oninput = () => {
  sliderVal.textContent = slider.value;
};
slider.onchange = async () => {
  try {
    await fetch('/api/set_timer?minutes=' + slider.value);
  } catch (e) {
    console.error(e);
  }
};

// ============================================================================
// Live Status Updates
// ============================================================================

/**
 * @brief Fetch and update all status information from device
 * 
 * Updates:
 * - Auto-off mode status
 * - Timer state and remaining time
 * - Relay state (ON/OFF)
 * - Progress bar visualization
 * - Relay report data (power, temperature, etc.)
 * - WiFi and IP information
 * 
 * Called every 500ms by setInterval
 */
async function refreshStatus() {
  try {
    const r = await fetch('/api/status');
    if (!r.ok) return;
    const j = await r.json();
    
    const autoMode = j.auto_mode;
    const tmr = j.timer;
    const rem = j.remaining_ms;
    const total = j.total_ms;
    const timerMin = j.timer_minutes;
    
    const modeBadge = document.getElementById('modeBadge');
    const tBadge = document.getElementById('timerBadge');
    const timeSpan = document.getElementById('timeRemaining');
    const bar = document.getElementById('timerProgress');
    const relayBadge = document.getElementById('relayBadge');
    const valPower = document.getElementById('valPower');
    const valWs = document.getElementById('valWs');
    const valTemp = document.getElementById('valTemp');
    const valEnergy = document.getElementById('valEnergy');
    const valTimeBoot = document.getElementById('valTimeBoot');
    const valReportState = document.getElementById('valReportState');
    const relayIpInput = document.getElementById('relayIpInput');
    const relayIpDisplay = document.getElementById('relayIpDisplay');
    const relayStateBadge = document.getElementById('relayStateBadge');
    const deviceIp = document.getElementById('deviceIp');
    const wifiSSID = document.getElementById('wifiSSID');
    
    const rv = j.report_valid;
    const relay = j.relay;
    
    // Update device IP
    if (j.device_ip) {
      deviceIp.textContent = j.device_ip;
    }
    
    // Update WiFi SSID
    if (j.wifi_ssid) {
      wifiSSID.textContent = j.wifi_ssid;
    }
    
    // Update relay IP
    if (j.relay_ip) {
      if (document.activeElement !== relayIpInput) {
        relayIpInput.value = j.relay_ip;
      }
      relayIpDisplay.textContent = j.relay_ip;
    }
    
    // Update auto mode badge
    modeBadge.textContent = autoMode ? 'ON' : 'OFF';
    modeBadge.className = 'status-value chip ' + (autoMode ? 'chip-auto-on' : 'chip-auto-off');
    
    // Update timer badge
    tBadge.textContent = tmr ? 'RUNNING' : 'STOPPED';
    tBadge.className = 'status-value chip ' + (tmr ? 'bg-warning text-dark' : 'bg-secondary');
    
    // Update remaining time and progress bar
    let txt = '-';
    let pct = 0;
    if (tmr) {
      const sec = Math.max(0, Math.floor(rem / 1000));
      const m = Math.floor(sec / 60);
      const s2 = sec % 60;
      txt = (m.toString().padStart(2, '0') + ':' + s2.toString().padStart(2, '0')) + ' min';
      pct = Math.min(100, Math.max(0, 100 * (total - rem) / total));
    }
    timeSpan.textContent = txt;
    bar.style.width = pct + '%';
    
    // Sync slider value
    if (typeof timerMin === 'number' && !isNaN(timerMin)) {
      slider.value = timerMin;
      sliderVal.textContent = timerMin;
    }
    
    // Update report status
    valReportState.textContent = rv ? 'report: OK' : 'report: error';
    
    if (rv) {
      relayStateBadge.textContent = relay ? 'ON' : 'OFF';
      relayStateBadge.className = 'status-value chip ' + (relay ? 'bg-success text-light' : 'bg-danger text-light');
      relayBadge.textContent = relay ? 'ON' : 'OFF';
      relayBadge.className = 'chip ' + (relay ? 'bg-success text-light' : 'bg-secondary text-light');
      valPower.textContent = j.power.toFixed(2) + ' W';
      valWs.textContent = j.ws.toFixed(2);
      valTemp.textContent = j.temperature.toFixed(2) + ' °C';
      valEnergy.textContent = j.energy_boot.toFixed(2);
      
      let tb = j.time_boot;
      let secs = tb;
      const days = Math.floor(secs / 86400);
      secs %= 86400;
      const hours = Math.floor(secs / 3600);
      secs %= 3600;
      const mins = Math.floor(secs / 60);
      secs %= 60;
      let tbStr = days + 'd ' + hours + 'h ' + mins + 'm ' + secs + 's';
      valTimeBoot.textContent = tbStr;
    } else {
      relayStateBadge.textContent = '?';
      relayStateBadge.className = 'status-value chip bg-secondary text-light';
      relayBadge.textContent = '?';
      relayBadge.className = 'chip bg-secondary text-light';
      valPower.textContent = valWs.textContent = valTemp.textContent = '-';
      valEnergy.textContent = valTimeBoot.textContent = '-';
    }
  } catch (e) {
    console.error(e);
  }
}

// ============================================================================
// Initialization
// ============================================================================

/**
 * Start automatic status polling and perform initial update
 */
setInterval(refreshStatus, 500);
refreshStatus();
