# GitHub Copilot Instructions for Fingerscanner

## Project Architecture

This is an ESP32-based fingerprint access/control system using PlatformIO with Arduino framework. The core architecture consists of:

- **FingerprintManager** (`src/FingerprintManager.h/cpp`) - Manages Grow R503 fingerprint sensor communication
- **SettingsManager** (`src/SettingsManager.h/cpp`) - Handles WiFi, MQTT, and app configuration via ESP32 Preferences
- **SolarCalc** (`src/SolarCalc.h/cpp`) - Calculates sunrise/sunset times for automated lighting control
- **AsyncWebServer** - Serves web interface from SPIFFS filesystem (`data/` directory)
- **MQTT Integration** - Both client and broker modes supported via AsyncMqttClient and ESPAsyncMQTTBroker

## Key Patterns & Conventions

### Hardware Pin Mapping
```cpp
// Fingerprint sensor (Serial2): RX2=GPIO16 (green), TX2=GPIO17 (yellow), touch=GPIO5
// Output pins: 23 (main entrance), 19 (garage), 18 (spare), 26 (spare)
// LED/Bell: 22 (LED lighting), 21 (ring/bell output)
// DHT22 sensor: GPIO25
```

### Version Management
All source files use automatic version updating via `update_version_in_code.py` pre-build script. Version format: `@version: 1.6.243 Builddatum 01:05:37 27-07.2025` in file headers.

### Operating Modes
```cpp
enum class Mode { scan, enroll, wificonfig, maintenance };
```
- **scan** - Normal fingerprint detection mode
- **enroll** - Adding new fingerprints (5-pass enrollment process)
- **wificonfig** - AP mode for initial WiFi setup (triggered by 10s touch during boot)
- **maintenance** - System maintenance operations

### MQTT Topics Pattern
```
fingerscanner/ring - "on"/"off" for ring/bell events
fingerscanner/matchId - matched fingerprint ID (-1 = no match)
fingerscanner/matchName - matched fingerprint name
fingerscanner/matchConfidence - match confidence (1-400)
fingerscanner/ignoreTouchRing - subscribe to disable touch ring in rain
```

## Development Workflow

### Building & Flashing
```powershell
# Build firmware
pio run
# Build SPIFFS filesystem (contains web interface)
pio run --target buildfs
# Upload both
pio run --target upload
pio run --target uploadfs
```

### Key Dependencies
- **ESPAsyncWebServer** - Web interface with Server-Sent Events for real-time updates  
- **ElegantOTA** - Over-the-air firmware updates via web interface
- **AsyncMqttClient** - Non-blocking MQTT client
- **Adafruit-Fingerprint-Sensor-Library** - Grow R503 sensor communication
- **DHTesp** - Temperature/humidity sensor support
- **ArduinoJson** - Configuration and API data handling

### Sensor Security
The system implements sensor pairing (`sensorPairingCode` in SettingsManager) to prevent sensor substitution attacks. Breaking pairing disables fingerprint matching but preserves ring/bell functionality.

## Integration Points

### Tedee Smart Lock Integration
Direct HTTP API calls to Tedee bridge for door control:
```cpp
bool istedeeBridge = true;
const char *tedeeBridgeIp = "192.168.111.225";
const char *tedeeToken = "fBmSkngfJG6b";
const int tedeeLockId = 20038;
```

### Weather-Aware Touch Ring
Touch ring sensitivity can be disabled via MQTT (`ignoreTouchRing`) to prevent false triggers during rain storms.

### Time & Solar Integration
NTP synchronization with configurable timezone (`tzInfo = "CET-1CEST,M3.5.0/2,M10.5.0/3"`). SolarCalc provides sunrise/sunset for automated lighting control.

## Critical Files
- `src/main.cpp` - Main application logic and web server setup
- `data/index.html` - Primary web interface with real-time updates via EventSource
- `platformio.ini` - Build configuration with custom pre/post scripts
- `partitions.csv` - ESP32 flash memory layout for firmware and SPIFFS
- `update_version_in_code.py` - Automatic version stamping across all files

When modifying sensor communication, always test pairing validation. When changing web interface, rebuild SPIFFS filesystem. For MQTT changes, test both client and broker modes.
