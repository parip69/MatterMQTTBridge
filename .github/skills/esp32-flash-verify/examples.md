<!-- Kompakte Prompt-Beispiele aus der ersten Version -->

# Kompakte Beispiele

1. `Nutze den Skill esp32-flash-verify. Fuege unten auf der Startseite einen neuen Button hinzu und verifiziere die UI-Aenderung lokal.`
2. `Nutze den Skill esp32-flash-verify. Optimiere die WLAN-Startlogik minimal-invasiv und fuehre danach das Szenario wifi_boot aus.`
3. `Nutze den Skill esp32-flash-verify. Pruefe nach meiner MQTT-Aenderung Flash, serielle Ausgabe und HTTP-Status.`
4. `Nutze den Skill esp32-flash-verify. Veraendere nur die Statusleistenaktion im VS-Code-Helferprojekt und teste gezielt die geaenderte Funktion.`
5. `Nutze den Skill esp32-flash-verify. Wenn der Test fehlschlaegt, lies die Logs strukturiert und bessere gezielt nach.`
6. `Nutze den Skill esp32-flash-verify. Fuehre nur einen Reset-Test aus, erkenne die IP ueber serielle Marker und pruefe danach /api/status.`

# Beispiele für esp32-flash-verify

Hier findest du praxisnahe Beispiele, wie du den Skill und Agenten-Workflow im Projekt nutzen kannst. Die Beispiele decken typische Aufgaben, UI-Änderungen, Netzwerk, MQTT, API, Fehlerbehebung und auch generische Suchen ab.

---

## UI/Frontend

**1. Button hinzufügen und testen**
> Füge unten auf der Startseite einen neuen Button "Licht Garage" hinzu und verifiziere die Änderung lokal mit esp32-verify.

**2. UI-Layout anpassen**
> Verschiebe den Statusbereich nach oben und prüfe, ob die Weboberfläche nach dem Flash korrekt angezeigt wird.

**3. Barcode-Scanner-Integration**
> Integriere einen Barcode-Scan-Button in die UI und teste, ob der Scan-Event korrekt an die API weitergeleitet wird.

---

## Netzwerk/WLAN

**4. WLAN-Startlogik optimieren**
> Optimiere die WLAN-Initialisierung minimal-invasiv und führe das Szenario wifi_boot aus.

**5. Fallback-AP testen**
> Ändere die Logik für den Fallback-Access-Point und prüfe, ob nach einem WLAN-Fehler die AP-Seite erreichbar ist.

---

## MQTT/Backend

**6. MQTT-Status prüfen**
> Passe die MQTT-Statusmeldung an und führe nach dem Flash einen gezielten mqtt_state-Test durch.

**7. Broker-Disconnect simulieren**
> Simuliere einen MQTT-Broker-Disconnect und prüfe, ob die Firmware korrekt reconnectet.

---

## API/HTTP

**8. API-Endpunkt erweitern**
> Ergänze den /api/status-Endpunkt um ein neues Feld und teste gezielt nur diesen Endpunkt nach dem Flash.

**9. HTTP-Fehlerbehandlung testen**
> Baue eine Fehlerbehandlung für HTTP 500 ein und prüfe, ob der Agent nach dem Flash ein FAIL korrekt erkennt.

---

## Serielle Logs/Fehlerbehebung

**10. Serial-Log auf Marker prüfen**
> Füge einen neuen Marker [FLASH_TEST] BARCODE hinzu und prüfe, ob dieser nach dem Scan im Serial-Log erscheint.

**11. Fehler-Iteration**
> Wenn der Test fehlschlägt, lies die Logs, grenze die Ursache ein und verbessere gezielt, bis PASS erreicht ist.

---

## Generische Suchen & Spezialfälle

**12. Suche nach Barcode-Events**
> Suche im gesamten Projekt nach allen Stellen, an denen Barcode-Events verarbeitet werden, und dokumentiere die Fundstellen.

**13. Test für neue Hardware**
> Integriere einen neuen Temperatursensor und führe nach der Implementierung einen serial_check und api_check durch.

**14. UI-Button mit Spezialfunktion**
> Füge einen Button "Notruf" hinzu, der einen speziellen API-Call auslöst, und prüfe gezielt nur diese Funktion.

---

Diese Beispiele kannst du direkt als Vorlage für deine Arbeit mit dem Agenten-Workflow nutzen oder individuell anpassen.

---

## Projektspezifische Beispiele (Fingerprint/ESP32)

**15. Sonnenaufgang/Sonnenuntergang berechnen und prüfen**
> Ändere die SolarCalc-Logik, führe nach dem Flash einen Test durch, prüfe ob die berechneten Zeiten für Sonnenaufgang und Sonnenuntergang im Webinterface und per API korrekt angezeigt werden.

**16. MQTT-Integration testen**
> Passe die MQTT-Topic-Logik (z.B. fingerscanner/matchId) an und führe nach dem Flash gezielt einen mqtt_state-Test durch. Prüfe, ob die Topics korrekt publiziert und empfangen werden.

**17. WiFi-Konfiguration und AP-Modus prüfen**
> Optimiere die WiFi-Konfiguration (z.B. AP-Timeout oder SSID) und führe das Szenario wifi_boot aus. Prüfe, ob der AP-Modus nach 10s Touch korrekt startet und die Konfigurationsseite erreichbar ist.

**18. Tedee Smart Lock API testen**
> Ändere die Tedee-Bridge-Integration (IP, Token, LockId) und prüfe nach dem Flash gezielt, ob der HTTP-Call an die Bridge funktioniert und der Türöffner korrekt angesteuert wird.

**19. Sensor-Pairing validieren**
> Passe die Sensor-Pairing-Logik an (sensorPairingCode) und prüfe, ob nach einer Änderung ein nicht gepairter Sensor keine Fingerabdrücke mehr akzeptiert, aber der Ring weiterhin funktioniert.

**20. Touch-Ring bei Regen deaktivieren**
> Ändere die Logik für ignoreTouchRing (MQTT), simuliere Regen und prüfe, ob der Touch-Ring zuverlässig deaktiviert wird und keine Fehlauslösungen auftreten.

**21. DHT22-Sensor testen**
> Ändere die DHT22-Integration (GPIO25) und prüfe, ob Temperatur und Luftfeuchtigkeit nach dem Flash korrekt im Webinterface und per MQTT angezeigt werden.

**22. SPIFFS-Webinterface aktualisieren**
> Aktualisiere eine HTML-Datei im data/-Ordner, baue und lade das Filesystem neu hoch, prüfe gezielt nur die geänderte Webfunktion (z.B. neues Setting oder Live-Status).
