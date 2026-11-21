---
titel: Stand „Noch baubar“ in der HUD-Statusleiste statt im Notizblock der To-do-Liste
typ: behoben
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: Sperren-1121
experiment:
---

**Was:** Die HUD-Statusleiste oben links hat eine zweite Zeile: „Noch baubar: …“ oder, solange der freigegebene AWESOME-Shop noch nicht steht, die Aufgabe „Baue den AWESOME-Shop“. Der Mod schreibt keinen Block mehr in den Notizblock der To-do-Liste und entfernt einen alten Block (`Einsatz.ToDo.Block` = 1 stellt ihn wieder her). Die Einkaufsliste der To-do-Liste bleibt.

**Warum:** Befund C15, Entscheidung Marius 21.11. („eigenes HUD-Feld“). Das Spiel liest die private To-do-Liste nur beim Laden (`FGPlayerState.h`), der Block zeigte deshalb alte Werte und blieb nach einem Sprachwechsel deutsch.

**Nachweis:** Foto der HUD-Zeile in den Plänen von Fenster 2; nach dem Laden eines alten Stands ist der Block aus dem Notizblock verschwunden.

**Changelog EN:** The status line at the top left now has a second line with what you can still build (or the Shop task); the mod no longer writes a block into the To-do notes, which did not update.

**Changelog DE:** Die Statusleiste oben links hat eine zweite Zeile mit dem, was du noch bauen kannst (oder der Shop-Aufgabe); der Mod schreibt keinen Block mehr in die Notizen der To-do-Liste, der sich dort nicht aktualisierte.
