// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#include "SettingsManager.h"
#include "global.h"

bool SettingsManager::loadWifiSettings()
{
    Preferences preferences;
    if (!preferences.begin("fs_wifi", true))
        return false;

    WifiSettings loaded;
    loaded.ssid = preferences.getString("ssid", loaded.ssid);
    loaded.password = preferences.getString("password", loaded.password);
    loaded.hostname = preferences.getString("hostname", loaded.hostname);
    loaded.passwordAdmin = preferences.getString("passwordAdmin", loaded.passwordAdmin);
    loaded.hostname.trim();

    preferences.end();

    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        wifiSettings = loaded;
        xSemaphoreGive(settingsMutex);
    }
    else
    {
        wifiSettings = loaded;
    }

    return true;
}

bool SettingsManager::loadAppSettings()
{
    Preferences preferences;
    if (!preferences.begin("fs_app", true))
        return false;

    AppSettings loaded;

    loaded.mqtt_isBroker = preferences.getBool("mqtt_isBroker", loaded.mqtt_isBroker);
    loaded.mqtt_isClient = preferences.getBool("mqtt_isClient", loaded.mqtt_isClient);
    loaded.mqtt_port = preferences.getString("mqtt_port", loaded.mqtt_port);
    loaded.mqtt_keepAlive = preferences.getString("mqtt_keepAlive", loaded.mqtt_keepAlive);
    loaded.mqttServer = preferences.getString("mqttServer", loaded.mqttServer);
    loaded.mqttUsername = preferences.getString("mqttUsername", loaded.mqttUsername);
    loaded.mqttPassword = preferences.getString("mqttPassword", loaded.mqttPassword);
    loaded.mqttRootTopic = preferences.getString("mqttRootTopic", loaded.mqttRootTopic);
    loaded.ntpServer = preferences.getString("ntpServer", loaded.ntpServer);
    loaded.ntpOffset = preferences.getString("ntpOffset", loaded.ntpOffset);
    loaded.passwordSetup = preferences.getString("passwordSetup", loaded.passwordSetup);
    loaded.enablePassword = preferences.getString("enablePassword", loaded.enablePassword);

    loaded.wifiRssiDisconnectThreshold = preferences.getInt("wifiRssiD", loaded.wifiRssiDisconnectThreshold);
    loaded.wifiRoamImproveDb = normalizeWifiRoamImproveDb(preferences.getInt("wifiRoamImp", loaded.wifiRoamImproveDb));
    loaded.wifiRoamMinRssi = normalizeWifiRoamMinRssi(preferences.getInt("wifiRoamMin", loaded.wifiRoamMinRssi));

    preferences.end();

    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        appSettings = loaded;
        xSemaphoreGive(settingsMutex);
    }
    else
    {
        appSettings = loaded;
    }

    return true;
}

void SettingsManager::saveWifiSettings()
{
    WifiSettings localCopy;
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        localCopy = wifiSettings;
        xSemaphoreGive(settingsMutex);
    }
    else
    {
        localCopy = wifiSettings;
    }

    Preferences preferences;
    if (!preferences.begin("fs_wifi", false))
        return;

    localCopy.hostname.trim();
    preferences.putString("ssid", localCopy.ssid);
    preferences.putString("password", localCopy.password);
    preferences.putString("hostname", localCopy.hostname);
    preferences.putString("passwordAdmin", localCopy.passwordAdmin);
    preferences.end();
}

void SettingsManager::saveAppSettings()
{
    AppSettings localCopy;
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        localCopy = appSettings;
        xSemaphoreGive(settingsMutex);
    }
    else
    {
        localCopy = appSettings;
    }

    Preferences preferences;
    if (!preferences.begin("fs_app", false))
        return;

    preferences.putBool("mqtt_isBroker", localCopy.mqtt_isBroker);
    preferences.putBool("mqtt_isClient", localCopy.mqtt_isClient);
    preferences.putString("mqtt_port", localCopy.mqtt_port);
    preferences.putString("mqtt_keepAlive", localCopy.mqtt_keepAlive);
    preferences.putString("mqttServer", localCopy.mqttServer);
    preferences.putString("mqttUsername", localCopy.mqttUsername);
    preferences.putString("mqttPassword", localCopy.mqttPassword);
    preferences.putString("mqttRootTopic", localCopy.mqttRootTopic);
    preferences.putString("ntpServer", localCopy.ntpServer);
    preferences.putString("ntpOffset", localCopy.ntpOffset);
    preferences.putString("passwordSetup", localCopy.passwordSetup);
    preferences.putString("enablePassword", localCopy.enablePassword);

    preferences.putInt("wifiRssiD", localCopy.wifiRssiDisconnectThreshold);
    localCopy.wifiRoamImproveDb = normalizeWifiRoamImproveDb(localCopy.wifiRoamImproveDb);
    localCopy.wifiRoamMinRssi = normalizeWifiRoamMinRssi(localCopy.wifiRoamMinRssi);
    preferences.putInt("wifiRoamImp", localCopy.wifiRoamImproveDb);
    preferences.putInt("wifiRoamMin", localCopy.wifiRoamMinRssi);

    preferences.end();
}

WifiSettings SettingsManager::getWifiSettings()
{
    WifiSettings copy;
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        copy = wifiSettings;
        xSemaphoreGive(settingsMutex);
    }
    else
    {
        copy = wifiSettings;
    }
    return copy;
}

void SettingsManager::saveWifiSettings(const WifiSettings& newSettings)
{
    WifiSettings normalized = newSettings;
    normalized.hostname.trim();

    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        wifiSettings = normalized;
        xSemaphoreGive(settingsMutex);
    }
    else
    {
        wifiSettings = normalized;
    }

    saveWifiSettings();
}

// P2-13: Copy-on-Read mit Mutex — sicher für Multi-Core (AsyncWebServer auf Core 0, loop auf Core 1)
AppSettings SettingsManager::getAppSettings()
{
    AppSettings copy;
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        copy = appSettings;
        xSemaphoreGive(settingsMutex);
    }
    else
    {
        copy = appSettings; // Fallback ohne Lock (Mutex noch nicht initialisiert / Timeout)
    }
    return copy;
}

void SettingsManager::saveAppSettings(const AppSettings& newSettings)
{
    AppSettings normalized = newSettings;
    normalized.wifiRoamImproveDb = normalizeWifiRoamImproveDb(normalized.wifiRoamImproveDb);
    normalized.wifiRoamMinRssi = normalizeWifiRoamMinRssi(normalized.wifiRoamMinRssi);

    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        appSettings = normalized;
        xSemaphoreGive(settingsMutex);
    }
    else
    {
        appSettings = normalized; // Fallback
    }
    saveAppSettings();
}

bool SettingsManager::isWifiConfigured()
{
    WifiSettings current = getWifiSettings();
    if (current.ssid.isEmpty() || current.password.isEmpty())
        return false;
    else
        return true;
}

bool SettingsManager::deleteAppSettings()
{
    Preferences preferences;
    bool rc = preferences.begin("fs_app", false);
    if (rc)
        rc = preferences.clear();
    preferences.end();
    return rc;
}

bool SettingsManager::deleteWifiSettings()
{
    Preferences preferences;
    bool rc = preferences.begin("fs_wifi", false);
    if (rc)
        rc = preferences.clear();
    preferences.end();
    return rc;
}

