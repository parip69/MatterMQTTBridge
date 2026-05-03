// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#pragma once

// Compile-time Default: USE_MQTT_CLIENT
#ifndef USE_MQTT_CLIENT
#define USE_MQTT_CLIENT 1
#endif

#if !USE_MQTT_CLIENT
// ====================================================================
// Stub-Klasse: Wird verwendet, wenn MQTT-Client NICHT kompiliert wird.
// Alle Methoden sind leer – der Compiler optimiert sie vollständig weg.
// ====================================================================
#include <Arduino.h>

enum class MqttConnectionMode { INVALID, IP_ADDRESS, DNS_HOSTNAME, MDNS_DISCOVERY };
struct ParsedServerConfig {
    MqttConnectionMode mode = MqttConnectionMode::INVALID;
    String hostOrService;
    uint16_t port = 0;
};

class AsyncMqttClient; // Forward-Deklaration (wird nie verwendet)
class SettingsManager;

class MqttConnectionManager
{
public:
    MqttConnectionManager() {}
    void begin() {}
    void loop() {}
    void onMqttConnect(bool) {}
    void onMqttDisconnect(int) {}
    void onWifiConnect() {}
    String getLastConnectionVia() const { return String(); }
    void publishMqttMessage(const String &, const String &, bool, int) {}
};

#else // USE_MQTT_CLIENT == 1

#include <Arduino.h>
#include <AsyncMqttClient.h>
#include <ESPmDNS.h>
#include "SettingsManager.h"
#include "global.h"

// NEU: Enum für den Verbindungsmodus
enum class MqttConnectionMode
{
    INVALID,
    IP_ADDRESS,
    DNS_HOSTNAME,
    MDNS_DISCOVERY
};

// NEU: Struct für die geparste Server-Konfiguration
struct ParsedServerConfig
{
    MqttConnectionMode mode = MqttConnectionMode::INVALID;
    String hostOrService; // Hostname oder mDNS Service-Name (ohne ._tcp.local)
    uint16_t port = 0;
};

class MqttConnectionManager
{
public:
    MqttConnectionManager(AsyncMqttClient &client, SettingsManager &settings);
    void begin();
    void loop();

    // Callbacks, die von main.cpp aufgerufen werden
    void onMqttConnect(bool sessionPresent);
    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
    void onWifiConnect();

    // Getter für die letzte Verbindungsart
    String getLastConnectionVia() const { return lastConnectionVia; }

    // MQTT-Publish-Hilfsfunktion
    void publishMqttMessage(const String &topic, const String &payload, bool retain, int qos);

private:
    // Referenzen
    AsyncMqttClient &mqttClient;
    SettingsManager &settingsManager;

    // Zustandsmaschine
    bool connectInFlight = false;
    uint32_t nextAttemptAt = 0;
    uint32_t connectStartedAt = 0;
    uint32_t lastResolveAttemptAt = 0;

    // Ziel-Konfiguration und aufgelöste IP
    ParsedServerConfig config;
    IPAddress resolvedIp;
    bool haveIp = false;
    String lastConnectionVia; // Speichert die letzte Verbindungsart (mdns, dns, ip, dns-fallback)

    // Timing-Konstanten
    static const uint32_t RETRY_INTERVAL_MS = 5000;         // 5s Intervall für Wiederverbindungen
    static const uint32_t RESOLVE_RETRY_INTERVAL_MS = 15000;
    static const uint32_t CONNECT_TIMEOUT_MS = 8000;        // 8s Verbindungs-Timeout
    static const uint32_t NETWORK_STABILIZE_DELAY_MS = 750; // Warten, bevor bei nicht bereitem WLAN erneut versucht wird
    static const uint32_t WIFI_RECONNECT_DELAY_MS = 3000;   // Ruhe nach WLAN-Reconnect

    // Puffer, die die Lebensdauer von AsyncMqttClient::connect() überdauern müssen
    String mqttUsername;
    String mqttPassword;
    String mqttLastWillTopic;
    String mqttClientId;
    uint16_t _currentKeepAliveSec = 45;

    // Private Methoden
    void _updateConfig();
    void _parseServerString(const String &server, int fallbackPort);
    void _startConnectionAttempt();
    void _resolveAndConnect();
    void _connectNow();
    uint8_t _consecutiveFailedAttempts = 0;
};

#endif // USE_MQTT_CLIENT
