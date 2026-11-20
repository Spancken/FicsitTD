---
titel: ADA kündigt neue Gegnerarten und Bosse an
typ: neu
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: Endlos
---

**Was:** Zu Beginn der Bauphase vor einer Welle mit neuer Gegnerart oder Boss meldet sich ADA mit einem kurzen, trockenen Satz (21 Ansagen, DE/EN, `TDAda.cpp`). Je Lauf jede Ansage einmal, ab Welle 60 ein kurzer Satz je Bosswelle. Mit Sprachaufnahme, sobald die Nachrichten erzeugt sind (Kette wie die Begrüßung), bis dahin als ADA-Zeile im Chat. Abschaltbar mit `ADA.Ansagen 0`. Testbefehl `/td ada liste`, `/td ada <Schlüssel>`.

**Warum:** Marius 16.11.

**Stand:** Ton freigegeben (Marius 16.11.: „ADAs Ansagen passen“). Text immer als ADA-Zeile im Chat, Stimme über die Unreal-Audioengine (`Scripts\Audio\Erzeuge-ADA-Ansagen.ps1`: Sätze aus `TDAda.cpp`, Windows-Stimme, Filter, Import). **Offen:** Aufnahmen erzeugen und im Spiel anhören.

**Changelog EN:** ADA now comments on new enemy species and boss waves.

**Changelog DE:** ADA kommentiert neue Gegnerarten und Bosswellen.
