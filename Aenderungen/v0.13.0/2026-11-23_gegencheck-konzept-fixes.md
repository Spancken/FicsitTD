---
titel: Gegencheck 0.13 Konzept – Ölbrand-Kombo, Modus Aus, Erklärtexte, Zähler der Gegner-Hilfen, Öl-Messung, Labyrinth-Test
typ: behoben
modi: alle
sichtbar: ja
doku: ja
simulation: ja
tests: BautenP1, BautenE3a, Labyrinth, ModusAus, Flieger
experiment:
---

**Was** (Block 1 und 2 aus `Notizen/Uebergabe-Gegencheck-0.13-Konzept-2026-11-23.md`):
- **C6a Ölbrand:** Die Kombo meldete sich als „Ölbrand“, abgefragt wurde „Oelbrand“. Jetzt einheitlich „Oelbrand“ (`TDSlickPlate.cpp`), damit kommen Ansage, Erklärsatz und Zähler.
- **R4-A Modus Aus:** Die Schnellleiste wird nur noch mit FICSIT-Abwehr auf die höchste Bandstufe umgestellt (`TDBandUpgrade.cpp`).
- **R4-E Modus Aus:** keine MAM-Karten, kein Turmstatus-Log und keine Strommessung im Modus Aus (`TDWaveSubsystem.cpp`, `TDWellenStatistik.cpp`).
- **R5 Erklärtexte (DE/EN):**
  - Kontingent mit Verweis auf den Shop statt `/td einsatz status`
  - Bausperre in der Welle mit Grund
  - Blaupausen mit Grund
  - beim Einsatzstart ein Hinweis, warum es keine Fundamente, Wände und Rampen gibt (Marius 23.11., Frage 7: bleibt so)
- **R3 Hilfen gezählt:** Bewuchs geräumt, Klippe gesperrt, Stau versetzt, Netzinsel, Front verlegt, Rundungsrest (`TDNetz::MeldeHilfe`).
  - Die Prüfung „Sicherheitsnetze“ weist sie mit aus („| Hilfen: …“), ohne Einfluss auf das Ergebnis.
  - Die Durchlauf-Regeln für Weltobjekte und Logistik sind Einstellungen, keine Ereignisse, und werden nicht gezählt.
- **Frage 6, Öl:** neuer Entwicklerbefehl `/td oel`. Er misst für die zwölf festen Kernplätze die Entfernung zum nächsten Rohöl-Knoten und zur nächsten Ölquelle.
- **Frage 5, Labyrinth:**
  - Der Testschritt `spielbau` kennt `lueckeWinkel`.
  - Neuer Plan `Labyrinth`: drei Ringe aus TD-Mauern mit wechselnder Lücke, Bolzentürme innen, fünf Wellen, Sicherheitsnetze 0.
- **Doku (Block 1):**
  - Spielregeln: Störfeld ohne Gabelung, Anschlüsse nach dem Optik-Durchgang, Talentpunkte, Handwaffen, Flieger
  - Simulator-README: Grenzen des Spielermodells, Kern-Bonus im Endlos
  - `Konzept-Gegnersteuerung.md`: Regeln 4 und 5 sind nicht gebaut
- **Simulator-Modell:**
  - Armierpistole bis zum Gewehrkauf
  - Jäger 3 nur am Bossanteil
  - Endlos-Wachstum mit den Endlos-Schlüsseln
  - Kern-Bonus im Endlos +10 % Maximum statt 30 % Heilung

- **Nachtest 23.11. nachmittags (Selbsttest `Gegencheck`, 10 Fehler in 4 Plänen), Ursachen:**
  - **Labyrinth:** Die A*-Suche der Engine bricht nach 2048 Knoten ab und liefert dann einen Teilweg bis vor die äußere Mauer. Der Kern galt als „Durchbrechbar“, Gegner liefen in die Mauer (21 Nachzügler versetzt). Neu: `Wege.SuchKnoten` (Standard 8192) im Gegnerfilter `TDNavFilter`, Zähler `Hirn: Wegsuche an der Suchgrenze` im Log und im KernZugang-Log. Der Plan prüft A/B (2048 → Durchbrechbar, 8192 → Frei).
  - **Flieger:** Planfehler. `wellenlauf` nennt die Zielwelle, nicht die Zahl der Wellen. Nach dem Sprung auf Welle 43 endete `wellenlauf: 1` sofort, es wurde nie eine Welle gestartet. Jetzt `wellenlauf: 43`.
  - **BautenP1:** Planfehler. Der Plan erforschte den Ölschlick nicht (seit A6 nötig, BautenE3a tut es). Ohne Forschung hat die Platte kein wählbares Rezept und bekommt kein Öl. Jetzt mit Forschung wie E3a.
  - **Balance-Warnungen:** Zehn Verbrauchsrezepte (Öl, Lockfeuer, Flammen-Varianten, Reparatur) hatten keinen Schlüssel `Munition.SchussJeItem.*`. Sie stehen jetzt ausdrücklich auf 1, das Verhalten bleibt gleich.
  - **Nachtest 1/2:**
    - Mit 8192 Knoten läuft das Labyrinth 5 Wellen ohne Sicherheitsnetz und ohne Hilfe (Kern 94–96 %). Die Gegenprobe mit 2048 zeigt wieder „Durchbrechbar“.
    - Der Zähler zählt nur noch zurückgegebene Teilwege. Im Netz des Alpha-Hogs ist der Kern oft nicht erreichbar, die Suche läuft dort immer bis zur Grenze, danach findet das Standardnetz den vollen Weg.
  - **Kernplattform als Sperre (Fels-S8, auch Lauf 10:45):** Nach dem Bau der Plattform braucht das Netz dort bis 95 s, bis es sie anbindet. Die Ladeprüfung meldete in der Zeit „abgeriegelt durch Fundament 17 m Osten“, gezählt als Abriegelung. Ein Fundament der eigenen Plattform riegelt nie ab (Spieler bauen im Einsatz keine Fundamente). Endet der Weg dort, gilt jetzt „Netz nicht bereit“ (`Kernplattform (Netzumbau)`): Die Ladeprüfung versucht es erneut, der Wellenstart wartet wie beim Netzumbau.
    - Im Nachtest 2 blieb Fels-S8 in E3a 5 min ohne vollen Weg, bis gebaute Türme das Netz auffrischten. Die eine Auffrischung 18 s nach dem Bau war zu früh.
    - Der Kernplatz frischt das Netz um die Plattform jetzt alle 20 s neu auf, solange aus 100 m kein voller Weg zum Kern führt (`Kernplatz.NetzAuffrischTakt`). Er wartet bis 120 s statt 60 s (`Kernplatz.NetzWarteSekunden`).
    - Häufigkeit heute: 3 von 7 Plattformbauten auf Fels-S8.
  - **Himmelsturm/Singularität:** Schlüssel `Tuerme.Level.*.L2.Schaden` ergänzt (0,2 wie bisher).
  - **Selbsttest-Schritt `entferne spielbau`:** Mit `zerstoertErlaubt` zählt eine im Lauf von Gegnern zerstörte Mauer nicht als Fehler.
  - **Flieger-Plan:** Er läuft jetzt in der Felsenwüste, die Bodengegner haben per Balance 5 % LP. Im Grasland (Gras-S8) blieben große Gegner an der verlegten Front 1 hängen (siehe offen).
  - **BautenE3a, Festgefahren 1:** Ein Hog stand 48 s hinter einem Klippen-Hog an der Front. Das ist nicht neu (Lauf 09.11. ebenfalls 1 Fehler) und bleibt offen.

**Warum:** Gegencheck 0.13 Konzept (`Notizen/Gegencheck-0.13-Konzept-Bericht.md`), Marius am 23.11.: Block 1 und 2 umsetzen.

**Noch offen:**
- **Gras-S8, Front 1 (Nordosten, 427 m):** Die Klippenprobe verlegt den Startpunkt zweimal hinter Klippen (um X=-57570 Y=216277). Große Gegner (Alpha-Hog, Spucker-Alpha, Elite-Stinger) bleiben dort gleich nach dem Spawn hängen. Zweimal reproduziert (Flieger-Plan, W43). Braucht eine eigene feste Front oder einen anderen Abstand in `TDFesteFronten`.
- **Kosten der Suchgrenze:** Wenn der Kern in einem Netz nicht erreichbar ist (Alpha-Netz), prüft jede Suche jetzt 8192 statt 2048 Knoten. KernZugang 1,1 ms statt 0,3 ms. Die Bildrate im Lauf ist unauffällig (Mittel 115, schlechtestes Fenster 46 Bilder/s), gemessen ist es nicht einzeln.
- L1/L2/L4 (Messen und Beschleunigen der Sperrflächen) brauchen Spielläufe.
- R6 (Netzzustand bei KZ-1)
- `StatusTitel` im MAM
- der Rest von Modus Aus (R4-B/C/D): Marius entscheidet

**Changelog EN:** Fixes: the Oil Fire combo is announced and counted; Off mode leaves your quick bar alone; clearer messages for wave build lock, quotas, blueprints and missing foundations during an operation.

**Changelog DE:** Fehlerbehebungen: Die Ölbrand-Kombo wird angesagt und gezählt. Der Modus Aus lässt die Schnellleiste in Ruhe. Klarere Meldungen zu Bausperre, Kontingenten, Blaupausen und fehlenden Fundamenten im Einsatz.

**Nachweis:** Build 23.11. nachmittags. Selbsttests: `Gegencheck` (`_1456`, 10 Fehler, Ursachen oben), Nachtest `_1522`/`_1544`/`_1603`/`_1621`, `Release` 29/30 (`_1726`), `ReleaseEn` 10/10 (`_1741`). Labyrinth, Flieger, BautenE3a, ModusAus und Energie grün. BautenP1 rot (Testaufbau, offen). Die Netz-Auffrischung des Kernplatzes ist im Release-Lauf belegt (BautenP2, Fels-S8: Auffrischung nach 20/40/60 s, nach 64 s 4 von 4 Fronten).
