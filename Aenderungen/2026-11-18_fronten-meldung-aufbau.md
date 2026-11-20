---
titel: Keine Fehlmeldung "Kein Weg geprueft" beim Einsatzstart, solange das Netz noch aufgebaut wird
typ: behoben
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: ja
tests: Vorbereitung
---

**Befund (B24, Handspiel Marius 18.11., Gras-S2 auf Erkundung):** Die erste Frontensuche lief 4 s nach dem Bau des Kernplatzes. Im
Log stand fuer Norden (r11) "teil teil Netz-": das Navigationsnetz war dort noch nicht fertig, daher "3 von 4 mit vollem Weg" und die
Chatmeldung "Kein Weg geprueft (Navigationsnetz fehlt dort?)". Die Nachpruefung (alle 15 s) fand 2 min 20 s spaeter "4 von 4" und 14
gueltige Richtungen, Welle 1 lief normal. Die feste Fronttabelle war also richtig; falsch war nur die Meldung.

**Aenderung:** In der Bauphase meldet `GetFrontenText` bei 0 geprueften Richtungen "Die Wege werden noch geprueft, das Navigationsnetz
baut sich auf. Das Ergebnis kommt gleich im Chat." Das Ergebnis meldet wie bisher `PruefeFrontenNach` ("Wege geprueft: die erste Welle
kommt aus ..."). Ausserhalb der Bauphase bleibt die alte Meldung mit Netzdiagnose.

**Changelog EN:** While the navigation mesh is still building after landing, the chat says the paths are still being checked instead of "No path checked".
**Changelog DE:** Solange das Navigationsnetz nach der Landung noch entsteht, meldet der Chat „Die Wege werden noch geprüft“ statt „Kein Weg geprüft“.

**Stand:** gebaut.
