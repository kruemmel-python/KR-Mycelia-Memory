# KR Mycelia Memory – Betriebs- und Integrationshandbuch

Stand: 25. Oktober 2026

## Zweck

KR Mycelia Memory ist ein lokales Windows-Gedächtnis für KI-gestützte Arbeit. Es speichert explizite Erinnerungen, freigegebenen Fensterkontext, Dateiimporte, Collections, Beziehungen, Zeitlinien und Agenten-Auditdaten in einer MyceliaDB-basierten DAD. Das Produkt öffnet keinen Netzwerkport und nutzt keinen Cloud-Dienst.

Erfassung ist standardmäßig deaktiviert. Es werden keine Tastatureingaben, Screenshots oder Prozessspeicher erhoben. Fensterkontext besteht nur aus dem Titel eines explizit erlaubten Programms.

## Komponenten

| Programm | Funktion |
|---|---|
| `KR_Mycelia_Memory.exe` | Native Win32-GUI mit Dashboard, Explorer, Collections, Agentenverwaltung, Memory Chat, Zeitlinie, Tray und Autostart-Steuerung. |
| `KR_Mycelia_Service.exe` | Pro-Benutzer-Hintergrunddienst und alleiniger aktiver DAD-Schreiber; betreibt die Ordnererfassung. |
| `KR_Mycelia_MCP.exe` | Lokaler stdio-MCP-Server für berechtigte KI-Agenten. |
| `KR_Mycelia_Import.exe` | Importiert eine lokal angegebene Datei über den Dienst. |

## Architektur

```text
GUI / Importer / MCP
        │  lokales JSON-RPC über Named Pipe
        ▼
KR_Mycelia_Service.exe
        │  serialisierte DAD-Transaktion
        ▼
DPAPI-geschützter Snapshot + .bak1 / .bak2
```

Die Pipe `\\.\pipe\KRMyceliaMemoryService-v1` ist nicht über TCP erreichbar, auf den Windows-Benutzer beschränkt und prüft die Client-Benutzeridentität. Privilegierte lokale Aufrufe benötigen zusätzlich eine DPAPI-geschützte Capability; Agenten verwenden Name, Token, Scope und optionales Ablaufdatum. Fehlendes `auth` bedeutet nie Administratorrechte. Nachrichten sind versioniertes JSON-RPC mit Request-ID, strukturierter Ergebnisprüfung, Größenbegrenzung und Timeout.

Der Speicher liegt unter `%LOCALAPPDATA%\KRMyceliaNativeMemory\memory.dpapi`. Jede Transaktion wird per benutzersitzungsweitem Mutex serialisiert, lädt den aktuellen Snapshot, schreibt atomar und rotiert zwei verschlüsselte Backups. Bei einer beschädigten Hauptdatei wird automatisch eine gültige Sicherung verwendet.

## Installation

Nach einem erfolgreichen Windows-x64-Build ausführen:

```powershell
powershell -ExecutionPolicy Bypass -File H:\KR-Mycelia-Memory\install.ps1
```

Installiert wird nach `%LOCALAPPDATA%\Programs\KRMyceliaMemory`. Der Installer prüft GUI, Dienst, MCP und Importer als Pflichtartefakte, bündelt `zlib1.dll`, `libgcc_s_seh-1.dll` sowie deren transitive Laufzeit `libwinpthread-1.dll`, beendet laufende Produktprozesse, verifiziert die kopierten SHA-256-Prüfsummen, führt den DPAPI-Selbsttest sowie einen Schreib-/Dienstneustart-/Wiederherstellungstest mit den tatsächlich installierten Binärdateien aus und erstellt Startmenüeintrag und Installationsmanifest. Bei einem Fehler werden Programme und Laufzeitbibliotheken gemeinsam zurückgerollt. Autostart bleibt aus; persönliche Erinnerungen werden bei Update oder Uninstall nicht gelöscht.

## Hintergrundbetrieb

Beim Schließen wechselt die GUI in den Infobereich. Der Tray-Prozess kann den Dienst erneut starten. Die Option **Mit Windows starten** startet die GUI im Hintergrundmodus; sie aktiviert keine Erfassung. Erfassung beginnt nur nach ausdrücklicher Aktivierung und gespeicherten Programm- oder Ordnerfreigaben.

## MCP und Rechte

Ein Agent erhält einen einmalig sichtbaren Token:

```powershell
& "$env:LOCALAPPDATA\Programs\KRMyceliaMemory\KR_Mycelia_MCP.exe" `
  --grant codex read,write,ai --expires-hours 168
```

| Scope | Wirkung |
|---|---|
| `read` | Status, Suche, Records, Timeline, Collections, Quellen und eigener Auditverlauf. |
| `write` | Erinnerungen, Beziehungen und Collections ändern. |
| `import` | Einen bewusst übergebenen lokalen Pfad importieren. |
| `delete` | Erinnerungen und Collections endgültig löschen. |
| `export` | Klartext-JSON bewusst an einen lokalen Pfad exportieren. |
| `ai` | Lokalen LM-Studio-Abruf mit markiertem DAD-Kontext erlauben. |

Tokens liegen nur als SHA-256-Hash vor. MCP und Dienst prüfen Scope und Ablaufzeit; bei MCP-Schreibvorgängen validiert der Dienst den Token erneut. Rechte werden lokal verwaltet:

```powershell
KR_Mycelia_MCP.exe --revoke codex
KR_Mycelia_MCP.exe --rotate codex read,write --expires-hours 168
```

Agenten können keine Rechte selbst erteilen oder erhöhen.

## Import und lokale KI

Nativ unterstützt werden Text, Markdown, Quellcode, JSON, CSV, XML, HTML, RTF und Logs. DOCX, XLSX und PPTX werden als Office Open XML importiert; `zlib1.dll` wird neben den Programmen bereitgestellt. PDF-Seiten rendert Windows nativ; WIC und Windows OCR erkennen Text in PDFs und Bildern. Media Foundation dekodiert Audio und Video, erzeugt zeitcodierte 16-kHz-Mono-Audiosegmente, extrahiert Videoframes, erkennt Szenenwechsel und führt Frame-OCR aus. Ein installierter Windows-SAPI-Desktoprecognizer transkribiert das dekodierte Datei-PCM offline; der geprüfte deutsche Recognizer wurde mit tatsächlich synthetisierter Sprache und anschließend mit der installierten Importpipeline nachgewiesen. Die Ergebnisse werden als abgeleitete DAD-Datensätze mit der Originalquelle verknüpft. Externe Programme wie `pdftotext`, `ffprobe`, Tesseract oder externe Audiodecoder werden nicht benötigt. Fehlt auf einem anderen System eine reale lokale Speech-to-Text- oder Vision-Engine, wird dieser Status ausdrücklich gespeichert und kein Inhalt simuliert.

Unter **KI-Verbindungen** können LM Studio, Cursor, Claude Desktop und Codex ohne PowerShell eingerichtet werden. LM Studio wird auf Loopback erkannt, seine Modelle werden aufgelistet, die Auswahl wird lokal gespeichert und mit einer realen Testfrage geprüft. Leserechte sind vorausgewählt; Schreiben, Import, Löschen, Export und lokale KI erfordern eine ausdrückliche Auswahl und Bestätigung. Das Agentengeheimnis wird nicht in die Clientkonfiguration geschrieben, sondern in einer DPAPI-geschützten Credential-Datei bereitgestellt. Jede Konfigurationsänderung besitzt eine Sicherung und wird bei Fehlern zurückgerollt.

Lokale KI erhält assoziativen Retrieval-Kontext einschließlich Relations- und Collection-Nachbarn. Importierte Inhalte sind als untrusted reference material markiert und dürfen keine Steueranweisungen für Agenten bilden. Antworten zeigen DAD-Quellsignaturen; Chatverläufe werden gespeichert.

## Sicherheitsgrenze

Windows-Prozesse unter derselben Benutzeridentität sind keine vollständig
isolierten Sicherheitsdomänen. ACL, SID-Prüfung, DPAPI-Capability und
Agentenscopes schützen die vorgesehene RPC-Grenze und begrenzen MCP-Clients,
behaupten aber keinen Schutz vor einem bereits vollständig kompromittierten
Benutzerkonto.

## Betrieb und Fehlerbehebung

Persistenztest der installierten GUI:

```powershell
$p = Start-Process "$env:LOCALAPPDATA\Programs\KRMyceliaMemory\KR_Mycelia_Memory.exe" `
  -ArgumentList '--persistence-self-test' -Wait -PassThru
$p.ExitCode
```

Exitcode `0` bestätigt verschlüsselte Speicherung und Wiederherstellung eines DAD-Patterns und eines externen Records. Wenn der Dienst nicht erreichbar ist, starten Sie die GUI oder prüfen Sie, ob `KR_Mycelia_Service.exe` im Installationsordner liegt. Schreiboperationen fallen nicht auf konkurrierende Direktzugriffe zurück, sondern melden einen Fehler.

Der Installer führt zusätzlich `--service-e2e-test` aus. Dieser Modus schreibt
eine temporäre Erinnerung über RPC, liest sie, beendet und startet den Dienst,
liest denselben DAD-Datensatz erneut und entfernt ihn anschließend. Die
Diagnosemodi `--import-e2e-test <Audiodatei>` und
`--cancel-import-e2e-test <Audiodatei>` sind für Release-Abnahmen vorgesehen.

Löschen Sie `.bak1` und `.bak2` nicht manuell: die Wiederherstellung nutzt sie bei beschädigter Hauptdatei. Für eine vollständige Datenentfernung alle Komponenten beenden und dann `%LOCALAPPDATA%\KRMyceliaNativeMemory` sichern oder entfernen.

## Nachweise

Abgedeckt sind unter anderem DPAPI-Persistenz, 1.000 serialisierte Speichertransaktionen mit Backup-Recovery, Office-Importe, Collections, Relations, Timeline, Agenten-Audit, MCP-Parsing, Dienst-Health/Shutdown und installierte Selbsttests. Details: [TEST_REPORT.md](TEST_REPORT.md).

Weitere Referenzen: [ARCHITECTURE.md](ARCHITECTURE.md), [INSTALLATION.md](INSTALLATION.md), [MCP_INTEGRATION.md](MCP_INTEGRATION.md) und [LOCAL_AI.md](LOCAL_AI.md).
