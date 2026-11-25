---
titel: Druck nach Welle 31, Handwaffe früh schwächer und spät stärker, zwei Talentpunkte je Bosswelle
typ: balance
modi: einsatz
sichtbar: ja
doku: ja
simulation: ja
tests: Grundtest, EinsatzPhase, Pionier-P3
experiment:
---

**Was:**
- **Gegnerleben:**
  - Zuwachs je Welle 0,35 → 0,28 bis W31, ab W31 0,1 → 1,5 (`Wellen.LPJeWelle`, `Wellen.LPJeWelleAb31`).
  - Endlos behält 0,1 ab W31 (`Endlos.LPJeWelleAb31`).
  - Stufen: Normal 1,2 → 1,25, Hart 1,2 → 1,25, Albtraum 1,3 → 1,35. Endlos gleicht das über `Endlos.<Stufe>.LPFaktor` aus (0,8 / 0,8 / 0,8148).
- **Handwaffen:**
  - `Pionier.WellenAnteil` 0,5 → 3.
  - `Pionier.HandFaktor`: Armierpistole 0,3, Gewehr 0,35, Nobelisk 0,5 (vorher je 1).
- **Talentpunkte:** `Pionier.PunkteJeBoss` 1 → 2. Auch Welle 50 gibt ihre Punkte (`TDWaveSubsystem.cpp`, vor dem Sieg). Im Einsatz sind das 10 Punkte, Stufe 5 ist damit erreichbar.

**Warum:** Gegencheck 0.13 Konzept, Antworten Marius am 23.11.:
- Frage 1: Auch zwischen W31 und W50 soll jemand scheitern.
- Frage 2: Balance für den passiven Spieler, Handwaffe früh dämpfen, spät stärken.
- Frage 3: Rollen sollen im Einsatz spürbar sein.

**Simulation** (`Saved\Pipeline\Sim-Gegencheck-2026-11-23`, mit Fliegern ab W43 und korrigiertem Spielermodell):

| Einsatz | passiv | aktiv |
|---|---|---|
| Normal | 50 % | 80 % |
| Hart | 32 % | 67 % |
| Albtraum | 20 % | 59 % |

- Siege bis W30: 71/51/37 %. Vorher fielen fast alle Niederlagen bis W20 (Normal: 87 bis W20, 6 nach W30).
- Spieleranteil (aktiv): Median 12 %, 8–24 %. Vorher 29 % in W1–5 und 4 % ab W31.
- Endlos ist unverändert: Normal W13,5, Erkundung kappt in 13 % der Läufe.

**Changelog EN:** Harder second half: enemy health grows much faster from wave 31. Hand weapons start weaker and grow stronger, your share stays around 10–20 %. Two Talent Points per boss wave, including wave 50.

**Changelog DE:** Schwerere zweite Hälfte: Die Gegnerleben wachsen ab Welle 31 deutlich schneller. Handwaffen anfangs schwächer, später stärker, dein Anteil bleibt bei etwa 10–20 %. Zwei Talentpunkte je Bosswelle, auch für Welle 50.

**Nachweis:** Simulation siehe oben. Build 23.11. nachmittags. Selbsttest `Release` 29/30 (`2026-11-23_1726`, einziger Fehler: Prüftext in `EinsatzPhase` erwartete noch „+1 Talentpunkt“, angepasst, Mehrzahl in der Meldung behoben), `ReleaseEn` 10/10 (`_1741`), `Pionier-P3` grün.
