<!-- Referenz fuer normierte Marker und fuer eine stabile, lokale ESP32-Verifikation. -->

# Verifikationsmarker

Fuer stabile lokale Geraetetests sollten serielle Logmarker moeglichst normiert ausgegeben werden:

- `[FLASH_TEST] IP=<ipv4>`
- `[FLASH_TEST] READY`

## Warum diese Marker wichtig sind

- Sie machen die IP-Erkennung robust und vermeiden Fehlgriffe auf MQTT- oder Payload-IPs.
- Sie erlauben eine klare Trennung zwischen Bootphase, Netzwerkanbindung und HTTP-Test.
- Sie machen wiederholbare Skripte moeglich, ohne chaotische Freitext-Logs erraten zu muessen.

## Empfehlung fuer Agenten

- Marker bevorzugt lesen, nicht beliebige IP-Muster aus anderen Logzeilen uebernehmen.
- Wenn Marker fehlen, dies im Bericht als Verbesserungsvorschlag nennen.
- Marker nicht blind in unbekannte Firmware-Dateien schreiben, solange die richtige Stelle nicht klar identifiziert ist.
