# Installation

1. Das CMake-Projekt in der KR Compiler IDE für Windows x64 bauen.
2. Anschließend im Projektordner ausführen:

```powershell
powershell -ExecutionPolicy Bypass -File H:\KR-Mycelia-Memory\install.ps1
```

Der Installer verlangt aktuelle Builds aller vier Programme, beendet laufende
Produktprozesse, legt eine Rollback-Kopie an, kopiert nach
`%LOCALAPPDATA%\Programs\KRMyceliaMemory`, verifiziert SHA-256-Prüfsummen,
liefert `zlib1.dll` und die benötigten GCC-Laufzeiten `libgcc_s_seh-1.dll`
sowie `libwinpthread-1.dll` direkt neben den Programmen aus,
führt den DPAPI-DAD-Selbsttest sowie einen Schreib-/Dienstneustart-/
Wiederherstellungstest auf den installierten Binärdateien aus und erstellt den
Startmenüeintrag. Danach läuft der Hintergrunddienst weiter. Ein
`install-manifest.json` der Version 3 dokumentiert die installierten Prüfsummen.
Die CMake-Konfiguration kopiert dieselben drei Laufzeitbibliotheken außerdem in
den Build-Ausgabeordner. Dadurch kann `KR_Mycelia_Memory.exe` auch direkt aus
`KR_BUILD\cmake_build` gestartet werden, ohne dass die Compiler-Toolchain im
`PATH` eingetragen sein muss.

Der Speicher bleibt bei einer Deinstallation erhalten. `uninstall.ps1` beendet
alle vier Produktprozesse, entfernt Startmenüeintrag und Programme, aber weder
persönliche Erinnerungen noch Einstellungen oder DPAPI-Capability.

## Nachweis der Persistenz

```powershell
$process = Start-Process `
  -FilePath "$env:LOCALAPPDATA\Programs\KRMyceliaMemory\KR_Mycelia_Memory.exe" `
  -ArgumentList '--persistence-self-test' -Wait -PassThru
$process.ExitCode
```

`0` bestätigt die verschlüsselte Speicherung und Wiederherstellung eines
MyceliaDB-Patterns und einer externen DAD-Zeile.

Der installierte Dienstpfad kann separat geprüft werden:

```powershell
$process = Start-Process `
  -FilePath "$env:LOCALAPPDATA\Programs\KRMyceliaMemory\KR_Mycelia_Memory.exe" `
  -ArgumentList '--service-e2e-test' -Wait -PassThru
$process.ExitCode
```

`0` bestätigt RPC-Schreiben, Lesen, kontrollierten Dienstneustart,
DPAPI-Wiederherstellung und Bereinigung des temporären Datensatzes.
