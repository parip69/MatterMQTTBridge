// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#include "MatterBridgeManager.h"

#include <Arduino.h>

#include "Bridge.h"
#include "SettingsManager.h"

extern SettingsManager settingsManager;
extern bool bridgeExecuteTrigger(uint8_t triggerNumber, const char* sourceTag);
extern void addLogMessage(const String& message);

#ifdef BRIDGE_MATTER_ENABLED
#include <Matter.h>
#include <MatterEndpoints/MatterOnOffPlugin.h>
#define BRIDGE_MATTER_HEADER_AVAILABLE 1
#define BRIDGE_MATTER_ENDPOINT_AVAILABLE 1
#else
#define BRIDGE_MATTER_HEADER_AVAILABLE 0
#define BRIDGE_MATTER_ENDPOINT_AVAILABLE 0
#endif

#if BRIDGE_MATTER_ENDPOINT_AVAILABLE
static constexpr uint8_t kMatterTriggerSlotCount = 5;
static MatterOnOffPlugin gMatterTriggerEndpoints[kMatterTriggerSlotCount];
static bool gMatterResetPending[kMatterTriggerSlotCount] = {false, false, false, false, false};
static const char* kMatterTriggerLabels[kMatterTriggerSlotCount] = {
    "Haustuer oeffnen",
    "Haustuer schliessen",
    "Falle ziehen",
    "Status abfragen",
    "Reserve"
};

static bool handleMatterTriggerState(uint8_t slot, bool state) {
    if (!state) {
        addLogMessage("Matter Slot " + String(slot) + " OFF erhalten");
        return true;
    }

    addLogMessage("Matter Slot " + String(slot) + " ON erhalten -> TriggerOutPin=" + String(slot));
    if (!bridgeExecuteTrigger(slot, "MATTER")) {
        addLogMessage("Matter Fehler: TriggerOutPin=" + String(slot) + " konnte nicht gesendet werden");
        return false;
    }

    gMatterResetPending[slot - 1] = true;
    return true;
}
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

    bool endpointsReady = true;
    for (uint8_t slot = 1; slot <= kMatterTriggerSlotCount; ++slot) {
        if (!gMatterTriggerEndpoints[slot - 1].begin(false)) {
            addLogMessage("Matter Fehler: OnOff-Endpunkt fuer Slot " + String(slot) + " konnte nicht erstellt werden");
            endpointsReady = false;
            break;
        }

        addLogMessage("Matter Slot " + String(slot) + " bereit: " + String(kMatterTriggerLabels[slot - 1]));
        gMatterTriggerEndpoints[slot - 1].onChange([slot](bool state) -> bool {
            return handleMatterTriggerState(slot, state);
        });
    }

    if (!endpointsReady) {
        ready_ = false;
        return;
    }

    Matter.begin();
    ready_ = true;
    addLogMessage("Matter gestartet");

    refreshPairingInfo();
    printPairingToSerial();
#else
    ready_ = false;
    addLogMessage("Matter deaktiviert: 5-Pin-Basisfirmware aktiv");
#endif
}

void MatterBridgeManager::loop() {
#if BRIDGE_MATTER_HEADER_AVAILABLE && BRIDGE_MATTER_ENDPOINT_AVAILABLE
    if (!ready_) {
        return;
    }

    for (uint8_t slot = 1; slot <= kMatterTriggerSlotCount; ++slot) {
        if (!gMatterResetPending[slot - 1]) {
            continue;
        }

        gMatterResetPending[slot - 1] = false;
        if (!gMatterTriggerEndpoints[slot - 1].setOnOff(false)) {
            addLogMessage("Matter Hinweis: Slot " + String(slot) + " konnte nicht automatisch auf OFF zurueckgesetzt werden");
        }
    }
#endif
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
    addLogMessage("Matter deaktiviert: Pairing-API nicht verfuegbar");
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
