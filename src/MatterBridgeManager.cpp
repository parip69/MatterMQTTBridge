// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#include "MatterBridgeManager.h"

#include <Arduino.h>

#include "Bridge.h"
#include "SettingsManager.h"

extern SettingsManager settingsManager;
extern bool bridgeSendMatterFunction(const String &matterFunction, uint8_t fallbackTriggerNumber);
extern void addLogMessage(const String& message);

#ifdef BRIDGE_MATTER_ENABLED
#include <Matter.h>
#include <MatterEndpoints/MatterOnOffLight.h>
#define BRIDGE_MATTER_HEADER_AVAILABLE 1
#define BRIDGE_MATTER_ENDPOINT_AVAILABLE 1
#else
#define BRIDGE_MATTER_HEADER_AVAILABLE 0
#define BRIDGE_MATTER_ENDPOINT_AVAILABLE 0
#endif

#if BRIDGE_MATTER_ENDPOINT_AVAILABLE
static MatterOnOffLight gOnOffLight;
#endif

void MatterBridgeManager::refreshPairingInfo() {
#if BRIDGE_MATTER_HEADER_AVAILABLE
    pairingCode_ = Matter.getManualPairingCode();
    qrCode_ = Matter.getOnboardingQRCodeUrl();

    if (!pairingCode_.isEmpty()) {
        addLogMessage("Pairing-Code verfuegbar");
    }
    if (!qrCode_.isEmpty()) {
        addLogMessage("QR-Code verfuegbar");
    }
#else
    pairingCode_ = "";
    qrCode_ = "";
#endif
}

void MatterBridgeManager::printPairingToSerial() const {
    Serial.println("=== MATTER START ===");
    Serial.println("Matter gestartet");
    Serial.printf("Pairing Code: %s\r\n", pairingCode_.c_str());
    Serial.printf("QR: %s\r\n", qrCode_.c_str());
    Serial.println("====================");
}

void MatterBridgeManager::begin() {
    if (started_) {
        return;
    }
    started_ = true;

#if BRIDGE_MATTER_HEADER_AVAILABLE && BRIDGE_MATTER_ENDPOINT_AVAILABLE
    String deviceName = settingsManager.getWifiSettings().hostname;
    deviceName.trim();
    if (deviceName.isEmpty()) {
        deviceName = "MatterMQTTBridge";
    }

    if (!gOnOffLight.begin(false)) {
        addLogMessage("Matter Fehler: OnOff-Endpunkt konnte nicht erstellt werden");
        ready_ = false;
        return;
    }

    gOnOffLight.onChange([](bool state) -> bool {
        if (state) {
            addLogMessage("Matter ON erhalten -> Funktion open");
            if (!bridgeSendMatterFunction("open", 1)) {
                addLogMessage("Matter Fehler: Funktion open konnte nicht gesendet werden");
                return false;
            }
            return true;
        }

        addLogMessage("Matter OFF erhalten -> Funktion close");
        if (!bridgeSendMatterFunction("close", 2)) {
            addLogMessage("Matter Fehler: Funktion close konnte nicht gesendet werden");
            return false;
        }
        return true;
    });

    Matter.begin();
    ready_ = true;
    addLogMessage("Matter gestartet");

    refreshPairingInfo();
    printPairingToSerial();
#else
    ready_ = false;
    addLogMessage("Matter Fehler: Matter API im aktuellen Build nicht verfuegbar");
#endif
}

void MatterBridgeManager::loop() {
    // Aktuell keine zyklische Verarbeitung noetig.
}

bool MatterBridgeManager::startPairing() {
#if BRIDGE_MATTER_HEADER_AVAILABLE
    if (!ready_) {
        addLogMessage("Matter Fehler: Pairing kann nicht gestartet werden (Matter nicht bereit)");
        return false;
    }

    if (Matter.isDeviceCommissioned()) {
        // Arduino-Matter bietet kein dediziertes API fuer Commissioning-Window.
        // Decommission startet den Pairing-Zustand erneut.
        Matter.decommission();
    }

    refreshPairingInfo();
    printPairingToSerial();
    addLogMessage("Matter Pairing gestartet");
    return true;
#else
    addLogMessage("Matter Fehler: Pairing-API nicht verfuegbar");
    return false;
#endif
}

bool MatterBridgeManager::isReady() const {
    return ready_;
}

String MatterBridgeManager::getPairingCode() const {
    return pairingCode_;
}

String MatterBridgeManager::getQrCode() const {
    return qrCode_;
}
