<!-- Kurzanleitung fuer die praktische Nutzung des Skills und des Workspace-Agents. -->

# Nutzung

## In VS Code Copilot Chat

- Oeffne den Workspace in VS Code.
- Rufe den Agenten `esp32-verify` oder den Skill `esp32-flash-verify` im Prompt direkt auf.
- Formuliere die Aenderung und nenne bei Bedarf das gewuenschte Szenario wie `ui_button`, `wifi_boot` oder `mqtt_state`.

## In Codex

- Arbeite im Projekt-Root.
- Bitte den Agenten explizit, den Skill `esp32-flash-verify` zu nutzen.
- Verweise bei Bedarf auf `tools/verify_change.py`, wenn ein reproduzierbarer lokaler Testlauf gewuenscht ist.

## Beispielanweisungen

1. `Fuege unten einen Button hinzu und verifiziere die Aenderung lokal.`
2. `Optimiere die WLAN-Startlogik minimal-invasiv und fuehre das Szenario wifi_boot aus.`
3. `Pruefe nach der MQTT-Aenderung Flash, Serial-Log und HTTP-Status.`
4. `Nutze den ESP32-Verify-Skill und teste nur die geaenderte UI-Funktion.`
5. `Wenn FAIL, lies die Logs und verbessere gezielt weiter.`
