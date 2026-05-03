// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉

#if USE_DHT

#include "DHTManager.h"

DHTManager::DHTManager() : _events(nullptr),
                           _isSensorConnected(false),
                           _lastReadTime(0),
                           _dhtHasAttempted(false),
                           _dhtDisabled(false),
                           _readFailCount(0),
                           _temperature("--.-"),
                           _humidity("--.-"),
                           _dewPoint("--.-"),
                           _heatIndex("--.-")
{
}

void DHTManager::setup(AsyncEventSource *events)
{
    _events = events;
    dht.setup(DHT_PIN, DHTesp::DHT22);
    LOG_PRINTLN("🌡️ DHTManager: Initialized. First read will happen on first loop call.");
}

void DHTManager::loop(bool isClientOnline)
{
    if (_dhtDisabled)
    {
        return;
    }

    if (!_dhtHasAttempted)
    {
        _dhtHasAttempted = true;
        bool initialStatus = checkDHT22Sensor(true);

        if (!initialStatus)
        {
            _isSensorConnected = false;
            _dhtDisabled = true;
            _temperature = "--.-";
            _humidity = "--.-";
            _dewPoint = "--.-";
            _heatIndex = "--.-";

            LOG_PRINTLN("⚠️ DHT22 not detected on first loop check - DHT disabled until next restart.");

            if (_events && isClientOnline)
            {
                _events->send("false", "isSensorConnected", millis());
                _events->send(_temperature.c_str(), "temperature", millis());
                _events->send(_humidity.c_str(), "humidity", millis());
                _events->send(_dewPoint.c_str(), "dewPoint", millis());
                _events->send(_heatIndex.c_str(), "heatIndex", millis());
            }
            return;
        }

        _isSensorConnected = true;
        _readFailCount = 0;
        _lastReadTime = millis();

        LOG_PRINTLN("🌡️ DHT22 detected on first loop check.");

        if (_events && isClientOnline)
        {
            _events->send("true", "isSensorConnected", millis());
            _events->send(_temperature.c_str(), "temperature", millis());
            _events->send(_humidity.c_str(), "humidity", millis());
            _events->send(_dewPoint.c_str(), "dewPoint", millis());
            _events->send(_heatIndex.c_str(), "heatIndex", millis());
        }
        return;
    }

    if (!_isSensorConnected)
    {
        return;
    }

    if (millis() - _lastReadTime < READ_INTERVAL)
    {
        return;
    }

    _lastReadTime = millis();

    TempAndHumidity data = dht.getTempAndHumidity();
    bool valid = (!isnan(data.temperature) && !isnan(data.humidity) &&
                  data.temperature > -40 && data.temperature < 80 &&
                  data.humidity >= 0 && data.humidity <= 100);

    if (!valid)
    {
        if (_readFailCount < 255)
        {
            _readFailCount++;
        }

        _temperature = "--.-";
        _humidity = "--.-";
        _dewPoint = "--.-";
        _heatIndex = "--.-";

        LOG_PRINTLN("⚠️ DHT22 invalid read.");

        if (_events && isClientOnline)
        {
            _events->send(_temperature.c_str(), "temperature", millis());
            _events->send(_humidity.c_str(), "humidity", millis());
            _events->send(_dewPoint.c_str(), "dewPoint", millis());
            _events->send(_heatIndex.c_str(), "heatIndex", millis());
        }

        if (_readFailCount >= 2)
        {
            _isSensorConnected = false;
            _dhtDisabled = true;

            LOG_PRINTLN("⚠️ DHT22 invalid twice in a row - DHT disabled until next restart.");

            if (_events && isClientOnline)
            {
                _events->send("false", "isSensorConnected", millis());
            }
        }
        return;
    }

    _readFailCount = 0;
    _temperature = String(data.temperature, 1);
    _humidity = String(data.humidity, 1);
    _dewPoint = String(dht.computeDewPoint(data.temperature, data.humidity), 1);
    _heatIndex = String(dht.computeHeatIndex(data.temperature, data.humidity, false), 1);

    if (_events && isClientOnline)
    {
        _events->send(_temperature.c_str(), "temperature", millis());
        _events->send(_humidity.c_str(), "humidity", millis());
        _events->send(_dewPoint.c_str(), "dewPoint", millis());
        _events->send(_heatIndex.c_str(), "heatIndex", millis());
    }
}

void DHTManager::retryIfDisabled(bool isClientOnline)
{
    if (!_dhtDisabled)
    {
        return;
    }

    LOG_PRINTLN("🔄 DHT22: Retry durch Webinterface-Refresh...");

    dht.setup(DHT_PIN, DHTesp::DHT22);

    _dhtDisabled = false;
    _readFailCount = 0;
    _dhtHasAttempted = true;

    bool ok = checkDHT22Sensor(true);

    if (ok)
    {
        _isSensorConnected = true;
        _dhtDisabled = false;
        LOG_PRINTLN("✅ DHT22: Retry erfolgreich, Sensor wieder aktiv.");
    }
    else
    {
        _isSensorConnected = false;
        _dhtDisabled = true;

        _temperature = "--.-";
        _humidity = "--.-";
        _dewPoint = "--.-";
        _heatIndex = "--.-";

        LOG_PRINTLN("❌ DHT22: Retry fehlgeschlagen, Sensor bleibt deaktiviert.");
    }

    if (_events && isClientOnline)
    {
        _events->send(_isSensorConnected ? "true" : "false", "isSensorConnected", millis());
        _events->send(_temperature.c_str(), "temperature", millis());
        _events->send(_humidity.c_str(), "humidity", millis());
        _events->send(_dewPoint.c_str(), "dewPoint", millis());
        _events->send(_heatIndex.c_str(), "heatIndex", millis());
    }
}

bool DHTManager::checkDHT22Sensor(bool verbose)
{
    if (verbose)
    {
        LOG_PRINTLN("🔍 Checking DHT22 sensor...");
    }

    TempAndHumidity data = dht.getTempAndHumidity();
    bool isValid = (!isnan(data.temperature) && !isnan(data.humidity) &&
                    data.temperature > -40 && data.temperature < 80 &&
                    data.humidity >= 0 && data.humidity <= 100);

    if (isValid)
    {
        _isSensorConnected = true;
        _temperature = String(data.temperature, 1);
        _humidity = String(data.humidity, 1);
        _dewPoint = String(dht.computeDewPoint(data.temperature, data.humidity), 1);
        _heatIndex = String(dht.computeHeatIndex(data.temperature, data.humidity, false), 1);
        if (verbose)
        {
            LOG_PRINTF("✅ Sensor OK - Temp: %s°C, Hum: %s%%\n", _temperature.c_str(), _humidity.c_str());
        }
    }
    else
    {
        _isSensorConnected = false;
        if (verbose)
        {
            LOG_PRINTLN("❌ Sensor error or invalid values.");
        }
    }
    return isValid;
}

String DHTManager::getTemperature() const
{
    return _temperature;
}

String DHTManager::getHumidity() const
{
    return _humidity;
}

String DHTManager::getDewPoint() const
{
    return _dewPoint;
}

String DHTManager::getHeatIndex() const
{
    return _heatIndex;
}

bool DHTManager::isSensorConnected() const
{
    return _isSensorConnected;
}

#endif // USE_DHT
