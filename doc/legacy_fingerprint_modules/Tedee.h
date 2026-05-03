// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#pragma once

#if USE_TEDEE

#include <Arduino.h>
#include "global.h"
#include <ESPAsyncWebServer.h>

// Forward declarations
void notifyClients(String message, const char *sourceTag);
class AsyncWebServer;

// Baut das dynamische HTML-Modul für die Settings-Seite
String buildTedeeModuleHtml();

void tedeeUnlock();
void tedeeLock();
void getTedeeStatus();
void toggleTedeeStatus();
void tedeeHandleSingleOutputAction(const String &action);
void tedee_init();                                   // Initialisiert den Worker-Task und die Queue
void registerTedeeEndpoints(AsyncWebServer &server); // API-Endpoints registrieren

#endif // USE_TEDEE
