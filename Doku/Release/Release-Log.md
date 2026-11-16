# FicsitTD – Release-Log (Änderungsmanagement)

Eine Zeile je veröffentlichter Version auf ficsit.app, neueste zuerst. Ablauf und Prüfstufen: `..\Notizen\Release-Prozess.md`. Texte der Mod-Seite: `ModSeite-ficsit-app.md`. Details je Version: Versionsblöcke in `..\Mods\README.md`.

| Version | Veröffentlicht | Inhalt | Prüfstufen | Git | ficsit.app | Kommunikation |
|---|---|---|---|---|---|---|
| **0.10.2** (enthält 0.10.0 und 0.10.1) | 15.11.2026 11:16 (Upload Marius, Virenscan durch) | Pionier mit vier Rollen, Coupons und Festplatten als Spiel-Items, Spielmodus-Auswahl im Menü, Absturz-Fix Laufprotokoll, Frontprüfung, Anzeige-/Textfehler, Mess-Hooks nur im Entwicklermodus | G0 Build ok; G1 `Offen010`, `Netzumbau`, `Waehrung010`; G2 DE 12/14 (je 1 Felspfeiler-Rest in `BautenP2`, `Wege-Marius-W06`, wie 0.10.0), EN 5/5; G3 bei 0.10.0 (`2026-11-14_1542_ReleaseLang`); G4 Handspiel **offen**; G5 mit der Zip bestanden (`2026-11-15_1002_ReleaseZip`) | Tag `v0.10.2` = `c508252`, gepusht 15.11. (`master` = `89422ed`) | Version https://ficsit.app/mod/FicsitTD/version/7ZJm86Vyrzghmn, Changelog 0.10.2/0.10.1/0.10.0; Full description neu (What's new, englische Screenshots; Aufbau nach Marius: erst Text, dann drei Vorschaubilder 480 × 270 in einer Reihe, Klick öffnet die große Fassung), von Claude am 15.11. gespeichert; Handbuch mit Versionsabzeichen und Abschnitt „Versions & changes“ | Ko-fi-Beitrag veröffentlicht 15.11. (https://ko-fi.com/post/FICSIT-Tower-Defense-0-10-2-meet-the-Pioneer-K2C22APA0V, Bild `hero.jpg`); Discord offen (Kanal klären) |
| 0.9.6 | 12.11.2026 10:21 | Review-Funde, Einsatzende, Speichersperre, Spieler P0/P1, Fehler melden, Handbuch auf GitHub Pages | G2-Vorläufer (Selbsttests), Release-Zip | Tag `v0.9.6` | https://ficsit.app/mod/FicsitTD/version/BTHrrhrezg4mf7 | Ko-fi-Beitrag veröffentlicht |

## Regeln

- Hochgeladen wird **genau die Zip, die G5 bestanden hat**. Wird neu paketiert, G5 wiederholen.
- Vor dem Upload: Push von `master` und Tag, damit Handbuch und Bilder (GitHub Pages) zur Version passen.
- Nach dem Upload: Versionsliste prüfen (erst nach dem Virenscan sichtbar), Full description aktualisieren, Eintrag hier, Checkliste in `ModSeite-ficsit-app.md`, READMEs.
- Bilder auf ficsit.app: ficsit.app zeigt sie in Originalgröße. Deshalb nur kleine Vorschaubilder (`docs/img/klein/`, 480 × 270), höchstens drei, verlinkt auf die große Fassung.
- Kommunikation (Ko-fi, Discord) erst, wenn die Version sichtbar ist. Texte hier verlinken.
- Upload abgeschickt heißt: Seite springt auf `/version/<id>`. Steht der Tab noch auf `/new-version`, ist nichts hochgeladen (15.11.: Feld „Änderungen“ war leer, Erstellen fehlte).
