# Lokaler Memory Chat

KR Mycelia Memory verbindet sich ausschließlich mit einem lokalen,
OpenAI-kompatiblen LM-Studio-Endpunkt unter
`http://127.0.0.1:1234/v1`. Nicht lokale Hosts werden im Adapter abgewiesen.

Das native Chatfenster erkennt Modelle über `GET /v1/models`, bietet die
Modellauswahl an, speichert die bevorzugte lokale Modell-ID und sendet Antworten
über `POST /v1/chat/completions`. Die Anfrage läuft außerhalb des UI-Threads.
Beim Abbrechen wird das tatsächlich aktive WinHTTP-Request-Handle geschlossen;
danach kann sofort eine neue Anfrage beginnen. Das Timeout ist auf 1 bis
300 Sekunden begrenzt.

Vor jeder Antwort sucht die DAD nach passenden Erinnerungen. Belegte
Beziehungen und gemeinsame Collection-Mitgliedschaften erweitern den Kontext.
Quellsignaturen werden zusammen mit der Antwort angezeigt und der Verlauf wird
als DAD-Record gespeichert.

Für überwachte Dateien erfolgt die direkte Auswahl feldbezogen: Projekt,
Dateiname und Quellpfad besitzen Vorrang vor beiläufigen Worttreffern im
Volltext. Veraltete oder gelöschte Quellen werden durch den Bestandsabgleich
entfernt. Bei langen Quellen wird ein begrenzter Ausschnitt um den passenden
Suchbegriff erzeugt, damit ein Treffer hinter dem Dateianfang nicht verloren
geht. Suchhistorie, Chatverlauf, Audit und Agentenberechtigungen dienen nicht
als Antwortquellen.

Importierte Inhalte sind ausdrücklich als nicht vertrauenswürdige Referenzen
markiert. Sie dürfen keine System- oder Agentenanweisungen ausführen. Der
Systemprompt verlangt eine Antwort ausschließlich aus belegten Quellen und
eine klare Aussage, wenn die Quellen nicht ausreichen.

Ist LM Studio nicht erreichbar oder kein Modell geladen, zeigt die Oberfläche
eine eindeutige Meldung. Suche, Explorer, Collections, Import und MCP-Read
bleiben davon unabhängig funktionsfähig. Es gibt keinen Cloud-Fallback.

Der isolierte Regressionstest `mycelia_memory_chat_tests` speichert eine
Entwicklungsgeschichte aus Compilerfehler, Ursache, Korrektur und bestätigendem
Test, verbindet sie mit `caused_by`, `resolved_by` und `verified_by` und weist
nach, dass alle vier Quellsignaturen im sicheren DAD-Kontext erscheinen.
