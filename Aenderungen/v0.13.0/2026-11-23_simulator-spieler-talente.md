---
titel: Simulator – aktiver Spieler und Talentbaum (Schalter SpielerAktiv), Release-Vorbereitung 0.13.0
typ: intern
modi: einsatz, endlos
sichtbar: nein
doku: ja
simulation: ja
tests: Grundtest
experiment:
---

**Was:**
- **Simulator (`strategien.py`, `sim_modi.py`):**
  - Neuer Schalter `SpielerAktiv`, Standard 0. Die Balance gilt für den passiven Spieler (Marius 12.11.).
  - Mit 1 kämpft die Handwaffe des Spielers als eigener „Turm“ mit: `SpielerDPS` 30, wächst mit `Pionier.WellenAnteil` wie `TDPionier.cpp`.
  - Je geschaffter Bosswelle gibt es einen Talentpunkt nach `TalentPlan` (höchstens 15).
  - Wirkung der vier Zweige:
    - Schütze: Handwaffe
    - Kommandant: Markierung +15 % auf den markierten Anteil
    - Techniker: Übersteuern
    - Jäger: Elite, Boss, Schwachstelle
  - Alle Annahmen stehen in `strategie_annahmen.json`.
- **`sim_alle_modi.py`:** Zusatztabelle „Einsatz mit aktivem Spieler“ mit Siegquote und Spieleranteil, ohne Band.
- **Release-Vorbereitung 0.13.0:**
  - Version in `FicsitTD.uplugin`
  - Handbuch (Abzeichen, Versionsabschnitt, bekannte Fehler, Baum Energie)
  - Mod-Seite („What's new in 0.13“)
  - Rückmeldebogen (Fragen 15a–15c)
  - Simulator-README

**Warum:** Marius am 23.11.: „Bereite alles für das Release vor, bzw. baue in den Simulator auch den neuen Talentbaum ein und die neue Stromversorgung. Freitag brauchen wir ein pushbares Release.“

**Simulation (`Saved\Pipeline\Sim-Release-2026-11-23`):**

| Einsatz | passiv | aktiver Spieler | Spieleranteil (Median) |
|---|---|---|---|
| Normal | 55 % | 88 % | 6 % |
| Hart | 37 % | 77 % | 7 % |
| Albtraum | 20 % | 65 % | 8 % |

- Die Talente bringen davon nur etwa einen Prozentpunkt (Normal 87 % ohne Talente).
- Die Wirkung kommt von der Handwaffe in den frühen Wellen: Ein Spieler, der nur bis W10 aktiv ist, hebt Normal schon auf 79 %.
- Endlos ist unverändert (Normal W13, Erkundung Kappung 12 %).

**Noch offen (Marius):**
- Soll die Balance für den passiven Spieler bleiben, obwohl ein aktiver Spieler sie stark verschiebt?
- Ist der Spieleranteil von 6–8 % (Ziel 10–20 %) richtig gemessen? Das zeigt die Statistik im Handspiel.

**Nachweis:** Simulation siehe oben. Kein Spieltest, weil der Rechner belegt war. Die neue Version 0.13.0 muss vor dem nächsten Selbsttest gebaut werden (Prüfschritt `Version`).
