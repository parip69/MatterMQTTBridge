// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉

#pragma once

#include <Adafruit_Fingerprint.h>
#include <Preferences.h>
#include "global.h"

#define mySerial Serial2
// Sicherheitsreserve für Template-Puffer. R503-Datenblatt nennt 768 Byte pro Template,
// in der Praxis wurden funktionierende Exportdateien bis 1536 Byte beobachtet.
// Der tatsächlich genutzte Datenanteil kann kleiner als der reservierte Puffer sein.
// 2048 ist bewusst konservativ gewählt, um alle bekannten Fälle sicher abzudecken.
#define FINGERPRINT_TEMPLATE_SIZE 2048

// Anzahl Enrollment-Scans (Finger-Aufnahmen pro Enrollment).
// 5 = bewusste Qualitätsentscheidung für bessere Template-Qualität (nicht wegen Template-Größe nötig).
// Original Grow R503: 5 funktioniert zuverlässig.
// Für Klone/Nachbauten: 2 oder 3 können robuster sein.
// Zum Anpassen einfach diesen Wert ändern (2, 3 oder 5).
#define ENROLLMENT_SAMPLES 5

// Debug-Schalter für Hex-Dumps von Fingerprint-Templates
#define DEBUG_FINGER_TEMPLATE_HEX 0

// R-07: Zusätzliche Farbwerte laut R503-Datenblatt für die Aura-LED
// Hinweis: FINGERPRINT_LED_RED (0x01), FINGERPRINT_LED_BLUE (0x02) und
// FINGERPRINT_LED_PURPLE (0x03) sind bereits in der Adafruit-Bibliothek definiert.
// Hier folgen die erweiterten Werte für alle 7 R503-Farben:
constexpr uint8_t R503_LED_RED = 0x01;     // identisch mit FINGERPRINT_LED_RED
constexpr uint8_t R503_LED_BLUE = 0x02;    // identisch mit FINGERPRINT_LED_BLUE
constexpr uint8_t R503_LED_PURPLE = 0x03;  // identisch mit FINGERPRINT_LED_PURPLE
constexpr uint8_t R503_LED_GREEN = 0x04;   // Grün (nicht in Adafruit-Lib)
constexpr uint8_t R503_LED_YELLOW = 0x05;  // Gelb (nicht in Adafruit-Lib)
constexpr uint8_t R503_LED_CYAN = 0x06;    // Cyan (nicht in Adafruit-Lib)
constexpr uint8_t R503_LED_WHITE = 0x07;   // Weiß (nicht in Adafruit-Lib)
/*
Serial2
Der ESP32 hat standardmÃƒÂ¤ÃƒÅ¸ig folgende Pins fÃƒÂ¼r Serial2 konfiguriert:
RX2: GPIO16 GrÃƒÂ¼n beim fingerprint reader Grow R503
TX2: GPIO17 Gelb beim fingerprint reader Grow R503

Beispiel:
Serial2.begin(9600, SERIAL_8N1, 22, 23);
Hier werden die Pins GPIO22 fÃƒÂ¼r RX2 GrÃƒÂ¼n und GPIO23 Gelb fÃƒÂ¼r TX2 verwendet.
*/
#define FINGERPRINT_WRITENOTEPAD 0x18 // Write Notepad on sensor
#define FINGERPRINT_READNOTEPAD 0x19  // Read Notepad from sensor

/*
  By using the touch ring as an additional input to the image sensor the sensitivity is much higher for door bell ring events. Unfortunately
  we cannot differ between touches on the ring by fingers or rain drops, so rain on the ring will cause false alarms.
*/
const int touchRingPin = 5; // touch/wakeup pin connected to fingerprint sensor

enum class ScanResult
{
  noFinger,
  matchFound,
  noMatchFound,
  error
};
enum class EnrollResult
{
  ok,
  error
};

struct Match
{
  ScanResult scanResult = ScanResult::noFinger;
  uint16_t matchId = 0;
  String matchName = "unknown";
  uint16_t matchConfidence = 0;
  uint8_t returnCode = 0;
};

struct NewFinger
{
  EnrollResult enrollResult = EnrollResult::error;
  uint8_t returnCode = 0;
};

#if USE_DOWNLOAD_UPLOAD
struct FingerprintFile
{
  String filename;
  uint8_t *data;
  size_t len;
};
#endif

class FingerprintManager
{
private:
  Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
  bool lastTouchState = false;
  String fs_flist[201];

#if USE_DOWNLOAD_UPLOAD
  uint8_t exportedFingerData[FINGERPRINT_TEMPLATE_SIZE];
  size_t exportedFingerLength;
  String exportedFilename;
#endif

  int fingerCountOnSensor = 0;
  bool ignoreTouchRing = false; // set to true when the sensor is usually exposed to rain to avoid false ring events. Can also be set conditional by a rain sensor over MQTT
  bool klingelAnAus = true;
  bool lastIgnoreTouchRing = false;
  bool scannerEnabled = true; // NEU: Wenn false, bleibt LED-Ring aus
  // bool statusIgnoreTouchRing = false;

  void updateTouchState(bool touched);
  // bool isRingTouched();
  void loadFingerListFromPrefs();
  // disconnect() entfernt (P3-12) — war deklariert aber nie definiert
  uint8_t writeNotepad(uint8_t pageNumber, const char *text, uint8_t length);
  uint8_t readNotepad(uint8_t pageNumber, char *text, uint8_t length);

public:
  bool isRingTouched();
  bool connected = false;
  bool connect();
  Match scanFingerprint();
  NewFinger enrollFinger(int id, const String& name);
  bool deleteFinger(int id);
  bool isFingerIdInList(int id);
  void renameFinger(int id, const String& newName);
  String getFingerListAsHtmlOptionList();
  void setIgnoreTouchRing(bool state);
  void setKlingelAnAus(bool state);
  void setScannerEnabled(bool state); // NEU: Scanner aktivieren/deaktivieren
  bool isFingerOnSensor();
  void setLedRingError();
  void setLedRingOff();
  void setLedRingWifiConfig();
  void setLedRingWifiDisconnected();
  void setLedRingMqttBrokerOffline();
  void setLedRingReady();
  String getPairingCode();
  bool setPairingCode(const String& pairingCode);

#if USE_DOWNLOAD_UPLOAD
  bool exportSingleFinger(uint8_t id);
  const uint8_t *getExportedFingerData() const { return exportedFingerData; }
  const String &getExportedFingerprintFilename() const;
  size_t getExportedFingerprintLength() const;
  // void importSingleFingerprint(uint8_t id, uint8_t *templateData);
  bool importSingleFingerprint(uint8_t id, uint8_t *templateData, size_t dataSize);
#endif

  bool deleteAll();

  // functions for sensor replacement
  void exportSensorDB();
  void importSensorDB();
  bool isScannerEnabled() const { return scannerEnabled; }
};
