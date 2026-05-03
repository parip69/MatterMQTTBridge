
# Vom GITHUB holen
# Stand vom Repository in Ihren lokalen Arbeitsbereich zu holen

git pull

# 1: Lokale Änderungen speichern und später wieder anwenden (Stash)

# git stash
#   → Speichert deine aktuellen Änderungen TEMPORÄR in einem "Versteck" (Stack)
#   → Dein Arbeitsbereich wird danach in den letzten Commit-Zustand zurückgesetzt

# git pull
#   → Holt die neuesten Änderungen von GitHub und merged sie lokal
#   → Keine Konflikte möglich, da du gerade keine lokalen Änderungen hast

# git stash pop
#   → Holt deine gespeicherten Änderungen aus dem Versteck zurück
#   → Appliziert sie auf deinen aktuellen Code (ggf. mit Konflikten möglich)

git stash
git pull
git stash pop

# 2: Lokale Änderungen committen

# git add .
#   → Fügt ALLE geänderten Dateien in den Staging-Bereich ein (bereit zum Commit)

# git commit -m "Lokale Änderungen vor Pull"
#   → Erstellt einen lokalen Commit mit deinen Änderungen (speichert sie in der Git-Historie)

# git pull
#   → Holt die neuesten Änderungen von GitHub und merged sie automatisch mit deinem lokalen Code
#   → Falls Konflikte entstehen, musst du sie manuell lösen

git add .
git commit -m "Lokale Änderungen vor Pull"
git pull

# 3: Lokale Änderungen verwerfen (⚠️ ACHTUNG: Unwiederbringlich!)

git reset --hard HEAD
git pull

# Änderungen zu GitHub hochladen
⚡ <1ms Return-Zeit - sofort zurück
🔒 HTTPS/SSL - funktioniert mit Telegram
🔥 Fire-and-Forget - keine Rückmeldung abwarten
📝 Minimales Log - nur ✓ (Erfolg) oder ✗ (Fehler)
🧹 Auto-Cleanup - Task räumt sich selbst auf
💪 Zuverlässig - bewährte HTTPClient-Bibliothek 👉🎯 🎉 ⚠️❤️  🎉 
📡 🔔 🔄 🟢  📊 🎯 ✅  🔍  🚀   📱 ✅ ❌ 📂 ⏱️ 📦 🎉 ⚠️ 🎯 🎉 ⚠️❤️ 🎉

# ##################################################################
# Hoch laden #######################################################

1. Änderungen speichern und committen:
  

  git add .
  git commit -m "❤️❌WiFi config❌❤️"
  git push --force



  # #######################################################

git add . 
git commit -m "🔄 ❌ ⚠️ ❌  MDS optimierung ❌ ⚠️ ❌ ⚠️ 🔄"
git push origin main


2. Änderungen zu GitHub hochladen:

  git push --force


# #######################################################
########################################################

# Weitere nützliche Git-Befehle
git add .
git commit -m "AGENTS"
git push --force
## Status und Übersicht

  git status           # Zeigt den aktuellen Status der Arbeitskopie
  git log              # Zeigt die Commit-Historie
  git log --oneline    # Kompakte Übersicht der Commits
  git diff             # Zeigt Unterschiede zwischen Arbeitskopie und letztem Commit
  git diff HEAD        # Zeigt Unterschiede zum letzten Commit
  git diff <branch1> <branch2>   # Vergleicht zwei Branches

## Themen-Icons Übersicht

### Netzwerk & Kommunikation
📡 WLAN / Netzwerk
📱 Mobilgerät
🔔 Benachrichtigung
🔄 Synchronisierung

#### WLAN-Status & Varianten
📶 WLAN verbunden
🚫📶 WLAN nicht verbunden
❌📡 WLAN getrennt
⚠️📡 WLAN Gefahr / unsicher
🔒📡 WLAN gesichert
🔓📡 WLAN offen
📶🟢 WLAN starkes Signal
📶🟡 WLAN mittleres Signal
📶🔴 WLAN schwaches Signal
📶⏳ WLAN verbindet...
📶❌ WLAN Fehler
📡🌐 Netzwerk online
📡❌ Netzwerk offline
📡🔄 Netzwerk wird gesucht
📡⚡ Netzwerk schnell
📡🐢 Netzwerk langsam

#### MQTT-Status
🟣 MQTT verbunden
⚫ MQTT nicht verbunden
🔄 MQTT verbindet...
❌ MQTT Fehler
⚠️ MQTT Warnung

### Haus & Sicherheit
🏠 Haus
🚀 Zugang / Öffnen
🔍 Überwachung / Suche
⚠️ Warnung / Gefahr

### Status & Aktionen
🟢 Aktiv / Online
✅ Erfolgreich / OK
❌ Fehler / Abbruch
⏱️ Zeit / Timer
🔄 Aktualisieren / Wiederholen

### Daten & Verwaltung
📊 Statistik / Auswertung
🎯 Ziel / Fokus
📂 Datei / Ordner
📦 Paket / Lieferung

### Sonstiges
🎉 Erfolg / Abschluss

---
Weitere Icons können nach Bedarf ergänzt werden.
## Branches

  git branch           # Listet alle lokalen Branches auf
  git branch -r        # Listet alle Remote-Branches auf
  git checkout <branch>    # Wechselt zu einem Branch
  git merge <branch>       # Führt einen Branch in den aktuellen zusammen

## Remote vergleichen

  git fetch            # Holt die neuesten Änderungen vom Remote, ohne zu mergen
  git diff origin/main # Vergleicht lokalen Stand mit Remote-Main
  git log origin/main  # Zeigt die Historie des Remote-Main-Branches

## Änderungen rückgängig machen

  git reset --hard HEAD    # Setzt alles auf den letzten Commit zurück
  git checkout -- <datei>  # Verwirft Änderungen an einer Datei
  git revert <commit>      # Macht einen bestimmten Commit rückgängig

## Konflikte lösen

  git status              # Zeigt Konfliktdateien
  git add <datei>         # Nach dem Lösen eines Konflikts Datei zum Commit vormerken
  git commit              # Commit nach Konfliktlösung

########################################################


3.  git commit -m "Aktualisiere Projektdateien"; git push

git add .
git commit -m "AGENTS"
git push --force