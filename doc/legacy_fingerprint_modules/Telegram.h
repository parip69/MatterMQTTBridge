// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#pragma once

#if USE_TELEGRAM

#include "SettingsManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <ESPAsyncWebServer.h>

// Telegram-Initialisierung
void telegram_init();

// Telegram-Loop (z.B. für Queue/Async)
void telegram_loop();

// Sende eine Nachricht
void sendTelegramMessage(const String &message);

// API-Endpunkte registrieren
void registerTelegramEndpoints(AsyncWebServer &server);

// HTML-Modul für Telegram (SSE)
String buildTelegramModuleHtml();
#endif // USE_TELEGRAM
