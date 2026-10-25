# Portierungsstatus v0.6

Neu: Native Snapshots einschließlich Attraktoren, Datenrecords, Reihenfolge, History und Query State; Integritätsprüfung und Backups. `src/snapshot.cpp`, `tests/test_snapshot.cpp`. Fix bei Retention-Bereinigung in `src/mycelia.cpp`.

Verifiziert: Linux GCC Release, CTest 6/6. Keine Aussage über Windows-Build, Enterprise-Vollständigkeit, Verschlüsselung oder Absturzsicherheit.

Ausstehend: Native Windows-Integration, vollständiges Mycelia-Quellmodulparitätsaudit, GPU- und Security-Komponenten. SHA-256 ist keine Verschlüsselung.
