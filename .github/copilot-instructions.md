# GitHub Copilot Instructions for MatterMQTTBridge

## Project Overview

This is an ESP32-based Matter-to-MQTT bridge (MatterMQTTBridge). It connects Matter / Google Home to an existing Fingerprint-ESP32 MQTT broker. This bridge runs only as an MQTT **client** – never as a broker.

## Architecture

```
Matter / Google Home
  → MatterMQTTBridge (this ESP32)
    → MQTT Client
      → Fingerprint-ESP32 MQTT-Broker
        → existing output / trigger / module logic
```

## Build Environment

Single environment: `[env:bridge]`

```powershell
# Build firmware
pio run -e bridge
# Build filesystem
pio run -e bridge -t buildfs
# Upload
pio run -e bridge -t upload
pio run -e bridge -t uploadfs
```

## Key Files
- `src/main.cpp` - Main application logic and web server
- `src/SettingsManager.h/cpp` - WiFi, MQTT, app config via ESP32 Preferences
- `data/index.html` - Web interface
- `platformio.ini` - Single bridge environment
- `partitions.csv` - ESP32 flash layout
- `update_version_in_code.py` - Automatic version stamping (never touch version lines!)

## Version Management

All source files start with a version line. **NEVER change this line.**
```cpp
// @version: X.X.XXX Builddatum HH:MM:SS DD-MM.YYYY
```

## Rules

- No Fingerprint sensor logic
- No Tedee logic
- No Nuki logic
- No Telegram logic
- No MQTT broker on this device
- MQTT client only
- ElegantOTA under /update
- Matter pairing code + QR in web UI and Serial Monitor

## Dependencies
- **ESPAsyncWebServer** - Web interface with SSE
- **AsyncMqttClient** - Non-blocking MQTT client
- **ElegantOTA** - OTA firmware updates
- **DHTesp** - Temperature/humidity sensor
- **ArduinoJson** - Config and API data

