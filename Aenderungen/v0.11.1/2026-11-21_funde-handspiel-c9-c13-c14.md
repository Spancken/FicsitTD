---
titel: Handspiel-Funde: Nobelisken-Zünder ab Caterium, Elektrozaun verkettbar, Klingenläufer nur als Ersatz
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: Ausruestung, BautenP2
experiment:
---

**Was:**
- **C14:** Der Nobelisken-Zünder ist ab der Forschung Caterium im Laden (vorher erst mit dem Nobelisk-Werfer) und kommt mit 10 Nobelisken (`Einsatz.Laden.Nobelisken-Zünder.Beigabe`). Neuer Posten „Nobelisken“: 5 Stück je Kauf, Grundpreis 5, jeder Kauf 20 % teurer, sichtbar erst mit dem Zünder.
- **C13:** Der Elektrozaun nimmt 2 Kabel (`Bauten.Elektrozaun.Kabel`). Eine Zaunreihe lässt sich vom Mast aus von Zaun zu Zaun verketten, wie beim Tor.
- **C9:** Klingenläufer stehen im Laden nur noch, wenn der Spieler keine besitzt, weder im Inventar noch angelegt. `/td kaufe Klingenläufer` antwortet sonst „hast du schon“.

**Warum:** Funde aus Marius' Handspiel vom 20.11. (Fahrplan 0.12, Befundliste; Bericht `Satisfactory\Notizen\Funde-Handspiel-2026-11-20.md`).
- C14: Das Caterium am Kernplatz lag unter einem sprengbaren Felsen. Nobelisken gab es nur über die Werfer-Forschung, mit Stahl- und Pulverkette, und der Zünder kam ohne Nobelisken: eine Sackgasse für jeden, der keine Nobelisk-Linie spielt.
- C13: Der Zaun ist eine Kopie des Konstruktors und erbte dessen Anschluss mit einem Kabel. Der Anschluss sitzt im Blueprint, deshalb setzt `ATDConductorWall::PostInitializeComponents` die Kabelzahl per Reflexion, vor dem Laden der Kabel.
- C9: Der Zähler des Ladens kannte das Startkit nicht.

**Nachweis:** Plan `Ausruestung`, erweitert: Klingenläufer abgelehnt; Zünder vor Caterium abgelehnt; Nobelisken vor dem Zünder abgelehnt; Zünder nach Caterium mit 10 Nobelisken; Nachkauf. Plan `BautenP2`: Prüfung `TurmWert Elektrozaun Kabel = 2`. Logzeile „Elektrozaun: 1 Anschluss, je 2 Kabel“. Offen für Marius' Spiel: zwei Zäune von Hand verketten, speichern, laden.

**Nicht geändert:** Der MAM-Text der Forschung Caterium nennt den Zünder noch nicht (Assets aus `create_research.py`, nächster Lauf).

**Changelog EN:** Nobelisk Detonator now unlocks with Caterium research and comes with 10 Nobelisks (more in the shop), so boulders on resource nodes can be blasted without the Nobelisk Launcher line; Electric Fences take 2 power wires and can be chained; spare Blade Runners only show up in the shop if you have none.

**Changelog DE:** Der Nobelisken-Zünder kommt mit der Forschung Caterium und bringt 10 Nobelisken mit (weitere im Laden), Felsen über Rohstoffknoten lassen sich so ohne die Nobelisk-Werfer-Linie sprengen; Elektrozäune nehmen 2 Kabel und lassen sich verketten; Ersatz-Klingenläufer stehen nur im Laden, wenn du keine hast.
