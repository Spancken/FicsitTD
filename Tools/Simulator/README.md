# FicsitTD-Simulator

Rechnet ohne Spiel, wie viel Gegnerdruck auf wie viel Abwehr trifft, für alle Spielmodi. Die Kennzahl ist die Druckquote **Q** (wie im Log: Gegner-EHP / (Σ Nenn-DPS × 60 s)). Nur Python-Standardbibliothek. Liest den Mod-Code direkt aus `Source\FicsitTD\Private` (Pfade relativ zum Repo), Änderungen an der Balance wirken also ohne Anpassung.

## Stand 23.11.2026 (Code 0.13.0: Strom, aktiver Spieler, Talente; davor 16.11. Turmlevel, Coupons, Festplatten)

### Aufruf

```bash
python sim_alle_modi.py --aus Ausgabe\Pipeline     # alle Modi und Stufen, etwa 30 s
python simulator.py [--stufe Normal]               # Q-Kurven Einsatz, Ausgabe\Simulation.html/.csv (etwa 40 s alle Stufen)
python strategien.py --stand 091 --laeufe 3        # Strategie-Prüfstand Einsatz (091 = heutiger Forschungsbaum)
```

`sim_alle_modi.py` ist die Schnittstelle für die Pipeline:

- `<ordner>\sim_ergebnis.json`: je Modus und Stufe `kennzahlen`, `pruefungen` (je Band `ok` und Wert), `ok`; oben `ok` gesamt, `code_stand` (SHA-256 je Quelldatei), `balance`, `laufzeit_s`.
- `<ordner>\sim_ergebnis.md`: Tabellen je Modus und Stufe, Liste der Bandverletzungen.
- `Tools\Simulator\sim_stand.json` (einchecken): Datum und dieselben Hashes. Weicht ein Hash vom aktuellen Code ab, ist die Simulation nicht nachgezogen.
- Gehasht werden `TDBalance.cpp`, `TDWaveSubsystem.cpp`, `TDEinsatz.cpp`, `TDBohrung.cpp`, `TDTurretBase.cpp`, `TDForschungInfo.inl`, `TDForschungVoraussetzungen.inl` und `Scripts\create_research.py` (daraus liest der Prüfstand den Forschungsbaum).
- Exit **0** alles im Band, **1** mindestens ein Band verletzt, **2** Fehler: das Modell passt nicht zum Code. Vor dem Rechnen prüft `pruefe_modell`: Forschungskosten und Freigabewellen aus `create_research.py` gleich `TDForschungInfo.inl` (nach Balance-Override), Level-Pakete in `TDTurretBase.cpp` lesbar, alle nötigen Balance-Schlüssel (Stufen, Endlos, Bohrung, Level, Coupons) vorhanden, Munitionswerte vorhanden, Gegnerkatalog und 5 Phasen gelesen, Grund-LP für jede Art, Endlos-Standardwerte in `TDEinsatz.cpp Starte` unverändert. Im Fehlerfall steht in `sim_ergebnis.json` nur `fehler` und `trace`, `sim_stand.json` bleibt unverändert.

### Modi

| Modus | Modell | Datei |
|---|---|---|
| 1 Einsatz | Strategie-Prüfstand, 228 Strategien (jede zweite aus `strategien.strategien`) × 1 Startwert, 50 Wellen, Fronten je Stufe | `strategien.py` |
| 2 Endlos-Wellen | Prüfstand-Spieler auf Zeit: 10 Linien × ohne/mit Vorziehen, Rechengrenze W100 | `sim_modi.py` (`spiele_endlos`) |
| 3 Freies Spiel mit Abwehr, Bohrung | fester Aufbau (Typ, Level, Munition), kleinste Turmzahl für 80 % Erfolg über 4 Wellensätze | `sim_modi.py` (`bohrung_lauf`) |
| 0 Aus | nichts zu rechnen | – |

**Endlos** (`TDWaveSubsystem.cpp` `EndlosAbstand`, Wellenende in `LaufTick`, `NaechsteWelleJetzt`; `TDEinsatz.cpp` `Starte`, `WaehleEndlosBonus`; `Endlos.*` in `TDBalance.cpp`):

- Erste Welle nach `Endlos.<Stufe>.ErsteWelle`, Abstand ab Wellenbeginn `A(n) = max(90, Abstand − 1,5 · n)`.
- Wellendauer im Modell = `EndlosAnlaufSekunden` + max(Gegnerstrom, Q_wirksam × 60 s / `EndlosFeuerAnteil`). Gegnerstrom wie im Code: 2/s, höchstens `Endlos.StromAnteil` × A.
- Früher fertig: Pause = max(30 s, A − Dauer). Nicht fertig bis A: Überlauf, der Rest-Anteil der Gegner-LP kommt zur nächsten Welle, Pause 5 s. Nach Bosswellen mindestens 300 s und Bonus: unter 60 % Kern das Kern-Maximum dauerhaft +10 % (`Einsatz.Bonus.KernMax`, wie `TDDefenseCore.cpp`, ohne Heilung; bis 23.11. heilte das Modell fälschlich 30 %), sonst +80 Coupons.
- Coupons und Festplatten je Welle wie im Einsatz (`WelleBeendet` läuft auch im Endlos-Modus), eine Front.
- **Bauzeit:** keine Bausperre, aber jedes Kontingent (Turm, Fabrikgebäude, Kraftwerk, Stützbau) kostet `BauSekundenJeGebaeude` = 111 s. Verfügbar: Pause + 50 % der Wellenzeit, Rest bis 300 s mitgenommen. Level im Turmfenster kosten keine Bauzeit. Fabrikzyklus = tatsächlicher Wellenabstand (kürzer als im Einsatz, also mehr Munition je Minute nötig).
- Vorziehen-Variante: bei lockerer Welle (Q < 0,25, Kern ≥ 90 %) wird ungenutzte Bauzeit gegen 1 Coupon je 10 s eingetauscht.
- Ende bei Kern 0, Wertung = letzte geschaffte Welle. Kennzahlen: Median-Welle, „kippt ab“ (erste Welle, die weniger als die Hälfte der Linien schafft), erster Überlauf, Nenn-Q je Welle, Anteil an der Rechengrenze.

**Bohrung** (`TDBohrung.cpp` `StarteBohrung`, `BohrWelleBeendet`; `TDEinsatz.h` `SetzeAbwehrKern`; `Bohrung.*`):

- Wellen `VonWelle..BisWelle` (Leicht 1–4, Mittel 15–19, Schwer 25–30) mit den Werten der Stufe **Normal** (`StarteKernVerteidigung(..., 1)`), Kern-LP 8000/12000/16000.
- Der Einsatz ist dabei nicht aktiv: keine Coupons, **keine Grundreparatur** zwischen den Wellen. Der Kernverlust summiert sich über alle Wellen.
- Geprüft wird ein fester Aufbau ohne Kern-Ausbau, Doktrin und Stützbauten, Munition voll versorgt. Ausgabe: Mindestzahl Türme je Aufbau und Stufe; Erz und Erstlohn stehen nur als Kennzahl dabei.

### Stand 23.11.2026: Strom, aktiver Spieler und Talentbaum

- **Strom (Baum Energie, `TDStrom.cpp`):**
  - `Wirtschaft.varianten` liefert je erforschter Linie Kraftwerk und Brennstoff: Kohle, Kompaktkohle, Petrolkoks, Treibstoff, Turbo- und Raketentreibstoff.
  - Die MW je Kraftwerk kommen aus `Strom.<Brennstoff>.Faktor`, der Brennstoff je Minute ist MW × 60 / Brennwert.
  - `Lauf.fabrik` wählt die billigste Variante: im Einsatz nach Coupons, im Endlos nach Bauzeit.
  - Der Ausbau (Kompaktkohle, Petrolkoks, Treibstoffkraftwerk, Turbo-Feuerung) beginnt ab `StromAusbauAbKraftwerken` (4).
  - Das Kohlekraftwerk kommt vorgezogen, sobald die Biomasse knapp wird (`strom_knapp`, `StromVorlaufWellen`).
  - Das Treibstoffkraftwerk liefert 250 MW (vorher fälschlich 150).
- **Aktiver Spieler und Talente** (Schalter `SpielerAktiv`, Standard 0):
  - Die Balance gilt für den passiven Spieler (Marius 12.11.).
  - Mit `SpielerAktiv` 1 kämpft die Handwaffe als eigener „Turm“ mit: `SpielerDPS` 30, wächst mit `Pionier.WellenAnteil` wie im Mod.
  - Je geschaffter Bosswelle gibt es einen Talentpunkt nach `TalentPlan`, höchstens 15.
  - Wirkung der Zweige:
    - Schütze: Kopftreffer, Schnellladen, Durchschlag, Sprengmantel
    - Kommandant: Markierung +15 % auf den markierten Anteil
    - Techniker: Übersteuern
    - Jäger: ×1,5 gegen Elite und Boss, Schwachstelle im Boss
  - Die Annahmen stehen in `strategie_annahmen.json`.
  - `sim_alle_modi.py` zeigt eine Zusatztabelle „Einsatz mit aktivem Spieler“ (Siegquote und Spieleranteil, ohne Band).
- **Flieger (23.11. nachmittags):**
  - Ab W43 kommen Krabben nach `Flieger.*`.
  - Der gute Spieler forscht den Himmelsturm nach Turmfertigung III und stellt je 8 Krabben einen auf (`FliegerJeHimmelsturm`, Preis wie im Laden).
  - Nicht abgefangene Krabben treffen den Kern mit `Flieger.KernSchaden` × (1 + 0,25 × Wellenzuwachs).
  - Annahme: Ein Himmelsturm fängt 8 Krabben je Welle. Im Spiel ist das noch ungemessen (Plan `Flieger`).
- **Später Druck (Gegencheck Konzept B1):** `Wellen.LPJeWelleAb31` 1,5, Endlos eigener Wert (`Endlos.LPJeWelleAb31` 0,1).
- **Werkzeuge:**
  - `eiche_stufen.py`, `eiche_endlos.py` (Eichen je Stufe)
  - `diagnose_endlos.py` (woran Endlos scheitert)
  - `varianten_gewehr_nobelisk.py`

### Modell-Spieler „guter Spieler“ (22.11.2026, Schalter `Spielerwahl`)

Anlass: Nach dem A6-Umbau des Baums rechnete die Simulation den Einsatz viel schwerer. Der Grund war der gierige Modell-Spieler, der jede Forschung sofort nutzte (`Notizen/Analyse-A6-Simulation-2026-11-22.md`). Mit `Spielerwahl` 1 in `strategie_annahmen.json` (Standard) spielt er wie ein guter Spieler. Mit 0 kommt das alte Verhalten zurück.

- **Forschung nach Bedarf** (`Lauf._forschungsplan`): Kupfer und Kohlekraftwerk nur, wenn die Linie sie braucht (Voraussetzungen reiht `add()` selbst davor). Das Kohlekraftwerk für die Fabrik kommt nach Turmfertigung II.
- **Strom nach Kosten** (`Lauf.fabrik`): Mit Kohlekraftwerk wählt er Kohlestrom nur, wenn die Biomasse für die Welle nicht reicht oder Kohle billiger ist. Im Einsatz zählen zuerst Coupons, im Endlos-Modus zuerst die Gebäude, also Bauzeit (`fabrik_schluessel`).
- **Munition nur, wenn sie sich rechnet** (`Lauf.stelle_munition_um`): Der Schadensgewinn an allen Türmen des Typs je Aufwand muss mindestens so groß sein wie ein weiterer Turm mit der bisherigen Munition. Aufwand heißt im Einsatz Coupons, im Endlos-Modus Gebäude (`aufwand`). Das gilt auch für den ersten Wechsel weg von der Grundmunition.
- Wirkung (A6-Baum): Einsatz Hart 58 → 86 %, Albtraum 38 → 76 %, Endlos Hart W11 → W11,5. Im alten Baum steigen die Werte mit dem guten Spieler ebenfalls (Hart 85 → 93 %).
- Noch gierig: Der Spieler baut eine neue Turmart seiner Linie, sobald sie erforscht ist (Gewichte der Linie, `baue()`), samt Munitionskette. Deshalb kostet der Gewehrturm ab Welle 3 die Mischlinien im Endlos-Modus Bauzeit. Mit dem Gewehrturm ab Welle 6 liegt alles auf dem alten Stand.

### Was am Modell geändert wurde (16.11.2026)

- **Turmlevel statt Stufen** (`simulator.py` `Spieler`): Level 1–5, gekauft mit Coupons. Preise `Tuerme.Level.Preis2..5` (Standard aus `ATDTurretBase::LevelPreis`), Doktrin-Rabatt. Wirkung `Tuerme.Level.<Turm>.L<n>.<Wert>` mit Standards aus `TDLevelStandards[]` in `TDTurretBase.cpp` (`LevelWertBis`). Nenn-DPS = Schaden × `SchadenBonus` × `TaktBonus` wie `NennDPS()`. Level-Grenze wie `LevelMax`: 2 mit dem Turm, 3/4 mit Turmfertigung II/III, 5 mit IV oder Doktrin. Der Modell-Spieler wählt je Kauf das Beste aus „neuer Turm“ und „ein Level mehr“ (Nenn-DPS je Coupon). Der alte KeyError `Tuerme.Stufen.SchadenJeStufe` ist damit weg. Der Prüfstand (`strategien.py`) hatte Level schon.
- **Forschung in Festplatten** (`simulator.lade_forschung_info`): Kosten, Freigabewelle, Voraussetzungen und Gabelungen aus `TDForschungInfo.inl`, Balance `Forschung.<Knoten>.AbWelle` geht vor. Die Reihenfolge in `annahmen.json` enthält nur noch Namen (dazu Turmfertigung II–IV). `strategien.lade_alles` übernimmt jetzt ebenfalls die Balance-Freigabewellen (vorher z. B. Teslaspule W6 statt W5).
- **Gegner-LP je Welle** (`simulator.Wellen.lp_faktor`): wie `TDWaveSubsystem.cpp` `1 + LPJeWelle · min(n−1, 30) + LPJeWelleAb31 · max(n−31, 0)`. Vorher wuchs das Leben ohne Knick weiter, ab W31 also zu schnell.
- **Munition** in `annahmen.json` auf den heutigen Stand (kein Biotreibstoff mehr, Raketentreibstoff, Plasmaflamme, Streu-/Schockbolzen, Puls/Streu-Nobelisk). `Turmname` Flammenwerfer für die Level-Schlüssel (Ladenschlüssel heißt Flammenturm).
- **Kernverlust je Stufe** (`strategien.Lauf.kampf`): Die Verlustkurve ist in % eines Normal-Kerns kalibriert (20000 LP). Jetzt × 20000 / Kern-LP der Stufe bzw. Bohrung. Vorher verlor ein Albtraum-Kern (10000 LP) prozentual so viel wie ein Normal-Kern.
- **Nenn-Q im Verlauf** des Prüfstands (7. Feld), damit die Pipeline Q wie im Log gegen `Tests\zielbaender.json` prüft.
- Neu: `sim_modi.py`, `sim_alle_modi.py`, `sim_baender.json`, `sim_stand.json`; neue Annahmen in `strategie_annahmen.json`: `VerlustBezugKernLP`, `BauSekundenJeGebaeude`, `BauAnteilInWelle`, `BauzeitSpeicherSekunden`, `EndlosAnlaufSekunden`, `EndlosFeuerAnteil`, `EndlosBonusKernUnter`, `EndlosVorziehenUnterQ` (jede mit Quelle bzw. „nicht gemessen“).

### Neue Annahmen und ihre Begründung

- **`BauSekundenJeGebaeude` 111 s:** Im Einsatz wartete Marius im Median 195 s je Bauphase (Live 03./04.11.). Der Modell-Spieler kauft im Einsatz Normal W2–30 im Mittel 1,75 Kontingente je Bauphase (`sim_modi.kalibriere_bauzeit`), also 195 / 1,75. Das ist eine Obergrenze, denn die Bauphase enthielt auch Warten und Versorgen. **Endlos reagiert stark darauf** (Normal, Median-Welle der 10 Linien): 0–55 s → W22, 111 s → W12,5, 160 s → W8. Ab etwa 55 s je Gebäude bremst die Bauzeit gar nicht mehr, darunter bestimmen nur Coupons und Fabrikzyklus.
- **`BauzeitSpeicherSekunden` 300 s:** Ohne Mitnahme scheitert jede Linie bis W7, weil ein Paket aus Turm und Miner (2 × 111 s) nie in eine Pause passt. Ein echter Spieler baut über zwei Pausen.
- **`BauAnteilInWelle` 0,5, `EndlosAnlaufSekunden` 20, `EndlosFeuerAnteil` 0,54** (Uptime-Messung im Einsatz): nicht gemessen bzw. aus dem Einsatz übertragen.
- **`EndlosBonusKernUnter` 60, `EndlosVorziehenUnterQ` 0,25:** Spielerverhalten, frei gewählt.

### Zielbänder (`sim_baender.json`, Vorschläge zum Abgleich mit Marius)

- **Einsatz Normal:** Q je Phase aus `Tests\zielbaender.json` (Median-Nenn-Q je Welle, mindestens 70 % der Wellen W1–30 im Band), Kern-Tiefpunkt der Erfolgreichen ≥ 50 % (`Kern_Proz_min`), Zielbild des Prüfstands (mehrere Wege, keine Linie > 90 %, nicht zu leicht, keine tote Kombination), Erfolg 30–65 % (`ZielQuoteMax`).
- **Einsatz andere Stufen:** nur Erfolgsquote; Erkundung ≥ 60 %, Hart 15–55 %, Albtraum 5–35 %. Begründung: Erkundung leichter als Normal, Hart und Albtraum selten, aber nicht unmöglich („mehrere Wege führen zum Sieg“).
- **Endlos:** Median-Welle (ohne Vorziehen) Erkundung W30–99, Normal W20–60, Hart W12–45, Albtraum W8–35; höchstens 10 % der Läufe an der Rechengrenze. Begründung: Bestwert-Jagd, auf Normal soll eine gute Abwehr den zweiten Boss schaffen, mit Zeitdruck aber meist vor der Einsatzlänge fallen; der Modus muss kippen, sonst gibt es keinen Bestwert.
- **Bohrung:** Bolzenturm L2 (Einstiegsturm) braucht Leicht 2–6, Mittel 6–20, Schwer 10–35 Türme; kein Aufbau darf Mittel mit weniger als 3 oder Schwer mit weniger als 5 Türmen schaffen.

### Ergebnis 16.11.2026 (`sim_alle_modi.py`, Exit 1)

Siehe `Ausgabe\Pipeline\sim_ergebnis.md`. Kurz:

- **Einsatz:** Erkundung 100 % Siege (im Band), Normal 76 % (über 65 %), fünf Linien über 90 %, Kern-Tiefpunkt 41 %. **Hart und Albtraum scheitern im Modell immer in W2**: Mit dem einen Start-Bolzenturm und 0 Coupons trifft W1 auf Budget × 2 (34 Punkte, Q_wirksam 0,76 bzw. 0,80).
- **Endlos:** Normal Median W12,5, kippt ab W14 (Einsatz Normal: Scheitern im Median W17). Erster Überlauf um W14. Erkundung W24, aber die Hälfte der Läufe erreicht W100 (die Rechengrenze). Hart/Albtraum W1, wie im Einsatz.
- **Bohrung:** Leicht schafft fast alles mit 1–2 Türmen. Mittel braucht 33 Bolzentürme L2, aber nur 2 Flammenwerfer; Schwer 57 Bolzentürme L2 gegen 3 Flammenwerfer.

### Grenzen des Modells

- **Spieler (seit 23.11.)**: nur mit `SpielerAktiv` 1 (die Bänder gelten für den passiven Spieler).
  - Der Spieler hat die Armierpistole (`SpielerDPSArmier`), das Gewehr erst nach der Forschung Gewehrturm und dem Kauf für 10 Coupons.
  - Er verbraucht keine Munition.
  - Zehn der zwanzig Talente fehlen im Modell (Reparatur, Beute, Anzeige, Bollwerk, Hinrichtung, Nachschub, Munition sparen, Sammelbefehl).
  - Jäger 3 wirkt nur am Bossanteil der Welle, Kommandant 5 nur über `UebersteuernFlaecheFaktor`.
- **Keine echte Fabrik**: Munition kommt aus einer Bilanz bis zum Erz (Kontingente, Strom), ohne Bänder, Wege, Platz und Fehler. In der Bohrung ist die Versorgung immer voll.
- Kein Ort: keine Reichweite, keine Fronten-Aufteilung, keine Laufwege, keine Mauern. Flächenwirkung nur über Faktoren (`Mehrziel_*`, Flamme 2,7 × 2 Ziele: daher der sehr starke Flammenwerfer in der Bohrung).
- Kernverlust aus Q ist eine Kurve aus Einsatz-Messungen bis W30 (R² 0,46). Alles danach und alles in Endlos/Bohrung ist hochgerechnet.
- Endlos: Wellendauer, Überlauf-Rest und Bauzeit sind grob. Die Bauzeit ist die wichtigste und unsicherste Annahme (siehe oben). Nach W50 begrenzt `Wellen.MaxGesamt` die Gegnerzahl, das Leben wächst nur linear: deshalb laufen einzelne Linien bis zur Rechengrenze.
- Bohrung: nur reine Aufbauten eines Turmtyps, Mindestzahl über 4 Wellensätze (Zufall der Wellen, nicht des Spiels: andere Zufallsfolge als `FRandomStream`).
- Meisterfähigkeiten (Level 5) sind wie in `NennDPS()` nicht eingerechnet.

## Frühere Stände

### Aufruf simulator.py (Stand bis 09.11.)

```bash
python simulator.py
```

Optionen:

- `--stufe Normal`: nur diese Stufe; mehrfach angeben für mehrere
- `--turmanteil 0.3`: Anteil der EP, der in Türme geht
- `--turmwahl gemischt`: Kaufstrategie des Modell-Spielers
- `--balance PFAD`: eine andere `Balance.json`
- `--aus ORDNER`: anderes Ausgabeverzeichnis

Laufzeit etwa 8 s für alle vier Stufen bei 300 Durchläufen. Es wird nur die Python-Standardbibliothek gebraucht.

Ausgabe in `Ausgabe\`:

- `Simulation.html`: zwei Diagramme (Q je Welle mit Zielbändern; Gegner-EHP gegen K × Referenzzeit), Wachstumsexponenten, Wert je EP, Abgleich mit den Spiel-Logs und eine Tabelle je Welle mit Filter
- `Simulation.csv`

### Woher die Werte kommen

| Was | Quelle |
|---|---|
| Wellenformel, Stufen, Munition, Resistenzen, EP/FP, Ladenpreise | `TDBalance.cpp` (Standardtabelle), darüber `%LOCALAPPDATA%\FactoryGame\Saved\FicsitTD\Balance.json` |
| Gegnerkatalog (Punkte, ab Welle, Rolle, Phase) und Bosse je Phase | `KernKatalog` und `KernPhasen` in `TDWaveSubsystem.cpp`, Balance `Katalog.*` und `Phasen.*` |
| Freigabe der Forschung je Phase | `TDForschungAbWelle` in `TDForschungVoraussetzungen.inl` (aus `create_research.py`), Balance `Forschung.*.AbWelle` |
| Wellen-Zusammenstellung | nachgebaut aus `PlanFuer` (0.51+): Einführungsgruppe, Phasengewicht, höchstens `Wellen.MaxGesamt`, Boss je Phase |
| Grund-LP der Kreaturen | `annahmen.json`, aus dem Satisfactory-Wiki; Johnny (1000) ist geschätzt |
| Rezeptdauern der Türme | `annahmen.json`, aus `create_turret_recipes.py` |
| Modell-Spieler, Forschungsreihenfolge, Q-Bänder | `annahmen.json` |

Der Simulator liest den Mod-Code direkt aus `E:\Modding\...\FicsitTD\Source`. Änderungen an der Balance-Tabelle wirken also ohne Anpassung.

### Modell-Spieler

- Er startet mit dem Startkontingent, also einem Bolzenturm.
- Nach jeder Welle bekommt er EP und FP nach der Formel des Spiels. Der Kern gilt dabei als voll (`KernAnteilProzent`).
- Ein Anteil `TurmAnteil` der EP geht in Türme. Gekauft wird der Turm mit dem besten Verhältnis Nenn-DPS zu Preis (`bester_wert`) oder reihum (`gemischt`). Die Preise steigen dabei mit jedem Kauf.
- FP gehen in fester Reihenfolge in die Forschung. Damit werden Gewehrturm, Teslaspule und bessere Munition frei.
- Die Abschüsse werden nach DPS-Anteil auf die Türme verteilt und bestimmen deren Stufe.
- **K** ist die Summe der Nenn-DPS gegen ein Einzelziel, so wie `NennDPS()` im Spiel.
- **EHP** sind die Gegner-LP geteilt durch den DPS-gewichteten Resistenzfaktor.
- **Q = EHP / (K × Referenzzeit)**.

### Stand 02.11.2026 (Balance.json und Standards, 300 Durchläufe)

- **Abgleich Wellengröße Normal (Spiel-Log / Simulation):**

  | Welle | 1 | 2 | 3 | 4 | 9 | 10 |
  |---|---|---|---|---|---|---|
  | Spiel | 7 | 9 | 13 | 9 | 17 | 23 |
  | Simulation | 7 | 9 | 13 | 9,2 | 18,2 | 23,0 |

  Die Formel ist also richtig nachgebaut.
- **Wachstum (Normal, Wellen 5–30):** Gegner-EHP etwa Welle^2,1, Abwehr K etwa Welle^1,4. Die Schere ist da, aber flacher als in der Recherche geschätzt (dort: kubisch gegen linear).
- **Q (Normal, 60 % Turmanteil):** W1 0,07, W10 0,09, W20 0,11, W30 0,23. Mit 15 % Turmanteil: W10 0,22, W30 0,47.
  - Die Bosswellen 10, 20 und 30 sind deutliche Spitzen. Johnny bestimmt die Wellen 20 und 30 stark, und seine LP sind geschätzt.
- **Was das heißt:** Die absolute Höhe von Q ist **noch nicht kalibriert**. Die Nenn-DPS unterstellen, dass jeder Turm ständig ein Ziel hat. Im Spiel kam ein Bolzenturm in Welle 10 nur auf etwa die Hälfte. Die Zielbänder aus der Recherche (0,55–0,75 für Normal) passen deshalb noch nicht zu dieser Skala.
- **Nächster Schritt:** einen Einsatz mit 0.29 spielen. Die gemessenen Werte aus `Metriken.csv` (Q, Kernverlust, Uptime) erscheinen dann als Punkte im Q-Diagramm. Danach werden die Referenzzeit oder die Bänder angepasst.

### Stand 03.11.2026 (0.53, 50 Wellen, 300 Durchläufe)

- Umgestellt auf Katalog und Phasen ab 0.51 sowie auf Flammenturm und Nobelisk-Werfer.
  - Flächenwaffen zählen mit einer angenommenen Zahl von Zielen (`Ziele`: Flamme 2, Nobelisk 3).
  - Die Grund-LP von rotem Spucker, Wüstenspucker und kleinem Wasserspucker sind geschätzt.
- **Q (Normal, 60 % Turmanteil):** W1 0,06, W10 0,11, W20 0,08, W30 0,09, W40 0,11, W50 0,30. Wachstum: EHP ~ Welle^2,05, K ~ Welle^1,47.
- **Im Spiel gemessen (0.51, nur 3 Bolzentürme):** W10 Q 0,31, W20 Q 1,45 mit Kernverlust. Der Modell-Spieler kauft dagegen mit 60 % seiner EP Türme und liegt deshalb weit darunter.
  - Für P4 (Q_krit) zählt der Messpunkt im Spiel, nicht die absolute Höhe der Simulation.

### Auswertung eines Balancing-Laufs (ab 0.55)

```
python auswertung_lauf.py [--ab "2026-11-04 18:00"]
```

- Liest `Metriken.csv`, `Wirtschaft.csv` und `Notizen.csv` aus `%LOCALAPPDATA%\FactoryGame\Saved\FicsitTD`.
- Ohne `--ab` nimmt es den letzten Lauf ab Welle 1.
- Ergebnis ist `Ausgabe/Lauf.html`: Q im Spiel gegen die Simulation (Normal), EP-Kurven, Strom und Bauphase. Dazu eine Tabelle je Welle mit Noten. Wellen mit Eingriff sind markiert.

### Grenzen

- Ort und Weg spielen keine Rolle: keine Reichweite, keine Fronten-Aufteilung, keine Mauern.
- Flächen- und Kettenschaden fließen nicht in K ein, genauso wenig wie im Spiel-Q.
- Das Gewehr ist ohne Nachladepause gerechnet, wie die Nenn-DPS im Spiel.
- Munitionsversorgung und Strom gelten als immer ausreichend. Wie oft das nicht stimmt, zeigt erst das Log.

### Balance-Kreislauf (`balance_kreislauf.py`, 06.11.2026)

Ablauf und Ziele: `Notizen/Testplan-Balance-Kreislauf.md`. Ein System je Iteration, Schrauben und Logikregeln in `stellschrauben.json`.

```
python balance_kreislauf.py simuliere --gruppe Wirtschaft   # Koordinatensuche, Ergebnis Ausgabe/Kreislauf/kandidat.json
python balance_kreislauf.py pakete                          # Spielpläne Kreis-<Linie>-Stand/Kandidat, Paket Kreislauf
python balance_kreislauf.py auswerten                       # neuester *_Kreislauf_Metriken.csv: Messung, gemeinsame Kalibrierung über verlust_fit.py
python verlust_fit.py [--schreiben]                         # Verlustkurve + Mehrziel je Typ über alle bekannten Messungen (07.11.)
python balance_kreislauf.py status
```

- Bewertung: 11 Kriterien (K1–K5 aus `zielbild`, W1 über den festen Plan Referenz-Misch, W2–W6 aus `wirtschaft_ziele`), Gleichstand nach Abstand zum Ziel.
- Der Selbsttest misst nur Turm- und Munitionswerte (Versorgung statt Fabrik, Strom frei). Wirtschafts-Kandidaten erzeugen deshalb nur Stand-Läufe zur Kalibrierung.
- Ändert nie den Mod. Kandidaten wirken im Spiel nur über den Schritt `balance`.
- `auswerten <csv>` liest das Paket aus dem Berichtsnamen (auch `Streuung`). Wellen mit Munitionsmangel bleiben außen vor.
- **Nenn-Q und wirksames Q:** Der Mod misst Q mit Einzelziel-DPS. Das Modell führt beides (`dps(..., einzel=True)`, `L.q_nenn`) und rechnet den Verlust mit wirksamem Q. `auswerten` rechnet die Messung je Welle mit Nenn-K/K des Modells für denselben Ausbau um.
- **Verlust (seit 07.11.):** `VerlustGrund + VerlustQ · Q'^VerlustP` mit gestrecktem Q' = Q / max(1, Gegner / `Wellen.MaxGleichzeitig`)^`VerlustStreckung`.
  - Das wirksame Q rechnet je Turm mit `Turmwerte.dps` mal `Mehrziel_<Typ>`. Gewehr und Kerngeschütz bleiben bei 1.
  - Zufallsfaktor aus `VerlustStreuung` (Mittel je Zehntel, auf Mittel 1 normiert). Über `StreuungAbQ` 0,3 wird er gedämpft: 1 + (f − 1) · (0,3/Q')^0,7.
  - Kalibriert am 07.11. mit `verlust_fit.py` (806 Wellen aus Streuung, Kreislauf und Vormittag): 58,5 · Q'^1,35, Mehrziel Bolzen 0,86 / Nobelisk 1,69 / Tesla 0,53 / Flamme 4,94, R² 0,46 statt 0,12. Einzelheiten im Testplan 3.10.
  - Jede Messung braucht die Pläne, mit denen sie lief. Strat-Pläne werden beim Neuerzeugen überschrieben, deshalb steht die Ablage je Messung in `verlust_fit.STANDARD`.
  - Am 06.11. kalibriert, aus dem Kreislauf allein: 0 + 53,9 · Q', Streckung 1,5, R² 0,51. Über alle drei Messungen erklärte die Gerade nur 0,12.
  - Davor galten 36,8 · Q ohne Streckung, Dezile nicht normiert, Alpha-Hog mit 80 LP. Ganz am Anfang waren es 1 + 4 · Q.
- **Gegner-LP:** `annahmen.json` → `Gegner_GrundLP`. Alpha-Hog hat 60 LP, aus der Messung bestimmt. Die Spucker-Werte sind geschätzt, in W24–30 liegt das Modell 10–15 % zu niedrig.

### Strategie-Prüfstand (`strategien.py`, 06.11.2026)

**Zweck:** Spielweisen automatisch durchspielen, mit Wirtschaft, Forschung und Kampf zusammen. Das Ziel (Marius): mehrere Wege führen zum Sieg, aber nicht alles funktioniert und es ist nicht zu leicht.

```bash
python strategien.py --laeufe 3
```

Ausgabe: `Ausgabe/Strategien_Normal.html` und `.csv`, etwa 1400 Läufe in rund 1,5 Minuten.

```bash
python balance_sweep.py --laeufe 2
```

Ausgabe: `Ausgabe/Balance_Sweep.html`. Je Variante gibt es die Erfolgsquote je Turmlinie und die Zahl der erfüllten Zielbild-Kriterien.

**Strategieraum:** 10 Turmlinien (5 Mono, 4 Duo, Breit) × Doktrin (keine / Hauptlinie) × Ausgabestil (erst Türme / erst Level / bester Wert) × Kern-Ausbau (ja/nein) × 4 zufällige Gabel-Kombinationen. Das ergibt 456 Strategien × Startwerte.

**Spieler-Modell:**
- Forscht nach Bedarf: Linie und Munition, Gabelwahl, Turmfertigung, Doktrin, Kern, Kohlekraft.
- Kauft zuerst die Fabrik-Kontingente für die Munition seiner Türme (Kette bis zum Erz wie in der Werkbilanz, Strom mit Kraftwerken).
- Danach Türme oder Level nach Stil. Ladenpreise steigen mit jedem Kauf.

**Kampf:**
- Nenn-DPS je Turm aus Balance und Level, dazu Doktrin, Resistenzen und grobe Mehrfachziel-Faktoren.
- Druckquote Q = EHP / (K × 60 s).
- Kernverlust aus Q, mit Ausreißern. Das ist eine Annahme in `strategie_annahmen.json`; die Messung zeigt bis Q 1 kaum einen Zusammenhang.
- Reparatur 5 %, mit Notreparatur 15 %.

**Zielbild** (`strategie_annahmen.json`):

| Kriterium | Ziel |
|---|---|
| tragfähige Linien (≥ 60 % Erfolg) | mindestens 3 |
| Erfolg gesamt | höchstens 65 % |
| keine Linie dominiert | keine über 90 % |
| keine Linie ist tot | keine unter 20 % |
| zu leicht (Sieg mit Kern ≥ 80 %) | höchstens 15 % |

**Grenzen:**
- Reichweite, Lage der Türme, Laufwege und Fehler im Spiel (Festgefahren, Abstürze) fehlen.
- Der Kernverlust ist geschätzt.
- Ergebnisse sind Richtungen für den Prüfstand im Spiel, keine fertige Balance.

### Spielpläne aus Modellstrategien

`python strategie_plaene.py --nur Mono [--vorziehen]` wählt je Linie den besten Plan, verdichtet auf die Abschnitte des Selbsttests (nur Neubau). Es schreibt `Tests\Plaene\Strat-*.json`, das Paket `Strategie` und die Ablage `Ausgabe\Kreislauf\strategie_plaene.json`. Die Ablage liest `balance_kreislauf.py auswerten` mit. Mit `--nur-modell` wird nur gerechnet. Details stehen im Testplan 3.4.

### Wirtschaft wie im Spiel (06.11.)

`zyklus_min` (Bauphase des Spielers + Welle, `BauphaseSekunden` 195 s aus Live-Daten), Munitionsvorrat je Turm (`STAPEL`, Munitionspuffer ×2), `FeueranteilJeWelle` aus der Uptime-Messung, `TURM_STROM` je Typ. Geprüft gegen Marius' Live-Spiel vom 03.11. (Fabrik bis W13 auf ±1–2 Gebäude). Details: Testplan 3.6. Sicherung des Vorstands: `Ausgabe\Kreislauf\*.vor_Wirtschaft`.

### Stand 09.11.2026 (0.9.1, Endgame-Pakete E1–E3)

**Neu im Prüfstand:**

```bash
python strategien.py --stand 091 --laeufe 3   # mit E1–E3 -> Ausgabe/Strategien_Normal_091.html/.csv
python strategien.py --stand 090              # ohne E1–E3, Standard, Verhalten wie bisher
python endgame_vergleich.py --laeufe 3        # 090, 091 und 091_sparen im selben Lauf -> Ausgabe/Endgame_091.json (etwa 3 min)
```

- **`--stand 090`** lädt den Forschungsbaum von 0.9.0 aus dem Mod-Git (`git show HEAD:Scripts/create_research.py`, nur lesen; Rückfall: Knoten von 0.9.1 entfernen und die Gabel Ölschlick/Elektrozaun wieder setzen).
  - Geprüft: Mit dem alten Code und demselben Baum kommen identische Ergebnisse heraus.
  - Andere Skripte (Kreislauf, Sweep, Pläne) rufen `lade_alles()` ohne Stand auf und rechnen damit weiter wie 0.9.0.
- **E1 Wirtschaft:** Mischer, Fertiger und Förderanlage stehen im Laden.
  - Stickstoff kommt aus dem Ressourcenbrunnen (Paket 3, `StickstoffJeFoerderanlage` 60 m³/min).
  - Der Druckerzeuger zählt nicht als Kontingent, braucht aber 150 MW (`DruckerzeugerStrom`).
- **E2 Munition:** Raketentreibstoff (120/s) und Plasmaflamme (ionisiert, 160/s), Rezepte aus dem Spiel-Datenexport:
  - Raketentreibstoff: 6 Turbotreibstoff + 1 Salpetersäure → 10 in 6 s, Mischer.
  - Salpetersäure: 12 Stickstoff + 3 Wasser + 1 Eisenplatte → 3 in 6 s, Mischer.
  - Ionisierter Treibstoff: 16 Raketentreibstoff + 1 Energiesplitter → 16 in 24 s, Raffinerie.
  - Verbrauch: 4 bzw. 3 m³/min. Energiesplitter gibt es nur als Beute (`Loot.Splitter.*`). Plasma wird nur genutzt, wenn die Splitter für die ganze Welle reichen.
  - Hochspannung: Tesla ×1,3, Forschung über Treibstoffkraft.
- **E3 Stützbauten:** Steht der Turm und ist die Forschung da, kauft der Spieler je 3 Türme einen Bau (`KomboTuermeJeBau`), samt Kohle, Schweröl und Strom.
  - Die Kombo-Wirkung geht nur ins wirksame DPS, nicht in Nenn-Q.
  - Engstelle (Tor): +1 Durchschlag mit Folgeschaden.
  - Sammelpunkt (Lockfeuer): Explosionen ×1,25 auf gelockte Gegner.
  - Leiter (Elektrozaun): 50 % des Grundschadens an Gegner am Zaun.
  - Ölbrand (Ölschlick): Bremse 35 % und Brand 20/s mit Nachbrennen.
- **Neue Annahmen** in `strategie_annahmen.json`, alle mit „Annahme, im Spiel nicht gemessen“ markiert: `KomboAnteilEngstelle` 0,5, `KomboAnteilGelockt` 0,4, `KomboGegnerAmZaun` 2, `KomboAnteilPlatte` 0,4, `KomboPlatteVerweilSekunden` 5, `KomboTuermeJeBau` 3, `SplitterFuerTreibstoff` 1,0, `MunitionSparen` 0 / `SparWellen` 10.
- Sicherung des Vorstands: `Ausgabe\*.vor_0911`.

**Ergebnis (Normal, 50 Wellen, 456 Strategien × 3 Startwerte, Erfolg bis W30 / W50 in %):**

| Linie | 090 | 091 | 091 mit Sparen |
|---|---|---|---|
| Mono Bolzen | 47 / 7 | 48 / 22 | 48 / 22 |
| Mono Gewehr | 92 / 92 | 92 / 92 | 92 / 92 |
| Mono Nobelisk | 99 / 99 | 99 / 99 | 99 / 99 |
| Mono Tesla | 97 / 96 | 99 / 99 | 99 / 99 |
| Mono Flamme | 62 / 62 | 62 / 62 | 62 / 62 |
| Bolzen+Gewehr | 52 / 52 | 52 / 52 | 52 / 52 |
| Gewehr+Tesla | 78 / 78 | 79 / 79 | 79 / 79 |
| Gewehr+Flamme | 90 / 90 | 90 / 90 | 90 / 90 |
| Gewehr+Nobelisk | 75 / 75 | 70 / 70 | 70 / 70 |
| Breit | 31 / 29 | 31 / 28 | 31 / 28 |
| gesamt | 70 | 72 | 72 |

- **Phase 4–5:** Fast alle Niederlagen fallen bis W30. 0.9.1 senkt den Median-Kernverlust je Welle in W31–50 um rund ein Drittel (Mono 2,8 → 1,9, Duo 2,5 → 1,9, Breit 3,0 → 2,2). Spielraum war schon da und wird größer, nur Mono Bolzen gewinnt dadurch Siege (7 → 22 %).
- **Dominanz:** Mono Gewehr, Mono Nobelisk, Mono Tesla und Gewehr+Flamme liegen in beiden Ständen über 90 %. Mono scheitert also nicht vor W50.
  - Tesla wird mit dem Leiter noch stärker: Q in W30 sinkt von 0,38 auf 0,19.
  - Das hängt an der unsicheren Annahme `KomboGegnerAmZaun`.
- **Raketentreibstoff** ist im Mod nicht stärker als Turbotreibstoff: gleiche 120/s, gleiches Nachbrennen, schmalerer Kegel.
  - Die Kette Rakete/Plasma kostet 420–660 Coupons, zu rund 80 % Kohlekraftwerke für Druckerzeuger und Mischer.
  - Ohne Sparen kommt die Plasmaflamme meist erst ab W45. Mit Sparen kommt sie ab W39–41, ändert aber keine Quote, weil Flammen-Linien bei W19 scheitern.
  - Splitter sind kein Engpass.
- **Lockfeuer** kostet in der knappen Phase W12–20 Coupons. Gewehr+Nobelisk fällt von 75 auf 70 % (im Rauschen von etwa ±4 Punkten).
- **Text und Code:** Laut Forschungstext wirkt die Plasmaflamme gegen hitzefeste Spucker, im Code gibt es keine Resistenz-Ausnahme.
- **Empfehlungen** (Details in `Endgame_091.json`):
  - Raketentreibstoff 140/s oder Kegel 1,0.
  - Mischer 16 → 10 und ein Förderanlagen-Paket mit der Forschung.
  - `Kombo.Leiter.Anteil` 0,5 → 0,3 oder höchstens 3 Gegner je Zaun.
  - Druck in Phase 4–5 erst messen, dann erhöhen.
  - Lockfeuer günstiger oder Sammelpunkt 1,4.
  - Plasma-Text oder Code angleichen.
- **Grenzen:**
  - Die Kombo-Faktoren sind geschätzt. ×0,5 und ×1,5 verschieben nur die Bolzen-Linien um einige Punkte.
  - Das Modell ist nur bis W30 an Messungen geprüft.
  - Störfeld und Himmelsturm (W35) sind nicht modelliert, Singularität und Dimensionslager kommen erst ab W51.
  - „Bremsen stapeln“ ist nicht modelliert; die Bremse wirkt nur auf die Flamme.
  - Der Spieler baut keine Treibstoffkraftwerke.
