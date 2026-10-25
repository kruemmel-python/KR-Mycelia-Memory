# Native GUI-Portierung – Abnahmebericht

Stand: 25. Oktober 2026

## Ergebnis

Das HTML-Mockup `design/kr_mycelia_memory_gui_mockup.html` wurde als natives
Win32/C++20-Dashboard umgesetzt. Die Anwendung verwendet weder eine WebView
noch ein zusätzliches UI-Framework. Karten, Seitenleiste, Telemetrieleiste und
Beziehungsgraph werden mit Direct2D und DirectWrite dargestellt; Eingabe,
Navigation, Listen, Fokusführung und Bedienung bleiben native Windows-
Steuerelemente.

## Umgesetzte Komponenten

- Maßstabsgetreue Seitenleiste mit den drei Navigationsgruppen und allen elf
  Zielansichten.
- Responsives Kartenraster für Status, manuelle Erinnerung, Suche, Treffer,
  Graph, Aktivität, Agenten und Suchhistorie/DAD-Metriken.
- Reale Telemetrie für Named Pipe, Attraktoren, mittlere Stabilität,
  Rauschfaktor, freigegebene Ordner, Erfassung und Autostart.
- Laufzeit-Themeauswahl mit den sechs CSS-Farbwelten Fluent Dark, Ocean,
  Forest, Aubergine, Elegant Dark und Light; Auswahl und bereichsspezifische
  Schrift-/Farbeinstellungen werden dauerhaft gespeichert.
- Die zentrale Theme-Schicht zeichnet auch alle nativen Nebenfenster und deren
  Buttons, Kontrollkästchen, Kombinationsfelder, Eingaben und Listen. Explorer,
  Verwaltung, Memory Chat, KI-Verbindungen und Einstellungen übernehmen einen
  Themewechsel unmittelbar, ohne Neustart.
- Interaktiver Direct2D-Graph mit echten DAD-Records und Relationen,
  typisierten Knoten, Hover/Selektion, Zoom, Verschieben, Zentrieren und
  Explorer-Navigation. Datenmenge ist zur Sicherung der Reaktionszeit begrenzt.
- Bestehende produktive Ansichten für Explorer, Collections/Timeline/Agenten,
  Memory Chat, KI-Verbindungen, Import, Datenschutz und Einstellungen bleiben
  über die neue Navigation erreichbar und verwenden die zentrale
  Themeverwaltung.
- Schreiben, Suche, Import, Chat, Agentenverwaltung und Erfassung bleiben an
  Dienst-RPC und die DPAPI-geschützte DAD gebunden. Es wurden keine
  Demonstrationsdatensätze aus dem HTML übernommen.

## Visuelle Prüfung

Die Referenz und die native Anwendung wurden mit 1440 × 900 Bildpunkten
gerendert:

- `docs/screenshots/mockup-reference.png`
- `docs/screenshots/native-dashboard.png`

Nach dem ersten Vergleich wurden DPI-Koordinaten, Seitenleistenpositionen,
Kartenraster, Themefarben, Aktionsbuttons und Graph-Skalierung korrigiert.
Direct2D-Dashboard und Graph verwenden beim Erzeugen und nach jedem
Themewechsel ausdrücklich eine 96-DPI-Pixelbasis; dadurch stimmen Karten und
native Steuerelemente auch bei maximiertem Fenster und Windows-Skalierung
maßgenau überein und werden nicht doppelt skaliert.
Die native Oberfläche übernimmt Struktur, Gewichtung, Abstände, Rundungen,
Farbwelt und Informationshierarchie der Referenz. Leere Bereiche zeigen
bewusst keine erfundenen Beispieldaten.

## Prüfung und Installation

- Windows-x64-Release-Build mit der bereitgestellten KR-Toolchain: erfolgreich.
- Gesamte CTest-Suite: 48 von 48 Tests erfolgreich.
- Theme-/Persistenztest: erfolgreich.
- Installation nach
  `%LOCALAPPDATA%\Programs\KRMyceliaMemory`: erfolgreich.
- DPAPI-Persistenz-Selbsttest der installierten EXE: Exitcode 0.
- Installierter Dienst-/DAD-Neustarttest: erfolgreich.
- Startprüfung der installierten GUI einschließlich Fenstererzeugung:
  erfolgreich.
- Installierte Theme-Prüfung aller sechs geöffneten Produktfenster: 6 von 6
  Fenster verwenden die aktive Palette; keine hellen Standardflächen im
  Dark-Theme.
- Maximiertes Dashboard nach zweimaligem Laufzeit-Themewechsel: keine
  Überschneidungen, alle acht Karten innerhalb des sichtbaren Clientbereichs.

## Bewusste native Abweichungen

- Segoe-UI-Symbolglyphen ersetzen einzelne SVG-Icons des Mockups.
- Explorer, Chat, Verbindungen und Verwaltung werden weiterhin als native
  Produktfenster geöffnet, statt HTML-Modal-Layer zu simulieren.
- Native Edit-, Listen- und Kombinationsfelder behalten Windows-
  Tastaturbedienung und Barrierefreiheit; ihre interne Textdarstellung kann je
  nach Windows-Schriftglättung geringfügig von Chromium abweichen.
- Schatten und Übergänge verwenden Direct2D/DWM ohne die CSS-Animationen der
  Referenz. Reduzierte Windows-Animationen werden dadurch automatisch
  respektiert.

Diese Abweichungen betreffen die native Plattformdarstellung, nicht die
MyceliaDB-Funktion oder die Informationsarchitektur.
