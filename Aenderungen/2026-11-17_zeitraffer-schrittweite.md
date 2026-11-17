---
titel: Selbsttest - Zeitraffer gedeckelt, damit der Fabrik-Takt mitkommt (B20)
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: nein
doku: ja
simulation: ja
tests: Fabrik-S1-Gegenprobe, Fabrik-S1-Gegenprobe-x1
---

**Ursache (Messung Paket B20, 17.11.):** Das Spiel gibt dem Fabrik-Takt je Bild hoechstens rund 1/30 s. Ist ein Rechenschritt laenger (echte Bildzeit × Zeitraffer), laufen Baender, Maschinen und das Nachladen der Tuerme langsamer als die Spielzeit, die Gegner aber nicht. Gemessen in Welle 5 mit gleicher Fabrik: ×4 – 36 s Welle, nur 18,8 s Fabrik-Takt (52 %, 33 ms je Takt gedeckelt), Kern danach 69 % bzw. 36 %; ×1 – 22 s Welle, 21,9 s Fabrik-Takt, Kern 91 %. Ausrichten, Freigabe und Fehlschuesse waren unauffaellig (neue Messwerte in der Turm-Bilanz).

**Behoben:** Der Selbsttest deckelt jeden Zeitraffer so, dass ein Rechenschritt `Test.MaxSchritt` (1/30 s) nicht uebersteigt (`UTDAutotest::SetzeZeitraffer`, Logzeile „Zeitraffer xN gedeckelt auf …“). Nachweis: `Fabrik-S1-Gegenprobe` mit ×4 laeuft gedeckelt auf ×1,5, Fabrik-Takt 22,3 s bei 22,3 s Welle, Kern nach W5 92 %. Turm-Bilanz im Log zusaetzlich mit Ausrichtung, Freigabe ohne Produktion, Fehlschuessen, Bildern und Fabrik-Takt.

**Simulation:** nachgezogen, unveraendert (betrifft nur den Selbsttest).

**Folgen:** Selbsttests mit Zeitraffer bei niedriger Bildrate dauern etwas laenger, messen aber richtig. Fruehere ×4-Laeufe mit langsamen Bildern haben die Abwehr unterschaetzt (Simulator-Kalibrierung, P12).

**Spieler unter 30 Bildern/s:** haben den Effekt auch (Spielverhalten von Satisfactory, alle Maschinen gleich). Marius 17.11.: so lassen, kein Ausgleich.
