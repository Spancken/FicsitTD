---
titel: Gras-S8 Front 1 neu (r14) und verlegte Frontstartpunkte bevorzugt mit Freiraum
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: Front-Gras-S8, Front-Gras-S8-r14, Front-Gras-S8-r11, Front-Gras-S8-r10, Wege-Marius-W06, BautenE3a, Flieger, Oel
experiment:
---

**Was:** Die Klippenprobe verlegt einen Frontstart hinter eine gesperrte Klippe. Neu ist, dass sie nur noch einen Wegpunkt wählt, an dem im Umkreis von `Gegner.FrontFreiRadius` (12 m) keine Klippe steht. Gibt es keinen solchen Punkt, bleibt es beim ersten Wegpunkt hinter der Sperre. Das Log meldet das dann als „OHNE Freiraum“. Code: `UTDWaveSubsystem::SperreKlippe`.

**Warum:**
- Auf Gras-S8 wanderte der Start von Front 1 (Nordosten) in drei Schritten in ein Klippenfeld (X −57570, Y 216277).
- Elite-Stinger (Kapsel 180 cm), Alpha-Hog (125 cm) und Spucker-Alpha (110 cm) liefen dort 1–1,4 m nach dem Spawn gegen CliffPillar_01/02 und wurden versetzt.
- Die Klippenprobe prüft mit 70 cm, große Gegner passen durch die Gasse nicht durch.
- Zweimal reproduziert in W43 (Flieger-Plan im Grasland, Gegencheck-Nachtest 23.11.). Marius am 23.11. abends: „Gras-S8 Front 1“.

**Changelog EN:** Grass Fields: the first front of one core site no longer leads through a canyon where big enemies got stuck.

**Changelog DE:** Grasfelder: Die erste Front eines Kernplatzes führt nicht mehr durch eine Schlucht, in der große Gegner hängen blieben.

**Nachweis:**
- Build 23.11. abends.
- `Front-Freiraum` (`2026-11-23_1953`): Wege-Marius-W06, BautenE3a und Flieger grün. Front-Gras-S8 ohne Festgefahrene, weil Front 1 in dem Lauf gar nicht verlegt wurde; rot war nur „OHNE Freiraum“ an Front 3.
- Die Prüfung zählte zuerst begehbaren Felsboden (CliffFlat) mit. Jetzt prüft sie mit 8 waagerechten Strahlen in 1,5 m Höhe.
- `Abend-2311` (`_2005`): **Gras-S8 Front 1 nicht gelöst.** Der Frontweg läuft vom ersten Klippentreffer bis 150 m vor dem Kern durch eine Schlucht, kein Wegpunkt hat 12 m Freiraum. Der Rückfall verlegt wie bisher an X −57570, Y 216277, und ein Elite-Stinger blieb hängen.
- Die Regel wirkt nur, wo es freie Punkte gibt (Fels-S8 Front 2: „frei im Umkreis 12 m“).

**Gelöst am 24.11. (Gras-S8 Front 1):**
- Neuer Testschalter `Test.FrontR1..4`/`Test.FrontD1..4`: ersetzt eine feste Front ohne neuen Build.
- Paket `Gras-S8-Kandidaten` (`2026-11-24_0832`): r14, r11 und r10 waren je W43 ohne Festgefahrene und ohne Sicherheitsnetz. Nur auf r14 hing die Klippenprobe für Front 1 gar nicht.
- Die Tabelle `TDFesteFronten` hat für Gras-S8 jetzt Front 1 = **r14 fern** (Nordosten): auf Normal 267 m, auf Erkundung fällt sie auf nah 267 m zurück. Vorher war es r13 fern (Schlucht, 389 m auf Normal, 427 m auf Erkundung).
- Korrektur am Mittag: Zuerst stand r14 nah drin. Das gab auf Normal nur 223 m und nach der Verlegung 156 m (K1 brach ab W5 ein).
- Offen für Marius: Front 1 ist damit gut 120–160 m kürzer als vorher. Ein längerer Kandidat wäre r12 (Norden, 308 m), noch nicht auf Klippen geprüft.

- Nachweis Tabelle (`Gras-S8-Neu`, `2026-11-24_0837`): Front 1 „Nordosten (Weg 267 m) (r=14 d=0)“ auf Erkundung; nach der Korrektur auf fern zeigt `Kalib-0124` (`2026-11-24_1155`) auf Normal „Nordosten (Weg 266 m) (r=14 d=2)“, W43 ohne Festgefahrene, ohne Sicherheitsnetz. Oel auf Gras-S8 grün.
- **Beobachten:** Front 3 (r1) wird bei CliffFlat_03 zweimal je Lauf „OHNE Freiraum“ verlegt. In 5 W43-Läufen blieb dort kein Gegner hängen. Die Prüfung im Plan ist deshalb nur Diagnose.

**Vorher offen (Gras-S8 Front 1):**
- Die feste Front 1 liegt auf r13 (Nordosten). Ihr Weg ist mit 389/427 m deutlich länger als die Nachbarn, das ist der Umweg durch die Schlucht.
- Kandidaten mit kurzem, geradem Weg: r14 Nordosten 267 m (nah), r11 Norden 269/294 m, r10 Nordwesten 277/313 m.
- Eine neue feste Front 1 für Gras-S8 braucht einen Messlauf je Kandidat (Paket Fronten-Messung, danach `Front-Gras-S8`).
