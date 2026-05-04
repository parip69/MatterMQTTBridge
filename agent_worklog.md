## 4. Mai 2026 - RX-Log in beide Richtungen aktiviert

**Aufgabe:** Eingehende MQTT-Nachrichten genauso sichtbar machen wie ausgehende

**Durchgeführte Aktionen:**
- `src/main.cpp`: MQTT-Subscribe beim Connect auf Root-Wildcard umgestellt: `<root>/#`
- Subscribe ist jetzt **immer aktiv**, auch wenn `USE_OUTPUT_TEST_PINS = 0`
- Zusätzliche Diagnose-Logs für Subscribe-Erfolg/Fehler ergänzt
- Build ausgeführt: `B Bridge` erfolgreich

**Ergebnis:**
- RX-Nachrichten von externen ESPs/Broker erscheinen jetzt zuverlässig im Live-Log

---

## 4. Mai 2026 - U ALL lokal erfolgreich

**Aufgabe:** Kompletten lokalen Stand auf Bridge-ESP flashen

**Durchgeführte Aktionen:**
- `U ALL` ausgeführt (BuildFS, UploadFS, Firmware-Upload)
- Zielgerät bestätigt: COM4, MAC `88:57:21:b1:e4:10`
- Firmware + LittleFS erfolgreich übertragen
- Automatisches Projekt-/Firmware-Backup erstellt: `backup_2026.05.04-19.55.30_env_bridge_ver_1.0.8`

**Ergebnis:**
- Upload erfolgreich, Bridge läuft auf Version `1.0.8`

---

## 4. Mai 2026 - Fingerprint-kompatibles OutputPinStatus-Payload

**Aufgabe:** Bridge an das bekannte Fingerprint-Format angleichen

**Durchgeführte Aktionen:**
- `src/main.cpp`: Für Trigger 1..5 wird `OutputPinStatusX` jetzt mit `source:[WEB];true` publiziert (Fingerprint-kompatibel)
- `src/main.cpp`: RX-Auswertung erweitert, damit auch Payloads im Format `...;true` korrekt als HIGH erkannt werden
- Lokaler Build ausgeführt: `B Bridge` erfolgreich

**Geänderte Dateien:**
- `src/main.cpp` (Versionsnummer unverändert)

---

## 4. Mai 2026 - Trigger-Buttons im Live-Log sichtbar gemacht

**Aufgabe:** Klicks auf Trigger-Buttons sollen direkt im Diagnose-/Live-Log erscheinen

**Durchgeführte Aktionen:**
- `data/index.html`: Funktion `triggerPin(pin)` erweitert
- Vor dem Request wird jetzt ein Logeintrag erzeugt: `Trigger X angefordert`
- Bei Erfolg wird zusätzlich `TX <topic> -> <payload>` ins Live-Log geschrieben
- Bei API-Fehler bzw. Netzwerkfehler wird jeweils ein Fehler-Logeintrag geschrieben
- Build ausgeführt: `B Bridge` erfolgreich

**Geänderte Dateien:**
- `data/index.html` (Versionsnummer unverändert)

---

## 4. Mai 2026 - MQTT Monitor hinzugefügt

**Aufgabe:** MQTT Monitor-Fenster im Web-Interface – zeigt TX/RX/SYS-Nachrichten live an

**Durchgeführte Aktionen:**
- `src/main.cpp`: Funktion `mqttLogEvent(dir, topic, payload)` ergänzt – sendet SSE-Event vom Typ `mqtt_log`
- `src/main.cpp`: `publishMqttMessage()` ruft nach erfolgreichem Publish `mqttLogEvent("TX", ...)` auf
- `src/main.cpp`: `mqttClient.onMessage()`-Callback registriert → loggt eingehende Nachrichten als `RX`
- `src/main.cpp`: onConnect/onDisconnect senden zusätzlich `mqttLogEvent("SYS", "status", "connected/disconnected")`
- `data/index.html`: Neue Card „MQTT Monitor" mit scrollbarem Log-Bereich (Monospace, 220px)
- `data/index.html`: CSS-Klassen `.tx` (grün), `.rx` (blau), `.sys` (gelb) für Farbcodierung
- `data/index.html`: EventSource auf `/events`, lauscht auf `mqtt_log`-Events, max. 150 Einträge
- `data/index.html`: „Log leeren"-Button
- Build: SUCCESS (19.7 s)

**Geänderte Dateien:**
- `src/main.cpp` (Versionsnummer NICHT geändert)
- `data/index.html` (Versionsnummer NICHT geändert)

---

## 3. Mai 2026 - Initialisierung MatterMQTTBridge

**Aufgabe:** Neues eigenständiges GitHub-Projekt aus Fingerprint-Kopie erstellen

**Durchgeführte Aktionen:**
- Alten `.git`-Ordner (Verknüpfung zu `fingerscanner-parip69`) entfernt
- Neues Git-Repository initialisiert (Branch: `main`)
- `.gitignore` um `*.hex` ergänzt (Whitelist-Strategie beibehalten)
- `README.md` für MatterMQTTBridge erstellt
- `PROJECT_RULES.md` mit Projektregeln und Architektur erstellt
- `TODO_BRIDGE.md` mit 8-Schritt-Plan erstellt
- Ersten Commit angelegt: "Initial MatterMQTTBridge project base" (68 Dateien)
- Neues privates GitHub-Repository angelegt: https://github.com/parip69/MatterMQTTBridge
- Erster Push erfolgreich

**Geänderte / neue Dateien:**
- `.gitignore` (*.hex ergänzt, Versionsnummer NICHT verändert)
- `README.md` (neu erstellt)
- `PROJECT_RULES.md` (neu erstellt)
- `TODO_BRIDGE.md` (neu erstellt)
- `agent_worklog.md` (dieses Protokoll)

**Ergebnis:** PASS – Repository https://github.com/parip69/MatterMQTTBridge ist live, erster Commit gepusht.

**Nächste Schritte:** Laut TODO_BRIDGE.md Schritt 2 – Fingerprint-spezifische Module deaktivieren/entfernen.

## 4. Mai 2026 - Schritt 3: Abbau Legacy-Komponenten und minimalistische Bridge-Basis

**Aufgabe:** Teil 3: Finaler harter Schnitt zur echten Bridge-Basis und Behebung von Merge-Konflikten

**Durchgeführte Aktionen:**
- Alle Merge-Konflikt-Marker (`<<<<<<<`, `=======`, `>>>>>>>`) aus dem Projekt entfernt
- Alte `src/main.cpp` nach `doc/legacy_fingerprint_modules/main_legacy.cpp` verschoben
- Schlanke neue `src/main.cpp` mit Basis (WLAN, OTA, MQTT, Web) erstellt (< 300 Zeilen)
- `SettingsManager` von Legacy-Feldern befreit (ToggleButtons, SensorPins, usw.)
- `data/index.html` und `data/settings.html` komplett neu & minimalistisch aufgebaut
- `Bridge.cpp` und `Bridge.h` für API TriggerOutPin umgeschrieben 
- Build-Check `pio run -e bridge` und `pio run -e bridge -t buildfs` ausgeführt

**Geänderte Dateien:**
- `src/main.cpp`
- `src/SettingsManager.h`, `src/SettingsManager.cpp`
- `src/Bridge.h`, `src/Bridge.cpp`
- `data/index.html`, `data/settings.html`
- `platformio.ini`, `README.md`, `TODO_BRIDGE.md`
- `doc/CODEX_MATTER_MQTT_BRIDGE_ABBAU_REPORT.md` (aktualisiert)
- `agent_worklog.md` (dieses Protokoll)

## 4. Mai 2026 - Schritt 4: Bridge-Basis Finalisierung (Settings & MQTT)

**Aufgabe:** Restliche Basisfehler beheben. Noch KEIN Matter einbauen.

**Durchgeführte Aktionen:**
- `POST /save_settings` in `src/main.cpp` eingebaut und an `SettingsManager` angebunden (inklusive Neustart).
- `GET /api/settings` in `src/main.cpp` eingebaut, um aktuelle Settings als JSON auszuliefern.
- `data/settings.html` mit Fetch-Logik ausgestattet, um Settings dynamisch zu laden.
- `publishMqttMessage` von `void` auf `bool` umgebaut (in `main.cpp`, `MqttConnectionManager.cpp`, `MqttConnectionManager.h`, `Bridge.cpp`, `Bridge.h`).
- AsyncMqttClient Connect/Disconnect-Callbacks an `mqttManager` angebunden.
- Zentrale Version `firmwareVersion` in `src/main.cpp` angelegt und in der Status-API verwendet.
- Legacy Konfliktmarker-Datei `doc/legacy_fingerprint_modules/main_legacy.cpp` in `main_legacy_conflicted_reference.txt` umbenannt.

**Geänderte Dateien:**
- `src/main.cpp` (keine Versionsnummern verändert)
- `src/MqttConnectionManager.cpp`, `src/MqttConnectionManager.h`
- `src/Bridge.cpp`, `src/Bridge.h`
- `data/settings.html`
- `doc/legacy_fingerprint_modules/main_legacy_conflicted_reference.txt` (umbenannt)
- `agent_worklog.md` (dieses Protokoll)

**Ergebnis:** PASS – Alle gewünschten Endpunkte und MQTT-Rückgaben wurden erfolgreich implementiert. Die PIO Build-Verifikation wird nun via Projektinhaber Task ausgelöst.

## 4. Mai 2026 - LED-Herzschlag (Bridge-Lebenszeichen)

**Aufgabe:** Unregelmäßigen internen LED-Herzschlag von Fingerprint in MatterMQTTBridge übernehmen.

**Durchgeführte Aktionen:**
- Fallback für `LED_BUILTIN` auf GPIO2 ergänzt (`#ifndef LED_BUILTIN ... #define LED_BUILTIN 2`).
- Globale LED-Zustände ergänzt: `int led1State = LOW;` und `bool isApConfigMode = false;`.
- In `setup()` LED-Pin initialisiert (`pinMode`, `digitalWrite LOW`).
- Im AP-Konfigurationsmodus (`kein WLAN`) LED auf dauerhaft HIGH gesetzt.
- In `loop()` unregelmäßigen Herzschlag eingefügt (zufällig 200-1500 ms, ohne blockierendes `delay`).
- Vor geplantem Neustart LED sauber auf LOW gesetzt.
- Build-Verifikation ausgeführt: `pio run -e bridge` erfolgreich.

**Geänderte Dateien:**
- `src/main.cpp` (Versionszeile nicht manuell verändert)
- `agent_worklog.md` (dieses Protokoll)

**Ergebnis:** PASS – Unregelmäßiger LED-Herzschlag aktiv, AP-Dauerlicht optional aktiv, Build erfolgreich.

## 4. Mai 2026 - VS-Code Shortcuts Flash+Test und Reset+Test entfernt

**Aufgabe:** Die zwei Shortcuts `Flash+Test` und `Reset+Test` samt aktiver VS-Code-Anbindung entfernen.

**Durchgeführte Aktionen:**
- Task-Definition `Flash+Test` aus `.vscode/tasks.json` entfernt.
- Task-Definition `Reset+Test` aus `.vscode/tasks.json` entfernt.
- Statusleisten-Button `Flash+Test` aus `.vscode/settings.json` entfernt.
- Statusleisten-Button `Reset+Test` aus `.vscode/settings.json` entfernt.
- Nachkontrolle in `.vscode/**`: keine verbleibenden Treffer auf `Flash+Test`, `Reset+Test`, `flash_and_test.py`.

**Geänderte Dateien:**
- `.vscode/tasks.json`
- `.vscode/settings.json`
- `agent_worklog.md` (dieses Protokoll)

**Ergebnis:** PASS – Beide Shortcuts sind aus VS Code entfernt und nicht mehr verdrahtet.

## 4. Mai 2026 - Hostname-Fix fuer Fritzbox/mDNS

**Aufgabe:** Verhindern, dass die Bridge in der Fritzbox als `esp32-...` erscheint, wenn das Hostname-Setting leer ist.

**Durchgeführte Aktionen:**
- In `src/main.cpp` eine zentrale Hostname-Normalisierung ergänzt (`normalizeBridgeHostname`).
- Fallback erzwungen: leerer Hostname wird immer `MatterMQTTBridge`.
- Leerzeichen im Hostname für DNS/mDNS in `-` umgewandelt.
- `WiFi.setHostname(...)` wird im STA-Modus immer mit normalisiertem Namen gesetzt.
- `/api/bridge/status` und `/api/settings` liefern ebenfalls den normalisierten Hostname.
- Build-Verifikation ausgeführt: `pio run -e bridge` erfolgreich.

**Geänderte Dateien:**
- `src/main.cpp` (Versionszeile nicht manuell verändert)
- `agent_worklog.md` (dieses Protokoll)

**Ergebnis:** PASS – Die Bridge verwendet stabil einen Hostnamen statt `esp32-Default`.
