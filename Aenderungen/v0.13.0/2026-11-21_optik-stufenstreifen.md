---
titel: Plan Optik-Stufen – Stufenstreifen L1 bis L5 je Turm für die Optik-Abnahme
typ: intern
modi: einsatz
sichtbar: nein
doku: nicht-noetig
simulation: nicht-noetig
tests: Optik-Stufen
experiment:
---

**Was:** Neuer Testplan `Optik-Stufen`: die sieben Türme bauen, nacheinander auf Level 1 bis 5 aufwerten, nach jedem Schritt die Laufzeitprüfung `Optik` und je Turm ein Foto aus derselben Stelle (30°, 16 m). `Tools\kontaktabzug.py --stufen` legt daraus je Turm eine Zeile Level 1–5 an. Startort, Versorgung und Bau-Schritte sind aus `Optik.json` übernommen.

Der Selbsttest-Schritt `stelle` kennt dafür das Feld `blickhoehe` (m über dem Fuß des Baus, sonst 2,5 m); der Plan stellt 16 m weg und blickt auf 4 m. Dazu vier Balance-Schlüssel, die bisher nur als Ersatzwert im Code standen (`Tuerme.Level.Himmelsturm.L3.Takt` 0,25, `.L4.Reichweite` 0,15, `Tuerme.Level.Singularitaet.L3.Takt` 0,2, `.L4.Reichweite` 0,15, Werte unverändert, nur ohne Warnung).

**Warum:** Bericht A3 (20.11.), Paket O-A: Der Kontaktabzug vom 16.11. zeigt nur Level 1 und 5, die Stufen 2–4 fehlen. Grundlage für Marius' Optik-Abnahme (O1, E6). Noch offen aus O-A: freie Kamera auf den einzelnen Bau, Schaureihe statt Ring, Sicht von oben, Zustandsstreifen, C5 (16:9 im Ultrawide).

**Nachweis:** Lauf `Teste-Mod.ps1 -Plan Optik-Stufen`, Kontaktabzug `<Zeit>_Optik-Stufen_Kontaktabzug.html` in `Satisfactory\Testberichte`.
