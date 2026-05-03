def read_file_with_fallback(filepath):
    try:
        with open(filepath, "r", encoding="utf-8") as file:
            return file.read()
    except UnicodeDecodeError:
        try:
            with open(filepath, "r", encoding="latin-1") as file:
                return file.read()
        except UnicodeDecodeError:
            with open(filepath, "r", encoding="utf-16") as file:
                return file.read()
import re
import time
from datetime import datetime
import os

# Pfade zur Hauptdatei, HTML-Datei und der Settings-Datei
main_file = "src/main.cpp"  # Passe diesen Pfad an, falls notwendig
version_placeholder = "// @version:"  # Platzhalter für den Kommentar
date_format = "%H:%M:%S %d-%m.%Y"  # Format für Datum und Uhrzeit
variable_pattern = re.compile(r'const\s+char\s*\*\s*firmwareVersion\s*=\s*"(.+?)"\s*;')

# Funktion zum Ermitteln des korrekten Kommentarformats für jeden Dateityp
def get_comment_format(file_extension):
    """Gibt das Kommentarformat basierend auf der Dateierweiterung zurück"""
    comment_formats = {
        # Skriptsprachen
        '.py': '# ',
        '.sh': '# ',
        '.pl': '# ',
        '.rb': '# ',
        
        # Webentwicklung
        '.html': '<!-- {} -->',
        '.htm': '<!-- {} -->',
        '.css': '/* {} */',
        '.js': '// ',
        '.php': '<?php // {} ?>',
        
        # C-Familie
        '.c': '// ',
        '.cpp': '// ',
        '.h': '// ',
        '.hpp': '// ',
        '.ino': '// ',  # Arduino-Dateien
        
        # Konfigurationsdateien
        '.json': '// ',
        '.xml': '<!-- {} -->',
        '.ini': '; ',
        '.conf': '# ',
        '.csv': '# ',
        
        # Dokumentation
        '.md': '<!-- {} -->'
    }
    return comment_formats.get(file_extension.lower(), None)

# Funktion zum Aktualisieren oder Einfügen des Versionskommentars in einer Datei
def update_version_comment(file_path, version_comment):
    file_ext = os.path.splitext(file_path)[1]
    comment_format = get_comment_format(file_ext)
    
    if not comment_format:
        print(f"Unbekanntes Dateiformat für {file_path}, kein Kommentar hinzugefügt.")
        return
    
    try:
        # Datei zum Lesen öffnen
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as file:
            content = file.read()
        
        # Versionstext ohne Platzhalter extrahieren (nur die Version selbst)
        version_text = version_comment.replace(version_placeholder, '').strip()
        
        # Neuen Kommentar im entsprechenden Format erstellen
        if '{}' in comment_format:
            new_comment = comment_format.format(f"@version: {version_text}")
        else:
            new_comment = f"{comment_format}@version: {version_text}"
        
        # Prüfen, ob bereits ein Versionskommentar vorhanden ist
        if '@version:' in content:
            # Vorhandenen Kommentar ersetzen
            lines = content.split('\n')
            update_count = 0
            
            for i, line in enumerate(lines):
                # Standard @version: Einträge suchen
                if '@version:' in line:
                    # Spezieller Fall für HTML-Footer-Eintrag
                    if 'Html @version:' in line:
                        # Nur den Teil nach Html @version: ersetzen, Rest beibehalten
                        prefix = line.split('Html @version:')[0]
                        suffix = ''
                        if '</li>' in line:
                            # Wenn es ein HTML-Element ist, Ende-Tag bewahren
                            suffix = '</li>'
                        lines[i] = f"{prefix}Html @version: {version_text}{suffix}"
                    else:
                        # Standard-Ersetzung für andere @version: Vorkommen
                        lines[i] = new_comment
                    
                    update_count += 1
            
            new_content = '\n'.join(lines)
            print(f"{update_count} Versionskommentar(e) in {os.path.basename(file_path)} aktualisiert.")
        else:
            # Neuen Kommentar am Anfang der Datei einfügen
            new_content = new_comment + "\n" + content
            print(f"Versionskommentar zu {os.path.basename(file_path)} hinzugefügt.")
        
        # Datei zum Schreiben öffnen
        with open(file_path, 'w', encoding='utf-8') as file:
            file.write(new_content)
    
    except Exception as e:
        print(f"Fehler beim Aktualisieren des Kommentars in {os.path.basename(file_path)}: {e}")

# Sperrzeit in Sekunden (hier 40 Sekunden)
lock_duration = 120
# Pfad zur Datei, die den letzten Ausführungszeitpunkt speichert
lock_file = ".version_lock"

# Funktion, um den Sperrstatus zu prüfen und zu aktualisieren
def is_locked():
    try:
        with open(lock_file, "r", encoding="utf-8") as f:
            last_run = float(f.read())
            if time.time() - last_run < lock_duration:
                print("Skript kürzlich ausgeführt, überspringe Versionsaktualisierung.")
                return True
    except FileNotFoundError:
        pass  # Datei existiert noch nicht
    return False

def set_lock():
    with open(lock_file, "w", encoding="utf-8") as f:
        f.write(str(time.time()))

# Hauptlogik für Versionsaktualisierung
def update_version():
    # Hauptdatei lesen (robust mit Fallback-Encoding)
    content = read_file_with_fallback(main_file)

    # Versionsnummer aus Kommentar extrahieren
    version_match = re.search(rf'^[ \t]*{version_placeholder} (\d+)\.(\d+)\.(\d+)', content, re.MULTILINE)
    if not version_match:
        raise ValueError("Kein gültiger Versionskommentar gefunden!")

    # Aktuelle Versionsnummer auslesen
    major, minor, patch = map(int, version_match.groups())

    # Versionsnummer erhöhen (z.B. Patch-Level)
    patch += 1

    # Neue Version
    new_version = f"{major}.{minor}.{patch}"

    # Aktuelles Datum und Uhrzeit im gewünschten Format erhalten
    current_datetime = datetime.now().strftime(date_format)

    # Neue Version mit Build-Datum
    version_with_date = f"{new_version} <br> Builddatum {current_datetime}"

    # Versionsnummer und Build-Datum im Kommentar aktualisieren
    new_content = re.sub(rf'{version_placeholder} \d+\.\d+\.\d+.*', 
                         f'{version_placeholder} {version_with_date}', 
                         content)

    # Versionsnummer und Build-Datum in der Variable aktualisieren
    def update_variable_version(match):
        return f'const char* firmwareVersion = "{version_with_date}";'

    new_content = variable_pattern.sub(update_variable_version, new_content)

    # Hauptdatei überschreiben
    with open(main_file, "w", encoding="utf-8") as file:
        file.write(new_content)

    print(f"Version auf {new_version} aktualisiert und Build-Datum auf {current_datetime} gesetzt in {main_file}.")
    print("Info: Versionsaktualisierung erfolgt nur noch in main.cpp. Alle anderen Dateien bleiben unverändert.")



# Skript nur ausführen, wenn nicht gesperrt
if not is_locked():
    update_version()
    set_lock()

# Hook für das Post-Build- und Post-Upload-Ereignis in VSCode integrieren
if "PLATFORMIO_BUILD_DIR" in os.environ:
    update_version()
