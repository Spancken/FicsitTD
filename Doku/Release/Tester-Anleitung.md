# FICSIT Tower Defense – Playtest Guide / Anleitung für Tester (Alpha)

> **Stand 0.10.2** (15.11.2026). Die gültigen Regeln stehen in `..\Spielregeln.md`, das Spielerhandbuch unter https://spancken.github.io/FicsitTD/. Bei jeder Version mit sichtbaren Änderungen hier nachziehen.

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
1. **New Game**, any start location. In the session settings open the category **FICSIT Tower Defense** and set **Game Mode** to **Operation**. The help text next to it explains each mode.
2. ADA greets you; a platform and the **Defense Core** appear. Build in peace, nothing attacks before you start a wave.
3. Press **E** at the Core: Core Menu with **Start wave**, shop (quotas, gear, core repair), research overview, statistics, **Pioneer** and **Report bug / idea**.
4. Build miners, smelters, constructors and turrets. Feed ammo by conveyor belt (Flamethrower: pipe). Turrets need power.
5. Currencies are real game items: **FICSIT Coupons** (shop, turret levels) and **Hard Drives** (research). Both go into your inventory; if it is full they wait at the Core.
6. Research new turrets, ammo and buildings in the **MAM** with Hard Drives. HUB milestones, blueprints and the AWESOME Sink are locked during an operation.
7. **Pioneer:** you earn XP and ranks yourself. In the Pioneer tab spend talent points on four roles (Marksman, Commander, Engineer, Hunter). Your strongest role is shown at the top left of the HUD.

### What we want to know
- How far did you get (wave), how long did it take, which turrets did you use?
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
1. **Neues Spiel**, beliebiger Startort. In den Sitzungseinstellungen die Kategorie **FICSIT Tower Defense** öffnen und **Spielmodus** auf **Einsatz** stellen. Der Hilfetext daneben erklärt jeden Modus.
2. ADA begrüßt dich, Plattform und **Abwehrkern** entstehen. In Ruhe bauen, vor dem ersten Wellenstart greift nichts an.
3. **E** am Kern öffnet das Kern-Menü: **Welle starten**, Laden (Kontingente, Ausrüstung, Kernreparatur), Forschungsübersicht, Statistik, **Pionier** und **Fehler melden / Idee**.
4. Miner, Schmelzöfen, Konstruktoren und Türme bauen. Munition per Förderband (Flammenwerfer per Rohr). Türme brauchen Strom.
5. Die Währungen sind echte Items des Spiels: **FICSIT-Coupons** (Laden, Turmlevel) und **Festplatten** (Forschung). Beide landen im Inventar; ist es voll, warten sie am Kern.
6. Neue Türme, Munition und Bauten im **MAM** mit Festplatten erforschen. HUB-Meilensteine, Blaupausen und der AWESOME-Sink sind im Einsatz gesperrt.
7. **Pionier:** Du sammelst selbst XP und Ränge. Im Reiter Pionier verteilst du Talentpunkte auf vier Rollen (Schütze, Kommandant, Techniker, Jäger). Die stärkste Rolle steht oben links im HUD.

### Was wir wissen wollen
- Bis zu welcher Welle, wie lange, welche Türme?
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
