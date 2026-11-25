---
titel: Simulator – Modell-Spieler spielt wie ein guter Spieler (Schalter Spielerwahl)
typ: intern
modi: einsatz, endlos
sichtbar: nein
doku: ja
simulation: ja
tests: keine
experiment:
---

**Was** (Marius 22.11.: „Erst Modell nachziehen“, Analyse `Satisfactory/Notizen/Analyse-A6-Simulation-2026-11-22.md`):
- `Tools/Simulator/strategien.py`, `sim_modi.py`, neuer Schalter `Spielerwahl` in `strategie_annahmen.json`. Standard ist 1, mit 0 kommt das alte Verhalten zurück.
- **Forschung nach Bedarf:** Kupfer und Kohlekraftwerk nur, wenn die Linie sie braucht. Das Kohlekraftwerk für die Fabrik kommt nach Turmfertigung II.
- **Strom nach Kosten:** Kohlestrom nur, wenn er billiger ist oder die Biomasse nicht reicht. Im Endlos-Modus zählen zuerst die Gebäude, also Bauzeit.
- **Munition nur, wenn sie sich rechnet:** Der Schadensgewinn je Aufwand muss mindestens so groß sein wie ein weiterer Turm. Das gilt auch für den ersten Wechsel weg von der Grundmunition.
- Simulator-README: neuer Abschnitt „Modell-Spieler guter Spieler“.

**Warum:** Der gierige Modell-Spieler nutzte jede Forschung sofort. Mit dem früheren Einstieg aus A6 baute er Kohlestrom und Munitionsketten statt Türme. Die Simulation zeigte deshalb einen viel härteren Einsatz, als ein guter Spieler ihn erleben würde (Regel „Prüfstand wie guter Spieler“).

**Changelog EN:** –

**Changelog DE:** –

**Nachweis (22.11. spätabends, ohne Spiel):**
- Kontrolle: Mit `Spielerwahl` 0 trifft `sim_alle_modi.py` die Zahlen vom Nachmittag genau.
- Mit `Spielerwahl` 1:
  - A6-Baum: Einsatz Hart 58 → 86 %, Albtraum 38 → 76 %.
  - Alter Baum: Hart 85 → 93 %.
  - Endlos Hart bleibt bei W11,5. Ursache ist der Gewehrturm ab Welle 3: Mischlinien bauen ihn sofort, samt Kupferkette.
  - Gegenversuch Gewehrturm ab 5: Einsatz 90/81 %, Endlos Hart W14. Ab 6: 91/83 %, W16.
- Entscheidung über die Freigabewelle des Gewehrturms: Marius.
