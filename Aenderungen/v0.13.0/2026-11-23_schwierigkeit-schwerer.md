---
titel: Einsatz schwerer – Normal, Hart und Albtraum mit mehr Gegnerleben
typ: balance
modi: einsatz
sichtbar: ja
doku: ja
simulation: ja
tests: Grundtest, EinsatzPhase
experiment:
---

**Was:**
- Gegnerleben wachsen im Einsatz schneller: `Wellen.LPJeWelle` 0,25 → 0,35 (bis Welle 31).
- Normal: `Stufen.Normal.GegnerLP` 1,0 → 1,2, `Stufen.Normal.KernLP` 20 000 → 15 000.
- Hart: `Stufen.Hart.GegnerLP` 1,0 → 1,2.
- Albtraum: `Stufen.Albtraum.GegnerLP` 1,1 → 1,3.
- Erkundung bleibt unverändert.
- Endlos bleibt auf dem Stand vor dem 23.11.:
  - eigener Zuwachs `Endlos.LPJeWelle` 0,25
  - Ausgleich je Stufe `Endlos.<Stufe>.LPFaktor` (Normal/Hart 0,8333 = 1/1,2, Albtraum 0,8462 = 1,1/1,3)
  - Code in `TDWaveSubsystem.cpp` und `simulator.py`
  - Nur der kleinere Normal-Kern wirkt auch im Endlos.

**Warum:** Marius am 23.11.: „Es sollte schon schwerer sein, also Normal und vor allem Hart und Albtraum.“ Laut Prüfstand (guter Spieler) gewann man vorher Normal zu 100 %, Hart zu 90 % und Albtraum zu 81 %. Alle drei Stufen lagen über ihrem Band.

**Simulation (Prüfstand guter Spieler, 228 Läufe je Stufe):**

| Einsatz | vorher | nachher | Band |
|---|---|---|---|
| Normal | 100 % | 50 % | 30–65 % |
| Hart | 90 % | 35 % | 15–55 % |
| Albtraum | 81 % | 20 % | 5–35 % |

- Scheitern liegt im Median bei Normal W17, Hart W12, Albtraum W9.
- Endlos (Median-Welle): Erkundung W31,5, Normal W13, Hart W12,5, Albtraum W14,5.

**Noch offen (Entscheidung Marius):**
- Kern-Tiefpunkt Normal: 29 % statt ≥ 50 %. Mit 20 000 bzw. 25 000 Kern-LP bleibt er bei 32–37 %. Der Kern leidet am Buckel um W15–20, das Band „≥ 50 %“ passt nicht zu 50 % Siegen.
- Gewehr+Nobelisk ist im Einsatz fast tot: Normal 4 %, Hart 4 %, Albtraum 0 %. Zwei getrennte Munitionsketten kosten bei W17 rund die Hälfte der Coupons für die Fabrik (Gewehr+Flamme rund 15 %).
- Endlos Normal liegt mit W13 unter dem Band W20–60. Vorher lag es bei W16, also auch schon darunter. Ursache ist der kleinere Normal-Kern.

**Changelog EN:** Harder difficulty: enemies on Normal, Hard and Nightmare have more health and gain it faster per wave, and the Defense Core on Normal has 15,000 HP. Endless waves stay as before.

**Changelog DE:** Mehr Herausforderung: Gegner haben in Normal, Hart und Albtraum mehr Leben und legen je Welle schneller zu, der Abwehrkern hat in Normal 15 000 LP. Die Endlos-Wellen bleiben wie bisher.

**Nachweis:**
- Simulation `Saved\Pipeline\Sim-Schwer-2026-11-23\sim_ergebnis.md`, bestätigt in der Pipeline `2026-11-23_0856`.
- Selbsttest `Release` (`2026-11-23_0854`) mit den neuen Werten: Grundtest und EinsatzPhase grün, Prüfstand im Spiel 32 Wellen.
- Spielgefühl ist noch nicht geprüft, das Handspiel macht Marius.
