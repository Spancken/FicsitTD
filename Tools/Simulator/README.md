# FicsitTD-Simulator

Berechnet ohne Spiel für jede Welle, wie viel Gegnerdruck auf wie viel Abwehr trifft. Die Kennzahl ist die Druckquote **Q**, dieselbe wie im Log ab FicsitTD 0.29. Grundlage ist Abschnitt 3 von `..\..\Notizen\Recherche-Progression-Endgame-Balancing.md`.

## Aufruf

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

## Woher die Werte kommen

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

## Modell-Spieler

- Er startet mit dem Startkontingent, also einem Bolzenturm.
- Nach jeder Welle bekommt er EP und FP nach der Formel des Spiels. Der Kern gilt dabei als voll (`KernAnteilProzent`).
- Ein Anteil `TurmAnteil` der EP geht in Türme. Gekauft wird der Turm mit dem besten Verhältnis Nenn-DPS zu Preis (`bester_wert`) oder reihum (`gemischt`). Die Preise steigen dabei mit jedem Kauf.
- FP gehen in fester Reihenfolge in die Forschung. Damit werden Gewehrturm, Teslaspule und bessere Munition frei.
- Die Abschüsse werden nach DPS-Anteil auf die Türme verteilt und bestimmen deren Stufe.
- **K** ist die Summe der Nenn-DPS gegen ein Einzelziel, so wie `NennDPS()` im Spiel.
- **EHP** sind die Gegner-LP geteilt durch den DPS-gewichteten Resistenzfaktor.
- **Q = EHP / (K × Referenzzeit)**.

## Stand 02.11.2026 (Balance.json und Standards, 300 Durchläufe)

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

## Stand 03.11.2026 (0.53, 50 Wellen, 300 Durchläufe)

- Umgestellt auf Katalog und Phasen ab 0.51 sowie auf Flammenturm und Nobelisk-Werfer.
  - Flächenwaffen zählen mit einer angenommenen Zahl von Zielen (`Ziele`: Flamme 2, Nobelisk 3).
  - Die Grund-LP von rotem Spucker, Wüstenspucker und kleinem Wasserspucker sind geschätzt.
- **Q (Normal, 60 % Turmanteil):** W1 0,06, W10 0,11, W20 0,08, W30 0,09, W40 0,11, W50 0,30. Wachstum: EHP ~ Welle^2,05, K ~ Welle^1,47.
- **Im Spiel gemessen (0.51, nur 3 Bolzentürme):** W10 Q 0,31, W20 Q 1,45 mit Kernverlust. Der Modell-Spieler kauft dagegen mit 60 % seiner EP Türme und liegt deshalb weit darunter.
  - Für P4 (Q_krit) zählt der Messpunkt im Spiel, nicht die absolute Höhe der Simulation.

## Auswertung eines Balancing-Laufs (ab 0.55)

```
python auswertung_lauf.py [--ab "2026-11-04 18:00"]
```

- Liest `Metriken.csv`, `Wirtschaft.csv` und `Notizen.csv` aus `%LOCALAPPDATA%\FactoryGame\Saved\FicsitTD`.
- Ohne `--ab` nimmt es den letzten Lauf ab Welle 1.
- Ergebnis ist `Ausgabe/Lauf.html`: Q im Spiel gegen die Simulation (Normal), EP-Kurven, Strom und Bauphase. Dazu eine Tabelle je Welle mit Noten. Wellen mit Eingriff sind markiert.

## Grenzen

- Ort und Weg spielen keine Rolle: keine Reichweite, keine Fronten-Aufteilung, keine Mauern.
- Flächen- und Kettenschaden fließen nicht in K ein, genauso wenig wie im Spiel-Q.
- Das Gewehr ist ohne Nachladepause gerechnet, wie die Nenn-DPS im Spiel.
- Munitionsversorgung und Strom gelten als immer ausreichend. Wie oft das nicht stimmt, zeigt erst das Log.

## Balance-Kreislauf (`balance_kreislauf.py`, 06.11.2026)

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

## Strategie-Prüfstand (`strategien.py`, 06.11.2026)

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

## Spielpläne aus Modellstrategien

`python strategie_plaene.py --nur Mono [--vorziehen]` wählt je Linie den besten Plan, verdichtet auf die Abschnitte des Selbsttests (nur Neubau). Es schreibt `Tests\Plaene\Strat-*.json`, das Paket `Strategie` und die Ablage `Ausgabe\Kreislauf\strategie_plaene.json`. Die Ablage liest `balance_kreislauf.py auswerten` mit. Mit `--nur-modell` wird nur gerechnet. Details stehen im Testplan 3.4.

## Wirtschaft wie im Spiel (06.11.)

`zyklus_min` (Bauphase des Spielers + Welle, `BauphaseSekunden` 195 s aus Live-Daten), Munitionsvorrat je Turm (`STAPEL`, Munitionspuffer ×2), `FeueranteilJeWelle` aus der Uptime-Messung, `TURM_STROM` je Typ. Geprüft gegen Marius' Live-Spiel vom 03.11. (Fabrik bis W13 auf ±1–2 Gebäude). Details: Testplan 3.6. Sicherung des Vorstands: `Ausgabe\Kreislauf\*.vor_Wirtschaft`.

## Stand 09.11.2026 (0.9.1, Endgame-Pakete E1–E3)

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
