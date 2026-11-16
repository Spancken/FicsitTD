---
titel: Statuslampe und Stromanschluss im Sockel, Klarraum aus dem Modell
typ: behoben
modi: alle
sichtbar: ja
doku: nicht-noetig
simulation: nicht-noetig
tests: Optik
experiment:
---

**Was:**
- **Statuslampe:** Sie ist jetzt ein Lampenkopf (`SM_TD_StatusLight`, 24 cm) auf einer Konsole im Sockelmodell.
  - Bisher hing ein 88-cm-Lampenpfosten auf 3,40 m neben dem Original-Mast `SM_ProductionLight_01`.
  - Dessen eigene Leuchte ging nie an.
- **Stromanschluss:** Er ist ein Anschlusskasten in der Sockelwand und ragt 24 cm heraus, bisher ragte der Wand-Strommast `SM_PowerPoleWall_MK1` bis 1 m heraus.
  - Teslaspule: Weil der Sockelblock nur 66 cm hoch und ab Level 2 voll belegt ist, gibt es eine Anschlusssäule auf dem Deck.
  - Der alte Faltenbalg-Pfosten ist entfernt.
- **Lage:** sucht `Modelle\td_anbauten.py` per Strahltest und Überschneidungsprüfung gegen alle Sockel- und Stufenteile bis Level 5.
- **Klarraum:** Er umfasst jetzt Sockel, Anbauten und Original-Eingang (`Modelle\td_klarraum.py`) und ist asymmetrisch: Nur die Seite mit dem Eingang wächst.
- **Eine Datenquelle:** `Modelle\Anbauten.json` → `python Scripts\erzeuge_anbauten.py` → `TDAnbauten.inl` und `Tests\Anbauten.json`.
  - `ATDTurretBase::UebernimmAnbauDaten` übernimmt Lampe, Strom und Klarraum.
  - Die Handtabellen in den Konstruktoren (Wandmast, Statusmast, `mLampenOrt`) sind entfernt.
- **Gummi-Albedo:** 0,036 statt 0,012 (Spielwert). Alle 14 Gebäude und die Lampe sind neu gebacken und importiert.
- **Prüfungen:**
  - `Tests\pruefe_optik_statisch.py` prüft Anbauten gegen `Anbauten.json` und ob die erzeugte Tabelle aktuell ist.
  - Die Sperrklinke ist von 98 auf 3 bekannte Fehler verkleinert (Tor, Reparaturstation).

**Warum:** Optik-Durchgang 16.11.2026. Befunde 1, 3 und 4 von Marius: Lampe in der Luft, „drangeklatschter“ Stromanschluss, Eingänge außerhalb der Standfläche. Dazu dunkle Sockelflächen. Entscheidungen Marius: eigener Lampenträger, Anschlusskasten im Sockel, Einlaufblock (bei Singularität, Drohnenplatz und Lockfeuer seit 0.9.3 vorhanden). Bericht: `Satisfactory\Notizen\Optik-Durchgang-2026-11-16.md`.

**Hinweis alte Spielstände:** Der Stromanschluss sitzt woanders. Ein vorhandenes Kabel kann am alten Ort enden und muss eventuell neu gezogen werden.

**Changelog EN:** Towers and buildings: status light and power socket are now part of the base (no floating lamp, no protruding power pole), build clearance includes the conveyor input, darker panels brightened.

**Changelog DE:** Türme und Bauten: Statuslampe und Stromanschluss sitzen im Sockel (keine schwebende Lampe, kein abstehender Strommast), der Bauabstand berücksichtigt den Bandeingang, zu dunkle Flächen aufgehellt.
