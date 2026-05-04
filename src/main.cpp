// Beschreibung: 2.0.0 erste wo alles get mit dem Templade dowenload
//******************************************************
//         Main of MatterMQTTBridge.
// nur hier die start wert der version Aendern.OK=======
// @version: 1.0.4 <br> Builddatum 19:32:35 04-05.2026
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
#include "global.h"

#if USE_MQTT_CLIENT
#include <AsyncMqttClient.h>
#include "MqttConnectionManager.h"
#endif

#if USE_MQTT_BROKER
#include "ESPAsyncMQTTBroker.h"
#endif

// ======================= GLOBALS =======================
const char* firmwareVersion = "1.0.4 <br> Builddatum 19:32:35 04-05.2026";
AsyncWebServer webServer(80);
SettingsManager settingsManager;

String mqttRootTopic = "fingerprint";
SemaphoreHandle_t networkMutex = nullptr;
volatile bool timeSet = false;
int led1State = LOW;
bool isApConfigMode = false;

static bool restartPending = false;
static uint32_t restartAtMs = 0;

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

String makeTopic(const String &tail) {
    String root = normalizeMqttRootTopic(settingsManager.getAppSettings().mqttRootTopic);
    return root + "/" + tail;
}

// ======================= MQTT MONITOR =======================
void mqttLogEvent(const String &dir, const String &topic, const String &payload) {
    JsonDocument doc;
    doc["dir"] = dir;
    doc["topic"] = topic;
    doc["payload"] = payload.substring(0, 200);
    String msg;
    serializeJson(doc, msg);
    events.send(msg.c_str(), "mqtt_log", millis());
}

// Für Bridge.cpp
bool publishMqttMessage(const String &topic, const String &message, bool retain, int qos) {
#if USE_MQTT_CLIENT
    AppSettings s = settingsManager.getAppSettings();
    if (s.mqtt_isClient) {
        bool ok = mqttManager.publishMqttMessage(topic, message, retain, qos);
        if (ok) mqttLogEvent("TX", topic, message);
        return ok;
    }
#endif

#if USE_MQTT_BROKER
    AppSettings s2 = settingsManager.getAppSettings();
    if (s2.mqtt_isBroker) {
        mqttBroker.publish(topic.c_str(), message.c_str(), retain, qos);
        mqttLogEvent("TX", topic, message);
        return true;
    }
#endif
    LOG_PRINTF("publishMqttMessage drop (neither client nor broker active): %s -> %s\n", topic.c_str(), message.c_str());
    return false;
}

// ======================= SETUP ROUTING =======================
void setupRouting() {
    // API Status
    webServer.on("/api/bridge/status", HTTP_GET, [](AsyncWebServerRequest *request) {
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

    // Settings speichern
    webServer.on("/save_settings", HTTP_POST, [](AsyncWebServerRequest *request) {
        WifiSettings ws = settingsManager.getWifiSettings();
        if (request->hasParam("ssid", true)) ws.ssid = request->getParam("ssid", true)->value();
        if (request->hasParam("password", true) && request->getParam("password", true)->value().length() > 0) {
            ws.password = request->getParam("password", true)->value();
        }
        if (request->hasParam("hostname", true)) ws.hostname = normalizeBridgeHostname(request->getParam("hostname", true)->value());
        if (request->hasParam("passwordAdmin", true) && request->getParam("passwordAdmin", true)->value().length() > 0) {
            ws.passwordAdmin = request->getParam("passwordAdmin", true)->value();
        }
        settingsManager.saveWifiSettings(ws);

        AppSettings as = settingsManager.getAppSettings();
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

    webServer.on("/api/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Restarting...");
        restartPending = true;
        restartAtMs = millis() + 1000UL;
    });

    // Statische Dateien explizit registrieren, damit /api/... nie in den Dateihandler fällt.
    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });
    webServer.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });
    webServer.serveStatic("/bootstrap.min.css", LittleFS, "/bootstrap.min.css");
    webServer.serveStatic("/icon-192.png", LittleFS, "/icon-192.png");
    webServer.serveStatic("/login.html", LittleFS, "/login.html");
    webServer.serveStatic("/settings.html", LittleFS, "/settings.html");
    webServer.serveStatic("/wificonfig.html", LittleFS, "/wificonfig.html");

    webServer.addHandler(&events);
    ElegantOTA.begin(&webServer);
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

            // mDNS starten – Gerät ist dann unter <hostname>.local erreichbar
            String mdnsName = normalizeBridgeHostname(ws.hostname);
            if (MDNS.begin(mdnsName.c_str())) {
                MDNS.addService("http", "tcp", 80);
                LOG_PRINTF("mDNS gestartet: http://%s.local\n", mdnsName.c_str());
            } else {
                LOG_PRINTLN("mDNS Start fehlgeschlagen.");
            }
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
        mqttClient.onConnect([](bool sessionPresent) {
            mqttManager.onMqttConnect(sessionPresent);
            mqttLogEvent("SYS", "status", "connected");
        });
        mqttClient.onDisconnect([](AsyncMqttClientDisconnectReason reason) {
            mqttManager.onMqttDisconnect(reason);
            mqttLogEvent("SYS", "status", "disconnected");
        });
        mqttClient.onMessage([](char* topic, char* payload, AsyncMqttClientMessageProperties, size_t len, size_t, size_t) {
            mqttLogEvent("RX", String(topic), String(payload, len));
        });
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

    delay(10);
}
