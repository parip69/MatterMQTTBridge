// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#pragma once

#include <Arduino.h>

class MatterBridgeManager {
public:
    void begin();
    void loop();
    bool startPairing();
    bool isReady() const;
    String getPairingCode() const;
    String getQrCode() const;

private:
    void refreshPairingInfo();
    void printPairingToSerial() const;

    bool ready_ = false;
    bool started_ = false;
    String pairingCode_;
    String qrCode_;
};
