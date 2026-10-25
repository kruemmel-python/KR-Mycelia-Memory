# Portierung v0.11 – MyceliaWorld CPU / Spatial State

Quellreferenz (nur gelesen): `CMS-MyceliaDB-Enterprise-main.zip` → `Mycelia_Database-main/mycelia_ai/simulation/mycelia_world.py`.

## Originalgetreu übernommener Teil

- `MyceliaWorld._build_neighbor_lattice`: kartesisches, zeilenweises Raster, exakte Offset-Reihenfolge und `-1`-Padding.
- `_derive_activity`: Min-/Max-Normalisierung, Untergrenze `1e-6`, leere Eingabe.
- `_initialize_fields` und `_simulate_fallback_step`: die drei trigonometrischen Felder, deren Clamping und zugehörige konstante Aktivität.
- `_apply_cpu_agent_injection`: Clamping und Koeffizienten für Energie/Pheromon/Nährstoff, ohne vorgezogene Mutation der ausgegebenen Tensoren.
- Laufender Schrittindex und endlicher Phasenzustand; nativer Speicher-Roundtrip zur Fortsetzung des CPU-Fallbacks.
- Testverbindung: WorldFields → ObserverNetwork → DynamicAssociativeDatabase.

## Grenzen und absichtliche Nichtbehauptungen

- Kein vollständiger `MyceliaWorld`-GPU-Port. Die ursprünglichen `subqg_*`, `step_pheromone_reinforce`, `step_pheromone_diffuse`, `step_reproduction_cycle`, `step_colony_update`, `step_subqg_feedback`, `read_pheromone_slice`, `set_neighbors_sparse`, `set_mood_state` und die übrigen Treiber-Operationen sind noch nicht in die Weltklasse integriert.
- Das Format `MYCW011` enthält nur den CPU-Fallback-Fortsetzungszustand. Es ist ein plattformlokaler, unverschlüsselter Test-Serializer ohne Integritätsschutz; **nicht** für private Daten oder für den Einsatz im Windows-Dienst bestimmt. Vor produktiver Verwendung muss der Zustand in die sichere, versionierte Persistenz integriert werden.
- GPU-VRAM-Zustände werden durch diesen Welt-Snapshot nicht gesichert.
- Nicht vollständig portiert: `CognitiveCore` mit Quantum-Guard und Quanten-Orakel, Enterprise/Web/Sicherheit, Windows-Kontexterfassung.
- Python und C++ können sich beim trigonometrischen Rechnen auf unterschiedlichen Plattformen um wenige ULP unterscheiden; Referenztests prüfen geeignete Toleranzen.

## Prüfung

- `mycelia_world_tests`: Raster-Reihenfolge, Aktivitätsnormierung, CPU-Fallback-Feldgleichungen, Agenteninjektion, Wiederaufnahme, korrupte Länge, ungültige Konfiguration, Observer→DAD.
- Linux Release / CTest: 11/11 Tests bestanden.
- Windows KR Compiler/GCC 16.1.0: **noch nicht getestet**.

## Windows

Projektverzeichnis in KR Compiler IDE auswählen; als `Windows x64` bauen. Danach:

```powershell
& "H:\KR-Mycelia-Memory\KR_BUILD\mycelia_world_tests.exe"
Write-Host "Exitcode: $LASTEXITCODE"
```

Erwartet: `PASS: original MyceliaWorld CPU fallback, lattice, activity, resume, Observer -> DAD` und Exitcode 0.
