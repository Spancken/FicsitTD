---
titel: Schwarze Platten neben Türmen entfernt, Optik-Prüfung in der Pipeline
typ: behoben
modi: alle
sichtbar: ja
doku: nicht-noetig
simulation: nicht-noetig
tests: Optik
experiment:
---

**Was:**
- Behoben: Alle 19 Baupläne, die von Spielgebäuden kopiert sind (Konstruktor, Aussichtsturm, Lager), hatten deren Dekorations-Vorlage `mDecoratorClass` geerbt.
  - Diese Vorlage stellt zur Laufzeit dunkle 2 × 2 m große „InputFogPlane“-Flächen an die Ein- und Ausgänge des Vorbilds, also neben unsere kleineren Sockel.
  - `Scripts\entferne_nebelflaechen.py` entfernt sie. `setup_turret_blueprint.py` erledigt das künftig beim Neuaufbau selbst.
- Neu: Optik-Prüfungen.
  - `Tests\pruefe_optik_statisch.py` ist als Pipeline-Schritt „Optik statisch“ eingebunden, mit Sperrklinke `Tests\optik_basis.json`.
  - `Scripts\pruefe_optik.py` läuft als Editor-Commandlet und prüft die Baupläne nur lesend.
  - Neuer Testplan `Optik` mit einer Fotostrecke aller Türme und Bauten.

**Warum:** Optik-Durchgang 16.11.2026, Befund von Marius: eine schwebende schwarze Platte am Bolzenturm, offen seit 0.5. Ursachen und Plan stehen in `Satisfactory\Notizen\Optik-Durchgang-2026-11-16.md` (U1). Im Spiel-Log fanden sich je Gebäude vier `InputFogPlane` an den Stellen der Konstruktor-Öffnungen.

**Changelog EN:** Removed the floating black panels next to towers and buildings (inherited fog planes of the copied vanilla buildings).

**Changelog DE:** Schwebende schwarze Platten neben Türmen und Bauten entfernt (geerbte Nebelflächen der kopierten Spielgebäude).
