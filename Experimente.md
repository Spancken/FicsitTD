# Experimente und Ideen

Ideen, die wir prüfen, bevor sie fest eingebaut werden. Ablauf: `ENTWICKLUNG.md`, Abschnitt 2.

**Status:** `offen` (wird geprüft) · `uebernommen` (fester Bestandteil, Schalter darf Standard ≠ 0 haben bzw. Zweig gemergt) · `verworfen` (bleibt als Beleg, nicht im Release).

| Name | Art | Status | Idee | Prüfung / Ergebnis | Entscheidung |
|---|---|---|---|---|---|
| ShopPrototyp | Schalter | übernommen | Kontingente im echten AWESOME-Shop verkaufen (Fahrplan 0.12, Etappe E1). | Testfenster 1 (21.11.): wiederholter Kauf geht, das Spiel liest den neuen Preis, `Unlock()` einmal je Kaufvorgang, mehrere Stück ohne Staffel (2 × Preis), Liste aus `GetVisibleSchematicCategoryData`. | übernommen 21.11. in E4: Shop ohne Schalter (`TDShop.cpp`, Plan `Shop`), der Schalter wirkt nicht mehr |
| Gegner.FrontPruefungAufbau | Schalter | übernommen | Frontprüfung und Klippenprobe schon in der Bauphase statt beim Druck auf „Welle starten“, damit die Wegvorschau den echten Weg zeigt (23.11., BautenP1: Ölschlick lag nach der Vorschau neben dem Weg). Standard 0 = aus. | Plan `BautenP1-Frueh` (Schalter 5 s): Prüfung läuft in der Bauphase, Klippenprobe vor dem Bau; BautenP1-Ölschlick lag an der Sperrfläche (eigener Fix); danach BautenP1-Frueh grün, BautenE3a ohne Schalter rot. | übernommen 23.11. abends (Marius), Standard 5 s |

Art: `Schalter` = `Experiment.<Name>` in `TDBalance.cpp` · `Zweig` = `experiment/<name>`.
