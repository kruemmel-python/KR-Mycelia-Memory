# Testbericht – KR Mycelia Memory

Stand: 26. Oktober 2026

## Release-Build

Der Windows-x64-Release-Build wurde mit der eingebundenen KR-Compiler-
Toolchain vollständig erzeugt. Erfolgreich gebaut wurden unter anderem:

- `KR_Mycelia_Memory.exe`
- `KR_Mycelia_Service.exe`
- `KR_Mycelia_MCP.exe`
- `KR_Mycelia_Import.exe`
- alle 48 registrierten Testprogramme

## Automatische Regression

Ausgeführt wurde:

```powershell
ctest --test-dir H:\KR-Mycelia-Memory\KR_BUILD\cmake_build `
  -C Release --output-on-failure -j 4
```

Ergebnis: **48 von 48 Tests bestanden, 0 Fehler**. Die abschließende Prüfung
des regulären Installer-Builds benötigte 189,11 Sekunden; ein unmittelbar
zuvor ausgeführter isolierter Release-Prüfbuild 36,22 Sekunden. Enthalten sind
DAD-Kern, Observer, SQL/Attraktoren, Snapshots,
DPAPI, GPU-Bridge, Runtime/World/MYCL, 1.000 MemoryStore-Transaktionen mit
Recovery, Office-Import, Collections, Beziehungen, Timeline, Agenten-Audit,
lokaler Chatkontext, Medienmetadaten, Löschen, Export, Folder-Policy,
JSON/MCP-Parser und die strikte RPC-Antwortprüfung. Neu abgedeckt sind außerdem
fragmentiertes SSE/UTF-8, tatsächlicher LM-Studio-Streamabbruch, reale Windows-
OCR, native PDF-Seitendarstellung mit OCR, Media-Foundation-Audiodekodierung,
echte Offline-SAPI-Dateitranskription, Videoerzeugung/-dekodierung,
Szenenwechsel, Video-OCR, DAD-Mediensegmente mit
Quellrelation und Snapshot-Restore, One-Click-Konfigurations-
Merge/Rollback/Trennung, Windows-11-Theme/DPI-Helfer sowie aktive Windows-
Prozessmitigierungen.

Der Datei-/Chat-Regressionsfall bildet außerdem den gemeldeten
Sammelordnerfehler nach: Eine entfernte `KALYX\README.md` erwähnt `MDraft`,
während die aktuelle `MDraft\README.md` andere Inhalte enthält. Geprüft werden
kanonischer Quellpfad, Projektzuordnung `MDraft`, Aktualisierung ohne Duplikat,
Löschung der Altquelle, Vorrang von Projekt/Datei/Pfad und ein Treffer hinter
5.000 Zeichen in einer langen Quelle. Der Memory Chat wählt in diesem Fall
nachweislich die aktuelle MDraft-Datei.

Der Theme-Test prüft zusätzlich sechs Designvorgaben, vier Schriftgrößen,
sieben kontrastgeprüfte Bereichsfarben und die lokale Einstellungspersistenz.
Ein nativer GUI-Smoke-Test öffnete das Einstellungsfenster über die neue
Seitenleistennavigation und bestätigte die Auswahlfelder mit 6/6/4/7 Einträgen.
Die installierte GUI wurde ebenfalls geprüft: Ozean, große Dashboard-Schrift
und Bernstein wurden über die echten Steuerelemente übernommen, in
`ui-settings.txt` bestätigt und die vorherigen Benutzereinstellungen danach
wiederhergestellt.

Der Chat-Regressionsfall nutzt ausschließlich eine isolierte In-Memory-DAD. Er
speichert Compilerfehler, Ursache, Korrektur und erfolgreichen Regressionstest,
verknüpft sie über `caused_by`, `resolved_by` und `verified_by` und bestätigt,
dass alle vier Quellsignaturen im sicheren Chatkontext enthalten sind.

Zusätzlich wurde der gleiche Fall gegen das tatsächlich laufende lokale
LM-Studio-Modell und den installierten Dienst ausgeführt. Die Antwort nannte
den Fehler `sql war nicht deklariert`, die Korrektur (Collection-Wert direkt
aus der SQL-Zeile) und `mycelia_memory_import_tests` als Bestätigung und führte
zu jedem Teil eine DAD-Quellsignatur auf. Eine zunächst sichtbar gewordene
Rangverdrängung wurde durch exakte lexikalische DAD-Vorselektion vor der
assoziativen/relationsbasierten Erweiterung korrigiert. Alle temporären Records,
Relations- und Chatverlaufsdaten wurden danach entfernt.

## Installierte End-to-End-Abnahme

`install.ps1` wurde mit den aktuellen Release-Artefakten ausgeführt. Der
Installer bestätigte Hashkopien, Installationsmanifest, den DPAPI-
Persistenz-Selbsttest und den integrierten Dienst-/DAD-Neustarttest der
tatsächlich installierten GUI. Anschließend wurde der installierte Dienst geprüft:

1. `service.health` beantwortet.
2. `memory.status` ohne Authentifizierung mit `unauthorized` abgewiesen.
3. Temporäres Markdown-Dokument über den Dienst importiert.
4. Importierte DAD-Erinnerung assoziativ wiedergefunden.
5. Kurzlebiger Testagent mit `read,write` angelegt.
6. Berechtigtes MCP-Schreiben über den Dienst erfolgreich.
7. Löschung ohne `delete`-Scope abgewiesen.
8. Installierter MCP-Prozess gestartet; `tools/list` enthielt `memory_search`,
   aber erwartungsgemäß kein `memory_delete`.
9. Dienst kontrolliert beendet und aus dem Installationsordner neu gestartet.
10. Zuvor bestätigte Erinnerung nach Neustart erneut geladen.
11. Temporäre Erinnerung, Import, automatische Collection, Testberechtigungen,
    Auditrecords und Quelldatei vollständig entfernt.
12. Eine reale, von Windows SAPI erzeugte deutsche WAV-Datei durch die
    installierte Pipeline importiert, offline transkribiert und über Inhalt plus
    Quellenpfad in der DAD wiedergefunden.
13. Einen zweiten Import parallel über eine eigene Pipeverbindung abgebrochen;
    der Abbruch wurde dem Media-/SAPI-Pfad zugestellt und hinterließ keinen
    partiell gespeicherten Quelldatensatz.
14. Alle vier installierten Programme sowie `zlib1.dll`,
    `libgcc_s_seh-1.dll` und `libwinpthread-1.dll` gegen ihre SHA-256-
    Quellprüfsummen geprüft; das Manifest enthält alle sieben Nutzlastdateien.
15. Den installierten Persistenztest mit einem auf Windows-Systemverzeichnisse
    reduzierten `PATH` ausgeführt. Er bestand ohne Compiler- oder Toolchain-
    Suchpfad und belegt die vollständige lokale Laufzeitbereitstellung.
16. Auch die unmittelbar aus `KR_BUILD\cmake_build` gestartete GUI mit
    bereinigtem `PATH` geprüft. CMake bündelt dort nun `zlib1.dll`,
    `libgcc_s_seh-1.dll` und die transitive Abhängigkeit
    `libwinpthread-1.dll`; der eigenständige Start bestand.

Die aktuelle Premium-Ausgabe wurde anschließend zusätzlich als installierte
Anwendung geprüft: responsive Größenänderung, vollständige Seitenleiste,
Datenschutzseite und Ordnerauswahl, Öffnen des KI-Verbindungsmanagers,
Fensterschließen in den Tray sowie vollständiges Beenden. Ein frisch erzeugtes
DPAPI-Credential wurde vom installierten MCP-Prozess entschlüsselt; MCP-
`initialize`, scope-abhängiges `tools/list` und der erneute installierte DAD-
Persistenztest waren erfolgreich. Der Deinstaller wurde gegen ein isoliertes
Installationsziel geprüft und ließ das Benutzerdatenziel unangetastet.

Damit verblieben keine E2E-Test-Erinnerungen im persönlichen Speicher.

## Sicherheitsnachweise

- Fehlendes `auth` gewährt ausschließlich `service.health`, niemals
  administrative Rechte.
- Lokale privilegierte Aufrufe benötigen eine DPAPI-geschützte Capability.
- MCP-Aufrufe werden im Dienst erneut anhand Agent, Token-Hash, Scope,
  Aktivstatus und Ablaufzeit geprüft.
- Antworterfolg wird strukturell anhand Version, Request-ID, booleschem `ok`
  und Fehlerobjekt geprüft; beschädigte, abgeschnittene, fremde und
  Teilstring-Antworten werden in `mycelia_service_rpc_tests` abgewiesen.
- Pipe-ACL und SID-Prüfung begrenzen den Transport auf den aktuellen Benutzer.
- Dieselbe Benutzeridentität bleibt ausdrücklich eine gemeinsame
  OS-Sicherheitsdomäne; eine unmögliche Isolation wird nicht behauptet.

## Native Mediennachweise

- Der OCR-Test erzeugt echte BMP-, PNG-, JPEG- und TIFF-Dateien und prüft den
  tatsächlich erkannten Text, nicht nur API-Verfügbarkeit. WEBP wird genutzt,
  sofern ein systemweiter WIC-Encoder vorhanden ist.
- Der PDF-Test erzeugt eine gültige PDF-Seite, rendert sie über
  `Windows.Data.Pdf` und erkennt deren Text über `Windows.Media.Ocr`.
- Der Audiotest dekodiert eine reale 8-kHz-WAV-Datei über Media Foundation und
  bestätigt 16-kHz-Mono-PCM sowie Zeitsegmente.
- Der Speech-Test erzeugt mit einer installierten deutschen Windows-SAPI-Stimme
  eine reale WAV-Datei, transkribiert sie mit dem installierten Offline-
  Recognizer, prüft das Wort „Test“, Zeitangaben und Abbruchweiterleitung.
- Der Videotest erzeugt eine WMV-Datei über Media Foundation, dekodiert
  zeitcodierte Frames und bestätigt Szenenwechsel, Deduplizierung und OCR.
- Der Streaming-Test spricht den real laufenden lokalen LM-Studio-Endpunkt an
  und belegt, dass ein laufender WinHTTP-Request am Transport beendet wird.

## Verbleibende externe Kompatibilitätstests

Für eine breitere Freigabe sollten reale, große Office- und PDF-Dokumente
unterschiedlicher Erzeuger, weitere Audioformate/Sprachen sowie verschiedene
lokale LM-Studio-Modelle geprüft werden. Offline-Transkription hängt von einem
installierten Desktop-SAPI-Recognizer der Zielsprache ab; Deutsch wurde auf dem
Abnahmesystem real nachgewiesen. Optionale semantische Vision-Modelle wurden
nicht simuliert und bleiben eine bewusst ausgewiesene Erweiterungsgrenze.
