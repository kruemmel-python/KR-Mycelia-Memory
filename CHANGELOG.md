# Changelog

Alle wesentlichen Produktänderungen werden in diesem Dokument festgehalten.
Das Format orientiert sich an [Keep a Changelog](https://keepachangelog.com/de/1.1.0/)
und die Versionierung an [Semantic Versioning](https://semver.org/lang/de/).

## [1.0.3] – 2026-10-26

### Behoben

- Der rekursive Ordnerabgleich hält nicht mehr während des gesamten Scans eine
  einzige DAD-Transaktion. Jede Textdatei wird in einer kurzen, begrenzt
  wartenden Transaktion verarbeitet; GUI, Memory Chat und RPC bleiben während
  großer Scans erreichbar.
- Die automatische Ordnerüberwachung startet keine OCR-, Office-, PDF-, Audio-
  oder Videoengine mehr. Ein im Praxisfall nachgewiesener nativer OCR-Absturz
  durch eine Binärdatei im überwachten Projekt kann den Dienst daher nicht
  mehr beenden.
- Collector-Wechsel und Beenden sind auch während eines laufenden
  Bestandsabgleichs abbrechbar und warten kontrolliert auf Watcher und Worker.
- Nicht automatisch unterstützte Binärdateien werden beim Abgleich nicht
  fälschlich als gelöschte Textquelle behandelt. Ihr bewusster Einzelimport
  bleibt erhalten.

### Sicherheit und Betrieb

- Rekursive Erfassung ist auf deterministische Text-, Quellcode-, Skript- und
  Konfigurationsformate beschränkt. Office, PDF, Bilder, Audio und Video stehen
  weiterhin über den ausdrücklich ausgelösten Dateiimport zur Verfügung.
- Neuer Windows-Regressionstest prüft gleichzeitigen DAD-Zugriff während des
  Scans, sicheren Collector-Neustart und eine absichtlich ungültige PNG-Datei.

## [1.0.2] – 2026-10-26

### Behoben

- Überwachte Dateien werden über ihren normalisierten absoluten Quellpfad
  aktualisiert, statt bei jeder Änderung einen weiteren konkurrierenden
  Volltextdatensatz anzulegen.
- Ein Hintergrundabgleich übernimmt beim Start der Erfassung den aktuellen
  Ordnerbestand, entfernt Datensätze gelöschter Quelldateien und bereinigt
  historische Duplikate derselben Quelle.
- Datei, überwachter Stammordner und erkanntes Projekt werden getrennt
  gespeichert. Containerordner wie `H:\mc_memory_test` ordnen
  `MDraft\README.md` dadurch ausdrücklich dem Projekt `MDraft` zu.
- Memory Chat und assoziative Suche gewichten Projekt, Dateiname und
  Quellpfad stärker als beiläufige Erwähnungen im Inhalt. Verwaltungs-, Such-
  und Chatverlaufsrecords verdrängen weiterhin keine Originalquellen.
- Bei langen Dateien wird der tatsächlich zur Frage passende Textausschnitt
  an das lokale Modell übergeben, auch wenn er erst weit hinter dem
  Dateianfang steht.
- Der fortgeschriebene Quelldatensatz hält die zuletzt hinzugefügte und
  entfernte Textregion sowie den vorherigen Inhaltsprüfwert fest. Fragen nach
  der letzten Dateiänderung sind damit aus der DAD belegbar.
- Der Ordnerabgleich läuft abbrechbar im Hintergrund; die laufende
  Dateiüberwachung ist sofort verfügbar und das Beenden der Erfassung wartet
  nicht auf einen vollständigen Großordnerscan.

### Erweitert

- Zusätzliche Quellcode-, Skript- und Konfigurationsformate für C/C++, C#,
  Java, Kotlin, Go, Rust, Swift, Python, JavaScript/TypeScript, CSS, SQL,
  Shell, PowerShell, Batch, CMake, TOML, YAML und INI-artige Dateien.

## [1.0.1] – 2026-10-25

### Lizenz

- Das Projekt wird unter der PolyForm Noncommercial License 1.0.0
  bereitgestellt.
- Installationspakete enthalten den vollständigen Lizenztext; der Installer
  übernimmt ihn zusätzlich in das lokale Programmverzeichnis.

## [1.0.0] – 2026-10-25

### Hinzugefügt

- Native Windows-11-Oberfläche mit Dashboard, Memory Explorer, Collections,
  Timeline, Relationen, Agentenverwaltung, Datenschutz und Tray.
- Eigenständiger Benutzer-Hintergrunddienst als zentraler DAD-Schreiber.
- DPAPI-geschützte DAD-Persistenz mit atomaren Transaktionen und zwei
  rotierenden Wiederherstellungskopien.
- Lokaler MCP-stdio-Server mit getrennten Scopes, Ablauf, Rotation, Widerruf
  und Audit.
- One-Click-Verbindungen für Codex, Cursor und Claude Desktop mit
  DPAPI-Credentials und transaktionalem Konfigurations-Rollback.
- Lokaler LM-Studio-Memory-Chat mit DAD-RAG, Quellsignaturen, echtem
  SSE-Streaming und HTTP-Abbruch.
- Native Dokument-, Office-, PDF-, Bild-, Audio- und Videoverarbeitung.
- Freigegebene Programm- und Ordnererfassung mit Opt-in-Datenschutzregeln.
- Sechs globale Designs sowie Schriftgröße und kontrastgeprüfte Schriftfarbe
  je Funktionsbereich.
- Vollständiges GUI-Handbuch, Produkt-, Betriebs-, Architektur-, MCP- und
  Testdokumentation.

### Behoben

- Exakte Kennwort-, Code- und Texttreffer werden vor assoziativen Ergänzungen
  priorisiert.
- Suchhistorie, Chatverlauf, Audit- und Berechtigungsrecords können echte
  Erinnerungen nicht mehr aus dem Chatkontext verdrängen.
- Relevante Record-Felder werden vollständig und mit begrenztem Kontextbudget
  an den lokalen RAG-Chat übergeben.
- Manuelle Erinnerungen werden nicht mehr still nach 240 Bytes abgeschnitten;
  die dokumentierte Grenze beträgt 3.072 UTF-8-Bytes.
- Streaming-Limit für lokale Reasoning-Modelle auf einen kontrollierten
  4-MiB-Datenstrom und fünf Minuten Laufzeit erweitert.
- Theme-, DPI- und Auswahl-Neuzeichnung über alle nativen Fenster vereinheitlicht.

### Sicherheit

- Owner-beschränkte Named Pipe mit zusätzlicher SID-Prüfung.
- DPAPI-geschützte lokale RPC-Capability und MCP-Credential-Dateien.
- Dienstseitige erneute Prüfung von Token-Hash, Scope, Status und Ablauf.
- Sichere DLL-Suchpfade, Windows-Prozessmitigierungen sowie Größen-, Zeit- und
  Pfadgrenzen für Parser und Medien.

[1.0.0]: https://github.com/kruemmel-python/KR-Mycelia-Memory/releases/tag/v1.0.0
[1.0.1]: https://github.com/kruemmel-python/KR-Mycelia-Memory/releases/tag/v1.0.1
[1.0.2]: https://github.com/kruemmel-python/KR-Mycelia-Memory/releases/tag/v1.0.2
[1.0.3]: https://github.com/kruemmel-python/KR-Mycelia-Memory/releases/tag/v1.0.3
