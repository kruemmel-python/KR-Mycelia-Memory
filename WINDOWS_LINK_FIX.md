# Windows GUI Link-Fix v0.1.1

Fehler: `undefined reference to WinMain` beim Linken von `KR_Mycelia_Memory.exe` mit MinGW-GCC `-mwindows`.

Ursache: Quellcode definierte nur `wWinMain`, MinGW CRT erwartete `WinMain` ohne `-municode`.

Fix: Portabler `WinMain`-Wrapper leitet an die vorhandene `wWinMain`-Funktion weiter; `GetCommandLineW()` liefert den Unicode-Kommandozeilenzeiger. Die Anwendungslogik bleibt unveraendert.

Status: Logisch und durch Codeinspektion geprueft. Der Windows-Build muss mit der KR-Toolchain durchgefuehrt werden.
