# Changelog 0.11.0 (Entwurf)

> Stand 18.11.2026, aus den Änderungszetteln in `Aenderungen\` (sichtbar: ja). Der englische Teil geht als `changelog.md` zu ficsit.app; die ersten Zeilen erscheinen im Discord-Kanal `#mod-updates`. Vor dem Upload prüfen: offene Punkte unten, Version, bekannte Probleme.

## English (ficsit.app)

**FICSIT Tower Defense 0.11.0 – three ways to play: Operation, Endless waves and Free play with defense.**

### New
- **Game Mode "Endless waves":** timed waves from one direction that never stop. Every boss wave gives a break and a bonus pick; a defeat ends the run with a best result per difficulty. From wave 50 enemies keep getting tougher, from wave 60 there are more enemies and more elites.
- **Game Mode "Free play with defense":** regular Satisfactory plus turrets and an optional drilling challenge at resource nodes. Nothing attacks unless you start it.
- **Operation phases:** every tenth wave closes a phase with a short report and a bonus pick; a final report and a best result per core site and difficulty end each run.
- **Ammo lasts longer:** one item fires several shots (basic ammo 4, advanced 3, special 2; rifle ammo 3/2/1 magazines). The turret window shows consumption per minute and how many turrets a Mk.1 belt can feed.
- New in the MAM: Conveyor Belt Mk.4 and Mk.5, Miner Mk.2 and Mk.3. Power Storage in the shop after Coal Power. You start with Blade Runners; the Jetpack (after Refinery) and the Hover Pack (after Heavy Industry) are in the shop.
- Turrets shield their ammo storage; researching the Singularity unlocks the Hazmat Suit and Iodine Filters.
- ADA comments on new enemy species and boss waves.

### Changed
- A new start window right after landing: choose Operation, Endless waves, Free play with defense or Off, and the difficulty. New games default to Operation. Existing worlds without a Defense Core stay regular Satisfactory.
- Choose the difficulty in the session settings (default: by start location). Hard and Nightmare were retuned and are still marked untested.
- Elite enemies appear later: from wave 16 instead of 13 on Normal, from wave 18 instead of 15 on Exploration.
- Rifle Turret master tier (Ammo Drum): +25 % fire rate.
- Game Mode "Off" is now plain Satisfactory (no turret recipes, no defense research).
- Test jump, Free Build and "Restart" are developer tools now – a new run is a new game. After ending an operation the world stays free to build.
- Fewer chat lines after each wave: one summary line, details in the Core Menu.
- Advanced game settings make a run unranked. The Core bonus permanently adds 10 % Core HP; unclaimed bonuses are kept.

### Fixed
- Wave enemies no longer get stuck on ore deposits, rock nodes, crab nests and nut or berry bushes next to their path, and less often at cliff overhangs.
- Each core site now has fixed enemy fronts, so a site always plays the same, no matter how fast the game loads. While the navigation mesh is still building after landing, the chat says the paths are still being checked instead of "No path checked".
- The wave panel on the left fits on screen in large waves.
- Xeno-Zapper and Xeno-Basher now count as hand weapons (self-defense: electric damage at about a third of a rebar bolt, growing with the waves).
- German texts now use proper umlauts.
- The New Game settings have short help texts and are easy to read again.
- Towers and buildings: no floating lamps, power poles or black panels, no flickering surfaces, darker patches removed.
- Wave saves carry the session name; "Retry wave" only loads saves of the current game. Turrets can no longer be dismantled near the Core during a wave.
- Free play with defense: drilling rewards and Pioneer rank belong to the save; destroyed buildings refund their materials during drilling; drilling only at real resource nodes; talent points capped at 15.

### Known issues
- Now and then an enemy gets stuck at a cliff or at a front's start point and is moved back onto its path after a few seconds.
- Hard and Nightmare are not fully balanced yet.

### Saves
- 0.10.2 saves load. Saves with Game Mode "Off" from older versions switch to Operation with `/td modus einsatz`.

## Deutsch (README, Block „Stand 0.11.0“)

**FicsitTD 0.11.0 – drei Spielweisen: Einsatz, Endlos-Wellen und Freies Spiel mit Abwehr.**

**Neu**
- Spielmodus „Endlos-Wellen“: Wellen auf Zeit aus einer Richtung, ohne Ende. Nach jeder Bosswelle Pause und Bonuswahl, eine Niederlage beendet den Lauf mit Bestwert je Stufe. Ab Welle 50 werden die Gegner stärker, ab Welle 60 kommen mehr Gegner und mehr Eliten.
- Spielmodus „Freies Spiel mit Abwehr“: normales Satisfactory mit Türmen und Bohrung auf Knopfdruck an Rohstoffknoten.
- Einsatz in Phasen: jede zehnte Welle schließt eine Phase mit Kurzbericht und Bonuswahl ab, ein Abschlussbericht und ein Bestwert je Kernplatz und Stufe beenden den Lauf.
- Munition hält länger: Ein Stück reicht für mehrere Schuss (Grundmunition 4, bessere 3, Spezialmunition 2; Gewehrmunition 3/2/1 Magazine). Das Turmfenster zeigt den Verbrauch pro Minute und wie viele Türme ein Band Mk1 versorgt.
- Neu im MAM: Förderband Mk4 und Mk5, Miner Mk2 und Mk3. Akku im Laden nach Kohlekraft. Klingenläufer gibt es von Anfang an, Jetpack (nach Raffinerie) und Hover-Pack (nach Schwerindustrie) im Laden.
- Türme schirmen ihre Munition ab; die Forschung Singularität schaltet Hazmat-Anzug und Iod-Filter frei.
- ADA kommentiert neue Gegnerarten und Bosswellen.

**Geändert**
- Neues Startfenster direkt nach der Landung: Einsatz, Endlos-Wellen, Freies Spiel mit Abwehr oder Aus wählen, dazu die Stufe. Neue Spiele stehen standardmäßig auf Einsatz. Bestehende Welten ohne Abwehrkern bleiben normales Satisfactory.
- Stufe in den Sitzungseinstellungen (Standard nach Startort); Hart und Albtraum neu eingestellt, weiter als ungetestet markiert.
- Elite-Gegner später: Normal ab Welle 16 statt 13, Erkundung ab Welle 18 statt 15.
- Gewehrturm-Meisterstufe (Gurttrommel): +25 % Feuerrate.
- Spielmodus „Aus“ ist reines Satisfactory.
- Testsprung, Freies Bauen und „Neu starten“ nur noch im Entwicklermodus; nach dem Beenden bleibt die Welt frei zum Weiterbauen.
- Weniger Chatzeilen: eine Sammelzeile je Welle, Details im Kern-Menü.
- Erweiterte Spieleinstellungen machen den Lauf wertungsfrei; Kern-Bonus dauerhaft +10 % Kern-LP, offene Boni bleiben.

**Behoben**
- Wellengegner bleiben nicht mehr an Erzvorkommen, Steinknoten, Krabbennestern und Büschen am Weg hängen, seltener an Felsüberhängen.
- Jeder Kernplatz hat feste Gegnerfronten – ein Platz spielt sich immer gleich, egal wie schnell das Spiel lädt. Solange das Navigationsnetz nach der Landung noch entsteht, meldet der Chat „Die Wege werden noch geprüft“ statt „Kein Weg geprüft“.
- Die Wellenleiste passt auch in großen Wellen ins Bild.
- Xeno-Zapper und Xeno-Basher zählen jetzt als Handwaffen (Notwehr: Stromschaden etwa ein Drittel eines Armierungsbolzens, wächst mit den Wellen).
- Deutsche Texte schreiben Umlaute jetzt richtig.
- Die Einstellungen im Menü „Neues Spiel“ haben kurze Hilfetexte und sind wieder gut lesbar.
- Optik: keine schwebenden Lampen, Strommasten oder schwarzen Platten, kein Flimmern, dunkle Flecken entfernt.
- Wellen-Spielstände tragen den Spielnamen, „Welle wiederholen“ lädt nur Stände dieses Spiels; kein Abriss am Kern während der Welle.
- Freies Spiel mit Abwehr: Bohr-Erfolge und Pionier-Rang gehören zum Spielstand, Material zerstörter Bauten zurück, Bohrung nur an echten Knoten, Talentpunkte höchstens 15.

**Bekannte Fehler**
- Gelegentlich bleibt ein Gegner an einer Klippe oder am Startpunkt einer Front hängen und wird nach einigen Sekunden auf den Weg zurückgesetzt.
- Hart und Albtraum sind noch nicht fertig eingestellt.

**Spielstände:** Stände aus 0.10.2 laden. Alte Stände mit Spielmodus „Aus“ stellen mit `/td modus einsatz` auf Einsatz um.

## Offen vor dem Upload

- [x] Fronten: feste Fronten je Kernplatz (18.11., Nachweis Fels-S8 und R3).
- [ ] Datum 20.11.2026 im Handbuch prüfen.
- [ ] Stinger am Startpunkt: behoben oder als bekannter Fehler (steht oben schon allgemein drin).
- [ ] Hart/Albtraum: Ergebnis P12-R / G3 eintragen oder Hinweis lassen.
- [ ] „0.10.2 saves load“ in G4 (Handspiel Marius, alter Stand) bestätigt.
