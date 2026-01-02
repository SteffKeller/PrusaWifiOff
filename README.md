# PrusaWifiOff

**Automatic Power-Off Controller for 3D Printers**

M5Stack Atom-based smart relay controller that monitors your 3D printer's status and automatically powers it off after a configurable delay when printing completes.

![Platform](https://img.shields.io/badge/platform-ESP32-blue)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D)
![License](https://img.shields.io/badge/license-MIT-green)

## Features

### 🎯 Core Functionality
- **Auto Power-Off**: Automatically cuts power to relay when external signal (printer status) goes low
- **Configurable Timer**: Set delay from 1-240 minutes via web interface
- **Manual Override**: Physical button for instant control without network access
- **Web Interface**: Responsive UI with live status updates and full control

### 📊 Monitoring
- Real-time power consumption tracking
- Temperature monitoring
- Energy usage statistics
- Relay state visualization
- Boot time and uptime tracking

### 🎨 Visual Feedback (5×5 LED Matrix)
- **Green "I"** (vertical line) → Auto-off DISABLED
- **Blue "X"** (diagonal cross) → Auto-off ENABLED  
- **Orange progress bar** (bottom-up) → Timer countdown active
- **Red "I"** → Power-off command sent

### 🔘 Physical Button Controls (GPIO 39)
- **Single-click**: Toggle auto-off mode ON/OFF
- **Double-click**: Manual relay toggle (immediate control)

## Hardware Requirements

- **M5Stack Atom** (ESP32-based development board)
- **HTTP-Controlled Relay** (e.g., MyStrom Switch, Shelly Plug, Tasmota device)
- **3D Printer** with status output signal (GPIO 23)

## Quick Start

### 1. Prerequisites

Install [PlatformIO](https://platformio.org/):
```bash
# VS Code extension (recommended)
# Or via CLI
pip install platformio
```

### 2. WiFi Configuration

Create `src/wifi_cred.h` (gitignored):
```cpp
#pragma once
#define WIFI_SSID_CONFIG "YourSSID"
#define WIFI_PASS_CONFIG "YourPassword"
```

### 3. Build & Upload

```bash
# Build project
pio run

# Upload to M5Stack Atom
pio run --target upload

# Monitor serial output (115200 baud)
pio device monitor
```

**VS Code Users**: Use PlatformIO sidebar tasks or `Ctrl+Alt+U` for upload.

## Web Interface

Access the web UI at `http://<device-ip>/` to:

- ⚙️ Configure relay IP address
- ⏱️ Adjust auto-off timer (slider: 1-240 minutes)
- 🔌 Manual power control (ON/OFF/TOGGLE)
- 📈 Monitor real-time power consumption
- 🌡️ View device temperature and statistics

The interface features a custom "glass morphism" dark theme with Prusa orange accent (`#F96831`), providing a modern and responsive experience with live AJAX updates every 500ms.

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main HTML interface |
| `/api/status` | GET | JSON status (timer, relay state, power, etc.) |
| `/api/mode` | GET | Toggle auto power-off mode |
| `/api/off_now` | GET | Power off relay immediately |
| `/api/on_now` | GET | Power on relay |
| `/api/toggle` | GET | Toggle relay state |
| `/api/set_timer?minutes=N` | GET | Set timer delay (1-240 min) |
| `/api/set_relay_ip?ip=X.X.X.X` | GET | Configure relay IP address |

## Configuration Storage

Settings are persisted in ESP32 NVS (Non-Volatile Storage):

- **Namespace**: `"coreone"`
- **Keys**:
  - `off_delay_ms` - Timer duration in milliseconds
  - `relay_ip` - Target relay IP address

## Target Relay Requirements

The relay device must provide HTTP endpoints:

| Endpoint | Description |
|----------|-------------|
| `/toggle` | Toggle relay state |
| `/relay?state=0\|1` | Set relay explicitly |
| `/report` | JSON status with fields: `power`, `relay`, `temperature`, `Ws`, `boot_id`, `energy_since_boot`, `time_since_boot` |

**Default IP**: `192.168.188.44` (configurable via web UI)

**Polling**: Status polled every 5 seconds

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ External Signal (GPIO 23) → Printer Status Monitoring       │
│                                  ↓                           │
│                            Debounce Logic                    │
│                                  ↓                           │
│                         Timer Start/Stop                     │
│                                  ↓                           │
│                    LED Matrix Visual Feedback                │
│                                  ↓                           │
│               Auto Power-Off (HTTP Relay Control)            │
├─────────────────────────────────────────────────────────────┤
│ Mode Button (GPIO 39) → Single/Double Click Detection       │
│                                  ↓                           │
│                    Toggle Mode / Manual Control              │
├─────────────────────────────────────────────────────────────┤
│ Web UI (Port 80) → Control & Monitor via Browser            │
│                    ↓                                         │
│         REST API + Live AJAX Status Updates (500ms)          │
└─────────────────────────────────────────────────────────────┘
```

### Key Components

- **`main.cpp`**: Core loop, state machine, WiFi management, HTTP polling
- **`ButtonMode.cpp/h`**: Debounced button input with click detection
- **`LedDisplay.cpp/h`**: 5×5 LED matrix patterns for visual feedback
- **`WebUi.cpp/h`**: Embedded web interface with Bootstrap styling

## Dependencies

```ini
m5stack/M5Atom@^0.1.3       # M5Stack Atom hardware library
fastled/FastLED@^3.10.3     # LED matrix control
bblanchon/ArduinoJson@^7.4.2 # JSON parsing for relay reports
```

All dependencies are automatically managed by PlatformIO.

## Use Cases

- **3D Printing**: Auto power-off printer/heated bed after print completion
- **Smart Home**: Timer-based appliance control with manual override
- **Power Management**: Reduce standby consumption with automatic shutoff
- **Lab Equipment**: Timed shutdown of test equipment

## Troubleshooting

### WiFi Not Connecting
- Verify credentials in `src/wifi_cred.h`
- Check 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Monitor serial output at 115200 baud for connection status

### Relay Not Responding
- Verify relay IP address via web UI
- Check relay device is powered and on same network
- Test relay endpoints manually: `http://<relay-ip>/report`
- Ensure relay supports required HTTP endpoints

### Timer Not Working
- Verify GPIO 23 connection to printer status signal
- Check signal logic (HIGH = active, LOW = idle/complete)
- Monitor LED matrix for visual feedback
- Enable auto-off mode (single-click button or web UI)

## Development

### Generate Documentation
```bash
# Requires Doxygen installed
doxygen Doxyfile
```

All source files include Doxygen-compatible comments for API documentation.

### Debug Output
Serial monitor (115200 baud) shows:
- WiFi connection status
- HTTP request/response codes
- Report parsing results
- Timer state transitions
- Preference storage operations

## License

MIT License - see LICENSE file for details

## Contributing

Contributions welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Acknowledgments

- Built with [PlatformIO](https://platformio.org/)
- Hardware: [M5Stack Atom](https://m5stack.com/)
- Inspired by the need for safer 3D printing workflows

---

**Made with ❤️ for the 3D printing community**
