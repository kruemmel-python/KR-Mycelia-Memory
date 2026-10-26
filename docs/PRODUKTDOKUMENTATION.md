# KR Mycelia Memory – Windows 11 Premium Edition

## Produktbeschreibung

KR Mycelia Memory ist ein dauerhaftes, lokal betriebenes KI-Gedächtnis für
Windows 11 x64. Inhalte, Quellen, Collections, zeitliche Ereignisse und
Beziehungen werden in der vorhandenen Dynamic Associative Database (DAD) von
MyceliaDB gespeichert. Die Anwendung benötigt keinen Cloud-Dienst und eröffnet
keinen eingehenden Netzwerkport. LM Studio ist ein optionaler Loopback-Endpunkt
für lokale Sprachmodelle.

Die Edition besteht aus vier nativen C++20-Programmen:

| Programm | Aufgabe |
|---|---|
| `KR_Mycelia_Memory.exe` | Windows-11-Oberfläche, Dashboard, Tray, Explorer, Chat, Datenschutz und Verbindungen |
| `KR_Mycelia_Service.exe` | Benutzerprozess im Hintergrund, zentraler DAD-Schreiber, Ordnerbeobachtung und RPC |
| `KR_Mycelia_MCP.exe` | Lokaler MCP-Server über stdin/stdout mit Agentenrechten |
| `KR_Mycelia_Import.exe` | Administrativer Einzeldatei-Import über den Dienst |

## Installation und erster Start

`install.ps1` installiert die vier Programme, `zlib1.dll` und die erforderlichen
GCC-Laufzeiten `libgcc_s_seh-1.dll` sowie `libwinpthread-1.dll` nach
`%LOCALAPPDATA%\Programs\KRMyceliaMemory`. Vorhandene Binärdateien werden vor
dem Austausch gesichert, alle Kopien per SHA-256 geprüft und bei Fehlern
zurückgerollt. Danach laufen der DAD-Persistenz-Selbsttest und ein vollständiger
Dienst-/DAD-Neustarttest mit der tatsächlich installierten GUI. Das
Installationsmanifest kennzeichnet Version 3 und die
Windows 11 Premium Edition.

Beim ersten Start sind Autostart und Kontexterfassung ausgeschaltet. Das
Schließen des Hauptfensters beendet das Gedächtnis nicht, sondern lässt den
Tray-Prozess und den davon unabhängigen Hintergrunddienst weiterlaufen. Nur
**Programm vollständig beenden** beendet die Oberfläche. Persönliche Daten
werden bei Update und Deinstallation nicht gelöscht.

## Benutzeroberfläche

Die native Oberfläche verwendet DWM-Hintergründe, abgerundete Windows-11-
Fenster, Systemakzent, Segoe UI Variable, automatische Hell-/Dunkelpalette und
Per-Monitor-DPI-Skalierung. Das Hauptlayout reagiert auf Größenänderungen und
bietet folgende Bereiche:

Das schlanke Darstellungsmenü bietet sechs fertige Themes – Windows-System,
Klar Hell, Elegant Dunkel, Ozean, Wald und Aubergine. Nutzer wählen lediglich
Bereich, Schriftgröße und eine kontrastgeprüfte Farbvorgabe. Einstellungen für
Dashboard, Explorer, Chat, Verwaltung und KI-Verbindungen bleiben getrennt,
werden sofort auf offene Fenster angewendet und in
`%LOCALAPPDATA%\KRMyceliaNativeMemory\ui-settings.txt` gespeichert.

- **Übersicht:** Dienst- und Erfassungsstatus, neue Erinnerung, assoziative
  Suche, Treffer, Verknüpfungen, Aktivität, Agenten und Suchhistorie.
- **Memory Explorer:** Quellen- und Collection-Filter, Detailansicht, Import,
  Export, Beziehungen und bestätigtes Löschen.
- **Collections / Timeline / Relationen / Agenten:** reale Verwaltungsdaten
  aus dem Dienst, keine Demo-Datensätze.
- **Memory Chat:** Modellwahl, echtes Streaming, HTTP-Abbruch und Quellen.
- **KI-Verbindungen:** LM-Studio-Test sowie One-Click-MCP-Verbindungen.
- **Import:** öffnet den Explorer mit nativer Dateiauswahl.
- **Datenschutz:** Opt-in-Erfassung, Autostart, Programmliste und native
  Ordnerauswahl.
- **Einstellungen:** Design, Schriftgröße und Bereichsfarbe ohne komplexen
  Theme-Editor.

Das Tray-Menü schaltet Erfassung, öffnet Gedächtnis, Chat, Aktivität,
Verbindungen und Einstellungen, verbindet den Dienst neu oder beendet das
Programm vollständig. Autostart und Erfassungsfreigabe bleiben getrennte
Entscheidungen.

## Speicher- und Wiederherstellungsmodell

Der Dienst ist der zentrale aktive Schreiber. Jede Änderung läuft als
serialisierte Transaktion durch `MemoryStore`, geschützt durch einen
benutzersitzungsweiten Mutex. Der Snapshot unter
`%LOCALAPPDATA%\KRMyceliaNativeMemory\memory.dpapi` ist mit Windows DPAPI für
den aktuellen Benutzer verschlüsselt. Eine neue Version wird zunächst
vollständig geschrieben und anschließend atomar ersetzt. Zwei rotierende
Sicherungen (`.bak1`, `.bak2`) ermöglichen Recovery bei Beschädigung.

Der nachgewiesene End-to-End-Fluss lautet:

```text
Datei/Medium
  → native Windows-Dekodierung und Analyse
  → Original + abgeleitete Inhalte
  → DAD + Collection + Relation + Quelle
  → lokaler Dienst
  → Explorer / MCP / Memory Chat
  → DPAPI-Snapshot und Backups
  → Prozessneustart
  → assoziative Wiederauffindung
```

## Dokument- und Medienverarbeitung

| Typ | Lokale Verarbeitung |
|---|---|
| Text, Markdown, Code, JSON, CSV, XML, HTML, RTF, Log | Direkter, größenbegrenzter Import |
| DOCX, XLSX, PPTX | Relevante Office-Open-XML-Einträge über gebündeltes zlib |
| PDF | Seitendarstellung mit `Windows.Data.Pdf`, OCR mit `Windows.Media.Ocr` |
| PNG, JPEG, BMP, TIFF, WEBP | WIC-Dekodierung, Skalierung und Windows OCR; WEBP nach installiertem WIC-Codec |
| WAV, MP3, M4A, WMA | Media Foundation, 16-kHz-Mono-PCM, Zeitsegmente, Pegel und Offline-Transkription über installierte Windows-SAPI-Recognizer |
| MP4, WEBM, WMV | Media Foundation, Zeitstempel, Frames, Szenenwechsel, pHash, OCR und Offline-Transkription einer vorhandenen Audiospur |

Abgeleitete Audiosegmente und Videoframes besitzen die Signatur der
Originalquelle, Zeitangaben, Collection-Mitgliedschaft und eine
`derived_from`-Relation. Dadurch bleiben Suche und Chat nachvollziehbar.

Windows-Sprachressourcen und API-Verfügbarkeit werden zur Laufzeit geprüft.
Die Anwendung verwendet einen installierten Desktop-SAPI-Recognizer als
native Offline-Datei-Engine und speist ihm das von Media Foundation dekodierte
PCM über einen COM-Stream zu. Text, Engine, Sprache, Status und Zeitpositionen
werden gespeichert. Der reale Test auf dem Abnahmesystem erkannte deutsche,
von einer Windows-SAPI-Stimme erzeugte Sprache und fand das Transkript nach
Import über den installierten Dienst in der DAD wieder. Fehlt ein passender
Recognizer, bleibt die Vorverarbeitung nutzbar und der Status wird ohne
erfundenen Text gespeichert. Eine semantische Bildbeschreibung wird ebenso
nur gespeichert, wenn ein reales lokales Vision-Modell ausgeführt wurde.

## Memory Chat und LM Studio

Die Anwendung erkennt den lokalen OpenAI-kompatiblen Endpunkt
`http://127.0.0.1:1234/v1`, liest dessen Modelle, speichert die gewählte
Modell-ID und führt eine echte Testabfrage aus. Chatfragen erhalten begrenzten, quellengebundenen DAD- und
Relationskontext. Importierter Text wird ausdrücklich als nicht
vertrauenswürdiges Referenzmaterial markiert.

Antworten treffen als Server-Sent Events ein. Parser und UTF-8-Decoder
verarbeiten beliebige Paketgrenzen, mehrere Events pro Puffer und fragmentierte
Codepoints. Text erscheint fortlaufend und in richtiger Reihenfolge. Beim
Abbrechen wird das aktive WinHTTP-Request-Handle geschlossen; verspätete
Callbacks dürfen die nächste Anfrage nicht verändern. Erfolg, Benutzerabbruch,
Timeout, Transportfehler und Modellfehler sind getrennte Zustände. Nur eine
vollständige Antwort wird mit ihren DAD-Quellsignaturen gespeichert.

## One-Click-Verbindungen und Rechte

Der Manager erkennt Cursor, Claude Desktop und Codex anhand ihrer tatsächlichen
Benutzerkonfigurationen. Cursor und Claude werden über `mcpServers` in JSON,
Codex über `[mcp_servers.kr_mycelia_memory]` in `~/.codex/config.toml`
eingebunden. Fremde Einträge werden strukturell erhalten.

Vor dem Verbinden zeigt die GUI alle beantragten Scopes. `read` ist
vorausgewählt; `write`, `import`, `delete`, `export` und `ai` benötigen eine
aktive Auswahl und Bestätigung. GUI-Verbindungen laufen nach 30 Tagen ab. Der
Dienst erzeugt 256-Bit-Zufallstokens, speichert nur deren SHA-256-Werte und
prüft sie bei jedem privilegierten Aufruf erneut.

Das Klartext-Token steht nicht in der Clientkonfiguration. Es wird mit DPAPI in
`%LOCALAPPDATA%\KRMyceliaNativeMemory\credentials` geschützt. Die Konfiguration
enthält nur den Agentennamen und `--credential <Pfad>`. **Trennen** widerruft
zuerst den Token und entfernt dann ausschließlich den Mycelia-Eintrag. Backup,
atomischer Austausch und Rollback schützen bestehende Clientkonfigurationen.

## Sicherheitsgrenzen

- Named Pipe nur für den Eigentümer, zusätzliche Client-SID-Prüfung.
- DPAPI-Capability für lokale administrative RPC-Aufrufe.
- MCP-Token-Hash, Scope, Aktivstatus, Ablauf und Audit im Dienst.
- Größen-, Zeit-, Pfad- und Ressourcengrenzen für Import und Medien.
- Sichere DLL-Suchpfade; keine aktuelle Arbeitsmappe als DLL-Quelle.
- Deaktivierte Legacy-Erweiterungspunkte sowie blockierte Remote- und
  Low-Integrity-Images im Prozess.
- Keine automatische Mikrofonaufnahme, kein Cloud-Fallback und keine
  unbeaufsichtigten Modelldownloads.

DPAPI, ACL und Token trennen Anwendungen auf Protokollebene. Sie bilden keine
vollständige Isolation gegenüber einem bereits kompromittierten Prozess mit
derselben Windows-Benutzeridentität. Eine solche AppContainer-Grenze wird
nicht behauptet.

## Abnahme und Supportdiagnose

Der Release-Stand besteht alle 49 registrierten Tests. Die Suite umfasst die
DAD-Engine, DPAPI/Recovery, GPU-/MYCL-Bestand, RPC und MCP, Streaming/Abbruch,
reale OCR in vier Bildformaten, PDF-OCR, Audio, Video, DAD-Medienrelationen,
Verbindungs-Rollback, echte Offline-SAPI-Dateitranskription, Importabbruch,
Theme/DPI und Prozessmitigierungen.

Zusätzlich wurden die installierten Binärdateien geprüft: Persistenztest,
Dienststart/-neustart, DAD-Wiederherstellung, reale Audio-Transkription mit
Suche, Importabbruch ohne Teilpersistenz, DPAPI-Credential, MCP-Handshake und
Toolliste, Premium-Navigation, Datenschutzseite, Verbindungsfenster,
responsives Resize, Tray-Verhalten sowie Deinstallation bei erhaltenem
Benutzerdatenziel.

Das vollständige, auch für neue Benutzer geschriebene Bedienhandbuch jeder
GUI-Eingabe und Einstellung steht in [GUI_HANDBUCH.md](GUI_HANDBUCH.md).
Weitere technische Details stehen in [ARCHITECTURE.md](ARCHITECTURE.md),
[MCP_INTEGRATION.md](MCP_INTEGRATION.md), [TEST_REPORT.md](TEST_REPORT.md) und
[FEATURE_PARITY_MATRIX.md](FEATURE_PARITY_MATRIX.md). Die bewusst getrennte
Portierungsmatrix der historischen Enterprise-Plattform steht in
[ENTERPRISE_PORT_STATUS.md](ENTERPRISE_PORT_STATUS.md).
