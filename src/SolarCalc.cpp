// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#include "SolarCalc.h"
#include "global.h"
#include <Arduino.h>
#include "SettingsManager.h"
#include <time.h>
#include <esp_task_wdt.h>

// SettingsManager settingsManager;
SolarCalc solarCalc;
extern SettingsManager settingsManager;
// tetsen  ///////////////////////////////////

// Variable für den letzten Berechnungstag
int lastCalculationDay = -1;
// Variable für den letzten Loop-Zeitpunkt

// P3-07: Globale Variable entfernt — nutze stattdessen den private Member SolarCalc::statusCheckLighting

// Ihre Koordinaten
SolarCalc::SolarCalc() : Breite(0.0), Laenge(0.0), Zone(0), statusCheckLighting(false) {}

// Ihre Funktionen hier
inline float sonnendeklination(int T)
{
    return 0.409526325277017 * sin(0.0169060504029192 * (T - 80.0856919827619));
}

inline float zeitdifferenz(float Deklination, float B)
{
    return 12.0 * acos((sin(-(50.0 / 60.0) * M_PI / 180.0) - sin(B) * sin(Deklination)) / (cos(B) * cos(Deklination))) / M_PI;
}

inline float zeitgleichung(int T)
{
    return -0.170869921174742 * sin(0.0336997028793971 * T + 0.465419984181394) - 0.129890681040717 * sin(0.0178674832556871 * T - 0.167936777524864);
}

float aufgang(int T, float B)
{
    float DK = sonnendeklination(T);
    return 12 - zeitdifferenz(DK, B) - zeitgleichung(T);
}

float untergang(int T, float B)
{
    float DK = sonnendeklination(T);
    return 12 + zeitdifferenz(DK, B) - zeitgleichung(T);
}

void SolarCalc::updateSunriseSunset()
{
    // Lese sunriseOffset und sunsetOffset aus den App-Einstellungen (nur 1x holen)
    const AppSettings app = settingsManager.getAppSettings();
    String sunriseOffsetStr = app.sunriseOffset;
    String sunsetOffsetStr = app.sunsetOffset;

    struct tm timeinfo;
    // Holen Sie die lokale Zeit - nur wenn NTP synchronisiert ist
    if (!timeSet || !getLocalTime(&timeinfo))
    {
        LOG_PRINTLN("SolarCalc::updateSunriseSunset - Zeit konnte nicht geladen werden oder NTP nicht synchronisiert.");
        return;
    }

    const float B = Breite * M_PI / 180.0; // geogr. Breite in Radians

    int T = timeinfo.tm_yday + 1;

    // P3-06: Timezone korrekt berechnen (auch für negative Zeitzonen und Tagesgrenzen)
    time_t now = time(NULL);
    struct tm utcBuf;
    gmtime_r(&now, &utcBuf);
    int diff = timeinfo.tm_hour - utcBuf.tm_hour;
    int dayDiff = timeinfo.tm_yday - utcBuf.tm_yday;
    if (dayDiff > 1) dayDiff = -1;   // Jahresgrenze: z.B. yday 0 vs 364 → UTC voraus
    if (dayDiff < -1) dayDiff = 1;   // Jahresgrenze: z.B. yday 364 vs 0 → Lokal voraus
    diff += dayDiff * 24;
    Zone = diff;

    // Überprüfe, ob sunriseOffsetStr im Format "HH:MM" oder "HHMM" vorliegt
    if (sunriseOffsetStr.indexOf(':') == -1) // Kein Doppelpunkt, daher im Format "HH" oder "-HH"
    {
        // Vorzeichen erkennen und entfernen, wenn negativ
        bool negative = sunriseOffsetStr.startsWith("-");
        if (negative)
            sunriseOffsetStr = sunriseOffsetStr.substring(1);

        // Auf max. zwei Zeichen kürzen, um nur Stunden zu verarbeiten
        sunriseOffsetStr = sunriseOffsetStr.substring(0, sunriseOffsetStr.length() > 2 ? 2 : sunriseOffsetStr.length());

        // Vorzeichen wieder hinzufügen, falls negativ
        if (negative)
            sunriseOffsetStr = "-" + sunriseOffsetStr;

        // Offset in Stunden berechnen und hinzufügen (in Minuten konvertiert)
        float sunriseOffset = sunriseOffsetStr.toFloat();
        float Aufgang = fmod(aufgang(T, B) - Laenge / 15.0 + Zone + sunriseOffset / 60, 24.0);
        if (Aufgang < 0)
            Aufgang += 24.0; // Negative Werte korrigieren

        // Uhrzeit im Format "HH:MM" erstellen (String-basiert ohne sprintf)
        int sunriseHour = (int)Aufgang;
        int sunriseMin = (int)((Aufgang - sunriseHour) * 60.0 + 0.5);
        if (sunriseMin >= 60)
        {
            sunriseMin -= 60;
            sunriseHour = (sunriseHour + 1) % 24;
        }
        String sunrise = (sunriseHour < 10 ? String("0") : String("")) + String(sunriseHour) + ":" +
                         (sunriseMin < 10 ? String("0") : String("")) + String(sunriseMin);
        solarCalc.Sunrise = sunrise;
    }
    else // Format "HH:MM" ist vorhanden
    {
        // Uhrzeit unverändert übernehmen und in "HH:MM" formatieren (String-basiert)
        int h = (int)sunriseOffsetStr.substring(0, 2).toInt();
        int m = (int)sunriseOffsetStr.substring(3).toInt();
        String sunriseFormatted = (h < 10 ? String("0") : String("")) + String(h) + ":" +
                                  (m < 10 ? String("0") : String("")) + String(m);
        solarCalc.Sunrise = sunriseFormatted;
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Überprüfe, ob sunsetOffsetStr im Format "HH:MM" oder "HHMM" vorliegt
    if (sunsetOffsetStr.indexOf(':') == -1) // Kein Doppelpunkt, daher im Format "HH" oder "-HH"
    {
        // Vorzeichen erkennen und entfernen, wenn negativ
        bool negative = sunsetOffsetStr.startsWith("-");
        if (negative)
            sunsetOffsetStr = sunsetOffsetStr.substring(1);

        // Auf max. zwei Zeichen kürzen, um nur Stunden zu verarbeiten
        sunsetOffsetStr = sunsetOffsetStr.substring(0, sunsetOffsetStr.length() > 2 ? 2 : sunsetOffsetStr.length());
        // Vorzeichen wieder hinzufügen, falls negativ
        if (negative)
            sunsetOffsetStr = "-" + sunsetOffsetStr;

        // Offset in Stunden berechnen und hinzufügen (in Minuten konvertiert)
        float sunsetOffset = sunsetOffsetStr.toFloat();
        float Untergang = fmod(untergang(T, B) - Laenge / 15.0 + Zone + (sunsetOffset / 60.0), 24.0);
        if (Untergang < 0)
            Untergang += 24.0; // Negative Werte korrigieren

        // Uhrzeit im Format "HH:MM" erstellen (String-basiert ohne sprintf)
        int sunsetHour = (int)Untergang;
        int sunsetMin = (int)((Untergang - sunsetHour) * 60.0 + 0.5);
        if (sunsetMin >= 60)
        {
            sunsetMin -= 60;
            sunsetHour = (sunsetHour + 1) % 24;
        }
        String sunset = (sunsetHour < 10 ? String("0") : String("")) + String(sunsetHour) + ":" +
                        (sunsetMin < 10 ? String("0") : String("")) + String(sunsetMin);
        solarCalc.Sunset = sunset;
    }
    else // Format "HH:MM" ist vorhanden
    {
        // Uhrzeit unverändert übernehmen und in "HH:MM" formatieren (String-basiert)
        int h2 = (int)sunsetOffsetStr.substring(0, 2).toInt();
        int m2 = (int)sunsetOffsetStr.substring(3).toInt();
        String sunsetFormatted = (h2 < 10 ? String("0") : String("")) + String(h2) + ":" +
                                 (m2 < 10 ? String("0") : String("")) + String(m2);
        solarCalc.Sunset = sunsetFormatted;
    }
    checkLighting(); // Die Beleuchtung überprüfen
}

void SolarCalc::checkLighting()
{
    // Überprüfen, ob die Sonnenberechnung aktiv ist
    if (!statusCheckLighting)
    {
        return; // Frühes Beenden, wenn die Funktion deaktiviert ist
    }

    // Prüfe zunächst, ob timeSet gesetzt ist (NTP erfolgreich)
    if (!timeSet)
    {
        return;
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        LOG_PRINTLN("SolarCalc::checkLighting - Fehler beim Abrufen der lokalen Zeit");
        return;
    }

    // Validiere die Sonnenauf- und -untergangszeiten
    if (solarCalc.Sunrise.isEmpty() || solarCalc.Sunset.isEmpty())
    {
        LOG_PRINTLN("Sonnenauf- oder -untergangszeit nicht verfügbar");
        return;
    }

    char currentTime[6];
    snprintf(currentTime, sizeof(currentTime), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    // Überprüfe, ob die aktuelle Zeit zwischen Sonnenaufgang und -untergang liegt
    bool isDaytime = false;

    // Berechnung ohne Exceptions (fno-exceptions Build)
    if (strcmp(solarCalc.Sunset.c_str(), solarCalc.Sunrise.c_str()) > 0)
    {
        // Normaler Fall: Sonnenaufgang vor Sonnenuntergang am selben Tag
        isDaytime = (strcmp(currentTime, solarCalc.Sunrise.c_str()) >= 0 &&
                     strcmp(currentTime, solarCalc.Sunset.c_str()) <= 0);
    }
    else
    {
        // Spezialfall: Sonnenuntergang vor Sonnenaufgang (über Mitternacht)
        isDaytime = (strcmp(currentTime, solarCalc.Sunrise.c_str()) >= 0 ||
                     strcmp(currentTime, solarCalc.Sunset.c_str()) <= 0);
    }

    // Beleuchtung steuern
    int currentState = digitalRead(LedBeleuchtung);
    int newState = isDaytime ? LOW : HIGH;

    if (currentState != newState)
    {
        digitalWrite(LedBeleuchtung, newState);
        String message = isDaytime ? "Es ist Tag. Die Klingelbeleuchtung wurde ausgeschaltet." : "Es ist Nacht. Die Klingelbeleuchtung wurde eingeschaltet.";
        notifyClients(message);
        LOG_PRINTF("%s Aktuelle Zeit: %s, Sonnenaufgang: %s, Sonnenuntergang: %s\n",
                   message.c_str(), currentTime, solarCalc.Sunrise.c_str(), solarCalc.Sunset.c_str());
    }
}

void SolarCalc::setup()
{
    const AppSettings app = settingsManager.getAppSettings();
    // Lese Werte aus den Einstellungen und konvertiere sie.
    Breite = app.latitude.toFloat();
    Laenge = app.longitude.toFloat();

    if (app.latitude.isEmpty() || app.longitude.isEmpty())
    {
        LOG_PRINTLN("📍 Geokoordinaten sind NICHT eingerichtet (werden später über WebSocket gemeldet)");
    }
    else
    {
        LOG_PRINTLN("📍 Geokoordinaten SIND eingerichtet (werden später über WebSocket gemeldet)");
        // updateSunriseSunset() wird erst in loop() aufgerufen, wenn Zeit verfügbar ist
    }

    if (app.sunriseOffset.isEmpty() || app.sunsetOffset.isEmpty())
    {
        LOG_PRINTLN("💡 LED-Beleuchtung ist deaktiviert (wird später über WebSocket gemeldet)");
        statusCheckLighting = false;
    }
    else
    {
        LOG_PRINTLN("💡 LED-Beleuchtung ist aktiviert (wird später über WebSocket gemeldet)");
        statusCheckLighting = true;
        // updateSunriseSunset() wird erst in loop() aufgerufen, wenn Zeit verfügbar ist
    }
}

void SolarCalc::loop()
{
    static int lastMinute = -1;
    static int errorCount = 0;
    static bool solarCalcInitialized = false;       // Einmalige Initialisierung
    static bool webSocketNotificationsSent = false; // Einmalige WebSocket-Nachrichten
    const int maxErrors = 5;
    const AppSettings app = settingsManager.getAppSettings();

    // Prüfe zunächst, ob timeSet gesetzt ist (NTP erfolgreich)
    if (!timeSet)
    {
        // Wenn Zeit noch nicht synchronisiert ist, warte ab
        return;
    }

    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        errorCount = 0; // Fehler zurücksetzen bei erfolgreicher Zeit-Abfrage

        // Einmalige WebSocket-Benachrichtigungen senden, wenn Zeit verfügbar ist
        if (!webSocketNotificationsSent)
        {
            if (app.latitude.isEmpty() || app.longitude.isEmpty())
            {
                notifyClients("Die Geokoordinaten sind NICHT eingerichtet.");
            }
            else
            {
                notifyClients("Die Geokoordinaten SIND eingerichtet.");
            }

            if (app.sunriseOffset.isEmpty() || app.sunsetOffset.isEmpty())
            {
                notifyClients("Die LED-Beleuchtung ist deaktiviert.");
            }
            else
            {
                notifyClients("Die LED-Beleuchtung ist aktiviert.");
            }
            webSocketNotificationsSent = true;
            LOG_PRINTLN("📡 SolarCalc: WebSocket-Benachrichtigungen nach Zeit-Sync gesendet");
        }

        // Einmalige Initialisierung nach der ersten erfolgreichen Zeit-Synchronisation
        if (!solarCalcInitialized && statusCheckLighting &&
            !app.latitude.isEmpty() &&
            !app.longitude.isEmpty())
        {
            LOG_PRINTLN("SolarCalc: Initialisiere Sonnenauf-/untergangsberechnung nach Zeit-Sync...");
            updateSunriseSunset();
            solarCalcInitialized = true;
        }

        if (timeinfo.tm_min != lastMinute)
        {
            lastMinute = timeinfo.tm_min; // Speichere die aktuelle Minute
            checkLighting();              // Beleuchtungsstatus überprüfen

            if (timeinfo.tm_yday != lastCalculationDay)
            {
                lastCalculationDay = timeinfo.tm_yday;
                LOG_PRINT("Die Berechnung findet nur einmal am Tag statt. Tag: ");
                LOG_PRINTLN(timeinfo.tm_yday);
                updateSunriseSunset(); // Berechnung durchführen
            }
        }
    }
    else
    {
        esp_task_wdt_reset(); // Watchdog zurücksetzen
        errorCount++;
        if (errorCount > maxErrors)
        {
                // Variante A: KEIN Neustart. SolarCalc pausiert, bis getLocalTime() wieder funktioniert.
                if (errorCount == (maxErrors + 1))
                {
                    notifyClients("⚠️ SolarCalc: Zeit nicht verfügbar (SolarCalc pausiert – kein Neustart)");
                    LOG_PRINTLN("SolarCalc: Zeit nicht verfügbar -> SolarCalc pausiert (kein Neustart)");
                }
                errorCount = maxErrors + 1; // deckeln, damit kein Spam
                return;
        }
        else
        {
            LOG_PRINTLN("SolarCalc: Warnung - Konnte lokale Zeit nicht laden. Versuch " + String(errorCount) + " von " + String(maxErrors));
            // Reduzierte Warnung ohne notifyClients um Spam zu vermeiden
        }
    }
}
