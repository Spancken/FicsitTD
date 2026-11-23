---
titel: Ölschlick-Feld wieder begehbar (keine Sperrfläche im Gegnernetz)
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: BautenP1, BautenP1-Frueh, BautenE3a
experiment:
---

**Was:** Das Ölschlick-Feld bekommt keine Sperrfläche mehr im Navigationsnetz der Gegner (`UTDWaveSubsystem::BrauchtBauSperre`, Schalter `Gegner.SlickBegehbar`, Standard 1). Die Gegner laufen wieder über die Platte statt um sie herum.

**Warum:**
- Seit N1 (22.11., Maschinen und Türme als Sperrflächen) galt das Ölschlick-Feld als Turm. Log: „BauSperren: Kasten fuer Build_TD_SlickPlate_C 9.0 x 8.1 x 5.8 m“.
- Die Gegner wichen der Platte aus. BautenP1 zeigte in vier Läufen Gebremst 0, auch nach den Plan-Korrekturen (Forschung, Bau nach Welle 1, frühe Frontprüfung).
- BautenE3a bremste trotzdem 62 Gegner, weil sich dort einige im Gedränge über die Platte schoben.

**Changelog EN:** The Oil Slick Field slows enemies again: since the last test build they walked around it.

**Changelog DE:** Das Ölschlick-Feld bremst wieder: Die Gegner sind zuletzt um die Platte herumgelaufen.

**Nachweis:**
- Build 23.11. abends, Paket `Slick` (`2026-11-23_2134`).
- **BautenP1 grün, Gebremst 82** (zuletzt grün am 09.11.). **BautenP1-Frueh grün.**
- BautenE3a rot (Gebremst 0): Dort steht das Feld schon vor Welle 1, beim Wellenstart verlegt die Klippenprobe die Wege. Das ist das Thema Wegvorschau (Schalter `Gegner.FrontPruefungAufbau`), kein Fehler des Felds. Mit dem Schalter (BautenP1-Frueh) liegt das Feld auf dem Weg.
