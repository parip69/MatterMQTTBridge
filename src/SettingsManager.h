// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#pragma once

#include <Preferences.h>
#include "global.h"
#include "freertos/semphr.h"

struct WifiSettings
{
  String ssid = "";
  String password = "";
  String hostname = "MatterMQTTBridge";
  String passwordAdmin = "admin";
};

struct AppSettings
{
  bool mqtt_isBroker = false;
  bool mqtt_isClient = false;
  String mqttServer = "";
  String mqtt_port = "1883";
  String mqtt_keepAlive = "45";
  String mqttUsername = "";
  String mqttPassword = "";
  String mqttRootTopic = "fingerprint";
  String ntpServer = "pool.ntp.org";
  String ntpOffset = "CET-1CEST,M3.5.0/2,M10.5.0/3";
  String passwordSetup = "";
  String enablePassword = "";
};

class SettingsManager
{
private:
  WifiSettings wifiSettings;
  AppSettings appSettings;
  SemaphoreHandle_t settingsMutex = xSemaphoreCreateMutex();

  void saveWifiSettings();
  void saveAppSettings();

public:
  bool loadWifiSettings();
  bool loadAppSettings();

  WifiSettings getWifiSettings();
  void saveWifiSettings(const WifiSettings& newSettings);

  AppSettings getAppSettings();
  void saveAppSettings(const AppSettings& newSettings);
  
  bool isWifiConfigured();
  bool deleteAppSettings();
  bool deleteWifiSettings();
};
