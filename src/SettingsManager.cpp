// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#include "SettingsManager.h"
<<<<<<< HEAD
=======
#include "global.h"
>>>>>>> 937c36a21b29111ed2f16fbabeffa236a6d16872

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
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        wifiSettings = loaded; xSemaphoreGive(settingsMutex);
    } else {
        wifiSettings = loaded;
    }
    return true;
}

bool SettingsManager::loadAppSettings()
{
    Preferences preferences;
    if (!preferences.begin("fs_app", true)) return false;

    AppSettings loaded;
    loaded.mqtt_isBroker = preferences.getBool("mqtt_isBroker", loaded.mqtt_isBroker);
    loaded.mqtt_isClient = preferences.getBool("mqtt_isClient", loaded.mqtt_isClient);
    loaded.mqtt_port = preferences.getString("mqtt_port", loaded.mqtt_port);
    loaded.mqtt_keepAlive = preferences.getString("mqtt_keepAlive", loaded.mqtt_keepAlive);
    loaded.mqttServer = preferences.getString("mqttServer", loaded.mqttServer);
    loaded.mqttUsername = preferences.getString("mqttUsername", loaded.mqttUsername);
    loaded.mqttPassword = preferences.getString("mqttPassword", loaded.mqttPassword);
    
    String rt = preferences.getString("mqttRootTopic", loaded.mqttRootTopic);
    if(rt.isEmpty()) rt = "fingerprint";
    loaded.mqttRootTopic = rt;
    
    loaded.ntpServer = preferences.getString("ntpServer", loaded.ntpServer);
    loaded.ntpOffset = preferences.getString("ntpOffset", loaded.ntpOffset);
    loaded.passwordSetup = preferences.getString("passwordSetup", loaded.passwordSetup);
    loaded.enablePassword = preferences.getString("enablePassword", loaded.enablePassword);
<<<<<<< HEAD
=======

#if USE_TELEGRAM
    loaded.telegram_enabled = preferences.getBool("tg_enabled", loaded.telegram_enabled);
    loaded.telegram_botToken = preferences.getString("tg_botToken", loaded.telegram_botToken);
    loaded.telegram_chatId = preferences.getString("tg_chatId", loaded.telegram_chatId);
#if USE_TELEGRAM_CMD
    loaded.telegram_cmdEnabled = preferences.getBool("tg_cmdEn", loaded.telegram_cmdEnabled);
    loaded.telegram_pollIntervalMs = preferences.getUInt("tg_pollIntv", loaded.telegram_pollIntervalMs);
    if (loaded.telegram_pollIntervalMs < 3000) loaded.telegram_pollIntervalMs = 3000;
    if (loaded.telegram_pollIntervalMs > 120000) loaded.telegram_pollIntervalMs = 120000;
#endif
#endif


    // Legacy-Felder (Phase 2 entfernen)
    loaded.latitude = preferences.getString("latitude", loaded.latitude);
    loaded.longitude = preferences.getString("longitude", loaded.longitude);
    loaded.sunriseOffset = preferences.getString("sunriseOffset", loaded.sunriseOffset);
    loaded.sunsetOffset = preferences.getString("sunsetOffset", loaded.sunsetOffset);
    loaded.toggleButton0 = preferences.getString("toggleButton0", loaded.toggleButton0);
    loaded.toggleButton1 = preferences.getString("toggleButton1", loaded.toggleButton1);
    loaded.toggleButton2 = preferences.getString("toggleButton2", loaded.toggleButton2);
    loaded.toggleButton3 = preferences.getString("toggleButton3", loaded.toggleButton3);
    loaded.toggleButton4 = preferences.getString("toggleButton4", loaded.toggleButton4);
    loaded.toggleButton5 = preferences.getString("toggleButton5", loaded.toggleButton5);
    loaded.toggleButton6 = preferences.getString("toggleButton6", loaded.toggleButton6);
    loaded.delayButton1 = preferences.getString("delayButton1", loaded.delayButton1);
    loaded.delayButton2 = preferences.getString("delayButton2", loaded.delayButton2);
    loaded.delayButton3 = preferences.getString("delayButton3", loaded.delayButton3);
    loaded.delayButton4 = preferences.getString("delayButton4", loaded.delayButton4);
    loaded.delayButton5 = preferences.getString("delayButton5", loaded.delayButton5);
    loaded.delayButton6 = preferences.getString("delayButton6", loaded.delayButton6);
    loaded.sensorPin = preferences.getString("sensorPin", loaded.sensorPin);
    loaded.sensorPairingCode = preferences.getString("pairingCode", loaded.sensorPairingCode);
    loaded.sensorPairingValid = preferences.getBool("pairingValid", loaded.sensorPairingValid);
    loaded.ignorTouchRing = preferences.getBool("ignorTouchRing", loaded.ignorTouchRing);
    loaded.klingelAnAus = preferences.getBool("klingelAnAus", loaded.klingelAnAus);
    loaded.fingerprintScannerEnabled = preferences.getBool("fpEnabled", loaded.fingerprintScannerEnabled);
    loaded.wifiRssiDisconnectThreshold = preferences.getInt("wifiRssiD", loaded.wifiRssiDisconnectThreshold);
    loaded.wifiRoamImproveDb = normalizeWifiRoamImproveDb(preferences.getInt("wifiRoamImp", loaded.wifiRoamImproveDb));
    loaded.wifiRoamMinRssi = normalizeWifiRoamMinRssi(preferences.getInt("wifiRoamMin", loaded.wifiRoamMinRssi));

#if USE_TEDEE
    loaded.tedee_enabled = preferences.getBool("tedee_enabled", loaded.tedee_enabled);
    loaded.tedee_bridge_ip = preferences.getString("tedee_bridge_ip", loaded.tedee_bridge_ip);
    loaded.tedee_lock_id = preferences.getInt("tedee_lock_id", loaded.tedee_lock_id);
    loaded.tedee_port = preferences.getInt("tedee_port", loaded.tedee_port);
    loaded.tedee_token = preferences.getString("tedee_token", loaded.tedee_token);
    loaded.tedee_unlock_single_output_action = preferences.getString("tedee_unlock_single_output_action", loaded.tedee_unlock_single_output_action);
    loaded.tedee_lock_single_output_action = preferences.getString("tedee_lock_single_output_action", loaded.tedee_lock_single_output_action);
#endif

#if USE_NUKI
    loaded.nuki_enabled = preferences.getBool("nuki_enabled", loaded.nuki_enabled);
    loaded.nuki_topic_unlock = preferences.getString("nuki_topic_unlock", loaded.nuki_topic_unlock);
    loaded.nuki_payload_unlock = preferences.getString("nuki_payload_unlock", loaded.nuki_payload_unlock);
    loaded.nuki_topic_lock = preferences.getString("nuki_topic_lock", loaded.nuki_topic_lock);
    loaded.nuki_payload_lock = preferences.getString("nuki_payload_lock", loaded.nuki_payload_lock);
    loaded.nuki_topic_unlatch = preferences.getString("nuki_topic_unlatch", loaded.nuki_topic_unlatch);
    loaded.nuki_payload_unlatch = preferences.getString("nuki_payload_unlatch", loaded.nuki_payload_unlatch);
    loaded.nuki_topic_lockngo = preferences.getString("nuki_topic_lockngo", loaded.nuki_topic_lockngo);
    loaded.nuki_payload_lockngo = preferences.getString("nuki_payload_lockngo", loaded.nuki_payload_lockngo);
    loaded.nuki_topic_lockngo_unlatch = preferences.getString("nuki_topic_lockngo_unlatch", loaded.nuki_topic_lockngo_unlatch);
    loaded.nuki_payload_lockngo_unlatch = preferences.getString("nuki_payload_lockngo_unlatch", loaded.nuki_payload_lockngo_unlatch);
    loaded.nuki_topic_full_lock = preferences.getString("nuki_topic_full_lock", loaded.nuki_topic_full_lock);
    loaded.nuki_payload_full_lock = preferences.getString("nuki_payload_full_lock", loaded.nuki_payload_full_lock);
    loaded.nuki_unlock_single_output_action = preferences.getString("nuki_unlock_single_output_action", loaded.nuki_unlock_single_output_action);
    loaded.nuki_lock_single_output_action = preferences.getString("nuki_lock_single_output_action", loaded.nuki_lock_single_output_action);
    loaded.nuki_unlatch_single_output_action = preferences.getString("nuki_unlatch_single_output_action", loaded.nuki_unlatch_single_output_action);
    loaded.nuki_lockngo_single_output_action = preferences.getString("nuki_lockngo_single_output_action", loaded.nuki_lockngo_single_output_action);
    loaded.nuki_lockngo_unlatch_single_output_action = preferences.getString("nuki_lockngo_unlatch_single_output_action", loaded.nuki_lockngo_unlatch_single_output_action);
    loaded.nuki_full_lock_single_output_action = preferences.getString("nuki_full_lock_single_output_action", loaded.nuki_full_lock_single_output_action);
#endif
>>>>>>> 937c36a21b29111ed2f16fbabeffa236a6d16872

    preferences.end();
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        appSettings = loaded; xSemaphoreGive(settingsMutex);
    } else {
        appSettings = loaded;
    }
    return true;
}

void SettingsManager::saveWifiSettings()
{
    WifiSettings localCopy;
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        localCopy = wifiSettings; xSemaphoreGive(settingsMutex);
    } else {
        localCopy = wifiSettings;
    }

    Preferences preferences;
    if (!preferences.begin("fs_wifi", false)) return;
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
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        localCopy = appSettings; xSemaphoreGive(settingsMutex);
    } else {
        localCopy = appSettings;
    }

    Preferences preferences;
    if (!preferences.begin("fs_app", false)) return;
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
<<<<<<< HEAD
=======


#if USE_TELEGRAM
    preferences.putBool("tg_enabled", localCopy.telegram_enabled);
    preferences.putString("tg_botToken", localCopy.telegram_botToken);
    preferences.putString("tg_chatId", localCopy.telegram_chatId);
#if USE_TELEGRAM_CMD
    preferences.putBool("tg_cmdEn", localCopy.telegram_cmdEnabled);
    preferences.putUInt("tg_pollIntv", localCopy.telegram_pollIntervalMs);
#endif
#endif


    // Legacy-Felder (Phase 2 entfernen)
    preferences.putString("latitude", localCopy.latitude);
    preferences.putString("longitude", localCopy.longitude);
    preferences.putString("sunriseOffset", localCopy.sunriseOffset);
    preferences.putString("sunsetOffset", localCopy.sunsetOffset);
    preferences.putString("toggleButton0", localCopy.toggleButton0);
    preferences.putString("toggleButton1", localCopy.toggleButton1);
    preferences.putString("toggleButton2", localCopy.toggleButton2);
    preferences.putString("toggleButton3", localCopy.toggleButton3);
    preferences.putString("toggleButton4", localCopy.toggleButton4);
    preferences.putString("toggleButton5", localCopy.toggleButton5);
    preferences.putString("toggleButton6", localCopy.toggleButton6);
    preferences.putString("delayButton1", localCopy.delayButton1);
    preferences.putString("delayButton2", localCopy.delayButton2);
    preferences.putString("delayButton3", localCopy.delayButton3);
    preferences.putString("delayButton4", localCopy.delayButton4);
    preferences.putString("delayButton5", localCopy.delayButton5);
    preferences.putString("delayButton6", localCopy.delayButton6);
    preferences.putString("sensorPin", localCopy.sensorPin);
    preferences.putString("pairingCode", localCopy.sensorPairingCode);
    preferences.putBool("pairingValid", localCopy.sensorPairingValid);
    preferences.putBool("ignorTouchRing", localCopy.ignorTouchRing);
    preferences.putBool("klingelAnAus", localCopy.klingelAnAus);
    preferences.putBool("fpEnabled", localCopy.fingerprintScannerEnabled);
    preferences.putInt("wifiRssiD", localCopy.wifiRssiDisconnectThreshold);
    localCopy.wifiRoamImproveDb = normalizeWifiRoamImproveDb(localCopy.wifiRoamImproveDb);
    localCopy.wifiRoamMinRssi = normalizeWifiRoamMinRssi(localCopy.wifiRoamMinRssi);
    preferences.putInt("wifiRoamImp", localCopy.wifiRoamImproveDb);
    preferences.putInt("wifiRoamMin", localCopy.wifiRoamMinRssi);

#if USE_TEDEE
    preferences.putBool("tedee_enabled", localCopy.tedee_enabled);
    preferences.putString("tedee_bridge_ip", localCopy.tedee_bridge_ip);
    preferences.putInt("tedee_port", localCopy.tedee_port);
    preferences.putInt("tedee_lock_id", localCopy.tedee_lock_id);
    preferences.putString("tedee_token", localCopy.tedee_token);
    preferences.putString("tedee_unlock_single_output_action", localCopy.tedee_unlock_single_output_action);
    preferences.putString("tedee_lock_single_output_action", localCopy.tedee_lock_single_output_action);
#endif

#if USE_NUKI
    preferences.putBool("nuki_enabled", localCopy.nuki_enabled);
    preferences.putString("nuki_topic_unlock", localCopy.nuki_topic_unlock);
    preferences.putString("nuki_payload_unlock", localCopy.nuki_payload_unlock);
    preferences.putString("nuki_topic_lock", localCopy.nuki_topic_lock);
    preferences.putString("nuki_payload_lock", localCopy.nuki_payload_lock);
    preferences.putString("nuki_topic_unlatch", localCopy.nuki_topic_unlatch);
    preferences.putString("nuki_payload_unlatch", localCopy.nuki_payload_unlatch);
    preferences.putString("nuki_topic_lockngo", localCopy.nuki_topic_lockngo);
    preferences.putString("nuki_payload_lockngo", localCopy.nuki_payload_lockngo);
    preferences.putString("nuki_topic_lockngo_unlatch", localCopy.nuki_topic_lockngo_unlatch);
    preferences.putString("nuki_payload_lockngo_unlatch", localCopy.nuki_payload_lockngo_unlatch);
    preferences.putString("nuki_topic_full_lock", localCopy.nuki_topic_full_lock);
    preferences.putString("nuki_payload_full_lock", localCopy.nuki_payload_full_lock);
    preferences.putString("nuki_unlock_single_output_action", localCopy.nuki_unlock_single_output_action);
    preferences.putString("nuki_lock_single_output_action", localCopy.nuki_lock_single_output_action);
    preferences.putString("nuki_unlatch_single_output_action", localCopy.nuki_unlatch_single_output_action);
    preferences.putString("nuki_lockngo_single_output_action", localCopy.nuki_lockngo_single_output_action);
    preferences.putString("nuki_lockngo_unlatch_single_output_action", localCopy.nuki_lockngo_unlatch_single_output_action);
    preferences.putString("nuki_full_lock_single_output_action", localCopy.nuki_full_lock_single_output_action);
#endif

>>>>>>> 937c36a21b29111ed2f16fbabeffa236a6d16872
    preferences.end();
}

WifiSettings SettingsManager::getWifiSettings()
{
    WifiSettings copy;
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        copy = wifiSettings; xSemaphoreGive(settingsMutex);
    } else {
        copy = wifiSettings;
    }
    return copy;
}

void SettingsManager::saveWifiSettings(const WifiSettings& newSettings)
{
    WifiSettings normalized = newSettings;
    normalized.hostname.trim();
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        wifiSettings = normalized; xSemaphoreGive(settingsMutex);
    } else {
        wifiSettings = normalized;
    }
    saveWifiSettings();
}

AppSettings SettingsManager::getAppSettings()
{
    AppSettings copy;
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        copy = appSettings; xSemaphoreGive(settingsMutex);
    } else {
        copy = appSettings; 
    }
    return copy;
}

void SettingsManager::saveAppSettings(const AppSettings& newSettings)
{
    AppSettings normalized = newSettings;
    if (settingsMutex && xSemaphoreTake(settingsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        appSettings = normalized; xSemaphoreGive(settingsMutex);
    } else {
        appSettings = normalized; 
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
    if (rc) rc = preferences.clear();
    preferences.end();
    return rc;
}

bool SettingsManager::deleteWifiSettings()
{
    Preferences preferences;
    bool rc = preferences.begin("fs_wifi", false);
    if (rc) rc = preferences.clear();
    preferences.end();
    return rc;
}
<<<<<<< HEAD
=======

>>>>>>> 937c36a21b29111ed2f16fbabeffa236a6d16872
