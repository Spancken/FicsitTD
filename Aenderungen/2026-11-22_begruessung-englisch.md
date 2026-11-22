---
titel: Neue ADA-Begrüßung in der Übersetzungstabelle, update_ada_text.py trägt sie künftig selbst ein
typ: behoben
modi: einsatz, endlos
sichtbar: nein
doku: nein
simulation: nicht-noetig
tests: ReleaseEn, Neustart
experiment:
---

**Was:**
- Die ADA-Begrüßung vom 22.11. (Boden 60 m, HUB → Shop → MAM) fehlte in `Scripts/uebersetzung_en.json`. Betroffen waren der ganze Text und vier neue Sätze.
- Der Englisch-Lauf hat sie gemeldet: `Uebersetzung-fehlt.json` enthielt 7 deutsche Einträge.
- Es gibt ein eigenes englisches Asset (`MSG_TD_Begruessung_en`), aber `TDSprache` schlägt auch das deutsche Asset nach.
- Jetzt eingetragen, `TDTexteEn.inl` ist neu erzeugt.
- `Scripts/update_ada_text.py` schreibt die Paare (ganzer Text und jeder Satz) künftig selbst in die Tabelle, wenn die Begrüßung neu gesetzt wird.
- Ungeklärt, aber nicht sichtbar: Die Kürzel „IR“ und „RA“ in der Fehlliste stammen aus den Audio-Assets der neuen ADA-Stimmen (vermutlich WAV-Metadaten).

**Warum:** Regel „Zweisprachig“: Neue Texte müssen im Englischen vollständig sein.

**Changelog EN:** –

**Changelog DE:** –

**Nachweis (22.11. abends):**
- `ReleaseEn` (`2026-11-22_2015`): 6 von 6 grün. Danach `Neustart` auf Englisch (`_2022`): grün, 21 Einträge in der Fehlliste, davon 0 deutsch (vorher 7).
- Vorher in diesem Testfenster: `Release` (`_2005`) 16 von 16 grün.
