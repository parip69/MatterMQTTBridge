// Beschreibung: 2.0.0 erste wo alles get mit dem Templade dowenload
//******************************************************
//         Main of MatterMQTTBridge.
// nur hier die start wert der version Aendern.OK=======
// @version: 1.0.67 <br> Builddatum 16:38:47 08-05.2026
//****************************************************

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

#include "SettingsManager.h"
#include "Bridge.h"
#include "MatterBridgeManager.h"
#include "global.h"

#if USE_MQTT_CLIENT
#include <AsyncMqttClient.h>
#include "MqttConnectionManager.h"
#endif

#if USE_MQTT_BROKER
#include "ESPAsyncMQTTBroker.h"
#endif

// ======================= TEST-AUSGABE-PINS (optional) =======================
#define USE_OUTPUT_TEST_PINS 0   // auf 1 setzen um TestPins zu aktivieren
#define TEST_OUTPUT_PIN_1 25
#define TEST_OUTPUT_PIN_2 26
#define TEST_OUTPUT_PIN_3 27
#define TEST_OUTPUT_PIN_4 32
#define TEST_OUTPUT_PIN_5 33

// ======================= GLOBALS =======================
const char* firmwareVersion = "1.0.67 <br> Builddatum 16:38:47 08-05.2026";
AsyncWebServer webServer(80);
SettingsManager settingsManager;
MatterBridgeManager matterBridge;

String mqttRootTopic = "fingerprint";
SemaphoreHandle_t networkMutex = nullptr;
volatile bool timeSet = false;
int led1State = LOW;
bool isApConfigMode = false;

static bool restartPending = false;
static uint32_t restartAtMs = 0;
static bool loggedIn = false;
static uint32_t lastActivityTime = 0;
static const uint32_t WEB_LOGIN_TIMEOUT_MS = 10UL * 60UL * 1000UL;

static String s_weatherTemperature = "";
static String s_weatherHumidity = "";
static String s_weatherFeelsLike = "";
static String s_weatherDewPoint = "";
static String s_weatherTemperatureSource = "";
static String s_weatherHumiditySource = "";
static String s_weatherFeelsLikeSource = "";
static String s_weatherDewPointSource = "";
static String s_weatherTemperatureTopic = "";
static String s_weatherHumidityTopic = "";
static String s_weatherFeelsLikeTopic = "";
static String s_weatherDewPointTopic = "";
static uint32_t s_weatherTemperatureAtMs = 0;
static uint32_t s_weatherHumidityAtMs = 0;
static uint32_t s_weatherFeelsLikeAtMs = 0;
static uint32_t s_weatherDewPointAtMs = 0;
static const uint32_t WEATHER_VALUE_FRESH_MS = 3UL * 60UL * 1000UL;

// ======================= LOG PUFFER =======================
#define LOG_BUFFER_SIZE 50
static String s_logBuf[LOG_BUFFER_SIZE];
static int    s_logHead  = 0;
static int    s_logCount = 0;

AsyncEventSource events("/events");

#if USE_MQTT_CLIENT
AsyncMqttClient mqttClient;
MqttConnectionManager mqttManager(mqttClient, settingsManager);
#endif

#if USE_MQTT_BROKER
ESPAsyncMQTTBroker mqttBroker;
#endif

// ======================= DUMMYS FÜR GLOBALS =======================
String getTimestampString() {
    return String(millis());
}

bool isBootNetworkGraceActive() {
    return millis() < 15000;
}

bool hasActiveWebClients() {
    return events.count() > 0;
}

void notifyClients(String message, const char *sourceTag) {
    if (sourceTag) {
        LOG_PRINTF("[%s] %s\n", sourceTag, message.c_str());
    } else {
        LOG_PRINTLN(message);
    }
}

String getModeString() {
    AppSettings s = settingsManager.getAppSettings();
    if (s.mqtt_isBroker) return "broker";
    if (s.mqtt_isClient) return "client";
    return "off";
}

String normalizeMqttRootTopic(const String &rawTopic) {
    String root = rawTopic;
    root.trim();
    if (root.isEmpty()) {
        root = "fingerprint";
    }
    while (root.endsWith("/")) {
        root.remove(root.length() - 1);
    }
    return root;
}

String normalizeBridgeHostname(const String &rawHostname) {
    String hn = rawHostname;
    hn.trim();
    if (hn.isEmpty()) {
        hn = "MatterMQTTBridge";
    }

    // Leerzeichen für DNS/mDNS vermeiden.
    hn.replace(" ", "-");
    return hn;
}

static String resolveSetupPassword()
{
    AppSettings app = settingsManager.getAppSettings();
    WifiSettings wifi = settingsManager.getWifiSettings();

    String password = app.passwordSetup;
    password.trim();

    if (password.isEmpty()) {
        password = wifi.passwordAdmin;
        password.trim();
    }

    if (password.isEmpty()) {
        password = "admin";
    }

    return password;
}

static bool isPasswordEnabled()
{
    AppSettings app = settingsManager.getAppSettings();
    return app.enablePassword == "on";
}

static void markWebActivity()
{
    lastActivityTime = millis();
}

static bool isWebAccessAllowed()
{
    return (!isPasswordEnabled()) || loggedIn;
}

static bool requireWebLogin(AsyncWebServerRequest *request)
{
    if (isWebAccessAllowed()) {
        markWebActivity();
        return true;
    }

    request->redirect("/login.html");
    return false;
}

static String readEnablePasswordFromRequest(AsyncWebServerRequest *request)
{
    String result = "off";

    const int paramCount = request->params();
    for (int i = 0; i < paramCount; i++) {
        const AsyncWebParameter *p = request->getParam(i);
        if (!p) {
            continue;
        }

        if (p->name() == "enablePassword") {
            if (p->value() == "on") {
                result = "on";
                break;
            }
        }
    }

    return result;
}

String processor(const String &var)
{
    WifiSettings ws = settingsManager.getWifiSettings();
    AppSettings as = settingsManager.getAppSettings();

    if (var == "HOSTNAME") {
        return normalizeBridgeHostname(ws.hostname);
    }

    if (var == "WIFI_SSID") {
        return ws.ssid;
    }

    if (var == "WIFI_PASSWORD") {
        return ws.password.isEmpty() ? "" : "********";
    }

    if (var == "PASSWORD_SETUP_WiFi_CONFIG") {
        String pass = resolveSetupPassword();
        if (pass == "admin") {
            return "admin";
        }
        return "********";
    }

    if (var == "PASSWORD_SETUP") {
        String pass = resolveSetupPassword();
        if (pass == "admin") {
            return "admin";
        }
        return "********";
    }

    if (var == "password_enabled_checked" || var == "PASSWORD_ENABLED_CHECKED") {
        return as.enablePassword == "on" ? "checked" : "";
    }

    return String();
}

String makeTopic(const String &tail) {
    String root = normalizeMqttRootTopic(settingsManager.getAppSettings().mqttRootTopic);
    return root + "/" + tail;
}

static String normalizeWeatherPayload(const String &rawPayload)
{
    String value = rawPayload;
    value.trim();

    const int lastSeparator = value.lastIndexOf(';');
    if (lastSeparator >= 0 && lastSeparator + 1 < (int)value.length()) {
        String candidate = value.substring(lastSeparator + 1);
        candidate.trim();
        if (!candidate.isEmpty()) {
            value = candidate;
        }
    }

    return value;
}

static bool endsWithIgnoreCase(const String &value, const String &suffix)
{
    if (suffix.length() > value.length()) {
        return false;
    }
    return value.substring(value.length() - suffix.length()).equalsIgnoreCase(suffix);
}

static bool isMqttPayloadTrue(const String &rawPayload)
{
    String payload = rawPayload;
    payload.trim();
    payload.toLowerCase();

    return payload == "true" ||
           payload == "1" ||
           payload == "on" ||
           payload.endsWith(";true");
}

static bool parseTriggerOutputPinTopic(const String &topicStr, uint8_t &pin)
{
    const String root = normalizeMqttRootTopic(settingsManager.getAppSettings().mqttRootTopic);
    const String prefix = root + "/TriggerOutputPin";

    if (!topicStr.startsWith(prefix)) {
        return false;
    }

    String pinText = topicStr.substring(prefix.length());
    pinText.trim();

    if (pinText.length() != 1) {
        return false;
    }

    char c = pinText[0];
    if (c < '0' || c > '6') {
        return false;
    }

    pin = (uint8_t)(c - '0');
    return true;
}

static String parseWeatherSourceFromPayload(const String &payloadStr)
{
    const String marker = "source:[";
    const int start = payloadStr.indexOf(marker);
    if (start < 0) {
        return "";
    }

    const int valueStart = start + (int)marker.length();
    const int end = payloadStr.indexOf(']', valueStart);
    if (end < 0) {
        return "";
    }

    String source = payloadStr.substring(valueStart, end);
    source.trim();
    return source;
}

static String parseWeatherSourceFromTopic(const String &topicStr, const String &root)
{
    const String rootPrefix = root + "/";
    if (!topicStr.startsWith(rootPrefix)) {
        return "";
    }

    String relative = topicStr.substring(rootPrefix.length());
    const int slashPos = relative.lastIndexOf('/');
    if (slashPos <= 0) {
        return "";
    }

    String source = relative.substring(0, slashPos);
    source.trim();
    return source;
}

static void updateWeatherValueFromTopic(const String &topicStr, const String &payloadStr, const String &rxSource)
{
    const String root = normalizeMqttRootTopic(settingsManager.getAppSettings().mqttRootTopic);
    if (root.isEmpty()) {
        return;
    }

    const String value = normalizeWeatherPayload(payloadStr);
    if (value.isEmpty()) {
        return;
    }

    const uint32_t nowMs = millis();
    const String rootPrefix = root + "/";
    if (!topicStr.startsWith(rootPrefix)) {
        return;
    }

    String topicLower = topicStr;
    topicLower.toLowerCase();

    String source = parseWeatherSourceFromPayload(payloadStr);
    if (source.isEmpty()) {
        source = parseWeatherSourceFromTopic(topicStr, root);
    }
    if (source.isEmpty()) {
        source = rxSource;
    }

    String sourceLabel = source;
    sourceLabel.trim();
    if (sourceLabel.isEmpty()) {
        sourceLabel = "unbekannt";
    }

    if (endsWithIgnoreCase(topicLower, "/temperature") || endsWithIgnoreCase(topicLower, "/temp")) {
        s_weatherTemperature = value;
        s_weatherTemperatureAtMs = nowMs;
        s_weatherTemperatureSource = sourceLabel;
        s_weatherTemperatureTopic = topicStr;
        return;
    }

    if (endsWithIgnoreCase(topicLower, "/humidity") || endsWithIgnoreCase(topicLower, "/hum")) {
        s_weatherHumidity = value;
        s_weatherHumidityAtMs = nowMs;
        s_weatherHumiditySource = sourceLabel;
        s_weatherHumidityTopic = topicStr;
        return;
    }

    if (endsWithIgnoreCase(topicLower, "/feelslike")) {
        s_weatherFeelsLike = value;
        s_weatherFeelsLikeAtMs = nowMs;
        s_weatherFeelsLikeSource = sourceLabel;
        s_weatherFeelsLikeTopic = topicStr;
        return;
    }

    if (endsWithIgnoreCase(topicLower, "/dewpoint")) {
        s_weatherDewPoint = value;
        s_weatherDewPointAtMs = nowMs;
        s_weatherDewPointSource = sourceLabel;
        s_weatherDewPointTopic = topicStr;
        return;
    }
}

static bool isWeatherFresh(uint32_t updatedAtMs)
{
    if (updatedAtMs == 0) {
        return false;
    }
    return (millis() - updatedAtMs) <= WEATHER_VALUE_FRESH_MS;
}

// ======================= DIAGNOSE-LOG =======================
void addLogMessage(const String& message) {
    LOG_PRINTLN(message);
    s_logBuf[s_logHead] = message;
    s_logHead = (s_logHead + 1) % LOG_BUFFER_SIZE;
    if (s_logCount < LOG_BUFFER_SIZE) s_logCount++;
    events.send(message.c_str(), "log", millis());
}

void setOptionalTestOutputPin(uint8_t pin, bool high);

static void handleTriggerOutputPinForLocalBridge(const String &topicStr, const String &payloadStr, const String &rxSource)
{
    uint8_t pin = 0;
    if (!parseTriggerOutputPinTopic(topicStr, pin)) {
        return;
    }

    const bool high = isMqttPayloadTrue(payloadStr);

    String line = "RX TriggerOutputPin";
    line += String(pin);
    line += high ? " = true" : " = false";

    if (!rxSource.isEmpty()) {
        line += " von ";
        line += rxSource;
    }

    addLogMessage(line);

#if USE_OUTPUT_TEST_PINS
    if (pin >= 1 && pin <= 5) {
        setOptionalTestOutputPin(pin, high);
    }
#endif
}

void handleIncomingMqttMessage(const String &topicStr, const String &payloadStr, const String &rxSource) {
    String line = "RX";
    if (!rxSource.isEmpty()) {
        line += "[" + rxSource + "]";
    }
    line += "  " + topicStr + "  ->  " + payloadStr.substring(0, 80);
    addLogMessage(line);
    updateWeatherValueFromTopic(topicStr, payloadStr, rxSource);
    handleTriggerOutputPinForLocalBridge(topicStr, payloadStr, rxSource);

}

String outputPinStatusTopic(uint8_t pin) {
    return makeTopic("OutputPinStatus" + String(pin));
}

bool publishMqttMessage(const String &topic, const String &message, bool retain, int qos);

static String normalizeBridgeSourceTag(const char* sourceTag) {
    String normalized = sourceTag ? String(sourceTag) : String("");
    normalized.trim();
    if (normalized.isEmpty()) {
        return "BRIDGE";
    }
    if (normalized.startsWith("[") && normalized.endsWith("]") && normalized.length() >= 2) {
        normalized = normalized.substring(1, normalized.length() - 1);
        normalized.trim();
    }
    if (normalized.isEmpty()) {
        return "BRIDGE";
    }
    return normalized;
}

bool bridgeExecuteTrigger(uint8_t pin, const char* sourceTag) {
    if (pin < 1 || pin > 5) {
        addLogMessage("Bridge Befehl abgelehnt: nur OutputPinStatus1..5 erlaubt, pin=" + String(pin));
        return false;
    }

    String normalizedSourceTag = normalizeBridgeSourceTag(sourceTag);
    String outputTopic = outputPinStatusTopic(pin);
    String outputPayload = "source:[" + normalizedSourceTag + "];true";

    addLogMessage("Bridge TX Befehl OutputPinStatus" + String(pin));

    bool sentOutputStatus = publishMqttMessage(outputTopic, outputPayload, false, 0);

    if (sentOutputStatus) {
        addLogMessage("Bridge OutputPinStatus" + String(pin) + " gesendet");
    } else {
        addLogMessage("Bridge Fehler: OutputPinStatus" + String(pin) + " konnte nicht gesendet werden");
    }

    return sentOutputStatus;
}

void setOptionalTestOutputPin(uint8_t pin, bool high) {
#if USE_OUTPUT_TEST_PINS
    static const int testPins[] = {
        TEST_OUTPUT_PIN_1,
        TEST_OUTPUT_PIN_2,
        TEST_OUTPUT_PIN_3,
        TEST_OUTPUT_PIN_4,
        TEST_OUTPUT_PIN_5
    };
    if (pin >= 1 && pin <= 5) {
        digitalWrite(testPins[pin - 1], high ? HIGH : LOW);
        addLogMessage("TestPin" + String(pin) + (high ? " -> HIGH" : " -> LOW"));
    }
#else
    (void)pin;
    (void)high;
#endif
}

// Für Bridge.cpp
bool publishMqttMessage(const String &topic, const String &message, bool retain, int qos) {
#if USE_MQTT_CLIENT
    AppSettings s = settingsManager.getAppSettings();
    if (s.mqtt_isClient) {
        bool ok = mqttManager.publishMqttMessage(topic, message, retain, qos);
        if (ok) addLogMessage("TX  " + topic + "  ->  " + message.substring(0, 80));
        else    addLogMessage("MQTT Publish fehlgeschlagen: " + topic);
        return ok;
    }
#endif

#if USE_MQTT_BROKER
    AppSettings s2 = settingsManager.getAppSettings();
    if (s2.mqtt_isBroker) {
        mqttBroker.publish(topic.c_str(), message.c_str(), retain, qos);
        addLogMessage("TX  " + topic + "  ->  " + message.substring(0, 80));
        return true;
    }
#endif
    LOG_PRINTF("publishMqttMessage drop (neither client nor broker active): %s -> %s\n", topic.c_str(), message.c_str());
    return false;
}

// ======================= SETUP ROUTING =======================
void setupRouting() {
    webServer.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
        String password = "";

        if (request->hasParam("password", true)) {
            password = request->getParam("password", true)->value();
        }

        if (password == resolveSetupPassword()) {
            loggedIn = true;
            markWebActivity();
            request->redirect("/");
            return;
        }

        loggedIn = false;
        request->redirect("/login.html?error=1");
    });

    webServer.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
        loggedIn = false;
        request->redirect("/login.html");
    });

    // API Status
    webServer.on("/api/bridge/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireWebLogin(request)) return;

        AppSettings appSettings = settingsManager.getAppSettings();
        JsonDocument doc;
        doc["ok"] = true;
        doc["mode"] = getModeString();
        doc["root"] = normalizeMqttRootTopic(appSettings.mqttRootTopic);
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["version"] = firmwareVersion;
        String hn = normalizeBridgeHostname(settingsManager.getWifiSettings().hostname);
        doc["hostname"] = hn;
        doc["mdns"] = hn + ".local";

        doc["mqtt"] = "disabled";
        #if USE_MQTT_CLIENT
        if (appSettings.mqtt_isClient) {
            doc["mqtt"] = mqttClient.connected() ? "connected" : "disconnected";
        }
        #endif
        #if USE_MQTT_BROKER
        if (appSettings.mqtt_isBroker) {
            doc["mqtt"] = "broker";
        }
        #endif

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    webServer.on("/api/matter/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireWebLogin(request)) return;

        JsonDocument doc;
        doc["ok"] = true;
        doc["ready"] = matterBridge.isReady();
        doc["pairingCode"] = matterBridge.getPairingCode();
        doc["qrCode"] = matterBridge.getQrCode();

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    webServer.on("/api/matter/pairing/start", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireWebLogin(request)) return;

        bool ok = matterBridge.startPairing();

        JsonDocument doc;
        doc["ok"] = ok;
        doc["ready"] = matterBridge.isReady();
        doc["pairingCode"] = matterBridge.getPairingCode();
        doc["qrCode"] = matterBridge.getQrCode();

        String resp;
        serializeJson(doc, resp);
        request->send(ok ? 200 : 500, "application/json", resp);
    });

    // Settings speichern
    webServer.on("/save_settings", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!isApConfigMode && !requireWebLogin(request)) return;

        WifiSettings ws = settingsManager.getWifiSettings();
        AppSettings as = settingsManager.getAppSettings();

        if (request->hasParam("ssid", true)) ws.ssid = request->getParam("ssid", true)->value();
        if (request->hasParam("password", true) && request->getParam("password", true)->value().length() > 0) {
            ws.password = request->getParam("password", true)->value();
        }
        if (request->hasParam("hostname", true)) ws.hostname = normalizeBridgeHostname(request->getParam("hostname", true)->value());

        if (request->hasParam("passwordSetup", true)) {
            String setupPass = request->getParam("passwordSetup", true)->value();
            setupPass.trim();
            if (!setupPass.isEmpty() && setupPass != "********") {
                as.passwordSetup = setupPass;
                ws.passwordAdmin = setupPass;
            }
        }

        if (request->hasParam("passwordAdmin", true) && request->getParam("passwordAdmin", true)->value().length() > 0) {
            String adminPass = request->getParam("passwordAdmin", true)->value();
            adminPass.trim();
            if (!adminPass.isEmpty() && adminPass != "********") {
                as.passwordSetup = adminPass;
                ws.passwordAdmin = adminPass;
            }
        }

        if (request->hasParam("enablePassword", true)) {
            as.enablePassword = readEnablePasswordFromRequest(request);
        }

        settingsManager.saveWifiSettings(ws);

        if (request->hasParam("mqtt_mode", true)) {
            String mode = request->getParam("mqtt_mode", true)->value();
            if (mode == "off") {
                as.mqtt_isBroker = false;
                as.mqtt_isClient = false;
            } else if (mode == "broker") {
                as.mqtt_isBroker = true;
                as.mqtt_isClient = false;
            } else if (mode == "client") {
                as.mqtt_isBroker = false;
                as.mqtt_isClient = true;
            }
        }
        if (request->hasParam("mqttServer", true)) as.mqttServer = request->getParam("mqttServer", true)->value();
        if (request->hasParam("mqtt_port", true)) as.mqtt_port = request->getParam("mqtt_port", true)->value();
        if (request->hasParam("mqttUsername", true)) as.mqttUsername = request->getParam("mqttUsername", true)->value();
        if (request->hasParam("mqttPassword", true) && request->getParam("mqttPassword", true)->value().length() > 0) {
            as.mqttPassword = request->getParam("mqttPassword", true)->value();
        }
        if (request->hasParam("mqttRootTopic", true)) as.mqttRootTopic = normalizeMqttRootTopic(request->getParam("mqttRootTopic", true)->value());
        
        settingsManager.saveAppSettings(as);

        request->send(200, "text/plain", "Einstellungen gespeichert. Neustart...");
        restartPending = true;
        restartAtMs = millis() + 1000UL;
    });

    // Settings laden
    webServer.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireWebLogin(request)) return;

        JsonDocument doc;
        WifiSettings ws = settingsManager.getWifiSettings();
        AppSettings as = settingsManager.getAppSettings();
        
        doc["ssid"] = ws.ssid;
        doc["hostname"] = normalizeBridgeHostname(ws.hostname);
        doc["mqtt_mode"] = getModeString();
        doc["mqttServer"] = as.mqttServer;
        doc["mqtt_port"] = as.mqtt_port;
        doc["mqttUsername"] = as.mqttUsername;
        doc["mqttRootTopic"] = normalizeMqttRootTopic(as.mqttRootTopic);
        doc["enablePassword"] = as.enablePassword;
        
        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    // API Trigger
    webServer.on("/api/bridge/trigger", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireWebLogin(request)) return;

        if (!request->hasParam("pin")) {
            JsonDocument doc;
            doc["ok"] = false;
            doc["error"] = "missing pin";
            String resp;
            serializeJson(doc, resp);
            request->send(400, "application/json", resp);
            return;
        }

        int pin = request->getParam("pin")->value().toInt();
        if (pin < 1 || pin > 5) {
            JsonDocument doc;
            doc["ok"] = false;
            doc["error"] = "invalid pin";
            String resp;
            serializeJson(doc, resp);
            request->send(400, "application/json", resp);
            return;
        }

        bool sent = bridgeExecuteTrigger((uint8_t)pin, "WEB");
        String outputTopic = outputPinStatusTopic((uint8_t)pin);
        String outputPayload = "source:[WEB];true";
        
        JsonDocument doc;
        if (sent) {
            doc["ok"] = true;
            doc["trigger"] = pin;
            doc["topic"] = outputTopic;
            doc["payload"] = outputPayload;
            doc["outputTopic"] = outputTopic;
            doc["outputPayload"] = outputPayload;
        } else {
            doc["ok"] = false;
            doc["error"] = "output status publish failed";
        }

        String resp;
        serializeJson(doc, resp);
        request->send(sent ? 200 : 500, "application/json", resp);
    });

    webServer.on("/api/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireWebLogin(request)) return;

        request->send(200, "text/plain", "Restarting...");
        restartPending = true;
        restartAtMs = millis() + 1000UL;
    });

    webServer.on("/api/bridge/log", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireWebLogin(request)) return;

        JsonDocument doc;
        JsonArray arr = doc["log"].to<JsonArray>();
        int start = (s_logCount < LOG_BUFFER_SIZE) ? 0 : s_logHead;
        for (int i = 0; i < s_logCount; i++) {
            arr.add(s_logBuf[(start + i) % LOG_BUFFER_SIZE]);
        }
        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    webServer.on("/api/weather/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireWebLogin(request)) return;

        JsonDocument doc;
        doc["ok"] = true;
        doc["temperature"] = s_weatherTemperature;
        doc["humidity"] = s_weatherHumidity;
        doc["feelsLike"] = s_weatherFeelsLike;
        doc["dewPoint"] = s_weatherDewPoint;
        doc["temperatureSource"] = s_weatherTemperatureSource;
        doc["humiditySource"] = s_weatherHumiditySource;
        doc["feelsLikeSource"] = s_weatherFeelsLikeSource;
        doc["dewPointSource"] = s_weatherDewPointSource;
        doc["temperatureTopic"] = s_weatherTemperatureTopic;
        doc["humidityTopic"] = s_weatherHumidityTopic;
        doc["feelsLikeTopic"] = s_weatherFeelsLikeTopic;
        doc["dewPointTopic"] = s_weatherDewPointTopic;

        doc["temperatureFresh"] = isWeatherFresh(s_weatherTemperatureAtMs);
        doc["humidityFresh"] = isWeatherFresh(s_weatherHumidityAtMs);
        doc["feelsLikeFresh"] = isWeatherFresh(s_weatherFeelsLikeAtMs);
        doc["dewPointFresh"] = isWeatherFresh(s_weatherDewPointAtMs);

        doc["freshWindowMs"] = WEATHER_VALUE_FRESH_MS;
        doc["rootTopic"] = normalizeMqttRootTopic(settingsManager.getAppSettings().mqttRootTopic);

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    // Statische Dateien explizit registrieren, damit /api/... nie in den Dateihandler fällt.
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireWebLogin(request)) return;
        request->send(LittleFS, "/index.html", "text/html");
    });
    webServer.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!requireWebLogin(request)) return;
        request->send(LittleFS, "/index.html", "text/html");
    });
    webServer.serveStatic("/icon-bridge.svg", LittleFS, "/icon-bridge.svg");
    webServer.serveStatic("/login.html", LittleFS, "/login.html");
    webServer.serveStatic("/settings.html", LittleFS, "/settings.html");
    webServer.on("/wificonfig.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/wificonfig.html", "text/html", false, processor);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    webServer.addHandler(&events);
    if (isApConfigMode) {
        String otaPassword = resolveSetupPassword();
        ElegantOTA.begin(&webServer, "admin", otaPassword.c_str());
        addLogMessage("OTA im WLAN-Konfigurationsmodus mit Passwortschutz aktiv");
    } else if (isPasswordEnabled()) {
        String otaPassword = resolveSetupPassword();
        ElegantOTA.begin(&webServer, "admin", otaPassword.c_str());
        addLogMessage("OTA mit Passwortschutz aktiv");
    } else {
        ElegantOTA.begin(&webServer);
        addLogMessage("OTA ohne Passwortschutz aktiv");
    }
}

// ======================= SETUP =======================
void setup() {
    Serial.begin(115200);
    delay(500);
    LOG_PRINTLN("\n=== MatterMQTTBridge ===");

    networkMutex = xSemaphoreCreateMutex();

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    if (!LittleFS.begin(false)) {
        LOG_PRINTLN("LittleFS Mount Failed");
        addLogMessage("LittleFS Fehler!");
    } else {
        addLogMessage("LittleFS bereit");
    }

    settingsManager.loadWifiSettings();
    settingsManager.loadAppSettings();
    
    mqttRootTopic = normalizeMqttRootTopic(settingsManager.getAppSettings().mqttRootTopic);

    WifiSettings ws = settingsManager.getWifiSettings();
    ws.hostname = normalizeBridgeHostname(ws.hostname);
    if (ws.ssid.isEmpty()) {
        LOG_PRINTLN("Kein WLAN konfiguriert! Starte als AP...");
        isApConfigMode = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP("MatterBridge-Setup", "12345678");
        digitalWrite(LED_BUILTIN, HIGH);
        addLogMessage("AP Setup gestartet: MatterBridge-Setup");
    } else {
        LOG_PRINTF("Verbinde mit WLAN: %s\n", ws.ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.setHostname(ws.hostname.c_str());
        WiFi.begin(ws.ssid.c_str(), ws.password.c_str());
        uint32_t startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();
        if (WiFi.status() == WL_CONNECTED) {
            LOG_PRINTF("WLAN verbunden! IP: %s\n", WiFi.localIP().toString().c_str());
            addLogMessage("WLAN verbunden: " + WiFi.localIP().toString());

            // mDNS starten – Gerät ist dann unter <hostname>.local erreichbar
            String mdnsName = normalizeBridgeHostname(ws.hostname);
            if (MDNS.begin(mdnsName.c_str())) {
                MDNS.addService("http", "tcp", 80);
                LOG_PRINTF("mDNS gestartet: http://%s.local\n", mdnsName.c_str());
                addLogMessage("mDNS gestartet: " + mdnsName + ".local");
            } else {
                LOG_PRINTLN("mDNS Start fehlgeschlagen.");
            }
        } else {
            LOG_PRINTLN("WLAN Verbindung fehlgeschlagen.");
            addLogMessage("WLAN Verbindung fehlgeschlagen");
        }
    }

    setupRouting();
    ElegantOTA.onStart([]() {
        addLogMessage("OTA gestartet");
    });
    ElegantOTA.onEnd([](bool success) {
        addLogMessage(success ? "OTA beendet" : "OTA Fehler");
    });
    webServer.begin();
    addLogMessage("Webserver gestartet");

    matterBridge.begin();

#if USE_OUTPUT_TEST_PINS
    {
        const int testPins[] = {TEST_OUTPUT_PIN_1, TEST_OUTPUT_PIN_2, TEST_OUTPUT_PIN_3, TEST_OUTPUT_PIN_4, TEST_OUTPUT_PIN_5};
        for (int i = 0; i < 5; i++) {
            pinMode(testPins[i], OUTPUT);
            digitalWrite(testPins[i], LOW);
        }
        addLogMessage("Test-Ausgabepins initialisiert");
    }
#endif

    AppSettings as = settingsManager.getAppSettings();

#if USE_MQTT_BROKER
    if (as.mqtt_isBroker) {
        LOG_PRINTLN("Starte MQTT Broker...");
        mqttBroker.onMessage([](const String& clientId, const String& topic, const String& message) {
            handleIncomingMqttMessage(topic, message, String("broker:") + clientId);
        });
        mqttBroker.begin();
        addLogMessage("MQTT Broker gestartet");
    }
#endif

#if USE_MQTT_CLIENT
    if (as.mqtt_isClient) {
        LOG_PRINTLN("Starte MQTT Client Manager...");
        addLogMessage("MQTT Client startet...");
        mqttClient.onConnect([](bool sessionPresent) {
            mqttManager.onMqttConnect(sessionPresent);
            addLogMessage("MQTT verbunden");
            String root = normalizeMqttRootTopic(settingsManager.getAppSettings().mqttRootTopic);
            String wildcardTopic = root + "/#";
            uint16_t subId = mqttClient.subscribe(wildcardTopic.c_str(), 0);
            if (subId != 0) {
                addLogMessage("MQTT Subscribe aktiv: " + wildcardTopic);
            } else {
                addLogMessage("MQTT Subscribe fehlgeschlagen: " + wildcardTopic);
            }
        });
        mqttClient.onDisconnect([](AsyncMqttClientDisconnectReason reason) {
            mqttManager.onMqttDisconnect(reason);
            addLogMessage("MQTT Verbindung verloren");
        });
        mqttClient.onMessage([](char* topic, char* payload, AsyncMqttClientMessageProperties, size_t len, size_t, size_t) {
            String topicStr(topic);
            String payloadStr(payload, len);
            handleIncomingMqttMessage(topicStr, payloadStr, "client");
        });
        mqttManager.begin();
    }
#endif
}

// ======================= LOOP =======================
void loop() {
    ElegantOTA.loop();
    matterBridge.loop();

#if USE_MQTT_CLIENT
    if (settingsManager.getAppSettings().mqtt_isClient) {
        mqttManager.loop();
    }
#endif

    // Effizienter, unregelmäßiger LED-Herzschlag für sichtbare Loop-Aktivität.
    if (!isApConfigMode)
    {
        static unsigned long nextBlinkTime = 0;
        if (millis() >= nextBlinkTime)
        {
            led1State = !led1State;
            digitalWrite(LED_BUILTIN, led1State);
            nextBlinkTime = millis() + random(200, 1500);
        }
    }

    if (restartPending && millis() >= restartAtMs) {
        digitalWrite(LED_BUILTIN, LOW);
        ESP.restart();
    }

    if (isPasswordEnabled() && loggedIn) {
        if (millis() - lastActivityTime > WEB_LOGIN_TIMEOUT_MS) {
            loggedIn = false;
            addLogMessage("Web-Login automatisch abgelaufen");
        }
    }

    delay(10);
}
