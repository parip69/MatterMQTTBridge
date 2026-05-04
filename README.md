# MatterMQTTBridge

## Beschreibung
Schlanke ESP32 MQTT-Bridge-Basis als Zwischenschritt.
Dies ist die bereinigte Firmware, die später für die Matter-Integration verwendet werden soll.
Sie enthält nur:
- WLAN
- Webserver
- OTA
- MQTT Broker/Client
- Bridge-Trigger API

Der Fingerprint-ESP bleibt weiterhin die Hauptzentrale. Die Trigger werden über Topic `<mqttRootTopic>/TriggerOutPin` an diesen gemeldet.

## Build
```bash
pio run -e bridge
pio run -e bridge -t buildfs
pio run -e bridge -t uploadfs
pio run -e bridge -t upload
```

Matter ist noch nicht eingebaut! Google Home ist noch nicht aktiv!
