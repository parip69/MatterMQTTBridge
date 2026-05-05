// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

bool bridgeSendTrigger(uint8_t triggerNumber);
bool bridgeSendMatterFunction(const String &matterFunction, uint8_t fallbackTriggerNumber);
String bridgeTriggerTopic();
String bridgeTriggerPayload(uint8_t triggerNumber);
String bridgeConfiguredSendTopic(uint8_t triggerNumber);
String bridgeConfiguredSendPayload(uint8_t triggerNumber);

void bridgeLoadMetaConfig();
void bridgeWriteMetaConfig(JsonObject target);
bool bridgeSaveMetaConfig(JsonVariant source, String &error);
bool bridgeHandleMetaFeedback(const String &topic, const String &payload);
size_t bridgeMetaSlotCount();
