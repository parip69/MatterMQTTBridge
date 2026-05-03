// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉

#if USE_TEDEE

#include "Tedee.h"
#include "SettingsManager.h"
#include "global.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Forward declare SettingsManager instance from main.cpp
extern SettingsManager settingsManager;
extern bool shouldReboot;

// Zustand für asynchrone Tedee-Steuerung
enum class TedeeAction
{
    LOCK,
    UNLOCK,
    GET_STATUS,
    TOGGLE
};

// FreeRTOS Queue and Task handles
static QueueHandle_t tedeeQueue = NULL;
static TaskHandle_t tedeeTaskHandle = NULL;
#define TEDEE_QUEUE_LENGTH 5
static const uint16_t TEDEE_STATUS_TIMEOUT_MS = 3000;
static const uint16_t TEDEE_COMMAND_TIMEOUT_MS = 8000;
static const uint16_t TEDEE_STATUS_AFTER_TIMEOUT_DELAY_MS = 2500;

// Vorwärtsdeklaration der Task-Funktion
void tedeeWorkerTask(void *pvParameters);

static bool ensureTedeeConfig(const AppSettings &settings, const char *context)
{
    if (settings.tedee_bridge_ip.isEmpty() || settings.tedee_token.isEmpty())
    {
        LOG_PRINTF("[Tedee] %s abgebrochen: unvollständige Konfiguration (Bridge-IP/Token).\n", context);
        return false;
    }
    if (settings.tedee_port <= 0)
    {
        LOG_PRINTF("[Tedee] %s abgebrochen: ungültiger Port %d.\n", context, settings.tedee_port);
        return false;
    }
    return true;
}

static bool ensureTedeeNetwork(const char *context)
{
    static uint32_t lastNotifyMs = 0;
    const uint32_t now = millis();

    if (WiFi.status() != WL_CONNECTED)
    {
        LOG_PRINTF("[Tedee] %s abgebrochen: WLAN nicht verbunden.\n", context);
        if (now - lastNotifyMs > 5000UL)
        {
            notifyClients("Tedee abgebrochen: WLAN nicht verbunden", "Tedee");
            lastNotifyMs = now;
        }
        return false;
    }
    return true;
}

static String buildTedeeBaseUrl(const AppSettings &settings)
{
    return "http://" + settings.tedee_bridge_ip + ":" + String(settings.tedee_port) + "/v1.0/lock/" + String(settings.tedee_lock_id);
}

static void tedeeHttpPrepare(HTTPClient &http, uint16_t timeoutMs = TEDEE_STATUS_TIMEOUT_MS)
{
    http.setTimeout(timeoutMs);
    http.setReuse(false);
    http.useHTTP10(true);
}

static bool tedeeShouldNotifyNow(uint32_t &lastNotifyMs, const uint32_t intervalMs = 1500UL)
{
    const uint32_t now = millis();
    if (now - lastNotifyMs >= intervalMs)
    {
        lastNotifyMs = now;
        return true;
    }
    return false;
}

static void sendActionToQueue(TedeeAction action)
{
    if (tedeeQueue == NULL)
    {
        LOG_PRINTLN("[Tedee] FEHLER: Queue nicht initialisiert.");
        return;
    }
    if (xQueueSend(tedeeQueue, &action, (TickType_t)0) != pdPASS)
    {
        LOG_PRINTLN("[Tedee] WARNUNG: Queue ist voll. Aktion verworfen.");
    }
    else
    {
        LOG_PRINTLN("[Tedee] >> Aktion zur Queue hinzugefügt.");
    }
}

static void queueTedeeCommand(TedeeAction command, const char *sourceTag, const char *actionName)
{
    AppSettings settings = settingsManager.getAppSettings();
    if (!settings.tedee_enabled)
    {
        LOG_PRINTF("[%s] %s ignoriert: Tedee deaktiviert.\n", sourceTag, actionName);
        return;
    }
    LOG_PRINTF("[%s] %s registriert.\n", sourceTag, actionName);
    sendActionToQueue(command);
}

void tedeeUnlock()
{
    queueTedeeCommand(TedeeAction::UNLOCK, "Tedee", "Öffnen-Anfrage");
}

void tedeeLock()
{
    queueTedeeCommand(TedeeAction::LOCK, "Tedee", "Schließen-Anfrage");
}

void getTedeeStatus()
{
    queueTedeeCommand(TedeeAction::GET_STATUS, "Tedee", "Status-Anfrage");
}

void toggleTedeeStatus()
{
    queueTedeeCommand(TedeeAction::TOGGLE, "Tedee", "Toggle-Anfrage");
}

void tedeeHandleSingleOutputAction(const String &action)
{
    AppSettings settings = settingsManager.getAppSettings();

    if (!settings.tedee_enabled)
        return;

    struct TedeeActionMap
    {
        const String *singleOutputAction;
        void (*execute)();
        const char *name;
    };

    const TedeeActionMap tedeeActions[] = {
        {&settings.tedee_unlock_single_output_action, tedeeUnlock, "Unlock"},
        {&settings.tedee_lock_single_output_action, tedeeLock, "Lock"},
    };

    for (const auto &entry : tedeeActions)
    {
        if (!entry.singleOutputAction->isEmpty() && action == *entry.singleOutputAction)
        {
            LOG_PRINTLN(String("[Tedee] SingleOutputAction -> ") + entry.name + ": " + action);
            entry.execute();
            return;
        }
    }
}

void tedeeWorkerTask(void *pvParameters)
{
    TedeeAction action;
    TedeeAction lastProcessedAction = TedeeAction::GET_STATUS; // Neutraler Startwert
    bool hasLastProcessedAction = false;
    uint32_t lastStatusNotifyMs = 0;
    uint32_t lastActionNotifyMs = 0;

    for (;;)
    {
        // Unbegrenzt auf eine Aktion in der Queue warten
        if (xQueueReceive(tedeeQueue, &action, portMAX_DELAY) == pdPASS)
        {
            AppSettings settings = settingsManager.getAppSettings();
            if (!ensureTedeeConfig(settings, "Task"))
            {
                vTaskDelay(pdMS_TO_TICKS(1000)); // Kurze Pause bei Konfigurationsfehler
                continue;
            }
            if (!ensureTedeeNetwork("Task"))
            {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }

            // Dubletten-Prüfung: identische direkte Wiederholungen überspringen
            static uint32_t lastCommandMs = 0;
            const uint32_t nowMs = millis();

            if (hasLastProcessedAction &&
                action != TedeeAction::GET_STATUS &&
                action == lastProcessedAction &&
                (nowMs - lastCommandMs < 1200UL))
            {
                LOG_PRINTLN("[Tedee Task] Doppelte Tedee-Aktion innerhalb kurzer Zeit übersprungen.");
                continue;
            }

            lastProcessedAction = action;
            hasLastProcessedAction = true;
            lastCommandMs = nowMs;

            String baseUrl = buildTedeeBaseUrl(settings);

            // F3: networkMutex NICHT über die gesamte HTTP-Transaktion halten.
            // Der Task ist bereits über die Queue serialisiert, HTTP-Objekte sind lokal.
            // Langes Lock (bis zu 5s Timeout) blockierte unnötig MQTT-Publishes.
            {
                HTTPClient http;
                WiFiClient client;

                if (action == TedeeAction::LOCK || action == TedeeAction::UNLOCK)
                {
                    String url;
                    String actionVerb = (action == TedeeAction::UNLOCK) ? "Öffnen" : "Schließen";
                    url.reserve(baseUrl.length() + settings.tedee_token.length() + 24);
                    url = baseUrl;
                    url += (action == TedeeAction::UNLOCK ? "/unlock" : "/lock");
                    url += F("?api_token=");
                    url += settings.tedee_token;

                    if (!http.begin(client, url))
                    {
                        LOG_PRINTLN("[Tedee Task] Fehler: HTTP-Verbindung zur Bridge konnte nicht aufgebaut werden.");
                        notifyClients("Fehler: Bridge-Verbindung konnte nicht aufgebaut werden", "Tedee");
                        continue;
                    }
                    tedeeHttpPrepare(http, TEDEE_COMMAND_TIMEOUT_MS);
                    http.addHeader("Content-Length", "0");
                    int httpCode = http.POST("");

                    if (httpCode == 204)
                    {
                        LOG_PRINTLN("[Tedee Task] Schloss wird " + actionVerb + "...");
                        if (tedeeShouldNotifyNow(lastActionNotifyMs, 1200UL))
                        {
                            notifyClients("Schloss wird " + actionVerb, "Tedee");
                        }
                    }
                    else if (httpCode == 406)
                    {
                        String msg = (action == TedeeAction::UNLOCK) ? "Schloss ist bereits entsperrt" : "Schloss ist bereits gesperrt";
                        LOG_PRINTLN("[Tedee Task] " + msg + " (HTTP 406)");
                        if (tedeeShouldNotifyNow(lastActionNotifyMs, 1200UL))
                        {
                            notifyClients(msg, "Tedee");
                        }
                    }
                    else if (httpCode == HTTPC_ERROR_READ_TIMEOUT)
                    {
                        LOG_PRINTF("[Tedee Task] %s-Befehl gesendet, aber Bridge-Antwort dauerte zu lange (read Timeout).\n", actionVerb.c_str());
                        notifyClients(actionVerb + "-Befehl gesendet, Status wird gleich geprüft", "Tedee");
                        vTaskDelay(pdMS_TO_TICKS(TEDEE_STATUS_AFTER_TIMEOUT_DELAY_MS));
                        sendActionToQueue(TedeeAction::GET_STATUS);
                    }
                    else
                    {
                        String errMsg = (httpCode < 0) ? http.errorToString(httpCode) : "HTTP " + String(httpCode);
                        LOG_PRINTF("[Tedee Task] Fehler beim %s: %s\n", actionVerb.c_str(), errMsg.c_str());
                        notifyClients("Fehler beim " + actionVerb + ": " + errMsg, "Tedee");
                    }
                }
                else if (action == TedeeAction::GET_STATUS || action == TedeeAction::TOGGLE)
                {
                    String url;
                    url.reserve(baseUrl.length() + settings.tedee_token.length() + 12);
                    url = baseUrl;
                    url += F("?api_token=");
                    url += settings.tedee_token;
                    if (!http.begin(client, url))
                    {
                        LOG_PRINTLN("[Tedee Task] Fehler: HTTP-Verbindung zur Bridge konnte nicht aufgebaut werden.");
                        notifyClients("Fehler: Bridge-Verbindung konnte nicht aufgebaut werden", "Tedee");
                        continue;
                    }
                    tedeeHttpPrepare(http);
                    int httpCode = http.GET();

                    if (httpCode == 200)
                    {
                        String payload = http.getString();
                        if (payload.isEmpty())
                        {
                            LOG_PRINTLN("[Tedee Task] Leere Antwort von der Bridge erhalten.");
                            notifyClients("Fehler: Leere Antwort von der Bridge", "Tedee");
                            http.end();
                            vTaskDelay(pdMS_TO_TICKS(1000));
                            continue;
                        }

                        JsonDocument doc;
                        DeserializationError err = deserializeJson(doc, payload);

                        if (err == DeserializationError::Ok && doc["state"].is<int>())
                        {
                            int stateVal = doc["state"];
                            if (action == TedeeAction::TOGGLE)
                            {
                                TedeeAction nextAction = (stateVal == 2) ? TedeeAction::LOCK : TedeeAction::UNLOCK;
                                LOG_PRINTF("[Tedee Task] Toggle: Schloss-Status ist %d, nächste Aktion: %s\n", stateVal, (nextAction == TedeeAction::LOCK ? "LOCK" : "UNLOCK"));
                                sendActionToQueue(nextAction);
                            }
                            else
                            {
                                String statusText;
                                switch (stateVal)
                                {
                                case 2:
                                    statusText = "Entsperrt";
                                    break;
                                case 6:
                                    statusText = "Gesperrt";
                                    break;
                                default:
                                    statusText = "Unbekannt (" + String(stateVal) + ")";
                                    break;
                                }
                                if (tedeeShouldNotifyNow(lastStatusNotifyMs, 1500UL))
                                {
                                    notifyClients("Schloss-Status: " + statusText, "Tedee");
                                }
                            }
                        }
                        else
                        {
                            LOG_PRINTF("[Tedee Task] Unerwartete API-Antwort oder JSON-Fehler: %s\n", err.c_str());
                            notifyClients("Fehler: Unerwartete API-Antwort", "Tedee");
                        }
                    }
                    else
                    {
                        String errMsg = (httpCode < 0) ? http.errorToString(httpCode) : "HTTP " + String(httpCode);
                        LOG_PRINTF("[Tedee Task] Fehler bei Status/Toggle: %s\n", errMsg.c_str());
                        notifyClients("Status-Abfrage fehlgeschlagen: " + errMsg, "Tedee");
                    }
                }

                http.end();
            }
            vTaskDelay(pdMS_TO_TICKS(1000)); // Kurze Pause zur Stabilisierung
        }
    }
}

void tedee_init()
{
    tedeeQueue = xQueueCreate(TEDEE_QUEUE_LENGTH, sizeof(TedeeAction));

    if (tedeeQueue == NULL)
    {
        LOG_PRINTLN("[Tedee] FEHLER: Queue konnte nicht erstellt werden.");
        return;
    }

    BaseType_t result = xTaskCreate(
        tedeeWorkerTask,
        "TedeeWorker",
        6144,
        NULL,
        5, // Priorität
        &tedeeTaskHandle);

    if (result != pdPASS)
    {
        LOG_PRINTLN("[Tedee] FEHLER: Worker-Task konnte nicht erstellt werden.");
        vQueueDelete(tedeeQueue);
        tedeeQueue = NULL;
    }
    else
    {
        LOG_PRINTLN("[Tedee] ✅ Worker-Task und Queue erfolgreich initialisiert.");
    }
}

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

static void appendTedeeInput(String &html, const __FlashStringHelper *id, const __FlashStringHelper *name,
                             const __FlashStringHelper *type, const __FlashStringHelper *label,
                             const String &value, const __FlashStringHelper *attrs = nullptr,
                             const __FlashStringHelper *hint = nullptr)
{
    html += F("<div class='form-group'><label class='col-md-4 control-label' for='");
    html += id;
    html += F("'>");
    html += label;
    html += F("</label><div class='col-md-4'><input id='");
    html += id;
    html += F("' name='");
    html += name;
    html += F("' type='");
    html += type;
    html += F("' class='form-control input-md'");
    if (attrs)
        html += attrs;
    html += F(" value='");
    html += value;
    html += F("'>");
    if (hint)
    {
        html += F("<small class='text-muted'>");
        html += hint;
        html += F("</small>");
    }
    html += F("</div></div>");
}

static void appendTedeeActions(String &html, const __FlashStringHelper *buttons)
{
    html += F("<div class='form-group'><label class='col-md-4 control-label'></label><div class='col-md-8 actions'>");
    html += buttons;
    html += F("</div></div>");
}

String buildTedeeModuleHtml()
{
    AppSettings settings = settingsManager.getAppSettings();

    String html;
    html.reserve(2400);

    html += F("<div class='settings-section' id='tedee-section'><form class='form-horizontal' action='/module/tedee/save' method='post' onsubmit='return closeEventSourceBeforeSubmit(this);'><fieldset><legend class='legend-cursor' onclick=\"sectionToggleCollapse('tedee-settings')\">Tedee (Schloss)</legend><div id='tedee-settings-content' class='collapsed-content'>");
    html += F("<div class='form-group'><label class='col-md-4 control-label' for='tedee-enabled'>Aktivieren</label><div class='col-md-4'><input type='hidden' name='tedee_enabled_value' value='0'><input id='tedee-enabled' name='tedee_enabled' type='checkbox' class='form-check-input'");
    if (settings.tedee_enabled)
        html += F(" checked");
    html += F(" value='1'></div></div>");

    if (settings.tedee_enabled)
    {
        appendTedeeInput(html, F("tedee-bridge-ip"), F("tedee_bridge_ip"), F("text"), F("Bridge-IP"),
                         htmlEscape(settings.tedee_bridge_ip), F(" placeholder='z.B. 192.168.1.50'"));
        appendTedeeInput(html, F("tedee-token"), F("tedee_token"), F("password"), F("Access-Token"),
                         settings.tedee_token.isEmpty() ? String() : String(F("********")),
                         F(" placeholder='********'"), F("Leer lassen = aktuelles Token behalten"));
        appendTedeeInput(html, F("tedee-port"), F("tedee_port"), F("number"), F("Port"),
                         String(settings.tedee_port), F(" min='1' max='65535'"));
        appendTedeeInput(html, F("tedee-lock-id"), F("tedee_lock_id"), F("number"), F("Lock ID"),
                         String(settings.tedee_lock_id), F(" min='0'"), F("Von der Tedee Bridge oder App"));
        appendTedeeInput(html, F("tedee-unlock-single-output-action"), F("tedee_unlock_single_output_action"),
                         F("text"), F("Tedee Öffnen bei SingleOutputAction"),
                         htmlEscape(settings.tedee_unlock_single_output_action));
        appendTedeeInput(html, F("tedee-lock-single-output-action"), F("tedee_lock_single_output_action"),
                         F("text"), F("Tedee Schließen bei SingleOutputAction"),
                         htmlEscape(settings.tedee_lock_single_output_action), nullptr,
                         F("Beispiel: 1 = Button/Finger 1, 2 = Button/Finger 2, 8 = reine Software-Aktion ohne Hardware-Pin"));
        appendTedeeActions(html, F("<button type='submit' class='btn btn-success' name='btnSaveSettings' value='tedee' style='margin-right:10px;'>Speichern</button><a href=\"javascript:fetch('/api/tedee/test').then(r=>r.json()).then(d=>alert(d.ok?'OK':'Error')).catch(e=>alert('Error: '+e))\" class='btn btn-primary'>Verbindung testen</a>"));
        appendTedeeActions(html, F("<a href=\"javascript:if(confirm('Möchten Sie das Schloss wirklich entsperren?')){fetch('/api/tedee/unlock',{method:'POST'})}\" class='btn btn-info' style='margin-right:10px;'>Entsperren</a><a href=\"javascript:if(confirm('Möchten Sie das Schloss wirklich sperren?')){fetch('/api/tedee/lock',{method:'POST'})}\" class='btn btn-warning' style='margin-right:10px;'>Sperren</a><a href=\"javascript:fetch('/api/tedee/test').then(r=>r.json()).then(d=>alert(d.ok?'Status angefragt':'Status-Abfrage fehlgeschlagen')).catch(e=>alert('Status-Abfrage fehlgeschlagen: '+e))\" class='btn btn-primary'>Status</a>"));
    }
    else
    {
        appendTedeeActions(html, F("<button type='submit' class='btn btn-success' name='btnSaveSettings' value='tedee'>Speichern</button>"));
    }

    html += F("</div></fieldset></form></div>");

    return html;
}

void registerTedeeEndpoints(AsyncWebServer &server)
{
    // UI-Snippet
    server.on("/ui/tedee", HTTP_GET, [](AsyncWebServerRequest *req)
              {
        String html = buildTedeeModuleHtml();
        AsyncWebServerResponse* res = req->beginResponse(200, "text/html; charset=utf-8", html);
        res->addHeader("Cache-Control", "no-store");
        req->send(res); });

    server.on("/module/tedee/save", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        AppSettings settings = settingsManager.getAppSettings();

        auto getBody = [&](const char* name) -> String {
            if (request->hasParam(name, true))
                return request->getParam(name, true)->value();
            return String();
        };

        // Checkbox robust: entweder klassisch als Flag oder als Fallback-Feld (0/1)
        if (request->hasParam("tedee_enabled", true))
        {
            settings.tedee_enabled = true;
        }
        else
        {
            String v = getBody("tedee_enabled_value");
            settings.tedee_enabled = (v == "1" || v == "on" || v == "true");
        }

        String ip = getBody("tedee_bridge_ip");
        if (!ip.isEmpty()) settings.tedee_bridge_ip = ip;

        String portStr = getBody("tedee_port");
        if (!portStr.isEmpty())
        {
            int newPort = portStr.toInt();
            if (newPort > 0 && newPort <= 65535) settings.tedee_port = newPort;
        }

        String lockIdStr = getBody("tedee_lock_id");
        if (!lockIdStr.isEmpty())
        {
            int lockId = lockIdStr.toInt();
            if (lockId >= 0) settings.tedee_lock_id = lockId;
        }

        // Token: "********" bedeutet: altes behalten
        if (request->hasParam("tedee_token", true))
        {
            String tokenArg = getBody("tedee_token");
            if (tokenArg != F("********"))
                settings.tedee_token = tokenArg;
        }

        if (request->hasParam("tedee_unlock_single_output_action", true))
            settings.tedee_unlock_single_output_action = getBody("tedee_unlock_single_output_action");
        if (request->hasParam("tedee_lock_single_output_action", true))
            settings.tedee_lock_single_output_action = getBody("tedee_lock_single_output_action");

        settingsManager.saveAppSettings(settings);
        LOG_PRINTLN("[Tedee] Modul-Save OK: enabled=" + String(settings.tedee_enabled) + ", ip=" + settings.tedee_bridge_ip + ", port=" + String(settings.tedee_port) + ", lockId=" + String(settings.tedee_lock_id));
        shouldReboot = true;
        // Option A (klassisch): nach Save zurück zur Settings-Seite.
        // JSON nur, wenn explizit angefordert (z.B. /module/tedee/save?format=json).
        bool wantsJson = false;
        if (request->hasParam("format") && request->getParam("format")->value().equalsIgnoreCase("json"))
        {
            wantsJson = true;
        }

        if (wantsJson)
        {
            request->send(200, "application/json", "{'ok':true}");
        }
        else
        {
            request->redirect("/settings");
        } });

    // API: Test/Lock/Unlock
    server.on("/api/tedee/test", HTTP_GET, [](AsyncWebServerRequest *req)
              {
        getTedeeStatus();
        req->send(200, "application/json", "{\"ok\":true}"); });
    server.on("/api/tedee/lock", HTTP_POST, [](AsyncWebServerRequest *req)
              {
        tedeeLock();
        req->send(200, "application/json", "{\"ok\":true}"); });
    server.on("/api/tedee/unlock", HTTP_POST, [](AsyncWebServerRequest *req)
              {
        tedeeUnlock();
        req->send(200, "application/json", "{\"ok\":true}"); });
}
#endif // USE_TEDEE
