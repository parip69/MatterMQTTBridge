// Beschreibung: Minimalistische Nuki-MQTT-Anbindung fuer SingleOutputAction
#pragma once

#if USE_NUKI

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

struct AppSettings;
class AsyncWebServer;

String buildNukiModuleHtml();
void registerNukiEndpoints(AsyncWebServer &server);
void updateNukiSettingsFromRequest(AppSettings &settings, AsyncWebServerRequest *request);
void nukiHandleSingleOutputAction(const String &action);

#endif // USE_NUKI
