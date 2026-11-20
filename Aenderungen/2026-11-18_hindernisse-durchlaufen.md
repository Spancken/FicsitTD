---
titel: Wellengegner laufen durch Weltobjekte, die schon als Sperrflaeche im Netz stehen (Festgefahren an Nussbusch und Krabbennest)
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: P12-E-R3-Gewehr-A
---

**Befund (P12-E2, R3 `P12-E-R3-Gewehr-A`, 17.11. abends):** Sicherheitsnetz „Festgefahren versetzt 4“. Kapselprobe im Log:
zweimal `BP_NutBush_C` bei (-243700, -116770) und zweimal `Char_CrabHatcher_C` bei (-262390, -115420), jeweils „Weg frei“,
Tempo 0, Neigung 90–94 Grad. Beide Klassen stehen seit 0.9.4 als Sperrflaeche (NavArea_Null) im Netz – der Weg fuehrt also
um sie herum, aber so knapp, dass die Kapsel (Radius 40–55 cm) die Kante streift. Das Raeumen der Frontwege und das Raeumen
im Gegnerhirn erfassen nur Foliage, diese Objekte sind eigene Actors.

**Ursache:** Weg und Kollision passen nicht zusammen – das Netz plant mit kleinerem Agentenradius als die Kapsel der Gegner.

**Was:** Die beim Netzeintrag gesammelten Weltobjekte (`UTDWaveSubsystem::mWeltHindernisse`, gleiche Klassenliste:
ResourceDeposit, NodeMeshActor, CrabHatcher, NutBush, BerryBush, Knochenboegen) ignorieren Wellengegner beim Laufen
(`IgnoreActorWhenMoving`, wie bei der Logistik seit 02.11.). Die Sperrflaeche bleibt, die Wege fuehren weiter aussen herum;
wer die Kante streift, laeuft durch. Nichts wird aus der Welt entfernt (Spielstand unberuehrt).
Schalter `Gegner.HindernisseDurchlaufen` (1 = an, 0 = altes Verhalten).

**Simulation:** nachgezogen, unveraendert (Wegfindung ist im Simulator nicht abgebildet).

**Nachweis (P12-E3, 18.11. frueh, Bericht `Testberichte/2026-11-18_0638_P12-E3.md`):** gebaut, R3 und R6 bis W33: kein Gegner mehr
an Nussbusch oder Krabbennest festgefahren. Eine Versetzung bleibt in R3 W7 (Alpha-Hog an `CliffPillar_02` bei der Front Nordwesten;
die Klippensperre wurde dort ausgelassen, weil sie den Startpunkt der Front einschliesst) - andere Ursache, offen.

**Changelog EN:** Wave enemies no longer get stuck on the edge of ore deposits, rock nodes, crab nests and nut or berry bushes next to their path.

**Changelog DE:** Wellengegner bleiben nicht mehr an der Kante von Erzvorkommen, Steinknoten, Krabbennestern und Nuss- oder Beerenbüschen neben dem Weg hängen.
