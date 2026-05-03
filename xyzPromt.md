❤️ 📂 🎉DNS auflösung alles geht. super 📂 ❤️



# WUNSCHTEXT: Die erste Zeile ist der exakt einzusetzende Text. NICHT unten erneut schreiben.
# Bitte diesen Workspace-Edit JETZT durchführen, nur in /src und /data.

ZIEL
- In jeder Datei unter /src und /data:
  1) Wenn am Dateianfang eine "Beschreibung:"-Zeile (oder auch "Beschreibeung:") existiert, ersetze NUR den Teil nach dem Doppelpunkt durch den WUNSCHTEXT (aus der allerersten Zeile dieses Prompts).
  2) Wenn keine Beschreibung vorhanden ist, füge ganz oben eine passende Kommentarzeile mit "Beschreibung: <WUNSCHTEXT>" ein.
- Die Chatbearbeitung soll so ausgeführt werden, dass pro Datei ein Timeline-Eintrag entsteht.

WICHTIG
- @version- oder AdVersion-Zeilen NICHT ändern, nicht löschen, nicht umsortieren.
- Shebangs (#!) stehen lassen; in solchen Dateien die Beschreibung direkt NACH der Shebang-Zeile einfügen/ersetzen.
- JSON-Dateien NICHT verändern.
- Keine sonstigen Formatierungen, Sortierungen oder Refactorings.
- Idempotenz: Wenn bereits exakt "Beschreibung: <WUNSCHTEXT>" vorhanden ist, nichts ändern. Falls eine minimale Änderung nötig ist, nur überflüssige Spaces nach dem Doppelpunkt normalisieren, um einen legitimen Edit zu haben.

DATEITYP-ABDECKUNG (Kommentar-Syntax automatisch wählen)
- C/C++/C#/Java/Go/Rust/JS/TS/JSX/TSX/INO:     // Beschreibung: <WUNSCHTEXT>
- Python/Shell/PS1/YAML/YML/TOML:              #  Beschreibung: <WUNSCHTEXT>
- INI:                                         ;  Beschreibung: <WUNSCHTEXT>
- CSS/SCSS:                                    /* Beschreibung: <WUNSCHTEXT> */
- HTML:                                        <!-- Beschreibung: <WUNSCHTEXT> -->

SCOPE / FILTER
- Ordner: /src und /data (rekursiv)
- Übliche Binär-/Build-/Tool-Ordner ignorieren: .git, .vscode, .pio, node_modules, build, dist, out, .next, .cache, .history
- JSON explizit ausschließen.

AUSGABE/WORKFLOW
1) Erzeuge eine konsolidierte Vorschau (Workspace Edit) aller geplanten Änderungen.
2) Biete mir direkt „Alle Änderungen übernehmen“ an (ohne Rückfrage wie „ja?“).
3) Verwende intern überall den WUNSCHTEXT aus der ERSTEN ZEILE dieses Prompts. Nicht duplizieren, nicht anders schreiben.
4) liste mir nicht den Verweis verwendet in auf das ich weiss welche dateien editiert wurden.
