---
titel: ADA-Hinweis "Hogs brauchen zwei Bolzen" vor der ersten Overkill-Welle
typ: neu
modi: einsatz
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: P12-K1-Bolzen (DE und EN)
experiment:
---

**Was:**
- ADA sagt im Einsatz einmal je Lauf den Hinweis `ZweiBolzen`, und zwar zu Beginn der Bauphase vor der ersten Welle, in der ein Hog mehr LP hat als der stärkste Bolzenturm je Treffer Schaden macht. Sprecher und Stimme wie bei den übrigen Ansagen.
- Auf Normal mit Bolzen L2 ist das W5 (Hog 53 gegen 48), mit L1 W4, auf Erkundung W10. Ohne Bolzentürme kommt kein Hinweis. Endlos und Bohrung haben ihn nicht.
- Technik: `UTDWaveSubsystem::LPFaktorFuer` (eigene Funktion, `SpawneKernWelle` nutzt sie auch), `ATDBoltTurret::SchadenJeTreffer`, `TDAda::PruefeBolzenSchwelle`.

**Warum:** Marius am 24.11. zu den Optionen gegen die frühe Wand: „E find ich nicht gut, erstmal D“ (sichtbar machen, keine Balance-Änderung).
Ursache der Wand ist der Overkill: Ab W5 braucht ein Hog zwei Bolzen, die wirksame Feuerkraft fällt auf 55 % (Kernschaden-Messung und Turmbilanzen vom 24.11.).

**Changelog EN:** ADA warns you once when Hogs start to take two bolts each.

**Changelog DE:** ADA warnt einmal, sobald Hogs zwei Bolzen brauchen.
