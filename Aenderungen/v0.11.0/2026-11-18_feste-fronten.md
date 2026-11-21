---
titel: Feste Fronten je Kernplatz (Fronten haengen nicht mehr am Ladestand des Netzes)
typ: behoben
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: ja
tests: Fronten-Fels-S8, P12-E-R3-Gewehr-A
---

**Befund:** Siehe `2026-11-18_fronten-netz-timing.md`. Auf Fels-S8 gab es je nach Ladestand des Netzes zwei Frontsaetze (Nordwesten 343 m
zuerst oder Norden 357 m zuerst); mit Norden fiel Normal in W27, mit Nordwesten hielt R3 W30 mit 84-87 %.

**Entscheidung Marius 18.11.:** feste Fronten je Platz.

**Was:** Tabelle `TDFesteFronten` in `TDWaveSubsystem.cpp`: je fester Kernplatz (12) vier Fronten als Richtung 0-15 und Abstand
nah/mittel/fern, in der Reihenfolge des Aufgehens. `SucheFronten` merkt sich je Richtung und Abstand den Kandidaten und setzt fuer den
Kern des Laufs (`mLaufPlatz`) die festen Fronten. Andere Stufen: anderer Abstand der Stufe, ohne vollen Weg der naechste Abstand derselben
Richtung. Fehlt der Weg ganz, gilt das als „Netz nicht bereit“ (Neusuche vor der Klippenprobe, Warten bis 30 s, Sicherheitsnetz).
Schalter `Gegner.FesteFronten` (1). Ohne festen Platz (Bohrung, Entwickler-Kerne) wie bisher. Log: `Front i: ... (r= d=)`.

**Messung (Paket `Fronten-Messung`, Bericht `Testberichte/2026-11-18_0923_Fronten-Messung.md`, Stufe Normal):** auf allen Plaetzen
lieferten die Suche beim Einsatzstart und die Suche 90 s spaeter dieselben Fronten; diese stehen in der Tabelle. Duene-S8 nachgemessen
im Paket `Fronten-Nachweis` (der erste Plan baute Gras-S2).

**Nachweis (Paket `Fronten-Nachweis`, Bericht `Testberichte/2026-11-18_0946_Fronten-Nachweis.md`):** Fels-S8 meldet „Fronten fest,
4 von 4 mit vollem Weg“, Nordwesten 343 m zuerst; R3 W30 Kern 87 %, kein Sicherheitsnetz. Die P12-Werte (alle mit Nordwesten) bleiben gueltig.
Nicht geprueft: andere Stufen und andere Plaetze im Lauf (nur die Frontwahl gemessen).

**Simulation:** nicht betroffen.

**Changelog EN:** Each core site now has fixed enemy fronts, so a site always plays the same, no matter how fast the game loads.

**Changelog DE:** Jeder Kernplatz hat jetzt feste Gegnerfronten – ein Platz spielt sich immer gleich, egal wie schnell das Spiel lädt.
