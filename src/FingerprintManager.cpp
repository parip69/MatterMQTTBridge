// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#include "FingerprintManager.h"
#include "SettingsManager.h"
#include "global.h"

#include <Adafruit_Fingerprint.h>

extern SettingsManager settingsManager;

namespace
{
  bool verifyPasswordWithTimeout(Adafruit_Fingerprint &finger, uint32_t timeoutMs)
  {
    const uint32_t start = millis();
    while (millis() - start < timeoutMs)
    {
      if (finger.verifyPassword())
      {
        return true;
      }
      delay(50);
    }
    return false;
  }

  void flushSerialWithQuietTimeout(HardwareSerial &serial, uint32_t quietTimeMs, uint32_t maxTimeMs)
  {
    const uint32_t start = millis();
    uint32_t lastByteAt = start;

    while ((millis() - start) < maxTimeMs)
    {
      bool gotByte = false;
      while (serial.available())
      {
        serial.read();
        gotByte = true;
      }

      if (gotByte)
      {
        lastByteAt = millis();
      }
      else if ((millis() - lastByteAt) >= quietTimeMs)
      {
        break;
      }

      delay(5);
    }
  }
}

bool FingerprintManager::connect()
{
  // Scanner deaktiviert: so tun als wäre er nicht vorhanden (keine Sensor-Kommunikation)
  if (!scannerEnabled)
  {
    connected = false;
    return false;
  }

  // initialize input pins
  pinMode(touchRingPin, INPUT_PULLDOWN);

  LOG_PRINTLN("\n\nAdafruit finger detect test");

  // set the data rate for the sensor serial port
  finger.begin(57600);

  bool sensorReady = verifyPasswordWithTimeout(finger, 1000);
  if (!sensorReady)
  {
    // Boot-Retry verkürzt: 2s statt 5s. Sensor braucht laut Datenblatt <1s zum Booten.
    // 3s Gesamtzeit (1+2) reicht auch nach OTA-Restart. Hintergrund-Reconnect greift bei Bedarf.
    sensorReady = verifyPasswordWithTimeout(finger, 2000);
  }

  if (!sensorReady)
  {
    LOG_PRINTLN("Did not find fingerprint sensor :(");
    connected = false;
    return connected;
  }

  LOG_PRINTLN("Found fingerprint sensor!");

  finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE, 0); // sensor connected signal

  LOG_PRINTLN(F("Reading sensor parameters"));
  if (finger.getParameters() != FINGERPRINT_OK)
  {
    LOG_PRINTLN(F("FEHLER: getParameters() fehlgeschlagen!"));
    connected = false;
    return connected;
  }
  LOG_PRINT(F("Status: 0x"));
  LOG_PRINTLN(finger.status_reg, HEX);
  LOG_PRINT(F("Sys ID: 0x"));
  LOG_PRINTLN(finger.system_id, HEX);
  LOG_PRINT(F("Capacity: "));
  LOG_PRINTLN(finger.capacity);
  LOG_PRINT(F("Security level: "));
  LOG_PRINTLN(finger.security_level);
  LOG_PRINT(F("Device address: "));
  LOG_PRINTLN(finger.device_addr, HEX);
  LOG_PRINT(F("Packet len: "));
  LOG_PRINTLN(finger.packet_len);
  LOG_PRINT(F("Baud rate: "));
  LOG_PRINTLN(finger.baud_rate);
  /*
  // R-04: Security Level auf Stufe 3 setzen (Balance aus FAR/FRR, laut R503-Datenblatt Stufen 1-5)
  // Stellt sicher, dass der Sensor unabhängig von Vorconfig immer mit dem erwarteten Level läuft.
  {
    const uint8_t targetSecurityLevel = 3;
    if (finger.security_level != targetSecurityLevel)
    {
      if (finger.setSecurityLevel(targetSecurityLevel) == FINGERPRINT_OK)
      {
        LOG_PRINTLN(String(F("Security Level geändert: ")) + finger.security_level + " -> " + targetSecurityLevel);
        finger.security_level = targetSecurityLevel; // Lokalen Cache aktualisieren
      }
      else
      {
        LOG_PRINTLN(F("WARNUNG: Security Level konnte nicht gesetzt werden"));
      }
    }
  }


  #if USE_DOWNLOAD_UPLOAD
    // Scanner-Parameter auch an Browser senden
    String params = "[Scanner] System-ID: 0x" + String(finger.system_id, HEX) +
                    ", Device-Addr: 0x" + String(finger.device_addr, HEX) +
                    ", Security: " + String(finger.security_level) +
                    ", Capacity: " + String(finger.capacity);
    notifyClients(params);
  #endif
  */
  if (finger.getTemplateCount() != FINGERPRINT_OK)
  {
    LOG_PRINTLN(F("FEHLER: getTemplateCount() fehlgeschlagen!"));
    connected = false;
    return connected;
  }
  LOG_PRINTLN("");
  LOG_PRINT("Sensor contains ");
  LOG_PRINT(finger.templateCount);
  LOG_PRINTLN(" templates");

  loadFingerListFromPrefs();

  connected = true;
  return connected;

  // updateTouchState(false);
}

void FingerprintManager::updateTouchState(bool touched)
{
  if ((touched != lastTouchState) || (ignoreTouchRing != lastIgnoreTouchRing))
  {
    // check if sensor or ring is touched
    if (touched)
    {
      // turn touch indicator on:
      finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 0);
    }
    else
    {
      // turn touch indicator off:
      setLedRingReady();
    }
  }
  lastTouchState = touched;
  lastIgnoreTouchRing = ignoreTouchRing;
}

Match FingerprintManager::scanFingerprint()
{

  Match match;
  match.scanResult = ScanResult::error;

  // Wenn der Scanner deaktiviert ist oder keine Verbindung besteht, wird der Scan sofort abgebrochen.
  if (!connected || !scannerEnabled)
  {
    return match;
  }

  // finger detection by capacitive touchRing state (increased sensitivy but error prone due to rain)
  bool ringTouched = false;
  if (!ignoreTouchRing)
  {
    if (isRingTouched())
      ringTouched = true;

    // DEBUG: Zeige Touch-Status
    // LOG_PRINTLN(String("[DEBUG] ignoreTouchRing=0, ringTouched=") + (ringTouched ? "1" : "0") + ", lastTouchState=" + (lastTouchState ? "1" : "0"));

    if (ringTouched || lastTouchState)
    {
      updateTouchState(true);
      // LOG_PRINTLN("touched");
    }
    else
    {
      updateTouchState(false);
      match.scanResult = ScanResult::noFinger;
      return match;
    }
  }

  bool doAnotherScan = true;
  int scanPass = 0;
  while (doAnotherScan)
  {
    doAnotherScan = false;
    scanPass++;

    // DEBUG: Scan-Durchgang
    // LOG_PRINTLN(String("[DEBUG] scanPass=") + scanPass);

    ///////////////////////////////////////////////////////////
    // STEP 1: Get Image from Sensor
    ///////////////////////////////////////////////////////////
    bool doImaging = true;
    int imagingPass = 0;
    while (doImaging)
    {
      doImaging = false;
      imagingPass++;
      // LOG_PRINTLN(String("Get Image try ") + imagingPass);
      match.returnCode = finger.getImage();
      switch (match.returnCode)
      {
      case FINGERPRINT_OK:
        // Important: do net set touch state to true yet! Reason:
        // - if touchRing is NOT ignored, updateTouchState(true) was already called a few lines up, ring is already flashing red
        // - if touchRing IS ignored, wait for next step because image still can be "too messy" (=raindrop on sensor), and we don't want to flash red in this case
        // updateTouchState(true);
        // LOG_PRINTLN("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
      case FINGERPRINT_PACKETRECIEVEERR: // occurs from time to time, handle it like a "nofinger detected but touched" situation
        if (ringTouched)
        {
          // no finger on sensor but ring was touched -> ring event
          // LOG_PRINTLN("ring touched");
          updateTouchState(true);
          if (imagingPass < 15) // up to x image passes in a row are taken after touch ring was touched until noFinger will raise a noMatchFound event
          {
            doImaging = true; // scan another image
            // delay(50);
            break;
          }
          else
          {
            // Touch-Ring wurde gedrückt (Signal "jemand will rein"), aber nach 15 Versuchen kein gültiger Finger erkannt
            // -> Klingel auslösen (noMatchFound), da jemand Einlass wünscht
            // LOG_PRINTLN("15 times no image after touching ring");
            match.scanResult = ScanResult::noMatchFound;
            return match;
          }
        }
        else
        {
          if (ignoreTouchRing && scanPass > 1)
          {
            // the scan(s) in last iteration(s) have not found any match, now the finger was released (=no finger) -> return "no match" as result
            match.scanResult = ScanResult::noMatchFound;
          }
          else
          {
            match.scanResult = ScanResult::noFinger;
            updateTouchState(false);
          }
          return match;
        }
      case FINGERPRINT_IMAGEFAIL:
        LOG_PRINTLN("Imaging error");
        updateTouchState(true);
        return match;
      default:
        LOG_PRINTLN("Unknown error");
        return match;
      }
    }

    ///////////////////////////////////////////////////////////
    // STEP 2: Convert Image to feature map
    ///////////////////////////////////////////////////////////
    match.returnCode = finger.image2Tz();
    switch (match.returnCode)
    {
    case FINGERPRINT_OK:
      // LOG_PRINTLN("Image converted");
      updateTouchState(true);
      break;
    case FINGERPRINT_IMAGEMESS:
      LOG_PRINTLN("Image too messy");
      return match;
    case FINGERPRINT_PACKETRECIEVEERR:
      LOG_PRINTLN("Communication error");
      return match;
    case FINGERPRINT_FEATUREFAIL:
      LOG_PRINTLN("Could not find fingerprint features");
      return match;
    case FINGERPRINT_INVALIDIMAGE:
      LOG_PRINTLN("Could not find fingerprint features");
      return match;
    default:
      LOG_PRINTLN("Unknown error");
      return match;
    }

    ///////////////////////////////////////////////////////////
    // STEP 3: Search DB for matching features
    ///////////////////////////////////////////////////////////
    match.returnCode = finger.fingerSearch();
    if (match.returnCode == FINGERPRINT_OK)
    {
      // found a match!
      finger.LEDcontrol(FINGERPRINT_LED_ON, 0, R503_LED_GREEN);
      // delay(1000);
      // setLedRingReady();

      match.scanResult = ScanResult::matchFound;
      match.matchId = finger.fingerID;
      match.matchConfidence = finger.confidence;
      // Bounds-Check: finger.fingerID muss im gültigen Bereich 1-200 liegen
      if (finger.fingerID >= 1 && finger.fingerID <= 200)
      {
        match.matchName = fs_flist[finger.fingerID];
      }
      else
      {
        match.matchName = "ID_out_of_range";
        LOG_PRINTLN(String("[WARN] fingerID außerhalb gültigem Bereich: ") + finger.fingerID);
      }
      // LOG_PRINTLN(String("[DEBUG] *** MATCH FOUND *** ID=") + finger.fingerID + ", scanPass=" + scanPass);
    }
    else if (match.returnCode == FINGERPRINT_PACKETRECIEVEERR)
    {
      LOG_PRINTLN("Communication error");
    }
    else if (match.returnCode == FINGERPRINT_NOTFOUND)
    {
      LOG_PRINTLN(String("Did not find a match. (Scan #") + scanPass + String(" of 5)"));
      match.scanResult = ScanResult::noMatchFound;
      if (scanPass < 5) // max 5 Scans until no match found is given back as result
      {
        doAnotherScan = true;
        // delay(25); // Minimaler Delay für Sensor-Stabilität
      }
    }
    else
    {
      LOG_PRINTLN("Unknown error");
    }

  } // while

  return match;
}

// Preferences
void FingerprintManager::loadFingerListFromPrefs()
{
  Preferences preferences;
  if (!preferences.begin("fs_flist", true))
  {
    for (int i = 1; i <= 200; i++)
    {
      fs_flist[i] = String("@empty");
    }
    LOG_PRINTLN(F("WARNUNG: fs_flist Preferences konnten nicht geöffnet werden."));
    return;
  }

  int counter = 0;
  for (int i = 1; i <= 200; i++)
  {
    char key[4];
    snprintf(key, sizeof(key), "%d", i);
    if (preferences.isKey(key))
    {
      fs_flist[i] = preferences.getString(key, "@empty");
    }
    else
    {
      fs_flist[i] = "@empty";
    }
    if (fs_flist[i] != "@empty")
    {
      counter++;
    }
  }

  LOG_PRINTLN(String(counter) + " fingers loaded from preferences.");
  LOG_PRINTLN("");

  if (counter != finger.templateCount)
    notifyClients(String("Warning: Fingerprint count mismatch! ") + finger.templateCount + " fingerprints stored on sensor, but we are aware of " + counter + " fingerprints.");
  preferences.end();
}

// Add/Enroll fingerprint
NewFinger FingerprintManager::enrollFinger(int id, const String &name)
{

  NewFinger newFinger;
  newFinger.enrollResult = EnrollResult::error;

  lastTouchState = true; // after enrollment, scan mode kicks in again. Force update of the ring light back to normal on first iteration of scan mode.

  notifyClients(String("Anmeldung für ID # ") + id + " gestartet. Wir müssen Ihren Finger " + String(ENROLLMENT_SAMPLES) + " Mal scannen, bis die Anmeldung abgeschlossen ist.");

  // Set maximum waiting time for finger placement
  const unsigned long maxWaitTime = 10000; // Example: 10 seconds

  // R-01: Standard-R503-Spec erlaubt nur CharBuffer 1+2 für image2Tz().
  // Originaler Grow R503 unterstützt intern bis zu 6 Buffer (getestet bis 5).
  // ENROLLMENT_SAMPLES = 2 → maximale Kompatibilität (auch Klone/Nachbauten).
  // ENROLLMENT_SAMPLES = 5 → bessere Template-Qualität auf echtem Grow R503.
  for (int nTimes = 1; nTimes <= ENROLLMENT_SAMPLES; nTimes++)
  {
    notifyClients(String("Take #" + String(nTimes)) + " (Legen Sie Ihren Finger auf den Sensor, bis der LED-Ring nicht mehr blinkt, und entfernen Sie ihn dann).");

    if (nTimes != 1) // not on the first run
    {
      newFinger.returnCode = 0xFF;
      uint32_t waitStart = millis();
      while (newFinger.returnCode != FINGERPRINT_NOFINGER)
      {
        if (millis() - waitStart > 5000)
          break; // Timeout nach 5s gegen WDT-Reset
        newFinger.returnCode = finger.getImage();
      }
    }

    LOG_PRINT("Taking image sample ");
    LOG_PRINT(nTimes);
    LOG_PRINTLN(": ");
    finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_PURPLE, 0);
    newFinger.returnCode = 0xFF;

    unsigned long startTime = millis();

    while (newFinger.returnCode != FINGERPRINT_OK)
    {
      // Check if the maximum waiting time is exceeded
      if (millis() - startTime > maxWaitTime)
      {
        // Enrollment canceled if the finger is not placed within the specified time
        notifyClients("Registrierung abgebrochen. Finger wurde nicht innerhalb der festgelegten Zeit platziert.");
        LOG_PRINTLN("Registrierung abgebrochen. Finger wurde nicht innerhalb der festgelegten Zeit platziert.");
        newFinger.enrollResult = EnrollResult::error;
        return newFinger;
      }

      newFinger.returnCode = finger.getImage();
      switch (newFinger.returnCode)
      {
      case FINGERPRINT_OK:
        LOG_PRINT("taken, ");
        break;
      case FINGERPRINT_NOFINGER:
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        LOG_PRINT("Communication error, ");
        break;
      case FINGERPRINT_IMAGEFAIL:
        LOG_PRINT("Imaging error, ");
        break;
      default:
        LOG_PRINT("Unknown error, ");
        break;
      }
      delay(10); // Kurze Entlastung für Scheduler/Netzwerk während der Wartephase
    }

    // OK success!

    newFinger.returnCode = finger.image2Tz(nTimes);
    switch (newFinger.returnCode)
    {
    case FINGERPRINT_OK:
      LOG_PRINT("converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      LOG_PRINT("too messy");
      return newFinger;
    case FINGERPRINT_PACKETRECIEVEERR:
      LOG_PRINT("Communication error");
      return newFinger;
    case FINGERPRINT_FEATUREFAIL:
      LOG_PRINT("Could not find fingerprint features");
      return newFinger;
    case FINGERPRINT_INVALIDIMAGE:
      LOG_PRINT("Could not find fingerprint features");
      return newFinger;
    default:
      LOG_PRINT("Unknown error");
      return newFinger;
    }
    if (nTimes == ENROLLMENT_SAMPLES)
    {
      finger.LEDcontrol(FINGERPRINT_LED_ON, 0, R503_LED_GREEN);
    }
    else
    {
      finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
    }
    // finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
    // finger.LEDcontrol(FINGERPRINT_LED_ON, 0, R503_LED_GREEN);
  }

  // OK converted!
  LOG_PRINTLN();
  LOG_PRINT("Creating model for #");
  LOG_PRINTLN(id);

  newFinger.returnCode = finger.createModel();
  if (newFinger.returnCode == FINGERPRINT_OK)
  {
    LOG_PRINTLN("Prints matched!");
  }
  else if (newFinger.returnCode == FINGERPRINT_PACKETRECIEVEERR)
  {
    LOG_PRINTLN("Communication error");

    // Optional: nach Kommunikationsfehler Puffer leeren (häufige Desync-Ursache)
    while (mySerial.available())
      mySerial.read();
    delay(20);

    return newFinger;
  }
  else if (newFinger.returnCode == FINGERPRINT_ENROLLMISMATCH)
  {
    LOG_PRINTLN("Fingerprints did not match");
    return newFinger;
  }
  else
  {
    LOG_PRINTLN("Unknown error");
    return newFinger;
  }

  LOG_PRINT("ID ");
  LOG_PRINTLN(id);

  // Bounds-Check: Sensor unterstützt IDs 1–200 (P1-04)
  if (id < 1 || id > 200)
  {
    LOG_PRINTLN("ERROR: ID out of range (1-200)");
    return newFinger;
  }

  newFinger.returnCode = finger.storeModel(id);
  if (newFinger.returnCode == FINGERPRINT_OK)
  {
    LOG_PRINTLN("Stored!");
    newFinger.enrollResult = EnrollResult::ok;
    // save to prefs
    fs_flist[id] = name;
    Preferences preferences;
    preferences.begin("fs_flist", false);
    preferences.putString(String(id).c_str(), name);
    preferences.end();
    // LED-Ring einmal grün aufleuchten lassen und danach wieder in den Bereitschaftsmodus wechseln
    // finger.LEDcontrol(FINGERPRINT_LED_ON, 0, R503_LED_GREEN);
    // delay(1000);
    // setLedRingReady();
  }
  else if (newFinger.returnCode == FINGERPRINT_PACKETRECIEVEERR)
  {
    LOG_PRINTLN("Communication error");

    // Optional: nach Kommunikationsfehler Puffer leeren (häufige Desync-Ursache)
    while (mySerial.available())
      mySerial.read();
    delay(20);

    return newFinger;
  }
  else if (newFinger.returnCode == FINGERPRINT_BADLOCATION)
  {
    LOG_PRINTLN("Could not store in that location");
    return newFinger;
  }
  else if (newFinger.returnCode == FINGERPRINT_FLASHERR)
  {
    LOG_PRINTLN("Error writing to flash");

    // UART-Puffer nach Fehler leeren, um Desync/Restbytes zu verhindern
    while (mySerial.available())
      mySerial.read();
    delay(50);

    return newFinger;
  }
  else
  {
    LOG_PRINTLN("Unknown error");
    return newFinger;
  }

  // finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_RED);

  return newFinger;
}

bool FingerprintManager::deleteFinger(int id)
{
  LOG_PRINTLN(String("Versuche, Fingerabdruck mit ID ") + id + " zu löschen.");

  if (!connected)
  {
    LOG_PRINTLN(F("Fehler: Sensor nicht verbunden."));
    notifyClients("Fehler: Fingerprint-Sensor ist nicht verbunden.");
    return false;
  }

  if (id < 1 || id > 200)
  {
    LOG_PRINTLN(String("Fehler: Ungültige ID ") + id + " für deleteFinger().");
    notifyClients(String("Fehler: Ungültige Finger-ID ") + id);
    return false;
  }

  int8_t result = finger.deleteModel(id);
  if (result != FINGERPRINT_OK)
  {
    LOG_PRINTLN(String("Fehler beim Löschen des Fingerabdrucks mit ID ") + id + ". Fehlercode: " + result);
    notifyClients(String("Delete of finger template #") + id + " from sensor failed with code " + result);
    return false;
  }

  LOG_PRINTLN(String("Fingerabdruck mit ID ") + id + " erfolgreich gelöscht.");
  notifyClients(String("Fingerabdruck mit ID ") + id + " wurde erfolgreich gelöscht.");
  fs_flist[id] = "@empty";

  Preferences preferences;
  if (preferences.begin("fs_flist", false))
  {
    preferences.remove(String(id).c_str());
    preferences.end();
  }

  return true;
}
// Diese Funktion überprüft, ob die gegebene ID in der Fingerliste vorhanden ist
bool FingerprintManager::isFingerIdInList(int id)
{
  if (id < 1 || id > 200)
  {
    return false;
  }

  // fs_flist wird in loadFingerListFromPrefs() initialisiert
  // und bei enroll/delete/rename aktuell gehalten.
  // "@empty" bedeutet: dieser Platz ist frei.
  return fs_flist[id] != "@empty";
}

void FingerprintManager::renameFinger(int id, const String &newName)
{
  // Debugging für renameFinger
  LOG_PRINTLN(String("Versuche, Fingerabdruck mit ID ") + id + " umzubenennen in " + newName);
  if ((id > 0) && (id <= 200))
  {
    Preferences preferences;
    preferences.begin("fs_flist", false);
    preferences.putString(String(id).c_str(), newName);
    preferences.end();
    LOG_PRINTLN(String("Fingerabdruck mit ID ") + id + " erfolgreich umbenannt in " + newName);

    notifyClients(String("Fingerabdruck mit ID ") + id + " erfolgreich umbenannt in " + newName);
    fs_flist[id] = newName;
  }
  else
  {
    LOG_PRINTLN(String("Fehler: Ungültige ID ") + id + " für Umbenennung.");
  }
}

String FingerprintManager::getFingerListAsHtmlOptionList()
{
  String htmlOptions;
  htmlOptions.reserve(4096);

  bool firstOption = true;
  for (int i = 1; i <= 200; i++)
  {
    if (fs_flist[i] == "@empty")
      continue;

    htmlOptions += F("<option value=\"");
    htmlOptions += i;
    if (firstOption)
      htmlOptions += F("\" selected>");
    else
      htmlOptions += F("\">");
    htmlOptions += i;
    htmlOptions += F(" - ");
    htmlOptions += fs_flist[i];
    htmlOptions += F("</option>");

    firstOption = false;
  }
  return htmlOptions;
}

void FingerprintManager::setIgnoreTouchRing(bool state)
{
  if (ignoreTouchRing != state)
  {
    ignoreTouchRing = state;

    // P2-09: Nur bei Wertänderung in NVS speichern (Flash-Wear vermeiden)
    AppSettings settings = settingsManager.getAppSettings();
    settings.ignorTouchRing = state;
    settingsManager.saveAppSettings(settings);

    notifyClients(state ? "IgnoreTouchRing is now 'on'" : "IgnoreTouchRing is now 'off'");
  }
}

void FingerprintManager::setKlingelAnAus(bool state)
{
  if (klingelAnAus != state)
  {
    klingelAnAus = state;

    // P2-09: Nur bei Wertänderung in NVS speichern (Flash-Wear vermeiden)
    AppSettings settings = settingsManager.getAppSettings();
    settings.klingelAnAus = state;
    settingsManager.saveAppSettings(settings);

    notifyClients(state ? "Die Klingel ist 'on'" : "Die Klingel ist 'off'");
  }
}

void FingerprintManager::setScannerEnabled(bool state)
{
  scannerEnabled = state;
  if (!state)
  {
    // Wenn Scanner deaktiviert wird, LED-Ring sofort ausschalten
    setLedRingOff();
  }
}

bool FingerprintManager::isRingTouched()
{
  if (digitalRead(touchRingPin) == LOW) // LOW = touched. Caution: touchSignal on this pin occour only once (at beginning of touching the ring, not every iteration if you keep your finger on the ring)
    return true;
  else
    return false;
}

bool FingerprintManager::isFingerOnSensor()
{
  // P3-15: Ohne Verbindung zum Sensor keine Kommunikation versuchen
  if (!connected)
    return false;

  // get an image
  uint8_t returnCode = finger.getImage();
  if (returnCode == FINGERPRINT_OK)
  {
    // try to find fingerprint features in image, because image taken does not already means finger on sensor, could also be a raindrop
    returnCode = finger.image2Tz();
    if (returnCode == FINGERPRINT_OK)
      return true;
  }
  return false;
}

void FingerprintManager::setLedRingError()
{
  // P3-14: Ohne Verbindung zum Sensor keine LED-Kommunikation
  if (!connected)
    return;
  // NEU: Wenn Scanner deaktiviert ist, LED-Ring aus lassen
  if (!scannerEnabled)
  {
    return; // Nicht einschalten wenn deaktiviert
  }
  finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED);
}
void FingerprintManager::setLedRingOff()
{
  // Schaltet den LED-Ring des Fingerabdrucksensors komplett aus
  finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE);
}
void FingerprintManager::setLedRingWifiConfig()
{
  // P3-14: Ohne Verbindung zum Sensor keine LED-Kommunikation
  if (!connected)
    return;
  // NEU: Wenn Scanner deaktiviert ist, LED-Ring aus lassen
  if (!scannerEnabled)
    return;
  finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 250, FINGERPRINT_LED_RED);
}

void FingerprintManager::setLedRingWifiDisconnected()
{
  // P3-14: Ohne Verbindung zum Sensor keine LED-Kommunikation
  if (!connected)
    return;
  // NEU: Wenn Scanner deaktiviert ist, LED-Ring aus lassen
  if (!scannerEnabled)
    return;
  finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_BLUE);
}

void FingerprintManager::setLedRingMqttBrokerOffline()
{
  // WLAN ist da, aber MQTT/Broker ist nicht erreichbar:
  // langsames gelbes Blinken statt dauerhaft Rot
  if (!connected)
    return;
  if (!scannerEnabled)
    return;

  finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 150, R503_LED_YELLOW, 0);
}

void FingerprintManager::setLedRingReady()
{
  // P3-14: Ohne Verbindung zum Sensor keine LED-Kommunikation
  if (!connected)
    return;
  // NEU: Wenn Scanner deaktiviert ist, LED-Ring aus lassen
  if (!scannerEnabled)
  {
    return; // Nicht einschalten wenn deaktiviert
  }

  if (!ignoreTouchRing)
    finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 250, FINGERPRINT_LED_BLUE);
  else
    finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE); // just an indicator for me to see if touch ring is active or not
}

bool FingerprintManager::deleteAll()
{
  if (!connected)
  {
    LOG_PRINTLN(F("Fehler: Sensor nicht verbunden."));
    notifyClients("Fehler: Fingerprint-Sensor ist nicht verbunden.");
    return false;
  }

  LOG_PRINTLN(F("Lösche alle Fingerabdrücke einzeln via deleteModel()..."));

  bool allDeleted = true;
  for (int id = 1; id <= 200; id++)
  {
    uint8_t p = finger.deleteModel(id);
    // FINGERPRINT_OK = gelöscht, FINGERPRINT_PACKETRECIEVEERR = Komm.-Fehler (ignorieren),
    // FINGERPRINT_DELETEFAIL (0x10) = Slot war leer (kein Fehler, Ziel erreicht)
    if (p != FINGERPRINT_OK && p != FINGERPRINT_PACKETRECIEVEERR && p != 0x10)
    {
      LOG_PRINTLN(String("WARNUNG: deleteModel(") + id + ") fehlgeschlagen, Code: " + p);
      allDeleted = false;
    }
  }

  if (allDeleted)
  {
    LOG_PRINTLN(F("Alle Fingerabdrücke erfolgreich vom Sensor gelöscht."));
  }
  else
  {
    LOG_PRINTLN(F("WARNUNG: Einige Fingerabdrücke konnten nicht gelöscht werden."));
  }

  Preferences preferences;
  bool rc = preferences.begin("fs_flist", false);
  if (rc)
    rc = preferences.clear();
  preferences.end();

  for (int i = 1; i <= 200; i++)
  {
    fs_flist[i] = String("@empty");
  };

  return allDeleted;
}

uint8_t FingerprintManager::writeNotepad(uint8_t pageNumber, const char *text, uint8_t length)
{
  uint8_t data[34];
  memset(data, 0, sizeof(data)); // Uninitialisierte Rest-Bytes vermeiden (P1-08)

  if (length > 32)
    length = 32;

  data[0] = FINGERPRINT_WRITENOTEPAD;
  data[1] = pageNumber;
  for (int i = 0; i < length; i++)
    data[i + 2] = text[i];

  Adafruit_Fingerprint_Packet packet(FINGERPRINT_COMMANDPACKET, sizeof(data), data);
  finger.writeStructuredPacket(packet);
  if (finger.getStructuredPacket(&packet) != FINGERPRINT_OK)
    return FINGERPRINT_PACKETRECIEVEERR;
  if (packet.type != FINGERPRINT_ACKPACKET)
    return FINGERPRINT_PACKETRECIEVEERR;
  return packet.data[0];
}

uint8_t FingerprintManager::readNotepad(uint8_t pageNumber, char *text, uint8_t length)
{
  uint8_t data[2];

  data[0] = FINGERPRINT_READNOTEPAD;
  data[1] = pageNumber;

  Adafruit_Fingerprint_Packet packet(FINGERPRINT_COMMANDPACKET, sizeof(data), data);
  finger.writeStructuredPacket(packet);
  if (finger.getStructuredPacket(&packet) != FINGERPRINT_OK)
    return FINGERPRINT_PACKETRECIEVEERR;
  if (packet.type != FINGERPRINT_ACKPACKET)
    return FINGERPRINT_PACKETRECIEVEERR;

  if (packet.data[0] == FINGERPRINT_OK)
  {
    // read data payload
    for (uint8_t i = 0; i < length; i++)
    {
      text[i] = packet.data[i + 1];
    }
  }

  return packet.data[0];
}

String FingerprintManager::getPairingCode()
{
  char buffer[33];
  buffer[32] = 0; // null termination needed for convertion to string at the end
  if (readNotepad(0, (char *)buffer, 32) == FINGERPRINT_OK)
    return String((char *)buffer);
  else
    return "";
}

bool FingerprintManager::setPairingCode(const String &pairingCode)
{
  if (!connected)
  {
    LOG_PRINTLN(F("Fehler: Sensor nicht verbunden."));
    return false;
  }

  // Zero-Padded Buffer um Heap Over-Read bei kurzen Strings zu verhindern (P1-05)
  char buf[32] = {0};
  size_t copyLen = pairingCode.length() < 32 ? pairingCode.length() : 32;
  memcpy(buf, pairingCode.c_str(), copyLen);
  if (writeNotepad(0, buf, 32) == FINGERPRINT_OK)
    return true;
  else
    return false;
}

// ToDo: support sensor replacement by enable transferring of sensor DB to another sensor
void FingerprintManager::exportSensorDB()
{
}

void FingerprintManager::importSensorDB()
{
}

#if USE_DOWNLOAD_UPLOAD

// Import eines einzelnen Fingerabdruck-Templates in den Sensor
bool FingerprintManager::importSingleFingerprint(uint8_t id, uint8_t *templateData, size_t dataSize)
{
  if (!connected)
  {
    LOG_PRINTLN(F("[Upload] Fehler: Sensor nicht verbunden."));
    notifyClients("Fehler: Fingerprint-Sensor ist nicht verbunden.");
    return false;
  }

  if (id < 1 || id > 200)
  {
    LOG_PRINTLN(String("[Upload] Fehler: Ungültige ID: ") + id);
    notifyClients(String("Fehler: Ungültige ID ") + id);
    return false;
  }
  if (templateData == nullptr || dataSize == 0 || dataSize > FINGERPRINT_TEMPLATE_SIZE)
  {
    LOG_PRINTLN(String("[Upload] Fehler: Ungültige Template-Daten. Größe: ") + dataSize + " Bytes (Max: " + FINGERPRINT_TEMPLATE_SIZE + ")");
    notifyClients("Fehler: Ungültige Template-Daten.");
    return false;
  }

  LOG_PRINTLN(String("Starte Import für Finger-ID #") + id);

  /*  // HexDump der ersten 32 Bytes für Browser-Anzeige
    String hexDump = "[Upload] Template HexDump (erste 32 Bytes): ";
    for (size_t i = 0; i < 32 && i < dataSize; i++)
    {
      char buf[4];
      sprintf(buf, "%02X ", templateData[i]);
      hexDump += buf;
    }
    LOG_PRINTLN(hexDump);
    notifyClients(hexDump);
  */

  // UART-Puffer leeren
  while (mySerial.available())
    mySerial.read();

  // DownChar-Befehl senden (0x09, BufferID 0x01)
  uint8_t cmd[] = {0x09, 0x01};
  Adafruit_Fingerprint_Packet packet(FINGERPRINT_COMMANDPACKET, sizeof(cmd), cmd);
  finger.writeStructuredPacket(packet);
  if (finger.getStructuredPacket(&packet, 2000) != FINGERPRINT_OK || packet.type != FINGERPRINT_ACKPACKET || packet.data[0] != FINGERPRINT_OK)
  {
    LOG_PRINTLN(String("Fehler bei DownChar/Empfangsbereitschaft. Code: ") + packet.data[0]);
    notifyClients(String("Fehler bei Sensor-Vorbereitung (Code ") + packet.data[0] + ")");
    return false;
  }

  // Template-Daten in Paketen übertragen (MIT ACK-HANDSHAKE nach jedem Paket für Stabilität)
  LOG_PRINTLN(String("Sende Template-Daten an den Sensor (") + dataSize + " Bytes)...");
  const size_t templateSize = dataSize; // Tatsächliche Größe verwenden, NICHT die Max-Größe!
  // R-05: Paketgröße dynamisch aus Sensor-Parameter ableiten
  // finger.packet_len ist der Size-Code (0=32, 1=64, 2=128, 3=256) laut R503-Datenblatt
  size_t chunkSize;
  switch (finger.packet_len)
  {
  case 0:
    chunkSize = 32;
    break;
  case 1:
    chunkSize = 64;
    break;
  case 3:
    chunkSize = 256;
    break;
  default:
    chunkSize = 128;
    break; // Code 2 oder unbekannt → 128 (bisheriger Default)
  }
  LOG_PRINTLN(String("Paketgröße: ") + chunkSize + " Bytes (Code: " + finger.packet_len + ")");
  size_t numPackets = (templateSize + chunkSize - 1) / chunkSize;

  for (size_t i = 0; i < numPackets; ++i)
  {
    size_t offset = i * chunkSize;
    size_t chunk = (offset + chunkSize > templateSize) ? (templateSize - offset) : chunkSize;
    uint8_t pid = (i == numPackets - 1) ? FINGERPRINT_ENDDATAPACKET : FINGERPRINT_DATAPACKET;

    // Manuelles Erstellen und Senden des Pakets
    mySerial.write(FINGERPRINT_STARTCODE >> 8);
    mySerial.write(FINGERPRINT_STARTCODE & 0xFF);
    mySerial.write(finger.device_addr >> 24);
    mySerial.write(finger.device_addr >> 16);
    mySerial.write(finger.device_addr >> 8);
    mySerial.write(finger.device_addr & 0xFF);
    mySerial.write(pid);
    uint16_t len = chunk + 2;
    mySerial.write(len >> 8);
    mySerial.write(len & 0xFF);

    uint16_t sum = (len >> 8) + (len & 0xFF) + pid;
    for (size_t j = 0; j < chunk; j++)
    {
      sum += templateData[offset + j];
    }

    // Jetzt Daten senden (NACH Checksummen-Berechnung)
    mySerial.write(templateData + offset, chunk);
    mySerial.write(sum >> 8);
    mySerial.write(sum & 0xFF);

    LOG_PRINTLN(String("Paket ") + (i + 1) + "/" + numPackets + ", Bytes: " + chunk + " gesendet. Checksumme: " + sum);

    // Kurze Pause zwischen Paketen (von 10ms auf 2ms optimiert für Stabilität + Geschwindigkeit)
    delay(2);
  }
  LOG_PRINTLN("Alle Template-Pakete erfolgreich gesendet.");

  // Sensor braucht Zeit, um die Daten zu verarbeiten, bevor Store-Befehl akzeptiert wird
  delay(200);

  // UART-Puffer leeren, bevor Store-Befehl gesendet wird
  while (mySerial.available())
    mySerial.read();

  // Modell speichern
  uint8_t p = finger.storeModel(id);
  if (p == FINGERPRINT_OK)
  {
    LOG_PRINTLN(String("Fingerabdruck für ID ") + id + " erfolgreich im Sensor gespeichert.");
    notifyClients(String("✓ Fingerabdruck ID ") + id + " erfolgreich hochgeladen und gespeichert.");

    // UART-Puffer nach Download leeren
    while (mySerial.available())
      mySerial.read();
    delay(200);
    while (mySerial.available())
      mySerial.read();

    return true;
  }
  else
  {
    LOG_PRINTLN(String("Fehler beim Speichern des Modells im Sensor. Code: ") + p);
    notifyClients(String("Fehler beim Speichern des Fingerabdrucks (Code ") + p + ")");

    // UART-Puffer nach Download gründlich leeren
    for (int retry = 0; retry < 3; retry++)
    {
      while (mySerial.available())
        mySerial.read();
      delay(50);
    }

    return false;
  }
}

bool FingerprintManager::exportSingleFinger(uint8_t id)
{
  memset(exportedFingerData, 0, FINGERPRINT_TEMPLATE_SIZE);
  exportedFingerLength = 0;

  if (!connected)
  {
    LOG_PRINTLN(F("[Download] Fehler: Sensor nicht verbunden."));
    notifyClients("Fehler: Fingerprint-Sensor ist nicht verbunden.");
    return false;
  }

  if (id == 0 || id > 200)
  {
    notifyClients("Fehler: Ungültige Finger-ID für Export.");
    return false;
  }
  if (fs_flist[id].compareTo("@empty") == 0)
  {
    notifyClients("Fehler: Kein Fingerabdruck für diese ID vorhanden.");
    return false;
  }

  /*
    // Scanner-Parameter anzeigen
    String scannerInfo = "[Scanner] System-ID: 0x" + String(finger.system_id, HEX) +
                         ", Device-Addr: 0x" + String(finger.device_addr, HEX);
    LOG_PRINTLN(scannerInfo);
    notifyClients(scannerInfo);
  */

  // UART-Puffer leeren
  while (mySerial.available())
    mySerial.read();

  // Template aus Sensor laden
  uint8_t p = finger.loadModel(id);
  if (p != FINGERPRINT_OK)
  {
    notifyClients(String("Fehler beim Laden des Fingerabdrucks (Code ") + p + ")");
    return false;
  }
  p = finger.getModel();
  if (p != FINGERPRINT_OK)
  {
    notifyClients(String("Fehler beim Herunterladen des Templates (Code ") + p + ")");
    return false;
  }

  // P2-07: Template-Daten DYNAMISCH lesen — direkt in Member-Buffer exportedFingerData
  // (vorher: 2048+768 Bytes auf dem Stack → Stack-Overflow-Risiko bei 8KB Task-Stack)
  // exportedFingerData wurde am Funktionsanfang bereits mit memset(0) initialisiert
  size_t templateBytesReceived = 0;
  bool finished = false;
  unsigned long startTime = millis();

  LOG_PRINTLN("[Download] Lese Template-Pakete dynamisch...");

  while (!finished && (millis() - startTime < 5000) && templateBytesReceived < FINGERPRINT_TEMPLATE_SIZE)
  {
    // Header: 9 Bytes lesen
    uint8_t header[9];
    size_t headerRead = 0;
    while (headerRead < 9 && (millis() - startTime < 5000))
    {
      if (mySerial.available())
      {
        header[headerRead++] = mySerial.read();
      }
      else
      {
        delay(0);
      }
    }

    if (headerRead < 9)
    {
      LOG_PRINTLN("[Download] WARN: Header unvollständig");
      break;
    }

    // Paket-Info extrahieren
    uint8_t pid = header[6];
    uint16_t len = ((uint16_t)header[7] << 8) | header[8];

    if (len < 2)
    {
      LOG_PRINTLN("[Download] WARN: Ungültige Paketlänge");
      break;
    }

    size_t dataLen = len - 2; // Minus Checksum
    if (dataLen > 768)
    {
      LOG_PRINTLN("[Download] WARN: Paketlänge überschreitet 768 Bytes");
      break;
    }
    LOG_PRINTLN(String("[Download] Paket PID=0x") + String(pid, HEX) + ", Länge=" + dataLen);

    // P2-07: Datenpakete (0x02/0x08) direkt in exportedFingerData lesen,
    // unbekannte Pakete byte-weise verwerfen (kein Stack-Buffer nötig)
    bool isDataPacket = (pid == 0x02 || pid == 0x08);
    uint8_t *readTarget = nullptr;
    if (isDataPacket)
    {
      size_t copyLen = dataLen;
      if (templateBytesReceived + copyLen > FINGERPRINT_TEMPLATE_SIZE)
      {
        LOG_PRINTLN("[Download] WARN: Template größer als Puffer!");
        copyLen = FINGERPRINT_TEMPLATE_SIZE - templateBytesReceived;
      }
      readTarget = &exportedFingerData[templateBytesReceived];
    }

    // Daten lesen
    size_t dataRead = 0;
    while (dataRead < dataLen && (millis() - startTime < 5000))
    {
      if (mySerial.available())
      {
        uint8_t b = mySerial.read();
        if (readTarget && dataRead < (FINGERPRINT_TEMPLATE_SIZE - templateBytesReceived))
        {
          readTarget[dataRead] = b;
        }
        dataRead++;
      }
      else
      {
        delay(0);
      }
    }

    // Checksumme (2 Bytes) überspringen
    for (int i = 0; i < 2 && (millis() - startTime < 5000);)
    {
      if (mySerial.available())
      {
        mySerial.read(); // Checksum verwerfen
        i++;
      }
      else
      {
        delay(0);
      }
    }

    // Nur Datenpakete (PID 0x02 oder 0x08) zählen
    if (isDataPacket)
    {
      size_t actualCopied = (dataRead < dataLen) ? dataRead : dataLen;
      if (templateBytesReceived + actualCopied > FINGERPRINT_TEMPLATE_SIZE)
        actualCopied = FINGERPRINT_TEMPLATE_SIZE - templateBytesReceived;
      templateBytesReceived += actualCopied;

      LOG_PRINTLN(String("[Download] Datenpaket verarbeitet, Total: ") + templateBytesReceived + " Bytes");

      // End-Paket erkannt?
      if (pid == 0x08)
      {
        LOG_PRINTLN("[Download] End-Paket (0x08) erkannt. Download abgeschlossen.");
        finished = true;
      }
    }
    else
    {
      LOG_PRINTLN(String("[Download] WARN: Unbekannte PID 0x") + String(pid, HEX) + " übersprungen");
    }
  }

  // Prüfen, ob der Download abgeschlossen wurde (Timeout-Check)
  if (!finished)
  {
    LOG_PRINTLN("[Download] Fehler: Timeout beim Lesen des Templates.");
    notifyClients("Fehler: Timeout beim Herunterladen des Templates.");

    // UART-Puffer nach fehlgeschlagenem Versuch leeren, um den Sensor zu stabilisieren
    while (mySerial.available())
      mySerial.read();
    delay(200);
    while (mySerial.available())
      mySerial.read();

    return false;
  }

  // P2-07: Daten stehen bereits direkt in exportedFingerData (kein Zwischen-Buffer)
  exportedFingerLength = templateBytesReceived;

  // Debug-Logs gemäß Aufgabenstellung
  LOG_PRINTLN(String("[Download] exportSingleFinger: ID=") + id + ", exportLength=" + exportedFingerLength);
  // Warnung nur bei unplausiblen Werten (0, extrem klein oder größer als Puffer)
  if (exportedFingerLength == 0)
  {
    LOG_PRINTLN("[Download] WARN: Template-Länge ist 0 – Export vermutlich fehlgeschlagen.");
  }
  else if (exportedFingerLength < 64)
  {
    LOG_PRINTLN(String("[Download] WARN: Template sehr klein (") + exportedFingerLength + " Bytes) – möglicherweise defekt.");
  }
  else if (exportedFingerLength > FINGERPRINT_TEMPLATE_SIZE)
  {
    LOG_PRINTLN(String("[Download] WARN: Template größer als Puffer (") + exportedFingerLength + " > " + FINGERPRINT_TEMPLATE_SIZE + ")");
  }
  else
  {
    LOG_PRINTLN(String("[Download] Template-Größe: ") + exportedFingerLength + " Bytes (R503 typisch ~768)");
  }

  /* // Hex-Dump der ersten 32 Bytes
  String hexDump;
  for (size_t i = 0; i < 32 && i < exportedFingerLength; i++)
  {
    char buf[4];
    sprintf(buf, "%02X ", exportedFingerData[i]);
    hexDump += buf;
  }
  LOG_PRINTLN(String("[Download] HexDump (erste 32 Bytes): ") + hexDump);
  notifyClients(String("[Download] Template HexDump (erste 32 Bytes): ") + hexDump);
  */

  // Dateiname setzen
  String fingerName = fs_flist[id];
  String filename = String(id) + "_-_" + fingerName + ".tmpl";
  filename.replace(" ", "_");
  filename.replace("/", "_");
  filename.replace("\\", "_");
  filename.replace(":", "-");
  filename.replace("*", "_");
  filename.replace("?", "_");
  filename.replace("\"", "_");
  filename.replace("<", "_");
  filename.replace(">", "_");
  filename.replace("|", "_");
  exportedFilename = filename;

  notifyClients(String("✓ Fingerabdruck ID ") + id + " exportiert: " + exportedFilename);

  // UART-Puffer nach Download robust leeren (ruhige Leitung statt fixer Wartezeit)
  flushSerialWithQuietTimeout(mySerial, 30, 400);
  return true;
}

const String &FingerprintManager::getExportedFingerprintFilename() const
{
  return exportedFilename;
}

size_t FingerprintManager::getExportedFingerprintLength() const
{
  return exportedFingerLength;
}
#endif