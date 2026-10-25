# KR Mycelia Memory – Funktions- und Nachweismatrix

Stand: 25. Oktober 2026. `IMPLEMENTIERT` bedeutet: im nativen Produktpfad
vorhanden und durch Build, Test oder installierten Ablauf nachgewiesen. Es ist
keine Behauptung vollständiger Gleichheit mit fremden Enterprise-Produkten.

| Bereich | Status | Nachweis / Grenze |
|---|---|---|
| DAD, Attraktoren, assoziative Suche | IMPLEMENTIERT | Native C++20-Engine, SQL-Records, Stabilität/Besuche, Snapshot- und Query-Regressionstests. |
| Dauerhafte Speicherung | IMPLEMENTIERT | DPAPI, atomarer Austausch, zwei Backups, Prozess-Mutex und Recovery; 1.000 Transaktionen im Test. |
| Zentraler Dienst | IMPLEMENTIERT | GUI-, Import-, Collection-, Relation-, Agenten- und MCP-Schreibvorgänge laufen über `KR_Mycelia_Service.exe`. |
| RPC-Sicherheit | IMPLEMENTIERT | Owner-ACL, SID-Prüfung, DPAPI-geschützte lokale Capability, MCP-Token/Scope/Ablauf, strukturierte Antwortprüfung, Größenlimits und Timeouts. |
| Prozesshärtung | IMPLEMENTIERT | Sichere DLL-Suchpfade, deaktivierte Legacy-Erweiterungspunkte sowie Blockade entfernter und Low-Integrity-Images; die aktiven Windows-Mitigation-Policies werden getestet. |
| Dashboard und Verwaltung | IMPLEMENTIERT | Responsive native Seitenleiste, Windows-11-DWM-Design, Hell/Dunkel, Akzentfarbe, DPI-Skalierung sowie echte Status-, Timeline-, Collection- und Agentendaten. |
| Memory Explorer | IMPLEMENTIERT | Asynchrone Suche, Quellen-/Collection-Filter, Detailtext, DAD-Stabilität, Import, Export, Relationseinblick und Löschen. Zeitraumfilter ist serverseitig verfügbar; die kompakte UI stellt ihn noch nicht als Kalender dar. |
| Collections und Beziehungen | IMPLEMENTIERT | Erstellen, Umbenennen, Archivieren, Zusammenführen, Mitgliedschaften und sieben belegte Relationstypen im Dienst; die GUI deckt die häufigsten Verwaltungsaktionen ab. |
| Agentenverwaltung | IMPLEMENTIERT | Auflisten, Scopes/Ablauf anzeigen, Token einmalig erzeugen/rotieren, widerrufen und Auditdaten lesen. |
| Memory Chat | IMPLEMENTIERT | Echtes SSE-Streaming über WinHTTP, UTF-8-/Paketfragmentierung, sofortige Anzeige, tatsächliches Schließen des laufenden HTTP-Handles, Neustart nach Abbruch, DAD-Kontext und Quellsignaturen. Kein Cloud-Fallback. |
| Text-/Office-Import | IMPLEMENTIERT | Textformate und Office Open XML (`DOCX`, `XLSX`, `PPTX`) mit gebündeltem zlib-Decoder. |
| PDF-Import | IMPLEMENTIERT | Native Seitendarstellung mit `Windows.Data.Pdf`, WIC-Oberflächen und reale `Windows.Media.Ocr`-Erkennung; Test-PDF wird tatsächlich erkannt. |
| Bild-OCR | IMPLEMENTIERT | WIC-Dekodierung, Orientierung/Skalierung, Windows-OCR, erkannter Text und Quellenbezug in der DAD; realer Bildtest. |
| Audio | IMPLEMENTIERT | Media-Foundation-Dekodierung, 16-kHz-Mono-PCM, Segmentierung, RMS, Zeitstempel, Abbruch und echte Offline-Dateitranskription über installierte Windows-SAPI-Desktoprecognizer. Transkriptsegmente sind mit Quelle, Collection und `derived_from`-Relation in der DAD gespeichert und wurden im installierten Produkt gesucht. |
| Video | IMPLEMENTIERT OHNE OPTIONALES VISION-MODELL | Media Foundation, Zeitstempel, Frame-Dekodierung, Szenenwechsel, pHash-Deduplizierung, Frame-OCR sowie Offline-Transkription einer vorhandenen Audiospur und DAD-Relationen. Semantische Bildbeschreibungen nur mit real bereitgestelltem lokalem Vision-Modell. |
| One-Click-Verbindungen | IMPLEMENTIERT | LM-Studio-Erkennung, Modellliste, gespeicherte Modellwahl und Testfrage sowie transaktionale Cursor-, Claude- und Codex-Konfiguration mit Backup, DPAPI-Credential, Scope-Freigabe, Widerruf und Rollback. |
| Kontext-/Ordnererfassung | IMPLEMENTIERT | Nur Opt-in-Fenstertitel erlaubter Programme und freigegebene Ordner; Standard AUS, Passwort-/Credential-Pfade blockiert, keine Tasten/Screenshots. |
| Tray und Autostart | IMPLEMENTIERT | Schließen minimiert in den Tray; Autostart ist opt-in und unabhängig von Erfassung. GUI überwacht und startet den Dienst erneut. |
| Installation/Update | IMPLEMENTIERT | Pflichtprüfung aller vier EXE-Dateien, Prozessbeendigung, Hashprüfung, Rollback-Kopie, DPAPI-Selbsttest, Manifest und Startmenü. Deinstallation erhält Benutzerdaten. |
| MCP | IMPLEMENTIERT | Lokaler stdio-Server, Scope-abhängige Toolliste, Dienst-Reautorisierung von Mutationen, Audit sowie strukturierte Dienstantworten. |
| GPU/MyceliaWorld/MYCL | IMPLEMENTIERT IM BESTEHENDEN UMFANG | Vorhandene CPU/OpenCL-, Checkpoint-, Resume- und Bundle-Tests bleiben erhalten. Vollständige Hardwareparität auf jeder GPU wird nicht behauptet. |

## Bewusste Grenzen

- Ein kompromittierter Prozess unter derselben Windows-Benutzeridentität ist
  keine unabhängige Sicherheitsdomäne. Die Capability verhindert versehentliche
  und nicht autorisierte Protokollnutzung, ersetzt aber keine OS-Kontentrennung.
- Offline-Speech-to-Text setzt einen installierten Desktop-SAPI-Recognizer für
  die gewünschte Sprache voraus. Auf dem Abnahmesystem wurde der deutsche
  Recognizer real geprüft. Fehlt er auf einem Zielsystem, meldet die Anwendung
  dies und speichert ausschließlich die weiterhin nutzbare Vorverarbeitung.
- Semantische Bildbeschreibungen benötigen ein tatsächlich vorhandenes lokales
  Vision-Modell. Die davon unabhängige Videoanalyse bleibt vollständig nutzbar.
