// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
//******************************************************
//         Main of Fingerscanner Parip69.
// nur hier die start wert der version Aendern.OK=======
// @version: 2.2.789 <br> Builddatum 20:52:45 03-05.2026
//****************************************************
#ifndef USE_MQTT_BROKER
#define USE_MQTT_BROKER 1
#endif
#ifndef USE_MQTT_CLIENT
#define USE_MQTT_CLIENT 1
#endif
#define USE_MQTT_ANY (USE_MQTT_BROKER || USE_MQTT_CLIENT)

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <DNSServer.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#if USE_MQTT_CLIENT
#include <AsyncMqttClient.h>
#endif
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "esp_err.h"
// ElegantOTA nach ESPAsyncWebServer einbinden
#include <ElegantOTA.h>
#if USE_MQTT_BROKER
#include "ESPAsyncMQTTBroker.h"
#endif
#include "SettingsManager.h"
#if USE_MQTT_CLIENT
#include "MqttConnectionManager.h"
#include "Bridge.h" // <-- NEU
#endif
#include "global.h"

#if USE_DHT
#include "DHTManager.h"
#endif

#if USE_TEDEE
#include "Tedee.h"
#endif

#if USE_NUKI
#include "Nuki.h"
#endif

#if USE_TELEGRAM
#include "Telegram.h"
#endif

#include <ArduinoJson.h>
// debug.h entfernt -> Logging jetzt in global.h integriert

// Guards & Defaults für DHT
#ifndef USE_DHT
#define USE_DHT 0
#endif

// Guard & Default für Tedee (falls nicht über build_flags definiert)
#ifndef USE_TEDEE
#define USE_TEDEE 0
#endif

// Guard & Default für Nuki (falls nicht über build_flags definiert)
#ifndef USE_NUKI
#define USE_NUKI 0
#endif

// Guard & Default für Telegram (falls nicht über build_flags definiert)
#ifndef USE_TELEGRAM
#define USE_TELEGRAM 0
#endif

// Guard & Default für Download/Upload-Funktionalität
#ifndef USE_DOWNLOAD_UPLOAD
#define USE_DOWNLOAD_UPLOAD 1
#endif

#if USE_MQTT_BROKER
// Broker Check Intervall
#ifndef BROKER_CHECK_MIN_INTERVAL
#define BROKER_CHECK_MIN_INTERVAL 5000 // 0.2 Hz Wartung/Reconnector
#endif

// MQTT Boot Wait
#ifndef MQTT_BOOT_WAIT_MS
#define MQTT_BOOT_WAIT_MS 5000 // 5s Gnadenfrist, bis Broker hochgefahren ist
#endif
#endif // USE_MQTT_BROKER

#include <ESPmDNS.h>
//////////////////////////////////////////////////////////////////////
const char* firmwareVersion = "2.2.789 <br> Builddatum 20:52:45 03-05.2026"; // Firmware Versio
// ===== UPLOAD-KONTEXT für multipart-Verarbeitung =====
// Hilfs-Struktur, um Daten vom Upload- zum Request-Handler zu transportieren
#if USE_DOWNLOAD_UPLOAD
struct UploadContext
{
	uint8_t *buffer;
	size_t total_len;
	String filename;
	bool overflowed; // true wenn Upload größer als Puffer
};
#endif
// Online-Erkennung (simple Heuristik)
extern bool loggedIn; // falls weiter unten definiert, hier nur Vorwärtsdeklaration

// Global mDNS-Status (für MQTT-Broker-Service-Registrierung)
bool mdnsStarted = false;

// ============================================================================
// SSE-Cache für Performance-Optimierung
// Verhindert redundante Updates und reduziert Netzwerk-Last
// ============================================================================
struct SSECache
{
	String lastWifiQuality = "";
	String lastMqttStatus = "";
	String lastChipTemp = "";
	String lastBrokerClients = "";
	unsigned long lastUpdate = 0;
	const unsigned long UPDATE_INTERVAL = 1000; // Max 1 Update/Sekunde

	bool shouldUpdate()
	{
		unsigned long now = millis();
		if (now - lastUpdate < UPDATE_INTERVAL)
		{
			return false;
		}
		lastUpdate = now;
		return true;
	}

	bool wifiQualityChanged(const String &newValue)
	{
		if (newValue != lastWifiQuality)
		{
			lastWifiQuality = newValue;
			return true;
		}
		return false;
	}

	bool mqttStatusChanged(const String &newValue)
	{
		if (newValue != lastMqttStatus)
		{
			lastMqttStatus = newValue;
			return true;
		}
		return false;
	}

	bool chipTempChanged(const String &newValue)
	{
		if (newValue != lastChipTemp)
		{
			lastChipTemp = newValue;
			return true;
		}
		return false;
	}

	bool brokerClientsChanged(const String &newValue)
	{
		if (newValue != lastBrokerClients)
		{
			lastBrokerClients = newValue;
			return true;
		}
		return false;
	}
};
static SSECache sseCache;

// Watchdog Timer Bibliothek einbinden
#include <esp_task_wdt.h>
// Brownout Detektor Bibliothek einbinden (minimal)
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
// +++ WLAN Energiespar- und SNTP-APIs des ESP32
#include "esp_wifi.h"
#include "esp_sntp.h"
// Watchdog-Timer-Konfiguration
#define WDT_TIMEOUT 120 // Zeitlimit in Sekunden, nach dem ein Reset ausgelöst wird (erhöht für Konfigurationsmodus)

// Build-Trigger (z. B. aktuelles PlatformIO-Environment) aus Build-Flag übernehmen
#ifndef BUILD_TRIGGER
#define BUILD_TRIGGER "unknown"
#endif
static const char *buildTrigger = BUILD_TRIGGER;

#ifndef DIAG_LOG_ENABLED
#define DIAG_LOG_ENABLED 0
#endif

static const uint32_t HTTP_ACTIVITY_WINDOW_MS = 15000UL;
static const uint32_t NETWORK_GRACE_AFTER_BOOT_MS = 120000UL;
static uint32_t lastHttpActivityMs = 0;

#if DIAG_LOG_ENABLED
#define DIAG_PRINTF(tag, fmt, ...) \
	do { Serial.printf("[%-4s +%06lu ms] " fmt "\n", (tag), (unsigned long)millis(), ##__VA_ARGS__); } while (0)
#define DIAG_LOG(tag, text) DIAG_PRINTF((tag), "%s", (text))
#else
#define DIAG_PRINTF(tag, fmt, ...)
#define DIAG_LOG(tag, text)
#endif

#if USE_TELEGRAM
// ===== Netzwerk-Beschäftigt-Ampel für Telegram =====
// Drei Zustände: WLAN-Scan/Roaming aktiv | großer HTTP-Transfer gestartet | Heap kritisch
static volatile bool wifiBusyForTelegram = false;
static volatile uint32_t lastHeavyHttpTransferMs = 0;
static const uint32_t TELEGRAM_HTTP_PAUSE_MS = 5000UL;

static void setWifiBusyForTelegram(bool active)
{
	wifiBusyForTelegram = active;
	DIAG_PRINTF("WIFI", "busy for telegram: %s", active ? "on" : "off");
}

static void noteHeavyHttpTransferForTelegram()
{
	lastHeavyHttpTransferMs = millis();
	DIAG_LOG("HTTP", "heavy transfer noted");
}

bool isTelegramNetworkPauseActive()
{
	if (wifiBusyForTelegram)
	{
		DIAG_LOG("NET ", "telegram pause: wifi busy");
		return true;
	}
	if (millis() - lastHeavyHttpTransferMs < TELEGRAM_HTTP_PAUSE_MS)
	{
		DIAG_LOG("NET ", "telegram pause: heavy http");
		return true;
	}
	if (ESP.getFreeHeap() < 50000UL)
	{
		DIAG_LOG("NET ", "telegram pause: low heap");
		return true;
	}
	return false;
}
// ===== Ende Netzwerk-Beschäftigt-Ampel =====
#endif // USE_TELEGRAM

static void noteHttpActivity(AsyncWebServerRequest *request)
{
	if (!request)
		return;

	lastHttpActivityMs = millis();

	// Große HTML/CSS-Dateien für Telegram-Pause markieren (immer, unabhängig von DIAG)
#if USE_TELEGRAM
	{
		const String &p = request->url();
		if (p == "/" || p == "/index.html" || p == "/settings.html" ||
			p == "/login.html" || p == "/wificonfig.html" ||
			p == "/ui-settings-modules" || p == "/bootstrap.min.css")
		{
			noteHeavyHttpTransferForTelegram();
		}
	}
#endif

#if DIAG_LOG_ENABLED
	const String path = request->url();

	if (!(path == "/" ||
		  path == "/index.html" ||
		  path == "/settings.html" ||
		  path == "/login.html" ||
		  path == "/wificonfig.html" ||
		  path == "/events" ||
		  path == "/ui-alive" ||
		  path == "/ui-settings-modules" ||
		  path == "/bootstrap.min.css" ||
		  path == "/icon-192.png"))
	{
		return;
	}

	const char *methodName = "HTTP";
	const uint8_t method = (uint8_t)request->method();

	if (method & HTTP_GET)
		methodName = "GET";
	else if (method & HTTP_POST)
		methodName = "POST";
	else if (method & HTTP_PUT)
		methodName = "PUT";
	else if (method & HTTP_DELETE)
		methodName = "DELETE";

	DIAG_PRINTF("HTTP", "%s %s", methodName, path.c_str());
#endif
}

#if DIAG_LOG_ENABLED
static void diagLogReadyStatus(const char *message)
{
	const int rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
	const IPAddress ip = WiFi.localIP();
	DIAG_PRINTF("READY", "%s, heap=%u, rssi=%d, ip=%s",
				message,
				(unsigned)ESP.getFreeHeap(),
				rssi,
				ip.toString().c_str());
}
#endif

bool isBootNetworkGraceActive()
{
	return millis() < NETWORK_GRACE_AFTER_BOOT_MS;
}

static bool isHttpRecentlyActive()
{
	const uint32_t last = lastHttpActivityMs;
	return last != 0 && (uint32_t)(millis() - last) < HTTP_ACTIVITY_WINDOW_MS;
}

// Erstelle eine zusätzliche Information mit der Versionsnummer und Builddatum.
String VersionInfo = String(firmwareVersion) + "<br>*** Parip69 *** | Event: " + String(buildTrigger) + ".";

// MQTT Broker (dynamisch wegen konfigurierbarem Port)
#if USE_MQTT_BROKER
ESPAsyncMQTTBroker *mqttBroker = nullptr;
#endif

#if USE_DHT
DHTManager dhtManager;
#endif

// Vorwärtsdeklarationen für Funktionen
void syncNtpTimeIfNeeded();
bool syncWithCustomNtpServers(const String &tzString, struct tm &timeinfo_ref);													 // NEU
bool trySyncWithNtpServer(const char *serverName, const char *tzString, struct tm &timeinfo_ref, const char *sourceDescription); // NEU
bool initWifi();
void publishMqttMessage(const String &topic, const String &message, bool retain = false, int qos = 0);
bool connectToBestAP(bool bootScan = false, bool forceBestAp = false); // NEU: Vorwärtsdeklaration für die Funktion zur Auswahl des besten APs


// Vorwärtsdeklaration für handleMqttMessage
#if USE_MQTT_ANY
void handleMqttMessage(const String &topic, const String &payload, const String &source, bool retained = false);
#endif

// +++ SNTP-Callback: setzt nur ein leichtes Log, Zeit wird vom System gestellt
static void onTimeSync(struct timeval *tv)
{
	LOG_PRINTLN("[NTP] Zeit synchronisiert (Callback)");
	timeSet = true;
}

// P4-05: Deduplizierte WiFi-RSSI→Qualitätsberechnung (statt 3-fach identisch)
static int rssiToQuality(int rssi) {
	if (rssi <= -100) return 0;
	if (rssi >= -30) return 100;
	return map(rssi, -100, -30, 0, 100);
}

// +++ SNTP-Intervall/Callback konfigurieren (nur einmalig aufrufen)
static void configureSntp()
{
	// Intervall auf 24 Stunden setzen (Millisekunden)
	sntp_set_sync_interval(24UL * 60UL * 60UL * 1000UL);
	sntp_set_time_sync_notification_cb(onTimeSync);
}

// Standard-Zeitzone mit automatischer Umstellung
#if USE_MQTT_BROKER
void onMqttBrokerClientDisconnect(String clientID);
#endif
String tzInfo = "CET-1CEST,M3.5.0/2,M10.5.0/3"; // Berlin (MEZ/MESZ)
String timezoneOffset;
String timezoneName;
String usedNtpSource = "nicht synchronisiert"; // Speichert die verwendete NTP-Quelle
int lastNtpDay = -1;
volatile bool timeSet = false;
static bool ntpQuickSyncPending = false;
static unsigned long ntpQuickSyncEarliestAt = 0;
static uint8_t ntpRetryCount = 0;
static String lastSuccessfulNtpServer = "";
static unsigned long lastDailyNtpAttemptAt = 0; // Throttle für täglichen NTP-Resync bei Fehlschlag

static void resetNtpRetryState()
{
	ntpRetryCount = 0;
	LOG_PRINTLN("[NTP] Retry-Zähler zurückgesetzt");
}

// Globale Variablen für Netzwerk- und MQTT-Verbindung
unsigned long lastNetworkCheck = 0;
const unsigned long networkCheckInterval = 30000; // 30s - weniger aggressiv (Production)

extern SettingsManager settingsManager; // Vorwärtsdeklaration

// +++ WLAN: Exponentieller Backoff & RSSI/Jitter
static const unsigned long WIFI_BACKOFF_STEPS[] = {5000, 15000, 30000, 60000, 120000, 300000};
static const size_t WIFI_BACKOFF_STEPS_LEN = sizeof(WIFI_BACKOFF_STEPS) / sizeof(WIFI_BACKOFF_STEPS[0]);
size_t wifiBackoffIndex = 0;
unsigned long wifiNextAttemptAt = 0;

// +++ WLAN: deterministischer Reconnect (Pending/ForceBegin)
volatile bool wifiReconnectPending = false;
volatile bool wifiReconnectForceBegin = false;
volatile bool wifiJustReconnected = false; // Flag: LED-Ring in loop() auf Ready setzen

static void requestWifiReconnect(uint32_t delayMs, bool forceBegin)
{
	wifiReconnectPending = true;
	if (forceBegin)
		wifiReconnectForceBegin = true;

	const unsigned long now = millis();
	const unsigned long at = now + delayMs;
	if (wifiNextAttemptAt == 0 || at < wifiNextAttemptAt)
		wifiNextAttemptAt = at;
}

int RSSI_DISCONNECT_THRESHOLD = -85; // Feld 1: kritischer RSSI-Wert fuer weiche WLAN-Rettung
static bool wifiInitPhase = false; // Während initWifi() kein Backoff-Eskalation im DISCONNECT-Event
static unsigned long wifiConnectedSince = 0; // Zeitpunkt der letzten Verbindung (Schonfrist)
const unsigned long RSSI_GRACE_PERIOD_MS = 60000; // 60s Schonfrist nach Connect – kein RSSI-Disconnect
static unsigned long wifiOfflineSince = 0; // Zeitpunkt seit Offline (0 = online)
// BSSID-Lock (beibehalten)
uint8_t lockedBssid[6] = {0};
int lockedChannel = 0;
bool hasLockedBssid = false;

struct WifiBestApCache
{
	bool valid = false;
	int rssi = -127;
	int channel = 0;
	uint8_t bssid[6] = {0};
	unsigned long updatedAt = 0;
};

static WifiBestApCache wifiBestApCache;
static const unsigned long WIFI_BEST_AP_CACHE_VALID_MS = 3UL * 60UL * 1000UL;

static void rememberWifiBestAp(int rssi, int channel, const uint8_t *bssid)
{
	if (!bssid || channel <= 0)
		return;

	wifiBestApCache.valid = true;
	wifiBestApCache.rssi = rssi;
	wifiBestApCache.channel = channel;
	memcpy(wifiBestApCache.bssid, bssid, 6);
	wifiBestApCache.updatedAt = millis();
}

static bool isWifiBestApCacheFresh()
{
	return wifiBestApCache.valid &&
		   wifiBestApCache.channel > 0 &&
		   (millis() - wifiBestApCache.updatedAt) <= WIFI_BEST_AP_CACHE_VALID_MS;
}

enum class WifiPerformanceProfile
{
	Balanced,
	Stability
};
WifiPerformanceProfile wifiPerformanceProfile = WifiPerformanceProfile::Balanced;
unsigned long wifiStableSince = 0;
long wifiLastMeasuredRssi = -127;
const unsigned long WIFI_STABLE_RESTORE_MS = 5UL * 60UL * 1000UL;
const long RSSI_GOOD_THRESHOLD = -65;

static void applyWifiPerformanceProfile(WifiPerformanceProfile newProfile)
{
	static bool profileInitialized = false;

	if (profileInitialized && wifiPerformanceProfile == newProfile)
	{
		return;
	}

	wifiPerformanceProfile = newProfile;
	profileInitialized = true;

	if (newProfile == WifiPerformanceProfile::Balanced)
	{
		WiFi.setSleep(true);
		esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
		WiFi.setTxPower(WIFI_POWER_15dBm);
		LOG_PRINTLN("[WiFi] Power profile -> Balanced (modem sleep, 15 dBm)");
	}
	else
	{
		WiFi.setSleep(false);
		esp_wifi_set_ps(WIFI_PS_NONE);
		WiFi.setTxPower(WIFI_POWER_19_5dBm);
		LOG_PRINTLN("[WiFi] Power profile -> Stability (sleep off, max TX)");
	}
}

// +++ MQTT: Reconnect-Management Variablen ENTFERNT, jetzt im MqttConnectionManager +++
// Globale Wartungsmarke (nur anlegen, wenn nicht vorhanden)
#if USE_MQTT_BROKER
static unsigned long lastBrokerMaintenance = 0;
#endif

// String globalToken;
//  #define LATITUDE        48.777444
//  #define LONGITUDE       11.619619
//  #define DST_OFFSET      2
#define TOUCH_PIN T0 // Das ist pin 4 und loest eine WiFi konfiguration aus.
int threshold = 50;	 // 50 war die einstellung
unsigned long wifiConfigStartTime = 0;

// Globale Variable für MQTT-Topic
#if USE_MQTT_ANY
String mqttRootTopic = "MatterMQTTBridge"; // Standard-Wert, wird später überschrieben
#else
// Minimaler Stub (12 Bytes BSS) – wird von Web-Handlern referenziert die publishMqttMessage (No-Op) aufrufen
static const String mqttRootTopic;
#endif

int led1State = LOW; // Zustand der LED1
// SolarCalc solarCalc(LATITUDE, LONGITUDE, DST_OFFSET);
// SolarCalc solarCalc(48.777444, 11.619619);.

// extern SolarCalc solarCalc; // Legacy entfernt

// Funktion zum Trennen und Säubern (Trimmen) der Zeitzonen-Strings timezoneName, timezoneOffset
void splitAndTrim(const String &input, const String &delimiter, String &part1, String &part2);

// WLAN-Qualität Push-Intervall
const unsigned long intervalWifiQuality = 30000UL; // 30s nur bei verbundenen Clients

enum class Mode
{
	scan,
	wificonfig,
	maintenance
};

// Benutzer benutzer; // Benutzerklasse-Instanz

// ===================================================================================================================
// Caution: below are not the credentials for connecting to your home network, they are for the Access Point mode!!!
// ===================================================================================================================
const char *WifiConfigSsid = "MatterMQTTBridgeConfig"; // SSID used for WiFi when in Access Point mode for configuration
const char *WifiConfigPassword = "12345678";		// password used for WiFi when in Access Point mode for configuration. Min. 8 chars needed!
IPAddress WifiConfigIp(192, 168, 4, 1);				// IP of access point in wifi config mode-

// FingerprintManager fingerManager; // Legacy entfernt
SettingsManager settingsManager;

const int logMessagesCount = 20;
String logMessages[logMessagesCount]; // Ringpuffer für Log-Meldungen
int logNextIndex = 0;				  // Zeiger auf nächste Schreibposition (0..logMessagesCount-1)
bool shouldReboot = false;
bool otaInProgress = false;
#if USE_MQTT_CLIENT
bool mqttExpectedDisconnect = false;
#endif

// =========================
// MQTT Topic-Helper
// =========================
#if USE_MQTT_ANY
String getModeString()
{
	const AppSettings &app = settingsManager.getAppSettings();
	if (app.mqtt_isBroker)
		return "broker";
	if (app.mqtt_isClient)
		return "client";
	return "off";
}

String makeTopic(const String &tail)
{
	const AppSettings &app = settingsManager.getAppSettings();
	const WifiSettings wifi = settingsManager.getWifiSettings();
	const String &root = app.mqttRootTopic.length() ? app.mqttRootTopic : (const String &)String("MatterMQTTBridge");
	String t;
	t.reserve(root.length() + wifi.hostname.length() + tail.length() + 16);
	t = root;
	t += "/";
	t += getModeString();
	t += "/";
	t += wifi.hostname;
	t += "/";
	t += tail;
	return t;
}
#endif // USE_MQTT_ANY

// [Legacy] enrollId/enrollName entfernt
Mode currentMode = Mode::scan;

const byte DNS_PORT = 53;
DNSServer dnsServer;

String www_password; // Hier das gemeinsame Passwort eintragen
String ENABLE_PASSWORD;

AsyncWebServer webServer(80); // AsyncWebServer  on port 80

// F6: loggedIn ist global für ALLE Clients – logisch unsauber.
// Ein sauberer Session-/Cookie-/IP-Ansatz würde 13+ Prüfstellen ändern.
// Umbau bewusst zurückgestellt, da halber Auth-Umbau schlimmer wäre als Status quo.
bool loggedIn = false;
unsigned long lastActivityTime = 0;

// Globale Definitionen
SemaphoreHandle_t networkMutex = NULL;

SemaphoreHandle_t logMutex = NULL; // schützt logMessages[]/logNextIndex gegen Race (Async-Tasks)

SemaphoreHandle_t sseSendMutex = NULL; // serialisiert events.send(...) (Try-Lock), init in setup()

// DEGRADED-Modus: true wenn kritische Mutexe nicht erstellt werden konnten.
// Im DEGRADED-Modus: Kern (Fingerprint/Relais/WLAN/Web/MQTT) bleibt aktiv,
// Zusatzmodule (Telegram/Tedee) und Komfort-SSE werden deaktiviert.
static bool degradedMode = false;

// ===== NET-MUTEX Helper (best-effort, kurz halten) =====
static bool netTryLock(uint32_t timeoutMs, bool &locked)
{
	locked = false;
	if (!networkMutex)
		return true; // kein Mutex -> als "ok" behandeln
	locked = (xSemaphoreTake(networkMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
	return locked;
}
static void netUnlock(bool locked)
{
	if (locked && networkMutex)
		xSemaphoreGive(networkMutex);
}
#if USE_MQTT_ANY
String mqttStatusString = "disabled"; // NEU: Globaler MQTT-Status für Live-Feedback
char mqttClientId[64] = {0};		  // MQTT Client ID global definiert
static int mqttPort = 1883;			  // Standard-MQTT-Port als globale Variable (nicht mehr const)
#endif

#if USE_MQTT_CLIENT
static volatile int mqttClientDisconnectReasonPending = (int)AsyncMqttClientDisconnectReason::TCP_DISCONNECTED;
static volatile bool mqttClientConnectEventPending = false;
static volatile bool mqttClientDisconnectEventPending = false;
static volatile bool mqttClientConnectSessionPresentPending = false;
static volatile bool mqttClientDisconnectWasExpectedPending = false;
static volatile bool mqttClientDisconnectWifiConnectedPending = false;
static unsigned long mqttClientReconnectGraceStartedAt = 0;
static String mqttClientReconnectGraceReason = "";
static const unsigned long MQTT_CLIENT_RECONNECT_GRACE_MS = 10000;
#endif

AsyncEventSource events("/events"); // event source (Server-Sent events)

// ===== SSE Client Status Flag & Heartbeat =====
static bool sseClientConnected = false;			 // true = min. 1 Client aktiv, false = kein Client
static unsigned long lastUiAliveTime = 0;		 // Zeitstempel des letzten UI-Heartbeats
const unsigned long UI_ALIVE_TIMEOUT_MS = 30000; // 30s Timeout für UI-Keep-Alive

// UI gilt nur als aktiv, wenn ein SSE-Client da ist UND der Heartbeat frisch ist.
// Damit vermeiden wir Heap/CPU-Arbeit, wenn niemand im Web schaut.
inline bool isUiActive()
{
	return sseClientConnected && (millis() - lastUiAliveTime <= UI_ALIVE_TIMEOUT_MS);
}

bool hasActiveWebClients()
{
	if (isHttpRecentlyActive())
		return true;
	if (events.count() > 0)
		return true;
	return isUiActive();
}

// NEU: MQTT-Instanzen
#if USE_MQTT_CLIENT
AsyncMqttClient mqttClient;
MqttConnectionManager mqttManager(mqttClient, settingsManager);
#endif

// Forward (ohne Default-Args): updateMqttStatus ruft explizit alle Parameter auf
inline void sendSSEEvent(const char *data, const char *event, unsigned long id, uint32_t retry);

#if USE_MQTT_ANY
// ===== STABILER KERNBEREICH =====
// MQTT-Status bewusst auf wenige klare Hauptzustaende reduziert:
// online / offline / reconnecting / disabled
// UI-Texte absichtlich kurz und ruhig halten.
// Keine neuen Sonderstatus oder zusaetzliche notifyClients()-Texte einfuehren,
// solange kein echter funktionaler Bedarf besteht.
static void updateMqttStatus(const char *status)
{
	if (mqttStatusString == status)
	{
		return;
	}

	const AppSettings app = settingsManager.getAppSettings();
	const bool brokerMode = app.mqtt_isBroker;
	const bool clientMode = app.mqtt_isClient && !brokerMode;
	// Nur wichtige, knappe Zustandsmeldungen an die UI schicken.
	// Detailstatus bleiben ueber mqtt_status / Log sichtbar.
	if (strcmp(status, "offline") == 0)
	{
		if (brokerMode)
		{
			notifyClients("MQTT-Broker: inaktiv");
		}
		else if (clientMode)
		{
			notifyClients("MQTT-Client: offline - Lokalbetrieb aktiv");
		}
		else
		{
			notifyClients("MQTT: offline - Lokalbetrieb aktiv");
		}
	}
	else if (strcmp(status, "online") == 0)
	{
		if (brokerMode)
		{
			notifyClients("MQTT-Broker: aktiv");
		}
		else if (clientMode)
		{
			notifyClients("MQTT-Client: online");
		}
		else
		{
			notifyClients("MQTT: online");
		}
	}
	else if (strcmp(status, "disabled") == 0)
	{
		notifyClients("MQTT: deaktiviert");
	}
	else if (strcmp(status, "reconnecting") == 0)
	{
		// absichtlich keine notifyClients()-Meldung:
		// kurzer Reconnect soll die UI nicht zuspammen
	}
	mqttStatusString = status;
	sendSSEEvent(status, "mqtt_status", 0, 0);
}
#endif // USE_MQTT_ANY

// ===== SSE Event Wrapper: Zentrale Funktion für alle Event-Sends =====
// Diese Funktion kümmert sich automatisch um die Prüfung, ob Clients verbunden sind
inline void sendSSEEvent(const char *data, const char *event, unsigned long id = 0, uint32_t retry = 0)
{
	if (!isUiActive())
	{
		return;
	}

	// Wichtig: sendSSEEvent wird auch aus esp_timer-Callbacks genutzt -> nicht blockieren.
	// Gleichzeitig serialisieren: events.send(...) ist nicht thread-safe (Race/Heap/Deadlock-Risiko).
	if (!sseSendMutex)
	{
		return;
	}

	if (xSemaphoreTake(sseSendMutex, 0) != pdTRUE)
	{
		return;
	}

	if (id == 0)
	{
		events.send(data, event);
	}
	else if (retry == 0)
	{
		events.send(data, event, id);
	}
	else
	{
		events.send(data, event, id, retry);
	}

	xSemaphoreGive(sseSendMutex);
}

#if USE_MQTT_CLIENT
static const char *mqttDisconnectReasonToString(AsyncMqttClientDisconnectReason reason)
{
	switch (reason)
	{
	case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:
		return "TCP disconnected";
	case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION:
		return "unacceptable protocol version";
	case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED:
		return "identifier rejected";
	case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:
		return "server unavailable";
	case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS:
		return "malformed credentials";
	case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:
		return "not authorized";
	case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE:
		return "not enough space";
	case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT:
		return "TLS fingerprint mismatch";
	default:
		return "unknown";
	}
}
#endif // USE_MQTT_CLIENT

// TimerHandle_t mqttReconnectTimer;

// Match lastMatch; // Legacy entfernt

void addLogMessage(const String &message)
{
	// Maximale Länge einer Lognachricht im Ringspeicher
	const size_t MAX_LOG_MSG_LEN = 300;

	// Kopie der Nachricht anlegen und bei Bedarf kürzen
	String shortened = message;
	if (shortened.length() > MAX_LOG_MSG_LEN)
	{
		shortened = shortened.substring(0, MAX_LOG_MSG_LEN - 3);
		shortened += "...";
	}

	// Schreibe gekürzte Nachricht an aktuelle Position und erhöhe Zeiger zirkulär
	if (logMutex)
	{
		xSemaphoreTake(logMutex, portMAX_DELAY);
	}
	logMessages[logNextIndex] = shortened;
	logNextIndex = (logNextIndex + 1) % logMessagesCount;
	if (logMutex)
	{
		xSemaphoreGive(logMutex);
	}
}

String getLogMessagesAsHtml()
{
	// Ausgabe von ältester zu neuester Nachricht
	String html;
	// Puffer reservieren, um Fragmentierung zu reduzieren (ca. 20 * 80 Zeichen)
	html.reserve(1800);
	if (logMutex)
	{
		xSemaphoreTake(logMutex, portMAX_DELAY);
	}
	for (int i = 0; i < logMessagesCount; i++)
	{
		int idx = (logNextIndex + i) % logMessagesCount; // älteste zuerst
		if (logMessages[idx] != "")
		{
			html += logMessages[idx];
			html += "<br>";
		}
	}
	if (logMutex)
	{
		xSemaphoreGive(logMutex);
	}

	return html;
}

// Zusätzliche globale Variablen

String getTimestampString()
{
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo, 100)) // Kurzer Timeout, um Blockieren zu verhindern
	{
		return "Zeit nicht synchronisiert";
	}

	char buffer[40];
	snprintf(buffer, sizeof(buffer), "%02d:%02d - %02d.%02d.%04d",
			 timeinfo.tm_hour, timeinfo.tm_min,
			 timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
	return String(buffer);
}

// Prüft, ob eine NTP-Synchronisierung notwendig ist – kurz und opportunistisch
void syncNtpTimeIfNeeded()
{
	// NTP-Synchronisierung nur versuchen, wenn nicht im WiFi-Konfigurationsmodus
	if (currentMode == Mode::wificonfig)
	{
		return;
	}

	if (WiFi.status() != WL_CONNECTED)
	{
		return;
	}

	struct tm timeinfo;
	if (!getLocalTime(&timeinfo, 200)) // Kurzer Check ob Zeit schon da ist
	{
		// Zeit nicht verfügbar: pro Aufruf nur EINEN Server versuchen (max ~2.5s statt ~9s Blockierung)
		// Bei Retry wird der nächste Server probiert (Rotation)
		LOG_PRINTLN("[NTP] Zeit nicht verfügbar – versuche Synchronisierung...");

		String ntpOffsetString = settingsManager.getAppSettings().ntpOffset;
		splitAndTrim(ntpOffsetString, "|", timezoneName, timezoneOffset);
		String tzString = timezoneOffset.isEmpty() ? tzInfo : timezoneOffset;

		bool syncOK = false;
		static uint8_t ntpServerRotation = 0;

		// Bevorzuge den zuletzt erfolgreichen Server (Laufzeit-Optimierung)
		if (lastSuccessfulNtpServer.length() > 0)
		{
			syncOK = trySyncWithNtpServer(lastSuccessfulNtpServer.c_str(), tzString.c_str(), timeinfo, "Bevorzugt");
		}

		if (!syncOK)
		{
			uint8_t serverIdx = ntpServerRotation % 3;
			ntpServerRotation++;

			switch (serverIdx)
			{
			case 0: // Router-IP
			{
				IPAddress gateway = WiFi.gatewayIP();
				if (gateway != IPAddress(0, 0, 0, 0))
				{
					char routerIp[16];
					sprintf(routerIp, "%d.%d.%d.%d", gateway[0], gateway[1], gateway[2], gateway[3]);
					syncOK = trySyncWithNtpServer(routerIp, tzString.c_str(), timeinfo, "Router");
				}
				break;
			}
			case 1: // Benutzerdefinierte Server
				syncOK = syncWithCustomNtpServers(tzString, timeinfo);
				break;
			case 2: // pool.ntp.org als Fallback
				syncOK = trySyncWithNtpServer("pool.ntp.org", tzString.c_str(), timeinfo, "Fallback");
				break;
			}
		}

		if (syncOK)
		{
			lastNtpDay = timeinfo.tm_mday;
			timeSet = true;
			ntpServerRotation = 0; // Rotation nach Erfolg zurücksetzen
			addLogMessage("🕒 NTP-Zeit synchronisiert");
			LOG_PRINTLN("[NTP] Zeit erfolgreich synchronisiert.");
		}
		else
		{
			LOG_PRINTLN("[NTP] Sync fehlgeschlagen – wird später erneut versucht.");
		}
		return;
	}

	// Zeit ist bereits verfügbar: tägliche Nachsynchronisation prüfen
	// Nur EIN Server pro Aufruf (Rotation) – bei Fehlschlag wird tryNtpSyncDaily() erneut auslösen
	int currentDay = timeinfo.tm_mday;
	if (currentDay != lastNtpDay || lastNtpDay == -1)
	{
		LOG_PRINTLN("[NTP] Tägliche Zeit-Synchronisierung wird durchgeführt...");

		String ntpOffsetString = settingsManager.getAppSettings().ntpOffset;
		splitAndTrim(ntpOffsetString, "|", timezoneName, timezoneOffset);
		String tzString = timezoneOffset.isEmpty() ? tzInfo : timezoneOffset;

		bool syncOK = false;

		// Bevorzuge zuletzt erfolgreichen Server
		if (lastSuccessfulNtpServer.length() > 0)
		{
			syncOK = trySyncWithNtpServer(lastSuccessfulNtpServer.c_str(), tzString.c_str(), timeinfo, "Bevorzugt");
		}

		if (!syncOK)
		{
			static uint8_t dailyNtpRotation = 0;
			uint8_t serverIdx = dailyNtpRotation % 3;
			dailyNtpRotation++;

			switch (serverIdx)
			{
			case 0: // Router
			{
				IPAddress gateway = WiFi.gatewayIP();
				if (gateway != IPAddress(0, 0, 0, 0))
				{
					char routerIp[16];
					sprintf(routerIp, "%d.%d.%d.%d", gateway[0], gateway[1], gateway[2], gateway[3]);
					syncOK = trySyncWithNtpServer(routerIp, tzString.c_str(), timeinfo, "Router");
				}
				break;
			}
			case 1: // Benutzerdefinierte Server
				syncOK = syncWithCustomNtpServers(tzString, timeinfo);
				break;
			case 2: // Fallback
				syncOK = trySyncWithNtpServer("pool.ntp.org", tzString.c_str(), timeinfo, "Fallback");
				break;
			}

			if (syncOK)
			{
				dailyNtpRotation = 0;
			}
		}

		if (syncOK)
		{
			lastNtpDay = timeinfo.tm_mday;
			timeSet = true;
			LOG_PRINTLN("[NTP] Tägliche Synchronisierung erfolgreich abgeschlossen.");
		}
		else
		{
			LOG_PRINTLN("[NTP] Tages-Sync fehlgeschlagen – nächster Server wird beim nächsten Durchlauf versucht.");
		}
	}
}

// Hilfsfunktion für NTP-Synchronisierungsversuche
bool trySyncWithNtpServer(const char *serverName, const char *tzString, struct tm &timeinfo_ref, const char *sourceDescription)
{
	LOG_PRINTLN("🕒 Versuche NTP-Server: " + String(serverName) + " (" + String(sourceDescription) + ")");
	// addLogMessage("🕒 Versuche NTP-Server: " + String(serverName) + " (" + String(sourceDescription) + ")");
	sntp_stop(); // verhindert "COMPLETED" vom vorherigen Sync (Schein-Erfolg)
	configTzTime(tzString, serverName);
	configureSntp(); // Callback/Intervall nach stop() sicherstellen

	// Warte auf die Synchronisierung, maximal 2.5 Sekunden (schneller bei Offline/Bad Server)
	unsigned long startTime = millis();
	while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
	{
		if (millis() - startTime > 2500)
		{
			LOG_PRINTLN("   -> Timeout beim Warten auf die Synchronisierung von " + String(serverName));
			return false;
		}
		delay(100);
	}

	// Jetzt, da die Synchronisierung abgeschlossen ist, hole die Zeit
	if (!getLocalTime(&timeinfo_ref, 1000))
	{
		LOG_PRINTLN("   -> Fehler beim Abrufen der Zeit von " + String(serverName) + " nach der Synchronisierung.");
		return false;
	}

	// Erfolgsfall: Alles hat geklappt
	usedNtpSource = String(serverName) + " (" + String(sourceDescription) + ")"; // Speichere den erfolgreichen Server und seine Quelle
	lastSuccessfulNtpServer = String(serverName); // Merke erfolgreichen Server für Bevorzugung
	LOG_PRINTLN("✅ Zeit erfolgreich von " + String(serverName) + " synchronisiert und abgerufen.");
	addLogMessage("🕒 ✅ NTP-Zeit erfolgreich synchronisiert: " + String(serverName) + " (" + String(sourceDescription) + ")");
	return true;
}

// NEU: Hilfsfunktion zum Synchronisieren mit der benutzerdefinierten NTP-Server-Liste
bool syncWithCustomNtpServers(const String &tzString, struct tm &timeinfo_ref)
{
	String customNtpList = settingsManager.getAppSettings().ntpServer;
	customNtpList.replace(" ", "");

	if (customNtpList.isEmpty())
	{
		return false;
	}

	LOG_PRINTLN("🕒 Versuche benutzerdefinierte NTP-Server-Liste: " + customNtpList);
	int start = 0;
	int serversTried = 0;
	while (start >= 0 && serversTried < 1) // Maximal 1 Custom-Server pro Durchlauf
	{
		int comma = customNtpList.indexOf(',', start);
		String server = (comma >= 0) ? customNtpList.substring(start, comma) : customNtpList.substring(start);
		start = (comma >= 0) ? comma + 1 : -1;

		server.trim();
		if (server.length() > 0)
		{
			serversTried++;
			if (trySyncWithNtpServer(server.c_str(), tzString.c_str(), timeinfo_ref, "Settings"))
			{
				return true; // Erfolg
			}
		}
	}
	return false; // Kein Server aus der Liste war erfolgreich
}

// Replaces placeholder in HTML pages.
// Optimiert mit String-Reserve für bessere Performance
String processor(const String &var)
{
	// P2-03: Einmaliger Cache statt 30+ Einzelaufrufe von getAppSettings()
	const AppSettings &app = settingsManager.getAppSettings();
	String result;
	result.reserve(128); // Vorab Speicher reservieren reduziert Heap-Fragmentierung

	if (var == "LOGMESSAGES")
	{
		return getLogMessagesAsHtml();
	}
	else if (var == "UHRZEIT_DATUM")
	{
		// Hängt die verwendete NTP-Quelle an den Zeitstempel an
		result.reserve(64); // Typische Länge: "HH:MM - DD.MM.YYYY (Quelle)"
		result = getTimestampString();
		result += " (";
		result += usedNtpSource;
		result += ")";
		return result;
	}
	else if (var == "WIFIQUALITY")
	{
		return String(rssiToQuality(WiFi.RSSI()));
	}
	else if (var == "HOSTNAME")
	{
		return settingsManager.getWifiSettings().hostname.c_str();
	}
	else if (var == "VERSIONINFO")
	{
		return VersionInfo;
	}
	else if (var == "WIFI_SSID")
	{
		return settingsManager.getWifiSettings().ssid;
	}
	else if (var == "WIFI_PASSWORD")
	{
		if (settingsManager.getWifiSettings().password.isEmpty())
			return "";
		else
			return "********"; // for security reasons the wifi password will not left the device once configured
	}
	else if (var == "PASSWORD_SETUP_WiFi_CONFIG")
	{
		return settingsManager.getWifiSettings().passwordAdmin;
	}
	else if (var == "MQTT_SERVER")
	{
		return app.mqttServer;
	}
	else if (var == "MQTT_USERNAME")
	{
		return app.mqttUsername;
	}
	else if (var == "MQTT_PASSWORD")
	{
		return app.mqttPassword;
	}
	else if (var == "MQTT_ROOTTOPIC")
	{
		return app.mqttRootTopic;
	}
	else if (var == "NTP_SERVER")
	{
		return app.ntpServer;
	}
	else if (var == "NTP_OFFSET")
	{
		return app.ntpOffset;
		// return timezoneOffset;
	}
	else if (var == "NTP_OFFSET_NAMEN")
	{
		return timezoneName;
	}
	else if (var == "NTP_OFFSET_TIME")
	{
		// return settingsManager.getAppSettings().ntpOffset;
		return timezoneOffset;
	}
	else if (var == "PASSWORD_SETUP")
	{
		if (app.passwordSetup == "admin")
			return "Das aktuelle Standardpasswort ist (admin).";
		else
			return "********"; // for security reasons the wifi password will not left the device once configured
	}
	else if (var == "password_enabled_checked")
	{
		// Annahme: ENABLE_PASSWORD ist ein String in deinen AppSettings
		String enablePasswordTemp = app.enablePassword;
		// Rückgabe von "checked", wenn ENABLE_PASSWORD "on" ist, ansonsten eine leere Zeichenkette
		return (enablePasswordTemp == "on") ? "checked" : "";
	}
	else if (var == "MQTT_PORT")
	{
		// Hier wird der Port als String zurückgegeben
		return String(app.mqtt_port);
	}
	else if (var == "MQTT_KEEPALIVE")
	{
		return String(app.mqtt_keepAlive);
	}
	else if (var == "isbroker_checked")
	{
		// Wenn mqtt_isBroker aktiv ist, wird "checked" zurückgegeben, ansonsten ein leerer String
		return app.mqtt_isBroker ? "checked" : "";
	}
	else if (var == "isclient_checked")
	{
		return app.mqtt_isClient ? "checked" : "";
	}
	else if (var == "MQTT_MODE")
	{
		if (app.mqtt_isBroker)
		{
			return "MQTT Broker";
		}
		else if (app.mqtt_isClient)
		{
			return "MQTT Client";
		}
		else
		{
			return "MQTT Aus";
		}
	}
	else if (var == "WIFI_RSSI_D")
	{
		return String(app.wifiRssiDisconnectThreshold);
	}
	else if (var == "WIFI_ROAM_IMPROVE_DB")
	{
		return String(normalizeWifiRoamImproveDb(app.wifiRoamImproveDb));
	}
	else if (var == "WIFI_ROAM_MIN_RSSI")
	{
		return String(normalizeWifiRoamMinRssi(app.wifiRoamMinRssi));
	}
#if USE_TELEGRAM
	else if (var == "telegram_enabled_checked")
	{
		return app.telegram_enabled ? "checked" : "";
	}
	else if (var == "TELEGRAM_BOT_TOKEN")
	{
		return app.telegram_botToken;
	}
	else if (var == "TELEGRAM_CHAT_ID")
	{
		return app.telegram_chatId;
	}
#endif
#if USE_DHT
	else if (var == "TEM")
	{
		return dhtManager.getTemperature();
	}
	else if (var == "HUM")
	{
		return dhtManager.getHumidity();
	}
	else if (var == "DEW")
	{
		return dhtManager.getDewPoint();
	}
	else if (var == "HEA")
	{
		return dhtManager.getHeatIndex();
	}
#endif
#if USE_DOWNLOAD_UPLOAD
	else if (var == "DOWNLOAD_BUTTON")
	{
		return String(DOWNLOAD_BUTTON_HTML);
	}
	else if (var == "UPLOAD_SECTION")
	{
		return String(UPLOAD_SECTION_HTML);
	}
	else if (var == "UPLOAD_HINT")
	{
		return String(UPLOAD_HINT_HTML);
	}
#endif
	/****************************************************
	****************************************************/
	return String();
}


// send LastMessage to websocket clients
// Funktion für einheitliche MQTT-Veröffentlichung - Vorwärtsdeklaration
// void publishMqttMessage(const String &topic, const String &message, bool retain, int qos);
// HINWEIS: Telegram-Logik ist in Telegram.cpp/h modularisiert - nicht hier!

// UI flood control
void notifyClients(String message, const char *sourceTag)
{
	// ══════════════════════════════════════════════════════════════
	// IMMER: SourceTag anhängen, Timestamp erstellen, Log + Ringpuffer
	// ══════════════════════════════════════════════════════════════
	if (sourceTag && *sourceTag)
	{
		message += " ";
		message += sourceTag;
	}

	// ✅ IMMER: Console-Log und Ringpuffer-Eintrag
	String messageWithTimestamp;
	messageWithTimestamp.reserve(message.length() + 32); // weniger Heap-Reallocs/Fragmentierung
	messageWithTimestamp = "[" + getTimestampString() + "]: " + message;
	LOG_PRINTLN(messageWithTimestamp);
	addLogMessage(messageWithTimestamp); // ✅ Immer in den Ringpuffer schreiben!

#if USE_TELEGRAM
	// ✅ GANZ AM ANFANG prüfen - wenn deaktiviert, nichts tun!
	if (settingsManager.getAppSettings().telegram_enabled)
	{
		sendTelegramMessage(message);
	}
#endif

	// ══════════════════════════════════════════════════════════════
	// NUR WENN UI AKTIV: SSE-Events mit Deduplication senden
	// ══════════════════════════════════════════════════════════════
	if (!isUiActive())
	{
		return; // Kein Client aktiv -> keine SSE-Events senden
	}

	// ══════════════════════════════════════════════════════════════
	// Ab hier: UI ist aktiv → SSE-Deduplication
	// ══════════════════════════════════════════════════════════════

	static String lastSentMsg = "";
	static unsigned long lastSentTs = 0;
	static int suppressedCount = 0;
	const unsigned long DEDUP_WINDOW_MS = 2000; // 2 Sekunden

	unsigned long now = millis();
	// Prüfe ob Nachricht identisch ist UND innerhalb des Deduplication-Fensters

	if (logMutex)
		xSemaphoreTake(logMutex, portMAX_DELAY);
	bool isDuplicate = (message == lastSentMsg && (now - lastSentTs) < DEDUP_WINDOW_MS);
	if (isDuplicate)
	{
		suppressedCount++;
		if (logMutex)
			xSemaphoreGive(logMutex);
		LOG_PRINTF("[SSE-DEDUP] Duplikat unterdrückt\n");
		return;
	}

	int currentSuppressed = suppressedCount;
	suppressedCount = 0;
	lastSentMsg = message;
	lastSentTs = now;
	if (logMutex)
		xSemaphoreGive(logMutex);

	if (currentSuppressed > 0)
	{
		String summary = "[" + getTimestampString() + "]: (↻ " + String(currentSuppressed) + " doppelte Meldungen unterdrückt)";
		sendSSEEvent(summary.c_str(), "new_log_message", millis(), 1000);
		addLogMessage(summary);
	}
	sendSSEEvent(messageWithTimestamp.c_str(), "new_log_message", millis(), 1000);
}

#if USE_MQTT_ANY
struct RecentMqttPublish
{
	String topic;
	String payload;
	unsigned long timestamp = 0;
};

static constexpr size_t RECENT_MQTT_PUBLISH_COUNT = 8;
static RecentMqttPublish recentMqttPublishes[RECENT_MQTT_PUBLISH_COUNT];
static size_t recentMqttPublishWriteIndex = 0;
static const unsigned long MQTT_SELF_ECHO_WINDOW_MS = 500;

static void rememberLocalMqttPublish(const String &topic, const String &payload)
{
	RecentMqttPublish &entry = recentMqttPublishes[recentMqttPublishWriteIndex];
	entry.topic = topic;
	entry.payload = payload;
	entry.timestamp = millis();

	recentMqttPublishWriteIndex = (recentMqttPublishWriteIndex + 1) % RECENT_MQTT_PUBLISH_COUNT;
}

static bool isRecentLocalMqttPublish(const String &topic, const String &payload)
{
	unsigned long now = millis();

	for (size_t i = 0; i < RECENT_MQTT_PUBLISH_COUNT; ++i)
	{
		const RecentMqttPublish &entry = recentMqttPublishes[i];
		if (entry.timestamp == 0)
		{
			continue;
		}

		if ((long)(now - entry.timestamp) > (long)MQTT_SELF_ECHO_WINDOW_MS)
		{
			continue;
		}

		if (entry.topic == topic && (entry.payload == payload || entry.payload.endsWith(";" + payload)))
		{
			return true;
		}
	}

	return false;
}
#endif // USE_MQTT_ANY

// Performance: Funktion für einheitliche MQTT-Veröffentlichung optimiert
void publishMqttMessage(const String &topic, const String &message, bool retain, int qos)
{
#if !USE_MQTT_BROKER && !USE_MQTT_CLIENT
	(void)topic;
	(void)message;
	(void)retain;
	(void)qos;
	return;
#else
	// Guard gegen Race-Conditions: Async-Web-Callbacks vs Loop (MQTT/WiFi nahe Calls)
	bool netLocked = false;

	bool brokerActive = false;
#if USE_MQTT_BROKER
	brokerActive = (mqttBroker != nullptr);
#endif

	// Broker ist lokal: keinen Mutex dafür
	if (!brokerActive && networkMutex)
	{
		for (int attempt = 0; attempt < 3; attempt++)
		{
			netLocked = (xSemaphoreTake(networkMutex, pdMS_TO_TICKS(80)) == pdTRUE);
			if (netLocked)
				break;
			vTaskDelay(pdMS_TO_TICKS(15)); // kurzer Retry, kein Aufblähen
		}
		if (!netLocked)
		{
			LOG_PRINTLN("[NET-MUTEX] publishMqttMessage übersprungen (Mutex busy)");
			return;
		}
	}

	// Performance: notifyTopic einmal bauen (vermeidet temporäre String-Allokation)
	String notifyTopic;
	notifyTopic.reserve(mqttRootTopic.length() + 7);
	notifyTopic = mqttRootTopic;
	notifyTopic += "/notify";

#if USE_MQTT_BROKER
	if (brokerActive)
	{
		// Für /notify: direkt lokal anzeigen, nicht publishen
		if (topic == notifyTopic)
		{
			handleMqttMessage(topic, message, "BROKER");
			rememberLocalMqttPublish(topic, message);
		}
		else
		{
			// Broker publish mit excludeClientId
			String excludeId = String(mqttClientId);

			bool ok = mqttBroker->publish(topic.c_str(), message.c_str(), retain, (uint8_t)qos, excludeId);
			if (!ok)
			{
				LOG_PRINTLN("Broker-Publish fehlgeschlagen.");
			}
			else
			{
				rememberLocalMqttPublish(topic, message);
			}
		}
	}
	else
#endif
#if USE_MQTT_CLIENT
		if (mqttClient.connected())
	{
		uint16_t packetId = mqttClient.publish(topic.c_str(), qos, retain, message.c_str());
		if (packetId > 0)
		{
			rememberLocalMqttPublish(topic, message);
		}
		else
		{
			LOG_PRINTLN("Client-Publish fehlgeschlagen.");
		}
	}
	else
#endif
	{
		LOG_PRINTLN("MQTT Publish übersprungen – nicht verbunden oder nicht mitkompiliert.");
	}

	if (netLocked && networkMutex)
	{
		xSemaphoreGive(networkMutex);
	}
#endif
}





// Stub-Funktionen (Legacy entfernt)
static bool doPairing() { return true; }
static bool checkPairingValid() { return true; }

// ============================================================================
// WiFi Event Handler - Zentrale Behandlung aller WiFi-Events
// ============================================================================
static void handleWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
	WifiSettings wifiSettings = settingsManager.getWifiSettings();
	switch (event)
	{
	case ARDUINO_EVENT_WIFI_STA_GOT_IP:
		// SSID-Validierung
		{
			const String cfg = settingsManager.getWifiSettings().ssid;
			const String got = WiFi.SSID();
			if (got != cfg)
			{
				LOG_PRINTLN("[WiFi] Warnung: verbundene SSID != Settings: " + got + " vs " + cfg);
			}
		}
		if (WiFi.localIP() != IPAddress(0, 0, 0, 0))
		{
			LOG_PRINT("[WiFi] Verbunden mit IP: ");
			LOG_PRINTLN(WiFi.localIP());
			LOG_PRINTLN("[FLASH_TEST] IP=" + WiFi.localIP().toString());
			// BSSID/Channel locken
			uint8_t *b = WiFi.BSSID();
			if (b)
			{
				memcpy(lockedBssid, b, 6);
				hasLockedBssid = true;
			}
			lockedChannel = WiFi.channel();
			// Backoff zurücksetzen
			wifiBackoffIndex = 0;
			wifiNextAttemptAt = 0;
			wifiStableSince = millis();
			wifiConnectedSince = millis();
			wifiOfflineSince = 0; // Online -> Offline-Tracking zurücksetzen
			wifiLastMeasuredRssi = WiFi.RSSI();
			applyWifiPerformanceProfile(WifiPerformanceProfile::Balanced);
			// SNTP konfigurieren (einmalig)
			configureSntp();
			if (!timeSet)
			{
				resetNtpRetryState(); // Nach frischem GOT_IP: Retry-Kaskade von vorne
				ntpQuickSyncPending = true;
				ntpQuickSyncEarliestAt = millis() + 3000UL;
			}
			// LED-Ring wird in loop() auf "bereit" gesetzt (Serial2 nicht im Event-Callback)
			wifiJustReconnected = true;

			// --- mDNS Zentralisierung ---
			// Bei JEDEM Reconnect mDNS sauber neu starten und Dienste registrieren.
			MDNS.end();
			if (MDNS.begin(wifiSettings.hostname.c_str()))
			{
				LOG_PRINTLN("[mDNS] Service gestartet: http://" + wifiSettings.hostname + ".local");
				MDNS.addService("http", "tcp", 80);
				MDNS.addServiceTxt("http", "tcp", "path", "/");
				mdnsStarted = true;

				// MQTT-Dienst registrieren, wenn Broker-Modus in den Settings AKTIV ist.
				// Dies ist robust gegen Race-Conditions, da es nicht vom Timing der Broker-Initialisierung abhängt.
#if USE_MQTT_BROKER
				const AppSettings &appMqtt = settingsManager.getAppSettings();
				if (appMqtt.mqtt_isBroker)
				{
					if (MDNS.addService("mqtt", "tcp", mqttPort))
					{
						LOG_PRINTF("[mDNS] ✅ MQTT Broker-Dienst auf Port %d angekündigt.\n", mqttPort);
					}
					else
					{
						LOG_PRINTLN("[mDNS] ⚠️ MQTT Broker-Dienst konnte nicht angekündigt werden.");
					}
				}
#endif // USE_MQTT_BROKER
			}
			else
			{
				LOG_PRINTLN("[mDNS] Fehler beim Starten des Dienstes");
			}
			// NEU: MQTT-Manager über WLAN-Verbindung informieren
#if USE_MQTT_CLIENT
			const AppSettings &app = settingsManager.getAppSettings();
			if (app.mqtt_isClient)
			{
				mqttManager.onWifiConnect();
			}
#endif
		}
		else
		{
			LOG_PRINTLN("[WiFi] Ungültige IP-Adresse (0.0.0.0) erhalten");
		}
		break;
	case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
		LOG_PRINTLN("[WiFi] Verbindung verloren");
		if (settingsManager.isWifiConfigured())
		{
			// /* fingerManager Legacy */;
		}
		else
		{
			// /* fingerManager Legacy */;
		}
		if (mdnsStarted)
		{
			MDNS.end();
			mdnsStarted = false;
		}
		// Reconnect anfordern – aber nicht während initWifi()-Phase (Boot)
		// Backoff-Eskalation passiert NUR in checkNetworkAndReconnectIfNeeded()
		if (!wifiInitPhase)
		{
			requestWifiReconnect(WIFI_BACKOFF_STEPS[wifiBackoffIndex], true);
		}
		wifiStableSince = 0;
		wifiConnectedSince = 0;
		if (wifiOfflineSince == 0) wifiOfflineSince = millis(); // Offline-Zeitpunkt merken
		wifiLastMeasuredRssi = -127;
		applyWifiPerformanceProfile(WifiPerformanceProfile::Stability);
		break;
	case ARDUINO_EVENT_WIFI_STA_START:
		LOG_PRINTLN("[WiFi] Station-Modus gestartet");
		break;
	case ARDUINO_EVENT_WIFI_STA_CONNECTED:
		LOG_PRINTLN("[WiFi] Mit Access Point verbunden, warte auf IP...");
		applyWifiPerformanceProfile(WifiPerformanceProfile::Stability);
		break;
	default:
		// Nicht explizit behandelte Events ignorieren (unterdrückt -Wswitch)
		break;
	}
}

// NEU: Verbesserte Logik zur Auswahl des besten Access Points, wiederverwendbar
bool connectToBestAP(bool bootScan, bool forceBestAp)
{
	WifiSettings wifiSettings = settingsManager.getWifiSettings();
	if (!bootScan && !forceBestAp)
	{
		if (isBootNetworkGraceActive())
		{
			DIAG_LOG("WIFI", "skip scan: boot grace");
			return false;
		}
		if (hasActiveWebClients())
		{
			DIAG_LOG("WIFI", "skip scan: active HTTP/SSE");
			return false;
		}
	}

	if (bootScan)
	{
		LOG_PRINTLN("[WiFi] Boot-Scan gestartet");
	}
	else
	{
		LOG_PRINTLN("[WiFi] Suche nach bester Verbindung für SSID: " + wifiSettings.ssid);
	}

	// Scan-Dauer auf 300ms begrenzen, um stabile AP-Erkennung beim Boot zu behalten.
	bool netLocked = false;
	if (!netTryLock(600, netLocked))
	{
		if (bootScan)
		{
			LOG_PRINTLN("[NET-MUTEX] Boot-Scan übersprungen (Mutex busy)");
		}
		else
		{
			LOG_PRINTLN("[NET-MUTEX] connectToBestAP: Mutex busy -> Scan übersprungen (Reconnect pending)");
			requestWifiReconnect(250, true);
		}
		return false;
	}

	DIAG_LOG("WIFI", forceBestAp ? "emergency scan start" : "scan start");
#if USE_TELEGRAM
	setWifiBusyForTelegram(true);
#endif
	int networksFound = WiFi.scanNetworks(false, false, false, 300, 0, wifiSettings.ssid.c_str());
	DIAG_PRINTF("WIFI", "scan done networks=%d", networksFound);
	bool connected = false;

	if (networksFound > 0)
	{
		struct APCandidate
		{
			int rssi;
			int channel;
			uint8_t bssid[6];
		};

		APCandidate bestCandidate = {-127, 0, {0}};
		bool hasBestCandidate = false;

		for (int i = 0; i < networksFound; i++)
		{
			if (WiFi.SSID(i) == wifiSettings.ssid)
			{
				const uint8_t *bssid = WiFi.BSSID(i);
				if (!bssid) continue; // NULL-Pointer Schutz (P1-09)

				const int rssi = WiFi.RSSI(i);
				if (!hasBestCandidate || rssi > bestCandidate.rssi)
				{
					bestCandidate.rssi = rssi;
					bestCandidate.channel = WiFi.channel(i);
					memcpy(bestCandidate.bssid, bssid, 6);
					hasBestCandidate = true;
				}
			}
		}

		WiFi.scanDelete(); // Scan-Ergebnisse freigeben

		if (hasBestCandidate)
		{
			rememberWifiBestAp(bestCandidate.rssi, bestCandidate.channel, bestCandidate.bssid);
			DIAG_PRINTF("WIFI", "best AP found RSSI=%d channel=%d", bestCandidate.rssi, bestCandidate.channel);

			// Nur roamen, wenn es wirklich besser ist (Hysterese), sonst Verbindung behalten.
			const AppSettings wifiRoamSettings = settingsManager.getAppSettings();
			const int roamRssiImprovementDb = normalizeWifiRoamImproveDb(wifiRoamSettings.wifiRoamImproveDb);

			bool alreadyConnected = (WiFi.status() == WL_CONNECTED);
			int currentRssi = alreadyConnected ? WiFi.RSSI() : -127;
			const uint8_t *curBssid = alreadyConnected ? WiFi.BSSID() : nullptr;

			// Wenn bereits am gleichen AP: nichts tun
			if (!forceBestAp && alreadyConnected && curBssid && memcmp(curBssid, bestCandidate.bssid, 6) == 0)
			{
				char bestMac[18] = {0};
				snprintf(bestMac, sizeof(bestMac), "%02X:%02X:%02X:%02X:%02X:%02X",
						 bestCandidate.bssid[0], bestCandidate.bssid[1], bestCandidate.bssid[2],
						 bestCandidate.bssid[3], bestCandidate.bssid[4], bestCandidate.bssid[5]);
				LOG_PRINTF("[WiFi] Bereits am besten AP (RSSI: %d dBm, MAC: %s) -> kein Wechsel\n", currentRssi, bestMac);
				LOG_PRINTLN("[WiFi] Kein AP-Wechsel: Signal gut genug oder Verbesserung zu klein");
				connected = true;
			}
			// Wenn Verbesserung zu klein: nichts tun
			else if (!forceBestAp && alreadyConnected && bestCandidate.rssi < (currentRssi + roamRssiImprovementDb))
			{
				LOG_PRINTF("[WiFi] Kein Wechsel: bester AP nur %d dB besser (aktuell %d dBm, best %d dBm)\n",
						   bestCandidate.rssi - currentRssi, currentRssi, bestCandidate.rssi);
				LOG_PRINTLN("[WiFi] Kein AP-Wechsel: Signal gut genug oder Verbesserung zu klein");
				connected = true;
			}
			else
			{
				if (forceBestAp)
				{
					LOG_PRINTLN("[WiFi] Feld-1-Rettung: verbinde mit bestem AP ohne Feld-3-Prüfung");
				}
				else if (alreadyConnected)
				{
					LOG_PRINTF("[WiFi] Besserer AP gefunden: aktuell=%d dBm, neu=%d dBm, Verbesserung=%d dB\n",
							   currentRssi, bestCandidate.rssi, bestCandidate.rssi - currentRssi);
				}
				if (bootScan)
				{
					LOG_PRINTF("[WiFi] Bester AP gefunden: RSSI %d dBm, Kanal %d\n", bestCandidate.rssi, bestCandidate.channel);
					LOG_PRINTLN("[WiFi] Verbinde direkt mit bestem AP");
				}
				else
				{
					char bestMac[18] = {0};
					snprintf(bestMac, sizeof(bestMac), "%02X:%02X:%02X:%02X:%02X:%02X",
							 bestCandidate.bssid[0], bestCandidate.bssid[1], bestCandidate.bssid[2],
							 bestCandidate.bssid[3], bestCandidate.bssid[4], bestCandidate.bssid[5]);
					LOG_PRINTF("[WiFi] Verbinde mit bestem AP (RSSI: %d dBm, Kanal: %d, MAC: %s)\n", bestCandidate.rssi, bestCandidate.channel, bestMac);
				}
				WiFi.disconnect(false, false); // sanft trennen: keine Credentials/Config löschen
				delay(50);
				WiFi.begin(wifiSettings.ssid.c_str(), wifiSettings.password.c_str(), bestCandidate.channel, bestCandidate.bssid);
				connected = true; // Verbindungsversuch wurde erfolgreich asynchron gestartet
				netUnlock(netLocked);
				netLocked = false;
				if (!bootScan)
				{
					LOG_PRINTLN("[WiFi] Bester AP wird jetzt asynchron verbunden.");
				}
			}
		}
	}

	netUnlock(netLocked);
#if USE_TELEGRAM
	setWifiBusyForTelegram(false);
#endif
	return connected;
}

bool initWifi()
{
	DIAG_LOG("WIFI", "init begin");
	// Backoff zurücksetzen – frischer Boot, keine Altlasten
	wifiBackoffIndex = 0;
	wifiNextAttemptAt = 0;
	wifiInitPhase = true; // DISCONNECT-Events sollen Backoff nicht eskalieren

	// WiFi-Einstellungen laden
	WifiSettings wifiSettings = settingsManager.getWifiSettings();
	WiFi.setHostname(wifiSettings.hostname.c_str());
	LOG_PRINTLN("[WiFi] Hostname gesetzt: " + wifiSettings.hostname);

	// Gründlichere Zurücksetzung des WiFi-Moduls
	bool netLocked = false;
	if (!netTryLock(2000, netLocked))
	{
		LOG_PRINTLN("[NET-MUTEX] initWifi: Mutex busy -> Abbruch, später erneut");
		wifiInitPhase = false;
		return false;
	}
	WiFi.disconnect(false, false); // Disconnect ohne Löschung der gespeicherten Credentials
	WiFi.mode(WIFI_OFF);
	netUnlock(netLocked);
	netLocked = false;
	delay(100); // Minimale Wartezeit für WiFi-Modul-Reset (100ms reicht laut ESP-IDF)

	// Erneute Initialisierung
	if (!netTryLock(2000, netLocked))
	{
		LOG_PRINTLN("[NET-MUTEX] initWifi: Mutex busy -> Abbruch, später erneut");
		wifiInitPhase = false;
		return false;
	}
	WiFi.mode(WIFI_STA);
	WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
	WiFi.setHostname(wifiSettings.hostname.c_str());

	// WiFi-Stabilitäts-Einstellungen für bessere Verbindung
	WiFi.setAutoReconnect(true);
	WiFi.persistent(false);

	// Power-Management: dynamisch, um Stabilität und Temperatur auszubalancieren
	applyWifiPerformanceProfile(WifiPerformanceProfile::Balanced);

	// Event-Handler für WiFi-Events – nur einmal registrieren
	static bool wifiEventRegistered = false;
	if (!wifiEventRegistered)
	{
		WiFi.onEvent(handleWifiEvent);
		wifiEventRegistered = true;
	}

	netUnlock(netLocked);
	netLocked = false;

	// Boot-Schnellstart ohne Flash-Cache: zuerst direkt verbinden, Scan nur als Fallback.
	bool bootDirectBeginStarted = false;
	if (netTryLock(500, netLocked))
	{
		LOG_PRINTLN("[WiFi] Boot-Schnellstart: direkter WiFi.begin ohne Vorab-Scan");
		DIAG_LOG("WIFI", "WiFi begin direct");
		WiFi.begin(wifiSettings.ssid.c_str(), wifiSettings.password.c_str());
		bootDirectBeginStarted = true;
		netUnlock(netLocked);
		netLocked = false;
	}
	else
	{
		LOG_PRINTLN("[NET-MUTEX] Boot-Schnellstart übersprungen (Mutex busy)");
	}
	{
		int attempts = 0;
		while (WiFi.status() != WL_CONNECTED && attempts < 6)
		{
			delay(500);
			LOG_PRINT(".");
			attempts++;
		}
	}

	if (WiFi.status() != WL_CONNECTED)
	{
		if (bootDirectBeginStarted)
		{
			LOG_PRINTLN("\n[WiFi] Direkter Boot-Schnellstart noch ohne Verbindung -> Best-AP-Scan");
			if (netTryLock(500, netLocked))
			{
				WiFi.disconnect(false, false);
				netUnlock(netLocked);
				netLocked = false;
				delay(50);
			}
		}

		// Beim Boot nur bei Bedarf den besten AP suchen, danach direkt auf WiFi.begin zurückfallen.
		bool bootBestApStarted = connectToBestAP(true, false);
		if (!bootBestApStarted)
		{
			LOG_PRINTLN("[WiFi] Kein brauchbarer AP aus Scan -> Fallback WiFi.begin");
			if (netTryLock(500, netLocked))
			{
				DIAG_LOG("WIFI", "WiFi begin fallback");
				WiFi.begin(wifiSettings.ssid.c_str(), wifiSettings.password.c_str());
				netUnlock(netLocked);
				netLocked = false;
			}
			else
			{
				LOG_PRINTLN("[NET-MUTEX] WiFi.begin übersprungen (Mutex busy)");
			}
		}
	}
	// Kurze Startchance: Best-AP-Scan bleibt erhalten, der Rest laeuft asynchron weiter.
	if (WiFi.status() != WL_CONNECTED)
	{
		int attempts = 0;
		while (WiFi.status() != WL_CONNECTED && attempts < 4)
		{
			delay(500);
			LOG_PRINT(".");
			attempts++;
		}
	}

	wifiInitPhase = false;

	if (WiFi.status() == WL_CONNECTED)
	{
		LOG_PRINTLN("\n[WiFi] Verbindung erfolgreich hergestellt.");
		DIAG_PRINTF("WIFI", "connected RSSI=%d IP=%s", WiFi.RSSI(), WiFi.localIP().toString().c_str());
		currentMode = Mode::scan;
		wifiConnectedSince = millis();
		if (false)
		{
			// /* fingerManager Legacy */;
		}
		return true;
	}
	else
	{
		LOG_PRINTLN("\n[WiFi] Verbindung konnte nicht hergestellt werden.");
		DIAG_LOG("WIFI", "connect deferred/no connection");
		wifiBackoffIndex = 0;
		wifiNextAttemptAt = 0;
		return false;
	}
}

void initWiFiAccessPointForConfiguration()
{
	WiFi.softAPConfig(WifiConfigIp, WifiConfigIp, IPAddress(255, 255, 255, 0));
	WiFi.softAP(WifiConfigSsid, WifiConfigPassword);

	// Hostname für mDNS setzen (.local Zugriff)
	WiFi.setHostname("MatterMQTTBridge");
	if (MDNS.begin("MatterMQTTBridge"))
	{
		LOG_PRINTLN("[mDNS] Hostname 'fingerscanner.local' registriert");
		MDNS.addService("http", "tcp", 80);
		MDNS.addServiceTxt("http", "tcp", "path", "/");
		mdnsStarted = true;
	}

	// if DNSServer is started with "*" for domain name, it will reply with
	// provided IP to all DNS request
	dnsServer.start(DNS_PORT, "*", WifiConfigIp);

	LOG_PRINTLN(" ");
	LOG_PRINT("AP IP address: ");
	LOG_PRINTLN(WifiConfigIp);
	LOG_PRINTLN("[Captive Portal] Verbindung zu 'fingerscanner.local' oder 192.168.4.1");
	LOG_PRINTLN(" ");
}

static String resolveSetupPassword()
{
	String password = settingsManager.getAppSettings().passwordSetup;
	if (password.isEmpty())
	{
		password = settingsManager.getWifiSettings().passwordAdmin;
		if (password.isEmpty())
		{
			password = "admin";
		}
	}
	return password;
}

static bool sendMissingFsFile(AsyncWebServerRequest *request, const char *path)
{
	DIAG_PRINTF("HTTP", "missing %s -> 503", path);
	String html;
	html.reserve(260);
	html += path;
	html += " fehlt oder LittleFS ist nicht verfügbar.<br>";
	html += "Bitte Data/LittleFS neu hochladen: <a href='/update'>/update</a>";
	request->send(503, "text/html; charset=utf-8", html);
	return false;
}

static bool fsFileReady(AsyncWebServerRequest *request, const char *path)
{
	if (!LittleFS.exists(path))
		return sendMissingFsFile(request, path);

	return true;
}

void startWebserver()
{
	DIAG_LOG("WEB", "startWebserver begin");

	// webserver for normal operating or wifi config?
	if (currentMode == Mode::wificonfig)
	{
		// =================
		// WiFi config mode
		// =================
		// === OTA im Konfigurationsmodus: GLEICH AM ANFANG (immer Passwort) ===

		String otaPassword = resolveSetupPassword();
		Serial.println("[OTA-Konfig] Passwortschutz aktiviert!");
		Serial.print("[OTA-Konfig] Passwort: ");
		Serial.println(otaPassword);
		DIAG_LOG("OTA", "ElegantOTA begin config");
		ElegantOTA.begin(&webServer, "admin", otaPassword.c_str());
		DIAG_LOG("OTA", "ElegantOTA ready config");

		// P4-16a: Erster onNotFound-Handler entfernt (wurde sofort vom zweiten überschrieben)
		// FIX: Statische Dateien (CSS, JS, Bilder) müssen trotzdem ausgeliefert werden!
		webServer.onNotFound([](AsyncWebServerRequest *request)
							 {
			noteHttpActivity(request);
			String path = request->url();
			if (path.endsWith(".css") || path.endsWith(".js") || path.endsWith(".png") || path.endsWith(".jpg") || path.endsWith(".ico")) {
				if (LittleFS.exists(path)) {
					request->send(LittleFS, path);
					return;
				}
			}
			if (!fsFileReady(request, "/wificonfig.html"))
				return;
			request->send(LittleFS, "/wificonfig.html", String(), false, processor); });

		// Root-Route
		webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
						 noteHttpActivity(request);
						 if (!fsFileReady(request, "/wificonfig.html"))
							 return;
#if USE_TELEGRAM
						 noteHeavyHttpTransferForTelegram();
#endif
						 request->send(LittleFS, "/wificonfig.html", String(), false, processor); });

		webServer.on("/save", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
			if(request->hasArg("hostname"))
			{
				LOG_PRINTLN("Save wifi config");
				WifiSettings settings = settingsManager.getWifiSettings();
				AppSettings settings2 = settingsManager.getAppSettings();
				settings.hostname = request->arg("hostname");
				settings2.passwordSetup = request->arg("passwordSetup");

				settings.ssid = request->arg("ssid");
				LOG_PRINTLN("Save wifi config "+String(loggedIn) + ENABLE_PASSWORD + www_password);
				if (request->arg("password").equals("********")) // password is replaced by wildcards when given to the browser, so if the user didn't changed it, don't save it
					settings.password = settingsManager.getWifiSettings().password; // use the old, already saved, one
				else
					settings.password = request->arg("password");
				settingsManager.saveWifiSettings(settings);
				settingsManager.saveAppSettings(settings2);
				shouldReboot = true;
			}
			request->redirect("/"); });
	}
	else
	{
		// =======================
		// normal operating mode
		// =======================
		events.onConnect([](AsyncEventSourceClient *client)
						 {
							 lastHttpActivityMs = millis();
							 lastUiAliveTime = millis(); // Setze den Heartbeat-Timer bei einer neuen Verbindung
							 sseClientConnected = true;	 // Und aktiviere den Stream sofort
							 DIAG_LOG("HTTP", "GET /events client connected");
							 LOG_PRINTF("[SSE] Client verbunden, Heartbeat-Timer gestartet.\n");
							 if (client->lastId())
							 {
								 LOG_PRINTF("Client reconnected! Last message ID it got was: %u\n", client->lastId());
							 }
							 // Start-SSE schlank halten: nur Basiswerte fuer die Index-Anzeige senden.
							 // Log-Historie und Settings-Module werden nicht mehr beim Connect gepusht.
#if USE_DHT
							 // Beim Verbinden aktuelle Sensorwerte pushen, falls vorhanden
							 dhtManager.retryIfDisabled(true);
							 client->send(dhtManager.getTemperature().c_str(), "temperature", millis(), 1000);
							 client->send(dhtManager.getHumidity().c_str(), "humidity", millis(), 1000);
							 client->send(dhtManager.getDewPoint().c_str(), "dewPoint", millis(), 1000);
							 client->send(dhtManager.getHeatIndex().c_str(), "heatIndex", millis(), 1000);
#endif
							 // Chip-Temperatur initial senden
							 float chipTemp = temperatureRead();
							 LOG_PRINTLN("[SSE] Sende ChipTemp: " + String(chipTemp, 1) + "°C");
							 sendSSEEvent(String(chipTemp, 1).c_str(), "chip_temperature", millis()); // WiFi-Qualität initial senden
							 int wifiRssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
							 String wifiPayload = String("{\"quality\":") + rssiToQuality(wifiRssi) + ",\"rssi\":" + wifiRssi + "}";
							 client->send(wifiPayload.c_str(), "wifi_quality", millis(), 1000);
							 // MQTT-Status initial senden
#if USE_MQTT_ANY
							 client->send(mqttStatusString.c_str(), "mqtt_status", millis());
#else
							 client->send("disabled", "mqtt_status", millis());
#endif
							 // MQTT-Broker Verfügbarkeit senden (compile-time Flag)
#if USE_MQTT_BROKER
							 client->send("1", "mqtt_broker_available", millis());
#else
							 client->send("0", "mqtt_broker_available", millis());
#endif
							 // MQTT-Client Verfügbarkeit senden (compile-time Flag)
#if USE_MQTT_CLIENT
							 client->send("1", "mqtt_client_available", millis());
#else
							 client->send("0", "mqtt_client_available", millis());
#endif
							 // MQTT-Broker Client-Anzahl initial senden
#if USE_MQTT_BROKER
							 if (settingsManager.getAppSettings().mqtt_isBroker && mqttBroker)
							 {
								 client->send(String(mqttBroker->getConnectedClientCount()).c_str(), "mqtt_broker_clients", millis());
							 }
#endif

							 // Settings-Modul-HTML wird erst ueber /ui-settings-modules geladen,
							 // wenn die Settings-Seite wirklich geoeffnet wurde.
						 });

		// ===== SSE Client Disconnect Handler =====
		events.onDisconnect([](AsyncEventSourceClient *client)
							{
								// Dieser Handler wird jetzt nur noch zum Logging verwendet.
								// Die Logik für sseClientConnected wird über den Heartbeat gesteuert.
								LOG_PRINTLN("[SSE] SSE-Stream-Verbindung von Client geschlossen."); });

		webServer.addHandler(&events);
		DIAG_LOG("SSE", "events ready");

		// NEU: Heartbeat-Endpunkt für die UI

		webServer.on("/ui-alive", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
						 noteHttpActivity(request);
						 static unsigned long lastHeartbeatLog = 0;
						 unsigned long now = millis();

						 lastUiAliveTime = millis();

						 // ✅ DEBUG: Heartbeat empfangen
						 if (now - lastHeartbeatLog > 5000)
						 { // Alle 5 Sekunden loggen (nicht bei JEDEM Request)
							 LOG_PRINTLN("[SSE-Heartbeat] ✅ Heartbeat empfangen! sseClientConnected=" + String(sseClientConnected) + ", Zeit=" + String(lastUiAliveTime));
							 lastHeartbeatLog = now;
						 }

						 // sseClientConnected wird hier auf true gesetzt, da ein Lebenszeichen empfangen wurde.
						 if (!sseClientConnected)
						 {
							 sseClientConnected = true;
							 LOG_PRINTLN("[SSE-Heartbeat] 🟢 AKTIVIERT: SSE-Stream ist jetzt aktiv!");
						 }
						 request->send(204); // "No Content" - eine leere, erfolgreiche Antwort
					 });

		// NEU: Settings-Modul-Endpunkt – liefert alle Settings-Module gesammelt als HTML
		webServer.on("/ui-settings-modules", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
						 noteHttpActivity(request);
						 if (!(loggedIn || ENABLE_PASSWORD != "on"))
						 {
							 request->send(401, "text/plain", "Nicht angemeldet");
							 return;
						 }

						 String modulesHtml;

#if USE_TEDEE
						 modulesHtml += buildTedeeModuleHtml();
#endif
#if USE_NUKI
						 modulesHtml += buildNukiModuleHtml();
#endif
#if USE_TELEGRAM
						 modulesHtml += buildTelegramModuleHtml();
#endif
						 // Weitere Module koennen hier ergaenzt werden

						 AsyncWebServerResponse *response = request->beginResponse(200, "text/html; charset=utf-8", modulesHtml);
						 response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
#if USE_TELEGRAM
						 noteHeavyHttpTransferForTelegram();
#endif
						 request->send(response); });

		// Route for root / web page
		auto sendIndexPage = [](AsyncWebServerRequest *request)
		{
			noteHttpActivity(request);
			if (loggedIn || ENABLE_PASSWORD != "on")
			{

				if (!fsFileReady(request, "/index.html"))
					return;
				AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html", String(), false, processor);
				response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
#if USE_TELEGRAM
				noteHeavyHttpTransferForTelegram();
#endif
				request->send(response);

			}
			else
			{
				if (!fsFileReady(request, "/login.html"))
					return;
				AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/login.html", String(), false);
				response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
#if USE_TELEGRAM
				noteHeavyHttpTransferForTelegram();
#endif
				request->send(response);
			}
		};

		webServer.on("/", HTTP_GET, sendIndexPage);
		webServer.on("/index.html", HTTP_GET, sendIndexPage);

		/////////////////////////////////////////////////////////////////////////////////////////////////////
		webServer.on("/save", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
					if(request->hasArg("hostname"))
					{
						LOG_PRINTLN("Save wifi config");
						WifiSettings settings = settingsManager.getWifiSettings();
						AppSettings settings2 = settingsManager.getAppSettings();
						settings.hostname = request->arg("hostname");
						settings2.passwordSetup = request->arg("passwordSetup");

						settings.ssid = request->arg("ssid");
						LOG_PRINTLN("Save wifi config "+String(loggedIn) + ENABLE_PASSWORD + www_password);
						if (request->arg("password").equals("********")) // password is replaced by wildcards when given to the browser, so if the user didn't changed it, don't save it
							settings.password = settingsManager.getWifiSettings().password; // use the old, already saved, one
						else
							settings.password = request->arg("password");
						settingsManager.saveWifiSettings(settings);
						settingsManager.saveAppSettings(settings2);
						shouldReboot = true;
					}
					request->redirect("/"); });
		/////////////////////////////////////////////////////////////////////////////////////////////////////

		webServer.on("/login", HTTP_POST, [](AsyncWebServerRequest *request)
					 {
						//String username = request->arg("username"); //username.equals(www_username)&& password.equals(www_password)
						String password = request->arg("password");
						if (password.equals(www_password) ) {
						loggedIn = true;
						lastActivityTime = millis();
						request->redirect("/");
						password = "";
						} else {
						if (!fsFileReady(request, "/login.html"))
							return;
						request->send(LittleFS, "/login.html", String(), false);
						} });

		// [Legacy] /enroll entfernt – FingerprintManager nicht mehr vorhanden
		webServer.on("/enroll", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
						 request->send(410, "text/plain", "Fingerprint-Enroll nicht verfuegbar (Legacy)");
					 });
		//////////////////////////////////////////////////////

		// [Legacy] /editFingerprints entfernt – FingerprintManager nicht mehr vorhanden
		webServer.on("/editFingerprints", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
						 request->send(410, "text/plain", "Fingerprint-Verwaltung nicht verfügbar (Legacy)");
					 });

		// Endpunkt zum Herunterladen eines einzelnen Fingerabdrucks (RAM-Puffer, FINGERPRINT_TEMPLATE_SIZE Bytes)
#if USE_DOWNLOAD_UPLOAD
		webServer.on("/downloadFinger", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
			if (!request->hasParam("id")) {
				request->send(400, "text/plain", "Fehler: Ungültige oder fehlende ID.");
				return;
			}

			int id = request->getParam("id")->value().toInt();
			if (id <= 0 || id > 200) {
				request->send(400, "text/plain", "Fehler: Ungültige ID (muss zwischen 1 und 200 liegen).");
				return;
			}

// [Legacy] 			if (!fingerManager.exportSingleFinger((uint8_t)id)) {
				request->send(500, "text/plain", "Fehler beim Export des Fingerabdrucks.");
				return;
			}

			const uint8_t *data = /* fingerManager Legacy */;
			size_t totalLen = /* fingerManager Legacy */;
			if (!data || totalLen == 0) {
				request->send(500, "text/plain", "Fehler: Keine exportierten Daten vorhanden.");
				return;
			}

			String filename = /* fingerManager Legacy */;
			if (filename.isEmpty()) {
				filename = String(id) + ".tmpl";
			}

			// Cache-Problem lösen: Timestamp zum Dateinamen hinzufügen für unique URL
			String timestamp = String(millis());
			String uniqueFilename = filename + "?t=" + timestamp;

				AsyncWebServerResponse *response = request->beginResponse("application/octet-stream", totalLen,
					[data, totalLen](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
						if (index >= totalLen) {
							// 0 Bytes => Stream ist zu Ende, AsyncWebServer kann die Antwort schließen
							return 0;
						}
						size_t remaining = totalLen - index;
						size_t bytesToWrite = (remaining < maxLen) ? remaining : maxLen;
						memcpy(buffer, data + index, bytesToWrite);
						return bytesToWrite;
					});

				response->addHeader("Content-Disposition", "attachment; filename=" + filename);
				response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
				response->addHeader("Pragma", "no-cache");
				response->addHeader("Expires", "0");
				// Verbindung explizit als "nach diesem Response schließen" markieren
				response->addHeader("Connection", "close");

				request->send(response); });

		// =================================================================================================
		// KORRIGIERTER UPLOAD-HANDLER: STRIKTE TRENNUNG VON DATENEMPFANG UND VERARBEITUNG
		// =================================================================================================
		webServer.on(
			"/uploadFinger", HTTP_POST,
			// Handler 1: Finale Antwort und Verarbeitung (wird NACH dem Upload ausgeführt)
			[](AsyncWebServerRequest *request)
			{
				UploadContext *context = (UploadContext *)request->_tempObject;
				if (!context || !context->buffer)
				{
					request->send(500, "text/plain", "Fehler: Upload-Kontext oder Puffer ist ungültig.");
					if (context)
						delete context; // Kontext aufräumen, falls nur der Puffer fehlt
					return;
				}

				// Lambda für sauberes Aufräumen
				auto cleanup = [&]()
				{
					delete[] context->buffer;
					delete context;
					request->_tempObject = nullptr;
				};

				// Logging: Datei, Länge, Warnung bei falscher Länge
				LOG_PRINTLN(String("[Upload] Datei empfangen: filename=") + context->filename + ", total_len=" + context->total_len);

				// Harte Ablehnung bei Overflow: Upload war größer als der reservierte Puffer
				if (context->overflowed)
				{
					String errorMsg = "Fehler: Template-Datei zu groß (" + String(context->total_len) + " Bytes). Maximum: " + String(FINGERPRINT_TEMPLATE_SIZE) + " Bytes.";
					LOG_PRINTLN("[Upload] " + errorMsg);
					notifyClients(errorMsg);
					request->send(400, "text/plain", errorMsg);
					cleanup();
					return;
				}

				// Validierung: Dateigröße muss im gültigen Bereich sein
				if (context->total_len == 0 || context->total_len > FINGERPRINT_TEMPLATE_SIZE)
				{
					String errorMsg = "Fehler: Ungültige Template-Größe. Größe: " + String(context->total_len) + " Bytes (zulässig: 1-" + FINGERPRINT_TEMPLATE_SIZE + ")";
					LOG_PRINTLN("[Upload] " + errorMsg);
					notifyClients(errorMsg);
					request->send(400, "text/plain", errorMsg);
					cleanup();
					return;
				}

				// Info-Log: Template-Größe (adaptive Templates haben unterschiedliche Größen)
				LOG_PRINTLN(String("[Upload] Template-Größe: ") + context->total_len + " Bytes (adaptives Template)");

				// === ID/NAME-LOGIK (PRIORITÄTEN-REGELN) ===
				String idField = request->hasParam("targetId", true) ? request->getParam("targetId", true)->value() : "";
				String nameField = request->hasParam("fingerName", true) ? request->getParam("fingerName", true)->value() : "";
				String originalFilename = context->filename;

				int idFromFilename = -1;
				String nameFromFilename;
				int sepIdx = originalFilename.indexOf("_-_");
				if (sepIdx > 0)
				{
					idFromFilename = originalFilename.substring(0, sepIdx).toInt();
					int dotIdx = originalFilename.lastIndexOf('.');
					nameFromFilename = (dotIdx > sepIdx + 3) ? originalFilename.substring(sepIdx + 3, dotIdx) : originalFilename.substring(sepIdx + 3);
					nameFromFilename.replace("_", " ");
					nameFromFilename.trim();
				}

				int finalId = idField.toInt();
				if (finalId <= 0)
					finalId = idFromFilename;

				if (finalId <= 0)
				{
					String errorMsg = "Keine gültige ID. Bitte Ziel-ID eingeben oder Datei 'ID_-_Name.tmpl' verwenden.";
					notifyClients(errorMsg);
					request->send(400, "text/plain", errorMsg);
					cleanup();
					return;
				}

				String finalName;
				if (nameField.length() > 0)
					finalName = nameField;
				else if (nameFromFilename.length() > 0)
					finalName = nameFromFilename;
				else
					finalName = "Unbenannt_" + String(finalId);

				LOG_PRINTF("Finale Auswahl: finalId=%d, finalName='%s'\n", finalId, finalName.c_str());
				LOG_PRINTLN(String("[Upload] Starte Import für finalId=") + finalId + ", context->total_len=" + context->total_len);

				// Validierung 2: Ziel-ID-Bereich
				if (finalId < 1 || finalId > 200)
				{
					String errorMsg = "Fehler: Ziel-ID ist ungültig: " + String(finalId);
					notifyClients(errorMsg);
					request->send(400, "text/plain", errorMsg);
					cleanup();
					return;
				}

				// Validierung 3: Prüfen, ob ID bereits belegt ist (KEIN automatisches Überschreiben)
// [Legacy] 				if (fingerManager.isFingerIdInList(finalId))
				{
					String errorMsg = "Fehler: Ziel-ID " + String(finalId) + " ist bereits in der Fingerliste vorhanden.";
					notifyClients(errorMsg);
					request->send(400, "text/plain", errorMsg);
					cleanup();
					return;
				}

				// Fingerabdruck importieren (liefert true bei Erfolg)
// [Legacy] 				bool importSuccess = fingerManager.importSingleFingerprint(finalId, context->buffer, context->total_len);

				if (!importSuccess)
				{
					String errorMsg = "Fehler: Upload zum Sensor fehlgeschlagen (siehe Logs für Details).";
					notifyClients(errorMsg);
					request->send(500, "text/plain", errorMsg);
					cleanup();
					return;
				}

				// --- NUR JETZT: Umbenennen & Erfolgsmeldung ---
				LOG_PRINTLN(String("Versuche, Fingerabdruck mit ID ") + finalId + " umzubenennen in " + finalName);
// [Legacy] 				fingerManager.renameFinger(finalId, finalName);
				LOG_PRINTLN(String("Fingerabdruck mit ID ") + finalId + " erfolgreich umbenannt in " + finalName);

				updateClientsFingerlist(getCurrentFingerlistHtml(), finalId);

				String successMsg = String("[Upload] ✅ Fingerabdruck für '") + finalName + "' (ID: " + finalId + ") erfolgreich hochgeladen und verifiziert.";
				notifyClients(successMsg);
				request->send(200, "text/plain", successMsg);

				// Speicher sicher freigeben
				cleanup();
			},
			// Handler 2: Der eigentliche Upload-Prozess (NUR Puffern!)
			[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
			{
				// Schritt 1: Beim ersten Chunk den Puffer initialisieren
				if (index == 0)
				{
					UploadContext *context = new (std::nothrow) UploadContext;
					if (!context)
					{
						LOG_PRINTLN("Fehler: Speicher für Upload-Kontext konnte nicht reserviert werden.");
						request->_tempObject = nullptr;
						return;
					}
					context->buffer = new (std::nothrow) uint8_t[FINGERPRINT_TEMPLATE_SIZE];
					if (!context->buffer)
					{
						LOG_PRINTLN("Fehler: Speicher für Upload-Puffer konnte nicht reserviert werden.");
						delete context;
						request->_tempObject = nullptr;
						return;
					}
					context->filename = filename;
					context->total_len = 0;
					context->overflowed = false;
					request->_tempObject = context;
				}

				UploadContext *context = (UploadContext *)request->_tempObject;
				if (!context || !context->buffer)
					return;

				// Schritt 2: Daten sicher in den Puffer kopieren (mit Überlaufschutz)
				size_t available_space = 0;
				if (index < FINGERPRINT_TEMPLATE_SIZE)
				{
					available_space = FINGERPRINT_TEMPLATE_SIZE - index;
				}
				size_t bytes_to_copy = min(len, available_space);
				if (bytes_to_copy > 0)
				{
					memcpy(context->buffer + index, data, bytes_to_copy);
				}

				// Overflow erkennen: mehr Daten empfangen als der Puffer fasst
				if (index + len > FINGERPRINT_TEMPLATE_SIZE)
				{
					context->overflowed = true;
				}

				// Schritt 3: Wenn Upload fertig, die *tatsächlich empfangene* Gesamtgröße speichern
				if (final)
				{
					context->total_len = index + len; // echte Gesamtgröße (auch bei Overflow)
				}
			});
#endif
		/////////////////////////////////////////////////////////////////////////////////////////////////////

		webServer.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
						noteHttpActivity(request);
						LOG_PRINTLN("ist rein von der stings buton. URI: " + request->url());
						//LOG_PRINTLN("Hat das Argument 'name': " + String(request->hasArg("name")));

						if (loggedIn || ENABLE_PASSWORD != "on")
							{

								if(request->hasArg("btnSaveSettings"))
								{
									LOG_PRINT("+++++++++++++++++++Save settings +++++++++++++++++++++");
									LOG_PRINTLN(request->arg("timezone"));
									
									AppSettings settings = settingsManager.getAppSettings();
									const String saveSection = request->arg("btnSaveSettings");

									bool requestContainsGeneralSettingsArgs = request->hasArg("mqtt_isBroker") || request->hasArg("mqtt_isClient") || request->hasArg("mqtt_port") || request->hasArg("timezone") || request->hasArg("wifiRssiDisconnectThreshold") || request->hasArg("wifiRoamImproveDb") || request->hasArg("wifiRoamMinRssi") || request->hasArg("enablePassword") || request->hasArg("btnSaveSettings");

									if (requestContainsGeneralSettingsArgs)
									{
										// MQTT Checkboxen:
										// - Wenn MQTT-Form gespeichert wird (btnSaveSettings=mqtt), dann dürfen Checkboxen auch AUS gehen.
										// - Bei fremden Saves (Telegram/Tedee/...) wollen wir NICHT unbeabsichtigt überschreiben.
										if (saveSection == "mqtt")
										{
											settings.mqtt_isBroker = request->hasArg("mqtt_isBroker");
											settings.mqtt_isClient = request->hasArg("mqtt_isClient");
											// Broker gewinnt, falls beide gesetzt wurden.
											if (settings.mqtt_isBroker)
											{
												settings.mqtt_isClient = false;
											}
										}
										else
										{
											// Patch-Semantik: nur auf true setzen, niemals unerwartet auf false.
											bool mqttModeChanged = false;
											if (request->hasArg("mqtt_isBroker"))
											{
												settings.mqtt_isBroker = true;
												mqttModeChanged = true;
											}
											if (request->hasArg("mqtt_isClient"))
											{
												settings.mqtt_isClient = true;
												mqttModeChanged = true;
											}
											if (mqttModeChanged)
											{
												if (settings.mqtt_isBroker)
												{
													settings.mqtt_isClient = false;
												}
											}
										}

										if (request->hasArg("mqtt_port"))
										{
											settings.mqtt_port = request->arg("mqtt_port");
										}
										if (request->hasArg("mqtt_keepAlive"))
										{
											settings.mqtt_keepAlive = request->arg("mqtt_keepAlive");
										}
										if (request->hasArg("mqtt_server"))
										{
											settings.mqttServer = request->arg("mqtt_server");
										}
										if (request->hasArg("mqtt_username"))
										{
											settings.mqttUsername = request->arg("mqtt_username");
										}
										if (request->hasArg("mqtt_password"))
										{
											settings.mqttPassword = request->arg("mqtt_password");
										}
										if (request->hasArg("mqtt_rootTopic"))
										{
											settings.mqttRootTopic = request->arg("mqtt_rootTopic");
										}
										if (request->hasArg("ntpServer"))
										{
											settings.ntpServer = request->arg("ntpServer");
										}
										if (request->hasArg("timezone"))
										{
											String tzArg = request->arg("timezone");
											if (!tzArg.isEmpty())
											{
												settings.ntpOffset = tzArg;
											}
										}
										if (request->hasArg("passwordSetup"))
										{
											String passwordArg = request->arg("passwordSetup");
											if (!passwordArg.isEmpty())
											{
												settings.passwordSetup = passwordArg;
											}
										}
										// enablePassword ist eine Checkbox.
										// In settings.html wird IMMER ein Default gesendet (hidden enablePassword=off)
										// und bei gesetzter Checkbox zusätzlich enablePassword=on.
										// AsyncWebServerRequest::arg("enablePassword") kann dabei je nach Implementierung
										// das erste oder letzte Feld liefern. Deshalb werten wir explizit aus:
										// - Wenn irgendwo "on" vorkommt => on
										// - sonst => off
										// Wichtig: Nur im Password-Abschnitt übernehmen, damit fremde Saves nicht ungewollt die Security ändern.
										if (saveSection == "password" && request->hasArg("enablePassword"))
										{
											String enablePasswordEff = "off";
											const int paramCount = request->params();
											for (int i = 0; i < paramCount; i++)
											{
												const AsyncWebParameter *p = request->getParam(i);
												if (!p)
													continue;
												if (p->name() == "enablePassword")
												{
													const String v = p->value();
													if (v == "on")
													{
														enablePasswordEff = "on";
														break;
													}
												}
											}
											settings.enablePassword = enablePasswordEff;
										}

										// WiFi Stabilität
										if (request->hasArg("wifiRssiDisconnectThreshold"))
										{
											int inputRssiDisconnect = request->arg("wifiRssiDisconnectThreshold").toInt();
											settings.wifiRssiDisconnectThreshold = (inputRssiDisconnect == 0) ? settingsManager.getAppSettings().wifiRssiDisconnectThreshold : inputRssiDisconnect;
										}
										if (request->hasArg("wifiRoamImproveDb"))
										{
											settings.wifiRoamImproveDb = normalizeWifiRoamImproveDb(request->arg("wifiRoamImproveDb").toInt());
										}
										if (request->hasArg("wifiRoamMinRssi"))
										{
											settings.wifiRoamMinRssi = normalizeWifiRoamMinRssi(request->arg("wifiRoamMinRssi").toInt());
										}
									}

									settingsManager.saveAppSettings(settings);


									request->redirect("/");
									shouldReboot = true;
								} else {
									if (!fsFileReady(request, "/settings.html"))
										return;
									AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/settings.html", String(), false, processor);
									response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
									request->send(response);
								}

							} else {
								if (!fsFileReady(request, "/login.html"))
									return;
								request->send(LittleFS, "/login.html", String(), false);
						} });

		/////////////////////////////////////////////////////////////////////////
		// [Legacy] /settingsScanner entfernt – FingerprintScanner nicht mehr vorhanden
		webServer.on("/settingsScanner", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
						 request->send(410, "text/plain", "Scanner-Einstellungen nicht verfuegbar (Legacy)");
					 });
		/////////////////////////////////////////////////////////////////////////
		// [Legacy] /saveButtonLabel entfernt – Toggle-Buttons nicht mehr vorhanden
		webServer.on("/saveButtonLabel", HTTP_POST, [](AsyncWebServerRequest *request)
					 {
						 request->send(410, "text/plain", "Button-Label nicht verfuegbar (Legacy)");
					 });

		/////////////////////////////////////////////////////////////////////////

		webServer.on("/pairing", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
	  if (loggedIn || ENABLE_PASSWORD != "on") { // Auth-Check (P2-04)
	  if(request->hasArg("btnDoPairing"))
	  {
		LOG_PRINTLN("Do (re)pairing");
		doPairing();
		request->redirect("/");
	  } else {
		if (!fsFileReady(request, "/settings.html"))
		  return;
		request->send(LittleFS, "/settings.html", String(), false, processor);
	  }
	  } else {
		if (!fsFileReady(request, "/login.html"))
		  return;
		request->send(LittleFS, "/login.html", String(), false);
	  } });

		webServer.on("/factoryReset", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
		if (loggedIn || ENABLE_PASSWORD != "on") { // Auth-Check (P2-04)
		if(request->hasArg("btnFactoryReset"))
		{
			notifyClients("HARD Reset gestartet: Fingerprints + komplette NVS (inkl. WLAN) werden gelöscht...");

			// Fingerprints sitzen im Sensor -> separat löschen
// [Legacy] 			if (!fingerManager.deleteAll())
				notifyClients("Die Fingerdatenbank konnte nicht gelöscht werden.");

			// NVS komplett löschen (inkl. WLAN, alle alten Namespaces/Keys)
			esp_err_t err = nvs_flash_erase();
			if (err != ESP_OK) {
				notifyClients(String("NVS erase FEHLER: ") + esp_err_to_name(err));
			} else {
				err = nvs_flash_init();
				if (err != ESP_OK) {
					notifyClients(String("NVS init FEHLER: ") + esp_err_to_name(err));
				} else {
					notifyClients("NVS komplett gelöscht. Neustart...");
				}
			}

			// WICHTIG: keine deleteAppSettings()/deleteWifiSettings() mehr,
			// weil NVS bereits komplett weg ist (und wir keine neuen Einträge erzeugen wollen).

			request->redirect("/");
			shouldReboot = true;
		} else {
			if (!fsFileReady(request, "/settings.html"))
				return;
			request->send(LittleFS, "/settings.html", String(), false, processor);
				}
				} else {
					if (!fsFileReady(request, "/login.html"))
						return;
					request->send(LittleFS, "/login.html", String(), false);
				} });
		/////////////////////////////////////////////////////////////////////////
		webServer.on("/softReset", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
					if (loggedIn || ENABLE_PASSWORD != "on") { // Auth-Check (P2-04)
					if(request->hasArg("btnSoftReset"))
					{
						notifyClients("SOFT Reset gestartet: WLAN + Fingerprints bleiben, nur App-Einstellungen werden gelöscht...");

						if (!settingsManager.deleteAppSettings())
							notifyClients("App-Einstellungen konnten nicht gelöscht werden.");

						request->redirect("/");
						shouldReboot = true;
					} else {
						if (!fsFileReady(request, "/settings.html"))
							return;
						request->send(LittleFS, "/settings.html", String(), false, processor);
					}
					} else {
						if (!fsFileReady(request, "/login.html"))
							return;
						request->send(LittleFS, "/login.html", String(), false);
					} });
		/////////////////////////////////////////////////////////////////////////
		webServer.on("/wificonfigFirst", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
		if (request->hasArg("btnWificonfig")) {
			
			if (!fsFileReady(request, "/wificonfig.html"))
				return;
			AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/wificonfig.html", String(), false, processor);
			response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
			request->send(response);
			LOG_PRINTLN("Started WiFi-Config mode");

		} });

		///////////////////////////////////////////////////////////////////////////

		webServer.on("/deleteAllFingerprints", HTTP_GET, [](AsyncWebServerRequest *request)
					 {
	  if (loggedIn || ENABLE_PASSWORD != "on") { // Auth-Check (P2-04)
	  if(request->hasArg("btnDeleteAllFingerprints"))
	  {
		notifyClients("Alle Fingerabdrücke werden gelöscht...");

// [Legacy] 		if (!fingerManager.deleteAll())
		  notifyClients("Die Fingerdatenbank konnte nicht gelöscht werden.");

		request->redirect("/");

	  } else {
		if (!fsFileReady(request, "/settings.html"))
		  return;
		request->send(LittleFS, "/settings.html", String(), false, processor);
	  }
	  } else {
		if (!fsFileReady(request, "/login.html"))
		  return;
		request->send(LittleFS, "/login.html", String(), false);
	  } });

		// Fallback-Handler für nicht gefundene Routen (effizientes Caching für statische Assets)
		webServer.onNotFound([](AsyncWebServerRequest *request)
							 {
			noteHttpActivity(request);
			String path = request->url();
			String contentType = "text/plain";
			bool isHtml = path.endsWith(".html");

			if (path.endsWith(".css")) contentType = "text/css";
			else if (path.endsWith(".js")) contentType = "application/javascript";
			else if (path.endsWith(".png")) contentType = "image/png";
			else if (path.endsWith(".jpg")) contentType = "image/jpeg";
			else if (path.endsWith(".ico")) contentType = "image/x-icon";
			else if (isHtml) contentType = "text/html";

			if (LittleFS.exists(path)) {
				AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, contentType);
				
				const bool isBootstrap = (path == "/bootstrap.min.css");

				// HTML immer frisch
				if (isHtml) {
					response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
				}
				// Nur bootstrap darf lange gecached werden
				else if (isBootstrap) {
					response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
				}
				// Alles andere absichtlich NICHT cachen (verhindert "alte Assets")
				else {
					response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
				}
				if (isHtml) {
#if USE_TELEGRAM
					noteHeavyHttpTransferForTelegram();
#endif
				} // HTML-Datei aus LittleFS: Telegram kurz pausieren
				request->send(response);
			} else {
				request->send(404);
			} });

		// === OTA im Normalmodus (GANZ UNTEN) & Start ===
		// Je nach Checkbox: Mit oder ohne Passwort
		if (ENABLE_PASSWORD == "on")
		{
			String otaPassword = resolveSetupPassword();
			Serial.println("[OTA] ✅ Passwortschutz aktiviert!");
			Serial.print("[OTA] Passwort: ");
			Serial.println(otaPassword);
			DIAG_LOG("OTA", "ElegantOTA begin auth");
			ElegantOTA.begin(&webServer, "admin", otaPassword.c_str());
		}
		else
		{
			Serial.println("[OTA] ⚠️   OTA ohne Passwortschutz (Checkbox nicht gesetzt)");
			DIAG_LOG("OTA", "ElegantOTA begin open");
			ElegantOTA.begin(&webServer);
		}
		DIAG_LOG("OTA", "ElegantOTA ready");

		ElegantOTA.onStart([]() {
			otaInProgress = true;
			Serial.println("[OTA] Upload gestartet - Netzwerk-Tasks pausiert.");
		});
		ElegantOTA.onEnd([](bool success) {
			otaInProgress = false;
			Serial.println(success ? "[OTA] Update erfolgreich." : "[OTA] Update fehlgeschlagen.");
		});

		// /otaPrepare: Wird vom Button per JS aufgerufen BEVOR /update geladen wird
		webServer.on("/otaPrepare", HTTP_GET, [](AsyncWebServerRequest *request) {
			otaInProgress = true;
			Serial.println("[OTA] Vorbereitung - Netzwerk-Tasks pausiert.");
			request->send(204);
		});

	} // end normal operating mode.

	// common url callbacks ESP.restart();

#if USE_TEDEE
	// ✅ Nur wenn enabled: Endpoints registrieren
	registerTedeeEndpoints(webServer);
	if (settingsManager.getAppSettings().tedee_enabled)
	{
		addLogMessage("✅ Tedee: Modul aktiviert ✅ - registriere Endpoints...");
		LOG_PRINTLN("✅ Tedee: Modul aktiviert ✅ - registriere Endpoints...");
	}
	else
	{
		addLogMessage("🎯 Tedee: Modul deaktiviert 🎯 - keine Endpoints registriert");
		LOG_PRINTLN("🎯 Tedee: Modul deaktiviert 🎯 - keine Endpoints registriert");
	}
#endif
#if USE_NUKI
	registerNukiEndpoints(webServer);
	if (settingsManager.getAppSettings().nuki_enabled)
	{
		addLogMessage("✅ Nuki: Modul aktiviert ✅ - Endpoints registriert...");
		LOG_PRINTLN("✅ Nuki: Modul aktiviert ✅ - Endpoints registriert...");
	}
	else
	{
		addLogMessage("🎯 Nuki: Modul deaktiviert 🎯 - Endpoints trotzdem registriert (für Save/Enable)");
		LOG_PRINTLN("🎯 Nuki: Modul deaktiviert 🎯 - Endpoints trotzdem registriert (für Save/Enable)");
	}
#endif
#if USE_TELEGRAM
	// ✅ Endpoints IMMER registrieren, damit Save/Enable auch bei deaktiviertem Modul funktioniert
	registerTelegramEndpoints(webServer);

	if (settingsManager.getAppSettings().telegram_enabled)
	{
		addLogMessage("✅ Telegram: Modul aktiviert ✅ - Endpoints registriert...");
		LOG_PRINTLN("✅ Telegram: Modul aktiviert ✅ - Endpoints registriert...");
	}
	else
	{
		addLogMessage("🎯 Telegram: Modul deaktiviert 🎯 - Endpoints trotzdem registriert (für Save/Enable)");
		LOG_PRINTLN("🎯 Telegram: Modul deaktiviert 🎯 - Endpoints trotzdem registriert (für Save/Enable)");
	}
#endif

	// ===== Bridge-Status-API =====
	webServer.on("/api/bridge/status", HTTP_GET, [](AsyncWebServerRequest *request) {
		noteHttpActivity(request);
		if (!(loggedIn || ENABLE_PASSWORD != "on")) {
			request->send(401, "application/json", "{\"error\":\"Nicht angemeldet\"}");
			return;
		}
		const AppSettings app = settingsManager.getAppSettings();
		const WifiSettings wifi = settingsManager.getWifiSettings();
		String json;
		json.reserve(256);
		json = "{";
		json += "\"hostname\":\"" + wifi.hostname + "\",";
		json += "\"uptime_ms\":" + String(millis()) + ",";
#if USE_MQTT_ANY
		json += "\"mqtt_status\":\"" + mqttStatusString + "\",";
		json += "\"mqtt_mode\":\"" + getModeString() + "\",";
#else
		json += "\"mqtt_status\":\"disabled\",";
		json += "\"mqtt_mode\":\"off\",";
#endif
		json += "\"mqtt_root_topic\":\"" + app.mqttRootTopic + "\",";
		json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
		json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
		json += "\"heap_free\":" + String(ESP.getFreeHeap());
		json += "}";
		AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
		response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
		request->send(response);
	});

	// ===== Bridge-Trigger-API =====
	webServer.on("/api/bridge/trigger", HTTP_POST, [](AsyncWebServerRequest *request) {
		noteHttpActivity(request);
		if (!(loggedIn || ENABLE_PASSWORD != "on")) {
			request->send(401, "application/json", "{\"error\":\"Nicht angemeldet\"}");
			return;
		}
#if USE_MQTT_CLIENT
		if (!request->hasArg("id")) {
			request->send(400, "application/json", "{\"error\":\"Parameter id fehlt\"}");
			return;
		}
		int triggerId = request->arg("id").toInt();
		if (triggerId < 0 || triggerId > 255) {
			request->send(400, "application/json", "{\"error\":\"id ungueltig (0-255)\"}");
			return;
		}
		bridgeSendTrigger((uint8_t)triggerId);
		notifyClients("Bridge-Trigger " + String(triggerId) + " gesendet", "[API]");
		request->send(200, "application/json", "{\"ok\":true,\"trigger\":" + String(triggerId) + "}");
#else
		request->send(503, "application/json", "{\"error\":\"MQTT Client nicht kompiliert\"}");
#endif
	});

	// common url callbacks ESP.restart();
	webServer.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
				 {
		LOG_PRINTLN("Restart wurde ausgeführt.");

		// Sende eine HTTP-Redirect-Antwort (optional mit ?return=...)
		String returnTo = "/index.html";
		if (request->hasParam("return"))
		{
			returnTo = request->getParam("return")->value();
			if (!returnTo.startsWith("/"))
				returnTo = "/" + returnTo;
		}
		request->redirect(returnTo);
		notifyClients("Neustart wurde eingeleitet...");
		// P2-01: Kein delay() im async Handler — stattdessen shouldReboot-Flag setzen.
		// reboot() wird in loop() sauber ausgeführt (inkl. MQTT-Disconnect, WiFi-Teardown).
		shouldReboot = true; });

	/// begin Webserver initialisieren saveDataBank ////////////////////////////

	/// end Webserver initialisieren saveDataBank ///////////////////////////////
	// [Legacy] /toggle0-6 entfernt – Toggle-Buttons (OutputPins, Klingel) nicht mehr vorhanden
	for (const char* legacyPath : {"/toggle0", "/toggle1", "/toggle2", "/toggle3", "/toggle4", "/toggle5", "/toggle6"}) {
		webServer.on(legacyPath, HTTP_POST, [](AsyncWebServerRequest *request) {
			request->send(410, "text/plain", "Toggle-Endpunkt nicht verfuegbar (Legacy)");
		});
	}
	/////////////////////////////////////////////////////////////////////////////////////////////
	// Serverstart EINMAL am Ende
	// Start server
	DIAG_LOG("WEB", "routes registered");
	webServer.begin();
	DIAG_LOG("WEB", "server begin OK");
}



void reboot()
{
	notifyClients("Das System wird jetzt neu gestartet...");
	delay(500); // Gnadenfrist für UI-Benachrichtigung

	// Sauberes Herunterfahren aller Services
#if USE_MQTT_CLIENT
	mqttExpectedDisconnect = true;
	if (mqttClient.connected())
	{
		bool netLocked = false;
		if (networkMutex)
		{
			netLocked = (xSemaphoreTake(networkMutex, pdMS_TO_TICKS(200)) == pdTRUE);
		}
		mqttClient.disconnect();
		if (netLocked && networkMutex)
		{
			xSemaphoreGive(networkMutex);
		}
	}
#endif
	delay(200); // Zeit für MQTT-Disconnect
	dnsServer.stop();
	webServer.end();
	WiFi.disconnect(false, false); // Explizit weiche Trennung vor Neustart
	delay(300);					   // Zeit für WiFi-Deauthentication

	ESP.restart();
}

// P4-16b: connectMqttClient() entfernt (vollständig durch MqttConnectionManager ersetzt)

#if USE_MQTT_CLIENT
void onMqttClientConnect(bool sessionPresent)
{
	mqttClientConnectSessionPresentPending = sessionPresent;
	mqttClientConnectEventPending = true;
	return;

#if 0
	mqttManager.onMqttConnect(sessionPresent); // Weiterleiten an den Manager
	mqttClient.subscribe((mqttRootTopic + "/#").c_str(), 0);
	mqttClientConnectUiPending = true;
	return;
	String connectionVia = mqttManager.getLastConnectionVia();
	if (connectionVia.isEmpty())
	{
		notifyClients("🟢 MQTT-Client: Verbindung hergestellt.🟢");
	}
	else
	{
		notifyClients("🟢 MQTT-Client: Verbindung hergestellt via " + connectionVia + ".🟢");
	}
	// publishMqttMessage(mqttRootTopic + "/status", "online", true, 1); // QoS 1
	mqttClient.subscribe((mqttRootTopic + "/#").c_str(), 0);
	mqttClientConnectUiPending = true;
#endif
}

void onMqttClientDisconnect(AsyncMqttClientDisconnectReason reason)
{
	mqttClientDisconnectReasonPending = (int)reason;
	mqttClientDisconnectWasExpectedPending = mqttExpectedDisconnect;
	mqttClientDisconnectWifiConnectedPending = WiFi.isConnected();
	mqttExpectedDisconnect = false;
	mqttClientDisconnectEventPending = true;
	return;

#if 0
	mqttManager.onMqttDisconnect(reason); // Weiterleiten an den Manager

	bool wasExpected = mqttExpectedDisconnect;
	mqttExpectedDisconnect = false;
	mqttClientDisconnectReasonPending = (int)reason;

	if (WiFi.isConnected())
	{
		if (wasExpected)
		{
			mqttClientDisconnectExpectedUiPending = true;
		}
		else
		{
			mqttClientDisconnectUnexpectedUiPending = true;
			return;
			const char *reasonStr = mqttDisconnectReasonToString(reason);
			static String lastDisconnectReasonStr = "";
			if (lastDisconnectReasonStr != String(reasonStr))
			{
				notifyClients(String("❌ MQTT-Client: Verbindung verloren (") + reasonStr + ") – Broker möglicherweise offline. ❌");
				lastDisconnectReasonStr = String(reasonStr);
			}
			// /* fingerManager Legacy */; // WLAN vorhanden, aber MQTT/Broker offline -> langsames Gelb-Blinken
		}
	}
	else
	{
		mqttClientDisconnectWifiOfflineUiPending = true;
		// notifyClients("MQTT-Client: ❌ ⚠️ ❌ Verbindung verloren????? ❌ ⚠️ ❌.");
	}
#endif
}

static void processPendingMqttClientUiActions()
{
	if (mqttClientDisconnectEventPending)
	{
		mqttClientDisconnectEventPending = false;

		AsyncMqttClientDisconnectReason reason = (AsyncMqttClientDisconnectReason)mqttClientDisconnectReasonPending;
		const bool wasExpected = mqttClientDisconnectWasExpectedPending;
		const bool wifiWasConnected = mqttClientDisconnectWifiConnectedPending;

		mqttManager.onMqttDisconnect(reason);

		if (wifiWasConnected)
		{
			if (wasExpected)
			{
				mqttClientReconnectGraceStartedAt = 0;
				mqttClientReconnectGraceReason = "";
				if (mqttStatusString != "disabled")
				{
					updateMqttStatus("offline");
				}
			}
			else
			{
				const char *reasonStr = mqttDisconnectReasonToString(reason);
				if (mqttClientReconnectGraceStartedAt == 0 && mqttStatusString != "offline")
				{
					mqttClientReconnectGraceStartedAt = millis();
				}
				mqttClientReconnectGraceReason = String(reasonStr);
				if (mqttStatusString != "reconnecting" && mqttStatusString != "offline")
				{
					updateMqttStatus("reconnecting");
				}
			}
		}
		else
		{
			mqttClientReconnectGraceStartedAt = 0;
			mqttClientReconnectGraceReason = "";
			updateMqttStatus("offline");
			LOG_PRINTLN("MQTT-Verbindung getrennt, oder kein WLAN. Warte auf WLAN-Wiederherstellung.");
		}
	}

	// ===== STABILER KERNBEREICH =====
	// MQTT-Reconnect-Grace-Phase bewusst so belassen.
	// Diese Logik verhindert unnoetige Offline-Meldungen bei kurzen Broker-/WLAN-Unterbrechungen.
	// Nur aendern, wenn echtes Fehlverhalten nachweisbar ist.
	// UI bewusst ruhig halten: Detailgruende nur ins Log, nicht mehrfach an Benutzer melden.
	if (mqttClientReconnectGraceStartedAt != 0)
	{
		if (mqttClient.connected() || !WiFi.isConnected())
		{
			mqttClientReconnectGraceStartedAt = 0;
			mqttClientReconnectGraceReason = "";
		}
		else if ((millis() - mqttClientReconnectGraceStartedAt) >= MQTT_CLIENT_RECONNECT_GRACE_MS)
		{
			mqttClientReconnectGraceStartedAt = 0;
			if (mqttStatusString != "disabled")
			{
				updateMqttStatus("offline");
			}

			String reasonText = mqttClientReconnectGraceReason;
			if (reasonText.isEmpty())
			{
				reasonText = "unbekannter Grund";
			}
			LOG_PRINTLN(String("[MQTT] Grace-Timeout abgelaufen. Server nicht erreichbar. Grund: ") + reasonText);
			// /* fingerManager Legacy */;
			mqttClientReconnectGraceReason = "";
		}
	}

	if (mqttClientConnectEventPending)
	{
		mqttClientConnectEventPending = false;
		const bool sessionPresent = mqttClientConnectSessionPresentPending;
		mqttClientReconnectGraceStartedAt = 0;
		mqttClientReconnectGraceReason = "";

		mqttManager.onMqttConnect(sessionPresent);
		mqttClient.subscribe((mqttRootTopic + "/#").c_str(), 0);

		updateMqttStatus("online");
		String connectionVia = mqttManager.getLastConnectionVia();
		if (connectionVia.isEmpty())
		{
			LOG_PRINTLN("[MQTT] MQTT-Client verbunden.");
		}
		else
		{
			LOG_PRINTLN(String("[MQTT] MQTT-Client verbunden via ") + connectionVia + ".");
		}
		// /* fingerManager Legacy */;
	}
}

// MQTT Heartbeat/Watchdog START
void onMqttClientPublish(uint16_t) { /* no-op in KeepAlive-only Mode */ }
// MQTT Heartbeat/Watchdog END
#endif // USE_MQTT_CLIENT

#if USE_MQTT_ANY
static bool isMqttKeepAlivePayload(const String &payload)
{
	return payload == "online" || payload == "ping";
}
#endif // USE_MQTT_ANY

// Minimales Topic-Rate-Limit für UI-Log (unterdrückt Bursts)
// UI flood control

// formatMqttDisplayMessage() entfernt (P3-02) — wurde nie gelesen

// Minimal stub for missing UI function to avoid compile error
void uiMaybeShowMinimal(const String &, const String &, const String &) { /* no-op */ }

#if USE_MQTT_ANY
// Zentrale Funktion zur Verarbeitung von MQTT-Nachrichten
void handleMqttMessage(const String &topic, const String &fullPayload, const String &source, bool retained)
{
	const AppSettings &app = settingsManager.getAppSettings();
	String payload = fullPayload;
	String extractedSource = source; // Standardwert ist die ursprüngliche Quelle

	// Payload parsen, um ALLE "source:[...];" Präfixe zu entfernen
	// Beispiel: "source:[FingerGaraj79];source:[WEB];true" → "true"
	while (payload.startsWith("source:["))
	{
		int start = payload.indexOf('[') + 1;
		int end = payload.indexOf(']');
		if (end > start)
		{
			extractedSource = payload.substring(start, end); // Letzten Source merken
			int semicolon = payload.indexOf(';', end);
			if (semicolon != -1)
			{
				payload = payload.substring(semicolon + 1); // Nach Semikolon weitermachen
			}
			else
			{
				break; // Kein Semikolon mehr gefunden
			}
		}
		else
		{
			break; // Ungültiges Format
		}
	}

	// Führende/nachfolgende Leerzeichen entfernen
	payload.trim();

	const String rootTopic = mqttRootTopic;
	const String logPrefix = "[MQTT][" + extractedSource + "]";
	const String sourceTag = "[" + extractedSource + "]";

	// Ignoriere leere Topics
	if (topic.isEmpty())
	{
		return;
	}

	// Topic-Validierung: Akzeptiere eigene Topics UND fremde Topics von anderen Geräten
#if USE_MQTT_ANY
	bool isOwnTopic = topic.startsWith(rootTopic);

	// Self-echo suppression nur für eigene Topics
	if (isOwnTopic && topic != (rootTopic + "/notify") && isRecentLocalMqttPublish(topic, payload))
	{
		LOG_PRINTLN(logPrefix + " self-echo unterdrückt");
		return;
	}
#endif

	// Befehl aus dem Topic extrahieren (der Teil nach dem letzten '/')
	String command;
	int lastSlashIndex = topic.lastIndexOf('/');
	if (lastSlashIndex != -1 && lastSlashIndex < topic.length() - 1)
	{
		command = topic.substring(lastSlashIndex + 1);
	}
	else
	{
		LOG_PRINTF("%s Kein Handler für Topic '%s' (kein Befehl gefunden)\n", logPrefix.c_str(), topic.c_str());
		return;
	}

	// Befehls-Dispatching
	if (command == "notify" || topic.endsWith("/notify"))
	{
		notifyClients(payload, sourceTag.c_str());
	}
	else
	{
		// Optional: Log unknown commands if needed, but avoid spamming the log.
		// LOG_PRINTF("%s Unbekannter Befehl '%s' in Topic '%s'\n", logPrefix.c_str(), command.c_str(), topic.c_str());
	}
}
#endif // USE_MQTT_ANY

#if USE_MQTT_CLIENT
void onMqttClientMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
	// P2-06: Statischer Buffer für Multi-Chunk-Nachrichten.
	// AsyncMqttClient liefert Chunks sequentiell auf dem async TCP-Thread —
	// parallele Nachrichten verschiedener Topics treten daher nicht auf.
	static String mqttPayloadBuffer;
	if (index == 0)
	{
		mqttPayloadBuffer = "";
		mqttPayloadBuffer.reserve(total);
	}

	// P2-06: concat() statt zeichenweiser Append (effizienter bei großen Chunks)
	mqttPayloadBuffer.concat(payload, len);

	if (index + len < total)
	{
		return;
	}

	String topicStr = String(topic);
	String messageTemp = std::move(mqttPayloadBuffer); // P2-06: Move statt Copy
	mqttPayloadBuffer = "";

	if (isMqttKeepAlivePayload(messageTemp))
	{
		return;
	}

	const String sourceLabel = (mqttClientId[0] != '\0') ? String(mqttClientId) : String("client");
	// formattedMessage entfernt (P3-02) — wurde berechnet aber nie gelesen

	LOG_PRINTF("[MQTT][CLIENT] topic='%s' payload='%s' qos=%d retain=%s dup=%s len=%u index=%u total=%u\n",
			   topic,
			   messageTemp.c_str(),
			   properties.qos,
			   properties.retain ? "yes" : "no",
			   properties.dup ? "yes" : "no",
			   static_cast<unsigned int>(len),
			   static_cast<unsigned int>(index),
			   static_cast<unsigned int>(total));

	uiMaybeShowMinimal(topicStr, messageTemp, sourceLabel);

	handleMqttMessage(topicStr, messageTemp, sourceLabel, properties.retain);
}
#endif // USE_MQTT_CLIENT

// Funktion zum Trennen und Säubern (Trimmen) der Zeitzonen-Strings timezoneName, timezoneOffset
void splitAndTrim(const String &input, const String &delimiter, String &part1, String &part2)
{
	int delimiterIndex = input.indexOf(delimiter); // Suche nach dem Trennzeichen, damit der Code flexibel bleibt

	if (delimiterIndex != -1)
	{
		// Trennzeichen gefunden
		part1 = input.substring(0, delimiterIndex);					  // Erster Teil vor dem Trennzeichen
		part2 = input.substring(delimiterIndex + delimiter.length()); // Zweiter Teil nach dem Trennzeichen

		// Entferne führende und nachfolgende Leerzeichen in part1 und part2
		part1.trim();
		part2.trim();

		// Falls part1 nach dem Trimmen leer ist, wird alles als Offset behandelt
		if (part1.length() == 0)
		{
			part2 = input; // Speichere gesamten Input in part2
			part2.trim();
			part1 = "Name | ??"; // Standardwert für leeren Namen setzen
		}
	}
	else
	{
		// Kein Trennzeichen gefunden
		part1 = "Name???"; // part1 bleibt leer
		part2 = input;	   // kompletten Input als Offset in part2 speichern
		part2.trim();	   // Trimmen von führenden und nachgestellten Leerzeichen in part2
	}

	// Sicherstellen, dass part2 nicht das Trennzeichen enthält, falls es fälschlicherweise drin ist
	if (part2.startsWith(delimiter))
	{
		part2 = part2.substring(delimiter.length());
		part2.trim();
	}
}

#if USE_MQTT_BROKER
void onMqttBrokerNewClient(String clientID, String clientIP, String username, int passwordLen)

{
	clientID.trim();
	if (clientID.isEmpty())
	{
		clientID = "<unbekannt>";
	}

	// Nur erfolgreiche CONNECTs kommen hier an.
	// Den aktuellen Stand liefern wir direkt aus dem Broker (robuster als eigener App-Counter).

	// Policy aus AppSettings rekonstruieren
	String cfgUser = settingsManager.getAppSettings().mqttUsername;
	String cfgPass = settingsManager.getAppSettings().mqttPassword;
	String policyMode;
	String cfgUserStr = cfgUser.isEmpty() ? "<empty>" : cfgUser;
	String cfgPassStr = cfgPass.isEmpty() ? "<empty>" : "<set>";
	if (cfgUser.isEmpty())
	{
		policyMode = "ANON";
	}
	else if (!cfgUser.isEmpty() && cfgPass.isEmpty())
	{
		policyMode = "USER";
	}
	else
	{
		policyMode = "USER+PASS";
	}

	// try-Flags bestimmen
	bool uFlag = !username.isEmpty();
	bool pFlag = passwordLen > 0;
	String tryStr = String(uFlag ? "U" : "-") + String(pFlag ? "P" : "-");

	// Log-Ausgabe
	LOG_PRINT("[MQTT][AUTH][BROKER] clientId='");
	LOG_PRINT(clientID);
	LOG_PRINT("' ip=");
	LOG_PRINT(clientIP);
	LOG_PRINT(" cfg=");
	LOG_PRINT(policyMode);
	LOG_PRINT(" try=");
	LOG_PRINT(tryStr);
	LOG_PRINT(" | cfgUser='");
	LOG_PRINT(cfgUserStr);
	LOG_PRINT("' cfgPass:");
	LOG_PRINT(cfgPassStr);
	LOG_PRINT(" | userIn='");
	LOG_PRINT(uFlag ? username : "<none>");
	LOG_PRINT("' passIn:");
	if (pFlag)
	{
		LOG_PRINT("<present> (PW-Len: ");
		LOG_PRINT(String(passwordLen));
		LOG_PRINT(")");
	}
	else
	{
		LOG_PRINT("<none>");
	}
	LOG_PRINTLN();

	// Konsistenzhinweise
#if LOGGING
	if (policyMode == "USER" && !uFlag)
	{
		LOG_PRINTLN("[MQTT][AUTH][BROKER][HINWEIS] Username erwartet, aber keiner geliefert");
	}
	if (policyMode == "USER" && pFlag)
	{
		LOG_PRINTLN("[MQTT][AUTH][BROKER][HINWEIS] Passwort wurde gesendet, ist in USER-Mode nicht erforderlich");
	}
	if (policyMode == "USER+PASS" && (!uFlag || !pFlag))
	{
		LOG_PRINTLN("[MQTT][AUTH][BROKER][HINWEIS] Username+Passwort erwartet, unvollständig geliefert");
	}
#endif

	// SSE-Event für Broker-Client-Anzahl senden (nur bei verbundenen Clients)
	if (isUiActive())
	{
		String brokerCountStr = String(mqttBroker ? mqttBroker->getConnectedClientCount() : 0);
		sendSSEEvent(brokerCountStr.c_str(), "mqtt_broker_clients", millis());
		LOG_PRINTF("🟢 [MQTT][BROKER] Client verbunden. Gesamt 🟢 : %s\n", brokerCountStr.c_str());
	}
}

void onMqttBrokerClientDisconnect(String clientID)
{
	clientID.trim();
	if (clientID.isEmpty())
	{
		clientID = "<unbekannt>";
	}

	LOG_PRINTF("[MQTT][BROKER] Client getrennt: %s. Gesamt: %u\n",
			   clientID.c_str(), (unsigned)(mqttBroker ? mqttBroker->getConnectedClientCount() : 0));

	// SSE-Event für Broker-Client-Anzahl senden (nur bei verbundenen Clients)
	if (isUiActive())
	{
		String brokerCountStr = String(mqttBroker ? mqttBroker->getConnectedClientCount() : 0);
		sendSSEEvent(brokerCountStr.c_str(), "mqtt_broker_clients", millis());
	}
}

void onMqttBrokerMessage(String clientID, String topic, String message)
{
	if (isMqttKeepAlivePayload(message))
	{
		return;
	}

	// Minimalistisch: clientID als sourceLabel nutzen, falls vorhanden
	String sourceLabel = clientID;
	if (sourceLabel.isEmpty())
		sourceLabel = "BROKER";
	// formattedMessage entfernt (P3-02) — wurde berechnet aber nie gelesen
	uiMaybeShowMinimal(topic, message, sourceLabel);

	LOG_PRINTF("[MQTT][BROKER] client='%s' topic='%s' payload='%s'\n",
			   clientID.c_str(),
			   topic.c_str(),
			   message.c_str());

	// Tag für Broker-Nachricht, clientID als sourceLabel weitergeben
	handleMqttMessage(topic, message, sourceLabel);
}
#endif // USE_MQTT_BROKER

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void tryNtpSyncAtStartup()
{
	if (timeSet)
		return;

	// NTP-Synchronisierung nur versuchen, wenn nicht im WiFi-Konfigurationsmodus
	if (currentMode == Mode::wificonfig)
	{
		LOG_PRINTLN("🔄 NTP-Synchronisierung übersprungen - WiFi-Konfigurationsmodus aktiv");
		return;
	}

	unsigned long scheduledAt = millis() + 3000UL;
	if (!ntpQuickSyncPending || ntpQuickSyncEarliestAt > scheduledAt)
	{
		ntpQuickSyncPending = true;
		ntpQuickSyncEarliestAt = scheduledAt;
	}
	LOG_PRINTLN("[NTP] Startup-Sync vorgemerkt - laeuft spaeter im Hintergrund.");
}

void tryNtpSyncDaily()
{
	if (!timeSet)
		return;
	// Throttle: nach fehlgeschlagenem Tages-Sync maximal alle 5 Minuten erneut vormerken
	unsigned long now = millis();
	if (lastDailyNtpAttemptAt != 0 && (now - lastDailyNtpAttemptAt) < 300000UL)
		return;
	struct tm timeinfo;
	if (getLocalTime(&timeinfo, 250))
	{
		if (timeinfo.tm_mday != lastNtpDay)
		{
			lastDailyNtpAttemptAt = now;
			unsigned long scheduledAt = millis() + 3000;
			if (!ntpQuickSyncPending || ntpQuickSyncEarliestAt > scheduledAt)
			{
				ntpQuickSyncPending = true;
				ntpQuickSyncEarliestAt = scheduledAt;
				LOG_PRINTLN("[NTP] Ein Tag vergangen - Tages-Sync vorgemerkt.");
			}
		}
	}
}

void checkNetworkAndReconnectIfNeeded()
{
	static unsigned long lastWifiRoamScanAt = 0;

	// Nur im Station-Modus prüfen
	if (WiFi.getMode() != WIFI_STA)
		return;

	unsigned long now = millis();

	// Wenn verbunden: RSSI ueberwachen und ggf. Roaming oder Rettung starten
	if (WiFi.status() == WL_CONNECTED)
	{
		// Reconnect-Flags zurücksetzen (wir sind online)
		if (wifiReconnectPending || wifiBackoffIndex > 0)
		{
			// /* fingerManager Legacy */;
		}
		wifiReconnectPending = false;
		wifiReconnectForceBegin = false;
		wifiBackoffIndex = 0;
		wifiNextAttemptAt = 0;
		wifiOfflineSince = 0;

		long rssi = WiFi.RSSI();
		wifiLastMeasuredRssi = rssi;

		const AppSettings wifiRoamSettings = settingsManager.getAppSettings();
		const int wifiRoamMinRssi = normalizeWifiRoamMinRssi(wifiRoamSettings.wifiRoamMinRssi);
		// Grace Period: Frisch verbundene Verbindungen nicht sofort wegen RSSI trennen
		bool inGracePeriod = (wifiConnectedSince > 0 && (now - wifiConnectedSince) < RSSI_GRACE_PERIOD_MS);

		if (!inGracePeriod && rssi <= RSSI_DISCONNECT_THRESHOLD)
		{
			wifiStableSince = 0;
			applyWifiPerformanceProfile(WifiPerformanceProfile::Stability);
			DIAG_LOG("WIFI", "emergency reconnect by field1");
			LOG_PRINTF("[WiFi] Feld-1-Rettung: RSSI kritisch (%ld dBm <= %d dBm)\n", rssi, RSSI_DISCONNECT_THRESHOLD);
			notifyClients("WLAN-Signal kritisch (" + String(rssi) + " dBm). Weiche WLAN-Rettung wird gestartet.");

			WifiSettings ws = settingsManager.getWifiSettings();
			bool rescueStarted = false;
			hasLockedBssid = false;

			if (isWifiBestApCacheFresh())
			{
				bool netLocked = false;
				if (netTryLock(300, netLocked))
				{
					LOG_PRINTF("[WiFi] Feld-1-Rettung: verbinde mit gemerktem bestem AP (RSSI: %d dBm, Kanal: %d) ohne Feld-3-Prüfung\n",
							   wifiBestApCache.rssi, wifiBestApCache.channel);
					WiFi.disconnect(false, false);
					delay(50);
					WiFi.begin(ws.ssid.c_str(), ws.password.c_str(), wifiBestApCache.channel, wifiBestApCache.bssid);
					netUnlock(netLocked);
					rescueStarted = true;
				}
				else
				{
					LOG_PRINTLN("[NET-MUTEX] Feld-1-Rettung per Cache übersprungen (Mutex busy)");
				}
			}

			if (!rescueStarted)
			{
				LOG_PRINTLN("[WiFi] Feld-1-Rettung: kein frischer Best-AP-Cache -> frischer Scan");
				rescueStarted = connectToBestAP(false, true);
			}

			if (!rescueStarted)
			{
				bool netLocked = false;
				LOG_PRINTLN("[WiFi] Feld-1-Rettung: Fallback WiFi.begin mit gleicher SSID");
				if (netTryLock(300, netLocked))
				{
					WiFi.disconnect(false, false);
					delay(50);
					WiFi.begin(ws.ssid.c_str(), ws.password.c_str());
					netUnlock(netLocked);
				}
				else
				{
					LOG_PRINTLN("[NET-MUTEX] Feld-1-Fallback übersprungen (Mutex busy) -> Reconnect pending");
					requestWifiReconnect(250, true);
				}
			}
			return;
		}

		if (wifiStableSince == 0)
			wifiStableSince = now;

		if (rssi < wifiRoamMinRssi)
		{
			if ((now - lastWifiRoamScanAt) >= 60000UL)
			{
				lastWifiRoamScanAt = now;
				if (isBootNetworkGraceActive())
				{
					DIAG_LOG("WIFI", "skip scan: boot grace");
				}
				else if (hasActiveWebClients())
				{
					DIAG_LOG("WIFI", "roaming deferred: active HTTP/SSE");
				}
				else
				{
					connectToBestAP(false, false);
				}
			}
		}

		// Zurück zu Balanced wenn stabil
		if (wifiStableSince != 0 &&
			wifiPerformanceProfile != WifiPerformanceProfile::Balanced &&
			(now - wifiStableSince) >= WIFI_STABLE_RESTORE_MS &&
			wifiLastMeasuredRssi >= RSSI_GOOD_THRESHOLD)
		{
			applyWifiPerformanceProfile(WifiPerformanceProfile::Balanced);
		}
		return;
	}

	// Backoff prüfen
	if (now < wifiNextAttemptAt)
		return;

	// Versuch starten
	WifiSettings ws = settingsManager.getWifiSettings();
	LOG_PRINTLN("🔄 WLAN-Reconnect (Backoff Stufe " + String(wifiBackoffIndex) + ")...");
	applyWifiPerformanceProfile(WifiPerformanceProfile::Stability);
	wifiStableSince = 0;

	// Standardpfad bewusst schlank: vorhandenen BSSID-Lock bevorzugen,
	// sonst normales WiFi.begin(). AP-Scan nur bei echtem WLAN-Problem.
	const unsigned long offlineDuration = (wifiOfflineSince > 0) ? (now - wifiOfflineSince) : 0;
	const bool useProblemRecovery = (wifiBackoffIndex >= 3 || offlineDuration > 60000UL);
	const bool forceBegin = (wifiReconnectForceBegin || (WiFi.status() != WL_CONNECTED));

	if (forceBegin)
	{
		bool netLocked = false;

		if (!useProblemRecovery)
		{
			if (netTryLock(300, netLocked))
			{
				if (hasLockedBssid && lockedChannel > 0)
				{
					LOG_PRINTF("[WiFi] Reconnect-Pfad: BSSID-Lock (%02X:%02X:%02X:%02X:%02X:%02X, Kanal %d)\n",
							   lockedBssid[0], lockedBssid[1], lockedBssid[2], lockedBssid[3], lockedBssid[4], lockedBssid[5], lockedChannel);
					WiFi.begin(ws.ssid.c_str(), ws.password.c_str(), lockedChannel, lockedBssid);
				}
				else
				{
					LOG_PRINTLN("[WiFi] Reconnect-Pfad: WiFi.begin");
					WiFi.begin(ws.ssid.c_str(), ws.password.c_str());
				}
				netUnlock(netLocked);
			}
			else
			{
				LOG_PRINTLN("[NET-MUTEX] WiFi.begin übersprungen (Mutex busy) -> Reconnect pending");
				requestWifiReconnect(250, true);
				return;
			}
		}
		else if (hasLockedBssid && lockedChannel > 0)
		{
			// Nur im echten Problemfall: bekannte BSSID gezielt erneut versuchen
			LOG_PRINTF("[WiFi] Reconnect-Pfad: BSSID-Lock (%02X:%02X:%02X:%02X:%02X:%02X, Kanal %d)\n",
					   lockedBssid[0], lockedBssid[1], lockedBssid[2], lockedBssid[3], lockedBssid[4], lockedBssid[5], lockedChannel);
			if (netTryLock(300, netLocked))
			{
				WiFi.begin(ws.ssid.c_str(), ws.password.c_str(), lockedChannel, lockedBssid);
				netUnlock(netLocked);
			}
			else
			{
				LOG_PRINTLN("[NET-MUTEX] WiFi.begin übersprungen (Mutex busy) -> Reconnect pending");
				requestWifiReconnect(250, true);
				return;
			}

			if (wifiBackoffIndex >= 4)
			{
				LOG_PRINTLN("[WiFi] BSSID-Lock wird aufgehoben (zu viele Fehlversuche)");
				hasLockedBssid = false;
			}
		}
		else
		{
			// ===== STABILER KERNBEREICH =====
			// WLAN-Verbindung bewusst asynchron.
			// Keine blockierenden Wartezeiten oder Schleifen mehr einfuehren.
			// Systemstart und Weboberflaeche sollen auch bei WLAN-/MQTT-Problemen schnell verfuegbar bleiben.
			LOG_PRINTLN("[WiFi] Reconnect-Pfad: AP-Scan (echter WLAN-Problemfall)");
			if (!connectToBestAP(false, false))
			{
				LOG_PRINTLN("[WiFi] AP-Scan fehlgeschlagen, Fallback auf WiFi.begin");
				if (netTryLock(300, netLocked))
				{
					WiFi.begin(ws.ssid.c_str(), ws.password.c_str());
					netUnlock(netLocked);
				}
				else
				{
					LOG_PRINTLN("[NET-MUTEX] WiFi.begin übersprungen (Mutex busy) -> Reconnect pending");
					requestWifiReconnect(250, true);
					return;
				}
			}
		}
	}
	else
	{
		bool netLocked = false;
		if (netTryLock(300, netLocked))
		{
			if (hasLockedBssid && lockedChannel > 0)
			{
				LOG_PRINTF("[WiFi] Reconnect-Pfad: BSSID-Lock (%02X:%02X:%02X:%02X:%02X:%02X, Kanal %d)\n",
						   lockedBssid[0], lockedBssid[1], lockedBssid[2], lockedBssid[3], lockedBssid[4], lockedBssid[5], lockedChannel);
				WiFi.begin(ws.ssid.c_str(), ws.password.c_str(), lockedChannel, lockedBssid);
			}
			else
			{
				LOG_PRINTLN("[WiFi] Reconnect-Pfad: WiFi.begin");
				WiFi.begin(ws.ssid.c_str(), ws.password.c_str());
			}
			netUnlock(netLocked);
		}
		else
		{
			LOG_PRINTLN("[NET-MUTEX] WiFi.begin übersprungen (Mutex busy) -> Reconnect pending");
			requestWifiReconnect(250, false);
			return;
		}
	}

	// Reconnect-Request verbraucht (Backoff läuft unten weiter)
	wifiReconnectPending = false;
	wifiReconnectForceBegin = false;

	wifiBackoffIndex = min(wifiBackoffIndex + 1, WIFI_BACKOFF_STEPS_LEN - 1);
	wifiNextAttemptAt = now + WIFI_BACKOFF_STEPS[wifiBackoffIndex] + random(0, 3000);
}

void serviceWifiReconnect()
{
	// Zentraler, schneller Reconnect-Service: nur wenn nötig
	if (WiFi.getMode() != WIFI_STA)
		return;

	// Wenn offline oder pending (Mutex-busy Fälle): Check häufiger anstoßen
	if (wifiReconnectPending || WiFi.status() != WL_CONNECTED)
	{
		checkNetworkAndReconnectIfNeeded();
	}
}

void setup()
{
	// (Mutexe werden nach Serial.begin() erstellt, damit Fatal-Logs zuverlässig sind)

	// ------------------------------
	// 0) Brownout-Detektor deaktivieren
	// PRODUKTHINWEIS: Der R503-Fingerprint-Sensor erzeugt beim Einschalten kurze
	// Stromspitzen (~200mA Peaks), die den ESP32-Brownout-Detektor auslösen können.
	// Ohne Deaktivierung kommt es zu Boot-Loops auf vielen USB-Netzteilen.
	// VORAUSSETZUNG: Stabile 5V-Versorgung mit mindestens 1A wird erwartet.
	// Bei echtem Unterspannungsproblem wird der Watchdog (120s) einen Reset auslösen.
	// WARNUNG: Bei Batterie-/Solar-Betrieb sollte der Brownout-Detektor
	// reaktiviert und die Stromversorgung entsprechend dimensioniert werden.
	// Dann Schwelle prüfen: WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1);
	// ------------------------------
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

	// ------------------------------
	// 1) Watchdog-Timer initialisieren
	// ------------------------------
	esp_task_wdt_init(WDT_TIMEOUT, true);
	esp_task_wdt_add(NULL);
	// ------------------------------
	// 2) Serielle Schnittstelle & Hostname
	// ------------------------------
	Serial.begin(115200);
	DIAG_LOG("BOOT", "start");
	DIAG_LOG("BOOT", "Serial begin OK");

	// ------------------------------
	// 2a) Globale Mutexe erstellen (nach Serial.begin)
	// Wenn Mutex-Erstellung fehlschlägt, läuft das System ohne Locking weiter (Code prüft auf NULL).
	// ------------------------------
	networkMutex = xSemaphoreCreateMutex();
	logMutex = xSemaphoreCreateMutex();
	sseSendMutex = xSemaphoreCreateMutex();
	if (!networkMutex || !logMutex || !sseSendMutex)
	{
		Serial.println("\n[FATAL] ============================================");
		Serial.println("[FATAL] Mutex-Erstellung fehlgeschlagen (Speichermangel?)");
		Serial.println("[FATAL] System läuft im DEGRADED-Modus ohne Locking.");
		Serial.println("[FATAL] Race-Conditions bei Netzwerk/SSE/Log möglich!");
		Serial.println("[FATAL] ============================================\n");
		degradedMode = true;
	}

	addLogMessage("Systemstart: ESP32 Fingerscanner initialisiert.");
	addLogMessage("Firmware-Version: " + String(firmwareVersion));

#if LOGGING
	// Nach Serial.begin(...)
	LOG_PRINTLN();
	LOG_PRINTLN("==================== ESP32 FINGERSCANNER PARIP69 ====================");
	LOG_PRINTLN("=                  SYSTEM-NEUSTART (ASCII MODE)                    =");

	// TEMP: Systemstatus und Heap-Auslastung beim Start ausgeben
	uint32_t freeHeap = ESP.getFreeHeap();
	uint32_t maxHeap = ESP.getHeapSize();
	uint32_t usedHeap = maxHeap - freeHeap;
	float usedPercent = (float)usedHeap / (float)maxHeap * 100.0f;
	LOG_PRINTLN("==================== ESP32 HEAP STATUS ====================");
	LOG_PRINTF("Freier Heap:   %u Bytes\n", freeHeap);
	LOG_PRINTF("Maximal Heap:  %u Bytes\n", maxHeap);
	LOG_PRINTF("Genutzt:       %u Bytes (%.1f%%)\n", usedHeap, usedPercent);

#ifdef ESP32
	Serial.printf("Chip-Temperatur: %.1f C\n", temperatureRead());
#endif
	Serial.printf("Uptime: %lu Sekunden\n", millis() / 1000);
	LOG_PRINTF("Firmware-Version: %s\n", firmwareVersion);

	LOG_PRINTLN("Initialisierung beginnt...");
	LOG_PRINTLN();
#endif
	// Hinweis: Hostname wird erst NACH dem Laden der gespeicherten Wifi-Settings gesetzt (siehe Schritt 6),
	// damit DHCP (z.B. Fritz!Box) direkt den korrekten Namen sieht. Die frühere Position war vor loadWifiSettings().

#if USE_DHT
	dhtManager.setup(&events);
#endif

	// ------------------------------
	// 4) Pin-Konfiguration
	// ------------------------------
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	// ------------------------------
	// 5) LittleFS initialisieren
	// ------------------------------
	DIAG_LOG("BOOT", "LittleFS begin");
	if (!LittleFS.begin(false))
	{
		LOG_PRINTLN("⚠️  Fehler beim Mounten von LittleFS - System läuft ohne Dateisystem weiter");
		DIAG_LOG("BOOT", "LittleFS begin FAIL");
		// Nicht die gesamte Setup-Funktion beenden, sondern nur eine Warnung ausgeben
	}
	else
	{
		LOG_PRINTLN("✅ LittleFS erfolgreich initialisiert");
		DIAG_LOG("BOOT", "LittleFS begin OK");
	}

	// ------------------------------
	// 6) Einstellungen laden
	// ------------------------------
	DIAG_LOG("BOOT", "Settings load begin");
	const bool wifiSettingsLoaded = settingsManager.loadWifiSettings();
	const bool appSettingsLoaded = settingsManager.loadAppSettings();
	DIAG_PRINTF("BOOT", "Settings loaded wifi=%s app=%s",
				wifiSettingsLoaded ? "OK" : "FAIL",
				appSettingsLoaded ? "OK" : "FAIL");
	// WLAN-Parameter (direkte Übernahme aus AppSettings – int Felder haben Default-Werte)
	RSSI_DISCONNECT_THRESHOLD = settingsManager.getAppSettings().wifiRssiDisconnectThreshold;

	LOG_PRINTF("→ Feld-1-Rettungsschwelle: %d dBm\n", RSSI_DISCONNECT_THRESHOLD);

	// Jetzt (nach dem Laden) Hostname sicher setzen – wichtig für DHCP-Name in Router-Anzeige
	{
		const WifiSettings &wifiSettings = settingsManager.getWifiSettings();
		if (!wifiSettings.hostname.isEmpty())
		{
			WiFi.setHostname(wifiSettings.hostname.c_str());
			LOG_PRINTLN(String("Hostname gesetzt: ") + wifiSettings.hostname);
		}
		else
		{
			LOG_PRINTLN("Hostname leer – verwende Standard (espressif)");
		}
	}

	// Nach dem Laden der App-Settings den tatsächlichen MQTT-Startmodus korrekt ausgeben
	{
		const AppSettings &appSettings = settingsManager.getAppSettings();
		if (appSettings.mqtt_isBroker)
		{
			LOG_PRINTLN("→ Startet im MQTT-BROKER-Modus");
		}
		else if (appSettings.mqtt_isClient)
		{
			LOG_PRINTLN("→ Startet im MQTT-CLIENT-Modus");
		}
		else
		{
			LOG_PRINTLN("→ Startet ohne MQTT-Unterstützung");
		}
	}

#if USE_TEDEE
	{
		const AppSettings &appSettings = settingsManager.getAppSettings();
		LOG_PRINTLN("→ Tedee-Konfiguration (nach Laden aus NVS):");
		LOG_PRINT("   ▸ Aktiviert: ");
		LOG_PRINTLN(appSettings.tedee_enabled ? "JA" : "NEIN");
		LOG_PRINT("   ▸ Bridge-IP: ");
		if (appSettings.tedee_bridge_ip.isEmpty())
		{
			LOG_PRINTLN("(leer)");
		}
		else
		{
			LOG_PRINTLN(appSettings.tedee_bridge_ip);
		}
		LOG_PRINT("   ▸ Port: ");
		LOG_PRINTLN(String(appSettings.tedee_port));
		LOG_PRINT("   ▸ Token gespeichert: ");
		LOG_PRINTLN(appSettings.tedee_token.isEmpty() ? "NEIN" : "JA (aus Sicherheitsgründen verborgen)");
		LOG_PRINT("   ▸ Lock-ID: ");
		if (appSettings.tedee_lock_id <= 0)
		{
			LOG_PRINTLN("(nicht gesetzt)");
		}
		else
		{
			LOG_PRINTLN(String(appSettings.tedee_lock_id));
		}
	}
#endif

	www_password = resolveSetupPassword();
	ENABLE_PASSWORD = String(settingsManager.getAppSettings().enablePassword);
	// ------------------------------
	// [Legacy] Fingerprint-Scanner entfernt

	// 8) Netzwerkmode wählen
	// ------------------------------
	// (A) Konfig-Modus: kein WLAN konfiguriert ODER stabiles bewusstes Touch/Finger-Signal
	bool wifiConfigRequested = false;
	if (!settingsManager.isWifiConfigured())
	{
		// Kein WLAN konfiguriert = klarer Konfigurationsgrund, keine Debounce nötig
		wifiConfigRequested = true;
		LOG_PRINTLN("[Boot] Kein WLAN konfiguriert -> Konfigurationsmodus");
	}
	else
	{
		// Touch/Finger nur bei bewusst stabilem Signal akzeptieren (Debounce)
		// Verhindert versehentlichen Config-Start durch Glitches/Regentropfen beim Einschalten
		const int DEBOUNCE_READS = 5;
		const int DEBOUNCE_DELAY_MS = 100;
		int fingerConfirmed = 0;
		int touchConfirmed = 0;
		for (int i = 0; i < DEBOUNCE_READS; i++)
		{
			if (false) fingerConfirmed++;
			if (touchRead(TOUCH_PIN) < threshold) touchConfirmed++;
			if (i < DEBOUNCE_READS - 1) delay(DEBOUNCE_DELAY_MS);
		}
		if (fingerConfirmed >= 4 || touchConfirmed >= 4)
		{
			wifiConfigRequested = true;
			LOG_PRINTF("[Boot] Stabiler Config-Trigger erkannt (Finger: %d/5, Touch: %d/5)\n", fingerConfirmed, touchConfirmed);
		}
	}

	if (wifiConfigRequested)
	{
		currentMode = Mode::wificonfig;
		wifiConfigStartTime = millis(); // Starte Timer für potentiellen Timeout
		LOG_PRINTLN();
		LOG_PRINTLN("Starte WLAN-Konfigurationsmodus (AP)");
		DIAG_LOG("WIFI", "config AP begin");
		// /* fingerManager Legacy */;
		initWiFiAccessPointForConfiguration(); // setzt SoftAP
		startWebserver();					   // webServer.begin(), OTA, events
	}
	// (B) Normaler Modus: WLAN-Station
	else
	{
		currentMode = Mode::scan;
		LOG_PRINTLN();
		LOG_PRINTLN("Starte normalen Betriebsmodus: WLAN-Station");
		DIAG_LOG("WIFI", "station mode begin");

		initWifi(); // Verbindung versuchen – Hintergrund-Reconnect übernimmt bei Fehlschlag
		addLogMessage("Heap-Status: Freier Heap " + String(ESP.getFreeHeap()) + " Bytes.");
		addLogMessage("Modus: " + String(currentMode == Mode::wificonfig ? "WiFi-Konfiguration" : "Normaler Betrieb"));
		addLogMessage("Fingerprint-Sensor: " + String(false ? "Verbunden" : "Nicht verbunden"));
		addLogMessage("WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Verbunden" : "Nicht verbunden"));
		if (timeSet)
			addLogMessage("NTP-Zeit: Synchronisiert");
		else if (WiFi.status() == WL_CONNECTED)
			addLogMessage("NTP-Zeit: Synchronisierung vorgemerkt");
		else
			addLogMessage("NTP-Zeit: Noch nicht synchronisiert");

		// Web-UI & OTA & Events nach WiFi-Init starten (braucht WiFi.mode(WIFI_STA))
		startWebserver();

		// Pairing-Prüfung NACH Kern-Betriebsbereitschaft (Scanner+Web stehen bereits)
		if (false)
		{
			if (!checkPairingValid())
			{
				LOG_PRINTLN("Sicherheitsproblem! Ungültige Kopplung.");
				addLogMessage("Ungültige Sensor-Kopplung – bitte neu koppeln.");
			}
		}

		// NTP-Sync vormerken (non-blocking, läuft später im Hintergrund)
		tryNtpSyncAtStartup();

		// MQTT Startlogik nach neuer Spezifikation (modular per Build-Flag)
		DIAG_LOG("MQTT", "init begin");
		const AppSettings &appSettings = settingsManager.getAppSettings();
		bool isBroker = appSettings.mqtt_isBroker;
		bool isClient = appSettings.mqtt_isClient;

		// Port ggf. aktualisieren
#if USE_MQTT_ANY
		int configuredPort = 1883;
		if (!appSettings.mqtt_port.isEmpty())
		{
			int p = appSettings.mqtt_port.toInt();
			if (p > 0 && p <= 65535)
				configuredPort = p;
		}
		mqttPort = configuredPort; // globale Variable aktualisieren
#endif

#if !USE_MQTT_BROKER
		if (isBroker)
		{
			LOG_PRINTLN("⚠️ MQTT Broker ist nicht mitkompiliert (USE_MQTT_BROKER=0) – Broker-Modus wird ignoriert.");
			addLogMessage("⚠️ MQTT Broker ist nicht mitkompiliert (USE_MQTT_BROKER=0) – Broker-Modus wird ignoriert.");
			isBroker = false;
		}
#endif
#if !USE_MQTT_CLIENT
		if (isClient)
		{
			LOG_PRINTLN("⚠️ MQTT Client ist nicht mitkompiliert (USE_MQTT_CLIENT=0) – Client-Modus wird ignoriert.");
			addLogMessage("⚠️ MQTT Client ist nicht mitkompiliert (USE_MQTT_CLIENT=0) – Client-Modus wird ignoriert.");
			isClient = false;
		}
#endif

#if USE_MQTT_ANY
		if (isBroker && isClient)
		{
			LOG_PRINTLN("Warnung: Beide MQTT Modi aktiv – Broker hat Priorität.");
			addLogMessage("Warnung: Beide MQTT Modi aktiv – Broker hat Priorität.");
			isClient = false; // Broker priorisieren
		}

#if USE_MQTT_BROKER
		if (isBroker)
		{
			mqttRootTopic = appSettings.mqttRootTopic;
			if (!mqttBroker)
			{
				mqttBroker = new ESPAsyncMQTTBroker((uint16_t)mqttPort);
				if (isUiActive())
				{
					String brokerCountStr = String(mqttBroker ? mqttBroker->getConnectedClientCount() : 0);
					sendSSEEvent(brokerCountStr.c_str(), "mqtt_broker_clients", millis());
				}
				LOG_PRINTLN(String("Broker erstellt (Port ") + mqttPort + ")");
			}
			else
			{
				LOG_PRINTLN(String("Broker Port (bestehend) verwendet: ") + mqttPort);
			}

#ifdef BROKER_DEBUG_LEVEL
			mqttBroker->setDebugLevel(BROKER_DEBUG_LEVEL);
			LOG_PRINTF("[MQTT][BROKER] Debug-Level: %d\n", BROKER_DEBUG_LEVEL);
#else
			mqttBroker->setDebugLevel(0);
			LOG_PRINTF("[MQTT][BROKER] Debug-Level: %d (default)\n", 0);
#endif
			ESPAsyncMQTTBrokerConfig brokerConfig;
			LOG_PRINTLN("[DEBUG][BROKER] Raw username: '" + appSettings.mqttUsername + "'");
			LOG_PRINTLN("[DEBUG][BROKER] Raw password length: " + String(appSettings.mqttPassword.length()));
			brokerConfig.username = appSettings.mqttUsername;
			brokerConfig.username.trim();
			brokerConfig.password = appSettings.mqttPassword;
			brokerConfig.password.trim();
			LOG_PRINTLN("[DEBUG][BROKER] Trimmed username: '" + brokerConfig.username + "'");
			LOG_PRINTLN("[DEBUG][BROKER] Trimmed password length: " + String(brokerConfig.password.length()));
			brokerConfig.ignoreLoopDeliver = true;

			mqttBroker->setConfig(brokerConfig);

			String policyMode;
			String userStr = brokerConfig.username.isEmpty() ? "<empty>" : brokerConfig.username;
			String passStr = brokerConfig.password.isEmpty() ? "<empty>" : "<set>";
			if (brokerConfig.username.isEmpty())
				policyMode = "ANON";
			else if (!brokerConfig.username.isEmpty() && brokerConfig.password.isEmpty())
				policyMode = "USER";
			else
				policyMode = "USER+PASS";

			LOG_PRINT("[MQTT][BROKER] policy: user='");
			LOG_PRINT(userStr);
			LOG_PRINT("' pass:");
			LOG_PRINT(passStr);
			LOG_PRINT(" -> mode=");
			LOG_PRINTLN(policyMode);

			LOG_PRINTLN(String("Broker-Port aktiv: ") + mqttPort);

			mqttRootTopic = appSettings.mqttRootTopic;
			LOG_PRINTLN(String("Broker Root-Topic: ") + mqttRootTopic);

			mqttBroker->onClientConnect(onMqttBrokerNewClient);
			mqttBroker->onClientDisconnect(onMqttBrokerClientDisconnect);
			mqttBroker->onMessage(onMqttBrokerMessage);
			DIAG_LOG("MQTT", "broker begin");
			mqttBroker->begin();
			// Interner Broker nutzt QoS2-State pro Client; Fingerscanner verwendet nur die öffentliche Broker-API.
			updateMqttStatus("online");
			LOG_PRINTLN("MQTT Broker gestartet.");
			DIAG_LOG("MQTT", "broker init done");
		}
		else
#endif
#if USE_MQTT_CLIENT
			if (!isBroker && isClient)
		{
#if USE_MQTT_BROKER
			// Sicherstellen dass kein Broker läuft
			if (mqttBroker)
			{
				LOG_PRINTLN("Stoppe bestehenden Broker vor Client-Start...");
				mqttBroker->stop();
			}
#endif
			mqttRootTopic = appSettings.mqttRootTopic;
			mqttClient.onConnect(onMqttClientConnect);
			mqttClient.onDisconnect(onMqttClientDisconnect);
			mqttClient.onMessage(onMqttClientMessage);
			mqttClient.onPublish(onMqttClientPublish);
			updateMqttStatus("offline"); // bis verbunden
			mqttManager.begin();
			DIAG_LOG("MQTT", "client init done");
		}
		else
#endif
		{
			// Beide Flags false -> alles aus (runtime)
#if USE_MQTT_BROKER
			if (mqttBroker)
			{
				mqttBroker->stop();
				LOG_PRINTLN("MQTT Broker gestoppt (deaktiviert).");
			}
#endif
#if USE_MQTT_CLIENT
			mqttExpectedDisconnect = true;
			if (mqttClient.connected())
			{
				bool netLocked = false;
				if (networkMutex)
				{
					netLocked = (xSemaphoreTake(networkMutex, pdMS_TO_TICKS(200)) == pdTRUE);
				}
				mqttClient.disconnect();
				if (netLocked && networkMutex)
				{
					xSemaphoreGive(networkMutex);
				}
			}
#endif
			updateMqttStatus("disabled");
			LOG_PRINTLN("MQTT vollständig deaktiviert (kein Broker, kein Client).");
			DIAG_LOG("MQTT", "mqtt disabled");
		}
#endif

		// Netzwerkabhängige Zusatzmodule erst NACH Kern-Setup starten
		// (Scanner, WebServer, MQTT sind bereits initialisiert)
		// Im DEGRADED-Modus keine Zusatzmodule starten (Race-Conditions ohne Mutexe)
		if (!degradedMode)
		{
#if USE_TELEGRAM
			if (settingsManager.getAppSettings().telegram_enabled)
			{
				LOG_PRINTLN("[✅ Telegram] Modul aktiviert - starte Worker-Task...");
				telegram_init();
			}
			else
			{
				LOG_PRINTLN("[❌ Telegram] Modul deaktiviert - KEIN Worker-Task gestartet");
			}
#endif

#if USE_TEDEE
			if (settingsManager.getAppSettings().tedee_enabled)
			{
				LOG_PRINTLN("[✅ Tedee] Modul aktiviert - starte Worker-Task...");
				tedee_init();
			}
			else
			{
				LOG_PRINTLN("[❌ Tedee] Modul deaktiviert - KEIN Worker-Task gestartet");
			}
#endif
		}
		else
		{
			LOG_PRINTLN("[DEGRADED] Zusatzmodule übersprungen (Telegram/Tedee) – fehlende Mutexe");
		}
	}
	// Nur im normalen Betriebsmodus (nicht im WiFi-Konfigurationsmodus) weitere Tests durchführen
	if (currentMode != Mode::wificonfig)
	{
		// ------------------------------
		// 10) LED-Status anzeigen
		// ------------------------------
		if (false)
		{
			// /* fingerManager Legacy */;
		}
		else
		{
			// /* fingerManager Legacy */;
		}

		// ------------------------------
		// 12) SolarCalc initialisieren
		// ------------------------------
		// solarCalc.setup();

		// ------------------------------
		// 13) System-Status-Zusammenfassung
		// ------------------------------
		LOG_PRINTLN("🔍 Erstelle System-Status-Bericht...");
		LOG_PRINTLN();
		LOG_PRINTLN("╔══════════════════════════════════════════════════════════════╗");
		LOG_PRINTLN("║                    SYSTEM-STATUS BERICHT                    ║");
		LOG_PRINTLN("╠══════════════════════════════════════════════════════════════╣");
		LOG_PRINTLN("║ Firmware Version: " + String(firmwareVersion).substring(0, 25) + String(25 - String(firmwareVersion).substring(0, 25).length(), ' ') + "║");
		LOG_PRINTLN("║ Version Info: " + VersionInfo.substring(0, 29) + String(29 - VersionInfo.substring(0, 29).length(), ' ') + "║");
		LOG_PRINTLN("╠══════════════════════════════════════════════════════════════╣");

		LOG_PRINTLN("╠══════════════════════════════════════════════════════════════╣");
		LOG_PRINTLN("║ 🔐 Fingerprint-Sensor: " + String(false ? "✅ VERBUNDEN        " : "❌ NICHT VERBUNDEN  ") + "║");
		LOG_PRINTLN("║ 📶 WiFi-Modus: " + String(currentMode == Mode::wificonfig ? "🔧 KONFIGURATION    " : "📡 STATION         ") + "║");
		LOG_PRINTLN("╚══════════════════════════════════════════════════════════════╝");
		LOG_PRINTLN();
		LOG_PRINTLN("🚀 System bereit! ESP32 Fingerscanner Parip69 läuft...");
	}
	else
	{
		// WiFi-Konfigurationsmodus - minimale Ausgabe
		LOG_PRINTLN("🔧 WiFi-Konfigurationsmodus aktiv - Sensor-Tests übersprungen");
		LOG_PRINTLN("📱 Verbinden Sie sich mit dem Access Point und konfigurieren Sie WiFi");
	}

	LOG_PRINTLN("✅ System-Setup abgeschlossen - bereit für Betrieb");

	// MQTT-Statusmeldung
#if USE_MQTT_ANY
	AppSettings appSettings = settingsManager.getAppSettings();

	if (appSettings.mqtt_isBroker)
	{
		addLogMessage("✅ MQTT Broker: Aktiv auf Port " + String(mqttPort) + ", Root-Topic: " + mqttRootTopic);
	}
	else if (appSettings.mqtt_isClient)
	{
		addLogMessage("✅ MQTT Client: Aktiv, Root-Topic: " + mqttRootTopic);
	}
	else
	{
		addLogMessage("MQTT: deaktiviert");
	}
#endif

#if USE_TELEGRAM
	// ✅ GANZ AM ANFANG prüfen - wenn deaktiviert, keine Nachricht senden!
	if (settingsManager.getAppSettings().telegram_enabled)
	{
		sendTelegramMessage("System gestartet");
	}
	else
	{
		notifyClients("❌ Telegram: Modul deaktiviert - keine Startup-Nachricht gesendet");
		LOG_PRINTLN("[❌ Telegram] Deaktiviert - Startup-Nachricht wird NICHT gesendet");
	}
#endif

	// LOG_PRINTLN();
	//  LittleFS-Dateien auflisten (nur wenn LOGGING=1 in platformio.ini)
#if LOGGING
	File root = LittleFS.open("/");
	File file = root.openNextFile();
	while (file)
	{
		Serial.printf("LittleFS File: %s (%d bytes)\n", file.name(), file.size());
		file = root.openNextFile();
	}
#endif
	// LOG_PRINTLN();
	//  ✅ STARTUP-COMPLETE: Alle Module erfolgreich initialisiert
	Serial.println();
#if DIAG_LOG_ENABLED
	diagLogReadyStatus("setup finished");
#endif
	Serial.println("[SETUP] 🎯  System bereit für den Betrieb. 🎯");
	addLogMessage("✅ 🎉 System bereit für Betrieb. 🎉 ✅");
	Serial.println("========================================");
}

void loop()
{
	// Watchdog weniger frequent (das kann bleiben)
	static unsigned long lastWdt = 0;
	if (millis() - lastWdt > 30000)
	{
		esp_task_wdt_reset();
		lastWdt = millis();
	}

#if DIAG_LOG_ENABLED
	static bool diagTenSecondStatusLogged = false;
	if (!diagTenSecondStatusLogged && millis() >= 10000)
	{
		diagTenSecondStatusLogged = true;
		diagLogReadyStatus("10s status");
	}
#endif

	// MQTT-Broker Timeout-Verarbeitung aus Timer-ISR in loop() (BP1-01)
#if USE_MQTT_BROKER
	if (mqttBroker) mqttBroker->loop();
#endif

	// Effizienter, unregelmäßiger LED-Herzschlag, um Prozessoraktivität "lebendig" anzuzeigen
	static unsigned long nextBlinkTime = 0;
	if (millis() >= nextBlinkTime)
	{
		led1State = !led1State;
		digitalWrite(LED_BUILTIN, led1State);
		// Nächstes Blinken in 0.2 bis 2.5 Sekunden
		nextBlinkTime = millis() + random(200, 1500);
	}

	// Automatischer Reset nach 10 Minuten im WiFi-Konfigurationsmodus
	// (Hauptlogik unten im wificonfig-Block mit shouldReboot, ESP.restart() entfernt — P2-11)

	if (currentMode == Mode::wificonfig && wifiConfigStartTime > 0)
	{
		// Watchdog im Konfigurationsmodus häufiger zurücksetzen
		static unsigned long lastWdtConfig = 0;
		if (millis() - lastWdtConfig > 5000) // Alle 5 Sekunden im Konfigurationsmodus (häufiger als normale 30s)
		{
			esp_task_wdt_reset();
			lastWdtConfig = millis();
			LOG_PRINTLN("Watchdog im Konfigurationsmodus zurückgesetzt (alle 5s)");
		}

		// ⏰ INTELLIGENTE TIMEOUT-LOGIK: Nur wenn WiFi KONFIGURIERT wurde!
		// Wenn WiFi nicht konfiguriert ist → Bleibe im Config-Mode (unbegrenzt)
		// Wenn WiFi konfiguriert ist → Nach 1 Minute Neustart/Rausgehen
		if (settingsManager.isWifiConfigured())
		{
			// WiFi wurde konfiguriert → Timeout nach 10 Minuten
			if (millis() - wifiConfigStartTime > 600000) // 600000 ms = 10 Minuten wifiConfigStartTime
			{
				LOG_PRINTLN("");
				LOG_PRINTLN("⏰ WiFi wurde konfiguriert. Konfigurationsmodus Timeout (10 Minute). Starten des ESP32 neu...");
				shouldReboot = true;
			}
		}
		else
		{
			// WiFi ist NICHT konfiguriert → Bleibe im Config-Mode, kein Timeout!
			// Das ist absichtlich leer - keine Aktion nötig
		}
	}

	if (shouldReboot)
	{
		LOG_PRINTLN("🔄 Bereite Neustart vor...");
		digitalWrite(LED_BUILTIN, LOW); // LED ausschalten

		// LED-Ring zurücksetzen vor Neustart
		if (false)
		{
			LOG_PRINTLN("🔵 Setze LED-Ring zurück vor Neustart...");
			// /* fingerManager Legacy */; // oder setLedRingError(), je nach Zustand
			delay(500);						 // Kurz warten, damit der Ring den Befehl verarbeitet
		}

		reboot();
	}

	// MQTT Verbindungsmanagement nach neuer Spezifikation
	unsigned long now = millis();

	// AppSettings Snapshot: nur 1x pro Sekunde kopieren (verhindert Heap-Fragmentierung im Loop)
	static AppSettings appCache;
	static unsigned long lastAppCacheMs = 0;
	if (lastAppCacheMs == 0 || (now - lastAppCacheMs) >= 1000)
	{
		appCache = settingsManager.getAppSettings();
		lastAppCacheMs = now;
	}
	const AppSettings &app = appCache;

	// Nur ausführen, wenn MQTT-Client-Modus aktiv ist
#if USE_MQTT_CLIENT
	processPendingMqttClientUiActions();
	if (app.mqtt_isClient && !app.mqtt_isBroker)
	{
		mqttManager.loop(); // <-- ALLES an den Manager delegieren!

		// Optionaler Online-Status-Log & App-Heartbeat
		if (mqttClient.connected())
		{
			static unsigned long lastOnlineLog = 0;
			if (now - lastOnlineLog > 30000)
			{
				lastOnlineLog = now;
				LOG_PRINTLN("[CLIENT] ONLINE rssi=" + String(WiFi.RSSI()) + " heap=" + String(ESP.getFreeHeap()));
			}
		}
	}
#endif

#if USE_MQTT_BROKER
	// Harte 1-Hz-Drossel für Broker-Checks (falls Broker-Modus verwendet wird)
	if (app.mqtt_isBroker && now - lastBrokerMaintenance >= BROKER_CHECK_MIN_INTERVAL)
	{
		lastBrokerMaintenance = now;
	}
#endif

	// do the actual loop work
	switch (currentMode)
	{
	case Mode::scan:
		currentMode = Mode::maintenance;
		break;

	case Mode::wificonfig:
		// Blink-Funktion für LED1 aufrufen
		digitalWrite(LED_BUILTIN, HIGH); // LED dauerhaft an
										 // digitalWrite(LED_BUILTIN, HIGH); // LED einschalten
		dnsServer.processNextRequest();	 // used for captive portal redirect
		break;

	case Mode::maintenance:
		// do nothing, give webserver exclusive access to sensor (not thread-safe for concurrent calls)
		break;
	}

	// Auto-Logout nach Inaktivität
	if (ENABLE_PASSWORD == "on")
	{
		if (loggedIn)
		{
			if (millis() - lastActivityTime > 600000) // 600000 = 10 min
			{
				loggedIn = false;
				LOG_PRINTLN("Automatisches Ausloggen aufgrund von Inaktivität.");
			}
		}
	}

	// NTP- und Netzwerk-Checks mit kleinem Startdelay und täglichem Resync
	if (currentMode != Mode::wificonfig)
	{
		// --- Netzwerk-Selbstheilung: IMMER, unabhängig von timeSet ---
		if (millis() - lastNetworkCheck > networkCheckInterval)
		{
			lastNetworkCheck = millis();
			checkNetworkAndReconnectIfNeeded();
		}
		serviceWifiReconnect();

		// LED-Ring nach WiFi-Reconnect auf "bereit" setzen (aus Event-Callback heraus verschoben)
		if (wifiJustReconnected)
		{
			wifiJustReconnected = false;
			if (false)
			{
				// /* fingerManager Legacy */;
			}
		}

		// --- NTP: ein einziger Hintergrundweg ---
		// ntpQuickSyncPending steuert alles: Boot, Retry, Daily
		if (ntpQuickSyncPending &&
			WiFi.status() == WL_CONNECTED &&
			millis() >= ntpQuickSyncEarliestAt &&
			(!false || !timeSet)) // Ring-Touch blockiert Initialsync nicht
		{
			ntpQuickSyncPending = false;
			syncNtpTimeIfNeeded();

			// Falls immer noch nicht synchronisiert: nächsten Retry vormerken
			if (!timeSet)
			{
				unsigned long retryDelay;

				if (ntpRetryCount < 1)
					retryDelay = 10000UL;		// 1. Retry nach 10s
				else if (ntpRetryCount < 2)
					retryDelay = 30000UL;		// 2. Retry nach 30s
				else if (ntpRetryCount < 3)
					retryDelay = 60000UL;		// 3. Retry nach 1 Min
				else if (ntpRetryCount < 5)
					retryDelay = 120000UL;		// 4./5. Retry nach 2 Min
				else
					retryDelay = 300000UL;		// danach alle 5 Min

				ntpRetryCount++;
				ntpQuickSyncPending = true;
				ntpQuickSyncEarliestAt = millis() + retryDelay;
				DIAG_LOG("NTP", "retry delayed");
			}
			else
			{
				// Erfolg: Retry-Kaskade zurücksetzen für nächsten eventuellen Ausfall
				resetNtpRetryState();
				lastDailyNtpAttemptAt = 0; // Daily-Throttle zurücksetzen
			}
		}

		// Tägliche Re-Sync nur wenn Ring nicht berührt
		if (timeSet && !false)
		{
			tryNtpSyncDaily();
		}
	}

	// NTP und Netzwerk-Checks bleiben wie bisher, SNTP-Callback übernimmt regelmäßige Syncs

	// NEU: SSE-Heartbeat-Überwachung
	if (sseClientConnected && (millis() - lastUiAliveTime > UI_ALIVE_TIMEOUT_MS))
	{
		sseClientConnected = false;
		LOG_PRINTLN("[SSE-Heartbeat] UI-Client-Timeout. SSE-Stream wird pausiert.");
	}

	// DHT – verzögertes, bedarfsorientiertes Auslesen (nur wenn aktiviert)
#if USE_DHT
	// Die Variable 'someoneOnline' wird jetzt direkt durch 'sseClientConnected' ersetzt,
	// da diese nun die zuverlässige Information über einen aktiven Client enthält.
	dhtManager.loop(sseClientConnected);
#endif

	// WLAN-Qualität alle 30s senden – nur bei online Clients
	// Im DEGRADED-Modus keine Komfort-SSE-Sends (Mutexe fehlen, String-Allokationen vermeiden)
	if (isUiActive() && !degradedMode)
	{
		static unsigned long lastPush = 0;
		if (millis() - lastPush >= intervalWifiQuality)
		{
			lastPush = millis();

			// WiFi Quality mit Cache
			int wifiRssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
			String qStr = String("{\"quality\":") + rssiToQuality(wifiRssi) + ",\"rssi\":" + wifiRssi + "}";
			// Nur senden wenn sich geändert hat
			if (sseCache.wifiQualityChanged(qStr))
			{
				sendSSEEvent(qStr.c_str(), "wifi_quality", millis(), 1000);
			}

			// MQTT Status mit Cache
#if USE_MQTT_ANY
			if (sseCache.mqttStatusChanged(mqttStatusString))
			{
				sendSSEEvent(mqttStatusString.c_str(), "mqtt_status", millis());
			}
#endif

			// MQTT Broker Client-Anzahl mit Cache (nur im Broker-Modus)
#if USE_MQTT_BROKER
			if (app.mqtt_isBroker && mqttBroker)
			{
				// Echte Anzahl aus dem Broker (robuster als ein App-seitiger Zähler bei Reconnects)
				String brokerClientsStr = String(mqttBroker->getConnectedClientCount());
				if (sseCache.brokerClientsChanged(brokerClientsStr))
				{
					sendSSEEvent(brokerClientsStr.c_str(), "mqtt_broker_clients", millis());
				}
			}
#endif

			// Chip-Temperatur mit Cache (nur senden wenn > 0.5°C Änderung)
			float chipTemp = temperatureRead();
			String chipTempStr = String(chipTemp, 1);
			if (sseCache.chipTempChanged(chipTempStr))
			{
				LOG_PRINTLN("[SSE] Sende ChipTemp: " + chipTempStr + "°C");
				sendSSEEvent(chipTempStr.c_str(), "chip_temperature", millis());
			}
		}
	}

	// Längeres Delay für realistische CPU-Last-Messung
	delay(5);
}
