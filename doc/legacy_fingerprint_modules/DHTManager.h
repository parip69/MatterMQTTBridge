// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#pragma once

#if USE_DHT

#include "global.h"
#include <Arduino.h>
#include <DHTesp.h>
#include <ESPAsyncWebServer.h>

class DHTManager
{
public:
    DHTManager();

    void setup(AsyncEventSource *events);
    void loop(bool isClientOnline);
    void retryIfDisabled(bool isClientOnline = true);

    String getTemperature() const;
    String getHumidity() const;
    String getDewPoint() const;
    String getHeatIndex() const;
    bool isSensorConnected() const;

private:
    bool checkDHT22Sensor(bool verbose = false);

    DHTesp dht;
    AsyncEventSource *_events;

    bool _isSensorConnected;
    unsigned long _lastReadTime;
    bool _dhtHasAttempted;
    bool _dhtDisabled;
    uint8_t _readFailCount;

    String _temperature;
    String _humidity;
    String _dewPoint;
    String _heatIndex;

    static const unsigned long READ_INTERVAL = 60000; // 60 seconds
    static const int DHT_PIN = 25;
};

#endif // USE_DHT
