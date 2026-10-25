# v0.18 – MYCL + Welt + DAD gebündelt

## Implementiert

- `src/mycel_bundle.hpp`, `src/mycel_bundle.cpp`: `MycelBundleCodec`.
- `encode()`/`decode()` verpacken validierte originale MYCL-v1-Bytes und den vorhandenen Welt/DAD-Checkpoint gemeinsam in `KRMYBND1`.
- Größenprüfungen, Versionsprüfung, MYCL-Header-/Dimensionsprüfung und SHA-256-Payloadintegrität.
- `save_secure()`/`load_secure()`: Windows-DPAPI im Benutzerkontext. Speicherung ausschließlich als `KRBNDENC`-Datei, ohne unverschlüsselte Zwischen-Datei. Bestehende Dateien werden nicht überschrieben.
- Auf Nicht-Windows-Systemen kein Klartext-Fallback.
- `mycelia_bundle_tests`: Roundtrip, Mutation, Abschneiden, falsche MYCL-Dimensionen und sichere Speicherpolitik; unter Windows zusätzlich DPAPI-Datei-Roundtrip und Überschreibschutz.

## Grenzen

- MYCL-v1-Dateien repräsentieren die vom Originalexport erfassten **Myzel-Zustände**, nicht den kompletten VRAM, OpenCL-Handles, alle RNG-Zustände oder alle SubQG-Ressourcen.
- Die Bundle-API erhält bereits erzeugte MYCL-Bytes. Sie stoppt **nicht selbst** GPU-Kernels und garantiert keine atomare Aufnahme der laufenden GPU- und CPU-Zustände. Für produktive Saves ist eine koordinierte Quiesce-/Checkpoint-Transaktion erforderlich.
- Ein unkeyed SHA-256-Hash ist Integritätskontrolle, keine Authentifizierung gegen einen Angreifer, der eine Klartextdatei absichtlich neu berechnet. Bei der Windows-Datei schützt DPAPI den Payload.
- Der MYCL-Loader des Originaltreibers ist noch nicht gegen alle feindlichen Eingaben gehärtet. Nur kontrollierte/gültige Daten verwenden; vor dem Laden separat überprüfen.
- Linux 18/18 Test-Suiten PASS. Windows-Kompatibilität der Erweiterung muss über KR Compiler und den neuen Windows-DPAPI-Test bestätigt werden.

## Windows

Nach dem Build:

```powershell
& 'H:\KR-Mycelia-Memory\KR_BUILD\mycelia_bundle_tests.exe'
Write-Host "Bundle Exitcode: $LASTEXITCODE"
```

Erwartet: `PASS: combined MYCL/world/DAD bundle integrity, resume state, secure policy (NOT entire VRAM)`.

Die originale `CMS-MyceliaDB-Enterprise` wurde nicht geändert.
