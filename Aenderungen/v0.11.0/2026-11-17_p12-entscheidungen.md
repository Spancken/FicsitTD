---
titel: Gewehrturm Level 5 +25 % Feuerrate, Bohrung ohne ungepruefte Wege, Pruefstand mit Referenzpause
typ: geaendert
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: Fabrik-S2, Fabrik-P10-Abnahme
---

**Was (Entscheidungen Marius 17.11. abends zum P12-Bericht und zu B17):**
- **Gewehrturm Level 5 (Gurttrommel):** wirkte seit P10 nicht mehr zusaetzlich (Dauerfeuer ist Standard). Jetzt +25 % Feuerrate (Level-Tabelle `Gewehrturm.L5.Takt` 0,25). Texte im Turmfenster, MAM (Turmfertigung IV), Spielregeln und Handbuch angepasst; Forschung neu erzeugt.
- **B17 Bohrung:** Fehlen der Bohrstufe Zugaenge, nimmt sie keine ungeprueften Ersatzpunkte mehr, sobald es mindestens einen geprueften Weg gibt. Die Gegner einer Welle verteilen sich reihum auf die aktiven Fronten – weniger Wege heisst entsprechend mehr Gegner je Weg bei gleicher Wellengroesse. Fronten werden beim Bohrstart frisch bestimmt.
- **Pruefstand:** `wellenlauf` mit `"pause": N` wartet N Spielsekunden zwischen den Wellen (Referenz 195 s wie Marius' Handspiel, Standard `Test.Pause` 0). Bei vollem Ausbau mehrerer Knoten versorgt jeder Knoten seine eigene Turmgruppe (ein Band Mk1 traegt nur 60/min).

**Simulation:** nachgezogen, Einsatz Normal 93 %, Endlos Erkundung W102, Endlos Normal W17 – unveraendert, die Einstellung folgt mit P12.

**Offen:** im Spiel nicht gelaufen.

**Changelog EN:** Rifle Turret master tier (Ammo Drum) now gives +25% fire rate. Drilling: if a site has fewer free approaches than the level needs, enemies come along the remaining paths instead of getting stuck on unchecked ones.

**Changelog DE:** Gewehrturm-Meisterstufe (Gurttrommel) gibt jetzt +25 % Feuerrate. Bohrung: Hat ein Platz weniger freie Zugänge, als die Stufe braucht, kommen die Gegner über die verbleibenden Wege, statt an ungeprüften hängen zu bleiben.
