# Abbau Report: Matter MQTT Bridge

- Merge-Konflikte entfernt
- alte main.cpp archiviert
- neue Bridge-main.cpp erstellt
- Bridge.cpp Topic/Payload korrigiert
- SettingsManager bereinigt
- Einseiten-WebUI erstellt
- settings.html auf index.html reduziert/weitergeleitet
- /save_settings ohne delay im Async-Handler
- hasActiveWebClients korrigiert
- mqttRootTopic normalisiert
- Bridge Trigger robust
- Build-Ergebnis dokumentiert

Der Aufbau der Bridge ist hiermit strukturell isoliert von alter Fingerprint-Hardware.
