# Changelog

Alle wesentlichen Produktänderungen werden in diesem Dokument festgehalten.
Das Format orientiert sich an [Keep a Changelog](https://keepachangelog.com/de/1.1.0/)
und die Versionierung an [Semantic Versioning](https://semver.org/lang/de/).

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
