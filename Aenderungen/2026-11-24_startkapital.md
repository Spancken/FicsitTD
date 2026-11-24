---
titel: Startkapital 40 Coupons im Einsatz
typ: balance
modi: einsatz
sichtbar: ja
doku: ja
simulation: ja
tests: Strat-MonoBolzen, Strat-Gewehr-Tesla, P12-K1-Bolzen, Release
experiment:
---

**Was:**
- Jeder neue Einsatz beginnt mit **40 FICSIT-Coupons** (`Einsatz.Start.Coupons`, `TDEinsatz.cpp`).
  - Sie kommen einmal je Einsatz, zusammen mit dem Startkit, auch nicht erneut über „Neu starten“.
  - Endlos-Wellen bekommen sie nicht.
  - Ausgegeben werden sie im AWESOME-Shop, der nach Welle 1 öffnet.
- Gegnerleben bleiben unverändert (Normal 1,25, Hart 1,25, Albtraum 1,35).
- Simulator: Startkapital in `strategien.py`, ausgebbar ab Welle 2.

**Warum:** Marius am 24.11.: „Yes, soften the early game“.
- Im Spiel ist der frühe Einsatz seit 0.13 eine Wand: Die Strat-Pläne mit 1–2 Türmen in W1–3 fielen in W5.
- Mit 4 Türmen ab Welle 1 hielt K1 bei denselben Gegnerleben bis W7–8. 40 Coupons reichen für etwa drei weitere Türme nach Welle 1.

**Verworfen (24.11. nachmittags):** Startkapital zusammen mit Gegnerleben 2,04/1,98/2,14, wie es der Simulator vorschlug. Im Spiel fielen damit alle vier Läufe in W6, also nicht später als vorher. Der Simulator schätzt die frühen Wellen etwa um den Faktor 2 zu günstig ein. Er wird erst wieder zum Einstellen genutzt, wenn er die Testläufe richtig vorhersagt. Mehr dazu in der Stand-Datei vom 24.11.

**Offen:** Ob die zweite Hälfte mit dem Startkapital zu leicht wird, zeigt erst das Handspiel G4.

**Changelog EN:** Every operation now starts with 40 FICSIT Coupons for your first turrets.

**Changelog DE:** Jeder Einsatz beginnt mit 40 FICSIT-Coupons für die ersten Türme.
