# Experimente und Ideen

Ideen, die wir prüfen, bevor sie fest eingebaut werden. Ablauf: `ENTWICKLUNG.md`, Abschnitt 2.

**Status:** `offen` (wird geprüft) · `uebernommen` (fester Bestandteil, Schalter darf Standard ≠ 0 haben bzw. Zweig gemergt) · `verworfen` (bleibt als Beleg, nicht im Release).

| Name | Art | Status | Idee | Prüfung / Ergebnis | Entscheidung |
|---|---|---|---|---|---|
| ShopPrototyp | Schalter | offen | Kontingente im echten AWESOME-Shop verkaufen (Fahrplan 0.12, Etappe E1): ein Posten „Kontingent Bolzenturm“, beliebig oft kaufbar, Preis steigt nach jedem Kauf. Ohne Schalter ist der Posten verborgen und nicht kaufbar, Hooks auf Shop und Schematic-Verwaltung werden nur mit Schalter eingerichtet. Der Prototyp vergibt kein Kontingent. | Plan `ShopPrototyp` im Testfenster 1, fünf Fragen (wiederholter Kauf, neuer Preis, `Unlock()` je Kauf, mehrere Stück, Quelle der Shop-Liste), dazu zwei Handkäufe von Marius im Shop-Fenster. Ergebnis: offen, gebaut 20.11., im Spiel ungeprüft. | offen – Weiche für Etappe E4 |

Art: `Schalter` = `Experiment.<Name>` in `TDBalance.cpp` · `Zweig` = `experiment/<name>`.
