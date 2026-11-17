---
titel: Fabrik-Pruefstand mit echtem Strom und Kosten, Akku im Laden
typ: neu
modi: einsatz
sichtbar: ja
doku: ja
simulation: ja
tests: Fabrik-S0, Fabrik-S1
---

**Was (Fahrplan P11, Konzept `Notizen\Konzept-Fabrik-Strom-2026-11-17.md`, Entscheidungen Marius 17.11.):**
- Schritt `"kosten": "frei" | "kontingent" | "echt"`: jeder Pruefstand-Bau prueft das Kontingent wie beim Spieler, kauft fehlendes im Laden (Coupons) und nimmt im Modus „echt“ das Baumaterial aus dem Inventar. Gesperrtes (Welle, Forschung) wird nicht gebaut und steht im Log („nicht gebaut“).
- Schritt `"strom": "Biomasse"`: Lager mit der Biomasse aus dem Inventar, Band ueber eine Splitter-Kette zu den Brennern (Standard 2), Masten Mk1 und echte Kabel. Cheat „kein Strom noetig“ wird abgeschaltet.
- Schritt `"strom": "Kohle"`: naechstes Gewaesser bis 400 m, Wasserpumpen am Ufer (45 m³/min je Kraftwerk, 120 je Pumpe), Rohre ueber Kreuzungen zu den Kraftwerken an Land, Kohle-Miner auf dem naechsten Kohleknoten mit Band (alle 45 m ein Verbinder als Zwischenstueck), Leitung mit Zwischenmasten ans vorhandene Netz. Anlaufstrom wie im Spiel aus dem Biomasse-Netz.
- Schritt `"netz"`: haengt alle Pruefstand-Bauten und Tuerme ohne Netz an; neue Masten schliessen an vorhandene an (ein Netz statt Inseln). Kabel, die das Spiel nicht verbinden kann, laufen unsichtbar und werden gezaehlt (Ziel 0, Warnung im Log).
- Sicherung: der Selbsttest prueft sie jeden Takt und setzt sie nach `Test.SicherungZurueck` (20 s) zurueck, zaehlt Ausloesungen und Spitzenverbrauch (`fabrikbericht`).
- Schritt `"erforsche": "<Knoten>", "echt": true`: bezahlt die Festplatten des Knotens aus dem Inventar und beachtet Welle, Voraussetzungen und Gabelungen wie das MAM.
- Laden: **Akku** (Stromspeicher, 10 Coupons, +40 % je Kauf), freigeschaltet mit der Forschung Kohlekraft (W6).
- Plaene `Fabrik-S0` (Biomasse, Kosten echt, bis W10) und `Fabrik-S1` (dazu Kohle ab W6 mit echter Forschung).

**Simulation:** nachgezogen, Ergebnis unveraendert (Einsatz Normal 93 %, Endlos Erkundung W102) – der Akku ist im Simulator nicht abgebildet.

**Offen:** nichts davon im Spiel gelaufen (Testfenster durch Marius). Unsicher sind Pumpenhoehe auf dem Wasser, Rohrlaengen und ob das Spiel die langen Kabel annimmt. Treibstoff-Strom (Oelpumpe, Raffinerie, lange Wege) folgt. Wo der Akku freigeschaltet wird (Kohlekraft), ist meine Wahl – Marius entscheidet.

**Changelog EN:** Power Storage is now available in the shop after researching Coal Power.

**Changelog DE:** Der Akku ist nach der Forschung Kohlekraft im Laden erhältlich.
