// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#include "Bridge.h"

extern void publishMqttMessage(const String &topic, const String &message, bool retain = false, int qos = 0);
extern String mqttRootTopic;

String bridgeTriggerTopic() {
    return mqttRootTopic + "/TriggerOutPin";
}

String bridgeTriggerPayload(uint8_t triggerNumber) {
    return String(triggerNumber);
}

bool bridgeSendTrigger(uint8_t triggerNumber) {
    if (triggerNumber < 1 || triggerNumber > 99) return false;
    publishMqttMessage(bridgeTriggerTopic(), bridgeTriggerPayload(triggerNumber), false, 0);
    return true;
}
