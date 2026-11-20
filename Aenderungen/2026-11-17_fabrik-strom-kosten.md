---
titel: Fabrik-Pruefstand mit echtem Strom und Kosten, Akku im Laden
typ: neu
modi: einsatz
sichtbar: ja
doku: ja
simulation: ja
tests: Fabrik-S0, Fabrik-S1, Fabrik-S2
---

**Was (Fahrplan P11, Konzept `Notizen\Konzept-Fabrik-Strom-2026-11-17.md`, Entscheidungen Marius 17.11.):**
- Schritt `"kosten": "frei" | "kontingent" | "echt"`: jeder Pruefstand-Bau prueft das Kontingent wie beim Spieler, kauft fehlendes im Laden (Coupons) und nimmt im Modus „echt“ das Baumaterial aus dem Inventar. Gesperrtes (Welle, Forschung) wird nicht gebaut und steht im Log („nicht gebaut“).
- Schritt `"strom": "Biomasse"`: Lager mit der Biomasse aus dem Inventar, Band ueber eine Splitter-Kette zu den Brennern (Standard 2), Masten Mk1 und echte Kabel. Cheat „kein Strom noetig“ wird abgeschaltet.
- Schritt `"strom": "Kohle"`: naechstes Gewaesser bis 400 m, Wasserpumpen am Ufer (45 m³/min je Kraftwerk, 120 je Pumpe), Rohre ueber Kreuzungen zu den Kraftwerken an Land, Kohle-Miner auf dem naechsten Kohleknoten mit Band (alle 45 m ein Verbinder als Zwischenstueck), Leitung mit Zwischenmasten ans vorhandene Netz. Anlaufstrom wie im Spiel aus dem Biomasse-Netz.
- Schritt `"strom": "Treibstoff"`: wie ein Spieler am Oelfeld – Oelpumpe auf dem naechsten freien Oelknoten bis 2 km (keine Fracking-Satelliten), Kreuzung, Raffinerien mit Rezept Treibstoff (eine Pumpe reicht fuer 2 Raffinerien = 4 Kraftwerke), Harz per Band in je ein Lager, Treibstoff ueber Kreuzungen zu den Kraftwerken, lange Leitung mit Zwischenmasten ans Netz. Rohre fliessen immer vom Erzeuger zum Verbraucher (gilt jetzt auch fuer Kohle).
- Schritt `"netz"`: haengt alle Pruefstand-Bauten und Tuerme ohne Netz an; neue Masten schliessen an vorhandene an (ein Netz statt Inseln). Kabel, die das Spiel nicht verbinden kann, laufen unsichtbar und werden gezaehlt (Ziel 0, Warnung im Log).
- Sicherung: der Selbsttest prueft sie jeden Takt und setzt sie nach `Test.SicherungZurueck` (20 s) zurueck, zaehlt Ausloesungen und Spitzenverbrauch (`fabrikbericht`).
- Schritt `"erforsche": "<Knoten>", "echt": true`: bezahlt die Festplatten des Knotens aus dem Inventar und beachtet Welle, Voraussetzungen und Gabelungen wie das MAM.
- Laden: **Akku** (Stromspeicher, 10 Coupons, +40 % je Kauf), freigeschaltet mit der Forschung Kohlekraft (W6).
- Plaene `Fabrik-S0` (Biomasse, Kosten echt, bis W10) `Fabrik-S1` (dazu Kohle ab W6 mit echter Forschung) und `Fabrik-S2` (dazu Treibstoff ab W15, Forschungskette Assembler, Giesserei, Raffinerie, Treibstoffkraft mit echten Festplatten).

**Test 17.11. (Fenster1117, Diagnose-Laeufe, Fenster1117b):**
- Erster Lauf: Netz 0 MW, Kern in Welle 1. Ursache: die Standardroute der Testbibliothek lieferte fuer Baender mit kurzem Abstand und quer stehenden Anschluessen keine Spline-Punkte („not enough spline points '0'“), solche Baender foerdern nicht – betroffen Lager → Splitter → Brenner und auch die Splitter-Kette zu den Tuermen (F1-Messung B18 neu bewerten). Behoben: alle Pruefstand-Baender und -Rohre mit der Tangenten-Route; Plaene pruefen die Warnung (max 0).
- Zweiter Lauf: Strom laeuft (Brenner 100 %, 60 MW). Die Sicherung blieb nach einem fehlgeschlagenen zweiten Stromschritt aus, weil dieser die Ueberwachung abschaltete – behoben; Zaehler, Netz und Kostenmodus je Plan zurueckgesetzt (`TDFabrik::NeuerPlan`). Kohlesuche 1,5 km, Oelsuche 5 km (Grass Fields).
- Nachtest: 0 s ohne Strom in allen Wellen, Festplatten kommen an. Eine Kette (15 Bolzen/min) haelt 4 Bolzentuerme L2 nur bis etwa Welle 4–6 (Munitionsnot ab W4/W5) – Befund fuer P10/P12; S1/S2 bauen jetzt drei Ketten.
- Lauf mit drei Ketten (`2026-11-17_1008_Fenster1117b`): Maschinen 100 %, 45 Bolzen/min kommen an, aber die Splitter-Kette verteilt 1/2 – 1/4 – 1/8 – 1/8 (Munition je Turm 76 / 35 / 15 / 15). Der erste Turm hortet, die letzten beiden hungern ab W3 (Munitionsnot 36–84 %), weil der Turmpuffer nie voll wird und die Kette sich deshalb nicht staut. Zwei Brenner (60 MW) reichen fuer drei Ketten + 4 Tuerme nicht: Grundlast 51 MW, Spitze 59 MW, Sicherung beim Anlauf mit leeren Brennern (S1: Welle 1 ohne Strom). Offen fuer Marius: Turmpuffer begrenzen (dann staut sich die Kette wie im Spiel) oder Pruefstand gleichmaessig verteilen; Pruefstand mit mehr Brennern.
- **Verteilbaum (Marius 17.11.: „gleichmaessig aufteilen, wie ein guter Spieler“)** statt Splitter-Kette fuer Tuerme und Brenner; S1/S2 mit 4 Brennern (120 MW). Lauf `2026-11-17_1036_Fenster1117c`: Munition je Turm 27 / 35 / 26 / 28, Munitionsnot 0 % in allen Wellen. F1-3K4T (3 Ketten = 45 Bolzen/min, 4 Bolzentuerme L2, ohne Strom) bestanden bis W10, Kern am Ende 6 % (Neubewertung B18). S1 mit echtem Strom: 0 s ohne Strom, Sicherung 0-mal, Spitze 83 MW von 120; Kohlekraftwerk mit echter Forschung gebaut (Pumpe, 4 Rohre, Kohleknoten 631 m vom Werk, 16 Baender, 13 Zwischenstuecke, 10 Masten, 150 MW) – die Kohle war nach 120 s noch unterwegs (631 m Band Mk1). Kern faellt in S1 trotzdem in W6 ohne Munitionsnot (in F1-3K4T 86 % in W6) – Unterschied noch nicht erklaert.
- **Befund Schrittweite (Gegenproben `Fabrik-S1-Gegenprobe`, `-x1`):** S1 fiel nicht am Strom. Die Turmwirkung haengt an der Rechenschrittweite (Bildzeit × Zeitraffer; `wellenlauf` laeuft ohne eigenes Feld mit ×4): Welle 5 gleiche Fabrik – 33 ms: 21,7 s / Kern 91 %, 40 ms: 23,4 s / 91 %, 100 ms: 30,2 s / 69 %, 136 ms: 38,6 s / 36 %. Gleich viele Schuesse je Welle, aber langsamer. UE-Timer holen verpasste Aufrufe nach, das Zielen ist es also nicht; die genaue Stelle ist offen (Fabrik-Takt, Produktionsfreigabe?). Folgen: Pruefstand-Wellen jetzt in Echtzeit (S0–S2); fruehere x4-Laeufe mit niedriger Bildrate unterschaetzen die Abwehr (wichtig fuer P12); Spieler mit sehr niedriger Bildrate koennten betroffen sein.
- Diagnose im `fabrikbericht`: Brennstoff, Lagerinhalt, Netznummer, Band-Anschluesse je Strombau (`Fabrik-Strom:` im Log). Diagnose-Plaene `Fabrik-S0-Diagnose`, `Fabrik-F1-Diagnose`, `Fabrik-S0-Diagnose-Cheat`.

**Simulation:** nachgezogen, Ergebnis unveraendert (Einsatz Normal 93 %, Endlos Erkundung W102) – der Akku ist im Simulator nicht abgebildet.

**Offen:** nichts davon im Spiel gelaufen (Testfenster durch Marius). Unsicher sind Pumpenhoehe auf dem Wasser, Rohrlaengen und ob das Spiel die langen Kabel annimmt. Beim Treibstoff liegt das Oelfeld weit weg: ob das Spiel dort Boden, Rohre und Kabel richtig setzt, solange die Gegend nicht geladen ist, zeigt erst der Lauf. Wo der Akku freigeschaltet wird (Kohlekraft), ist meine Wahl – Marius entscheidet.

**Changelog EN:** Power Storage is now available in the shop after researching Coal Power.

**Changelog DE:** Der Akku ist nach der Forschung Kohlekraft im Laden erhältlich.
