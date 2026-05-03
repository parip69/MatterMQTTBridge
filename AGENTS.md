# 🤖 Agenten-Anweisungen für das ESP32 Fingerprint Scanner Projekt

**ACHTUNG: Dieses Dokument richtet sich an KI-Agenten (Jules, GitHub Copilot, etc.)**

Dieses Dokument enthält **KRITISCHE REGELN** und Anweisungen für KI-Agenten, die an diesem Repository arbeiten. Die **strikte Einhaltung** dieser Regeln ist **obligatorisch**.

---

## 🚨 REGEL #1: VERSIONSNUMMERN - ABSOLUT NIEMALS ÄNDERN! 🚨

**⚠️ OBERSTE PRIORITÄT - KRITISCH ⚠️**

### Die Regel:
**JEDE Datei (C++, HTML, JavaScript, Python, etc.) beginnt mit einer Versionsnummer. Diese Zeile darf unter KEINEN Umständen geändert, aktualisiert, erhöht oder auch nur berührt werden!**

### Versionsnummern-Format:
```
@version: X.X.XXX Builddatum HH:MM:SS DD-MM.YYYY
```

Diese Zeile erscheint in verschiedenen Kommentar-Formaten:
- **C/C++/Arduino:** `// @version: 1.6.243 Builddatum 01:05:37 27-07.2025`
- **HTML/Markdown:** `<!-- @version: 1.99.553 <br> Builddatum 17:41:51 05-11.2025 -->`
- **Python:** `# @version: 2.1.15 Builddatum 12:30:00 03-11.2025`
- **JavaScript/TypeScript:** `// @version: 1.5.200 Builddatum 14:22:15 01-11.2025`

### Wer verwaltet die Versionsnummern?
- ✅ **NUR** der Projektinhaber (über das Skript `update_version_in_code.py`)
- ❌ **NIEMALS** ein Agent/KI-Modell

### Was Agenten NIEMALS tun dürfen:
- ❌ Die Versionsnummer lesen (=ignorieren!)
- ❌ Die Versionsnummer verändern
- ❌ Die Versionsnummer erhöhen
- ❌ Die Versionsnummer anpassen
- ❌ Das Builddatum ändern
- ❌ Die gesamte Versionszeile anfassen
- ❌ Diese Zeile in irgendeiner Form modifizieren
- ❌ Diese Zeile kommentieren oder umstrukturieren

### Was Agenten IMMER tun müssen:
- ✅ Die Versionsnummer komplett ignorieren (als wäre sie nicht existent)
- ✅ Diese Zeile bei jeder Dateibearbeitung unverändert lassen
- ✅ Die Zeile beim Replace um diese Zeile herum konstruieren (Kontext DAVOR und DANACH einschließen, aber NICHT die Versionsnummer anrühren)
- ✅ Im `oldString` bei einem Replace mindestens 3-5 Zeilen NACH der Versionsnummer einschließen, um die Versionsnummer in Ruhe zu lassen

### Praktische Beispiele:

**❌ FALSCH - Agent hat die Versionsnummer erhöht:**
```cpp
// @version: 1.6.244 Builddatum 08:15:22 12-11.2025  ← AGENT HAT DAS GEÄNDERT! KONFLIKTE!
#include <Arduino.h>
```

**✅ RICHTIG - Agent ignoriert die Versionsnummer komplett:**
```cpp
// @version: 1.6.243 Builddatum 01:05:37 27-07.2025  ← UNVERÄNDERT!
#include <Arduino.h>
// Agent hat hier Code eingefügt, aber die Versionsnummer NICHT angefasst
```

**❌ FALSCH - Agent hat die Versionsnummer kommentiert:**
```cpp
// // @version: 1.6.243 Builddatum 01:05:37 27-07.2025  ← AGENT HAT KOMMENTIERT!
#include <Arduino.h>
```

**✅ RICHTIG - oldString bei Replace enthält Kontext NACH Versionsnummer:**
```cpp
// @version: 1.6.243 Builddatum 01:05:37 27-07.2025
#include <Arduino.h>

oldString:
// @version: 1.6.243 Builddatum 01:05:37 27-07.2025
#include <Arduino.h>
void setup() {

newString:
// @version: 1.6.243 Builddatum 01:05:37 27-07.2025
#include <Arduino.h>
void setup() {
  Serial.begin(115200);  ← Agent hat nur HIER Code hinzugefügt!
```
---

## 📝 REGEL #2: Kommunikation ausschließlich auf Deutsch

**Regel:** Die gesamte Kommunikation muss auf Deutsch erfolgen:

- ✅ Chat-Antworten
- ✅ Commit-Nachrichten (Titel und Beschreibung)
- ✅ Pull-Request-Beschreibungen
- ✅ Code-Kommentare (falls vom Agent hinzugefügt)
- ✅ Alle anderen vom Agenten generierten Texte

**Beispiel (RICHTIG):**
```
Commit: "Fix: Fingerprint-Sensor-Initialisierung verbessert"
Description: "Die Sensor-Kommunikation wurde optimiert und die Fehlerbehandlung erweitert."
```

---

## 📋 REGEL #3: Arbeitsprotokoll in agent_worklog.md führen

**Regel:** Für jede Aufgabe muss ein Protokoll in der Datei `agent_worklog.md` geführt werden.

**Vorgehen:**
1. Falls `agent_worklog.md` nicht existiert → erstellen
2. Für jede neue Aufgabe einen neuen Abschnitt hinzufügen mit:
   - Datum und Uhrzeit
   - Aufgabenbeschreibung
   - Stichpunkte der durchgeführten Aktionen
3. Am Ende der Aufgabe die wichtigsten Änderungen zusammenfassen

**Beispiel:**
```markdown
## 12. November 2025 - 14:30 Uhr

**Aufgabe:** Fix: Sensorkommunikation fehlerhaft

**Durchgeführte Aktionen:**
- Datei `src/FingerprintManager.cpp` analysiert
- Fehler in der Sensor-Initialisierung gefunden
- Retry-Logik implementiert
- Build erfolgreich: `pio run` ohne Fehler
- Tests durchgeführt

**Geänderte Dateien:**
- `src/FingerprintManager.cpp` (3 Änderungen, Versionsnummer NICHT erhöht)
- Keine anderen Dateien verändert
```

---

## 🔧 REGEL #4: Umgang mit eigenen Bibliotheken (z.B. ESPAsyncMQTTBroker)

**Regel:** Eigene Bibliotheken wie `ESPAsyncMQTTBroker` befinden sich im `doc`-Ordner als Quellcode.

**Vorgehen:**
1. ✅ Verbesserungen direkt in `doc/ESPAsyncMQTTBroker.cpp` und `doc/ESPAsyncMQTTBroker.h` vornehmen
2. ✅ Nach Abschluss den Projektinhaber informieren → Bibliothek wird ins GitHub-Repository übernommen
3. ❌ Die Versionsnummern in den Kopfzeilen NIEMALS verändern (→ Regel #1)
4. ✅ Änderungen im `agent_worklog.md` dokumentieren

---

## 🎯 CHECKLISTE FÜR AGENTEN - VOR JEDER DATEIÄNDERUNG:

Bevor Sie eine Datei ändern, fragen Sie sich:

- [ ] Ist die erste Zeile eine Versionsnummer? → **IGNORIEREN und NICHT ANFASSEN!**
- [ ] Nutze ich `replace_string_in_file`? → **Kontext DAVOR und DANACH einschließen (mindestens 3-5 Zeilen), aber Versionsnummer NICHT in den oldString einbinden!**
- [ ] Antworte ich auf Deutsch? → **Ja, immer!**
- [ ] Dokumentiere ich die Änderung in `agent_worklog.md`? → **Ja, nach der Aufgabe!**
- [ ] Habe ich eine eigene Bibliothek geändert? → **Im `doc`-Ordner und im Protokoll dokumentieren!**

---

## 🔍 BESONDERE HINWEISE FÜR JULES / GITHUB COPILOT

Diese Datei ist **öffentlich für KI-Agenten gedacht**. Bitte beachten Sie:

1. **Die Versionsnummern-Regel ist NICHT verhandelbar.** Dies verhindert Merge-Konflikte.
2. **Replace-Operations:** Wenn Sie `replace_string_in_file` verwenden, konstruieren Sie den `oldString` so, dass er die Versionsnummer **NICHT einschließt**, aber genug Kontext bietet, um eindeutig zu sein.
3. **Bei Unklarheiten:** Wenn Sie nicht sicher sind, ob eine Zeile eine Versionsnummer ist, bearbeiten Sie die Datei **NICHT** und informieren Sie den Benutzer.

---

## 📞 Fragen?

Wenn Sie als Agent Fragen zu diesen Regeln haben, wenden Sie sich an den Projektinhaber. Diese Regeln sind **unveränderlich**.

---

## Projektweite Verifikation fuer Codex

Diese Zusatzregeln gelten fuer Coding-Agenten, die in diesem ESP32-/PlatformIO-Projekt arbeiten:

**Aktuelle Projektentscheidung des Projektinhabers (hat fuer dieses Repository Vorrang):**
- `tools/verify_change.py` und `tools/flash_and_test.py` nicht selbststaendig starten; diese Testskripte stoesst der Projektinhaber bei Bedarf manuell an.
- Die normale Compile-Pruefung einmal ueber den Statusleisten-Task/Shortcut `B Max` ausfuehren.

1. Dieses Repository ist ein **ESP32-/PlatformIO-Projekt** mit lokaler Hardware-Verifikation.
2. Arbeiten Sie **minimal-invasiv**. Vermeiden Sie grossflaechige Refactorings, wenn nur eine lokale Aenderung verlangt ist.
3. Bei Aenderungen an **Weboberflaeche, WLAN, MQTT, Statusanzeige oder Bootlog** nach Moeglichkeit lokal verifizieren.
4. Fuer Verifikation bevorzugt **bestehende Projektskripte** verwenden, insbesondere `tools/verify_change.py` und `tools/flash_and_test.py`, statt freier Ad-hoc-Kommandos.
5. Serielle Marker bevorzugen. Bevorzugte normierte Ausgaben sind:
   - `[FLASH_TEST] IP=<ipv4>`
   - `[FLASH_TEST] READY`
6. Wenn eine Aenderung die **UI** betrifft, nach Flash/Reset die passende Weboberflaeche oder relevante HTTP-Endpunkte pruefen.
7. Wenn eine Aenderung **WLAN, Boot oder Erreichbarkeit** betrifft, Bootverhalten, IP-Erkennung, READY-Erkennung und HTTP-Erreichbarkeit pruefen.
8. Wenn eine Verifikation fehlschlaegt, Logs **strukturiert auswerten** und gezielt nachbessern. Keine wilden Vermutungen aus unstrukturierten Logfragmenten ableiten.
9. Vorhandene Build-, Upload- und Test-Infrastruktur **wiederverwenden**, nicht unnötig umbauen.
10. Wenn die Firmware die normierten Marker noch nicht ausgibt, das im Bericht als **Empfehlung** nennen, aber nicht blind in unklare Dateien hineinschreiben.
11. Vor Abschluss immer kurz berichten:
   - welche Dateien geaendert wurden
   - welche Verifikation ausgefuehrt wurde
   - PASS oder FAIL
   - offene Risiken oder Restpunkte
