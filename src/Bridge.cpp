// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#include "Bridge.h"
#include "global.h"

extern void publishMqttMessage(const String &topic, const String &message, bool retain, int qos);

void bridgeSendTrigger(uint8_t triggerNumber)
{
    String topic = makeTopic("trigger/" + String(triggerNumber));
    publishMqttMessage(topic, "true", false, 0);
}
