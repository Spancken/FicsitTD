---
titel: Maschinen und Türme als Sperrfläche im Gegnernetz (N1)
typ: neu
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: KZ-2-Maschinen, Grundtest, EinsatzPhase, Bohrung
experiment:
---

**Was:**
- Jeder Bau des Spiels oder des Mods, der nicht von selbst im Navigationsnetz der Gegner steht, bekommt einen Sperrkasten. Das betrifft Maschinen, Lager, Generatoren, MAM, Shop, HUB und Türme, im Umkreis `Gegner.BauSperrenRadius` = 250 m um den Kern.
- Der Kasten ist wie bei der Klippensperre gebaut:
  - eigener Actor ohne Kollision, also keine unsichtbare Wand für den Spieler
  - dynamisches Hindernis mit `UNavArea_Null`
  - gedreht wie der Bau, Maße aus den lokalen Grenzen der Komponenten mit Kollision, bis 1,5 m unter den Fuß. Ohne solche zählen alle Komponenten, aber höchstens 12 m je Seite, denn der Reichweitenring der Türme hatte riesige Kästen erzeugt.
- Ausgenommen:
  - Logistik, Fundamente und Wände, die schon im Netz stehen
  - TD-Mauer, Sperre, Testmauer, Tor und Elektrozaun, die ihre eigene Netzwirkung haben
  - der Kern und Kleinteile unter 1 m
- Neu gesetzt wird beim Laufstart, in jeder Bauphase, vor jeder Prüfung „Kern erreichbar“, 1 s nach jedem Bau und bei der Bohrungsprüfung. Kästen abgerissener Bauten verschwinden binnen 3 s.
- Die Kästen bleiben auch nach dem Laufende stehen. Würden sie entfernt und bei der nächsten Bohrungsprüfung neu gesetzt, baut das Netz genau beim Start um (Lauf 0855). Gespeichert werden sie nicht, und mit ihrem Bau verschwinden sie.
- Schalter `Gegner.BauSperren` (1), `Gegner.BauSperrenRadius` (250 m).
- Neue Datei `TDBauSperren.cpp`.

**Warum:** Messung 22.11. (Plan `KZ-0-Messung`):
- Maschinen und Türme wirken nicht auf das Netz der Gegner. Die Gegner planen hindurch und bleiben hängen.
- Ein Ring aus Maschinen oder Türmen war für die Prüfung „Kern erreichbar“ unsichtbar.
- Marius hat am 22.11. N1 gewählt (Fahrplan 0.13, Abschnitt 7).
- Erwartete Nebenwirkung: Die Gegner laufen um Fabrik und Türme herum statt hinein, es gibt weniger „Festgefahren“.

**Changelog EN:** Enemies now path around your machines and turrets instead of walking into them, and a ring of machines or turrets around the Core is recognized as sealing it off.

**Changelog DE:** Gegner laufen jetzt um Maschinen und Türme herum statt hinein, und ein Ring aus Maschinen oder Türmen um den Kern gilt als Abriegelung.

**Nachweis (22.11. vormittags):** Gesamtlauf `2026-11-22_1031_Kern-Zugang-K1`: `KZ-2-Maschinen`, Grundtest, EinsatzPhase und Bohrung bestanden.
- Ring aus 18 Konstruktoren auf der Plattform: Netz unter 17 von 18 weg, als Abriegelung erkannt, Welle wartet.
- Nach dem Abriss sind die Kästen entfernt, und die Welle startet.
- Kästen: Bolzenturm 6,0 × 4,1 m, Konstruktor 7,6 × 9,6 m, MAM 4,6 × 8,3 m.
- Vorher korrigiert:
  - Riesenkästen durch den Reichweitenring der Türme (Lauf 0831)
  - Sperren direkt beim Bau-Ereignis (Maße noch unfertig)
  - Entfernen am Laufende (Lauf 0855)
