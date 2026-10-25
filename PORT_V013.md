# v0.13 – GPU-Beobachtungscheckpoint (kein VRAM-Restore)

## Ausgangsbasis

Separater Fork von v0.12. Das CMS-MyceliaDB-Enterprise-Original wird nicht verändert. Die bisherige `CC_OpenCL.dll` wird im neuen Test nicht durch eine Neuimplementierung ersetzt; die Test-DLL dient dem reproduzierbaren ABI-Test.

## Neu

- `src/gpu_observation.hpp/.cpp`: `GpuObservationCodec::capture`, `encode`, `decode`.
- Readback des sichtbaren `nutrient`-Feldes aus `GpuBridge` und Zuordnung zur zuletzt übergebenen Aktivität.
- Gemeinsames Binärformat `KRGPUOBS1`: CPU-Welt-/DAD-Checkpoint, Kapazität, aktive Zellzahl, Simulationsschritt, Aktivität und Nährstoffwerte (float32-Bitmuster).
- Klare Größenlimits, endliche Floatwerte, SHA-256-Korruptionserkennung, strenge Längenvalidierung, atomare Rückgabe erst nach vollständiger Prüfung.
- Neuer CTest `mycelia_gpu_observation_tests`: DLL-Readback, Rundlauf, Integrität, abgeschnittene Eingabe, Dimensionen.

## Wichtige Einschränkungen

Das vorhandene native `GpuBridge`-ABI bietet nur `read_nutrient()` als Zustands-Readback. Kein vollständiger Export/Import von OpenCL-Buffern, Agentenverteilung, Pheromon-Zuständen oder internen Zufalls-/Kernelzuständen ist in dieser Integration implementiert. Deshalb **kein vollständiger GPU-Neustart**, keine Wiederaufnahme bitidentischer GPU-Simulation und kein produktiver Windows-Snapshotdienst.

`GpuObservationCodec::encode()` erzeugt ein **unverschlüsseltes In-Memory-Binärformat**; es wird hier absichtlich **nicht** als Datei gespeichert. Wer es später persistiert, muss es zuerst mit einer authentifizierten und korrekt getesteten sicheren Speicherung schützen. Der SHA-256-Anhang ist eine Integritätskontrolle, keine Authentifizierung gegen böswillige Änderungen.

## Nachweise

- Linux / C++20 Release: 13/13 CTest bestanden.
- Windows x64 / KR Compiler: neue Version noch nicht getestet.
- Echte AMD-OpenCL-DLL: bisheriger Echt-GPU-Pfad in v0.10.1 durch Nutzer mit Exitcode 0 nachgewiesen; der neue Codec ist noch nicht gegen diese DLL getestet.

## Windows-Test

Mit KR Compiler IDE Windows x64 bauen, anschließend in PowerShell:

```powershell
$B = 'H:\KR-Mycelia-Memory\KR_BUILD'
& "$B\mycelia_gpu_observation_tests.exe" "$B\libmycelia_test_gpu.dll"
Write-Host "Exitcode: $LASTEXITCODE"
```

Erwartete Ausgabe: `PASS: GPU observed nutrient/activity + world/DAD checkpoint (NOT full VRAM restore)`.
