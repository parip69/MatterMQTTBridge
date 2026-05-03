import os
import re
import shutil
import time
from datetime import datetime
from shutil import which

# Pfad zur Hauptdatei und Platzhalter für die Versionsnummer
main_file = "src/main.cpp"  # Passe diesen Pfad ggf. an.
# @version: 1.4.150 Builddatum 18:53:45 02-04.2025

# Verzeichnis für Backups
project_dir = os.getcwd()
backup_base_dir = os.path.join(project_dir, "MyBackupNeu")
os.makedirs(backup_base_dir, exist_ok=True)

# Backup-Flag
backup_done = False  # Flag zum Verhindern mehrmaliger Backups

def _running_in_ci():
    return os.getenv("GITHUB_ACTIONS") == "true" or os.getenv("CI") == "true"

def _backup_mode():
    return (os.getenv("P69_BACKUP_MODE") or "").strip().lower()

# Funktion zum Kopieren von Ordnern oder Dateien ins Backup
def backup_path(src_path, backup_folder):
    if os.path.exists(src_path):
        if os.path.isdir(src_path):
            shutil.copytree(src_path, os.path.join(backup_folder, os.path.basename(src_path)))
        else:
            shutil.copy(src_path, os.path.join(backup_folder, os.path.basename(src_path)))
        print(f"{os.path.basename(src_path)} erfolgreich gesichert.")
    else:
        print(f"{os.path.basename(src_path)} nicht gefunden.")

def _path_exists(p):
    try:
        return os.path.exists(p)
    except Exception:
        return False

# Funktion zum Umbenennen und Kopieren von Firmware- und SPIFFS/LittleFS-Dateien (env-bewusst)
def rename_firmware_and_spiffs(version, backup_dir, build_dir, env_name):
    firmware_path = os.path.join(build_dir, "firmware.bin")
    # LittleFS wird als littlefs.bin erstellt, SPIFFS als spiffs.bin - beide prüfen
    littlefs_path = os.path.join(build_dir, "littlefs.bin")
    spiffs_path = os.path.join(build_dir, "spiffs.bin")

    # Wartezeit zur Sicherheit
    print("⏱️  Wartezeit (2 Sekunden) - Dateien werden finalisiert...")
    time.sleep(2)

    # Unterordner "bin" für alle .bin-Dateien erstellen
    bin_dir = os.path.join(backup_dir, "bin")
    os.makedirs(bin_dir, exist_ok=True)

    # Firmware-Datei umbenennen und kopieren (mit env-Kennung)
    new_firmware_name = f"firmware_{env_name}_ver_{version}.bin"
    new_firmware_path = os.path.join(bin_dir, new_firmware_name)
    if _path_exists(firmware_path):
        shutil.copy(firmware_path, new_firmware_path)
        print(f"✅ Firmware gesichert: Bin/{new_firmware_name}")
    else:
        print("❌ Firmware-Datei nicht gefunden!")

    # Bootloader sichern (mit env-Kennung und Version)
    bootloader_path = os.path.join(build_dir, "bootloader.bin")
    new_bootloader_name = f"bootloader_{env_name}_ver_{version}.bin"
    new_bootloader_path = os.path.join(bin_dir, new_bootloader_name)
    if _path_exists(bootloader_path):
        shutil.copy(bootloader_path, new_bootloader_path)
        print(f"✅ Bootloader gesichert: Bin/{new_bootloader_name}")
    else:
        print("⚠️  Bootloader nicht gefunden (optional)")

    # Partitions-Tabelle sichern (mit env-Kennung und Version)
    partitions_path = os.path.join(build_dir, "partitions.bin")
    new_partitions_name = f"partitions_{env_name}_ver_{version}.bin"
    new_partitions_path = os.path.join(bin_dir, new_partitions_name)
    if _path_exists(partitions_path):
        shutil.copy(partitions_path, new_partitions_path)
        print(f"✅ Partitions gesichert: Bin/{new_partitions_name}")
    else:
        print("⚠️  Partitions-Datei nicht gefunden (optional)")

    # Dateisystem-Datei umbenennen und kopieren (LittleFS oder SPIFFS)
    # SPIFFS oder littlefs wird IMMER für ALLE Umgebungen mit "all" gekennzeichnet
    new_fs_name = f"littlefs_all_ver_{version}.bin"
    new_fs_path = os.path.join(bin_dir, new_fs_name)
    
    # Erst LittleFS versuchen, dann SPIFFS als Fallback
    if _path_exists(littlefs_path):
        shutil.copy(littlefs_path, new_fs_path)
        print(f"✅ LittleFS gesichert: Bin/{new_fs_name} (Quelle: littlefs.bin)")
    elif _path_exists(spiffs_path):
        shutil.copy(spiffs_path, new_fs_path)
        print(f"✅ SPIFFS gesichert: Bin/{new_fs_name}")
    else:
        print("❌ Dateisystem-Datei (littlefs.bin/spiffs.bin) nicht gefunden!")

# Funktion zur Erstellung des SPIFFS-Dateisystems vor dem Hauptprogramm
def build_spiffs_before_program(source, target, env):
    # Für das aktive Environment (min/max) buildfs ausführen
    env_name = env.get("PIOENV", "unknown")
    print(f"═══════════════════════════════════════════════════════════")
    print(f"  SPIFFS-Dateisystem wird erstellt (env={env_name})...")
    print(f"═══════════════════════════════════════════════════════════")
    # 'pio' verwenden und Environment explizit setzen; Fallback auf Windows-User-Path
    pio_cmd = which("pio") or which("platformio") or "C:/Users/gerha/.platformio/penv/Scripts/platformio.exe"
    result = os.system(f"\"{pio_cmd}\" run -e {env_name} --target buildfs")
    if result != 0:
        print("❌ Fehler beim Erstellen des SPIFFS-Dateisystems!")
        env.Exit(1)
    else:
        print(f"✅ SPIFFS erfolgreich erstellt für {env_name}")

# Funktion, die nach dem Build ausgeführt wird
def after_build(source, target, env, event_name="post-action"):
    global backup_done  # Verwende das globale Flag

    if _running_in_ci():
        print("ℹ️  CI-Umgebung erkannt, Backup- und SPIFFS-Post-Action wird übersprungen.")
        return

    backup_mode = _backup_mode()
    if backup_mode in ("skip", "off", "0", "false"):
        print(f"ℹ️  Backup für diesen Zwischenschritt deaktiviert (P69_BACKUP_MODE={backup_mode}).")
        return

    # Backup nur einmal durchführen
    if backup_done:
        return

    # Aktives Environment und Build-Verzeichnis ermitteln
    env_name = env.get("PIOENV", "unknown")
    build_dir = env.subst("$BUILD_DIR") if hasattr(env, 'subst') else os.path.join(project_dir, ".pio", "build", env_name)
    
    print(f"═══════════════════════════════════════════════════════════")
    print(f"  Backup-Post-Event: {event_name}, Environment: {env_name}")
    print(f"═══════════════════════════════════════════════════════════")
    
    # Dateisystem (LittleFS/SPIFFS) IMMER vor dem Backup erstellen (falls noch nicht vorhanden)
    littlefs_path = os.path.join(build_dir, "littlefs.bin")
    spiffs_path = os.path.join(build_dir, "spiffs.bin")
    
    # Prüfen ob LittleFS oder SPIFFS existiert
    if not (_path_exists(littlefs_path) or _path_exists(spiffs_path)):
        print(f"⚠️  Dateisystem nicht gefunden - wird jetzt erstellt...")
        build_spiffs_before_program(source, target, env)
    else:
        if _path_exists(littlefs_path):
            print(f"✅ LittleFS bereits vorhanden: {littlefs_path}")
        else:
            print(f"✅ SPIFFS bereits vorhanden: {spiffs_path}")

    # Versionsnummer auslesen und verwenden, ohne sie zu ändern
    with open(main_file, "r", encoding='utf-8', errors='ignore') as file:
        content = file.read()

    # Definieren Sie den Platzhalter für die Versionsnummer
    version_placeholder = "// @version:"

    # Suchen Sie nach der Versionsnummer
    version_match = re.search(rf'{version_placeholder} (\d+\.\d+\.\d+)', content)

    if version_match:
        current_version = version_match.group(1)
        print(f"Verwende bestehende Versionsnummer: {current_version}")
    else:
        raise ValueError("Keine gültige Versionsnummer im Hauptdateikommentar gefunden!")

    # Backup-Verzeichnis erstellen
    current_datetime_backup = datetime.now().strftime("%Y.%m.%d-%H.%M.%S")
    backup_dir = os.path.join(backup_base_dir, f"backup_{current_datetime_backup}_env_{env_name}_ver_{current_version}")

    try:
        os.makedirs(backup_dir, exist_ok=True)
        print(f"📁 Erstelle Backup im Ordner: {backup_dir}")
        print(f"")

        # Firmware / SPIFFS kopieren (wurden bereits vor dem Backup erstellt)
        rename_firmware_and_spiffs(current_version, backup_dir, build_dir, env_name)
        print(f"")


        # Quellverzeichnisse sichern
        print(f"📦 Sichere Projektdateien...")
        backup_path(os.path.join(project_dir, "src"), backup_dir)
        backup_path(os.path.join(project_dir, "data"), backup_dir)
        backup_path(os.path.join(project_dir, "platformio.ini"), backup_dir)
        backup_path(os.path.join(project_dir, "partitions.csv"), backup_dir)
        backup_path(os.path.join(project_dir, "update_backup_projekt_and_firmware.py"), backup_dir)
        backup_path(os.path.join(project_dir, "update_version_in_code.py"), backup_dir)
        backup_path(os.path.join(project_dir, "agent_worklog.md"), backup_dir)

        # Kopiere die Broker-Bibliothek auf gleicher Ebene (bleibt im Projekt-Backup)
        backup_mqtt_broker_lib(backup_dir, env_name)

        print(f"")
        print(f"═══════════════════════════════════════════════════════════")
        print(f"  ✅ Backup erfolgreich abgeschlossen!")
        print(f"  📂 {backup_dir}")
        print(f"═══════════════════════════════════════════════════════════")

        # Setze das Backup-Flag, um zu verhindern, dass es erneut ausgeführt wird
        backup_done = True

        # History und Brain NACH dem Backup kopieren (dauert lange, blockiert nicht die Anzeige)
        central_history_dir = r"D:\!FingerPrint\History"
        try:
            os.makedirs(central_history_dir, exist_ok=True)
            print(f"⏳ Aktualisiere zentrale History in: {central_history_dir} ...")

            # 1. VS Code History-Ordner sichern (ZENTRAL)
            user_home = os.path.expanduser("~")
            vscode_history_path = os.path.join(user_home, "AppData", "Roaming", "Code", "User", "History")
            if os.path.exists(vscode_history_path):
                target_vscode = os.path.join(central_history_dir, "VSCode_History")
                shutil.copytree(vscode_history_path, target_vscode, dirs_exist_ok=True)
                print(f"✅ VS Code History zentral aktualisiert.")
            else:
                print(f"VS Code History-Ordner nicht gefunden: {vscode_history_path}")

            # 2. Sichern des Antigravity Brains (ZENTRAL)
            backup_antigravity_brain(central_history_dir)
            
        except Exception as e:
            print(f"⚠️  Fehler beim Aktualisieren der zentralen History: {e}")

    except Exception as e:
        print(f"❌ Fehler beim Erstellen des Backups: {e}")

def backup_mqtt_broker_lib(backup_folder, env_name):
    """
    Kopiert die ESPAsyncMQTTBroker-Bibliothek aus dem libdeps-Ordner
    in den Backup-Ordner auf gleicher Ebene wie src und data.
    """
    src_lib_path = os.path.join(os.getcwd(), ".pio", "libdeps", env_name, "ESPAsyncMQTTBroker")
    dst_lib_path = os.path.join(backup_folder, "ESPAsyncMQTTBroker")
    if os.path.exists(src_lib_path):
        try:
            shutil.copytree(src_lib_path, dst_lib_path, dirs_exist_ok=True)
            print(f"ESPAsyncMQTTBroker nach Backup kopiert: {dst_lib_path}")
        except Exception as e:
            print(f"Fehler beim Kopieren der Broker-Bibliothek: {e}")
    else:
        print(f"Bibliothek nicht gefunden: {src_lib_path}")

def backup_antigravity_brain(backup_folder):
    """
    Sichert die Antigravity-Artefakte (.gemini/antigravity/brain)
    """
    user_home = os.path.expanduser("~")
    # Pfad anpassen, falls nötig. Standard ist ~/.gemini/antigravity/brain
    brain_path = os.path.join(user_home, ".gemini", "antigravity", "brain")
    
    if os.path.exists(brain_path):
        target_path = os.path.join(backup_folder, "Antigravity_History")
        try:
            # dirs_exist_ok=True erlaubt das Überschreiben/Ergänzen
            shutil.copytree(brain_path, target_path, dirs_exist_ok=True)
            print(f"🧠 Antigravity History gesichert: {target_path}")
        except Exception as e:
            print(f"⚠️  Fehler beim Sichern der Antigravity History: {e}")
    else:
        # Nur Info, kein Fehler, falls man auf einem anderen PC ohne Brain ist
        print(f"ℹ️  Antigravity History Pfad nicht gefunden (nicht gesichert): {brain_path}")

# Stelle sicher, dass `env` definiert ist
# Diese Variablen werden von PlatformIO zur Laufzeit bereitgestellt
try:
    Import("env")  # type: ignore  # PlatformIO-spezifische Funktion
except NameError:
    # Fallback für lokale Python-Ausführung (nur für Syntax-Checking)
    env = None  # type: ignore

# Aktionen für SPIFFS, Firmware und Backup festlegen
# Nur ausführen wenn env verfügbar ist (PlatformIO-Kontext)
if env is not None:
    # Event-spezifische Wrapper, damit wir im Log sehen, was ausgelöst hat
    def _after_build_upload(source, target, env):
        return after_build(source, target, env, event_name="upload")

    def _after_build_buildprog(source, target, env):
        return after_build(source, target, env, event_name="buildprog")

    def _after_build_uploadfs(source, target, env):
        return after_build(source, target, env, event_name="uploadfs")

    # Backup NACH jedem Build/Upload erstellen
    # WICHTIG: Wir erstellen SPIFFS NICHT mehr in PreActions, da dies durch die Tasks gesteuert wird (-t buildfs)
    # Das verhindert Endlosschleifen, da buildfs sonst buildprog triggert
    env.AddPostAction("buildprog", _after_build_buildprog)      # Backup nach Firmware-Build erstellen
    env.AddPostAction("upload", _after_build_upload)            # Backup nach Upload erstellen
    env.AddPostAction("uploadfs", _after_build_uploadfs)        # Backup nach Upload des Dateisystems erstellen
