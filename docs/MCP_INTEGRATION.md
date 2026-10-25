# Lokale MCP-Integration

`KR_Mycelia_MCP.exe` kommuniziert ausschließlich über stdin/stdout mit
JSON-RPC 2.0. Es öffnet keinen TCP-Port.

## Empfohlene Einrichtung

Öffnen Sie in der Anwendung **KI-Verbindungen**, wählen Sie Cursor, Claude
Desktop oder Codex und klicken Sie **Verbinden**. Die Anwendung erkennt die
Clientkonfiguration, zeigt die beantragten Rechte, fordert eine Bestätigung an,
sichert und ergänzt die bestehende Konfiguration strukturell und prüft das
Credential am Dienst. Fremde MCP-Einträge werden nicht überschrieben.

Das Token wird nur einmal erzeugt, anschließend als Hash in der DAD geführt
und als DPAPI-geschützte Datei unter
`%LOCALAPPDATA%\KRMyceliaNativeMemory\credentials` gespeichert. Die
Clientkonfiguration enthält nur `--agent` und `--credential`; kein Klartext-
Token. **Trennen** widerruft es und entfernt ausschließlich den eigenen Eintrag.

## Administrative Kommandozeile

```powershell
& "$env:LOCALAPPDATA\Programs\KRMyceliaMemory\KR_Mycelia_MCP.exe" `
  --grant codex read,write,ai --expires-hours 168
```

Für Diagnose kann eine DPAPI-Credential-Datei über die Oberfläche erzeugt und
so gestartet werden:

```json
{
  "command": "C:\\Users\\USER\\AppData\\Local\\Programs\\KRMyceliaMemory\\KR_Mycelia_MCP.exe",
  "args": ["--agent", "codex", "--credential", "C:\\...\\codex.dpapi"]
}
```

Das konkrete Konfigurationsformat muss zur verwendeten Clientversion passen.
Diese Datei bestätigt nicht, dass ein beliebiger Cloud-Client lokale Prozesse
erreichen kann.

## Berechtigungen

| Scope | Erlaubte Wirkung |
|---|---|
| `read` | Suche, Status, Timeline, Quellen, Records und eigener Auditverlauf |
| `write` | explizite Erinnerungen und nachvollziehbare Beziehungen |
| `import` | ein explizit angegebener lokaler Dateipfad |
| `delete` | endgültiges Entfernen einer Erinnerung samt Beziehungen |
| `export` | bewusster Klartext-JSON-Export an einen absoluten lokalen Pfad |
| `ai` | lokaler LM-Studio-Abruf mit untrusted DAD-Kontext |

Tokens werden nur als SHA-256-Verifikationswert gespeichert. Widerruf und
Rotation sind lokal ausführbar:

```powershell
KR_Mycelia_MCP.exe --revoke codex
KR_Mycelia_MCP.exe --rotate codex read,ai --expires-hours 168
```

Für automatisierte lokale Prüfungen steht zusätzlich
`--expires-seconds N` zur Verfügung. Produktive Berechtigungen sollten mit
`--expires-hours` vergeben werden. Beide Varianten akzeptieren maximal 365
Tage Laufzeit.

Eine MCP-Verbindung darf keine Berechtigungen selbst erteilen oder erhöhen.
