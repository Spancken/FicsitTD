# Entwicklung von FicsitTD – Ablauf und Deploy-Straße

Stand 16.11.2026 (Marius: „agil, am Ende immer eine auf ficsit.app deploybare Version; Doku, Testwerkzeuge, Simulation und Spielversion laufen nicht auseinander; Forks und Ideen, die man prüft und verwirft, brauchen eine eigene Mechanik“).

Prüfstufen G0–G6 und Upload: `Satisfactory\Notizen\Release-Prozess.md` (OneDrive). Dieses Dokument regelt, **wie Änderungen ins Repo kommen**. Die Pipeline (`Tools\Pipeline.ps1`) führt die Prüfstufen aus.

## 1. Was im Repo liegt

| Ordner | Inhalt | Versioniert mit dem Code, weil … |
|---|---|---|
| `Source`, `Content`, `Config`, `Scripts` | Mod | – |
| `Tests` | Autotest-Pläne, Pakete, Prüfskripte | Pläne gehören zum Code-Stand, den sie prüfen |
| `Tools\Simulator` | Balancing-Simulator für alle Spielmodi | Modell muss zum Code passen (`sim_stand.json`) |
| `Tools\Pipeline.ps1` | Deploy-Straße | – |
| `Doku\Spielregeln.md` | Spielregeln mit allen Werten | Regeln = Code |
| `Doku\Release\` | Tester-Anleitung, Rückmeldebogen, Mod-Seite (ficsit.app), Release-Log, Ankündigungen | Spielertexte je Version |
| `docs\` | Handbuch DE/EN (GitHub Pages) | Spielertexte je Version |
| `Aenderungen\` | Änderungszettel, eine Datei je Änderung | Changelog entsteht daraus |
| `Experimente.md` | Liste der Ideen und Versuche | – |

In OneDrive bleiben Notizen, Konzepte, Gegenchecks, Testberichte und Testspielstände.

## 2. Zweige

| Zweig | Zweck | Regel |
|---|---|---|
| `master` | veröffentlichte Versionen | nur Releases, jeder Stand hat einen Tag `vX.Y.Z` und ist auf ficsit.app deploybar. Kein direkter Commit. |
| `entwicklung` | nächste Version | nur Themen, deren Pipeline **Voll** grün ist. Immer baubar. |
| `thema/<name>` | ein Thema (Funktion, Fehler, Balance) | von `entwicklung`, zurück nach `entwicklung` per Merge, wenn fertig |
| `experiment/<name>` | Idee, die wir prüfen und vielleicht verwerfen | eigener Eintrag in `Experimente.md`. Wird übernommen (Merge nach `entwicklung`) oder verworfen (Zweig bleibt mit Tag `verworfen/<name>`, wird nicht gemergt) |

Kleine Balance-Ideen brauchen keinen Zweig: **Schalter** `Experiment.<Name>` in der Balance-Tabelle (`TDBalance.cpp`), Standard `0`. Im Code: `if (TDBalance::Wert(TEXT("Experiment.<Name>"), 0.f) > 0.f) { … }`. Ein- und ausschalten per `Balance.json` oder Testplan (`"balance": { "Experiment.<Name>": 1 }`). Die Pipeline lehnt ein Release ab, wenn ein Schalter mit Standard ≠ 0 nicht als „übernommen“ in `Experimente.md` steht.

## 3. Definition of Done einer Änderung

Eine Änderung ist fertig, wenn:
1. **Änderungszettel** in `Aenderungen\` liegt (Vorlage `Aenderungen\_Vorlage.md`): was, warum, welche Modi, Status von Doku, Simulation und Tests.
2. **Doku** nachgezogen ist: Handbuch DE/EN, Spielregeln, Tester-Anleitung/Rückmeldebogen/Mod-Seite, soweit sichtbar (Regel „Anleitung mitpflegen“).
3. **Simulation** nachgezogen ist, wenn Balance, Wellen, Türme, Wirtschaft oder ein Modus betroffen sind: `python Tools\Simulator\sim_alle_modi.py` läuft, `Tools\Simulator\sim_stand.json` passt zum Code.
4. **Tests**: ein Plan in `Tests\Plaene\`, der die Änderung belegt, grün; neue Funktionen kommen ins passende Paket (`Schnell`/`Release`).
5. Pipeline **Voll** grün (Abschnitt 4).

## 4. Pipeline

```
& E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Tools\Pipeline.ps1 -Stufe Schnell|Voll|Release
```

| Stufe | Wofür | Schritte | Dauer |
|---|---|---|---|
| **Schnell** | während der Arbeit an einem Thema | G0 Build + statische Prüfung, Konsistenz (Hinweise), Simulation, Paket `Schnell` | ~15 min |
| **Voll** | bevor ein Thema nach `entwicklung` geht | wie Schnell, Konsistenz streng, G2 `Release` + `ReleaseEn` | ~45 min |
| **Release** | Kandidat für ficsit.app | wie Voll, G3 `ReleaseLang` (Minor), Release-Zip + G5, Versionsabgleich | ~2 h |

Ergebnis: Bericht `Satisfactory\Testberichte\<Zeit>_Pipeline_<Stufe>.md` mit **DEPLOYBAR: ja/nein** und der Liste, was fehlt. G4 (Handspiel Marius) bleibt Handarbeit und steht im Bericht als offener Punkt.

Die Konsistenzprüfung (`Tests\pruefe_konsistenz.py`) prüft:
- Code geändert seit dem letzten Tag, aber kein Änderungszettel.
- Zettel unvollständig oder mit „Doku/Simulation/Tests: nein“.
- Simulation nicht nachgezogen (`sim_stand.json` passt nicht zu den Quelldateien).
- Experiment-Schalter an, ohne übernommen zu sein.
- Version in `FicsitTD.uplugin`, Handbuch-Abzeichen, Release-Log und Mod-Seite gleich (Stufe Release).
- Handbuch: gleich viele deutsche wie englische Abschnitte.
- Testpläne aus den Zetteln vorhanden.

**Optik** (seit 16.11.2026, Optik-Durchgang, Bericht `Satisfactory\Notizen\Optik-Durchgang-2026-11-16.md`), in allen Stufen:

- **`Tests\pruefe_optik_statisch.py`** (ohne Editor, Sekunden), Schritt „Optik statisch“. Er prüft:
  - Original-Anschlussteile (C++ `OriginalTeil`) im Klarraum der Bauvorschau
  - kein Original-Statusmast und kein Wand-Strommast (Entscheidung 16.11.)
  - Bandeingang nicht höher als der Sockel
  - Backfarben in `td_backen.py` nicht dunkler als die Spielwerte (Gummi ≥ 0,03)
  - keine großen fast schwarzen Flächen in den gebackenen Texturen
  - Flächen ohne AO (nur Hinweis)
- **Sperrklinke `Tests\optik_basis.json`:**
  - Bekannte Fehler (Stand 16.11.) bleiben grün, jeder neue Fehler macht rot.
  - Wer Fehler behebt, verkleinert die Basis mit `--basis-schreiben`.
  - `--streng` ignoriert die Basis.
- **`Scripts\pruefe_optik.py`** (Editor-Commandlet, nur lesen, ~3 min, nach jedem Asset-Skript). Er prüft:
  - geerbte Nebelflächen (`mFogPlaneLocations`)
  - Komponenten des Spiel-Vorbilds
  - Material-Slots ohne Material oder mit Standardmaterial
  - Anschluss-Komponenten außerhalb bzw. tief in der Sockelfläche

  Er schreibt `E:\Modding\Logs\pruefe_optik.json` und `Tests\Modellmasse.json`; die Sockelhöhen daraus nutzt die statische Prüfung.
- **`Tests\pruefe_ao.py`** (Blender, ~20 s), Schritt „Optik AO“. Er misst je gebackenem Teil den Anteil sichtbarer Fläche mit AO < 0,1 (`Satisfactory\Modelle\td_ao_sichtbar.py`; Kontaktflächen und schmale Spalte gelten als verdeckt).
  - Sperrklinke `Tests\ao_basis.json`: Fehler, wenn ein Teil mehr als 3 Prozentpunkte schlechter wird oder ein neues Teil über 20 % liegt.
  - Ohne Blender oder Modellquellen nur ein Hinweis.
- **Selbsttest `Optik`** (Plan `Tests\Plaene\Optik.json`, Testfenster): Prüfung `pruefe: Optik` auf Level 1 und 5 (kein Original-Strom-/Statusmast, keine Nebelfläche, Stromanschluss im Klarraum), Fotos aller Bauten und dreier Spielgebäude als Vergleich. Danach `python Tools\kontaktabzug.py` für den Kontaktabzug (P4).
- **Modelle:** `Satisfactory\Modelle\td_deckgleich.py` findet sichtbare deckungsgleiche Flächen (Z-Fighting) und schiebt mit `--beheben` das kleinere Teil 3 mm heraus.

## 4a. Testfenster (Marius 16.11.)

Kein Testen von Fehler zu Fehler. Zwischen den Testfenstern läuft nur `Pipeline.ps1 -OhneSpiel` (Build, Prüfungen, Simulation) – kein Spielstart. Ein Testfenster gibt Marius frei; was darin läuft, steht vorher im Fahrplan (`Satisfactory\Notizen\Fahrplan-<Version>-*.md`, Abschnitt Testfenster), danach kommt ein Bericht mit allen Befunden. Befunde werden gesammelt (Befundliste im Fahrplan) und im nächsten Fenster mitgetestet; nach zwei erfolglosen Anläufen entscheidet Marius (bekannter Fehler oder Experiment).

## 4b. Spielersicht und Fabrik-Prüfstand (16.11.)

- **Spielersicht:** Alle Selbsttests laufen im Entwicklermodus. Der Schritt `"spielersicht": true` schaltet ihn zur Laufzeit ab (`UTDKernplatz::SetzeSpielersicht`), `"pruefe": "TestKnoepfe"` prüft die sichtbaren Entwickler-Knöpfe im Kern-Menü, `"befehl"` mit `"antwortEnthaelt"` prüft die Ablehnung. Jeder Plan beginnt wieder mit Entwicklermodus. Plan `Spielersicht` gehört in jede Regression.
- **Fabrik-Prüfstand** (`Notizen\Konzept-Fabrik-Pruefstand-2026-11-16.md`): `"fabrik": "Bolzen"` baut eine echte Munitionslinie mit Spielgebäuden und Bändern (`TDFabrikPruefstand.cpp`, Bänder über die Testbibliothek des Spiels), `"fabrikbericht"` meldet Produktivität und Munition. Plan `Fabrik-F1`. Stufen F2–F4 (zweite Linie, Endlos mit Bauzeit, Bohrung).
- **Strom und Kosten im Prüfstand (17.11., P11):** `"kosten": "echt"` kauft Kontingent mit Coupons und nimmt das Baumaterial aus dem Inventar (`"kontingent"` nur Kauf, `"frei"` wie bisher). `"strom": "Biomasse"` baut Lager + Band + Brenner, `"strom": "Kohle"` Wasserpumpen am nächsten Gewässer, Rohre, Kohle-Miner mit Band und Leitung ans vorhandene Netz (Anlaufstrom aus Biomasse). `"netz": true` verkabelt alles ohne Netz. Die Sicherung setzt der Selbsttest nach `Test.SicherungZurueck` (20 s) zurück und zählt sie; unsichtbare Kabel werden gezählt (Ziel 0). `"erforsche"` mit `"echt": true` bezahlt Festplatten wie das MAM. `"strom": "Treibstoff"` baut am nächsten Ölknoten (bis 2 km) Ölpumpe, Raffinerien mit Harzlager und Treibstoffkraftwerke und führt eine lange Leitung ans Netz. Rohre fließen immer vom Erzeuger zum Verbraucher. Pläne `Fabrik-S0` (Biomasse), `Fabrik-S1` (+ Kohle ab W6), `Fabrik-S2` (+ Treibstoff ab W15).

## 5. Release

1. `entwicklung` ist grün in **Voll**, Thema(s) fertig. Version in `FicsitTD.uplugin` erhöhen.
2. Zettel zu Changelog zusammenfassen: `python Tests\pruefe_konsistenz.py --changelog` erzeugt den Entwurf (DE für README, EN für ficsit.app); in Handbuch (`#versions`), Mod-Seite, Release-Log übernehmen. Zettel danach nach `Aenderungen\vX.Y.Z\` verschieben.
3. Pipeline **Release**. G4 Marius.
4. Merge `entwicklung` → `master`, Tag `vX.Y.Z`. Push und Upload nur mit Marius' Freigabe.
