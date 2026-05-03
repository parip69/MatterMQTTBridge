# MatterMQTTBridge

**Version:** MatterMQTTBridge_BachnerGerhard_Ver1

## Beschreibung

ESP32-basierte Matter-zu-MQTT-Bridge für den bestehenden Fingerprint-ESP32.

Der Fingerprint-ESP32 bleibt die Zentrale und kann weiterhin als MQTT-Broker laufen.
Diese Bridge läuft nur als MQTT-Client und verbindet Matter / Google Home mit dem bestehenden System.

## Ziele

- Matter / Google Home anbinden
- MQTT-Befehle an Fingerprint senden
- MQTT-Sensorwerte empfangen
- Kleine Weboberfläche für WLAN, MQTT, Matter und OTA
- ElegantOTA unter `/update`
- Matter Pairing-Code und QR-Code im Web und im Serial Monitor

## Architektur

```
Matter / Google Home
  → MatterMQTTBridge (dieser ESP32)
    → MQTT Client
      → Fingerprint-ESP32 MQTT-Broker
        → bestehende Output-/Trigger-/Modul-Logik
```

## Build

```bash
# Firmware bauen
pio run -e max

# Filesystem bauen
pio run -e max -t buildfs

# Flashen
pio run -e max -t upload
pio run -e max -t uploadfs
```

## Projektregeln

Siehe [PROJECT_RULES.md](PROJECT_RULES.md)

## TODO

Siehe [TODO_BRIDGE.md](TODO_BRIDGE.md)
