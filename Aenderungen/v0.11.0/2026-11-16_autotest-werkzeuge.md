---
titel: Autotest: Stillstandswächter, Modus 2/3, Zählung ohne eigene Zeilen
typ: intern
modi: alle
sichtbar: nein
doku: nicht-noetig
simulation: nicht-noetig
tests: Endlos, Bohrung
---

**Was:** `Teste-Mod.ps1` beendet das Spiel nach 12 min ohne Fortschritt im Log. Autotest übergibt Modus 2 und 3 richtig, `aufZeit` bricht im manuellen Lauf ab, `LogEnthaelt` zählt die eigenen Autotest-Zeilen nicht mit, neue Schritte `warteBis BohrungVorbei`, `MAMKosten coupons`, Befehl `/td bohrung`.

**Warum:** Endlos-Langlauf hing am 15.11. zwei Stunden; Bohrung zählte die eigene OK-Zeile.
