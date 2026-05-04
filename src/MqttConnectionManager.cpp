// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#include "MqttConnectionManager.h"

#if USE_MQTT_CLIENT

#include <Arduino.h>
#include <WiFi.h>

// Helper, um unnötiges Neuparsen der gleichen Zeichenkette zu verhindern
static String lastProcessedServerString = "";
static int lastProcessedMqttPortSetting = -1;

// NEU: Helper, um das Ende eines Strings ohne Berücksichtigung der Groß-/Kleinschreibung zu prüfen
static bool endsWithIgnoreCase(const String &str, const String &suffix)
{
    if (suffix.length() > str.length())
    {
        return false;
    }
    return str.substring(str.length() - suffix.length()).equalsIgnoreCase(suffix);
}

MqttConnectionManager::MqttConnectionManager(AsyncMqttClient &client, SettingsManager &settings)
    : mqttClient(client), settingsManager(settings)
{
}

void MqttConnectionManager::begin()
{
    lastProcessedServerString = "";
    lastProcessedMqttPortSetting = -1;
    // Startverzögerung, um dem System Zeit zum Stabilisieren zu geben
    nextAttemptAt = millis() + 2000;
}

void MqttConnectionManager::loop()
{
    uint32_t now = millis();
    _updateConfig(); // Stellt sicher, dass die Konfiguration aktuell ist

    // Timeout-Logik für laufende Verbindungsversuche
    if (connectInFlight && (now - connectStartedAt > CONNECT_TIMEOUT_MS))
    {
        LOG_PRINTF("[CLIENT] CONNECT TIMEOUT elapsed_ms=%lu\n", (unsigned long)(now - connectStartedAt));
        connectInFlight = false;
        mqttClient.disconnect(true); // Führt zu onMqttDisconnect
    }

    // Startet einen neuen Verbindungsversuch, wenn die Zeit reif ist
    if (config.mode != MqttConnectionMode::INVALID && !mqttClient.connected() && !connectInFlight && now >= nextAttemptAt)
    {
        _startConnectionAttempt();
    }
}

void MqttConnectionManager::onMqttConnect(bool sessionPresent)
{
    LOG_PRINTF("[CLIENT] ✅ CONNECTED keepAlive=%u sessionPresent=%d\n", _currentKeepAliveSec, sessionPresent);
    LOG_PRINTF("[CLIENT] ✅ Server: %s:%u (via: %s)\n", resolvedIp.toString().c_str(), config.port, lastConnectionVia.c_str());
    // "Birth" (Online) sofort veröffentlichen, retained + QoS1
    publishMqttMessage(makeTopic("status"), "online", /*retain*/ true, /*qos*/ 1);
    LOG_PRINTLN("[CLIENT] ✅ Status 'online' veröffentlicht - Client ist bereit!");
    connectInFlight = false;
    lastResolveAttemptAt = 0;
    nextAttemptAt = 0; // Kein erneuter Versuch nötig
    _consecutiveFailedAttempts = 0;
}

void MqttConnectionManager::onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    LOG_PRINTF("[CLIENT] ❌ DISCONNECTED reason=%d next_retry_ms=%u\n", (int)reason, RETRY_INTERVAL_MS);
    LOG_PRINTF("[CLIENT] ❌ Failed attempts: %u\n", _consecutiveFailedAttempts);
    connectInFlight = false;
    nextAttemptAt = millis() + RETRY_INTERVAL_MS;
    if (WiFi.isConnected())
    {
        _consecutiveFailedAttempts++;
    }
}

void MqttConnectionManager::onWifiConnect()
{
    LOG_PRINTLN("[MQTT] reconnect delayed after WiFi reconnect");
    const uint32_t delayedAttemptAt = millis() + WIFI_RECONNECT_DELAY_MS;
    if (nextAttemptAt == 0 || nextAttemptAt > delayedAttemptAt)
    {
        nextAttemptAt = delayedAttemptAt;
    }
    // Bei WiFi-Reconnect: gecachte IP invalidieren, um mDNS-Discovery zu erzwingen
    // Dies stellt sicher, dass nach Broker-Neustart die mDNS-Announcement erkannt wird
    haveIp = false;
    resolvedIp = INADDR_NONE;
    lastConnectionVia = "";
    lastProcessedServerString = "";
    lastProcessedMqttPortSetting = -1; // Konfiguration neu einlesen
    lastResolveAttemptAt = 0;
}

void MqttConnectionManager::_startConnectionAttempt()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        LOG_PRINTLN("[CLIENT] ⚠️ WiFi nicht verbunden - Versuch verschoben");
        nextAttemptAt = millis() + RETRY_INTERVAL_MS;
        return;
    }

    if (_consecutiveFailedAttempts >= 3)
    {
        LOG_PRINTF("[CLIENT] 🔄 Re-resolving after %u failed attempts.\n", _consecutiveFailedAttempts);
        haveIp = false;
        _consecutiveFailedAttempts = 0;
    }

    LOG_PRINTF("[CLIENT] 🚀 Starting connection attempt (mode=%d, server=%s:%u)\n",
               (int)config.mode, config.hostOrService.c_str(), config.port);
    connectInFlight = true;
    connectStartedAt = millis();
    nextAttemptAt = millis() + RETRY_INTERVAL_MS; // Nächster Versuch, falls dieser fehlschlägt

    // Wenn die IP bereits bekannt ist (z.B. aus vorherigem Versuch oder weil es eine IP war), direkt verbinden.
    // Ansonsten, die Auflösung einmalig anstoßen.
    if (haveIp)
    {
        _connectNow();
    }
    else
    {
        uint32_t now = millis();
        if ((now - lastResolveAttemptAt) < RESOLVE_RETRY_INTERVAL_MS)
        {
            LOG_PRINTLN("[CLIENT] Resolve gedrosselt - warte vor erneutem DNS/mDNS-Versuch.");
            connectInFlight = false;
            nextAttemptAt = now + RETRY_INTERVAL_MS;
            return;
        }

        lastResolveAttemptAt = now;
        _resolveAndConnect();
    }
}

void MqttConnectionManager::_resolveAndConnect()
{
    bool success = false;
    const char *via = ""; // Verwende const char* für Effizienz

    LOG_PRINTLN("[CLIENT] 🔄 Resolving according to configured mode...");

    switch (config.mode)
    {
    case MqttConnectionMode::IP_ADDRESS:
        // Bereits in _parseServerString aufgelöst und in this->resolvedIp gespeichert
        success = true;
        via = "ip";
        LOG_PRINTF("[CLIENT] Using pre-resolved IP address: %s:%u\n", this->resolvedIp.toString().c_str(), config.port);
        break;

    case MqttConnectionMode::DNS_HOSTNAME:
    {
        String host = config.hostOrService;
        LOG_PRINTF("[CLIENT] 🔍 Attempting DNS resolution for: %s\n", host.c_str());
        if (WiFi.hostByName(host.c_str(), this->resolvedIp) == 1)
        {
            success = true;
            via = "dns";
            LOG_PRINTF("[CLIENT] ✅ DNS resolution successful: %s -> %s:%u\n", host.c_str(), this->resolvedIp.toString().c_str(), config.port);
        }
        else
        {
            LOG_PRINTF("[CLIENT] ❌ DNS resolution failed for: %s\n", host.c_str());
        }
        break;
    }

    case MqttConnectionMode::MDNS_DISCOVERY:
    {
        LOG_PRINTF("[CLIENT] 🔍 Attempting mDNS discovery for service 'mqtt', host '%s'\n", config.hostOrService.c_str());
        int n = MDNS.queryService("mqtt", "tcp");
        if (n > 0)
        {
            LOG_PRINTF("[CLIENT] mDNS query returned %d service(s).\n", n);
            bool useFirstAvailable = (config.hostOrService == "*");

            if (useFirstAvailable)
            {
                this->resolvedIp = MDNS.IP(0);
                this->config.port = MDNS.port(0); // Port ebenfalls aktualisieren
                success = true;
                via = "mdns-auto";
                LOG_PRINTF("[CLIENT] ✅ mDNS Auto-Discovery: Using first available service '%s' (%s:%u)\n",
                           MDNS.hostname(0).c_str(), this->resolvedIp.toString().c_str(), this->config.port);
            }
            else
            {
                String targetHost = config.hostOrService;
                // .local Suffix für den Vergleich entfernen, da MDNS.hostname() es nicht enthält
                if (endsWithIgnoreCase(targetHost, ".local"))
                {
                    targetHost.remove(targetHost.length() - 6);
                }

                for (int i = 0; i < n; i++)
                {
                    String discoveredHost = MDNS.hostname(i);
                    LOG_PRINTF("[CLIENT] mDNS Check: Service %d, host='%s'\n", i, discoveredHost.c_str());
                    if (discoveredHost.equalsIgnoreCase(targetHost))
                    {
                        this->resolvedIp = MDNS.IP(i);
                        this->config.port = MDNS.port(i); // Port ebenfalls aktualisieren
                        success = true;
                        via = "mdns-specific";
                        LOG_PRINTF("[CLIENT] ✅ mDNS Matched: service '%s' with IP %s:%u\n",
                                   discoveredHost.c_str(), this->resolvedIp.toString().c_str(), this->config.port);
                        break; // Schleife verlassen, da der Host gefunden wurde
                    }
                }
            }
        }

        // Fallback zu DNS, wenn mDNS fehlschlägt, ABER NICHT für .local-Adressen, Wildcard '*' oder expliziten,mdns-Hint
        String lowerCaseServerString = lastProcessedServerString;
        lowerCaseServerString.toLowerCase();
        bool explicitMdnsOnly = lowerCaseServerString.indexOf(",mdns") != -1;

        if (!success && !explicitMdnsOnly && !endsWithIgnoreCase(config.hostOrService, ".local") && config.hostOrService != "*")
        {
            LOG_PRINTF("[CLIENT] ⚠️ mDNS failed for '%s', falling back to DNS.\n", config.hostOrService.c_str());
            String host = config.hostOrService;

            if (WiFi.hostByName(host.c_str(), this->resolvedIp) == 1)
            {
                success = true;
                via = "dns-fallback";
                LOG_PRINTF("[CLIENT] ✅ DNS-Fallback success for host %s -> %s\n", host.c_str(), this->resolvedIp.toString().c_str());
            }
            else
            {
                LOG_PRINTF("[CLIENT] ❌ DNS-Fallback FAILED for host %s\n", host.c_str());
            }
        }
        else if (!success)
        {
            LOG_PRINTF("[CLIENT] ❌ mDNS failed for '%s'. No DNS fallback for this type.\n", config.hostOrService.c_str());
        }
        break;
    }

    default:
        LOG_PRINTLN("[CLIENT] ❌ Invalid connection mode.");
        break; // MqttConnectionMode::INVALID
    }

    // ========== SCHRITT 3: FINALER FALLBACK - MDNS AUTO-DISCOVERY ==========
    bool allowFinalMdnsFallback = (config.mode == MqttConnectionMode::MDNS_DISCOVERY);

    if (!success && allowFinalMdnsFallback)
    {
        LOG_PRINTLN("[CLIENT] ⚠️ Configured mDNS method failed. Final attempt: mDNS Auto-Discovery for any broker...");
        int n = MDNS.queryService("mqtt", "tcp");
        if (n > 0)
        {
            this->resolvedIp = MDNS.IP(0);
            this->config.port = MDNS.port(0);
            success = true;
            via = "mdns-fallback-auto";
            LOG_PRINTF("[CLIENT] ✅ mDNS Auto-Discovery Fallback successful: Found '%s' (%s:%u)\n",
                       MDNS.hostname(0).c_str(), this->resolvedIp.toString().c_str(), this->config.port);
        }
        else
        {
            LOG_PRINTLN("[CLIENT] ❌ mDNS Auto-Discovery Fallback failed. No services found.");
        }
    }

    // ========== ERGEBNIS VERARBEITEN ==========
    if (success)
    {
        haveIp = true;
        this->lastConnectionVia = String(via);
        LOG_PRINTF("[CLIENT] ✅ RESOLVED server=%s ip=%s port=%u via=%s\n",
                   config.hostOrService.c_str(), this->resolvedIp.toString().c_str(), config.port, via);
        _connectNow();
    }
    else
    {
        LOG_PRINTF("[CLIENT] ❌ RESOLVE FAILED for %s (all methods failed)\n", config.hostOrService.c_str());
        connectInFlight = false;
    }
}

void MqttConnectionManager::_connectNow()
{
    LOG_PRINTF("[CLIENT] TRY CONNECT target=%s:%u\n", resolvedIp.toString().c_str(), config.port);

    IPAddress localIp = WiFi.localIP();
    if (localIp == IPAddress() || localIp[0] == 0)
    {
        LOG_PRINTLN("[CLIENT] WiFi-IP noch nicht verfügbar. Verbindung wird verschoben.");
        connectInFlight = false;
        nextAttemptAt = millis() + NETWORK_STABILIZE_DELAY_MS;
        return;
    }

    mqttClient.setServer(resolvedIp, config.port);
    const AppSettings &appSettings = settingsManager.getAppSettings();
    int keepAlive = appSettings.mqtt_keepAlive.toInt();
    if (keepAlive <= 0)
    {
        keepAlive = 45; // Fallback auf 45 Sekunden (robuster bei zickigem Netz)
    }
    _currentKeepAliveSec = keepAlive;
    mqttClient.setKeepAlive(_currentKeepAliveSec);

    mqttUsername = appSettings.mqttUsername;
    mqttUsername.trim();
    mqttPassword = appSettings.mqttPassword;
    mqttPassword.trim();

    if (!mqttUsername.isEmpty())
    {
        mqttClient.setCredentials(mqttUsername.c_str(), mqttPassword.c_str());
    }
    else
    {
        mqttClient.setCredentials(nullptr, nullptr);
    }

    // Last-Will nach konsistentem Schema <root>/<mode>/<host>/status
    mqttLastWillTopic = makeTopic("status");
    mqttClient.setWill(mqttLastWillTopic.c_str(), 1, true, "offline");

    String hostname = settingsManager.getWifiSettings().hostname;
    // Stabile, nicht-leere Client-ID sicherstellen
    mqttClientId = hostname;
    mqttClientId.trim();
    if (mqttClientId.isEmpty())
    {
        mqttClientId = "esp32-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);
    }
    mqttClient.setClientId(mqttClientId.c_str());

    mqttClient.connect();
}

void MqttConnectionManager::_updateConfig()
{
    const AppSettings &appSettings = settingsManager.getAppSettings();
    String currentServerString = appSettings.mqttServer;
    int configuredPort = appSettings.mqtt_port.toInt();
    if (configuredPort <= 0 || configuredPort > 65535)
    {
        configuredPort = 1883;
    }

    if (currentServerString != lastProcessedServerString || configuredPort != lastProcessedMqttPortSetting)
    {
        _parseServerString(currentServerString, configuredPort);
    }
}

void MqttConnectionManager::_parseServerString(const String &serverConfig, int fallbackPort)
{
    bool attemptsWereDisabled = (nextAttemptAt == 0xFFFFFFFF);

    lastProcessedServerString = serverConfig;
    lastProcessedMqttPortSetting = fallbackPort;

    // Zustand zurücksetzen
    config = {}; // Setzt auf Standardwerte (INVALID, etc.)
    haveIp = false;
    resolvedIp = INADDR_NONE;
    lastConnectionVia = "";

    String server = serverConfig;
    server.trim();
    LOG_PRINTF("[CLIENT] 🔧 Parsing server config: '%s' (port: %d)\n", server.c_str(), fallbackPort);

    if (server.isEmpty() || server.equalsIgnoreCase("dummy"))
    {
        LOG_PRINTLN("[CLIENT] ⚠️ Server config INVALID (empty or dummy) - Client disabled");
        config.mode = MqttConnectionMode::INVALID;
        nextAttemptAt = 0xFFFFFFFF; // Versuche deaktivieren
        return;
    }

    String primaryPart = server;
    bool mdnsHint = false;

    int commaPos = server.indexOf(',');
    if (commaPos != -1)
    {
        primaryPart = server.substring(0, commaPos);
        String hintsPart = server.substring(commaPos + 1);
        hintsPart.trim();
        if (hintsPart.equalsIgnoreCase("mdns"))
        {
            mdnsHint = true;
        }
    }
    primaryPart.trim();

    // Port extrahieren, falls vorhanden
    int colonPos = primaryPart.indexOf(':');
    if (colonPos != -1)
    {
        config.hostOrService = primaryPart.substring(0, colonPos);
        String portStr = primaryPart.substring(colonPos + 1);
        int parsedPort = portStr.toInt();
        if (parsedPort > 0 && parsedPort <= 65535)
        {
            config.port = parsedPort;
        }
    }
    else
    {
        config.hostOrService = primaryPart;
    }
    config.hostOrService.trim();

    // Fallback auf Port aus den globalen Einstellungen
    if (config.port == 0)
    {
        config.port = fallbackPort;
    }

    // Modus bestimmen
    if (mdnsHint)
    {
        config.mode = MqttConnectionMode::MDNS_DISCOVERY;
        config.hostOrService.toLowerCase(); // Erzwinge Kleinschreibung für Konsistenz
        LOG_PRINTF("[CLIENT] 📡 Mode: MDNS_DISCOVERY (hint) - service: %s\n", config.hostOrService.c_str());
    }
    else if (endsWithIgnoreCase(config.hostOrService, ".local"))
    {
        config.mode = MqttConnectionMode::MDNS_DISCOVERY;
        config.hostOrService.toLowerCase(); // .local-Adressen immer in Kleinschreibung behandeln
        LOG_PRINTF("[CLIENT] 📡 Mode: MDNS_DISCOVERY (.local) - host: %s\n", config.hostOrService.c_str());
    }
    else
    {
        IPAddress tempIp;
        if (tempIp.fromString(config.hostOrService))
        {
            resolvedIp = tempIp;
            config.mode = MqttConnectionMode::IP_ADDRESS;
            haveIp = true; // IP ist bereits bekannt
            LOG_PRINTF("[CLIENT] 🌐 Mode: IP_ADDRESS - IP: %s:%u\n", resolvedIp.toString().c_str(), config.port);
        }
        else
        {
            config.mode = MqttConnectionMode::DNS_HOSTNAME;
            // DNS-Hostnamen NICHT verändern, um Case-Sensitivity-Probleme bei TLS/SNI zu vermeiden.
            LOG_PRINTF("[CLIENT] 🌐 Mode: DNS_HOSTNAME - host: %s:%u\n", config.hostOrService.c_str(), config.port);
        }
    }

    if (config.mode != MqttConnectionMode::INVALID)
    {
        if (attemptsWereDisabled)
        {
            LOG_PRINTLN("[CLIENT] ✅ Client mode enabled - starting connection attempts");
            nextAttemptAt = millis();
        }
    }
}

bool MqttConnectionManager::publishMqttMessage(const String &topic, const String &payload, bool retain, int qos)
{
    uint16_t packetId = mqttClient.publish(topic.c_str(), qos, retain, payload.c_str());
    return packetId != 0;
}

#endif // USE_MQTT_CLIENT
