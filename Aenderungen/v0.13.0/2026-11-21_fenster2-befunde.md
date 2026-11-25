---
titel: Befunde Testfenster 2 – Balance-Schlüssel Shop-Kontingent, Meldungen je Spiel, Shop-Plan nach Balance-Preisen
typ: intern
modi: einsatz, endlos
sichtbar: nein
doku: nicht-noetig
simulation: nicht-noetig
tests: Sperren-1121, Shop
experiment:
---

**Was:**
- **T2-1:** Startkontingent „AWESOME-Shop“ fragte den Balance-Schlüssel `Einsatz.Start.AWESOME-Shop`, der in der Tabelle fehlte (Warnung, Wert 1 als Ersatz). Schlüssel mit 1 eingetragen.
- **T2-2:** Die Zeile „Abriss: Sperre der Kernplattform eingerichtet“ steht nur beim Modulstart im Log, vor jedem Plan. Jetzt meldet jeder Einsatzstart „Abriss: Sperre der Kernplattform aktiv (2 Hooks …)“ (`GTDAbrissHooks`), der Plan `Sperren-1121` prüft diese Zeile.
- **T2-3:** „Shop: N Artikel des Spiels im Einsatz ausgeblendet“ wurde nur gemeldet, wenn sich die Zahl änderte, auch über Spielstarts hinweg (statischer Merker). Jetzt auch nach jedem Weltwechsel.
- **T2-4 (C8 geklärt):** Der Kern verkaufte den Bolzenturm für 4 Coupons, der Plan `Shop` rechnete mit 5. Maßgeblich ist die Balance-Tabelle (`Einsatz.Laden.Bolzenturm.Grundpreis` 4, Spielregeln-Tabelle), die Zahl in der Ladenliste ist nur der Ersatzwert. Plan auf 4/5/6/8/9 umgestellt, im Spiel waren alle Käufe stimmig (5, dann 6 + 8 = 14, Festplatte 40).

**Warum:** Testfenster 2 am 21.11. (`Testberichte\2026-11-21_1659_Fenster-2.md`): 5 Fehler in `Sperren-1121` und `Shop`, keiner davon ein Spielfehler außer der Warnung T2-1.

**Nachweis:** Nachlauf `Sperren-1121` und `Shop`.
