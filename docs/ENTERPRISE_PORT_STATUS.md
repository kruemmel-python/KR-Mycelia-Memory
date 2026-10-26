# MyceliaDB Enterprise – abgegrenzter Portierungsnachweis

Stand: 25. Oktober 2026

## Abgrenzung

Die Windows-11-Premiumanwendung ist ein produktiver nativer Client auf Basis
der portierten MyceliaDB-Kernfunktionen. Sie ist nicht identisch mit der
historischen webbasierten Enterprise-Gesamtplattform. Der Masterauftrag führt
die vollständige Enterprise-Portierung ausdrücklich als eigenständiges,
separat nachzuweisendes Ziel. Dieses Dokument verhindert, dass die
Fertigstellung der Windows-Anwendung fälschlich als vollständige Portierung
aller historischen PHP-, Python-, JavaScript-, WebAuthn-, Shop-, Forum- oder
VRAM-Zertifizierungsfunktionen ausgegeben wird.

Referenz des auditierten Originalbestands:

- Archiv: `CMS-MyceliaDB-Enterprise-main.zip`
- SHA-256: `06e396a26215651561c51a33383bbe3d0aa21b6ff2c71e14ad654ed1baf3cdfb`
- Auditquelle: `audit/original_manifest.json` und `audit/PORT_MATRIX.md` im
  vorhandenen Projektarchiv `KR-Mycelia-Memory.zip`
- Erfasster Bestand: 37 Dateien aus KI/Simulation/Kognition/Datenbank,
  29 Enterprise-Webdateien, 14 native C/OpenCL-/Daten-/Dokumentdateien,
  76 ursprüngliche Regressionstests, 33 Gateway-/Legacy-Dateien und
  12 Sicherheits-/Wartungswerkzeuge

## Nativ portierter und aktuell getesteter Kern

| Originalbereich | Nativer Produktpfad | Nachweis |
|---|---|---|
| Dynamic Associative Database | `mycelia.hpp/.cpp` | Pattern-Lifecycle, Signaturen, Attraktoren, Visits/Stabilität, assoziative Abfragen und SQL-Records in mehreren Regressionstests |
| Observer Network | `mycelia.hpp/.cpp` | Goldenvektoren und Observer-Test |
| SQL-Import und Attraktoren | `sql_dump.*`, `sql_attractors.cpp` | Parser-, Typ- und Attraktorregressionen |
| Snapshots | `snapshot.cpp`, `secure_snapshot.cpp`, `memory_store.cpp` | Binärsnapshot, DPAPI, atomarer Austausch, zwei Backups und Recovery |
| MyceliaWorld | `mycelia_world.*`, `world_checkpoint.*` | CPU-Welt, kombinierter Welt-/DAD-Checkpoint und Wiederherstellung |
| CC_OpenCL-Anbindung | `gpu_bridge.*`, `runtime_cycle.*` | ABI-Bridge, GPU-Pin/Observation, Runtime-Cycle und reale Probe-Schnittstelle |
| MYCL | `mycel_state_persistence.cpp`, `mycl_compare.*`, `mycel_bundle.*` | Laden/Speichern, Neustartfortsetzung, Vergleich und gemeinsames MYCL/Welt/DAD-Bundle |
| Native Windows-Produktintegration | Dienst, MCP, GUI, Medien- und Connection-Module | 49/49 Windows-Regressionen sowie installierte End-to-End-Abnahme |

## Nicht als vollständig portiert ausgewiesene Enterprise-Bereiche

- Historische PHP-Webplattform einschließlich Forum, Blog, Shop, Profil,
  Nachrichten, Adminoberfläche, Telemetrie und Download-Portal.
- Browserseitiger JavaScript-Vault, WebAuthn-, E2EE- und Direct-Ingest-Code.
- Python-Webgateway, Federation-Sync und frühere Streamlit-Oberfläche.
- Vollständige Python-Cognition-/Simulation-Parität jenseits des nativ
  nachgewiesenen DAD-, Observer- und MyceliaWorld-Kerns.
- Vollständiger GPU-VRAM-Snapshot beziehungsweise bitgenaue Wiederaufnahme
  beliebiger herstellerspezifischer GPU-Zustände.
- Die ursprünglichen 76 Python-/Web-/Enterprise-Regressionsfälle als
  eins-zu-eins-C++-Port. Die Windows-Suite prüft stattdessen den hier
  implementierten nativen Produktumfang.

## Ergebnis

Die Windows-Gedächtnisanwendung verwendet keine parallele Ersatzdatenbank und
behält die portierte DAD als Eigentümerin der Gedächtnisinhalte. Ihr
Produktumfang ist gebaut, installiert und getestet. Eine Aussage
„vollständige Enterprise-Portierung“ wäre dagegen weiterhin sachlich falsch.
Für dieses separate Vorhaben muss die Manifestdatei datei- und symbolweise
fortgeschrieben und jeder Status `NOT_PORTED` durch eine native Implementierung
plus Paritätstest ersetzt werden.
