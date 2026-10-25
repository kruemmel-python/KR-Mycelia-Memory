# Portierungsinkrement v0.9 – native Laufzeitkopplung

## Originalbezug
Aus `Mycelia_Database-main/mycelia_ai/cognition/cognitive_core.py` stammt der Ablauf von `CognitiveCore.reflect`: Observer über Pheromon- und Nährstofffelder, `qualia=(harmony-tension)*0.5`, anschließend `DynamicAssociativeDatabase.observe`. Die ABI-Funktionen `step_mycel_update` und `read_nutrient` stammen aus der unveränderten CC_OpenCL-Exportliste.

## Neu
- `src/runtime_cycle.hpp/.cpp`: RuntimeCycle bindet GPU-Update und Nährstoff-Readback an die bestehende Observer- und DAD-Implementierung.
- Ein Aufruf erfordert vom aufrufenden System echte `energy`- und `pheromone`-Felder; diese Daten werden nicht vorgetäuscht.
- Endliche Zahlen, Feldlängen und unerwartete GPU-Ausgabelängen werden geprüft.
- `tests/test_runtime_cycle.cpp`: Rückführung GPU-Nährstoff -> Observer -> Attraktor, Rauschzustandsentwicklung, ungültige Eingaben und Snapshot-Roundtrip.

## Grenzen
- Die Tests verwenden `tests/fake_gpu.cpp`, **nicht** die produktive CC_OpenCL.dll. Damit sind ABI-Kopplung und C++-Datenfluss getestet, aber nicht reale OpenCL-Kernels.
- `MyceliaWorld`-Evolution, Quantenorakel, Intuitions-Guard, Action Planning, vollständiger GPU-State-Snapshot und Enterprise-Webfunktionen sind **noch nicht portiert**.
- Der vollständige Originalzustand (insbesondere VRAM/Kernel-Zustände) überlebt durch den bestehenden DAD-Snapshot **nicht**.
- Der Bridge-Aufruf ist bislang synchron. Ein Fehler *nach* erfolgreichem GPU-Update kann einen neuen GPU-Zustand ohne korrespondierenden neuen DAD-Eintrag hinterlassen. Dafür fehlen noch Transaktions-/Checkpoint-Regeln.
- Unter Windows erneut mit der KR-Toolchain bauen und die Tests ausführen. Linux-Test: 9/9 bestanden.

## Windows
Die KR Compiler IDE sollte alle CMake-Targets erzeugen, darunter `mycelia_runtime_cycle_tests.exe`. Der neue Test erwartet den DLL-Pfad:

```powershell
& "H:\KR-Mycelia-Memory\KR_BUILD\mycelia_runtime_cycle_tests.exe" "H:\KR-Mycelia-Memory\KR_BUILD\libmycelia_test_gpu.dll"
```

Das Originalarchiv und die Compilerinstallation wurden nicht verändert.
