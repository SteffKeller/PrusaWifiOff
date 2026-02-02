/**
 * @file app.js
 * @brief Modern client-side JavaScript for M5Stack Atom web interface
 * @author Steff8583
 * @date 02.01.2026
 * 
 * Modern architecture with ES6+ and debugging support
 */

'use strict';

// ============================================================================
// API Service - Handles all HTTP communication
// ============================================================================

class ApiService {
  constructor(baseUrl = '') {
    this.baseUrl = baseUrl;
    this.retryAttempts = 3;
    this.retryDelay = 1000;
  }

  async request(path, options = {}) {
    let lastError;
    
    for (let attempt = 0; attempt < this.retryAttempts; attempt++) {
      try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 5000);
        
        const response = await fetch(this.baseUrl + path, {
          ...options,
          signal: controller.signal
        });
        
        clearTimeout(timeoutId);
        
        if (!response.ok) {
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        return response;
      } catch (error) {
        lastError = error;
        console.warn(`[API] Failed (attempt ${attempt + 1}/${this.retryAttempts}):`, path, error.message);
        
        if (attempt < this.retryAttempts - 1) {
          await new Promise(resolve => setTimeout(resolve, this.retryDelay));
        }
      }
    }
    
    console.error('[API] All retries failed:', path, lastError);
    throw lastError;
  }

  async call(path) {
    try {
      await this.request(path);
    } catch (error) {
      console.error('[API] Call failed:', path, error);
    }
  }

  async getJson(path) {
    const response = await this.request(path);
    return response.json();
  }
}

// ============================================================================
// Application State - Central state management
// ============================================================================

class AppState {
  constructor() {
    this.state = {
      autoMode: false,
      timerRunning: false,
      remainingMs: 0,
      totalMs: 0,
      timerMinutes: 10,
      relayState: false,
      reportValid: false,
      power: 0,
      ws: 0,
      temperature: 0,
      energyBoot: 0,
      timeBoot: 0,
      relayIp: '',
      deviceIp: '',
      wifiSsid: '',
      loggingEnabled: false,
      logCount: 0,
      logMaxCount: 0,
      logDuration: 0
    };
    
    this.listeners = new Set();
  }

  subscribe(callback) {
    this.listeners.add(callback);
    return () => this.listeners.delete(callback);
  }

  update(updates) {
    const hasChanges = Object.keys(updates).some(
      key => this.state[key] !== updates[key]
    );
    
    if (hasChanges) {
      this.state = { ...this.state, ...updates };
      this.notify();
    }
  }

  notify() {
    this.listeners.forEach(callback => {
      try {
        callback(this.state);
      } catch (error) {
        console.error('[State] Listener error:', error);
      }
    });
  }

  getState() {
    return { ...this.state };
  }
}

// ============================================================================
// UI Controller - Manages all DOM interactions
// ============================================================================

class UIController {
  constructor(api, state) {
    this.api = api;
    this.state = state;
    this.elements = this.cacheElements();
    this.setupEventListeners();
    this.subscribeToState();
    console.log('[UI] Controller initialized');
  }

  cacheElements() {
    return {
      modeBadge: document.getElementById('modeBadge'),
      timerBadge: document.getElementById('timerBadge'),
      relayStateBadge: document.getElementById('relayStateBadge'),
      relayBadge: document.getElementById('relayBadge'),
      logStatusBadge: document.getElementById('logStatusBadge'),
      timeRemaining: document.getElementById('timeRemaining'),
      timerProgress: document.getElementById('timerProgress'),
      valPower: document.getElementById('valPower'),
      valWs: document.getElementById('valWs'),
      valTemp: document.getElementById('valTemp'),
      valEnergy: document.getElementById('valEnergy'),
      valTimeBoot: document.getElementById('valTimeBoot'),
      valReportState: document.getElementById('valReportState'),
      relayIpInput: document.getElementById('relayIpInput'),
      relayIpDisplay: document.getElementById('relayIpDisplay'),
      deviceIp: document.getElementById('deviceIp'),
      wifiSSID: document.getElementById('wifiSSID'),
      relayWarning: document.getElementById('relayWarning'),
      relayWarningIp: document.getElementById('relayWarningIp'),
      timerSlider: document.getElementById('timerSlider'),
      timerSliderValue: document.getElementById('timerSliderValue'),
      btnMode: document.getElementById('btnMode'),
      btnOffNow: document.getElementById('btnOffNow'),
      btnOnNow: document.getElementById('btnOnNow'),
      btnToggle: document.getElementById('btnToggle'),
      btnSaveIp: document.getElementById('btnSaveIp'),
      btnResetWifi: document.getElementById('btnResetWifi'),
      btnSaveAuth: document.getElementById('btnSaveAuth'),
      btnSaveTariff: document.getElementById('btnSaveTariff'),
      btnStartLog: document.getElementById('btnStartLog'),
      btnStopLog: document.getElementById('btnStopLog'),
      btnClearLog: document.getElementById('btnClearLog'),
      btnSaveAutoLog: document.getElementById('btnSaveAutoLog'),
      autoLogEnabled: document.getElementById('autoLogEnabled'),
      autoLogThreshold: document.getElementById('autoLogThreshold'),
      autoLogDebounce: document.getElementById('autoLogDebounce'),
      tariffHigh: document.getElementById('tariffHigh'),
      tariffLow: document.getElementById('tariffLow'),
      tariffCurrency: document.getElementById('tariffCurrency'),
      tariffStartHour: document.getElementById('tariffStartHour'),
      tariffEndHour: document.getElementById('tariffEndHour'),
      authUsername: document.getElementById('authUsername'),
      authPassword: document.getElementById('authPassword'),
      logStats: document.getElementById('logStats'),
      logCount: document.getElementById('logCount'),
      logDuration: document.getElementById('logDuration'),
      logTotalEnergy: document.getElementById('logTotalEnergy')
    };
  }

  setupEventListeners() {
    if (this.elements.btnMode) {
      this.elements.btnMode.addEventListener('click', () => this.api.call('/api/mode'));
    }
    
    if (this.elements.btnOffNow) {
      this.elements.btnOffNow.addEventListener('click', () => this.api.call('/api/off_now'));
    }
    
    if (this.elements.btnOnNow) {
      this.elements.btnOnNow.addEventListener('click', () => this.api.call('/api/on_now'));
    }
    
    if (this.elements.btnToggle) {
      this.elements.btnToggle.addEventListener('click', () => this.api.call('/api/toggle'));
    }
    
    if (this.elements.btnSaveIp) {
      this.elements.btnSaveIp.addEventListener('click', () => this.saveRelayIp());
    }
    
    if (this.elements.btnResetWifi) {
      this.elements.btnResetWifi.addEventListener('click', () => this.resetWifi());
    }
    
    if (this.elements.btnSaveAuth) {
      this.elements.btnSaveAuth.addEventListener('click', () => this.saveAuth());
    }
    
    if (this.elements.btnSaveTariff) {
      this.elements.btnSaveTariff.addEventListener('click', () => this.saveTariff());
    }
    
    if (this.elements.btnStartLog) {
      this.elements.btnStartLog.addEventListener('click', () => this.startLogging());
    }
    
    if (this.elements.btnStopLog) {
      this.elements.btnStopLog.addEventListener('click', () => this.stopLogging());
    }
    
    if (this.elements.btnClearLog) {
      this.elements.btnClearLog.addEventListener('click', () => this.clearLog());
    }
    
    if (this.elements.btnSaveAutoLog) {
      this.elements.btnSaveAutoLog.addEventListener('click', () => this.saveAutoLog());
    }
    
    if (this.elements.timerSlider) {
      this.elements.timerSlider.addEventListener('input', (e) => {
        this.elements.timerSliderValue.textContent = e.target.value;
      });
      
      this.elements.timerSlider.addEventListener('change', (e) => {
        this.api.call(`/api/set_timer?minutes=${e.target.value}`);
      });
    }
    
    this.setupCollapseAnimations();
  }

  subscribeToState() {
    this.state.subscribe((state) => this.render(state));
  }

  render(state) {
    this.renderAutoMode(state);
    this.renderTimer(state);
    this.renderRelay(state);
    this.renderRelayReport(state);
    this.renderNetworkInfo(state);
    this.renderWarnings(state);
    this.renderLogging(state);
  }

  renderAutoMode(state) {
    if (!this.elements.modeBadge) return;
    
    this.elements.modeBadge.textContent = state.autoMode ? 'ON' : 'OFF';
    this.elements.modeBadge.className = `status-value chip ${
      state.autoMode ? 'chip-auto-on' : 'chip-auto-off'
    }`;
  }

  renderTimer(state) {
    if (!this.elements.timerBadge) return;
    
    this.elements.timerBadge.textContent = state.timerRunning ? 'RUNNING' : 'STOPPED';
    this.elements.timerBadge.className = `status-value chip ${
      state.timerRunning ? 'bg-warning text-dark' : 'bg-secondary'
    }`;
    
    let timeText = '-';
    let progressPct = 0;
    
    if (state.timerRunning && state.totalMs > 0) {
      const sec = Math.max(0, Math.floor(state.remainingMs / 1000));
      const m = Math.floor(sec / 60);
      const s = sec % 60;
      timeText = `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')} min`;
      progressPct = Math.min(100, Math.max(0, 100 * (state.totalMs - state.remainingMs) / state.totalMs));
    }
    
    if (this.elements.timeRemaining) {
      this.elements.timeRemaining.textContent = timeText;
    }
    
    if (this.elements.timerProgress) {
      this.elements.timerProgress.style.width = `${progressPct}%`;
    }
    
    if (this.elements.timerSlider && state.timerMinutes > 0) {
      this.elements.timerSlider.value = state.timerMinutes;
      this.elements.timerSliderValue.textContent = state.timerMinutes;
    }
  }

  renderRelay(state) {
    const isOn = state.reportValid ? state.relayState : null;
    const text = isOn === null ? '?' : (isOn ? 'ON' : 'OFF');
    const className = isOn === null ? 'bg-secondary' : (isOn ? 'bg-success' : 'bg-danger');
    
    if (this.elements.relayStateBadge) {
      this.elements.relayStateBadge.textContent = text;
      this.elements.relayStateBadge.className = `status-value chip ${className} text-light`;
    }
    
    if (this.elements.relayBadge) {
      this.elements.relayBadge.textContent = text;
      this.elements.relayBadge.className = `chip ${className} text-light`;
    }
  }

  renderRelayReport(state) {
    if (state.reportValid) {
      this.elements.valPower && (this.elements.valPower.textContent = `${state.power.toFixed(2)} W`);
      this.elements.valWs && (this.elements.valWs.textContent = state.ws.toFixed(2));
      this.elements.valTemp && (this.elements.valTemp.textContent = `${state.temperature.toFixed(2)} °C`);
      this.elements.valEnergy && (this.elements.valEnergy.textContent = state.energyBoot.toFixed(2));
      
      if (this.elements.valTimeBoot) {
        const days = Math.floor(state.timeBoot / 86400);
        const hours = Math.floor((state.timeBoot % 86400) / 3600);
        const mins = Math.floor((state.timeBoot % 3600) / 60);
        const secs = state.timeBoot % 60;
        this.elements.valTimeBoot.textContent = `${days}d ${hours}h ${mins}m ${secs}s`;
      }
      
      this.elements.valReportState && (this.elements.valReportState.textContent = 'report: OK');
    } else {
      this.elements.valPower && (this.elements.valPower.textContent = '-');
      this.elements.valWs && (this.elements.valWs.textContent = '-');
      this.elements.valTemp && (this.elements.valTemp.textContent = '-');
      this.elements.valEnergy && (this.elements.valEnergy.textContent = '-');
      this.elements.valTimeBoot && (this.elements.valTimeBoot.textContent = '-');
      this.elements.valReportState && (this.elements.valReportState.textContent = 'report: error');
    }
  }

  renderNetworkInfo(state) {
    if (state.deviceIp && this.elements.deviceIp) {
      this.elements.deviceIp.textContent = state.deviceIp;
    }
    
    if (state.wifiSsid && this.elements.wifiSSID) {
      this.elements.wifiSSID.textContent = state.wifiSsid;
    }
    
    if (state.relayIp) {
      if (this.elements.relayIpInput && document.activeElement !== this.elements.relayIpInput) {
        this.elements.relayIpInput.value = state.relayIp;
      }
      
      if (this.elements.relayIpDisplay) {
        this.elements.relayIpDisplay.textContent = state.relayIp;
      }
      
      if (this.elements.relayWarningIp) {
        this.elements.relayWarningIp.textContent = state.relayIp;
      }
    }
  }

  renderWarnings(state) {
    if (!this.elements.relayWarning) return;
    
    if (!state.reportValid && state.relayIp) {
      this.elements.relayWarning.style.display = 'block';
      
      if (this.elements.relayIpInput) {
        this.elements.relayIpInput.style.borderColor = 'rgba(249,104,49,0.8)';
        this.elements.relayIpInput.style.boxShadow = '0 0 0 0.25rem rgba(249,104,49,0.25)';
      }
    } else {
      this.elements.relayWarning.style.display = 'none';
      
      if (this.elements.relayIpInput) {
        this.elements.relayIpInput.style.borderColor = '';
        this.elements.relayIpInput.style.boxShadow = '';
      }
    }
  }

  renderLogging(state) {
    if (!this.elements.logStatusBadge) return;
    
    if (state.loggingEnabled) {
      this.elements.logStatusBadge.textContent = 'Recording';
      this.elements.logStatusBadge.className = 'chip bg-danger text-light';
      this.elements.btnStartLog.disabled = true;
      this.elements.btnStopLog.disabled = false;
      this.elements.logStats.style.display = 'block';
      
      this.elements.logCount.textContent = `${state.logCount} / ${state.logMaxCount}`;
      const minutes = Math.floor(state.logDuration / 60000);
      this.elements.logDuration.textContent = `${minutes}m`;
    } else {
      const hasData = state.logCount > 0;
      this.elements.logStatusBadge.textContent = hasData ? 'Stopped' : 'Idle';
      this.elements.logStatusBadge.className = hasData ? 'chip bg-warning text-dark' : 'chip bg-secondary text-light';
      this.elements.btnStartLog.disabled = false;
      this.elements.btnStopLog.disabled = true;
      
      if (hasData) {
        this.elements.logStats.style.display = 'block';
        this.elements.logCount.textContent = `${state.logCount} / ${state.logMaxCount}`;
      } else {
        this.elements.logStats.style.display = 'none';
      }
    }
  }

  async saveRelayIp() {
    const ip = this.elements.relayIpInput.value.trim();
    if (!ip) {
      alert('Please enter a valid IP address');
      return;
    }
    
    try {
      await this.api.call(`/api/set_relay_ip?ip=${encodeURIComponent(ip)}`);
      await new Promise(resolve => setTimeout(resolve, 500));
    } catch (error) {
      alert('Failed to save IP address');
    }
  }

  async resetWifi() {
    if (!confirm('This will reset WiFi settings and restart the device. You will need to reconnect to the "M5Stack-AutoOff" access point to reconfigure. Continue?')) {
      return;
    }
    
    try {
      await this.api.call('/api/reset_wifi');
      alert('Device is restarting. Please connect to "M5Stack-AutoOff" WiFi network to reconfigure.');
    } catch (error) {
      console.error('WiFi reset error:', error);
    }
  }

  async saveAuth() {
    const user = this.elements.authUsername.value.trim();
    const pass = this.elements.authPassword.value.trim();
    
    if (!user || !pass) {
      alert('Please enter both username and password');
      return;
    }
    
    if (user.length < 3) {
      alert('Username must be at least 3 characters');
      return;
    }
    
    if (pass.length < 4) {
      alert('Password must be at least 4 characters');
      return;
    }
    
    if (!confirm('Update authentication credentials? You will need to re-authenticate with the new credentials.')) {
      return;
    }
    
    try {
      await this.api.request(`/api/set_auth?user=${encodeURIComponent(user)}&pass=${encodeURIComponent(pass)}`);
      alert('Credentials updated successfully! Please re-authenticate when prompted.');
      this.elements.authPassword.value = '';
    } catch (error) {
      alert('Failed to update credentials: ' + error.message);
    }
  }

  async saveTariff() {
    const high = this.elements.tariffHigh.value;
    const low = this.elements.tariffLow.value;
    const curr = this.elements.tariffCurrency.value.trim();
    const start = this.elements.tariffStartHour.value;
    const end = this.elements.tariffEndHour.value;
    
    if (!high || !low || !curr) {
      alert('Please fill in all tariff fields');
      return;
    }
    
    if (parseFloat(high) <= 0 || parseFloat(low) <= 0) {
      alert('Tariff values must be positive numbers');
      return;
    }
    
    try {
      const url = `/api/tariff_set?high=${encodeURIComponent(high)}&low=${encodeURIComponent(low)}&currency=${encodeURIComponent(curr)}&start=${encodeURIComponent(start)}&end=${encodeURIComponent(end)}`;
      await this.api.request(url);
      alert('Tariff settings saved successfully!');
    } catch (error) {
      alert('Failed to save tariff settings: ' + error.message);
    }
  }

  async saveAutoLog() {
    const enabled = this.elements.autoLogEnabled.checked ? '1' : '0';
    const threshold = this.elements.autoLogThreshold.value;
    const debounce = this.elements.autoLogDebounce.value;
    
    if (!threshold || !debounce) {
      alert('Please fill in all auto-logging fields');
      return;
    }
    
    if (parseFloat(threshold) < 0) {
      alert('Threshold must be a positive number');
      return;
    }
    
    if (parseInt(debounce) < 0) {
      alert('Debounce time must be a positive number');
      return;
    }
    
    try {
      const url = `/api/autolog_set?enabled=${enabled}&threshold=${encodeURIComponent(threshold)}&debounce=${encodeURIComponent(debounce)}`;
      await this.api.call(url);
      console.log('[UI] Auto-logging saved');
      alert('Auto-logging settings saved successfully!');
    } catch (error) {
      console.error('[UI] Failed to save auto-logging:', error);
      alert('Failed to save auto-logging settings: ' + error.message);
    }
  }

  async loadAutoLog() {
    try {
      const data = await this.api.getJson('/api/autolog_get');
      if (data.enabled !== undefined) {
        this.elements.autoLogEnabled.checked = data.enabled;
      }
      if (data.threshold !== undefined) {
        this.elements.autoLogThreshold.value = data.threshold;
      }
      if (data.debounce !== undefined) {
        this.elements.autoLogDebounce.value = data.debounce;
      }
      console.log('[UI] Auto-logging settings loaded:', data);
    } catch (error) {
      console.error('[UI] Failed to load auto-logging settings:', error);
    }
  }

  async startLogging() {
    await this.api.call('/api/log_start');
  }

  async stopLogging() {
    await this.api.call('/api/log_stop');
  }

  async clearLog() {
    if (!confirm('Clear all logged data? This cannot be undone.')) return;
    
    await this.api.call('/api/log_clear');
    this.elements.logTotalEnergy.textContent = '0 Wh';
  }

  setupCollapseAnimations() {
    const collapseElements = document.querySelectorAll('.collapse');
    
    collapseElements.forEach(element => {
      element.addEventListener('show.bs.collapse', () => {
        const icon = element.parentElement.querySelector('.bi-chevron-down');
        if (icon) {
          icon.classList.replace('bi-chevron-down', 'bi-chevron-up');
        }
      });
      
      element.addEventListener('hide.bs.collapse', () => {
        const icon = element.parentElement.querySelector('.bi-chevron-up');
        if (icon) {
          icon.classList.replace('bi-chevron-up', 'bi-chevron-down');
        }
      });
    });
  }
}

// ============================================================================
// Status Poller
// ============================================================================

class StatusPoller {
  constructor(api, state, interval = 500) {
    this.api = api;
    this.state = state;
    this.interval = interval;
    this.timerId = null;
    this.isRunning = false;
  }

  start() {
    if (this.isRunning) return;
    
    this.isRunning = true;
    this.poll();
    this.timerId = setInterval(() => this.poll(), this.interval);
    console.log('[Poller] Status polling started');
  }

  stop() {
    if (this.timerId) {
      clearInterval(this.timerId);
      this.timerId = null;
    }
    this.isRunning = false;
  }

  async poll() {
    try {
      const data = await this.api.getJson('/api/status');
      
      this.state.update({
        autoMode: data.auto_mode,
        timerRunning: data.timer,
        remainingMs: data.remaining_ms,
        totalMs: data.total_ms,
        timerMinutes: data.timer_minutes,
        relayState: data.relay,
        reportValid: data.report_valid,
        power: data.power || 0,
        ws: data.ws || 0,
        temperature: data.temperature || 0,
        energyBoot: data.energy_boot || 0,
        timeBoot: data.time_boot || 0,
        relayIp: data.relay_ip || '',
        deviceIp: data.device_ip || '',
        wifiSsid: data.wifi_ssid || ''
      });
    } catch (error) {
      console.error('[Poller] Status poll error:', error);
    }
  }
}

// ============================================================================
// Power Graph
// ============================================================================

class PowerGraph {
  constructor(api, state) {
    this.api = api;
    this.state = state;
    this.chart = null;
    this.updateInterval = 2000;
    this.timerId = null;
  }

  init() {
    const ctx = document.getElementById('powerChart');
    if (!ctx) {
      console.warn('[Graph] powerChart element not found');
      return;
    }

    if (typeof Chart === 'undefined') {
      console.error('[Graph] Chart.js not loaded');
      return;
    }

    console.log('[Graph] Initializing Chart.js');

    this.chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: [],
        datasets: [
          {
            label: 'Power (W)',
            data: [],
            borderColor: '#F96831',
            backgroundColor: 'rgba(249, 104, 49, 0.1)',
            borderWidth: 2,
            tension: 0.3,
            fill: true,
            yAxisID: 'y'
          },
          {
            label: 'Energy (Wh)',
            data: [],
            borderColor: '#3B82F6',
            backgroundColor: 'rgba(59, 130, 246, 0.1)',
            borderWidth: 2,
            tension: 0.3,
            fill: true,
            yAxisID: 'y1'
          },
          {
            label: 'Cost',
            data: [],
            borderColor: '#10B981',
            backgroundColor: 'rgba(16, 185, 129, 0.1)',
            borderWidth: 2,
            tension: 0.3,
            fill: true,
            yAxisID: 'y2'
          }
        ]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: {
          mode: 'index',
          intersect: false
        },
        plugins: {
          legend: {
            display: true,
            position: 'top',
            labels: {
              color: '#e5e7eb',
              font: { size: 11 }
            }
          },
          tooltip: {
            backgroundColor: 'rgba(15, 23, 42, 0.95)',
            titleColor: '#F96831',
            bodyColor: '#e5e7eb',
            borderColor: 'rgba(249, 104, 49, 0.5)',
            borderWidth: 1
          }
        },
        scales: {
          x: {
            type: 'linear',
            title: {
              display: true,
              text: 'Time (minutes)',
              color: '#94a3b8'
            },
            ticks: {
              color: '#94a3b8',
              callback: value => Math.floor(value / 60000)
            },
            grid: {
              color: 'rgba(148, 163, 184, 0.1)'
            }
          },
          y: {
            type: 'linear',
            display: true,
            position: 'left',
            title: {
              display: true,
              text: 'Power (W)',
              color: '#F96831'
            },
            ticks: {
              color: '#F96831'
            },
            grid: {
              color: 'rgba(249, 104, 49, 0.1)'
            }
          },
          y1: {
            type: 'linear',
            display: true,
            position: 'right',
            title: {
              display: true,
              text: 'Energy (Wh)',
              color: '#3B82F6'
            },
            ticks: {
              color: '#3B82F6'
            },
            grid: {
              drawOnChartArea: false
            }
          },
          y2: {
            type: 'linear',
            display: true,
            position: 'right',
            title: {
              display: true,
              text: 'Cost',
              color: '#10B981'
            },
            ticks: {
              color: '#10B981',
              callback: function(value) {
                return value.toFixed(4);
              }
            },
            grid: {
              drawOnChartArea: false
            }
          }
        }
      }
    });

    console.log('[Graph] Chart initialized');
    this.startAutoUpdate();
    this.updateCurrencyLabel();
  }

  async update() {
    if (!this.chart) return;

    try {
      const data = await this.api.getJson('/api/log_data');
      
      this.chart.data.labels = data.timestamps;
      this.chart.data.datasets[0].data = data.power;
      this.chart.data.datasets[1].data = data.energy;
      this.chart.data.datasets[2].data = data.cost;
      this.chart.update('none');
    } catch (error) {
      console.error('[Graph] Update error:', error);
    }
  }

  clear() {
    if (!this.chart) return;
    
    this.chart.data.labels = [];
    this.chart.data.datasets.forEach(dataset => {
      dataset.data = [];
    });
    this.chart.update();
  }

  startAutoUpdate() {
    if (this.timerId) return;
    
    this.timerId = setInterval(async () => {
      const state = this.state.getState();
      if (state.loggingEnabled) {
        await this.update();
        await this.updateTotalEnergy();
      }
    }, this.updateInterval);
  }

  stopAutoUpdate() {
    if (this.timerId) {
      clearInterval(this.timerId);
      this.timerId = null;
    }
  }

  async updateCurrencyLabel() {
    if (!this.chart) return;
    
    try {
      const tariff = await this.api.getJson('/api/tariff_get');
      if (this.chart.options.scales.y2) {
        this.chart.options.scales.y2.title.text = `Cost (${tariff.currency})`;
        this.chart.update('none');
      }
    } catch (error) {
      console.error('[Graph] Failed to update currency label:', error);
    }
  }

  async updateTotalEnergy() {
    const energyEl = document.getElementById('logTotalEnergy');
    if (!energyEl || !this.chart) return;
    
    const energyData = this.chart.data.datasets[1].data;
    const costData = this.chart.data.datasets[2].data;
    
    if (energyData.length === 0) {
      energyEl.textContent = '0 Wh';
      return;
    }
    
    const totalEnergy = energyData[energyData.length - 1] || 0;
    const totalCost = costData[costData.length - 1] || 0;
    
    try {
      const tariff = await this.api.getJson('/api/tariff_get');
      energyEl.textContent = `${totalEnergy.toFixed(2)} Wh (${totalCost.toFixed(3)} ${tariff.currency})`;
    } catch {
      energyEl.textContent = `${totalEnergy.toFixed(2)} Wh`;
    }
  }
}

// ============================================================================
// Log Status Manager
// ============================================================================

class LogStatusManager {
  constructor(api, state) {
    this.api = api;
    this.state = state;
    this.updateInterval = 2000;
    this.timerId = null;
  }

  start() {
    if (this.timerId) return;
    
    this.update();
    this.timerId = setInterval(() => this.update(), this.updateInterval);
  }

  stop() {
    if (this.timerId) {
      clearInterval(this.timerId);
      this.timerId = null;
    }
  }

  async update() {
    try {
      const status = await this.api.getJson('/api/log_status');
      
      this.state.update({
        loggingEnabled: status.enabled,
        logCount: status.count,
        logMaxCount: status.max,
        logDuration: status.duration_ms
      });
    } catch (error) {
      console.error('[LogStatus] Update error:', error);
    }
  }
}

// ============================================================================
// Tariff Manager
// ============================================================================

class TariffManager {
  constructor(api) {
    this.api = api;
  }

  async load() {
    try {
      const data = await this.api.getJson('/api/tariff_get');
      
      const elements = {
        tariffHigh: document.getElementById('tariffHigh'),
        tariffLow: document.getElementById('tariffLow'),
        tariffCurrency: document.getElementById('tariffCurrency'),
        tariffStartHour: document.getElementById('tariffStartHour'),
        tariffEndHour: document.getElementById('tariffEndHour')
      };
      
      if (elements.tariffHigh) elements.tariffHigh.value = data.high;
      if (elements.tariffLow) elements.tariffLow.value = data.low;
      if (elements.tariffCurrency) elements.tariffCurrency.value = data.currency;
      if (elements.tariffStartHour) elements.tariffStartHour.value = data.start_hour;
      if (elements.tariffEndHour) elements.tariffEndHour.value = data.end_hour;
      
      console.log('[Tariff] Settings loaded');
      
      // Update graph currency label
      if (window.app && window.app.powerGraph) {
        window.app.powerGraph.updateCurrencyLabel();
      }
    } catch (error) {
      console.error('[Tariff] Failed to load settings:', error);
    }
  }
}

// ============================================================================
// Application
// ============================================================================

class Application {
  constructor() {
    this.api = new ApiService();
    this.state = new AppState();
    this.ui = new UIController(this.api, this.state);
    this.statusPoller = new StatusPoller(this.api, this.state);
    this.powerGraph = new PowerGraph(this.api, this.state);
    this.logStatus = new LogStatusManager(this.api, this.state);
    this.tariffManager = new TariffManager(this.api);
  }

  async init() {
    console.log('[App] Initializing...');
    
    try {
      this.statusPoller.start();
      this.powerGraph.init();
      this.logStatus.start();
      await this.tariffManager.load();
      await this.ui.loadAutoLog();
      
      console.log('[App] ✓ Application ready');
    } catch (error) {
      console.error('[App] ✗ Initialization failed:', error);
    }
  }

  destroy() {
    this.statusPoller.stop();
    this.powerGraph.stopAutoUpdate();
    this.logStatus.stop();
  }
}

// ============================================================================
// Bootstrap
// ============================================================================

let app;

document.addEventListener('DOMContentLoaded', () => {
  try {
    app = new Application();
    window.app = app;
    app.init();
  } catch (error) {
    console.error('[Bootstrap] Failed:', error);
  }
});

document.addEventListener('visibilitychange', () => {
  // Polling continues in background
});

window.addEventListener('error', (event) => {
  console.error('[Global Error]', event.error);
  console.error('[Global Error] Message:', event.message);
  console.error('[Global Error] Stack:', event.error?.stack);
});

window.addEventListener('unhandledrejection', (event) => {
  console.error('[Unhandled Rejection]', event.reason);
  console.error('[Unhandled Rejection] Promise:', event.promise);
});
