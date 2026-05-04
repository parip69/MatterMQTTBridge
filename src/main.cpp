// Beschreibung: 2.0.0 erste wo alles get mit dem Templade dowenload
//******************************************************
//         Main of MatterMQTTBridge.
// nur hier die start wert der version Aendern.OK=======
// @version: 1.0.12 <br> Builddatum 20:16:45 04-05.2026
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

// ======================= TEST-AUSGABE-PINS (optional) =======================
#define USE_OUTPUT_TEST_PINS 0   // auf 1 setzen um TestPins zu aktivieren
#define TEST_OUTPUT_PIN_1 25
#define TEST_OUTPUT_PIN_2 26
#define TEST_OUTPUT_PIN_3 27
#define TEST_OUTPUT_PIN_4 32
#define TEST_OUTPUT_PIN_5 33

// ======================= GLOBALS =======================
const char* firmwareVersion = "1.0.12 <br> Builddatum 20:16:45 04-05.2026";
AsyncWebServer webServer(80);
SettingsManager settingsManager;

String mqttRootTopic = "fingerprint";
SemaphoreHandle_t networkMutex = nullptr;
volatile bool timeSet = false;
int led1State = LOW;
bool isApConfigMode = false;

static bool restartPending = false;
static uint32_t restartAtMs = 0;

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

String makeTopic(const String &tail) {
    String root = normalizeMqttRootTopic(settingsManager.getAppSettings().mqttRootTopic);
    return root + "/" + tail;
}

// ======================= DIAGNOSE-LOG =======================
void addLogMessage(const String& message) {
    LOG_PRINTLN(message);
    s_logBuf[s_logHead] = message;
    s_logHead = (s_logHead + 1) % LOG_BUFFER_SIZE;
    if (s_logCount < LOG_BUFFER_SIZE) s_logCount++;
    events.send(message.c_str(), "log", millis());
}

void handleIncomingMqttMessage(const String &topicStr, const String &payloadStr, const String &rxSource) {
    String line = "RX";
    if (!rxSource.isEmpty()) {
        line += "[" + rxSource + "]";
    }
    line += "  " + topicStr + "  ->  " + payloadStr.substring(0, 80);
    addLogMessage(line);

#if USE_OUTPUT_TEST_PINS
    String root = normalizeMqttRootTopic(settingsManager.getAppSettings().mqttRootTopic);
    for (int i = 1; i <= 5; i++) {
        if (topicStr == root + "/OutputPinStatus" + String(i)) {
            String p = payloadStr;
            p.trim();
            p.toLowerCase();
            bool hi = (p == "true" || p == "1" || p == "on" || p.endsWith(";true"));
            setOptionalTestOutputPin((uint8_t)i, hi);
            break;
        }
    }
#endif
}

String outputPinStatusTopic(uint8_t pin) {
    return makeTopic("OutputPinStatus" + String(pin));
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

        addLogMessage("Aktion " + String(pin) + " angefordert");
        bool sentTrigger = bridgeSendTrigger(pin);
        bool sentOutputStatus = true;

        String outputTopic = "";
        String outputPayload = "";
        if (pin >= 1 && pin <= 5) {
            outputTopic = outputPinStatusTopic((uint8_t)pin);
            outputPayload = "source:[WEB];true";
            sentOutputStatus = publishMqttMessage(outputTopic, outputPayload, false, 0);
            if (sentOutputStatus) {
                setOptionalTestOutputPin((uint8_t)pin, true);
            }
        }

        bool sent = sentTrigger && sentOutputStatus;
        
        JsonDocument doc;
        if (sent) {
            doc["ok"] = true;
            doc["trigger"] = pin;
            doc["topic"] = bridgeTriggerTopic();
            doc["payload"] = bridgeTriggerPayload(pin);
            if (pin >= 1 && pin <= 5) {
                doc["outputTopic"] = outputTopic;
                doc["outputPayload"] = outputPayload;
            }
        } else {
            doc["ok"] = false;
            if (!sentTrigger) {
                doc["error"] = "trigger publish failed";
            } else if (!sentOutputStatus) {
                doc["error"] = "output status publish failed";
            } else {
                doc["error"] = "mqtt unavailable";
            }
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

    webServer.on("/api/bridge/log", HTTP_GET, [](AsyncWebServerRequest *request) {
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
