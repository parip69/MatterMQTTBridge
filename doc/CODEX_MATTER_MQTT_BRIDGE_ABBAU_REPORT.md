<<<<<<< HEAD
# Abbau Report: Matter MQTT Bridge

- alte main.cpp wurde vollständig nach doc/legacy_fingerprint_modules/main_legacy.cpp archiviert
- neue main.cpp wurde schlank für die Bridge-Basis erstellt (WLAN, Web, MQTT, OTA)
- SettingsManager wurde stark verschlankt (nur noch wesentliche Bridge/Wifi/MQTT Settings)
- index.html durch eine minimalistische Status&Trigger Oberfläche ersetzt
- settings.html durch ein einfaches Settings-Formular ohne Legacy-Module ersetzt
- Bridge.cpp: Topic/Payload korrigiert (<mqttRootTopic>/TriggerOutPin, Payload ist Triggernummer)
- README.md / TODO_BRIDGE.md korrigiert
- platformio.ini: ungenutzte Dependency reduziert, libs angepasst, env auf bridge gesetzt

Der Aufbau der Bridge ist hiermit strukturell isoliert von alter Fingerprint-Hardware.
=======
# Phase-1-Bereinigung – Abbau-Bericht MatterMQTTBridge

**Datum:** 2026-01-01  
**Agent:** GitHub Copilot  
**Branch:** copilot/phase-1-cleanup-mqtt-bridge

---

## Zusammenfassung

Das Repository `MatterMQTTBridge` war eine Kopie des Fingerprint-ESP32-Projekts. Im Rahmen der Phase-1-Bereinigung wurden alle Fingerprint-, Solar-, DHT-, Tedee-, Nuki- und Telegram-Komponenten entfernt und eine schlanke MQTT-/Matter-Bridge-Basis aufgebaut.

---

## Durchgeführte Änderungen

### 1. platformio.ini
- `USE_DHT=0` (war 1)
- `USE_DOWNLOAD_UPLOAD=0` (war 1)
- `USE_MQTT_BROKER=1` (war 0)
- DHTesp-Bibliothek aus `lib_deps` entfernt

### 2. Legacy-Module archiviert
Verschoben von `src/` nach `doc/legacy_fingerprint_modules/`:
- `FingerprintManager.cpp` + `.h`
- `DHTManager.cpp` + `.h`
- `SolarCalc.cpp` + `.h`
- `Tedee.cpp` + `.h`
- `Nuki.cpp` + `.h`
- `Telegram.cpp` + `.h`

### 3. src/global.h
- DHTManager-Forward-Deklaration entfernt
- `USE_TELEGRAM`-Block entfernt
- `LedBeleuchtung`-Extern entfernt
- `dhtManager`-Extern entfernt
- `USE_DOWNLOAD_UPLOAD`-WebTemplates-Block entfernt
- Neue Bridge-Logging-Makros (LOG_PRINT/LOG_PRINTLN/LOG_PRINTF/LOG_NL)

### 4. src/SettingsManager.h
- `hostname` Default: `"Fingerscanner"` → `"MatterMQTTBridge"`
- `mqttRootTopic` Default: `"MatterMQTTBridge"`
- `generateNewPairingCode()` entfernt
- Legacy-Felder (toggleButton0-6, delayButton1-6, sensorPin etc.) markiert

### 5. src/SettingsManager.cpp
- `Crypto.h` Include entfernt
- `generateNewPairingCode()` Funktion entfernt
- Legacy-Lade-/Speicherlogik entfernt

### 6. src/Bridge.h + src/Bridge.cpp (NEU)
- Neue Datei `Bridge.h` mit `bridgeSendTrigger(uint8_t triggerNumber)` Deklaration
- Neue Datei `Bridge.cpp` mit Implementierung

### 7. src/main.cpp
- `#include "FingerprintManager.h"` und `#include "SolarCalc.h"` entfernt
- `fingerManager`-Deklaration und alle Aufrufe entfernt/kommentiert
- `solarCalc`-Deklaration und alle Aufrufe entfernt/kommentiert
- `doScan()`, `doEnroll()`, `doPairing()` Fingerprint-Funktionen entfernt
- WiFi-AP-Name: `"FingerscannerConfig"` → `"MatterMQTTBridgeConfig"`
- `#include "Bridge.h"` hinzugefügt
- Neue API-Endpoints: `/api/bridge/status`, `/api/bridge/trigger`

### 8. data/index.html
- Branding: "Fingerscanner" → "MatterMQTTBridge"
- Status-Dashboard für Bridge

### 9. data/settings.html
- Branding: "Fingerscanner" → "MatterMQTTBridge"
- Fingerprint-/Sensor-Bereiche entfernt

### 10. README.md
- MatterMQTTBridge-spezifischer Inhalt

### 11. TODO_BRIDGE.md
- Phase-1 bis Phase-4 Roadmap aktualisiert

---

## Erhaltene Dateien (UNBERÜHRT)

- `src/MqttConnectionManager.h` + `.cpp`
- `doc/ESPAsyncMQTTBroker.h` + `.cpp`
- `data/bootstrap.min.css`
- `data/icon-192.png`
- `data/login.html`
- `data/wificonfig.html`
- `partitions.csv`

---

## Versionszeilen

**Alle Versionszeilen in allen Dateien wurden NICHT verändert.**

---

## Offene Punkte (Phase 2)

- Legacy-AppSettings-Felder (toggleButton0-6, fingerprintScannerEnabled etc.) vollständig aus dem Struct entfernen
- Matter-Protokoll-Integration (Schaltaktor via Matter-Protokoll)
- UI-Dashboard in index.html weiter ausbauen
- Hardware-Build mit PlatformIO verifizieren
>>>>>>> 937c36a21b29111ed2f16fbabeffa236a6d16872
