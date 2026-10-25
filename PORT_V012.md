# KR Mycelia Native v0.12 — World + DAD checkpoint

## Implementiert

- `WorldCheckpointCodec::encode` kombiniert die bereits portierte MyceliaWorld-CPU-Zustandsserialisierung und den originalgetreuen DAD-Snapshot in einem versionierten Binärcontainer (`KRCHECK1`).
- Beide Teilstücke erhalten ihre ursprünglichen Formate (`MYCW011`, `MYCELIA6`); die kombinierte Hülle wird zusätzlich mit SHA-256 gegen zufällige Beschädigung abgesichert.
- `decode` prüft Header, Feldgrößen, Gesamtgröße und Prüfsumme, bevor **beide** Zustände erstellt und als zusammengehöriges Paar zurückgegeben werden. Keine teilweise Rückgabe.
- `save_secure` verwendet unter Windows DPAPI im aktuellen Benutzerkontext und schreibt nur Ciphertext über eine temporäre Datei mit Flush und MoveFileExW. Es gibt keinen unverschlüsselten Datei-Fallback.
- `load_secure` entschlüsselt und validiert den gemeinsamen Checkpoint.
- Neuer Regressionstest prüft Welt/DAD-Roundtrip, Zustandsfortsetzung, Tamper- und Truncation-Abwehr sowie die Fail-Closed-Policy auf Nicht-Windows. Der Windows-DPAPI-Testzweig ist vorhanden, aber hier nicht ausgeführt.

## Grenzen

- Dies ist ein konsistenter Checkpoint **der bislang portierten CPU-MyceliaWorld und DAD**, **kein** VRAM-/OpenCL-Snapshot. Die originalen OpenCL-Buffer, Kernelzustände, LBM, STDP, Quantenoperationen, Koloniezustände oder Enterprise-Features werden nicht vollständig serialisiert.
- Die C++-CPU-Welt kann von gespeicherter Phase, Parameterwerten und Fallback-Feldern aus weiterlaufen. Das gilt nicht für GPU-Berechnung; zur Fortsetzung der GPU-Simulation ist gesondertes Device-State-Export/Import nötig.
- SHA-256 im portablen Container ist eine unkeyed Prüfsumme, keine Authentifizierung gegenüber böswilligen Schreibzugriffen; DPAPI schützt das Windows-Payload benutzergebunden.
- Gleichzeitige Schreiber / Prozesssperren, automatische Backup-Rotation und Crash-Recovery über mehrere Dateien sind nicht implementiert.
- DPAPI-Daten sind an Windows-Kontext/Benutzer gebunden. Wiederherstellung unter anderen Benutzerkonten oder auf anderen Geräten ist nicht zugesichert.
- Die Windows-API-Build- und Laufzeitparität muss noch mit KR Compiler IDE verifiziert werden.

## Build / Test

Mit der KR Compiler IDE das Projekt normal für Windows x64 bauen. Danach:

```powershell
$B = 'H:\KR-Mycelia-Memory\KR_BUILD'
& "$B\mycelia_world_checkpoint_tests.exe"
Write-Host "Exitcode: $LASTEXITCODE"
```

Erwartete Meldung: `PASS: world-memory checkpoint integrity, resume, secure storage policy` und Exitcode 0.

## Teststatus hier

Linux x64, Release, GCC, Ninja: **12/12 CTest grün**. Die Windows-DPAPI-Ausführung sowie echte GPU-Hardwaretests der v0.12 wurden hier nicht ausgeführt.

Das MyceliaDB-Originalarchiv und die KR Compiler IDE wurden nicht verändert.
