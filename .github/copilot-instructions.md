# PrusaWifiOff - AI Agent Instructions

## Project Overview
This is a **PlatformIO ESP32 project** for the M5Stack Atom controlling a 3D printer power relay with auto-off functionality. The device monitors an external input signal (printer status) and automatically powers off the relay after a configurable delay when the signal goes low.

## Hardware Platform
- **Board**: M5Stack Atom (`m5stack-atom`)
- **Framework**: Arduino (ESP32)
- **Key Hardware**:
  - GPIO 23: External input signal (printer status monitoring)
  - GPIO 39: Mode button (`INPUT_PIN_MODE`)
  - 5×5 LED matrix: Visual status feedback

## Architecture & Data Flow

### Component Structure
1. **main.cpp**: Core loop, WiFi management, HTTP polling, state machine
2. **ButtonMode.cpp**: Debounced button input with single/double-click detection
3. **LedDisplay.cpp**: 5×5 LED matrix patterns (visual feedback)
4. **WebUi.cpp**: Embedded web interface with live AJAX status updates

### State Flow
```
External Signal (GPIO 23) → Debounce → Timer Start → Countdown Display → Auto Power-Off
                                ↓
                          Mode Button (GPIO 39) → Toggle auto-mode / Manual relay toggle
                                ↓
                          Web UI → Control & monitor from browser
```

### External Integration
- **Target Device**: HTTP relay (configurable IP, default: `192.168.188.44`)
  - `/toggle` - Toggle relay state
  - `/relay?state=0|1` - Set relay explicitly
  - `/report` - JSON status (power, temperature, relay state, boot info)
- Polls every 5 seconds (`REPORT_POLL_INTERVAL_MS`)
- IP address is configurable via web UI and stored in Preferences

## Critical Developer Workflows

### Build & Upload
```bash
# PlatformIO commands (use these, not Arduino IDE)
pio run                    # Build only
pio run --target upload    # Build and upload to device
pio device monitor         # Serial monitor (115200 baud)
```

**VS Code**: Use PlatformIO sidebar tasks or `Ctrl+Alt+U` for upload.

### WiFi Configuration
- **Local file**: `src/wifi_cred.h` (gitignored)
- Define `WIFI_SSID_CONFIG` and `WIFI_PASS_CONFIG` macros
- Template structure:
  ```cpp
  #pragma once
  #define WIFI_SSID_CONFIG "YourSSID"
  #define WIFI_PASS_CONFIG "YourPassword"
  ```

### Persistent Settings
- Uses ESP32 **Preferences** library (NVS storage)
- Namespace: `"coreone"`
- Key: `"off_delay_ms"` - Timer duration in milliseconds
- Modified via web UI slider (1-240 minutes)

## Project-Specific Conventions

### LED Visual Language
- **Green "I"** (vertical line): Auto-off DISABLED
- **Blue "X"** (diagonal cross): Auto-off ENABLED
- **Orange progress bar** (bottom-up): Timer countdown active
- **Red "I"**: Power-off command sent

### Button Behavior
- **Single-click** (GPIO 39): Toggle auto-off mode
- **Double-click** (GPIO 39): Manual relay toggle
- Debounce: 60ms, double-click window: 250ms

### State Management Pattern
All key state variables are **global in main.cpp** and accessed via `extern` declarations in headers:
- `autoPowerOffEnabled`, `offTimerRunning`, `offDelayMs`
- `reportValid`, `reportRelay`, `reportPower`, etc.
- This pattern keeps state centralized for embedded simplicity

### Web UI Style
- Custom "glass morphism" dark theme with Prusa orange accent (`#F96831`)
- Bootstrap 5.3.3 + Bootstrap Icons
- Live updates every 500ms via `/api/status` JSON endpoint
- No page reloads - pure AJAX interaction

## Dependencies & Libraries
```ini
m5stack/M5Atom@^0.1.3       # Hardware abstraction
fastled/FastLED@^3.10.3     # LED matrix control
bblanchon/ArduinoJson@^7.4.2 # JSON parsing for relay reports
```

## Common Pitfalls
1. **Missing wifi_cred.h**: Create locally before building (not in repo)
2. **Hardcoded IP**: Target relay IP `192.168.188.44` is project-specific
3. **NVS namespace**: Always use `"coreone"` for Preferences
4. **Global state**: Don't create local copies - modify globals directly

## Testing & Debugging
- Monitor serial output at 115200 baud for:
  - WiFi connection status
  - HTTP response codes
  - Report parsing errors
  - Timer state transitions
- Web UI at `http://<device-ip>/` for live status
- LED matrix provides instant visual feedback
