---
titel: Handbuch – ganzer Forschungsbaum hinter einem Spoiler, Planer für MAM und Pionier-Talente
typ: neu
modi: alle
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: keine
experiment:
---

**Was** (Marius 22.11.: „vollständigen Baum in der Doku mit einem Spoiler-Button … Talentbaum je nach Welle durchspielen, Schieber für die Wellen, Punkte konkret vergeben“; Frage „welcher Baum“ beantwortet mit „Beide“):
- Neuer Abschnitt „Forschungsbaum & Planer“ im Handbuch (`docs/index.html#tree`). Der Inhalt erscheint erst nach dem Knopf „Ganzen Baum und Planer zeigen (Spoiler)“.
- **MAM:** alle vier Bäume mit Plätzen und Linien wie im Spiel, Welle, Preis, Gabelungen und Turmbildern.
  - Ein Klick erforscht einen Knoten. Ein Klick auf einen erforschten Knoten nimmt ihn zurück, samt allem, was darauf aufbaut.
  - Das Infofeld zeigt Text, Voraussetzungen, die Gegenseite der Gabel und die Folgeknoten.
- **Wellen-Schieber** „Vor Welle n“ (1–50, mit „Endlos“ bis 150). Die Festplatten werden wie im Spiel gerechnet: 2 je Welle, +1 bei Kern über 75 %, +3 je Bosswelle.
  - Wählbar sind der Boss-Bonus „Festplatten“ und gekaufte Festplatten (mit Coupon-Kosten nach der Preisstaffel).
- **Pionier:** Talentpunkte aus den geschafften Bosswellen plus Punkte aus der Bohrung, höchstens 15. Vier Rollen mit je fünf Stufen, die der Reihe nach gelernt werden.
- Wird die Welle kleiner, fällt alles weg, was nicht mehr passt: zu früh, ohne Voraussetzung oder über dem Budget.
- Der Plan steht in der Adresse (`#plan=…`), „Link kopieren“ teilt einen Build. Deutsch und Englisch wie das übrige Handbuch.
- **Daten aus dem Code:** `Scripts/erzeuge_doku_baeume.py` schreibt `docs/baeume-daten.js` aus `create_research.py`, `TDPionier.cpp`, `TDBalance.cpp` und `uebersetzung_en.json`. `Tests/pruefe_mod.py` erneuert die Datei bei jedem Bau, wie die Zahlen in den Spielregeln. Die Logik steht in `docs/baeume.js`.

**Warum:** Marius will Builds im Handbuch ausprobieren können, ohne das Spiel. Der Spoiler-Knopf hält den Baum für alle verborgen, die ihn im Spiel entdecken wollen.

**Changelog EN:** The player guide now has the full research tree behind a spoiler button, plus a planner: pick a wave, spend the Hard Drives and Talent Points you would have by then, and share your build as a link.

**Changelog DE:** Das Handbuch zeigt jetzt den ganzen Forschungsbaum hinter einem Spoiler-Knopf, dazu einen Planer: Welle wählen, die Festplatten und Talentpunkte bis dahin ausgeben und den Build als Link teilen.

**Nachweis (22.11.):** im Browser über einen lokalen Server geprüft (Desktop und Handybreite 375 px, Deutsch und Englisch, keine Konsolenfehler).
- Vor Welle 3 gibt es 6 Festplatten. Assembler und Band Mk2 erforscht ergibt 0 frei, Kupfer und Gießerei sind dann gestrichelt.
- Welle 30 mit Boss-Bonus und 2 gekauften Festplatten ergibt 105 Festplatten (2 gekauft für 90 Coupons) und 2 Talentpunkte. Der dritte Punkt aus einem Link wurde gestrichen.
- Nach Zielsuch ist Panzerbrechend ausgeschlossen, nach Schockbolzen ist Streubolzen ausgeschlossen.
- Auf dem Handy scrollt die Seite nicht seitlich, der Baum scrollt in seinem Kasten.
- Online ist der Abschnitt erst, wenn `docs/` auf dem Zweig von GitHub Pages liegt.
