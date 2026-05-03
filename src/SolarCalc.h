// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#pragma once

#include "global.h"
#include <Arduino.h>

class SolarCalc
{
public:
  SolarCalc();
  void setup();
  void loop();
  void updateSunriseSunset(); // Diese Zeile hinzufÃ¼gen
  void checkLighting();

  String Sunrise;
  String Sunset;

private:
  float Breite;
  float Laenge;
  int Zone;
  bool statusCheckLighting;
};

// double latitude = 48.777452;    // Deine geographische Breite
// double longitude = 11.619658;  // Deine geographische LÃ¤nge

/****************************************************

//meine SolarCalc.h datei
#pragma once

class SolarCalc {
public:
  void setup();
  void loop();


private:
};

// Deklaration der globalen Instanz mit extern
extern SolarCalc solarCalc;

/////////////////////////////////////////////////////
// meiene SolarCalc.cpp datei
#include <Arduino.h>
// Erstellen Sie die Instanz hier
SolarCalc solarCalc;

// Speichern Sie die aktuelle Zeit
time_t lastCalculation = 0;

void SolarCalc::setup() {
    // Ihr Setup-Code hier
    LOG_PRINTLN("bin in der setup  >>>>>>>>>>> setup");
}

void SolarCalc::loop() {
        time_t now = time(0);
        if (difftime(now, lastCalculation) >= 24*60*60) {
            // Ein Tag ist vergangen, fÃ¼hren Sie die Berechnung erneut aus
            LOG_PRINTLN("bin in der Loop >>>>>>>>>>> Loop");
    lastCalculation = now;
        }
    }
///////////////////////////////////////////////

****************************************************/
