---
titel: Simulator – Kohlekraftwerk bei knappem Strom vorziehen, Kern-Tief-Band Normal 25 %, Endlos-Diagnose
typ: intern
modi: einsatz, endlos
sichtbar: nein
doku: nicht-noetig
simulation: ja
tests: Grundtest
experiment:
---

**Was:**
- **Spielermodell (`strategien.py`, `strom_knapp`, Annahme `StromVorlaufWellen` 3):** Der gute Spieler (Spielerwahl) zieht das Kohlekraftwerk in der Forschung vor, sobald die Biomasse nicht mehr für 3 Wellen reicht. Vorher stand es fest hinter Turmfertigung II.
  - Gewehr+Nobelisk erforschte es so in 0 von 24 Läufen, die Biomasse ging aus, und der Strom fehlte.
  - Jetzt steht Gewehr+Nobelisk im Einsatz Normal bei 50 % statt 4 %. Einsatz gesamt 56/36/21 %, alle Einsatzbänder sind erfüllt.
- **Varianten zu Gewehr+Nobelisk** (`varianten_gewehr_nobelisk.py`, Entscheidung 2D):
  - B, je Nobelisk ein Schuss mehr: keine Wirkung. Selbst ×10 Schuss ändert die Siege nicht.
  - C, Kombi-Rabatt 25 % auf Fabrikgebäude: bringt Gewehr+Nobelisk nichts, hilft nur Gewehr+Flamme (79 → 88 %).
  - Die Munition war also nie der Engpass, sondern der Strom.
- **Zielband Kern-Tiefpunkt Normal** 50 → 25 % (`sim_baender.json`, `Tests/zielbaender.json`), vorläufig bis zum Handspiel (Marius 3A).
- **Endlos-Diagnose** (`diagnose_endlos.py`, Entscheidung 4): Jede Variante nimmt genau einen Druck weg. Median-Welle Normal / Hart / Albtraum:

| Variante | Normal | Hart | Albtraum |
|---|---|---|---|
| heute | W13 | W12,5 | W14,5 |
| Gegnerleben ×0,5 | W18,5 | W17 | W16 |
| Coupons ×3 | W16,5 | W17 | W16 |
| Fabrikgebäude kostenlos | W18 | W16,5 | W17 |
| Kern-LP ×2 | W17,5 | W17 | W18,5 |
| Bauzeit je Gebäude halb | W123 | W97 | W91 |
| Bauzeit ansparen ohne Grenze | W94 | W28 | W16 |

- **Ursache:** Es fehlt Bauzeit. Ab etwa W5 stockt der Ausbau bei 3–4 Türmen, obwohl über 200 Coupons übrig sind.
  - Ein weiterer Turm braucht mit seiner Fabrik-Erweiterung 5–6 Gebäude à 117 s (`BauSekundenJeGebaeude`).
  - Je Welle stehen Pause plus halbe Welle plus höchstens 300 s Angespartes zur Verfügung (`BauzeitSpeicherSekunden`). Das reicht für etwa 4 Gebäude.
  - Gegner, Munition und Kern sind Nebenursachen: Jede einzelne Änderung dort bringt nur 2–6 Wellen.
- **Modellgrenze:** Ob die 300-s-Grenze stimmt, ist offen. Ein echter Spieler baut die Fabrik vor, wenn Coupons übrig sind. Ohne Grenze ist Normal mit W94 eher zu leicht.

**Warum:** Marius am 23.11.: „Testen, was das Problem ist. Es soll schwer sein, aber nicht unschaffbar. Liegt es an den Gegnern, der Zeit oder der Munition?“

**Noch offen:** Marius entscheidet, wie die Bauzeit im Endlos modelliert wird (Handspiel-Messung), danach eicht der Simulator Endlos je Stufe neu.
