// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#pragma once
#include <Arduino.h>

<<<<<<< HEAD
bool bridgeSendTrigger(uint8_t triggerNumber);
String bridgeTriggerTopic();
String bridgeTriggerPayload(uint8_t triggerNumber);
=======
// Sendet einen MQTT-Trigger-Befehl über den konfigurierten Root-Topic
void bridgeSendTrigger(uint8_t triggerNumber);
>>>>>>> 937c36a21b29111ed2f16fbabeffa236a6d16872
