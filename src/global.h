// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#pragma once

#include <WString.h>
#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include "freertos/semphr.h"

// Compile‑time Logging: -D LOGGING=1 (an) / -D LOGGING=0 (aus)
#ifndef LOGGING
#define LOGGING 1
#endif

#if LOGGING
#define LOG_PRINT(...) Serial.print(__VA_ARGS__)
#define LOG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define LOG_PRINTF(...) Serial.printf(__VA_ARGS__)
#define LOG_NL() Serial.println()
#else
#define LOG_PRINT(...)
#define LOG_PRINTLN(...)
#define LOG_PRINTF(...)
#define LOG_NL()
#endif

// ---------- Externe Symbole ----------
extern SemaphoreHandle_t networkMutex;
extern void notifyClients(String message, const char *sourceTag = nullptr);
extern String getTimestampString();
extern bool isBootNetworkGraceActive();
extern bool hasActiveWebClients();
extern AsyncEventSource events;
extern volatile bool timeSet; // Globale Zeit-Synchronisation-Flag

// --- MQTT Topic-Helper (Definition in main.cpp)
// Liefert "broker" / "client" / "off"
String getModeString();
// Baut "<root>/<tail>"
String makeTopic(const String &tail);

