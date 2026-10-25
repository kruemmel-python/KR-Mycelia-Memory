# KR Mycelia Memory – Windows Edition 0.1

## Wirklich implementiert

- Win32 GUI (`KR_Mycelia_Memory.exe`) ohne Python, Qt oder Web-Framework.
- Originaler v0.18 C++20-DAD-Kern unverändert integriert.
- Manuelle Erinnerungen als DAD-SQL-Attraktoren und assoziative Suche.
- Fenstertitel opt-in: nur EXE-Namen der Allowlist, keine Screenshots, kein Tastatur- oder Dokumentenlesen.
- Win32 Foreground-Fensterprüfung alle 2 Sekunden **nur während GUI läuft**; kein Systemdienst.
- Keine Erfassung beim Start, auch wenn zuvor aktiviert.
- Verschlüsselte DAD-Persistenz via vorhandener Windows-DPAPI-Snapshot-Engine.
- User-Level Startmenü-Installer und Uninstaller, kein Administrator nötig.
- Linux-Test der Richtlinien; Windows-Compiler und GUI-Laufzeittest müssen auf dem Zielsystem geprüft werden.

## Nicht implementiert

- Vollständige MyceliaDB-Enterprise-Parität (Backend, Web-Funktionen, alle Sicherheitsmodule).
- MCP-Server/Remote-Zugriff, LM Studio, Tray, Autostart oder Windows-Dienst.
- Vollständiger Kontextinhalt aus Dokumenten (nur erlaubte Fenstertitel und manuelle Eingaben).
- Gemeinsame GPU/DAD-Live-Transaktion; bestehende GPU-Tests bleiben enthalten.

## Bauen und installieren

Projektordner in KR Compiler IDE v4.3.1 öffnen, Windows x64 CMake konfigurieren und bauen.
Danach im Projektordner `powershell -NoProfile -ExecutionPolicy Bypass -File .\install.ps1` ausführen.
Über Startmenü `KR Mycelia Memory Native` starten. `notepad.exe` steht standardmäßig auf der Allowlist, Erfassung bleibt aus, bis sie aktiv eingeschaltet wird.

Daten: `%LOCALAPPDATA%\KRMyceliaNativeMemory\memory.dpapi`. Einstellungen: `settings.txt` (nur Programmnamen).

## Sicherheits-/Funktionsgrenzen

- Der Fenstertitel kann personenbezogene Information enthalten. Aufzeichnung erfolgt nur mit ausdrücklicher Freigabe; sensible Schlüsselwörter werden best-effort ausgeschlossen.
- Wird die Snapshot-Datei beschädigt oder unzugänglich, bricht der Programmstart mit Fehlermeldung ab. Keine stille Neuanlage.
- Beim aktuellen Stand ist die sichere DAD-Datei auf den Windows-Nutzer/DPAPI-Kontext gebunden.
- Bisheriger MyceliaDB-Code besitzt eine Begrenzung für gesamte Snapshot-Größe und Retention; produktive Last- und Crash-Tests stehen aus.
