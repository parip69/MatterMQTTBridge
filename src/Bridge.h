// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#pragma once
#include <Arduino.h>

bool bridgeExecuteTrigger(uint8_t triggerNumber, const char* sourceTag = "BRIDGE");
bool bridgeSendTrigger(uint8_t triggerNumber);
String bridgeTriggerTopic();
String bridgeTriggerPayload(uint8_t triggerNumber);
