// Beschreibung: 🎉2.0.0 erste wo alles get mit dem Templade dowenload🎉
#pragma once

#include <WString.h>
#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include "freertos/semphr.h"

// Compile‑time Logging: -D LOGGING=1 (an) / -D LOGGING=0 (aus)
#ifndef LOGGING
#define LOGGING 1
#endif

#if LOGGING
#define LOG_PRINT(...) Serial.print(__VA_ARGS__)
#define LOG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define LOG_PRINTF(...) Serial.printf(__VA_ARGS__)
#define LOG_NL() Serial.println()
#else
#define LOG_PRINT(...)
#define LOG_PRINTLN(...)
#define LOG_PRINTF(...)
#define LOG_NL()
#endif

// ---------- Externe Symbole ----------
// Forward declaration
class DHTManager;

extern SemaphoreHandle_t networkMutex;
extern void notifyClients(String message, const char *sourceTag = nullptr);
extern String getTimestampString();
extern bool isBootNetworkGraceActive();
extern bool hasActiveWebClients();
#if USE_TELEGRAM
extern bool isTelegramNetworkPauseActive(); // Netzwerk-Pause-Ampel fuer Telegram (Definition in main.cpp)
#endif
extern const int LedBeleuchtung; // Externe Deklaration der Variable
extern AsyncEventSource events;
extern volatile bool timeSet; // Globale Zeit-Synchronisation-Flag
extern DHTManager dhtManager;

// --- MQTT Topic-Helper (Definition in main.cpp)
// Liefert "broker" / "client" / "off"
String getModeString();
// Baut "<root>/<mode>/<host>/<tail>"
String makeTopic(const String &tail);

// WebTemplates.h
// =============================================================================
// Web UI HTML/JavaScript Templates
// =============================================================================
// Zweck: Auslagerung aller großen HTML/JavaScript-Strings aus main.cpp
// für bessere Wartbarkeit und Übersichtlichkeit
// =============================================================================
// =============================================================================
// 🔒 Download/Upload Feature Templates (USE_DOWNLOAD_UPLOAD)
// =============================================================================
// Diese Templates werden nur kompiliert, wenn USE_DOWNLOAD_UPLOAD aktiviert ist
// =============================================================================

#if USE_DOWNLOAD_UPLOAD

// -----------------------------------------------------------------------------
// Download-Button Template mit JavaScript
// -----------------------------------------------------------------------------
// Beschreibung: Erzeugt einen Button zum Download eines ausgewählten
//               Fingerabdrucks als .tmpl-Datei
// -----------------------------------------------------------------------------
const char DOWNLOAD_BUTTON_HTML[] PROGMEM =
	"<button type=\"button\" id=\"btnDownload\" name=\"btnDownload\" class=\"btn btn-primary\" onclick=\"downloadFingerprint()\">Download</button>";

// -----------------------------------------------------------------------------
// Upload-Section Template mit umfassender JavaScript-Validierung
// -----------------------------------------------------------------------------
// Beschreibung: Erzeugt einen Upload-Button mit clientseitiger Validierung
//               - Prüft Dateiauswahl (.tmpl)
//               - Extrahiert ID aus Dateiname oder Eingabefeld
//               - Validiert ID-Bereich (1-200)
//               - Prüft auf Duplikate
//               - Sendet Daten via FormData an /uploadFinger
// -----------------------------------------------------------------------------
const char UPLOAD_SECTION_HTML[] PROGMEM = R"rawliteral(
<button id="btnUploadFingerprint" name="btnUploadFingerprint" class="btn btn-primary" type="button" onclick="uploadFingerprint()">Template hochladen</button>
<script>
	// Verbesserte Upload-Funktion mit umfassender clientseitiger Validierung.
	function uploadFingerprint() {
	  const fileInput = document.getElementById('fingerUploadFile');
	  const idInput = document.getElementById('newFingerprintId');
	  const nameInput = document.getElementById('newFingerprintName');

	  if (!fileInput.files || fileInput.files.length === 0) {
		alert('Bitte wählen Sie zuerst eine .tmpl-Datei aus.');
		return;
	  }

	  let effectiveIdStr = idInput.value.trim();
	  const filename = fileInput.files[0].name;

	  // Priorität 1: ID aus dem manuellen Eingabefeld.
	  // Priorität 2: Wenn das Feld leer ist, ID aus dem Dateinamen extrahieren.
	  if (!effectiveIdStr) {
		const match = filename.match(/^(\d+)/);
		if (match) {
		  effectiveIdStr = match[1];
		}
	  }

	  // --- Start der Validierung ---

	  // 1. Prüfen, ob eine ID ermittelt werden konnte.
	  if (!effectiveIdStr) {
		alert("Fehler: Es konnte keine ID ermittelt werden.\nBitte geben Sie eine ID im Feld 'Speicherplatz' an oder benennen Sie die Datei nach dem Schema 'ID_-_Name.tmpl'.");
		return;
	  }

	  const effectiveId = parseInt(effectiveIdStr, 10);

	  // 2. Prüfen, ob die ID eine gültige Zahl ist.
	  if (isNaN(effectiveId)) {
		alert(`Fehler: Die ermittelte ID \"${effectiveIdStr}\" ist keine gültige Zahl.`);
		return;
	  }

	  // 3. Prüfen, ob die ID im erlaubten Bereich von 1 bis 200 liegt.
	  if (effectiveId < 1 || effectiveId > 200) {
		alert(`Fehler: Die ID ${effectiveId} liegt außerhalb des gültigen Bereichs von 1-200.`);
		return;
	  }

	  // 4. Prüfen, ob die ID bereits in der Select-Liste vorhanden ist (Eindeutigkeit).
		const fingerprintSelect = document.getElementById('html_sel_fp');
	  for (let i = 0; i < fingerprintSelect.options.length; i++) {
		if (fingerprintSelect.options[i].value == effectiveId) {
		  alert(`Fehler: Der Speicherplatz ${effectiveId} ist bereits belegt. Bitte wählen Sie einen anderen Platz.`);
		  return;
		}
	  }

	  // --- Ende der Validierung ---

	  const file = fileInput.files[0];
	  const formData = new FormData();
	  formData.append('file', file);

	  // Sende die *validierte* ID als targetId.
	  formData.append('targetId', effectiveId);
	  formData.append('fingerName', nameInput.value.trim());

	  fetch('/uploadFinger', {
		method: 'POST',
		body: formData
	  })
		.then(response => response.text().then(text => ({ ok: response.ok, text })))
		.then(({ ok, text }) => {
		  alert(text);
		  if (ok) {
			fileInput.value = '';
			if (idInput) idInput.value = '';
			if (nameInput) nameInput.value = '';
		  }
		})
		.catch(err => {
		  console.error('Upload-Fehler:', err);
		  alert('Fehler beim Upload: ' + err.message);
		});
	}
</script>
)rawliteral";

// -----------------------------------------------------------------------------
// Upload-Hint Template
// -----------------------------------------------------------------------------
// Beschreibung: HTML-Formular für Datei-Upload-Feld mit Hinweistext
// -----------------------------------------------------------------------------
const char UPLOAD_HINT_HTML[] PROGMEM =
	"<!-- Upload eines einzelnen Fingerabdrucks aus .tmpl-Datei -->"
	"<div class=\"form-group\">"
	"<label class=\"col-md-4 control-label\" for=\"fingerUploadFile\">Fingerprint-Upload (.tmpl):</label>"
	"<div class=\"col-md-4\">"
	"<input id=\"fingerUploadFile\" name=\"fingerUploadFile\" type=\"file\" accept=\".tmpl\" class=\"form-control input-md\">"
	"<small class=\"text-muted\">Dateiname z.B. <strong>12_-_Max Mustermann.tmpl</strong>. ID und Name können automatisch aus dem Dateinamen übernommen werden.</small>"
	"</div></div>";

#endif // USE_DOWNLOAD_UPLOAD
