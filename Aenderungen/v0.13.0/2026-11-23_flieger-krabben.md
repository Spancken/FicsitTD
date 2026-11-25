---
titel: Fliegende Krabben ab Welle 43 – Ziele für den Himmelsturm
typ: neu
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: ja
tests: Flieger
experiment:
---

**Was:**
- **Spawn:** Ab Welle 43 (`Katalog.Krabbe.AbWelle`) spawnt jede Welle fliegende Krabben des Spiels (`BP_FlyingBabyCrab`).
  - Anzahl: `Flieger.Anzahl` 6 + `Flieger.JeWelle` 0,5 × (Welle − 43), × Budget der Stufe, höchstens 24.
  - Sie verteilen sich auf die Fronten und starten 6 m über Grund.
- **Ziel:** Der Aggro-Radius ist 0, sie greifen den Spieler also nie an. Ihr Zuhause ist ein Punkt 4 m über dem Kern, dorthin fliegen sie ohne Netz und über Mauern.
- **Am Kern:** Unter 12 m Abstand richten sie 15 × (1 + 0,25 × Wellenzuwachs) Schaden an und explodieren.
- **Treffen:** Bodentürme suchen nur Bodenwesen und treffen sie deshalb nicht. Der Himmelsturm (sucht genau diese Klasse) und die Handwaffen treffen sie, ein Treffer tötet.
- **Wellenende:** Die Welle endet erst, wenn alle Krabben weg sind.
- **Ansage:** beim ersten Mal „FLIEGER!“.
- **Code:** `TDWaveSubsystem.cpp` (`SpawneFlieger`, `PflegeFlieger`, `EntferneFlieger`).
- **Simulator:** Krabben ab W43. Der gute Spieler forscht den Himmelsturm und stellt je 8 Krabben einen auf (`FliegerJeHimmelsturm`). Nicht abgefangene Krabben treffen den Kern.

**Warum:** Gegencheck 0.13 Konzept, Frage 9. Marius am 23.11.: „Nein, Flieger bauen.“ Grundlage ist die Spezifikation `Notizen/Spezifikation-Endgame-E4-E7-2026-11-09.md` E5 (Marius 09.11.).

**Noch offen** (Spezifikation E5):
- Krabbennester an den Fronten
- Luftabwehr-Munition je Turmlinie (Flak-Bolzen, Flak-Zielsuche, Luftzünder, Plasma-Tesla, Flammensäule)
- eigene LP der Krabben: Im Spiel stirbt sie beim ersten Treffer.

**Changelog EN:** Flying crabs from wave 43 fly straight to the Core over walls. Ground turrets can't hit them, the Sky Tower and your hand weapons can.

**Changelog DE:** Fliegende Krabben ab Welle 43 fliegen über Mauern direkt zum Kern. Bodentürme treffen sie nicht, der Himmelsturm und deine Handwaffen schon.

**Nachweis:** Selbsttest `Flieger` grün (`2026-11-23_1603` und `_1621` Gegencheck-Nachtest, `_1726` Release): Krabben gespawnt, Aggro aus, Ansage, Himmelsturm trifft. Erster Lauf `_1456` rot durch Planfehler (Zielwelle).
