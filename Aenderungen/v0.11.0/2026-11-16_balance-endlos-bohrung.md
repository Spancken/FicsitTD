---
titel: Balance Endlos-Wellen und Bohrung nach Simulation
typ: balance
modi: endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: Endlos, Bohrung
---

**Was (Fahrplan P2/P3, 16.11.):**
- Endlos: Gegnerleben wachsen ab Welle 50 je Welle um 3 % (zinsartig, `Endlos.LPWachstum`, `Endlos.LPWachstumAbWelle`) – vorher gab es nach W50 keinen Druck mehr (Gegnerzahl gedeckelt, Coupons wachsen weiter), die Hälfte der Läufe erreichte die Rechengrenze. Budget je Welle × 0,9 (`Endlos.BudgetFaktor`). Normal: Abstand 180 → 220 s.
- Bohrung: Kern repariert sich nach jeder Bohrwelle um 10 % (`Bohrung.Reparatur`), Kern-LP Mittel 12 000 → 18 000, Schwer 16 000 → 24 000.

**Simulation (`Tools\Simulator`, Varianten: `varianten_endlos.py`, `varianten_bohrung.py`):** Endlos Erkundung W24 → W31,5 (im Band), Kappung 0 %; Normal W12,5 → W14 (Band W20–60 noch nicht erreicht). Bohrung Mittel 33 → 19 Bolzentürme L2, Schwer 57 → 29 (beide im Band).

**Simulator-Korrekturen dabei:** Überlauf trug alle Gegner mit anteiligem Leben weiter (Liste wuchs auf Tausende, die Streckung löste den Druck auf) – jetzt ein Anteil der Gegner mit vollem Leben, höchstens `Wellen.MaxGleichzeitig`. Rechengrenze der Pipeline 100 → 150 Wellen.

**Offen:** Endlos Normal/Hart/Albtraum und Einsatz Hart/Albtraum scheitern im Modell am Wellenberg ab W11 (Elite, Stinger) – das ist der Wellenaufbau selbst (Fahrplan P6, erst im Spiel messen). Flammenwerfer schafft im Modell jede Bohrstufe mit 1–2 Türmen (geschätzter Flächenfaktor, im Spiel prüfen). Alle Werte im Spiel noch nicht gemessen (Fenster B).

**Changelog EN:** Endless waves: enemies keep getting tougher after wave 50 so there is always a best run to beat; slightly smaller waves and more time between waves on Normal. Drilling: the drill repairs itself between waves, more HP on Medium and Hard.

**Changelog DE:** Endlos-Wellen: Gegner werden nach Welle 50 weiter stärker, Wellen etwas kleiner, auf Normal mehr Zeit zwischen den Wellen. Bohrung: Der Bohrer repariert sich zwischen den Wellen, mehr Leben auf Mittel und Schwer.
