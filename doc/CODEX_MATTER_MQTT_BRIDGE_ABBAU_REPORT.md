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
