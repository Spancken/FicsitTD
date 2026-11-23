---
titel: Verlegte Frontstartpunkte bevorzugt mit Freiraum (Gras-S8 Front 1 noch offen)
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: nein
doku: ja
simulation: nicht-noetig
tests: Front-Gras-S8, Wege-Marius-W06, BautenE3a, Flieger
experiment:
---

**Was:** Die Klippenprobe verlegt einen Frontstart hinter eine gesperrte Klippe. Neu ist, dass sie nur noch einen Wegpunkt wählt, an dem im Umkreis von `Gegner.FrontFreiRadius` (12 m) keine Klippe steht. Gibt es keinen solchen Punkt, bleibt es beim ersten Wegpunkt hinter der Sperre. Das Log meldet das dann als „OHNE Freiraum“. Code: `UTDWaveSubsystem::SperreKlippe`.

**Warum:**
- Auf Gras-S8 wanderte der Start von Front 1 (Nordosten) in drei Schritten in ein Klippenfeld (X −57570, Y 216277).
- Elite-Stinger (Kapsel 180 cm), Alpha-Hog (125 cm) und Spucker-Alpha (110 cm) liefen dort 1–1,4 m nach dem Spawn gegen CliffPillar_01/02 und wurden versetzt.
- Die Klippenprobe prüft mit 70 cm, große Gegner passen durch die Gasse nicht durch.
- Zweimal reproduziert in W43 (Flieger-Plan im Grasland, Gegencheck-Nachtest 23.11.). Marius am 23.11. abends: „Gras-S8 Front 1“.

**Changelog EN:** -

**Changelog DE:** - (erst mit gelöster Gras-S8-Front)

**Nachweis:**
- Build 23.11. abends.
- `Front-Freiraum` (`2026-11-23_1953`): Wege-Marius-W06, BautenE3a und Flieger grün. Front-Gras-S8 ohne Festgefahrene, weil Front 1 in dem Lauf gar nicht verlegt wurde; rot war nur „OHNE Freiraum“ an Front 3.
- Die Prüfung zählte zuerst begehbaren Felsboden (CliffFlat) mit. Jetzt prüft sie mit 8 waagerechten Strahlen in 1,5 m Höhe.
- `Abend-2311` (`_2005`): **Gras-S8 Front 1 nicht gelöst.** Der Frontweg läuft vom ersten Klippentreffer bis 150 m vor dem Kern durch eine Schlucht, kein Wegpunkt hat 12 m Freiraum. Der Rückfall verlegt wie bisher an X −57570, Y 216277, und ein Elite-Stinger blieb hängen.
- Die Regel wirkt nur, wo es freie Punkte gibt (Fels-S8 Front 2: „frei im Umkreis 12 m“).

**Noch offen (Gras-S8 Front 1):**
- Die feste Front 1 liegt auf r13 (Nordosten). Ihr Weg ist mit 389/427 m deutlich länger als die Nachbarn, das ist der Umweg durch die Schlucht.
- Kandidaten mit kurzem, geradem Weg: r14 Nordosten 267 m (nah), r11 Norden 269/294 m, r10 Nordwesten 277/313 m.
- Eine neue feste Front 1 für Gras-S8 braucht einen Messlauf je Kandidat (Paket Fronten-Messung, danach `Front-Gras-S8`).
