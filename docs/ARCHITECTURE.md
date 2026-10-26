# KR Mycelia Memory – Architektur

## Laufzeitkomponenten

`KR_Mycelia_Memory.exe` ist die native Win32-Benutzeroberfläche. Sie stellt
ein responsives Windows-11-Dashboard mit Seitenleiste, Explorer, Collections,
Timeline, Beziehungen, Streaming-Chat, Agenten- und Verbindungsverwaltung,
Medienimport, Datenschutz, Tray und Opt-in-Autostart bereit. Der angezeigte Dienststatus wird über einen lokalen
Named-Pipe-Ping ermittelt; die Oberfläche behauptet daher nicht pauschal einen
laufenden Hintergrundprozess.

`KR_Mycelia_Service.exe` ist ein pro Benutzer gestarteter Hintergrundprozess
und der einzige aktive Schreiber der DAD. Er liest ausschließlich die lokal
gespeicherten, zuvor bestätigten Erfassungseinstellungen und betreibt den
Ordner-Collector auch dann, wenn das Hauptfenster nicht sichtbar ist. Sein
Named Pipe `\\.\pipe\KRMyceliaMemoryService-v1` hat keinen Netzwerkzugang,
akzeptiert nur denselben Windows-Benutzer und verwendet versionierte JSON-RPC-
Nachrichten mit Größenbegrenzung. GUI, Importer sowie die MCP-Schreibwerkzeuge
übergeben Erinnerungen, Imports, Collections, Beziehungen, Löschungen und
Agenten-Tokenverwaltung an diesen Dienst. Ein nicht erreichbarer Dienst wird
als Fehler gemeldet; es gibt keinen stillen Schreib-Fallback in andere Prozesse.
Bei MCP-Agentenerinnerungen validiert der Dienst zusätzlich den Agenten-Token
gegen seinen gespeicherten SHA-256-Hash, Scope und Ablaufzeit; eine Prüfung
allein im stdio-Prozess genügt dafür nicht.

RPC-Anfragen bleiben auf 64 KiB begrenzt. Antworten werden im Nachrichtenmodus
vollständig über mehrere Pipe-Blöcke gelesen und besitzen eine harte Obergrenze
von 32 MiB. Listenoperationen können kompakte Metadaten liefern; der Explorer
lädt den vollständigen Datensatz erst nach der Auswahl über `memory.get`.

`KR_Mycelia_MCP.exe` ist ein lokaler stdio-MCP-Server. Jeder Aufruf benötigt
einen Agentennamen und einen hash-verifizierten Token mit passendem Scope. Der
One-Click-Manager schreibt Tokens nicht in Clientkonfigurationen: er schützt
sie pro Benutzer mit DPAPI und übergibt dem Server nur den Credential-Pfad.
Cursor-/Claude-JSON und Codex-TOML werden strukturell zusammengeführt,
gesichert und bei Fehlern zurückgerollt.

`KR_Mycelia_Import.exe` importiert einen einzelnen, vom Benutzer angegebenen
lokalen Pfad ohne Cloud- oder Python-Laufzeit.

## Nativer Verarbeitungsfluss

Dokumente und Medien werden ausschließlich über Windows- und Projektkomponenten
verarbeitet. WIC dekodiert Bilder, `Windows.Data.Pdf` rendert PDF-Seiten,
`Windows.Media.Ocr` liefert Text und Geometrie, und Media Foundation dekodiert
Audio beziehungsweise Video. Audio wird zu 16-kHz-Mono-PCM und Zeitsegmenten
dekodiert und über einen installierten Windows-SAPI-Desktoprecognizer offline
transkribiert. Video wird zu zeitcodierten, per pHash zusammengefassten
Szenenframes mit OCR; eine vorhandene Audiospur durchläuft dieselbe lokale
Transkriptionspipeline.
Original, abgeleitete Segmente, Collection-Mitgliedschaft und `derived_from`-
Relationen werden gemeinsam in derselben DAD gespeichert. Es existiert keine
zweite Medien- oder Vektordatenbank.

Der Chatfluss lädt DAD- und Relationskontext über den Dienst, kennzeichnet ihn
als nicht vertrauenswürdiges Referenzmaterial und sendet ihn optional an den
Loopback-Endpunkt von LM Studio. WinHTTP verarbeitet fragmentierte SSE- und
UTF-8-Daten fortlaufend. Ein Abbruch schließt das echte Request-Handle; nur
vollständige Antworten werden zusammen mit ihren Quellsignaturen gespeichert.

## Gedächtnisspeicher

Alle Komponenten verwenden denselben DPAPI-geschützten DAD-Snapshot unter
`%LOCALAPPDATA%\KRMyceliaNativeMemory\memory.dpapi`. `MemoryStore` schützt
Reads und Writes mit einem sitzungsweiten Windows-Mutex. Jede Schreiboperation
lädt den aktuellen Zustand, führt genau eine Transaktion aus, rotiert zwei
verschlüsselte Backups und ersetzt die Snapshot-Datei atomar. Bei beschädigter
Hauptdatei werden `.bak1` und danach `.bak2` geprüft.

Dies verhindert konkurrierende Snapshot-Überschreibungen. Der Hintergrund-
service besitzt den Ordner-Collector und führt die über IPC angeforderten
Schreibtransaktionen zentral aus. Lesende Aufrufe bleiben DPAPI- und
Mutex-geschützt; für nachfolgende Ausbaustufen werden sie ebenfalls über die
Dienstschnittstelle vereinheitlicht.

Der Collector identifiziert Quelldateien über einen kanonischen, unter Windows
groß-/kleinschreibungsunabhängigen Pfadschlüssel. Änderungen aktualisieren den
bestehenden Record; ein abbrechbarer Hintergrundabgleich entfernt gelöschte
Quellen und historische Duplikate. Neben der unmittelbaren Collection werden
der freigegebene Stammordner und das abgeleitete Projekt gespeichert. Die
Chat-Retrievalstufe gewichtet diese Provenienzfelder vor Volltexttreffern und
bildet bei langen Inhalten ein suchbegriffszentriertes Kontextfenster.

Der rekursive Abgleich enumeriert außerhalb des DAD-Mutex und übernimmt jede
zulässige Textquelle anschließend in einer kurzen Transaktion. Zwischen den
Transaktionen erhalten interaktive Leser und Schreiber Zugriff. Automatische
Erfassung ruft keine Binärdecoder, OCR-, Office-, PDF-, Audio- oder
Videoengine auf; diese Pipeline ist ausschließlich dem expliziten Import
zugeordnet.

## Sicherheitsmodell

Named-Pipe-DACL und SID-Prüfung erzwingen denselben angemeldeten Benutzer.
Lokale administrative RPC-Aufrufe benötigen zusätzlich eine DPAPI-Capability;
MCP-Aufrufe werden anhand Token-Hash, Scope, Aktivstatus und Ablauf erneut im
Dienst autorisiert und auditiert. Prozesse verwenden eingeschränkte DLL-
Suchpfade, deaktivierte Legacy-Erweiterungspunkte und blockieren Images von
Remote- oder Low-Integrity-Speicherorten. Parser haben Eingabe- und
Ressourcengrenzen. Diese Maßnahmen sind Defense-in-Depth, keine behauptete
AppContainer-Isolation von bereits kompromittierten Prozessen derselben
Windows-Benutzeridentität.

## Datenschutzgrenzen

Erfassung ist standardmäßig deaktiviert. Fensterkontext wird nur für explizit
freigegebene Anwendungen gespeichert und enthält lediglich Titel, nicht
Prozessspeicher, Tastenanschläge oder Screenshots. Ordner werden nur nach
expliziter Freigabe beobachtet. Importierte Texte sind untrusted data und
werden beim lokalen KI-Kontext ausdrücklich als Referenzmaterial behandelt.
