// Beschreibung: 2.0.0 erste wo alles get mit dem Templade dowenload
//******************************************************
//         Main of Fingerscanner Parip69.
// nur hier die start wert der version Aendern.OK=======
// @version: 2.2.795 <br> Builddatum 06:38:22 04-05.2026
//****************************************************

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>

#include "SettingsManager.h"
#include "Bridge.h"
#include "global.h"

#if USE_MQTT_CLIENT
#include <AsyncMqttClient.h>
#include "MqttConnectionManager.h"
#endif

#if USE_MQTT_BROKER
#include "ESPAsyncMQTTBroker.h"
#endif

// ======================= GLOBALS =======================
AsyncWebServer webServer(80);
SettingsManager settingsManager;

String mqttRootTopic = "fingerprint";
SemaphoreHandle_t networkMutex = xSemaphoreCreateMutex();
volatile bool timeSet = false;

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
    return true; 
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

String makeTopic(const String &tail) {
    String root = settingsManager.getAppSettings().mqttRootTopic;
    if (root.endsWith("/")) root = root.substring(0, root.length() - 1);
    return root + "/" + tail;
}

// Für Bridge.cpp
void publishMqttMessage(const String &topic, const String &message, bool retain, int qos) {
#if USE_MQTT_CLIENT
    AppSettings s = settingsManager.getAppSettings();
    if (s.mqtt_isClient) {
        mqttManager.publishMqttMessage(topic, message, retain, qos);
        return;
    }
#endif

#if USE_MQTT_BROKER
    AppSettings s2 = settingsManager.getAppSettings();
    if (s2.mqtt_isBroker) {
        mqttBroker.publish(topic.c_str(), message.c_str(), retain, qos);
        return;
    }
#endif
    LOG_PRINTF("publishMqttMessage drop (neither client nor broker active): %s -> %s\n", topic.c_str(), message.c_str());
}

// ======================= SETUP ROUTING =======================
void setupRouting() {
    // Statische Dateien
    webServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // API Status
    webServer.on("/api/bridge/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["ok"] = true;
        doc["mode"] = getModeString();
        doc["root"] = settingsManager.getAppSettings().mqttRootTopic;
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["version"] = "2.2.790";

        bool mqttConnected = false;
        #if USE_MQTT_CLIENT
        if(settingsManager.getAppSettings().mqtt_isClient) mqttConnected = mqttClient.connected();
        #endif
        #if USE_MQTT_BROKER
        if(settingsManager.getAppSettings().mqtt_isBroker) mqttConnected = true; // broker is always "connected" locally
        #endif
        doc["mqtt"] = mqttConnected ? "connected" : "disconnected";

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    // API Trigger
    webServer.on("/api/bridge/trigger", HTTP_GET, [](AsyncWebServerRequest *request) {
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
        if (pin < 1 || pin > 99) {
            JsonDocument doc;
            doc["ok"] = false;
            doc["error"] = "invalid pin";
            String resp;
            serializeJson(doc, resp);
            request->send(400, "application/json", resp);
            return;
        }

        bool sent = bridgeSendTrigger(pin);
        
        JsonDocument doc;
        if (sent) {
            doc["ok"] = true;
            doc["trigger"] = pin;
            doc["topic"] = bridgeTriggerTopic();
            doc["payload"] = bridgeTriggerPayload(pin);
        } else {
            doc["ok"] = false;
            doc["error"] = "mqtt unavailable";
        }

        String resp;
        serializeJson(doc, resp);
        request->send(sent ? 200 : 500, "application/json", resp);
    });

    webServer.addHandler(&events);
    ElegantOTA.begin(&webServer);
}

// ======================= SETUP =======================
void setup() {
    Serial.begin(115200);
    delay(500);
    LOG_PRINTLN("\n=== MatterMQTTBridge ===");

    if (!LittleFS.begin(false)) {
        LOG_PRINTLN("LittleFS Mount Failed");
    }

    settingsManager.loadWifiSettings();
    settingsManager.loadAppSettings();
    mqttRootTopic = settingsManager.getAppSettings().mqttRootTopic;

    WifiSettings ws = settingsManager.getWifiSettings();
    if (ws.ssid.isEmpty()) {
        LOG_PRINTLN("Kein WLAN konfiguriert! Starte als AP...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("MatterBridge-Setup", "12345678");
    } else {
        LOG_PRINTF("Verbinde mit WLAN: %s\n", ws.ssid.c_str());
        WiFi.mode(WIFI_STA);
        if(!ws.hostname.isEmpty()) {
            WiFi.setHostname(ws.hostname.c_str());
        }
        WiFi.begin(ws.ssid.c_str(), ws.password.c_str());
        uint32_t startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
            delay(500);
            Serial.print(".");
        }
        Serial.println();
        if (WiFi.status() == WL_CONNECTED) {
            LOG_PRINTF("WLAN verbunden! IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
            LOG_PRINTLN("WLAN Verbindung fehlgeschlagen.");
        }
    }

    setupRouting();
    webServer.begin();

    AppSettings as = settingsManager.getAppSettings();

#if USE_MQTT_BROKER
    if (as.mqtt_isBroker) {
        LOG_PRINTLN("Starte MQTT Broker...");
        mqttBroker.begin();
    }
#endif

#if USE_MQTT_CLIENT
    if (as.mqtt_isClient) {
        LOG_PRINTLN("Starte MQTT Client Manager...");
        mqttManager.begin();
    }
#endif
}

// ======================= LOOP =======================
void loop() {
    ElegantOTA.loop();

#if USE_MQTT_CLIENT
    if (settingsManager.getAppSettings().mqtt_isClient) {
        mqttManager.loop();
    }
#endif

    delay(10);
}
