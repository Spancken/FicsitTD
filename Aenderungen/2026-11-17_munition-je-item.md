---
titel: Munition je Item (gestuft), Verbrauch im Turmfenster, Miner Mk2/Mk3 und Band Mk4/Mk5
typ: balance
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: Fabrik-F1, Grundtest, MAMBild
---

**Was (Fahrplan P10, Konzept `Notizen\Konzept-Munition-je-Item-2026-11-17.md`, Entscheidungen Marius 17.11.):**
- Neuer Balance-Schluessel `Munition.SchussJeItem.<Rezept>`: Aktionen je Item, gestuft 4/3/2 (Gewehr: Magazine 3/2/2/2/1, Kampfdrohne 4 Einsaetze, Treibstoff 1). Fehlt der Schluessel, gilt 1.
- Turm-Basis: ein Zyklus = eine Ladung (`GetProductionCycleTime` × Faktor), Aktionen verteilt nach Zyklusfortschritt (`GetProductionProgress`). Gewehr: Zyklus = Dauer der Schuesse daraus, Schuesse fliessen laufend ins Magazin (kein Nachladen mehr; Level 5 Gurttrommel wirkt dadurch nicht mehr zusaetzlich – Balance P12).
- Startmunition und Pionier-Nachschub zaehlen Schuss (Items = Schuss / N).
- Turmfenster: „1 Stueck = N Schuss · Stueck/min bei Dauerfeuer · Band Mk1 traegt X Tuerme“.
- MAM Industrie: Band Mk4 (W25, Schwerindustrie), Band Mk5 (W30), Miner Mk2 (W25, nach Band Mk4), Miner Mk3 (W35, Mischtechnik). Band Mk6 nicht (braucht Ficsit). Kontingent Miner liefert das Material der hoechsten freigeschalteten Miner-Stufe. Bandumbau automatisch auch Mk4/Mk5.
- Simulator: Verbrauch ÷ SchussJeItem (`strategien.py`, `SCHUSS_REZEPT`).

**Simulation:** Einsatz Normal jetzt 93 % Erfolg (vorher 76 %), Endlos Erkundung W102 – die Wirtschaft ist deutlich entlastet, die Balance folgt mit der Methode (P12).

**Offen:** im Spiel nicht gemessen; Abnahme „1 voller Knoten / 4 Bolzentuerme L2 bis W5 ohne Munitionsloch“ (Fabrik-Pruefstand, Kette mit vollem Knoten folgt mit P11). Ob das Maschinenfenster die neue Zyklusdauer zeigt, ist im Spiel zu pruefen. Flammenwerfer-Verbrauch nach Messung (B9).

**Changelog EN:** Ammo now lasts longer: one item fires several shots (basic ammo 4, advanced 3, special 2; rifle ammo 3/2/1 magazines). The turret window shows consumption per minute and how many turrets a Mk.1 belt can feed. New in the MAM: Conveyor Belt Mk.4 and Mk.5, Miner Mk.2 and Mk.3.

**Changelog DE:** Munition hält länger: Ein Stück reicht für mehrere Schuss (Grundmunition 4, bessere 3, Spezialmunition 2; Gewehrmunition 3/2/1 Magazine). Das Turmfenster zeigt den Verbrauch pro Minute und wie viele Türme ein Band Mk1 versorgt. Neu im MAM: Förderband Mk4 und Mk5, Miner Mk2 und Mk3.
