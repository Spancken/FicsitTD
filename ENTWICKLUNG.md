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

## 5. Release

1. `entwicklung` ist grün in **Voll**, Thema(s) fertig. Version in `FicsitTD.uplugin` erhöhen.
2. Zettel zu Changelog zusammenfassen: `python Tests\pruefe_konsistenz.py --changelog` erzeugt den Entwurf (DE für README, EN für ficsit.app); in Handbuch (`#versions`), Mod-Seite, Release-Log übernehmen. Zettel danach nach `Aenderungen\vX.Y.Z\` verschieben.
3. Pipeline **Release**. G4 Marius.
4. Merge `entwicklung` → `master`, Tag `vX.Y.Z`. Push und Upload nur mit Marius' Freigabe.
