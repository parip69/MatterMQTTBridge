// @version: 0.0.1 Builddatum 00:00:00 01-01.2025
#include "Bridge.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

extern bool publishMqttMessage(const String &topic, const String &message, bool retain = false, int qos = 0);
extern String mqttRootTopic;
extern void addLogMessage(const String& message);

namespace {
constexpr const char *META_CONFIG_PATH = "/meta_config.json";
constexpr size_t MAX_META_SLOTS = 10;

struct BridgeMetaSlot {
    bool enabled = true;
    uint8_t nr = 0;
    String name;
    String matterFunction;
    uint8_t triggerOutPin = 0;
    String sendTopic;
    String sendPayload;
    String listenTopic;
    String listenPayload;
    String statusText;
};

BridgeMetaSlot s_metaSlots[MAX_META_SLOTS];
size_t s_metaSlotCount = 0;
bool s_metaLoaded = false;

String trimCopy(String value) {
    value.trim();
    return value;
}

String normalizedRootTopic() {
    String root = mqttRootTopic;
    root.trim();
    if (root.isEmpty()) root = "fingerprint";
    while (root.endsWith("/")) {
        root.remove(root.length() - 1);
    }
    return root;
}

String normalizeFunction(String value) {
    value.trim();
    value.toLowerCase();
    if (value.isEmpty()) value = "custom";
    return value;
}

String resolveMetaTopic(String topic) {
    topic.trim();
    if (topic.isEmpty()) return topic;

    const String root = normalizedRootTopic();
    topic.replace("{root}", root);
    topic.replace("${root}", root);

    if (topic.startsWith("/")) {
        topic = root + topic;
    }

    if (root != "fingerprint" && topic.startsWith("fingerprint/")) {
        topic = root + topic.substring(String("fingerprint").length());
    }
    return topic;
}

String defaultTriggerTopic() {
    return normalizedRootTopic() + "/TriggerOutPin";
}

String defaultPayload(uint8_t triggerNumber) {
    return String(triggerNumber);
}

void setMetaSlot(
    BridgeMetaSlot &slot,
    uint8_t nr,
    const char *name,
    const char *matterFunction,
    uint8_t triggerOutPin,
    const char *listenTail,
    const char *listenPayload,
    const char *statusText
) {
    const String root = normalizedRootTopic();
    slot.enabled = true;
    slot.nr = nr;
    slot.name = name;
    slot.matterFunction = matterFunction;
    slot.triggerOutPin = triggerOutPin;
    slot.sendTopic = root + "/TriggerOutPin";
    slot.sendPayload = String(triggerOutPin);
    slot.listenTopic = root + "/" + String(listenTail);
    slot.listenPayload = listenPayload;
    slot.statusText = statusText;
}

void setDefaultMetaConfig() {
    s_metaSlotCount = 5;
    setMetaSlot(s_metaSlots[0], 1, "Haustuer oeffnen", "open", 1, "OutputPinStatus1", "true", "Oeffnen / Trigger 1");
    setMetaSlot(s_metaSlots[1], 2, "Haustuer schliessen", "close", 2, "OutputPinStatus2", "true", "Schliessen / Trigger 2");
    setMetaSlot(s_metaSlots[2], 3, "Falle ziehen", "unlatch", 3, "OutputPinStatus3", "true", "Falle ziehen / Trigger 3");
    setMetaSlot(s_metaSlots[3], 4, "Status abfragen", "status", 4, "status", "online", "Fingerprint online");
    setMetaSlot(s_metaSlots[4], 5, "Reserve / Zusatz", "custom", 5, "OutputPinStatus5", "true", "Reserve / Trigger 5");
}

uint8_t readSlotByte(JsonVariant value, uint8_t fallback) {
    if (value.isNull()) return fallback;
    int raw = value.as<int>();
    if (raw < 0) return fallback;
    if (raw > 99) return 99;
    return static_cast<uint8_t>(raw);
}

bool applyMetaConfig(JsonVariant source, String &error) {
    if (!source.is<JsonObject>()) {
        error = "meta config is not an object";
        return false;
    }

    JsonArray slots = source["slots"].as<JsonArray>();
    if (slots.isNull()) {
        error = "slots missing";
        return false;
    }

    BridgeMetaSlot parsed[MAX_META_SLOTS];
    size_t parsedCount = 0;

    for (JsonVariant item : slots) {
        if (parsedCount >= MAX_META_SLOTS) break;
        if (!item.is<JsonObject>()) continue;

        BridgeMetaSlot slot;
        slot.enabled = item["enabled"] | true;
        slot.nr = readSlotByte(item["nr"], static_cast<uint8_t>(parsedCount + 1));
        slot.name = trimCopy(String(item["name"] | ""));
        slot.matterFunction = normalizeFunction(String(item["matterFunction"] | "custom"));
        slot.triggerOutPin = readSlotByte(item["triggerOutPin"], slot.nr);
        slot.sendTopic = trimCopy(String(item["sendTopic"] | ""));
        slot.sendPayload = trimCopy(String(item["sendPayload"] | ""));
        slot.listenTopic = trimCopy(String(item["listenTopic"] | ""));
        slot.listenPayload = trimCopy(String(item["listenPayload"] | ""));
        slot.statusText = trimCopy(String(item["statusText"] | ""));

        if (slot.nr == 0) slot.nr = static_cast<uint8_t>(parsedCount + 1);
        if (slot.triggerOutPin == 0) slot.triggerOutPin = slot.nr;
        if (slot.name.isEmpty()) slot.name = "Funktion " + String(slot.nr);
        if (slot.sendTopic.isEmpty()) slot.sendTopic = defaultTriggerTopic();
        if (slot.sendPayload.isEmpty()) slot.sendPayload = defaultPayload(slot.triggerOutPin);
        if (slot.statusText.isEmpty()) slot.statusText = slot.name;

        parsed[parsedCount++] = slot;
    }

    s_metaSlotCount = parsedCount;
    for (size_t i = 0; i < parsedCount; i++) {
        s_metaSlots[i] = parsed[i];
    }
    s_metaLoaded = true;
    return true;
}

void ensureMetaLoaded() {
    if (!s_metaLoaded) {
        bridgeLoadMetaConfig();
    }
}

bool writeMetaConfigFile(String &error) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    bridgeWriteMetaConfig(root);

    File file = LittleFS.open(META_CONFIG_PATH, "w");
    if (!file) {
        error = "meta config file open failed";
        return false;
    }

    size_t written = serializeJson(doc, file);
    file.close();
    if (written == 0) {
        error = "meta config file write failed";
        return false;
    }
    return true;
}

bool findSlotByTrigger(uint8_t triggerNumber, BridgeMetaSlot &slot) {
    ensureMetaLoaded();
    for (size_t i = 0; i < s_metaSlotCount; i++) {
        if (s_metaSlots[i].enabled && s_metaSlots[i].triggerOutPin == triggerNumber) {
            slot = s_metaSlots[i];
            return true;
        }
    }
    return false;
}

bool findSlotByFunction(const String &matterFunction, BridgeMetaSlot &slot) {
    ensureMetaLoaded();
    const String wanted = normalizeFunction(matterFunction);
    for (size_t i = 0; i < s_metaSlotCount; i++) {
        if (s_metaSlots[i].enabled && normalizeFunction(s_metaSlots[i].matterFunction) == wanted) {
            slot = s_metaSlots[i];
            return true;
        }
    }
    return false;
}

bool publishSlot(const BridgeMetaSlot &slot, uint8_t fallbackTriggerNumber) {
    String topic = resolveMetaTopic(slot.sendTopic);
    if (topic.isEmpty()) topic = defaultTriggerTopic();

    String payload = slot.sendPayload;
    payload.trim();
    if (payload.isEmpty()) payload = defaultPayload(fallbackTriggerNumber);

    return publishMqttMessage(topic, payload, false, 0);
}

bool payloadMatches(String actual, String expected) {
    actual.trim();
    expected.trim();
    actual.toLowerCase();
    expected.toLowerCase();

    if (expected.isEmpty()) return true;
    if (actual == expected) return true;
    return actual.endsWith(";" + expected);
}
}

String bridgeTriggerTopic() {
    return defaultTriggerTopic();
}

String bridgeTriggerPayload(uint8_t triggerNumber) {
    return String(triggerNumber);
}

bool bridgeSendTrigger(uint8_t triggerNumber) {
    if (triggerNumber < 1 || triggerNumber > 99) return false;
    BridgeMetaSlot slot;
    if (findSlotByTrigger(triggerNumber, slot)) {
        return publishSlot(slot, triggerNumber);
    }
    return publishMqttMessage(bridgeTriggerTopic(), bridgeTriggerPayload(triggerNumber), false, 0);
}

bool bridgeSendMatterFunction(const String &matterFunction, uint8_t fallbackTriggerNumber) {
    BridgeMetaSlot slot;
    if (findSlotByFunction(matterFunction, slot)) {
        return publishSlot(slot, fallbackTriggerNumber);
    }
    return bridgeSendTrigger(fallbackTriggerNumber);
}

String bridgeConfiguredSendTopic(uint8_t triggerNumber) {
    BridgeMetaSlot slot;
    if (findSlotByTrigger(triggerNumber, slot)) {
        return resolveMetaTopic(slot.sendTopic);
    }
    return bridgeTriggerTopic();
}

String bridgeConfiguredSendPayload(uint8_t triggerNumber) {
    BridgeMetaSlot slot;
    if (findSlotByTrigger(triggerNumber, slot)) {
        String payload = slot.sendPayload;
        payload.trim();
        if (!payload.isEmpty()) return payload;
    }
    return bridgeTriggerPayload(triggerNumber);
}

void bridgeLoadMetaConfig() {
    if (!LittleFS.exists(META_CONFIG_PATH)) {
        setDefaultMetaConfig();
        s_metaLoaded = true;
        addLogMessage("Meta-Konfiguration: Standard geladen");
        return;
    }

    File file = LittleFS.open(META_CONFIG_PATH, "r");
    if (!file) {
        setDefaultMetaConfig();
        s_metaLoaded = true;
        addLogMessage("Meta-Konfiguration: Standard geladen (Datei nicht lesbar)");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    String error;
    if (err || !applyMetaConfig(doc.as<JsonVariant>(), error)) {
        setDefaultMetaConfig();
        s_metaLoaded = true;
        addLogMessage("Meta-Konfiguration: Standard geladen (JSON ungueltig)");
        return;
    }

    addLogMessage("Meta-Konfiguration geladen: " + String(s_metaSlotCount) + " Slot(s)");
}

void bridgeWriteMetaConfig(JsonObject target) {
    ensureMetaLoaded();

    target["version"] = 1;
    target["rootTopic"] = normalizedRootTopic();
    JsonArray slots = target["slots"].to<JsonArray>();

    for (size_t i = 0; i < s_metaSlotCount; i++) {
        JsonObject item = slots.add<JsonObject>();
        item["enabled"] = s_metaSlots[i].enabled;
        item["nr"] = s_metaSlots[i].nr;
        item["name"] = s_metaSlots[i].name;
        item["matterFunction"] = s_metaSlots[i].matterFunction;
        item["triggerOutPin"] = s_metaSlots[i].triggerOutPin;
        item["sendTopic"] = resolveMetaTopic(s_metaSlots[i].sendTopic);
        item["sendPayload"] = s_metaSlots[i].sendPayload;
        item["listenTopic"] = resolveMetaTopic(s_metaSlots[i].listenTopic);
        item["listenPayload"] = s_metaSlots[i].listenPayload;
        item["statusText"] = s_metaSlots[i].statusText;
    }
}

bool bridgeSaveMetaConfig(JsonVariant source, String &error) {
    if (!applyMetaConfig(source, error)) {
        return false;
    }
    if (!writeMetaConfigFile(error)) {
        return false;
    }
    addLogMessage("Meta-Konfiguration gespeichert: " + String(s_metaSlotCount) + " Slot(s)");
    return true;
}

bool bridgeHandleMetaFeedback(const String &topic, const String &payload) {
    ensureMetaLoaded();
    bool matched = false;

    for (size_t i = 0; i < s_metaSlotCount; i++) {
        const BridgeMetaSlot &slot = s_metaSlots[i];
        if (!slot.enabled || slot.listenTopic.isEmpty()) continue;

        if (topic == resolveMetaTopic(slot.listenTopic) && payloadMatches(payload, slot.listenPayload)) {
            String text = slot.statusText;
            if (text.isEmpty()) text = slot.name;
            addLogMessage("Meta-Horch-Treffer: " + slot.name + " -> " + text);
            matched = true;
        }
    }

    return matched;
}

size_t bridgeMetaSlotCount() {
    ensureMetaLoaded();
    return s_metaSlotCount;
}
