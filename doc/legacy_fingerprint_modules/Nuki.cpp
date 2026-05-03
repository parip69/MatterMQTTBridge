// Beschreibung: Minimalistische Nuki-MQTT-Anbindung fuer SingleOutputAction

#if USE_NUKI

#include "Nuki.h"
#include "SettingsManager.h"
#include "global.h"
#include <ArduinoJson.h>

extern SettingsManager settingsManager;
extern bool shouldReboot;
void publishMqttMessage(const String &topic, const String &message, bool retain = false, int qos = 0);

static String htmlEscape(const String &input)
{
    String out;
    out.reserve(input.length() + 8);
    for (size_t i = 0; i < input.length(); ++i)
    {
        char c = input[i];
        switch (c)
        {
        case '&': out += F("&amp;"); break;
        case '<': out += F("&lt;"); break;
        case '>': out += F("&gt;"); break;
        case '"': out += F("&quot;"); break;
        case '\'': out += F("&#39;"); break;
        default: out += c; break;
        }
    }
    return out;
}

static void appendTextInput(String &html, const String &label, const String &id, const String &name, const String &value)
{
    html += F("      <div class='form-group'>");
    html += F("        <label class='col-md-4 control-label' for='");
    html += id;
    html += F("'>");
    html += label;
    html += F("</label>");
    html += F("        <div class='col-md-4'>");
    html += F("          <input id='");
    html += id;
    html += F("' name='");
    html += name;
    html += F("' type='text' class='form-control input-md' value='");
    html += htmlEscape(value);
    html += F("'>");
    html += F("        </div>");
    html += F("      </div>");
}

static void appendNukiCommandInputs(String &html,
                                    const String &title,
                                    const String &baseId,
                                    const String &actionName,
                                    const String &actionValue,
                                    const String &topicName,
                                    const String &topicValue,
                                    const String &payloadName,
                                    const String &payloadValue)
{
    html += F("      <hr>");
    html += F("      <h4>");
    html += title;
    html += F("</h4>");

    String actionId = baseId + "-single-output-action";
    String actionLabel = title + ": SingleOutputAction";
    appendTextInput(html, actionLabel, actionId, actionName, actionValue);

    String topicId = baseId + "-topic";
    String topicLabel = title + ": MQTT Topic";
    appendTextInput(html, topicLabel, topicId, topicName, topicValue);

    String payloadId = baseId + "-payload";
    String payloadLabel = title + ": MQTT Payload";
    appendTextInput(html, payloadLabel, payloadId, payloadName, payloadValue);
}

String buildNukiModuleHtml()
{
    AppSettings settings = settingsManager.getAppSettings();

    String html;
    html.reserve(5200);

    html += F("<div class='settings-section' id='nuki-section'>");
    html += F("<form class='form-horizontal' action='/module/nuki/save' method='post' onsubmit='return closeEventSourceBeforeSubmit(this);'>");
    html += F("  <fieldset>");
    html += F("    <legend class='legend-cursor' onclick=\"sectionToggleCollapse('nuki-settings')\">Nuki (MQTT-Schloss)</legend>");
    html += F("    <div id='nuki-settings-content' class='collapsed-content'>");

    html += F("      <div class='form-group'>");
    html += F("        <label class='col-md-4 control-label' for='nuki-enabled'>Aktivieren</label>");
    html += F("        <div class='col-md-4'>");
    html += F("          <input type='hidden' name='nuki_enabled_value' value='0'>");
    html += F("          <input id='nuki-enabled' name='nuki_enabled' type='checkbox' class='form-check-input'");
    if (settings.nuki_enabled)
    {
        html += F(" checked");
    }
    html += F(" value='1'>");
    html += F("        </div>");
    html += F("      </div>");

    if (settings.nuki_enabled)
    {
        html += F("      <div class='form-group'>");
        html += F("        <label class='col-md-4 control-label'></label>");
        html += F("        <div class='col-md-8'>");
        html += F("          <small class='text-muted'>Beispiel: 1 = Finger/Button/Output 1, 2 = Finger/Button/Output 2, 8 = reine Software-Aktion ohne Hardware-Pin</small>");
        html += F("        </div>");
        html += F("      </div>");

        appendNukiCommandInputs(html, F("Nuki Öffnen"), F("nuki-unlock"), F("nuki_unlock_single_output_action"), settings.nuki_unlock_single_output_action, F("nuki_topic_unlock"), settings.nuki_topic_unlock, F("nuki_payload_unlock"), settings.nuki_payload_unlock);
        appendNukiCommandInputs(html, F("Nuki Schließen"), F("nuki-lock"), F("nuki_lock_single_output_action"), settings.nuki_lock_single_output_action, F("nuki_topic_lock"), settings.nuki_topic_lock, F("nuki_payload_lock"), settings.nuki_payload_lock);
        appendNukiCommandInputs(html, F("Nuki Falle ziehen"), F("nuki-unlatch"), F("nuki_unlatch_single_output_action"), settings.nuki_unlatch_single_output_action, F("nuki_topic_unlatch"), settings.nuki_topic_unlatch, F("nuki_payload_unlatch"), settings.nuki_payload_unlatch);
        appendNukiCommandInputs(html, F("Nuki Lock'n'Go"), F("nuki-lockngo"), F("nuki_lockngo_single_output_action"), settings.nuki_lockngo_single_output_action, F("nuki_topic_lockngo"), settings.nuki_topic_lockngo, F("nuki_payload_lockngo"), settings.nuki_payload_lockngo);
        appendNukiCommandInputs(html, F("Nuki Lock'n'Go Falle"), F("nuki-lockngo-unlatch"), F("nuki_lockngo_unlatch_single_output_action"), settings.nuki_lockngo_unlatch_single_output_action, F("nuki_topic_lockngo_unlatch"), settings.nuki_topic_lockngo_unlatch, F("nuki_payload_lockngo_unlatch"), settings.nuki_payload_lockngo_unlatch);
        appendNukiCommandInputs(html, F("Nuki Full Lock"), F("nuki-full-lock"), F("nuki_full_lock_single_output_action"), settings.nuki_full_lock_single_output_action, F("nuki_topic_full_lock"), settings.nuki_topic_full_lock, F("nuki_payload_full_lock"), settings.nuki_payload_full_lock);
    }

    html += F("      <div class='form-group'>");
    html += F("        <label class='col-md-4 control-label'></label>");
    html += F("        <div class='col-md-8 actions'>");
    html += F("          <button type='submit' class='btn btn-success' name='btnSaveSettings' value='nuki'>Speichern</button>");
    html += F("        </div>");
    html += F("      </div>");

    html += F("    </div>");
    html += F("  </fieldset>");
    html += F("</form>");
    html += F("</div>");

    return html;
}

void nukiHandleSingleOutputAction(const String &action)
{
    AppSettings settings = settingsManager.getAppSettings();

    if (!settings.nuki_enabled)
        return;

    struct NukiActionMap
    {
        const String *singleOutputAction;
        const String *topic;
        const String *payload;
        const char *name;
    };

    const NukiActionMap nukiActions[] = {
        {&settings.nuki_unlock_single_output_action, &settings.nuki_topic_unlock, &settings.nuki_payload_unlock, "Unlock"},
        {&settings.nuki_lock_single_output_action, &settings.nuki_topic_lock, &settings.nuki_payload_lock, "Lock"},
        {&settings.nuki_unlatch_single_output_action, &settings.nuki_topic_unlatch, &settings.nuki_payload_unlatch, "Unlatch"},
        {&settings.nuki_lockngo_single_output_action, &settings.nuki_topic_lockngo, &settings.nuki_payload_lockngo, "Lock'n'Go"},
        {&settings.nuki_lockngo_unlatch_single_output_action, &settings.nuki_topic_lockngo_unlatch, &settings.nuki_payload_lockngo_unlatch, "Lock'n'Go Unlatch"},
        {&settings.nuki_full_lock_single_output_action, &settings.nuki_topic_full_lock, &settings.nuki_payload_full_lock, "Full Lock"},
    };

    for (const auto &entry : nukiActions)
    {
        if (!entry.singleOutputAction->isEmpty() && action == *entry.singleOutputAction)
        {
            publishMqttMessage(entry.topic->c_str(), entry.payload->c_str());
            LOG_PRINTLN(String("[Nuki] ") + entry.name + " via SingleOutputAction: " + action);
            return;
        }
    }
}

void updateNukiSettingsFromRequest(AppSettings &settings, AsyncWebServerRequest *request)
{
    settings.nuki_enabled = request->hasArg("nuki_enabled");

    if (request->hasArg("nuki_unlock_single_output_action")) settings.nuki_unlock_single_output_action = request->arg("nuki_unlock_single_output_action");
    if (request->hasArg("nuki_lock_single_output_action")) settings.nuki_lock_single_output_action = request->arg("nuki_lock_single_output_action");
    if (request->hasArg("nuki_unlatch_single_output_action")) settings.nuki_unlatch_single_output_action = request->arg("nuki_unlatch_single_output_action");
    if (request->hasArg("nuki_lockngo_single_output_action")) settings.nuki_lockngo_single_output_action = request->arg("nuki_lockngo_single_output_action");
    if (request->hasArg("nuki_lockngo_unlatch_single_output_action")) settings.nuki_lockngo_unlatch_single_output_action = request->arg("nuki_lockngo_unlatch_single_output_action");
    if (request->hasArg("nuki_full_lock_single_output_action")) settings.nuki_full_lock_single_output_action = request->arg("nuki_full_lock_single_output_action");

    if (request->hasArg("nuki_topic_unlock")) settings.nuki_topic_unlock = request->arg("nuki_topic_unlock");
    if (request->hasArg("nuki_payload_unlock")) settings.nuki_payload_unlock = request->arg("nuki_payload_unlock");
    if (request->hasArg("nuki_topic_lock")) settings.nuki_topic_lock = request->arg("nuki_topic_lock");
    if (request->hasArg("nuki_payload_lock")) settings.nuki_payload_lock = request->arg("nuki_payload_lock");
    if (request->hasArg("nuki_topic_unlatch")) settings.nuki_topic_unlatch = request->arg("nuki_topic_unlatch");
    if (request->hasArg("nuki_payload_unlatch")) settings.nuki_payload_unlatch = request->arg("nuki_payload_unlatch");
    if (request->hasArg("nuki_topic_lockngo")) settings.nuki_topic_lockngo = request->arg("nuki_topic_lockngo");
    if (request->hasArg("nuki_payload_lockngo")) settings.nuki_payload_lockngo = request->arg("nuki_payload_lockngo");
    if (request->hasArg("nuki_topic_lockngo_unlatch")) settings.nuki_topic_lockngo_unlatch = request->arg("nuki_topic_lockngo_unlatch");
    if (request->hasArg("nuki_payload_lockngo_unlatch")) settings.nuki_payload_lockngo_unlatch = request->arg("nuki_payload_lockngo_unlatch");
    if (request->hasArg("nuki_topic_full_lock")) settings.nuki_topic_full_lock = request->arg("nuki_topic_full_lock");
    if (request->hasArg("nuki_payload_full_lock")) settings.nuki_payload_full_lock = request->arg("nuki_payload_full_lock");
}

void registerNukiEndpoints(AsyncWebServer &server)
{
    server.on("/ui/nuki", HTTP_GET, [](AsyncWebServerRequest *req)
              {
        String html = buildNukiModuleHtml();
        AsyncWebServerResponse* res = req->beginResponse(200, "text/html; charset=utf-8", html);
        res->addHeader("Cache-Control", "no-store");
        req->send(res); });

    server.on("/module/nuki/save", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        AppSettings settings = settingsManager.getAppSettings();

        auto getBody = [&](const char* name) -> String {
            if (request->hasParam(name, true))
                return request->getParam(name, true)->value();
            return String();
        };

        if (request->hasParam("nuki_enabled", true))
        {
            settings.nuki_enabled = true;
        }
        else
        {
            String v = getBody("nuki_enabled_value");
            settings.nuki_enabled = (v == "1" || v == "on" || v == "true");
        }

        settings.nuki_unlock_single_output_action = getBody("nuki_unlock_single_output_action");
        settings.nuki_lock_single_output_action = getBody("nuki_lock_single_output_action");
        settings.nuki_unlatch_single_output_action = getBody("nuki_unlatch_single_output_action");
        settings.nuki_lockngo_single_output_action = getBody("nuki_lockngo_single_output_action");
        settings.nuki_lockngo_unlatch_single_output_action = getBody("nuki_lockngo_unlatch_single_output_action");
        settings.nuki_full_lock_single_output_action = getBody("nuki_full_lock_single_output_action");

        settings.nuki_topic_unlock = getBody("nuki_topic_unlock");
        settings.nuki_payload_unlock = getBody("nuki_payload_unlock");
        settings.nuki_topic_lock = getBody("nuki_topic_lock");
        settings.nuki_payload_lock = getBody("nuki_payload_lock");
        settings.nuki_topic_unlatch = getBody("nuki_topic_unlatch");
        settings.nuki_payload_unlatch = getBody("nuki_payload_unlatch");
        settings.nuki_topic_lockngo = getBody("nuki_topic_lockngo");
        settings.nuki_payload_lockngo = getBody("nuki_payload_lockngo");
        settings.nuki_topic_lockngo_unlatch = getBody("nuki_topic_lockngo_unlatch");
        settings.nuki_payload_lockngo_unlatch = getBody("nuki_payload_lockngo_unlatch");
        settings.nuki_topic_full_lock = getBody("nuki_topic_full_lock");
        settings.nuki_payload_full_lock = getBody("nuki_payload_full_lock");

        settingsManager.saveAppSettings(settings);
        LOG_PRINTLN("[Nuki] Modul-Save OK: enabled=" + String(settings.nuki_enabled));
        shouldReboot = true;
        request->redirect("/settings"); });

    server.on("/api/nuki/get", HTTP_GET, [](AsyncWebServerRequest *req)
              {
        AppSettings settings = settingsManager.getAppSettings();
        JsonDocument doc;
        doc["enabled"] = settings.nuki_enabled;
        doc["unlock_single_output_action"] = settings.nuki_unlock_single_output_action;
        doc["lock_single_output_action"] = settings.nuki_lock_single_output_action;
        doc["unlatch_single_output_action"] = settings.nuki_unlatch_single_output_action;
        doc["lockngo_single_output_action"] = settings.nuki_lockngo_single_output_action;
        doc["lockngo_unlatch_single_output_action"] = settings.nuki_lockngo_unlatch_single_output_action;
        doc["full_lock_single_output_action"] = settings.nuki_full_lock_single_output_action;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out); });
}

#endif // USE_NUKI
