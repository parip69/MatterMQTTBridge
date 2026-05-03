---
name: esp32-flash-verify
description: Implementiere und verifiziere lokale ESP32-/PlatformIO-Aenderungen minimal-invasiv. Verwende diesen Skill, wenn Aenderungen an UI, WLAN, MQTT, Bootlog, Statusleiste, Webserver oder HTTP/API vorgenommen wurden oder wenn nach einer Aenderung ein lokaler Geraetetest mit Flash, serieller Ausgabe, IP-Erkennung und HTTP-Pruefung erforderlich ist.
---

<!-- Projektinterner Skill fuer deterministische ESP32-Verifikation. -->

# ESP32 Flash Verify

Nutze diesen Skill, wenn eine Aenderung nicht nur implementiert, sondern lokal auf dem Geraet nachvollziehbar verifiziert werden soll.

## Ziel

- Aenderungen minimal-invasiv umsetzen.
- Bestehende Projektwerkzeuge bevorzugen.
- Ergebnisse reproduzierbar mit PASS oder FAIL bewerten.

## Ablauf

1. Verstehe zuerst die betroffene Aenderung und grenze ein, welche Funktion wirklich betroffen ist.
2. Waehle den passenden Verifikationsmodus:
   - `flash-test`, wenn Filesystem/Firmware neu auf das Geraet muessen
   - `reset-test`, wenn ein Reset mit erneutem Boot-Check ausreicht
3. Verwende bevorzugt `tools/verify_change.py` als stabile Einstiegsebene.
4. Falls notwendig, lasse Build/Upload ueber die bestehende PlatformIO-Infrastruktur laufen.
5. Werte die serielle Konsole strukturiert aus und bevorzuge normierte Marker:
   - `[FLASH_TEST] IP=<ipv4>`
   - `[FLASH_TEST] READY`
6. Pruefe danach nur die zur Aenderung passenden HTTP-Endpunkte oder UI-Pfade.
7. Gib am Ende ein klares PASS oder FAIL aus.
8. Wenn FAIL auftritt, grenze die Ursache gezielt ein:
   - Flash/Upload
   - COM-Reset oder Boot
   - IP-Erkennung
   - READY-Erkennung
   - HTTP/UI/API

## Regeln

- Bevorzuge deterministische Skripte statt improvisierter Einzelkommandos.
- Interpretiere chaotische Logs nicht mit freien Vermutungen.
- Pruefe moeglichst nur die geaenderte Funktion statt das ganze System breit neu zu bewerten.
- Halte Berichte knapp und protokolliere Ergebnis, Testweg und offene Risiken.
- Nutze vorhandene Projektdateien weiter, statt parallele Test-Logik aufzubauen.
- Wenn Marker fehlen, empfehle sie im Bericht, schreibe sie aber nicht blind in unbekannte Firmware-Stellen.

## Bevorzugte Werkzeuge

- `tools/verify_change.py`
- `tools/flash_and_test.py`
- `tools/flash_test_config.ini`
- `platformio.ini`
- `agent_worklog.md`

## Typische Einsatzfaelle

- Neuer Button oder neue Anzeige in der Weboberflaeche
- Geaenderte WLAN-Startlogik oder Bootausgabe
- Angepasstes MQTT-Statusverhalten
- Neue Statusleistenaktion im VS-Code-Helferprojekt

## Zusaetzliche Referenzen

- Beispiele: [examples.md](examples.md)
- Nutzung: [USAGE.md](USAGE.md)
- Marker-Hinweise: [resources/verification_markers.md](resources/verification_markers.md)
- Skript-Wrapper: [scripts/run_verify_change.ps1](scripts/run_verify_change.ps1)
