// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#include "Bridge.h"

extern bool publishMqttMessage(const String &topic, const String &message, bool retain = false, int qos = 0);
extern String mqttRootTopic;

String bridgeTriggerTopic() {
    String root = mqttRootTopic;
    root.trim();
    if (root.isEmpty()) root = "fingerprint";
    while (root.endsWith("/")) {
        root.remove(root.length() - 1);
    }
    return root + "/TriggerOutPin";
}

String bridgeTriggerPayload(uint8_t triggerNumber) {
    return String(triggerNumber);
}

bool bridgeSendTrigger(uint8_t triggerNumber) {
    if (triggerNumber < 1 || triggerNumber > 99) return false;
    return publishMqttMessage(bridgeTriggerTopic(), bridgeTriggerPayload(triggerNumber), false, 0);
}
