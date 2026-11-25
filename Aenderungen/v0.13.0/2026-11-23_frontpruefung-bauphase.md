---
titel: Frontprüfung und Klippenprobe schon in der Bauphase (Wegvorschau stimmt vor dem Start)
typ: neu
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: BautenP1-Frueh
experiment:
---

**Was:**
- Neuer Schalter `Gegner.FrontPruefungAufbau` (Sekunden nach Beginn der Bauphase). **Standard 5 s, eingeschaltet** am 23.11. abends („Yes, turn it on“); 0 = aus wie vorher.
- Ist er gesetzt, gleicht der Mod die Fronten ab und probt die Klippen schon in der Bauphase. Die Wegvorschau und die Frontmarker zeigen dann den Weg, den die Welle wirklich nimmt.
- Beim Start prüft er noch einmal, denn Bauten können die Wege seitdem verschoben haben.
- Code: `UTDWaveSubsystem` Tick (Aufbau), `mAufbauUhr`, `mFrontCheckFruehWelle`.

**Warum:** Im Einsatz steht der Countdown. Frontprüfung und Klippenprobe liefen deshalb erst beim Druck auf „Welle starten“, die Sperrflächen verlegten die Wege danach. In BautenP1 lag das Ölschlick-Feld nach der Wegvorschau so neben dem neuen Weg (Gebremst 0). Ob das an ist, entscheidet Marius; offen seit dem Bericht vom 23.11.

**Changelog EN:** The enemy path preview now shows the final route before you start the wave – cliffs are checked during the build phase, not when you press Start wave.

**Changelog DE:** Die Wegvorschau zeigt schon vor dem Wellenstart den endgültigen Weg der Gegner – die Klippen werden in der Bauphase geprüft, nicht erst beim Druck auf „Welle starten“.

**Nachweis:**
- Build 23.11. abends, Plan `BautenP1-Frueh` (`Abend-2311`, `2026-11-23_2005`).
- Der Schalter wirkt: „Frontpruefung fuer Welle 1 in der Bauphase“. Die Klippenprobe lief, bevor das Ölschlick-Feld gebaut wurde, beim Start kam nur noch eine Sperre dazu. Ab Welle 11 lief sie 5 s nach Beginn der Bauphase.
- **Ölschlick trotzdem Gebremst 0:** Das BautenP1-Problem liegt also nicht (nur) am Zeitpunkt, sondern im Testaufbau. Die Ursache ist offen.
- Nachtrag `Slick` (`2026-11-23_2134`): Nach dem Ölschlick-Fix ist **BautenP1-Frueh grün** (mit Schalter). BautenE3a ohne Schalter ist rot: Das Feld steht vor dem Wellenstart, danach verschieben sich die Wege. Das spricht dafür, den Schalter einzuschalten.
- Marius 23.11. abends: einschalten. Standard jetzt 5 s.
