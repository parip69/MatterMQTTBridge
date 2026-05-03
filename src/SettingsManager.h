// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#pragma once

#include <Preferences.h>
#include "global.h"
#include "freertos/semphr.h" // P2-13: Mutex für Thread-Safety

struct WifiSettings
{
  String ssid = "";
  String password = "";
  String hostname = "Fingerscanner";
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
  String mqttRootTopic = "fingerscanner";
  String ntpServer = "pool.ntp.org";
  String ntpOffset = "CET-1CEST,M3.5.0/2,M10.5.0/3"; // Berlin
  String latitude = "";
  String longitude = "";
  String passwordSetup = "";
  String enablePassword = "";
  String sunriseOffset = "0";
  String sunsetOffset = "0";

  // WiFi Stabilität
  int wifiRssiDisconnectThreshold = WIFI_RSSI_DISCONNECT_THRESHOLD_DEFAULT;
  int wifiRoamImproveDb = WIFI_ROAM_IMPROVE_DB_DEFAULT;
  int wifiRoamMinRssi = WIFI_ROAM_MIN_RSSI_DEFAULT;

  String toggleButton0 = "IgnorTouchRing";
  String toggleButton1 = "Output (1)";
  String toggleButton2 = "Output (2)";
  String toggleButton3 = "Output (3)";
  String toggleButton4 = "Output (4)";
  String toggleButton5 = "Klingel";
  String toggleButton6 = "Bell On/Off";

  String delayButton1 = "500";
  String delayButton2 = "500";
  String delayButton3 = "500";
  String delayButton4 = "500";
  String delayButton5 = "1000";
  String delayButton6 = "0"; // Button 6 hat keine Verzögerung

  String currentUserID = "";
  String sensorPin = "00000000";
  String sensorPairingCode = "";
  bool sensorPairingValid = false;
  bool ignorTouchRing = false;
  bool klingelAnAus = true;
  bool fingerprintScannerEnabled = true; // NEU: Scanner standardmäßig aktiviert

#if USE_TELEGRAM
  // Telegram Settings
  bool telegram_enabled = false;
  String telegram_botToken = "";
  String telegram_chatId = "";
#if USE_TELEGRAM_CMD
  bool telegram_cmdEnabled = true;
  uint32_t telegram_pollIntervalMs = 15000;
#endif
#endif

#if USE_TEDEE
  // Tedee Settings
  bool tedee_enabled = false;
  String tedee_bridge_ip = "";
  int tedee_port = 80;
  int tedee_lock_id = 0;
  String tedee_token = "";
  String tedee_unlock_single_output_action = "1";
  String tedee_lock_single_output_action = "2";
#endif

#if USE_NUKI
  // Nuki Settings
  bool nuki_enabled = false;
  String nuki_topic_unlock = "";
  String nuki_payload_unlock = "";
  String nuki_topic_lock = "";
  String nuki_payload_lock = "";
  String nuki_topic_unlatch = "";
  String nuki_payload_unlatch = "";
  String nuki_topic_lockngo = "";
  String nuki_payload_lockngo = "";
  String nuki_topic_lockngo_unlatch = "";
  String nuki_payload_lockngo_unlatch = "";
  String nuki_topic_full_lock = "";
  String nuki_payload_full_lock = "";
  String nuki_unlock_single_output_action = "1";
  String nuki_lock_single_output_action = "2";
  String nuki_unlatch_single_output_action = "3";
  String nuki_lockngo_single_output_action = "";
  String nuki_lockngo_unlatch_single_output_action = "";
  String nuki_full_lock_single_output_action = "";
#endif
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

  String generateNewPairingCode();
};
