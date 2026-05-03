// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#pragma once

#include <Preferences.h>
#include "global.h"
#include "freertos/semphr.h" // P2-13: Mutex für Thread-Safety

struct WifiSettings
{
  String ssid = "";
  String password = "";
  String hostname = "MatterMQTTBridge";
  String passwordAdmin = "admin";
};

static const int WIFI_RSSI_DISCONNECT_THRESHOLD_DEFAULT = -80;
static const int WIFI_ROAM_IMPROVE_DB_DEFAULT = 10;
static const int WIFI_ROAM_MIN_RSSI_DEFAULT = -69;

static inline int normalizeWifiRoamImproveDb(int value)
{
  return (value >= 3 && value <= 30) ? value : WIFI_ROAM_IMPROVE_DB_DEFAULT;
}

static inline int normalizeWifiRoamMinRssi(int value)
{
  return (value >= -90 && value <= -50) ? value : WIFI_ROAM_MIN_RSSI_DEFAULT;
}

struct AppSettings
{
  bool mqtt_isBroker = false;
  bool mqtt_isClient = false;
  String mqttServer = "";
  String mqtt_port = "1883";
  String mqtt_keepAlive = "45";
  String mqttUsername = "";
  String mqttPassword = "";
  String mqttRootTopic = "MatterMQTTBridge";
  String ntpServer = "pool.ntp.org";
  String ntpOffset = "CET-1CEST,M3.5.0/2,M10.5.0/3"; // Berlin
  String latitude = "";
  String longitude = "";
  String passwordSetup = "";
  String enablePassword = "";

  // WiFi Stabilität
  int wifiRssiDisconnectThreshold = WIFI_RSSI_DISCONNECT_THRESHOLD_DEFAULT;
  int wifiRoamImproveDb = WIFI_ROAM_IMPROVE_DB_DEFAULT;
  int wifiRoamMinRssi = WIFI_ROAM_MIN_RSSI_DEFAULT;

  // ---- Legacy-Felder (Phase 2: entfernen, wenn UI/Webserver bereinigt) ----
  String latitude = "";
  String longitude = "";
  String sunriseOffset = "0";
  String sunsetOffset = "0";
  String toggleButton0 = "Button0";
  String toggleButton1 = "Output (1)";
  String toggleButton2 = "Output (2)";
  String toggleButton3 = "Output (3)";
  String toggleButton4 = "Output (4)";
  String toggleButton5 = "Output (5)";
  String toggleButton6 = "Bell On/Off";
  String delayButton1 = "500";
  String delayButton2 = "500";
  String delayButton3 = "500";
  String delayButton4 = "500";
  String delayButton5 = "1000";
  String delayButton6 = "0";
  String currentUserID = "";
  String sensorPin = "00000000";
  String sensorPairingCode = "";
  bool sensorPairingValid = false;
  bool ignorTouchRing = false;
  bool klingelAnAus = true;
  bool fingerprintScannerEnabled = false;
};

class SettingsManager
{
private:
  WifiSettings wifiSettings;
  AppSettings appSettings;
  SemaphoreHandle_t settingsMutex = xSemaphoreCreateMutex(); // P2-13: Thread-Safety

  void saveWifiSettings();
  void saveAppSettings();

public:
  bool loadWifiSettings();
  bool loadAppSettings();

  WifiSettings getWifiSettings(); // TODO P3-03: könnte const WifiSettings& sein, belassen wegen interner Mutation
  void saveWifiSettings(const WifiSettings& newSettings); // P3-03: by-ref

  AppSettings getAppSettings(); // P2-13: Copy-on-Read für Thread-Safety (vorher const&)
  void saveAppSettings(const AppSettings& newSettings);
  // fillPasswordMap() entfernt (P3-13) — war deklariert aber nie implementiert
  bool isWifiConfigured();

  bool deleteAppSettings();
  bool deleteWifiSettings();
};
