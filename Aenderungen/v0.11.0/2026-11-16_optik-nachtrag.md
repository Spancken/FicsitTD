---
titel: Tor mit Anschlusskasten, AO ohne Kollisionskörper gebacken, Optik-Prüfung im Spiel
typ: behoben
modi: alle
sichtbar: ja
doku: nicht-noetig
simulation: nicht-noetig
tests: Optik
experiment:
---

**Was:**
- **Tor:** Der Original-Wandmast (`SM_PowerPoleWall_MK1`, ragte 1 m heraus) ist ersetzt durch einen Anschlusskasten an der Rückseite der linken Säule (im Modell `SM_TD_Gate_Base`).
  - Die Statuslampe sitzt auf dem Lampensockel des Sturzes (5,22 m statt 5,25 m).
  - Lage und Klarraum kommen aus `Modelle\Anbauten.json` (Eintrag „Tor“): X weiter ±4 m, Y −1,2 bis +1,0 m.
  - Die flachen Rampen zählen nicht zum Klarraum.
- **Umgebungsverdeckung (AO) beim Backen:**
  - **Ursache:** `td_backen.py` ließ die Kollisionskörper (`UCX_*`) als sichtbare Geometrie in der Szene. Sie umschließen die Teile als Kästen.
  - **Folge:** Ganze Flächen bekamen AO ≈ 0, im Spiel also fast schwarz. Wo eine Kastenseite auf einer echten Fläche lag, entstand das Sägezahnmuster.
  - **Behoben:** Die Kollisionskörper sind beim Backen ausgeblendet, beim Export bleiben sie erhalten. Alle Gebäude, das Tor und die Statuslampe sind neu gebacken.
  - **Beleg am Bolzensockel:** Vorher hatten 24,9 % der sichtbaren Fläche AO < 0,1, nachher 1,1 %. Gemessen mit `Modelle\td_ao_sichtbar.py`, Kontaktflächen und Spalte gelten als verdeckt.
- **Bauplan:** `Scripts\setup_turret_blueprint.py` setzt die Stromkomponente aus `Tests\Anbauten.json`. Die Handwerte `strom_ort` sind entfernt.
- **Prüfungen:**
  - **Statisch:** `pruefe_optik_statisch.py` meldet Original-Strommast oder -Statusmast, wenn sie im Code angebaut werden (auch in `BeginPlay`). Bauten ohne `ATDTurretBase` (Tor) prüft sie gegen ihren eigenen Klarraum.
  - **Autotest:** Neue Prüfung `pruefe: Optik` (P3). Sie schlägt an bei einem Original-Strom- oder -Statusmast als Teil, bei einer Nebelfläche in 6 m Umkreis oder bei einem Stromanschluss außerhalb des Klarraums.
  - **Schritt `stelle`:** Mit `"suchen": true` sucht er den Standplatz reihum: Boden ohne Gebäude, höchstens 4 m über oder unter dem Fuß des Baus. Vorher zeigte das Foto des Drohnenplatzes nur Himmel. Ohne `suchen` verhält er sich wie bisher. `stelle` kennt jetzt auch „Tor“. Die Änderungen an `TDAutotest.cpp` sind rein additiv.
  - **Plan `Optik`:** zusätzlich das Tor, die Prüfung `Optik` auf Level 1 und 5 und Fotos auf Level 5.
- **Werkzeuge:**
  - `Modelle\td_deckgleich.py` findet deckungsgleiche Flächen verschiedener Teile.
  - `Modelle\td_ao_sichtbar.py` misst AO-Löcher auf sichtbaren Flächen.

**Warum:** Offene Punkte des Optik-Durchgangs 16.11.2026 (Bericht `Satisfactory\Notizen\Optik-Durchgang-2026-11-16.md`, Abschnitt 8). Die AO-Löcher sind der verbliebene Teil von Befund 2: dunkle Flächen am Sockel.

**Hinweis alte Spielstände:** Das Stromkabel eines vorhandenen Tors kann am alten Ort enden und muss eventuell neu gezogen werden.

**Changelog EN:** Defense gate: power socket built into the pillar, status light seated on its mount. Darker patches on towers and buildings removed (ambient occlusion re-baked).

**Changelog DE:** Tor: Stromanschluss in der Säule, Statuslampe sitzt auf ihrem Sockel. Dunkle Flecken an Türmen und Bauten entfernt (Umgebungsverdeckung neu gebacken).
