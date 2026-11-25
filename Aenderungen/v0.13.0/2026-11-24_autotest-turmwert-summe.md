---
titel: Selbsttest TurmWert zählt auf Wunsch über alle Türme eines Typs
typ: intern
modi: alle
sichtbar: nein
doku: nicht-noetig
simulation: nicht-noetig
tests: Flieger
experiment:
---

**Was:**
- Die Prüfung `TurmWert` kennt das Feld `"summe": true`. Dann zählt sie den Wert über alle Türme des Typs zusammen, nicht nur am ersten. Im Ergebnis steht „(Summe über n Türme)“.
- Im Plan `Flieger` nutzt „Himmelsturm trifft Flieger“ jetzt die Summe.

**Warum:** In der Release-Regression vom 24.11. (`2026-11-24_1919_Release`) meldete der Flieger-Plan 0 Treffer. Turm 3 hatte 3-mal getroffen (720 Schaden), Turm 1 gar nicht, und gemessen wurde nur Turm 1. Die Flieger kommen je Lauf aus einer anderen Richtung. Es war also ein Fehler im Test, nicht im Spiel.

**Changelog EN:** –

**Changelog DE:** –
