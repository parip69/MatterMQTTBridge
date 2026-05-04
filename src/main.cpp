// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
//******************************************************
//         Main of Fingerscanner Parip69.
// nur hier die start wert der version Aendern.OK=======
// @version: 2.2.790 <br> Builddatum 06:09:05 04-05.2026
//****************************************************

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include "SettingsManager.h"
#include "Bridge.h"
#include "global.h"

#if USE_MQTT_CLIENT
#include <AsyncMqttClient.h>
#endif

#if USE_MQTT_BROKER
#include "ESPAsyncMQTTBroker.h"
#endif

AsyncWebServer webServer(80);
SettingsManager settingsManager;
String mqttRootTopic = "fingerprint";

#if USE_MQTT_CLIENT
AsyncMqttClient mqttClient;
bool mqttClientConnected = false;
#endif

#if USE_MQTT_BROKER
ESPAsyncMQTTBroker* mqttBroker = nullptr;
bool mqttBrokerRunning = false;
#endif

String getMqttModeString() {
    AppSettings app = settingsManager.getAppSettings();
    if (app.mqtt_isBroker) return "broker";
    if (app.mqtt_isClient) return "client";
    return "off";
}

String getMqttStatusString() {
    AppSettings app = settingsManager.getAppSettings();
    if (app.mqtt_isBroker) {
#if USE_MQTT_BROKER
        return mqttBrokerRunning ? "connected" : "offline";
#else
        return "offline";
#endif
    }
    if (app.mqtt_isClient) {
#if USE_MQTT_CLIENT
        return mqttClientConnected ? "connected" : "offline";
#else
        return "offline";
#endif
    }
    return "aus";
}

void initLittleFS() {
    if (!LittleFS.begin(false)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
}

void initWifi() {
    WifiSettings wifi = settingsManager.getWifiSettings();
    if (wifi.ssid.length() == 0) return;
    
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(wifi.hostname.c_str());
    WiFi.begin(wifi.ssid.c_str(), wifi.password.c_str());
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    Serial.println("");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }
}

void publishMqttMessage(const String &topic, const String &message, bool retain = false, int qos = 0) {
    AppSettings app = settingsManager.getAppSettings();
    if (app.mqtt_isBroker) {
#if USE_MQTT_BROKER
        if (mqttBrokerRunning && mqttBroker) {
            mqttBroker->publish(topic.c_str(), message.c_str(), qos, retain);
        }
#endif
    } else if (app.mqtt_isClient) {
#if USE_MQTT_CLIENT
        if (mqttClientConnected) {
            mqttClient.publish(topic.c_str(), qos, retain, message.c_str());
        }
#endif
    }
}

void initMqtt() {
    AppSettings app = settingsManager.getAppSettings();
    if (app.mqtt_isBroker) {
#if USE_MQTT_BROKER
        mqttBroker = new ESPAsyncMQTTBroker();
        mqttBrokerRunning = mqttBroker->begin(app.mqtt_port.toInt());
#endif
    } else if (app.mqtt_isClient) {
#if USE_MQTT_CLIENT
        mqttClient.setServer(app.mqttServer.c_str(), app.mqtt_port.toInt());
        if (app.mqttUsername.length() > 0) {
            mqttClient.setCredentials(app.mqttUsername.c_str(), app.mqttPassword.c_str());
        }
        mqttClient.onConnect([](bool sessionPresent) { mqttClientConnected = true; });
        mqttClient.onDisconnect([](AsyncMqttClientDisconnectReason reason) { mqttClientConnected = false; });
        mqttClient.connect();
#endif
    }
}

void registerBridgeApi() {
    webServer.on("/api/bridge/status", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{";
        json += "\"ok\": true,";
        json += "\"mode\": \"" + getMqttModeString() + "\",";
        json += "\"mqtt\": \"" + getMqttStatusString() + "\",";
        json += "\"root\": \"" + mqttRootTopic + "\",";
        json += "\"ip\": \"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\": " + String(WiFi.RSSI()) + ",";
        json += "\"version\": \"MatterMQTTBridge\"";
        json += "}";
        request->send(200, "application/json", json);
    });

    webServer.on("/api/bridge/trigger", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("pin")) {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"missing pin\"}");
            return;
        }
        int pin = request->getParam("pin")->value().toInt();
        if (pin < 1 || pin > 99) {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid pin\"}");
            return;
        }
        if (getMqttStatusString() != "connected") {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"mqtt unavailable\"}");
            return;
        }
        bool ok = bridgeSendTrigger(pin);
        if (!ok) {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid pin\"}");
            return;
        }
        String json = "{";
        json += "\"ok\": true,";
        json += "\"trigger\": " + String(pin) + ",";
        json += "\"topic\": \"" + bridgeTriggerTopic() + "\",";
        json += "\"payload\": \"" + bridgeTriggerPayload(pin) + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });
}

void registerWebRoutes() {
    webServer.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
}

void initWeb() {
    registerBridgeApi();
    registerWebRoutes();
    
#ifdef ELEGANTOTA_USE_ASYNC_WEBSERVER
    ElegantOTA.begin(&webServer);
#endif
    webServer.begin();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    initLittleFS();
    settingsManager.loadWifiSettings();
    settingsManager.loadAppSettings();
    mqttRootTopic = settingsManager.getAppSettings().mqttRootTopic;
    if(mqttRootTopic.isEmpty()) mqttRootTopic = "fingerprint";
    
    initWifi();
    initMqtt();
    initWeb();
}

void loop() {
#ifdef ELEGANTOTA_USE_ASYNC_WEBSERVER
    ElegantOTA.loop();
#endif
}
