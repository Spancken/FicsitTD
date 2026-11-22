---
titel: MAM-Baum entflochten (A6), Festplatte abgeben, Gegenseite im Freien Spiel kaufbar
typ: geaendert
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: A6-Forschung, A6-Abwehr, Shop, Spezialisierung, Bohrung
experiment:
---

**Was** (Bericht `Satisfactory/Notizen/Auftraege-0.13/Bericht-A6-Forschungsbaum.md`, Variante A „Entflechten“ mit E1 bis E7, Marius 22.11.: „Ja, wie empfohlen“):
- **Industrie ohne Pflichtkette** (`Scripts/create_research.py`). Sechs Wurzeln: Assembler, Kupfer, Band Mk2, Kohlekraftwerk, Schwefel, Quarz. Spalten Rohstoffe 0 | Fertigung 1 | Kupfer 2 | Logistik 3–4 | Strom und Öl 5.
  - E1: Band Mk2 hängt nicht mehr an Kupfer.
  - E2: Schwefel und Quarz sind Wurzeln. Schwefel bringt das Schwarzpulver-Rezept mit.
  - E3: Die Raffinerie hängt am Kohlekraftwerk statt an der Gießerei, Bauxit an der Gießerei.
  - Der Himmelsturm ist eigene Wurzel (braucht Bauxit, Schwerindustrie, Schwefel).
- **Voraussetzungen:**
  - Schwerindustrie braucht die Raffinerie.
  - Explosivbolzen braucht den Schwefel.
  - Turbotreibstoff braucht den Assembler.
  - Der Flammenwerfer braucht nur die Raffinerie, der Schildgenerator keine Akkubank mehr.
  - Beim Treibstoffkraftwerk fällt die doppelte Kohlekraft-Bedingung weg.
- **E4 Einstieg ab Welle 3:** Kohlekraftwerk, Teslaspule und Durchschlagbolzen ab Welle 3, also Preisstufe 1 (3 Festplatten). Der Gewehrturm kommt ab Welle 5: Marius 22.11. abends nach der Simulation mit gutem Spieler (Option B2 in `Notizen/Analyse-A6-Simulation-2026-11-22.md`). Kettenblitz und Überlast stehen ausdrücklich ab Welle 6.
- **E5:** Caterium und Panzerbrechend ab Welle 10. Damit sind Zielsuch und Panzerbrechend zur selben Zeit wählbar.
- **E6:**
  - Die Gabel Schildgenerator/Störfeld ist aufgelöst.
  - Miner Mk2 steht im Schlüssel auf 25 (der Wert wirkte schon so).
- **E7 Festplatte abgeben:**
  - Neuer Shop-Posten `Festplattenabgabe` bringt 12 Coupons je Festplatte (`Einsatz.Laden.Festplatte.Rueckgabe`).
  - Der Shop zeigt 1 Coupon an, `TDShopNachKauf` erstattet ihn.
  - Er steht im Bereich Pionier und Kern, Unterkategorie „Kern und Festplatten“, und gilt nur im Einsatz und im Endlos-Modus.
- **Entscheidung 8 (Freies Spiel mit Abwehr):** Eine gewählte Gabelseite sperrt die andere nicht. Die Gegenseite kostet `Abwehr.GabelFaktor` (2) mal so viele Coupons (`UTDAbwehrSubsystem::SetzeGabelPreise`, im 3-s-Takt und vor jedem Forschungsstart).
- **Alte Spielstände:** `TDForschung::GibStilleRezepte` gibt beim Laden jedem erforschten FicsitTD-Knoten alle Rezepte, die er heute freischaltet. Log „Forschung nachgezogen: …“.
- **Texte DE/EN:**
  - Neues Werkzeug `Scripts/ergaenze_en_forschung.py` rechnet die Beschreibungen des Baums nach und baut das Englische aus vorhandenen Teilen (60 neue Paare).
  - `TDTexteEn.inl` ist neu erzeugt. Dabei kamen die englischen Shop-Namen aus H5 mit, die bisher fehlten.
- **Simulator:**
  - Neue Messgrößen `FP_ungenutzt`, `Knoten_offen` und `Knoten_bezahlbar` je Bauphase.
  - Die Forschungsreihenfolge in `annahmen.json` ist neu: Bolzen und Tesla zuerst.
- **Nebenbei:** `create_shop.py` sortierte Festplatte und Kernreparatur unter „Ausrüstung“ statt „Kern und Festplatten“ (Prüfreihenfolge).

**Warum:** Handspiel 22.11. H6: „Am MAM-Baum hat sich noch nichts geändert, das fühlt sich noch genau so linear an.“ Die Linie kam aus der Pflichtkette (Gießerei, Kupfer) und den Freigabewellen, nicht aus zu wenigen Gabeln (Bericht A6, Abschnitt 1 und 5).

**Changelog EN:** The MAM research tree is untangled: Industry has six entry points and no forced chain (Belt Mk.2 without copper, Refinery after Coal Power, Sulfur brings Black Powder). The first research comes before wave 3: Bolt and Tesla lines open from wave 3, the Rifle Turret from wave 5. Shield Generator and Jammer Projector are no longer a branch. Spare Hard Drives can be returned in the AWESOME Shop for 12 Coupons. In Free play with defense the other side of a branch can be researched at double price.

**Changelog DE:** Der MAM-Baum ist entflochten: Die Industrie hat sechs Einstiege und keine Pflichtkette mehr (Band Mk2 ohne Kupfer, Raffinerie nach dem Kohlekraftwerk, Schwefel bringt Schwarzpulver). Die erste Forschung kommt vor Welle 3, Bolzen- und Tesla-Linie gehen ab Welle 3, der Gewehrturm ab Welle 5. Schildgenerator und Störfeld-Projektor sind keine Gabelung mehr. Übrige Festplatten lassen sich im AWESOME-Shop für 12 Coupons abgeben. Im Freien Spiel mit Abwehr ist die Gegenseite einer Gabelung zum doppelten Preis erforschbar.

**Nachweis (22.11. nachmittags):**
- Editor: `create_research.py` (67 Knoten, 39 Voraussetzungen) und `create_shop.py` (48 Posten) ohne Fehler. Alte Assets gesichert in `E:\Modding\Backup\A6_vor_Forschungsbaum_1122`.
- Bau grün (`Pipeline Schnell` `2026-11-22_1543`). Rot ist wie immer nur die Simulation (bekannte Bandverletzungen).
- Paket `A6-Forschungsbaum` (`2026-11-22_1542`): 5 von 6 Plänen grün. `A6-Forschung` hatte eine Warnung, weil die Schlüssel `Einsatz.Laden.Festplattenabgabe.*` fehlten. Nach dem Nachtrag lief `A6-Forschung` allein grün (`_1546`).
  - Band Mk2 ohne Kupfer, Bolzen und Tesla ab Welle 3 je 3 Festplatten, Raffinerie „ZuFrueh“ statt „Gesperrt“, Schwefel und Quarz als Wurzeln, Schwarzpulver mit dem Schwefel.
  - Zielsuch und Panzerbrechend ab Welle 10 beide wählbar. Nach Störfeld ist der Schildgenerator nicht ausgeschlossen.
  - Festplatte abgeben: Coupons 507 → 519, Mindestpreis erstattet („Spiel 1 + Staffel -1 = 0“), zwei Stück +24. Danach eine Festplatte kaufen: −40.
  - `A6-Abwehr`: Kettenblitz nach Überlast startbar, Log „1 Gegenseiten mit Faktor 2.0“, Preis 80 Coupons statt 40.
  - `Shop`, `Spezialisierung` (Gabeln im Einsatz weiter endgültig), `Bohrung`, `Grundtest` grün.
- Simulation (`sim_alle_modi.py`, vorher = Commit `c90fd28`, nachher = A6):
  - Einsatz Siege: Normal 100 → 94 %, Hart 85 → 58 %, Albtraum 70 → 38 %. Hart und Albtraum liegen damit knapp über ihren Bändern (55 % bzw. 35 %), vorher weit darüber.
  - Dominante Linien: 10 → 6. Der Befund „zu leicht“ (Siege ohne Kern unter 80 %) entfällt.
  - Endlos Median: Normal W18,5 → W43 (jetzt im Band), Hart W15,5 → W11 (jetzt unter dem Band W12), Erkundung W115 → W122.
  - Modell-Spieler Normal: ungenutzte Festplatten in Welle 11–30 zwischen 0 und 3, also unter dem Preis des billigsten offenen Knotens (Ziel aus dem Bericht erfüllt).
  - `Knoten_bezahlbar` zählt jeden Knoten einzeln. Vor Welle 3 sind alle 6 offenen einzeln bezahlbar, zusammen nur 2.
- Nicht automatisch geprüft:
  - Alter Spielstand mit erforschtem Schwefel: Es gibt keinen Testspielstand dafür. Das Nachziehen ist allgemein gebaut.
  - Englische MAM-Texte im Spiel: nur offline vollständig, das heißt 0 fehlende Teile.
  - Handspiel (Bericht 6.3).
- **Nachtrag 22.11. abends, Gewehrturm ab Welle 5:**
  - Forschung neu erzeugt, Bau grün (`Pipeline Schnell`).
  - Simulation mit gutem Spieler: Einsatz Hart 90 %, Albtraum 81 %, Endlos Hart W14 (im Band).
  - Selbsttest im Spiel steht aus (Marius: „heute nicht mehr im Spiel testen“). Morgen Paket `A6-Forschungsbaum`.
