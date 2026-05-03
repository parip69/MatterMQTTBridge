# GitHub Copilot Anweisungen für Fingerscanner

**⚠️ ACHTUNG FÜR JULES / GITHUB COPILOT: Diese Datei ist für Sie geschrieben!**

---

## 🚨 REGEL #1: VERSIONSNUMMERN NIEMALS ÄNDERN

**KRITISCH!** Jede Datei in diesem Projekt startet mit einer Versionsnummer:
```
// @version: 1.6.243 Builddatum 01:05:37 27-07.2025
```

Diese Zeile darf **NIEMALS** verändert, erhöht oder angepasst werden - auch nicht um "0.0.1"!

**Warum?** Der Projektinhaber verwaltet Versionen mit `update_version_in_code.py`. Wenn Jules die Versionsnummer ändert, entstehen **Merge-Konflikte**, die das Projekt blockieren.

**Agenten-Regel:**
- ✅ Ignorieren Sie diese Zeile komplett
- ✅ Bei `replace_string_in_file`: Kontext NACH der Versionsnummer einbeziehen
- ❌ NIEMALS die Versionsnummer verändern
- ❌ NIEMALS das Builddatum ändern

**Beispiel FALSCH:**
```cpp
// @version: 1.6.244 Builddatum 08:15:22 12-11.2025  ← FALSCH! Erhöht!
#include <Arduino.h>
```

**Beispiel RICHTIG:**
```cpp
// @version: 1.6.243 Builddatum 01:05:37 27-07.2025  ← UNVERÄNDERT!
#include <Arduino.h>
// Agent hat hier Code eingefügt
```

---

## 🔨 REGEL #2: BUILD & TEST PROZESS - IMMER VALIDIEREN

Jules muss nach jeder Code-Änderung **IMMER** validieren:

### A) Syntax-Validierung (lokaler Check)
- Keine `#include` Fehler
- Keine unvollständigen Klammer-Strukturen
- Keine offensichtlichen Compiler-Fehler

### B) PlatformIO Build-Prozess

**Bitte IMMER in dieser Reihenfolge ausführen:**

```powershell
# 1. Firmware kompilieren
pio run

# 2. SPIFFS Filesystem bauen (Web-Interface!)
pio run --target buildfs

# 3. Bei Erfolg: Monitor starten zum Testen
pio device monitor
```

**WICHTIG:**
- ✅ `pio run` muss **OHNE FEHLER** abgeschlossen sein
- ✅ `pio run --target buildfs` muss **OHNE FEHLER** abgeschlossen sein
- ❌ Falls `pio run` fehlschlägt: **NICHT weitermachen!**
- ❌ Falls Compiler-Fehler auftauchen: **Sofort beheben!**

### C) Kritische Bereiche für Fehler

| Bereich | Problem | Lösung |
|---------|---------|--------|
| **main.cpp** | Include-Fehler, fehlende Libs | Überprüfen Sie Abhängigkeiten in `platformio.ini` |
| **FingerprintManager.cpp** | Serial2-Initialisierung | GPIO16/17 richtig konfiguriert? |
| **SettingsManager.cpp** | MQTT Connection Fehler | Port 1883 offen? Broker erreichbar? |
| **Web-Interface** (data/index.html) | EventSource-Fehler | Nach Änderung **IMMER** `pio run --target buildfs` |

---

## 🎯 REGEL #3: WEB-INTERFACE ÄNDERUNGEN

**Wichtig:** Wenn Sie `data/index.html` oder andere Web-Dateien ändern:

```powershell
# 1. Web-Datei ändern
# 2. SPIFFS neu bauen
pio run --target buildfs

# 3. Erst dann uploaden
pio run --target uploadfs
```

**Fehler vermeiden:**
- ❌ HTML ändern und SPIFFS **nicht** neu bauen = alte Version bleibt
- ❌ EventSource-Syntax falsch = Live-Updates funktionieren nicht
- ✅ Nach jeder HTML-Änderung: `buildfs`

---

## 🔌 REGEL #4: HARDWARE PIN MAPPING - NICHT VERÄNDERN

Diese Pins sind **hardware-gekoppelt** und dürfen NICHT geändert werden:

```cpp
// Fingerprint sensor (Serial2)
#define FP_RX 16    // GPIO16
#define FP_TX 17    // GPIO17
#define FP_TOUCH 5  // Touch pin

// Output pins
#define DOOR_MAIN 23    // Main entrance
#define DOOR_GARAGE 19  // Garage
#define DOOR_SPARE1 18  // Spare 1
#define DOOR_SPARE2 26  // Spare 2

// Signals
#define LED_PIN 22      // LED lighting
#define BELL_PIN 21     // Doorbell output
#define DHT_PIN 25      // Temperature/Humidity
```

**Regel:** Diese Definitionen sind physikalisch mit dem ESP32-Board verdrahtet. Falsche Pins = Hardware-Fehler!

---

## 📡 REGEL #5: MQTT ÄNDERUNGEN - BEIDE MODI TESTEN

Wenn Sie MQTT-Code ändern, müssen Sie **BEIDE Modi** testen:

1. **Client-Modus** (normaler MQTT-Betrieb)
   ```
   fingerscanner/ring → "on"/"off"
   fingerscanner/matchId → Fingerprint ID
   fingerscanner/matchName → Fingerprint Name
   fingerscanner/matchConfidence → Konfidenz (1-400)
   ```

2. **Broker-Modus** (eingebauter MQTT-Broker)
   - Broker lädt `ESPAsyncMQTTBroker` Bibliothek
   - Clients verbinden sich auf Port 1883

**Test nach Änderung:**
- ✅ Nachricht auf `fingerscanner/ring` → ESP32 reagiert
- ✅ Fingerprint-Match publiziert → Topic `matchId` aktualisiert
- ✅ Keine Verbindungsabbrüche in Loop

---

## 🌍 REGEL #6: TEDEE SMART LOCK INTEGRATION

Diese Konfiguration ist **hardcodiert** für ein spezifisches Setup:

```cpp
bool istedeeBridge = true;
const char *tedeeBridgeIp = "192.168.111.225";
const char *tedeeToken = "fBmSkngfJG6b";
const int tedeeLockId = 20038;
```

**Agenten-Regel:**
- ❌ NIEMALS diese Werte ändern (außer der Projektinhaber fragt direkt danach)
- ✅ Wenn Sie HTTP-Requests zum Tedee-Bridge ändern: Vorher User fragen
- ✅ Bei Token-Änderungen: Dokumentieren im `agent_worklog.md`

---

## 🌤️ REGEL #7: SOLAR-KALKULATION & ZEITZONE

Zeitzone ist konfigurierbar:
```cpp
const char *tzInfo = "CET-1CEST,M3.5.0/2,M10.5.0/3";
```

**Agenten-Regel:**
- ✅ Sie können SolarCalc-Logik verbessern (Sunrise/Sunset-Berechnungen)
- ❌ Zeitzone NICHT ändern (nur Projektinhaber!)
- ✅ Nach Änderungen: NTP-Sync testen und Sonnenauf-/Untergangszeit validieren

---

## 📋 REGEL #8: ARBEITSPROTOKOLL FÜHREN

**Immer** nach einer Aufgabe ein Protokoll in `agent_worklog.md` erstellen:

```markdown
## 12. November 2025 - 14:30 Uhr

**Aufgabe:** Fix: Sensorkommunikation fehlerhaft

**Durchgeführte Aktionen:**
- Datei `src/FingerprintManager.cpp` analysiert
- Fehler in der Sensor-Initialisierung gefunden
- Retry-Logik implementiert
- Build erfolgreich: `pio run` ohne Fehler
- `pio run --target buildfs` erfolgreich
- Tests durchgeführt

**Geänderte Dateien:**
- `src/FingerprintManager.cpp` (3 Änderungen, Versionsnummer NICHT erhöht)
- Keine anderen Dateien verändert

**Build-Status:** ✅ Erfolgreich
```

---

## 🎯 CHECKLISTE VOR JEDER DATEIÄNDERUNG

- [ ] Ist die erste Zeile eine `@version` Zeile? → **NICHT ANFASSEN!**
- [ ] Nutze ich `replace_string_in_file`? → **Min. 3-5 Zeilen NACH der Versionsnummer als Kontext!**
- [ ] Ändere ich Web-Dateien (HTML/CSS/JS)? → **IMMER `pio run --target buildfs` danach!**
- [ ] Ändere ich MQTT-Code? → **BEIDE Modi (Client + Broker) validieren!**
- [ ] Ändere ich Hardware-Pins? → **NIEMALS - Diese sind hardware-gekoppelt!**
- [ ] Kann ich es mit `pio run` kompilieren? → **JA = Weitermachen, NEIN = Fehler beheben!**
- [ ] Habe ich es in `agent_worklog.md` dokumentiert? → **JA!**

---

## 📞 Wichtige Kontakte & Verweise

- **Versionsverwaltung:** `update_version_in_code.py` (nur Projektinhaber!)
- **Build-Konfiguration:** `platformio.ini`
- **Hardware-Layout:** `partitions.csv`
- **Web-Interface:** `data/index.html` (nach Änderung: `buildfs`!)
- **Agenten-Regeln:** `AGENTS.md` (Deutsch)

---

## 🔍 Häufige Fehler und Lösungen

### ❌ Fehler: `pio run` schlägt fehl

**Mögliche Ursachen:**
1. Include-Pfade falsch
2. Abhängigkeit in `platformio.ini` fehlt
3. Versionsnummer wurde verändert (Git-Konflikt!)

**Lösung:**
```powershell
pio run --target clean
pio run
```

### ❌ Fehler: Web-Interface wird nicht aktualisiert

**Problem:** HTML geändert, aber SPIFFS nicht neu gebaut

**Lösung:**
```powershell
pio run --target buildfs
pio run --target uploadfs
```

### ❌ Fehler: MQTT funktioniert nicht

**Mögliche Ursachen:**
1. Broker nicht erreichbar (Port 1883 offen?)
2. Topic falsch geschrieben
3. Payload-Format falsch

**Lösung:**
- Beide Modi testen (Client + Broker)
- Monitor-Output überprüfen: `pio device monitor`

### ❌ Fehler: Sensor reagiert nicht

**Mögliche Ursachen:**
1. GPIO16/17 nicht korrekt initialisiert
2. Sensor-Pairingcode gebrochen
3. Serial2-Baudrate falsch

**Lösung:**
- `FingerprintManager` Debug-Ausgaben überprüfen
- Sensor-Pairing validieren
- GPIO-Verbindungen prüfen

---

## 📝 Projekt-Struktur

```
src/
  ├── main.cpp                    (Hauptprogramm - NICHT ändern!)
  ├── FingerprintManager.h/cpp    (Sensor-Kommunikation)
  ├── SettingsManager.h/cpp       (WiFi/MQTT/Konfiguration)
  ├── SolarCalc.h/cpp            (Sonnenauf-/Untergang)
  └── ... weitere Module

data/
  ├── index.html                 (Web-Interface - nach Änderung: buildfs!)
  ├── style.css
  └── script.js

doc/
  ├── ESPAsyncMQTTBroker.cpp/h   (Eigene Bibliothek - darf verbessert werden!)
  └── ... weitere Dokumentation

platformio.ini                    (Build-Konfiguration)
partitions.csv                    (Flash-Layout)
update_version_in_code.py        (Versionsverwaltung - nur Projektinhaber!)
AGENTS.md                        (Deutsche Agenten-Regeln)
```

---

## ✅ Erfolgreiches Build-Rezept

```powershell
# 1. Code-Änderungen durchführen
# 2. Syntax prüfen (keine roten Fehler)
# 3. Firmware bauen
pio run

# 4. Wenn Web-Interface geändert wurde
pio run --target buildfs

# 5. Upload
pio run --target upload

# 6. Bei Bedarf: Monitor starten
pio device monitor

# 7. Testen & Validieren

# 8. agent_worklog.md aktualisieren
```

**Das ist Ihre Garantie für saubere Builds und keine Konflikte!** ✅
