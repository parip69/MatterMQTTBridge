// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉

#if USE_TELEGRAM

#include "Telegram.h"
#include "global.h"
#include "SettingsManager.h"
#if USE_TELEGRAM_CMD
#include <ArduinoJson.h>
#if USE_DHT
#include "DHTManager.h"
#endif
#endif
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <time.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

extern SettingsManager settingsManager;
extern bool shouldReboot;
extern bool otaInProgress;

// Diagnose-Logging (gekapselt, identisches Format wie main.cpp)
#ifndef DIAG_LOG_ENABLED
#define DIAG_LOG_ENABLED 0
#endif
#if DIAG_LOG_ENABLED
#define TG_DIAG_PRINTF(tag, fmt, ...) \
	do { Serial.printf("[%-4s +%06lu ms] " fmt "\n", (tag), (unsigned long)millis(), ##__VA_ARGS__); } while (0)
#else
#define TG_DIAG_PRINTF(tag, fmt, ...)
#endif

static String htmlEscape(const String &input)
{
    String out;
    out.reserve(input.length() + 8);
    for (size_t i = 0; i < input.length(); ++i)
    {
        char c = input[i];
        switch (c)
        {
        case '&':
            out += F("&amp;");
            break;
        case '<':
            out += F("&lt;");
            break;
        case '>':
            out += F("&gt;");
            break;
        case '"':
            out += F("&quot;");
            break;
        case '\'':
            out += F("&#39;");
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

String buildTelegramModuleHtml()
{
    AppSettings settings = settingsManager.getAppSettings();
    String html;
    html.reserve(1200);
    html += F("<div class='settings-section' id='telegram-section'>");
    html += F("<form class='form-horizontal' action='/module/telegram/save' method='post' onsubmit='return closeEventSourceBeforeSubmit(this);'>");
    html += F("  <fieldset>");
    html += F("    <legend class='legend-cursor' onclick=\"sectionToggleCollapse('telegram-settings')\">Telegram Setup</legend>");
    html += F("    <div id='telegram-settings-content' class='collapsed-content'>");
    html += F("      <div class='form-group'>");
    html += F("        <label class='col-md-4 control-label' for='telegram_enabled'>Telegram aktivieren</label>");
    html += F("        <div class='col-md-4'>");
    // Hidden-Fallback: wenn die Checkbox nicht angehakt ist, sendet der Browser kein Feld.
    // Damit der Server trotzdem einen eindeutigen Wert bekommt, senden wir zusätzlich telegram_enabled_value=0.
    html += F("          <input type='hidden' name='telegram_enabled_value' value='0'>");
    html += F("          <input id='telegram_enabled' name='telegram_enabled' type='checkbox' class='form-check-input'");
    if (settings.telegram_enabled)
        html += F(" checked");
    // Wenn angehakt, überschreibt dieses Feld per HTML-Form-Serialisierung den hidden-Wert (in der Praxis kommen beide an).
    // Unser Server-Handler behandelt beides robust.
    html += F(" value='1'>");
    html += F("        </div>");
    html += F("      </div>");

    // NUR wenn aktiviert: restliche Felder anzeigen
    if (settings.telegram_enabled)
    {
        html += F("      <div class='form-group'>");
        html += F("        <label class='col-md-4 control-label' for='telegram_botToken'>Bot Token</label>");
        html += F("        <div class='col-md-4'>");
        html += F("          <input id='telegram_botToken' name='telegram_botToken' type='text' placeholder='Bot Token' class='form-control input-md' value='");
        html += htmlEscape(settings.telegram_botToken);
        html += F("'>");
        html += F("        </div>");
        html += F("      </div>");
        html += F("      <div class='form-group'>");
        html += F("        <label class='col-md-4 control-label' for='telegram_chatId'>Chat ID</label>");
        html += F("        <div class='col-md-4'>");
        html += F("          <input id='telegram_chatId' name='telegram_chatId' type='text' placeholder='Chat ID' class='form-control input-md' value='");
        html += htmlEscape(settings.telegram_chatId);
        html += F("'>");
        html += F("        </div>");
        html += F("      </div>");
#if USE_TELEGRAM_CMD
        html += F("      <div class='form-group'>");
        html += F("        <label class='col-md-4 control-label' for='telegram_cmdEnabled'>Telegram-Befehle aktivieren</label>");
        html += F("        <div class='col-md-4'>");
        html += F("          <input type='hidden' name='telegram_cmdEnabled_value' value='0'>");
        html += F("          <input id='telegram_cmdEnabled' name='telegram_cmdEnabled' type='checkbox' class='form-check-input'");
        if (settings.telegram_cmdEnabled)
            html += F(" checked");
        html += F(" value='1'>");
        html += F("        </div>");
        html += F("      </div>");
        html += F("      <div class='form-group'>");
        html += F("        <label class='col-md-4 control-label' for='telegram_pollIntervalMs'>Telegram Abfrageintervall (s)</label>");
        html += F("        <div class='col-md-4'>");
        html += F("          <input id='telegram_pollIntervalMs' name='telegram_pollIntervalMs' type='number' min='3' max='120' step='1' class='form-control input-md' value='");
        html += String(settings.telegram_pollIntervalMs / 1000);
        html += F("'>");
        html += F("        </div>");
        html += F("      </div>");
#endif
        html += F("      <div class='form-group'>");
        html += F("        <label class='col-md-4 control-label' for='telegram_customMsg'>Test-Nachricht</label>");
        html += F("        <div class='col-md-4'>");
        html += F("          <input id='telegram_customMsg' name='telegram_customMsg' type='text' placeholder='Das ist eine Testnachricht von Ihrem ESP32 Fingerprint Sensor. Es spielt 32 ich telegramm.' class='form-control input-md'>");
        html += F("        </div>");
        html += F("      </div>");
        html += F("      <div class='form-group'>");
        html += F("        <label class='col-md-4 control-label'></label>");
        html += F("        <div class='col-md-8 actions'>");
        html += F("          <button type='submit' class='btn btn-success' name='btnSaveSettings' value='telegram' style='margin-right: 10px;'>Speichern</button>");
        html += F("          <a href=\"javascript:fetch('/api/telegram/test?msg='+encodeURIComponent(document.getElementById('telegram_customMsg').value.trim().length > 0 ? document.getElementById('telegram_customMsg').value : 'Das ist eine Testnachricht von Ihrem ESP32 Fingerprint Sensor. Es spielt 32 ich telegramm.')).catch(e=>alert('Fehler: '+e))\" class='btn btn-primary'>Test senden</a>");
        html += F("        </div>");
        html += F("      </div>");
    }
    else
    {
        // Wenn deaktiviert: nur Speichern-Button
        html += F("      <div class='form-group'>");
        html += F("        <label class='col-md-4 control-label'></label>");
        html += F("        <div class='col-md-8 actions'>");
        html += F("          <button type='submit' class='btn btn-success' name='btnSaveSettings' value='telegram'>Speichern</button>");
        html += F("        </div>");
        html += F("      </div>");
    }

    html += F("    </div>");
    html += F("  </fieldset>");
    html += F("</form>");
    html += F("</div>");
    return html;
}

// FreeRTOS Queue and Task handles
static QueueHandle_t telegramQueue = NULL;
static TaskHandle_t telegramTaskHandle = NULL;

// Maximale Anzahl der Nachrichten in der Queue und maximale Nachrichtenlänge
#define TELEGRAM_QUEUE_LENGTH 10
#define TELEGRAM_MAX_MSG_LENGTH 256

#if USE_TELEGRAM_CMD
#define TELEGRAM_FAST_POLL_INTERVAL_MS 3000UL

static unsigned long telegramLastPollMs = 0;
static long telegramLastUpdateId = 0;
static bool telegramCommandsRegistered = false;
static unsigned long telegramFastPollUntilMs = 0;
static bool telegramInitialUpdateSyncDone = false;

static uint32_t telegramGetEffectivePollIntervalMs()
{
    const unsigned long now = millis();
    if ((long)(telegramFastPollUntilMs - now) > 0)
    {
        return TELEGRAM_FAST_POLL_INTERVAL_MS;
    }
    return settingsManager.getAppSettings().telegram_pollIntervalMs;
}

static bool telegramIsCommandFeatureActive(const AppSettings &settings)
{
    return settings.telegram_enabled &&
           settings.telegram_cmdEnabled &&
           !settings.telegram_botToken.isEmpty() &&
           !settings.telegram_chatId.isEmpty();
}
#endif

static String urlEncode(const String &str)
{
    String encoded = "";
    for (char c : str)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (isalnum(uc) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += c;
        }
        else if (c == ' ')
        {
            encoded += "%20";
        }
        else
        {
            char buf[4];
            sprintf(buf, "%%%02X", uc);
            encoded += buf;
        }
    }
    return encoded;
}

#if USE_TELEGRAM_CMD

static String telegramEscapeText(const String &s)
{
    String out = s;
    out.replace("\r", "");
    return out;
}

static String telegramBuildHelpText()
{
    String msg;
    msg.reserve(120);
    msg += F("Verfuegbare Befehle:\n");
    msg += F("/start - Hilfe anzeigen\n");
    msg += F("/wetter - DHT-Uebersicht\n");
    msg += F("/status - Systemstatus\n");
    msg += F("/reboot - Neustart ausloesen");
    return msg;
}

static String telegramBuildStatusText()
{
    const AppSettings &settings = settingsManager.getAppSettings();
    const WifiSettings &wifi = settingsManager.getWifiSettings();

    String msg;
    msg.reserve(180);
    msg += wifi.hostname.length() ? wifi.hostname : String(F("ESP32"));
    msg += F("\nWLAN: ");
    msg += (WiFi.status() == WL_CONNECTED) ? F("verbunden") : F("getrennt");
    msg += F("\nTelegram: ");
    msg += settings.telegram_enabled ? F("aktiv") : F("aus");

#if USE_DHT
    msg += F("\nDHT: ");
    msg += dhtManager.isSensorConnected() ? F("verbunden") : F("nicht verbunden");
#else
    msg += F("\nDHT: nicht mitkompiliert");
#endif

    return msg;
}

static String telegramBuildWeatherText()
{
#if USE_DHT
    if (!dhtManager.isSensorConnected())
    {
        return F("DHT-Sensor aktuell nicht verbunden.");
    }

    String msg;
    msg.reserve(160);
    msg += F("Wetter-Daten (DHT-Sensor)\n");
    msg += F("Temperatur: ");
    msg += dhtManager.getTemperature();
    msg += F(" C\nLuftfeuchtigkeit: ");
    msg += dhtManager.getHumidity();
    msg += F(" %\nGefuehlte Temp.: ");
    msg += dhtManager.getHeatIndex();
    msg += F(" C\nTaupunkt: ");
    msg += dhtManager.getDewPoint();
    msg += F(" C");
    return msg;
#else
    return F("DHT ist nicht mitkompiliert.");
#endif
}

static String telegramHandleCommandText(const String &rawText)
{
    String text = rawText;
    text.trim();
    text.toLowerCase();

    const int botPos = text.indexOf('@');
    if (botPos > 0)
    {
        text = text.substring(0, botPos);
    }

    if (text == "/start")
        return telegramBuildHelpText();
    if (text == "/wetter")
        return telegramBuildWeatherText();
    if (text == "/status")
        return telegramBuildStatusText();
    if (text == "/reboot")
    {
        shouldReboot = true;
        return F("Neustart wurde eingeleitet.");
    }

    if (text.startsWith("/"))
        return F("Unbekannter Befehl. Bitte /start senden.");

    return String();
}

static void telegramRegisterCommandsIfNeeded()
{
    if (telegramCommandsRegistered)
        return;
    if (otaInProgress)
        return;

    const AppSettings &settings = settingsManager.getAppSettings();
    if (!telegramIsCommandFeatureActive(settings) || WiFi.status() != WL_CONNECTED)
        return;

    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();

    String url = "https://api.telegram.org/bot" + settings.telegram_botToken + "/setMyCommands";
    if (!http.begin(client, url))
    {
        return;
    }

    http.setTimeout(5000);
    http.addHeader("Content-Type", "application/json");

    String body = F("{\"commands\":[");
    body += F("{\"command\":\"start\",\"description\":\"Hilfe anzeigen\"},");
    body += F("{\"command\":\"wetter\",\"description\":\"DHT-Uebersicht\"},");
    body += F("{\"command\":\"status\",\"description\":\"Systemstatus\"},");
    body += F("{\"command\":\"reboot\",\"description\":\"Neustart ausloesen\"}");
    body += F("]}");

    const int httpCode = http.POST(body);
    if (httpCode > 0 && httpCode == HTTP_CODE_OK)
    {
        telegramCommandsRegistered = true;
        LOG_PRINTLN("[Telegram] Befehlsmenue registriert.");
    }
    http.end();
}

static void telegramPollUpdates()
{
    if (otaInProgress)
        return;

    const AppSettings &settings = settingsManager.getAppSettings();
    if (!telegramIsCommandFeatureActive(settings) || WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    const unsigned long now = millis();
    if ((now - telegramLastPollMs) < telegramGetEffectivePollIntervalMs())
    {
        return;
    }
    // Netzwerk-Pause-Ampel: Poll überspringen wenn WLAN/HTTP gerade beschäftigt
    if (isTelegramNetworkPauseActive())
    {
        TG_DIAG_PRINTF("TG  ", "poll skipped: network busy");
        return; // telegramLastPollMs unveraendert: naechster Versuch nach normalem Intervall
    }
    telegramLastPollMs = now;

    LOG_PRINTLN("[TG-Poll] getUpdates...");

    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();

    String url = "https://api.telegram.org/bot" + settings.telegram_botToken + "/getUpdates?timeout=0&limit=3";
    if (telegramLastUpdateId > 0)
    {
        url += "&offset=" + String(telegramLastUpdateId + 1);
    }

    if (!http.begin(client, url))
    {
        LOG_PRINTLN("[TG-Poll] http.begin fehlgeschlagen!");
        return;
    }

    http.setTimeout(5000);
    const int httpCode = http.GET();
    if (httpCode <= 0 || httpCode != HTTP_CODE_OK)
    {
        LOG_PRINTLN("[TG-Poll] HTTP-Fehler: " + String(httpCode));
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    LOG_PRINTLN("[TG-Poll] Payload: " + payload.substring(0, 150));

    JsonDocument doc;
    if (deserializeJson(doc, payload))
    {
        LOG_PRINTLN("[TG-Poll] JSON-Parse-Fehler!");
        return;
    }

    JsonArray results = doc["result"].as<JsonArray>();
    if (results.size() > 0) { LOG_PRINTLN("[TG-Poll] Updates: " + String(results.size())); }

    if (!telegramInitialUpdateSyncDone)
    {
        telegramInitialUpdateSyncDone = true;
        if (results.size() > 0)
        {
            LOG_PRINTLN("[TG-Poll] Initiale Alt-Updates verworfen.");
        }
        return;
    }

    const int64_t nowEpoch = (int64_t)time(nullptr);
    const int64_t staleWindowSec = (int64_t)(settings.telegram_pollIntervalMs / 1000UL);
    bool hasLatestCommand = false;
    long latestCommandDate = -1;
    String latestCommandText;

    for (JsonObject update : results)
    {
        const long updateId = update["update_id"] | 0;
        if (updateId > telegramLastUpdateId)
        {
            telegramLastUpdateId = updateId;
        }

        JsonObject message = update["message"];
        if (message.isNull())
            continue;

        String chatId = String((long long)(message["chat"]["id"] | 0));
        LOG_PRINTLN("[TG-Poll] chatId=" + chatId + " erwartet=" + settings.telegram_chatId);
        if (chatId != settings.telegram_chatId)
            continue;

        String text = message["text"] | "";
        if (text.isEmpty())
            continue;

        const long messageDate = message["date"] | 0;
        if (nowEpoch > 1000000000LL && messageDate > 0)
        {
            int64_t ageSec = nowEpoch - (int64_t)messageDate;
            if (ageSec < 0)
            {
                ageSec = 0;
            }
            if (ageSec > staleWindowSec)
            {
                LOG_PRINTLN("[TG-Poll] Verworfen (zu alt): " + text);
                continue;
            }
        }

        if (!hasLatestCommand || messageDate >= latestCommandDate)
        {
            hasLatestCommand = true;
            latestCommandDate = messageDate;
            latestCommandText = text;
        }
    }

    if (!hasLatestCommand)
    {
        return;
    }

    LOG_PRINTLN("[TG-Poll] Befehl: " + latestCommandText);
    telegramFastPollUntilMs = millis() + settings.telegram_pollIntervalMs;

    String answer = telegramHandleCommandText(latestCommandText);
    if (!answer.isEmpty())
    {
        // Antwort direkt mit dem vorhandenen client/http senden (kein neuer TLS-Buffer!)
        vTaskDelay(pdMS_TO_TICKS(200));
        String sendUrl = "https://api.telegram.org/bot" + settings.telegram_botToken +
                         "/sendMessage?chat_id=" + settings.telegram_chatId +
                         "&text=" + urlEncode(telegramEscapeText(answer));
        bool sent = false;
        if (http.begin(client, sendUrl))
        {
            http.setTimeout(8000);
            const int sendCode = http.GET();
            sent = (sendCode > 0 && sendCode == HTTP_CODE_OK);
            if (!sent)
            {
                LOG_PRINTLN("[TG-Poll] Send-Fehler: " + String(sendCode));
            }
            http.end();
        }
        else
        {
            LOG_PRINTLN("[TG-Poll] send http.begin fehlgeschlagen!");
        }
        LOG_PRINTLN(sent ? "[TG-Poll] Antwort gesendet." : "[TG-Poll] Antwort FEHLGESCHLAGEN!");
    }
}

#endif // USE_TELEGRAM_CMD

// Persistenter Worker-Task für den Versand von Telegram-Nachrichten
void telegramWorkerTask(void *pvParameters)
{
    char message[TELEGRAM_MAX_MSG_LENGTH];
    WiFiClientSecure client;
    HTTPClient http;

    for (;;)
    {
        // Auf eine Nachricht in der Queue warten
#if USE_TELEGRAM_CMD
        const AppSettings workerSettings = settingsManager.getAppSettings();
        const bool telegramCmdActive = telegramIsCommandFeatureActive(workerSettings);
        const TickType_t waitTicks = telegramCmdActive ? pdMS_TO_TICKS(TELEGRAM_FAST_POLL_INTERVAL_MS) : portMAX_DELAY;
        if (xQueueReceive(telegramQueue, &message, waitTicks) == pdPASS)
#else
        if (xQueueReceive(telegramQueue, &message, portMAX_DELAY) == pdPASS)
#endif
        {
            if (otaInProgress)
            {
                LOG_PRINTLN("[Telegram Task] >> Nachricht waehrend OTA verworfen.");
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            const AppSettings &settings = settingsManager.getAppSettings();
            if (WiFi.status() == WL_CONNECTED && settings.telegram_enabled && !settings.telegram_botToken.isEmpty() && !settings.telegram_chatId.isEmpty())
            {
                LOG_PRINTLN("[Telegram Task] >> Nachricht aus Queue erhalten, sende: " + String(message));

                // Pause-Prüfung: Netzwerk gerade beschäftigt (WLAN-Scan/Roaming oder großer HTTP-Transfer)?
                if (isTelegramNetworkPauseActive())
                {
                    TG_DIAG_PRINTF("TG  ", "send delayed: network busy");
                    // Nachricht nicht verlieren: zurück in Queue legen und kurz warten
                    if (xQueueSendToFront(telegramQueue, &message, 0) == pdPASS)
                    {
                        vTaskDelay(pdMS_TO_TICKS(2000)); // 2s warten, dann erneut versuchen
                        continue;
                    }
                    // Queue voll: trotzdem versuchen zu senden (Nachricht nicht verwerfen)
                }

                // Verzögerung um Race Conditions zu vermeiden (NICHT unter networkMutex halten)
                vTaskDelay(pdMS_TO_TICKS(100));

                client.setInsecure(); // Zertifikatsprüfung umgehen

                String url = "https://api.telegram.org/bot" + settings.telegram_botToken +
                             "/sendMessage?chat_id=" + settings.telegram_chatId +
                             "&text=" + urlEncode(String(message));

                // F3: networkMutex NICHT über die gesamte HTTPS-Transaktion halten.
                // Der Task ist bereits über die Queue serialisiert.
                // Langes Lock (bis zu 5s Timeout) blockierte unnötig MQTT-Publishes.
                {
                    // Try-Catch-ähnliches Pattern für defensive Programmierung
                    bool sendSuccess = false;

                    if (http.begin(client, url))
                    {
                        http.setTimeout(5000);
                        int httpCode = http.GET();
                        if (httpCode > 0 && httpCode != HTTP_CODE_OK)
                        {
                            LOG_PRINTLN("[Telegram Task] Fehler: HTTP-Code " + String(httpCode));
                        }
                        else if (httpCode <= 0)
                        {
                            LOG_PRINTLN("[Telegram Task] Fehler: Senden fehlgeschlagen: " + http.errorToString(httpCode));
                        }
                        else
                        {
                            sendSuccess = true;
                        }
                        http.end();
                    }
                    else
                    {
                        LOG_PRINTLN("[Telegram Task] Fehler: HTTP-Verbindung konnte nicht hergestellt werden.");
                    }

                    if (sendSuccess)
                    {
                        LOG_PRINTLN("[Telegram Task] ✅ Nachricht erfolgreich gesendet.");
                    }
                }
            }
            else
            {
                LOG_PRINTLN("[Telegram Task] >> Nachricht verworfen (WiFi/Einstellungen nicht bereit).");
            }
            // Kurze Pause zur Stabilisierung und um API-Limits zu vermeiden
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
#if USE_TELEGRAM_CMD
        if (!telegramCmdActive)
        {
            continue;
        }
        const AppSettings pollSettings = settingsManager.getAppSettings();
        if (!telegramIsCommandFeatureActive(pollSettings))
        {
            continue;
        }
        telegramRegisterCommandsIfNeeded();
        telegramPollUpdates();
        vTaskDelay(pdMS_TO_TICKS(50));
#endif
    }
}

void telegram_init()
{
    telegramQueue = xQueueCreate(TELEGRAM_QUEUE_LENGTH, TELEGRAM_MAX_MSG_LENGTH);

    if (telegramQueue == NULL)
    {
        LOG_PRINTLN("[Telegram] FEHLER: Queue konnte nicht erstellt werden.");
        return;
    }

    BaseType_t result = xTaskCreate(
        telegramWorkerTask,
        "TelegramWorker",
        12288, // 12KB Stack für HTTPS (erhöht von 8KB wegen Stack-Overflow-Risiko)
        NULL,
        1, // Priorität (von 5 auf 1 gesenkt: TLS/HTTPS soll Webserver/WLAN nicht verdrängen)
        &telegramTaskHandle);

    if (result != pdPASS)
    {
        LOG_PRINTLN("[Telegram] FEHLER: Worker-Task konnte nicht erstellt werden.");
        vQueueDelete(telegramQueue);
        telegramQueue = NULL;
        return;
    }

#if USE_TELEGRAM_CMD
    telegramLastPollMs = 0;
    telegramLastUpdateId = 0;
    telegramCommandsRegistered = false;
    telegramFastPollUntilMs = 0;
    telegramInitialUpdateSyncDone = false;
#endif
    LOG_PRINTLN("[Telegram] ✅ Worker-Task und Queue erfolgreich initialisiert.");
}

void telegram_loop()
{
    // Empfang und Versand laufen bewusst im vorhandenen Worker-Task,
    // damit kein zweiter Task noetig ist.
}

void sendTelegramMessage(const String &message)
{
    const AppSettings &settings = settingsManager.getAppSettings();
    if (!settings.telegram_enabled || settings.telegram_botToken.isEmpty() || settings.telegram_chatId.isEmpty() || telegramQueue == NULL)
    {
        return;
    }

    String hostname = settingsManager.getWifiSettings().hostname;
    String fullMessage = hostname + ":  " + message;

    if (fullMessage.length() >= TELEGRAM_MAX_MSG_LENGTH)
    {
        LOG_PRINTLN("[Telegram] FEHLER: Nachricht zu lang, wird gekürzt.");
        fullMessage = fullMessage.substring(0, TELEGRAM_MAX_MSG_LENGTH - 4) + "...";
    }

    char messageBuffer[TELEGRAM_MAX_MSG_LENGTH];
    strncpy(messageBuffer, fullMessage.c_str(), TELEGRAM_MAX_MSG_LENGTH);
    messageBuffer[TELEGRAM_MAX_MSG_LENGTH - 1] = '\0';

    if (xQueueSend(telegramQueue, &messageBuffer, (TickType_t)0) != pdPASS)
    {
        LOG_PRINTLN("[Telegram] WARNUNG: Queue ist voll. Nachricht verworfen.");
    }
    else
    {
        LOG_PRINTLN("[Telegram] >> Nachricht zur Queue hinzugefügt.");
    }
}

void registerTelegramEndpoints(AsyncWebServer &server)
{
    // Telegram-Test-API: sendet eine Testnachricht (keine Browser-Bestätigung)
    server.on("/api/telegram/test", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String customMsg = "";
        if (request->hasParam("msg")) {
            customMsg = request->getParam("msg")->value();
        }
        if (customMsg.length() > 0) {
            LOG_PRINTLN("[Telegram API] Test-Anfrage erhalten: " + customMsg);
            notifyClients(customMsg + " via notify.");
            sendTelegramMessage(customMsg + " via Test Button.");
        }
        request->send(200, "text/plain", ""); });

    server.on("/ui/telegram", HTTP_GET, [](AsyncWebServerRequest *req)
              {
        String html = buildTelegramModuleHtml();
        AsyncWebServerResponse* res = req->beginResponse(200, "text/html; charset=utf-8", html);
        res->addHeader("Cache-Control", "no-store");
        req->send(res); });

    server.on("/module/telegram/save", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        AppSettings settings = settingsManager.getAppSettings();

        LOG_PRINTLN("[Telegram] /module/telegram/save hit, method=" + String(request->method()) + ", params=" + String(request->params()));
        for (int i = 0; i < request->params(); i++)
        {
            const AsyncWebParameter* p = request->getParam(i);
            if (!p) continue;
            LOG_PRINTLN(String("[Telegram] Param ") + p->name() + "=" + p->value() + " (isPost=" + String(p->isPost()) + ")");
        }

        auto getBody = [&](const char* name) -> String {
            if (request->hasParam(name, true))
                return request->getParam(name, true)->value();
            return String();
        };

        // Checkbox robust: entweder klassisch als Flag oder als Fallback-Feld (0/1)
        bool enableTelegram = false;
        if (request->hasParam("telegram_enabled", true))
        {
            enableTelegram = true;
        }
        else
        {
            String v = getBody("telegram_enabled_value");
            enableTelegram = (v == "1" || v == "on" || v == "true");
        }
        settings.telegram_enabled = enableTelegram;

        if (enableTelegram)
        {
            String token = getBody("telegram_botToken");
            if (!token.isEmpty()) settings.telegram_botToken = token;

            String chatId = getBody("telegram_chatId");
            if (!chatId.isEmpty()) settings.telegram_chatId = chatId;

#if USE_TELEGRAM_CMD
            bool enableTelegramCmd = false;
            if (request->hasParam("telegram_cmdEnabled", true))
            {
                enableTelegramCmd = true;
            }
            else
            {
                String cmdValue = getBody("telegram_cmdEnabled_value");
                enableTelegramCmd = (cmdValue == "1" || cmdValue == "on" || cmdValue == "true");
            }
            settings.telegram_cmdEnabled = enableTelegramCmd;

            String fpw = getBody("telegram_pollIntervalMs");
            if (!fpw.isEmpty())
            {
                uint32_t val = (uint32_t)fpw.toInt() * 1000UL;
                if (val < 3000) val = 3000;
                if (val > 120000) val = 120000;
                settings.telegram_pollIntervalMs = val;
            }
#endif
        }
        // Wenn deaktiviert: Token/ChatId bewusst nicht überschreiben.

        settingsManager.saveAppSettings(settings);
        LOG_PRINTLN("[Telegram] Modul-Save OK: enabled=" + String(settings.telegram_enabled) +
#if USE_TELEGRAM_CMD
                    ", cmdEnabled=" + String(settings.telegram_cmdEnabled) +
#endif
                    ", tokenEmpty=" + String(settings.telegram_botToken.isEmpty()) + ", chatEmpty=" + String(settings.telegram_chatId.isEmpty()));
        shouldReboot = true;
    // Option A (klassisch): bei normalem Formular-Submit IMMER zurück zur Settings-Seite.
    // JSON nur, wenn explizit angefordert (z.B. /module/telegram/save?format=json).
    bool wantsJson = false;
    if (request->hasParam("format") && request->getParam("format")->value().equalsIgnoreCase("json"))
    {
        wantsJson = true;
    }

    if (wantsJson)
    {
        String out = String("{\"ok\":true,\"enabled\":") + (settings.telegram_enabled ? "true" : "false") +
#if USE_TELEGRAM_CMD
                     String(",\"cmdEnabled\":") + (settings.telegram_cmdEnabled ? "true" : "false") +
#endif
                     String(",\"tokenEmpty\":") + (settings.telegram_botToken.isEmpty() ? "true" : "false") +
                     String(",\"chatEmpty\":") + (settings.telegram_chatId.isEmpty() ? "true" : "false") + "}";

        AsyncWebServerResponse *res = request->beginResponse(200, "application/json", out);
        res->addHeader("X-Module", "telegram");
        res->addHeader("Cache-Control", "no-store");
        request->send(res);
    }
    else
    {
           request->redirect("/settings");
    } });
}
#endif // USE_TELEGRAM
