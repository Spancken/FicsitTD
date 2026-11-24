---
titel: Simulator sauber aufgesetzt - Abgleich Modell gegen Spiel als Pflicht, Kernschaden je Gegnerart im Log
typ: intern
modi: alle
sichtbar: nein
doku: nicht-noetig
simulation: ja
tests: Startkapital-0124, Zielwahl-0124 (verworfen), Grundtest
experiment:
---

**Was:**
- Mod: Der Abwehrkern zählt je Welle Schaden, Treffer und Angreifer je Gegnerart. Zu Wellenende steht im Log
  `Kernschaden Welle N: <Art> <LP> LP von <n> Gegnern (<t> Treffer), ...` (`TDDefenseCore`, `TDWaveSubsystem.cpp`). Nur Messung, keine Spielwirkung.
- Simulator:
  - `messungen.py`: eine Quelle für Spielmessungen. Läufe, gültige und abgeschnittene Wellen, Kernschaden aus dem Log, Ausbau aus der Plandatei.
  - `abgleich.py`: spielt Testläufe fester Pläne im Modell nach. Grenzen: Fallwelle ±2, Kern ±15 Punkte. Stand in `abgleich_stand.json`.
  - `sim_alle_modi.py`: Exit 3 „Modell nicht bestätigt“, solange der Abgleich fehlt, durchfällt oder die Modellannahmen seitdem geändert wurden. Die Pipeline meldet das.
  - Modell: Overkill (Schüsse ganzzahlig, `Turmwerte.treffer`, Schalter `VerlustOverkill`), Verlustkurve neu geeicht mit `eiche_verlust.py` (150 Wellen, Overkill-bereinigt, kleinste Quadrate): VerlustQ 15, VerlustP 0,8, VerlustWenigeTuerme 2, Exponent 0,5.
  - Abgleich danach: Gewehr-Tesla bestanden, Mono-Bolzen (Modell W31, Spiel W22) und K1 (Modell W10, Spiel W7–8) nicht. `sim_alle_modi` meldet Exit 3.
  - 20 Einzelwerkzeuge nach `Tools/Simulator/Archiv/`. README des Simulators beschreibt den Ablauf „messen, abgleichen, eichen, rechnen“.

**Warum:** Marius am 24.11.: „ich möchte nicht immer einen Fehler beheben, nur um einen anderen aufzumachen“ und „Simulation sauber aufsetzen“.
Am 24.11. schlug der Simulator 40 Start-Coupons mit 63 % mehr Gegnerleben vor. Im Spiel fielen damit alle Läufe in W6, weil das Modell die frühen Wellen etwa um den Faktor 2 zu günstig rechnet.
Die Balance darf erst wieder nach dem Simulator gehen, wenn er die Testläufe richtig vorhersagt.

**Changelog EN:** –

**Changelog DE:** –
