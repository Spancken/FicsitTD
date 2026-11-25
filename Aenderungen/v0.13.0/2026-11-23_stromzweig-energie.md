---
titel: Baum Energie – Kohle, Öl und Kernkraft, bessere Brennstoffe geben mehr MW je Kraftwerk
typ: neu
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: Energie, A6-Forschung, A6-Abwehr, Shop
experiment:
---

**Was:**
- **Neuer fünfter MAM-Baum „Energie“** (`create_research.py`): drei Strom-Linien ohne Sperre. Kohle und Öl gehen gleichzeitig.
  - **Kohle:**
    - Kohlekraftwerk (W3, zieht aus Industrie um, Schematic-Pfad bleibt)
    - Kompaktkohle-Feuerung (W10, braucht Schwefel und Assembler)
    - Petrolkoks-Feuerung (W20, braucht die Raffinerie)
  - **Öl:**
    - Treibstoffkraftwerk (W15, zieht um, braucht die Raffinerie)
    - Turbo-Feuerung (W25)
    - Raketen-Feuerung (W35, braucht Mischtechnik)
  - **Kernkraft:**
    - Kernkraft (W51, braucht Schwerindustrie, Mischtechnik, Caterium)
    - Plutonium-Brennstab (W56)
- **Die Raffinerie** wird Wurzel im Industrie-Baum und braucht das Kohlekraftwerk aus Energie (A6 E3 bleibt).
- **Mehr MW je Kraftwerk durch bessere Brennstoffe** (`TDStrom.cpp`, Schlüssel `Strom.<Brennstoff>.Faktor`):

| Kraftwerk | Brennstoff | MW |
|---|---|---|
| Kohlekraftwerk | Kohle | 75 |
| Kohlekraftwerk | Kompaktkohle | 100 |
| Kohlekraftwerk | Petrolkoks | 125 |
| Treibstoffkraftwerk | Treibstoff | 250 |
| Treibstoffkraftwerk | Turbotreibstoff | 325 |
| Treibstoffkraftwerk | Raketentreibstoff | 400 |
| Treibstoffkraftwerk | ionisiert | 500 |
| Atomkraftwerk | Uran | 2500 |
| Atomkraftwerk | Plutonium | 3250 |

  - Der Brennstoff verbrennt entsprechend schneller.
  - Alle `Strom.Takt` (2 s) setzt der Mod `mPowerProduction` je Kraftwerk nach dem geladenen Brennstoff (Access Transformer `Friend` für `FTDStrom`).
  - Gilt nicht im Modus Aus.
- **Shop:** neue Kontingente Atomkraftwerk und Teilchenbeschleuniger (je 40 Coupons, +60 %), zusammen 50 Posten. Der Teilchenbeschleuniger zählt jetzt als eigenes Kontingent.
- **Freies Spiel mit Abwehr:** Der Baum Energie ist wie Industrie verborgen (`TDModusAbhaengigkeit`, `TDIndustrieKnoten`).
- **Fund:** Das Treibstoffkraftwerk liefert im Spiel 250 MW. Knotentext und Simulator sagten 150 MW, beides ist korrigiert.
- **Selbsttest:** neuer Schritt `kraftwerk` (Kraftwerk befüllen, Brennstoff laden, MW prüfen) und Plan `Energie`.
- **Simulator** (`strategien.py`):
  - Der gute Spieler wählt Kraftwerk und Brennstoff nach Kosten (Einsatz Coupons, Endlos Bauzeit).
  - Ab 4 Kohlekraftwerken baut er die Stromlinie aus (`StromAusbauAbKraftwerken`).

**Warum:** Marius am 23.11.: „Strom sollte ein separater Zweig sein, wo man sich entscheiden kann, auf welche Stromtechnologie man setzt und sich spezialisieren möchte. Hierbei gerne auch Upgrades der Stromproduktion einplanen.“ Antworten:
- eigener Baum, mit Tesla abgleichen
- keine Sperre, Kohle und Öl gleichzeitig
- Kohle, Öl, Nuklear nach W50
- mehr MW über bessere Brennstoffe
- noch in 0.13

Konzept: `Notizen/Konzept-Stromzweig-2026-11-23.md`.

**Simulation** (`Saved\Pipeline\Sim-Strom-2026-11-23`):
- Einsatz 55/37/20 %, alle Einsatzbänder erfüllt.
- Tesla wird nicht zu stark: Mono Tesla Normal 38 %, Hart 21 %, Albtraum 4 % (wie vorher).
- Die Ausbauten nutzt der Prüfstand meist ab der Spielmitte.
- Endlos: Erkundung erreicht in 12 % der Läufe die Kappung (vorher 10 %, Ziel ≤ 10 %), die beste Linie ist dort Mono Tesla.

**Noch offen:**
- Petrolkoks ist im Modell schwach, weil der Simulator Schweröl eigens herstellt. Im Spiel fällt Schweröl bei Kunststoff und Treibstoff nebenbei ab. Wie sich das anfühlt, zeigt das Handspiel.
- Nuklear ist nur über Rezepte und Kontingente geprüft, nicht im Lauf über Welle 50.

**Changelog EN:** New Power tree: coal, oil and (after wave 50) nuclear power have their own MAM tree, nothing locks anything out. Better fuels give every generator more output (Compacted Coal 100 MW, Petroleum Coke 125 MW, Turbofuel 325 MW, Rocket Fuel 400 MW, Plutonium 3250 MW); Nuclear Power Plants and Particle Accelerators in the Shop from wave 51.

**Changelog DE:** Neuer Baum Energie: Kohle, Öl und nach Welle 50 Kernkraft in einem eigenen MAM-Baum, ohne Sperre. Bessere Brennstoffe geben jedem Kraftwerk mehr Leistung (Kompaktkohle 100 MW, Petrolkoks 125 MW, Turbotreibstoff 325 MW, Raketentreibstoff 400 MW, Plutonium 3250 MW); Atomkraftwerk und Teilchenbeschleuniger im Shop ab Welle 51.

**Nachweis (23.11. vormittags):**
- Build grün (Build-Mod 10:37), Editor: 72 Knoten in 5 Bäumen, 50 Shop-Posten, Übersetzung 847 ohne Lücke.
- Paket `Stromzweig` (`2026-11-23_1047`): 6 von 6 grün.
  - **Energie:** Kraftwerk mit Kohle 75, Kompaktkohle 100, Petrolkoks 125, Treibstoff 250, Turbotreibstoff 325, Raketentreibstoff 400 MW (`GetPowerProductionCapacity`). Wechsel im Log, ohne Flackern über die 2-s-Pflege.
  - **A6-Abwehr:** Energie-Baum im Freien Spiel verborgen.
  - **Shop:** 50 von 50 Posten.
- Grundtest und EinsatzPhase als Gegenprobe grün.
- Volles Paket `Release` (`2026-11-23_1145`): 28/28 grün. `ReleaseEn` (`_1201`): 10/10 grün.
- Danach fehlten noch Englisch für Baumtitel, Knotennamen und drei Herstellungskarten. Sie sind ergänzt, die Fehlliste ist wieder ohne deutsche Einträge (Lauf 12:05).
- Die Läufe Energie EN und MAMBild EN sind bestanden, aber wegen Rechnerlast nicht wertbar (Marius am Rechner).
- Nicht geprüft: Strom im echten Netz unter Last und Nuklear im Lauf. Das kommt mit dem Handspiel.
