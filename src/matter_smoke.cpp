// @version: 0.0.1 Builddatum 00:00:00 05-05.2026
#include <Arduino.h>
#include <Preferences.h>
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
#include <WiFi.h>
#endif

MatterOnOffLight gMatterLight;

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

namespace {
Preferences wifiPrefs;

String loadWifiValue(const char *key) {
  return wifiPrefs.getString(key, "");
}

bool onMatterLightChange(bool state) {
  digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
  return true;
}

void printPairingData() {
  Serial.println("=== MATTER SMOKE TEST ===");
  Serial.printf("Commissioned: %s\r\n", Matter.isDeviceCommissioned() ? "yes" : "no");
  Serial.printf("Manual pairing code: %s\r\n", Matter.getManualPairingCode().c_str());
  Serial.printf("QR code URL: %s\r\n", Matter.getOnboardingQRCodeUrl().c_str());
  Serial.println("=========================");
}
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

#if !CONFIG_ENABLE_CHIPOBLE
  if (!wifiPrefs.begin("fs_wifi", true)) {
    Serial.println("[SMOKE] Preferences fs_wifi konnten nicht geoeffnet werden");
    return;
  }

  String ssid = loadWifiValue("ssid");
  String password = loadWifiValue("password");
  String hostname = loadWifiValue("hostname");
  wifiPrefs.end();

  ssid.trim();
  password.trim();
  hostname.trim();

  if (hostname.isEmpty()) {
    hostname = "MatterSmoke";
  }

  if (ssid.isEmpty() || password.isEmpty()) {
    Serial.println("[SMOKE] Keine gespeicherten WLAN-Daten gefunden");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.printf("[SMOKE] Verbinde WLAN %s\r\n", ssid.c_str());

  uint32_t startAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAt < 30000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[SMOKE] WLAN-Verbindung fehlgeschlagen");
    return;
  }

  Serial.printf("[SMOKE] WLAN ok: %s\r\n", WiFi.localIP().toString().c_str());
#endif

  if (!gMatterLight.begin(false)) {
    Serial.println("[SMOKE] MatterOnOffLight.begin(false) fehlgeschlagen");
    return;
  }

  gMatterLight.onChange(onMatterLightChange);

  Serial.println("[SMOKE] Matter.begin() startet");
  Matter.begin();

  if (Matter.isDeviceCommissioned()) {
    Serial.println("[SMOKE] Geraet ist bereits commissioned. Decommission wird gestartet...");
    Matter.decommission();
    delay(2000);
    ESP.restart();
  }

  printPairingData();
}

void loop() {
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 10000) {
    lastPrint = millis();
    printPairingData();
  }
  delay(50);
}