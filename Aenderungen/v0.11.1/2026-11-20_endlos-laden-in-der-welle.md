---
titel: Endlos-Wellen: Laden im Kern-Menü auch in der Welle bedienbar
typ: behoben
modi: endlos
sichtbar: ja
doku: nicht-noetig
simulation: nicht-noetig
tests: Endlos-Laden, Einsatz-Laden
experiment:
---

**Was:** Im Spielmodus Endlos-Wellen lassen sich Plus, Minus und „Bestätigen“ im Laden des Kern-Menüs jetzt auch bedienen, während eine Welle läuft. Im Einsatz bleibt der Laden in der Welle geschlossen.

**Warum:** Befund C7 im Fahrplan 0.12, betrifft das veröffentlichte 0.11.0. Die Regel vom 16.11. („im Endlos ist der Laden auch in der Welle offen“) stand nur in der Kauffunktion. Das Kern-Menü fragte an drei Stellen selbst „läuft eine Welle?“ und sperrte die Knöpfe ohne Blick auf den Modus. Der Kauf war also erlaubt, aber nicht erreichbar (außer per `/td kaufe`). Jetzt gibt es eine Abfrage `UTDEinsatzSubsystem::LadenInWelleZu()`, die `Kaufe`, `KaufeKorb` und das Kern-Menü gemeinsam benutzen. Handbuch und Spielregeln nennen die Regel schon richtig, dort ändert sich nichts.

**Nachweis:** Prüfung `LadenKnoepfe` liest, ob die Plus-Knöpfe im offenen Kern-Menü bedienbar sind. Plan `Endlos-Laden`: Welle läuft, Knöpfe frei, Kauf einer Mauer. Plan `Einsatz-Laden`: Welle läuft, Knöpfe gesperrt, Kauf abgelehnt. Logzeile „Kern-Menue Laden: Welle ja, Endlos ja -> Knoepfe frei“.

**Changelog EN:** Endless waves: the shop in the Core menu can now be used during a wave, as the rules always said (the buttons were locked).

**Changelog DE:** Endlos-Wellen: Der Laden im Kern-Menü ist jetzt auch während der Welle bedienbar, wie es die Regel vorsieht (die Knöpfe waren gesperrt).
