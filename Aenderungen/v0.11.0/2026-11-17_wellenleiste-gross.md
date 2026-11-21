---
titel: Wellenleiste passt in grosse Wellen (kompakte Zeilen, besiegte Arten ausgeblendet)
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: P12-E-Leiste
---

**Befund (Marius, Screenshot W42 am 17.11.):** Die Wellenleiste links lief in grossen Wellen unten aus dem Bild.

**Was (`TDKernTerminal.cpp`, `UTDWellenLeiste`):** Bereich mit fester Hoehe (9-74 % der Bildhoehe) und Abschneiden am Rand. Ab 8 Gegnerarten
kleinere Symbole (24 statt 32 px), weniger Abstand, ganz besiegte Arten (ausser Boss) ausgeblendet. Zeilentext kuerzer:
„40 + 7 offen · 0 / 47 besiegt“, Schadenszeile ohne das Wort „Schaden“.

**Nachweis:** P12-E2 (17.11.), Foto W42 mit 10 Arten und Schadensblock ganz im Bild. Das Ausblenden besiegter Arten war auf dem
Foto noch nicht zu sehen (keine Art ganz besiegt). Commit `c103d66`.

**Simulation:** nicht betroffen.

**Changelog EN:** The wave panel on the left now fits on screen in large waves: compact lines, fully defeated enemy types are hidden.

**Changelog DE:** Die Wellenleiste links passt auch in großen Wellen ins Bild: kompakte Zeilen, ganz besiegte Gegnerarten werden ausgeblendet.
