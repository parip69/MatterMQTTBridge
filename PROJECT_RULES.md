# MatterMQTTBridge Projektregeln

Dieses Projekt ist eine separate Matter-zu-MQTT-Bridge für den bestehenden Fingerprint-ESP32.

Der bestehende Fingerprint-ESP32 bleibt die Zentrale und kann weiterhin als MQTT-Broker laufen.

Die MatterBridge läuft nur als MQTT-Client.

## Ziele

- Matter / Google Home anbinden
- MQTT-Befehle an Fingerprint senden
- MQTT-Sensorwerte empfangen
- kleine Weboberfläche für WLAN, MQTT, Matter und OTA
- ElegantOTA unter /update
- Matter Pairing-Code und QR-Code im Web und im Serial Monitor anzeigen

## Nicht erlaubt

- keine Fingerprint-Logik übernehmen
- keine Tedee-Logik übernehmen
- keine Nuki-Logik übernehmen
- keine Telegram-Logik übernehmen
- kein MQTT-Broker auf der Bridge
- keine unnötigen großen Refactorings
- keine Funktion entfernen, bevor sie als unnötig bestätigt wurde

## Architektur

Matter / Google Home
→ MatterMQTTBridge
→ MQTT Client
→ Fingerprint-ESP32 MQTT-Broker
→ bestehende Output-/Trigger-/Modul-Logik

## Arbeitsweise

Immer kleine Schritte.
Nach jedem größeren Schritt muss das Projekt kompilieren.
Keine großen Umbauten auf einmal.

## Versionsregel

Bei jeder bereitgestellten Download-Datei oder wichtigem Stand wird die Versionsnummer erhöht.

Start:
MatterMQTTBridge_BachnerGerhard_Ver1
