---
titel: Einsatz nach Gegencheck - Schlupfloecher zu, Stufe waehlbar, Phasenabschluss, Bestwert, Leiter Hart/Albtraum
typ: geaendert
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: ja
tests: EinsatzPhase, Grundtest
---

**Was (Gegencheck Einsatz 16.11., Entscheidungen Marius 16.11., Notizen `Gegencheck-Einsatz-Bericht.md`):**
- **Testsprung** „zu Welle N“ (Wellenleiste) nur noch im Entwicklermodus – in 0.10.2 brachte 1 → 50 rund 3 400 Coupons und 110 Festplatten ohne Kampf (A1).
- **„Neu starten“** für Spieler entfernt (A2): Es behielt Coupons, Turmlevel und Forschung und zahlte alle Wellen erneut. Ein neuer Lauf ist ein neues Spiel. „Durchgang+“ ist eine Idee für später. Im Entwicklermodus bleibt der Knopf für Selbsttests.
- **Freies Bauen** nur im Entwicklermodus (A3); alte Stände schalten es beim Laden ab.
- **Startkit** je Kern nur einmal, auch über „Einsatz beenden“ (A5); ohne Entwicklermodus kein zweiter Einsatz von Hand am selben Kern.
- **Kernreparatur** nach der Niederlage gesperrt (A7, kostete Coupons und heilte nichts). A8 (Neu starten nach Laden mit 30 Wellen/Normal) entfällt für Spieler mit dem Knopf.
- **Stufe wählbar:** Sitzungseinstellung „Stufe“ (`FicsitTD.StufeInt`, Asset `SessionSetting_Stufe` aus `Scripts/create_session_stufe.py`), Standard „nach Startort“, gilt für Einsatz und Endlos. Hart und Albtraum sind als „ungetestet“ markiert (Auswahl, Startmeldung).
- **Leiter geglättet** (Simulation `Tools/Simulator/varianten_einsatz.py`): Hart 2,0 × 1,6 → 1,1 × 1,0; Albtraum 2,0 × 1,7 → 1,1 × 1,1 und Kern-LP 10 000 → 15 000. Frühere Eliten, mehr Fronten und weniger Kern-LP machen die Stufen schon schwer genug.
- **Phasenabschluss** nach Welle 10/20/30/40: Kurzbericht (Gegner, tiefster Kernstand, Coupons, Festplatten, Schaden je Turmart) und Bonuswahl wie im Endlos-Modus (`Einsatz.Phasenabschluss`, Boni `Endlos.Bonus.*`).
- **Abschlussbericht und Bestwert** nach Sieg und Niederlage: Bestwert je Kernplatz und Stufe in `Saved/FicsitTD/Bestwerte.json`; Läufe mit Eingriffen (Testsprung, `/gib`, Freies Bauen …, Zähler `mLaufEingriffe` am Kern) ohne Wertung.
- **Länge bleibt fest 50** (Marius: ein Lauf darf bis ins Endgame dauern), **kein Frühstart-Bonus**, **Material mit dem Kontingent bleibt** (Doku jetzt ehrlich).
- Doku: 13 Abweichungen aus dem Bericht korrigiert (D1–D7, D9, D11, D12, D14–D16, D22 teilweise); Handbuch DE/EN, Mod-Seite, Tester-Anleitung.

**Simulation:** Erfolg W50 Hart 0 % → 24 %, Albtraum 0 % → 14 % (Bänder 15–55 % / 5–35 %). Normal unverändert 76 % (über dem Band, Fahrplan P6). Die Bonuswahl ist im Modell nicht enthalten. Alle Läufe scheitern im Median um W10 (Wellenberg B8).

**Offen:** Im Spiel nicht nachgewiesen: Stufen-Auswahl im Menü, Phasenabschluss, Bestwert, Abschlussbericht (Fenster A/B). A4 (Kern zubauen), A9–A13 und D8, D10, D13, D17–D24 bleiben im Fahrplan.

**Changelog EN:** Operation: the test jump, Free Build and “Restart” are developer tools now – a new run is a new game. Choose the difficulty in the session settings (default: by start location); Hard and Nightmare were retuned and are marked untested. Every tenth wave closes a phase with a short report and a bonus pick; a final report and a best result per core site and difficulty end each run.

**Changelog DE:** Einsatz: Testsprung, Freies Bauen und „Neu starten“ sind jetzt Entwicklerwerkzeuge – ein neuer Lauf ist ein neues Spiel. Die Stufe wählt man in den Sitzungseinstellungen (Standard: nach Startort); Hart und Albtraum sind neu eingestellt und als ungetestet markiert. Jede zehnte Welle schließt eine Phase mit Kurzbericht und Bonuswahl ab; ein Abschlussbericht und ein Bestwert je Kernplatz und Stufe beenden jeden Lauf.
