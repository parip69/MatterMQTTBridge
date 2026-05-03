// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#pragma once
#include <Arduino.h>

// Sendet einen MQTT-Trigger-Befehl über den konfigurierten Root-Topic
void bridgeSendTrigger(uint8_t triggerNumber);
