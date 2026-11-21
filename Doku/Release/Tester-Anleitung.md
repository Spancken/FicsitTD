# FICSIT Tower Defense – Playtest Guide / Anleitung für Tester (Alpha)

> **Stand 0.11.1** (Patch 21.11.2026, Inhalt wie 0.11.0 plus Fehlerbehebungen, `Changelog-0.11.1.md`). Die gültigen Regeln stehen in `..\Spielregeln.md`, das Spielerhandbuch unter https://spancken.github.io/FicsitTD/. Bei jeder Version mit sichtbaren Änderungen hier nachziehen.

*English first, Deutsch weiter unten.*

## English

**Thank you for testing!** This is an early alpha. Expect rough edges.

### Requirements
- Satisfactory 1.2 (Steam or Epic), **single-player only**.
- Satisfactory Mod Manager (SMM): https://smm.ficsit.app
- About 1–2 hours for a first session (waves 1–15). A full operation of 50 waves takes several hours. There is no countdown: each wave starts when you press the button.

### Install
1. **Back up your saves**: `%LOCALAPPDATA%\FactoryGame\Saved\SaveGames`.
2. In SMM create a new profile (e.g. "TD Alpha"), install **FICSIT Tower Defense** (SML is installed automatically). For hidden alpha versions use the link you received and "Show all versions".
3. Start the game from SMM.

### Play
1. **New Game**, any start location. Right after landing, a window asks how you want to play: pick **Operation** and a difficulty (“By start location” is fine), then **Let's go**. **Difficulty** stays on “By start location” unless you want a fixed one (Hard and Nightmare are untested so far, reports welcome).
2. ADA greets you; a platform and the **Defense Core** appear. Build in peace, nothing attacks before you start a wave.
3. Press **E** at the Core: Core Menu with **Start wave**, shop (quotas, gear, core repair), research overview, statistics, **Pioneer** and **Report bug / idea**.
4. Build miners, smelters, constructors and turrets. Feed ammo by conveyor belt (Flamethrower: pipe). Turrets need power.
5. Currencies are real game items: **FICSIT Coupons** (shop, turret levels) and **Hard Drives** (research). Both go into your inventory; if it is full they wait at the Core.
6. Research new turrets, ammo and buildings in the **MAM** with Hard Drives. HUB milestones, blueprints and the AWESOME Sink are locked during an operation.
7. **Pioneer:** you earn XP and ranks yourself. In the Pioneer tab spend talent points on four roles (Marksman, Commander, Engineer, Hunter). Your strongest role is shown at the top left of the HUD.

### Other Game Modes (0.11)
- **Endless waves:** like Operation, but waves come on a timer and never end, always from one direction. The timer counts from the start of a wave; if you are too slow, the next wave comes on top. You can build at any time. After every boss wave: 5-minute break and a bonus at the Core. When the Core falls, the run ends with your best wave.
- **Free play with defense:** regular Satisfactory with HUB and milestones, plus turrets and defense research in the MAM (paid with Coupons from the AWESOME Sink). Nothing attacks unless you start it: place a Defense Core next to an ore node and start a **drilling** in its menu (Easy, Medium, Hard). Every wave you hold gives ore; the first success per resource and level gives Hard Drives. Abort any time without penalty.
- Existing saves: `/td modus abwehr` in the chat switches to Free play with defense.
- **Also new in 0.11:** one ammo item lasts for several shots (basic 4, advanced 3, special 2); the turret window shows consumption per minute. Power Storage in the shop (Coal Power research), Miner Mk.2/Mk.3 and belts up to Mk.5 in the MAM. Turrets and defenses cannot be dismantled near the Core during a wave. Wave saves carry the session name.

### What we want to know
- How far did you get (wave), how long did it take, which turrets did you use?
- Ammo per item: how many turrets does one miner, chain or belt feed for you? Did you run out of ammo, and in which wave?
- Endless waves: is the timer too hectic or too relaxed? At which wave did it break, and why (turrets, ammo, enemy type)?
- Free play with defense: did the drilling feel worth it? Was it clear where to place the Core?
- Fun (1–5), difficulty per section (waves 1–10, 11–20, 21–30, 31+), was it clear what to do?
- Did the Pioneer matter? Which role did you pick?
- Bugs: what happened, at which wave, screenshot if possible. Easiest: **Report bug / idea** in the Core Menu.
- Please fill in the feedback form: <link>

### Logs (for bugs)
**Report bug / idea** opens a prefilled GitHub issue and a folder with log and screenshot. Otherwise in SMM: **Help → Generate debug info** and send the zip, or copy `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log` **before** restarting the game, plus the folder `%LOCALAPPDATA%\FactoryGame\Saved\FicsitTD`.

### Known issues
Full list in the handbook: https://spancken.github.io/FicsitTD/#issues (section "Known issues"). The most important:
- Single-player only, no dedicated server.
- Balance is not final (tough spots around waves 10–16, the Core's remaining health varies a lot).
- Enemies sometimes get stuck on rock pillars; they are moved automatically after a while.
- Coupons stay in your inventory after an operation ends.

---

## Deutsch

**Danke fürs Testen!** Das ist eine frühe Alpha, es wird noch hakeln.

### Voraussetzungen
- Satisfactory 1.2 (Steam oder Epic), **nur Einzelspieler**.
- Satisfactory Mod Manager (SMM): https://smm.ficsit.app
- Etwa 1–2 Stunden für die erste Runde (Wellen 1–15). Ein ganzer Einsatz mit 50 Wellen dauert mehrere Stunden. Es gibt keinen Countdown: Jede Welle startet erst auf Knopfdruck.

### Installation
1. **Spielstände sichern**: `%LOCALAPPDATA%\FactoryGame\Saved\SaveGames`.
2. In SMM ein neues Profil anlegen (z. B. „TD Alpha“), **FICSIT Tower Defense** installieren (SML kommt automatisch mit). Bei versteckten Alpha-Versionen den zugeschickten Link und „Alle Versionen anzeigen“ nutzen.
3. Spiel aus SMM starten.

### Spielen
1. **Neues Spiel**, beliebiger Startort. Direkt nach der Landung fragt ein Fenster, wie du spielen willst: **Einsatz** und eine Stufe wählen („Nach Startort“ passt), dann **Los geht's**. **Stufe** bleibt auf „Nach Startort“, außer du willst eine feste (Hart und Albtraum sind noch ungetestet, Rückmeldungen erwünscht).
2. ADA begrüßt dich, Plattform und **Abwehrkern** entstehen. In Ruhe bauen, vor dem ersten Wellenstart greift nichts an.
3. **E** am Kern öffnet das Kern-Menü: **Welle starten**, Laden (Kontingente, Ausrüstung, Kernreparatur), Forschungsübersicht, Statistik, **Pionier** und **Fehler melden / Idee**.
4. Miner, Schmelzöfen, Konstruktoren und Türme bauen. Munition per Förderband (Flammenwerfer per Rohr). Türme brauchen Strom.
5. Die Währungen sind echte Items des Spiels: **FICSIT-Coupons** (Laden, Turmlevel) und **Festplatten** (Forschung). Beide landen im Inventar; ist es voll, warten sie am Kern.
6. Neue Türme, Munition und Bauten im **MAM** mit Festplatten erforschen. HUB-Meilensteine, Blaupausen und der AWESOME-Sink sind im Einsatz gesperrt.
7. **Pionier:** Du sammelst selbst XP und Ränge. Im Reiter Pionier verteilst du Talentpunkte auf vier Rollen (Schütze, Kommandant, Techniker, Jäger). Die stärkste Rolle steht oben links im HUD.

### Weitere Spielmodi (0.11)
- **Endlos-Wellen:** wie Einsatz, aber die Wellen kommen auf Zeit, enden nie und kommen immer aus einer Richtung. Die Uhr zählt ab Wellenbeginn; bist du zu langsam, kommt die nächste Welle obendrauf. Bauen geht jederzeit. Nach jeder Bosswelle: 5 Minuten Pause und ein Bonus am Kern. Fällt der Kern, endet der Lauf mit deinem Bestwert.
- **Freies Spiel mit Abwehr:** normales Satisfactory mit HUB und Meilensteinen, dazu Türme und Abwehr-Forschung im MAM (bezahlt mit Coupons aus dem AWESOME-Sink). Nichts greift an, was du nicht startest: Stell einen Abwehrkern neben einen Erzknoten und starte in seinem Menü eine **Bohrung** (Leicht, Mittel, Schwer). Jede gehaltene Welle bringt Erz, der erste Erfolg je Rohstoff und Stufe Festplatten. Abbrechen geht jederzeit ohne Strafe.
- Bestehende Spielstände: `/td modus abwehr` im Chat stellt auf Freies Spiel mit Abwehr um.
- **Außerdem neu in 0.11:** Ein Stück Munition reicht für mehrere Schuss (Grund 4, Stufe 2 drei, Spezial 2); das Turmfenster zeigt den Verbrauch pro Minute. Akku im Laden (Forschung Kohlekraft), Miner Mk2/Mk3 und Bänder bis Mk5 im MAM. Türme und Abwehrbauten lassen sich während der Welle am Kern nicht abreißen. Wellen-Spielstände tragen den Namen des Spiels.

### Was wir wissen wollen
- Bis zu welcher Welle, wie lange, welche Türme?
- Munition je Stück: Wie viele Türme versorgt bei dir ein Miner, eine Kette, ein Band? Ging die Munition aus, und in welcher Welle?
- Endlos-Wellen: Ist die Uhr zu hektisch oder zu ruhig? In welcher Welle kippte es, und woran lag es (Türme, Munition, Gegnerart)?
- Freies Spiel mit Abwehr: Hat sich die Bohrung gelohnt? War klar, wo der Kern hin muss?
- Spaß (1–5), Schwierigkeit je Abschnitt (W1–10, 11–20, 21–30, ab 31), war klar, was zu tun ist?
- Hat der Pionier etwas ausgemacht? Welche Rolle hast du gewählt?
- Fehler: was, in welcher Welle, möglichst mit Screenshot. Am einfachsten über **Fehler melden / Idee** im Kern-Menü.
- Bitte den Rückmeldebogen ausfüllen: <Link>

### Logs (bei Fehlern)
**Fehler melden / Idee** öffnet ein ausgefülltes GitHub-Issue und einen Ordner mit Log und Bildschirmfoto. Sonst in SMM: **Hilfe → Debug-Infos erzeugen** und die Zip schicken, oder `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log` **vor** dem Neustart kopieren, dazu den Ordner `%LOCALAPPDATA%\FactoryGame\Saved\FicsitTD`.

### Bekannte Probleme
Vollständige Liste im Handbuch: https://spancken.github.io/FicsitTD/#issues (Abschnitt „Bekannte Fehler“). Die wichtigsten:
- Nur Einzelspieler, kein dedizierter Server.
- Balance nicht fertig (enge Stellen um Welle 10–16, Kern-LP am Ende schwanken stark).
- Gegner bleiben selten an Felspfeilern hängen und werden nach einer Weile automatisch versetzt.
- Coupons bleiben nach dem Einsatz im Inventar.
