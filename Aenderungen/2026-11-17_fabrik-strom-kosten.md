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
- Diagnose im `fabrikbericht`: Brennstoff, Lagerinhalt, Netznummer, Band-Anschluesse je Strombau (`Fabrik-Strom:` im Log). Diagnose-Plaene `Fabrik-S0-Diagnose`, `Fabrik-F1-Diagnose`, `Fabrik-S0-Diagnose-Cheat`.

**Simulation:** nachgezogen, Ergebnis unveraendert (Einsatz Normal 93 %, Endlos Erkundung W102) – der Akku ist im Simulator nicht abgebildet.

**Offen:** nichts davon im Spiel gelaufen (Testfenster durch Marius). Unsicher sind Pumpenhoehe auf dem Wasser, Rohrlaengen und ob das Spiel die langen Kabel annimmt. Beim Treibstoff liegt das Oelfeld weit weg: ob das Spiel dort Boden, Rohre und Kabel richtig setzt, solange die Gegend nicht geladen ist, zeigt erst der Lauf. Wo der Akku freigeschaltet wird (Kohlekraft), ist meine Wahl – Marius entscheidet.

**Changelog EN:** Power Storage is now available in the shop after researching Coal Power.

**Changelog DE:** Der Akku ist nach der Forschung Kohlekraft im Laden erhältlich.
