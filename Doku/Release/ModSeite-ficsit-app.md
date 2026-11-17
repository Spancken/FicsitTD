# ficsit.app – Texte für die Mod-Seite (zum Einfügen, Englisch)

> **Stand 0.10.2** (15.11.2026, live auf ficsit.app). Aufbau nach Marius 15.11.: zuerst Beschreibung, Bilder gesammelt unter „Screenshots“; Heldenbild, Marksman und Commander von Marius herausgenommen. Danach (Marius: „zu viele, zu groß“) nur drei Vorschaubilder 480 × 270 (`docs/img/klein/`) in einer Reihe, verlinkt auf die großen Fassungen. ficsit.app zeigt Bilder in Originalgröße, höchstens Spaltenbreite. Neu: englische Screenshots aus dem Plan `Screenshots-EN` (`hero`, `fight`, `wave-preview`, `pioneer-tab`, `role-*`, `core-menu` in `docs/img/`), Logistik-Satz korrigiert (Splitter/Verbinder/Lager mit Kontingent). Bilder liegen im Repo unter `docs/img/` und werden über GitHub Pages ausgeliefert (`https://spancken.github.io/FicsitTD/`). Sie erscheinen erst, wenn `docs/` gepusht und Pages eingeschaltet ist. `https://ko-fi.com/mariusspancken` durch die echte Ko-fi-Adresse ersetzen (auch in `docs/index.html`).

Mod reference: `FicsitTD` (permanent). Name: **FICSIT Tower Defense**. Sichtbarkeit: **Hidden** (Fremdtest).

## Short description (max. ~200 chars)

Factory-fed tower defense: real ammo chains feed your turrets. Hold a FICSIT Defense Core through 50 waves. Alpha.

## Full description (Markdown)

```markdown
# FICSIT Tower Defense

**Your factory makes the ammo. The turrets protect the factory.**

> 🧪 **Alpha – playtesters wanted!** FICSIT Tower Defense is playable from wave 1 to 50, but balance and some features are still rough, and not every feature has been tested in a full run yet. Please play it, break it and tell me what feels wrong or boring: **Core Menu → Report bug / idea**, or https://github.com/Spancken/FicsitTD/issues. Every report helps shape the next version.

FICSIT Tower Defense turns Satisfactory into a tower defense that lives off your factory. A FICSIT Defense Core stands in your start biome, and waves of native fauna attack it from up to three fronts. Every turret fires **real ammunition from your own production lines**, delivered by conveyor belt or pipe. Fifty waves lead you from simple iron rebar to plasma flames.

## 🆕 What's new in 0.10.2
- **The Pioneer (0.10.0):** you fight alongside your turrets, earn ranks and pick talents in four roles (details below).
- **Real game items (0.10.0):** FICSIT Coupons are the game's Coupon item, research is paid with the game's **Hard Drives**. Old saves are converted when loaded.
- **Game Mode menu (0.10.1):** a new session settings category **FICSIT Tower Defense** with a **Game Mode** dropdown (Off / Operation) and a help text for each mode. More modes will follow.
- **Fixes (0.10.1–0.10.2):** crash when a save was loaded twice within a few seconds, fronts no longer discarded right after loading, ADA follows the mod language, Core and Tank Trap show their real maximum health, chat messages point to the **Start wave** button, diagnostic tick measurements only run in developer mode.

### The Pioneer

You are no longer just the builder. Fight next to your turrets and earn **Pioneer ranks** for your share of the damage, kills, assists and repairs. Every rank gives a Talent Point (up to 15 per operation) for one of **four roles**, five abilities each:

- **Marksman** – headshots, fast reload, piercing shots, ammo saving, explosive rounds
- **Commander** – mark enemies for +15 % turret damage, marks jump on, **rally order** (nearby turrets fire at your target), overdrive for all turrets around you
- **Engineer** – **field repair** with Iron Plates, **overdrive button** in the turret window (+50 % clock for 10 s), ammo resupply from your inventory, bulwark (one building per wave survives at half health)
- **Hunter** – ×1.5 against elites and bosses, guaranteed loot, weak spots, resistances on the boss bar, execution

Take two roles fully or three halfway. Your strongest role shows as a badge in the HUD, and repairs, rally orders and overdrive are visible on the turrets.

📖 **Player guide (English/German):** https://spancken.github.io/FicsitTD/
☕ **Support the mod:** https://ko-fi.com/mariusspancken

## Quick start
1. Back up your saves. Start a **new game** and, in the session settings category **FICSIT Tower Defense**, set **Game Mode** to **Operation** (the help text explains each mode).
2. After landing, the mod builds a platform with the **Defense Core** near your start, and ADA briefs you.
3. Build miners, smelters and constructors, then a **Bolt Turret**. Feed it rebar by conveyor and connect power.
4. Press **E** at the Core and hit **Start wave** when you are ready. There is no timer.

## What you get
- **An operation of 50 waves** in five phases with a boss every tenth wave, elites and new enemy types: Hogs, Stingers, forest, desert and water Spitters, Nuclear Hogs and Johnny.
- **5 turret lines with real ammo chains:** Bolt Turret (rebar → piercing → shrapnel/stun → explosive), Rifle Turret (rifle ammo → homing/armor-piercing → incendiary → turbo), Nobelisk Launcher (nobelisk → gas/pulse → cluster), Tesla Coil (power, chain lightning or overload) and Flamethrower (fuel → turbofuel → rocket fuel → ionized fuel). Late game: Sky Tower (flyers still in development) and Singularity Launcher (Endless mode, after wave 50).
- **Turret levels 1–5** bought with Coupons in the turret window, visible on the model. Level 5 unlocks a master ability per turret. Pick a **doctrine** for your main line.
- **Seven combos** between turrets and buildings: Lightning Rod, Heat Crack, Deflagration, Oil Fire, Chokepoint, Rally Point, Conductor.
- **Defensive buildings:** walls and gate, lure fire, oil slick field, electric fence, radio mast (radar and front warnings), shield generator, jammer projector, drone pad with repair or combat drones.
- **Quotas instead of free building:** production and defense buildings are limited per type; FICSIT Coupons from the Core (now the game's Coupon item in your inventory) buy more in the Core Menu shop, sorted by category. Belts, lifts, pipes, power lines and foundations stay free; splitters, mergers and storage count as buildings.
- **MAM research** with four trees (Industry, Ballistics, Elemental, Fortress), branches where you choose one side per operation, paid with **Hard Drives** from waves (the game's own Hard Drive). HUB milestones and blueprints are locked during an operation.
- **Your own share counts:** hand weapons use the turret ammo values and grow with the waves; the wave report shows your damage, share, assists and Pioneer XP.
- **Losing never deletes your factory.** After a defeat you can retry the wave or end the operation; every tenth wave closes a phase with a short report and a bonus pick, and the end of a run shows a final report and your best result per core site and difficulty. The mod saves before and after every wave; saving during a wave is locked.
- **English and German**, following the game language.

## Screenshots
[![A wave of hogs reaches the turret line](https://spancken.github.io/FicsitTD/img/klein/fight.jpg)](https://spancken.github.io/FicsitTD/img/fight.jpg) [![Pioneer tab: four roles with five abilities each](https://spancken.github.io/FicsitTD/img/klein/pioneer-tab.jpg)](https://spancken.github.io/FicsitTD/img/pioneer-tab.jpg) [![Wave preview: who comes next and from where](https://spancken.github.io/FicsitTD/img/klein/wave-preview.jpg)](https://spancken.github.io/FicsitTD/img/wave-preview.jpg)

*Click a picture for full size.*

## Status: Alpha (0.10.2)
- Steam and Epic, Windows. **Single-player only**; multiplayer and dedicated servers are not supported.
- Balance is work in progress; the target length is 50 waves.
- **Known issues** (stuck enemies, Sky Tower without flyers yet, Tesla power spikes, retry wave untested and more): https://spancken.github.io/FicsitTD/#issues

## Feedback
Report bugs or suggest ideas right in the game: Core Menu → **Report bug / idea**. It opens a prefilled GitHub issue in your browser and a folder with your log and a screenshot. Or directly: https://github.com/Spancken/FicsitTD/issues
Log file: `%LOCALAPPDATA%\FactoryGame\Saved\Logs\FactoryGame.log`

## Use of AI (required disclosure)
This mod was developed with substantial help of AI (Anthropic Claude): most of the C++ code, the asset generation scripts, the English translation, the player guide and parts of the 3D models/icons were created by AI under the direction of the author, who designed, tested and reviewed the mod.

**Network activity:** none by the mod itself. Two Core Menu buttons only open links in your web browser (Ko-fi and a prefilled GitHub issue).
**Source:** https://github.com/Spancken/FicsitTD
```

## Changelog für die nächste hochgeladene Version

> **Entwurf 0.11 (unveröffentlicht, Stand 17.11.2026, erzeugt mit `python Tests\pruefe_konsistenz.py --changelog`, vor dem Upload neu erzeugen):**
> **New**
> - New Game Mode “Endless waves”: timed waves from one direction that never stop – build and keep your turrets supplied.
> - New Game Mode “Free play with defense”: regular Satisfactory plus turrets and an optional drilling challenge at resource nodes – nothing attacks unless you start it.
> - ADA now comments on new enemy species and boss waves.
> - Turrets shield their ammo storage; researching the Singularity unlocks the Hazmat Suit and Iodine Filters.
> - Power Storage is now available in the shop after researching Coal Power.
>
> **Changed**
> - Game Mode “Off” is now plain Satisfactory (no turret recipes, no defense research); old saves switch with `/td modus einsatz`.
> - Operation: the test jump, Free Build and “Restart” are developer tools now – a new run is a new game. Choose the difficulty in the session settings (default: by start location); Hard and Nightmare were retuned and are marked untested. Every tenth wave closes a phase with a short report and a bonus pick; a final report and a best result per core site and difficulty end each run.
> - Fewer chat lines after each wave: one summary line, details in the Core Menu. Endless waves: from wave 60 on, more enemies and more elites.
>
> **Balance**
> - Endless waves: enemies keep getting tougher after wave 50 so there is always a best run to beat; slightly smaller waves and more time between waves on Normal. Drilling: the drill repairs itself between waves, more HP on Medium and Hard.
> - Ammo now lasts longer: one item fires several shots (basic ammo 4, advanced 3, special 2; rifle ammo 3/2/1 magazines). The turret window shows consumption per minute and how many turrets a Mk.1 belt can feed. New in the MAM: Conveyor Belt Mk.4 and Mk.5, Miner Mk.2 and Mk.3.
>
> **Fixed**
> - Fewer enemies stuck at cliffs next to a front's start point.
> - Fewer enemies stuck at cliff overhangs.
> - Free play with defense: research no longer waits for wave numbers, destroyed buildings refund their materials during drilling and defenses heal each drilling wave, drilling only works at real resource nodes, talent points are capped at 15. Operation: after ending an operation the world stays free to build, advanced game settings make a run unranked, the Core bonus now permanently adds 10 % Core HP and unclaimed bonuses are kept. Endless waves: the timer keeps running while enemies are still queued, an unfinished boss wave gives no break or bonus, the shop is open during waves, no saves per wave.
> - Towers and buildings: status light and power socket are now part of the base (no floating lamp, no protruding power pole), build clearance includes the conveyor input, darker panels brightened.
> - Defense gate: power socket built into the pillar, status light seated on its mount. Darker patches on towers and buildings removed (ambient occlusion re-baked).
> - Removed the floating black panels next to towers and buildings (inherited fog planes of the copied vanilla buildings).
> - Towers and buildings: removed flickering overlapping surfaces.
> - Free play with defense: drilling rewards and Pioneer rank belong to the save, not to a single Defense Core.
> - Wave saves now carry the session name, and "Retry wave" only loads a save of the current game. Turrets and defenses can no longer be dismantled near the Core during a wave. Research unlocked "from wave N" opens after the previous wave ends, not during it.


**0.10.2 (Tag `v0.10.2`, G2 DE 12/14 mit bekanntem Felspfeiler-Rest, EN 5/5, G5 bestanden):**

```
0.10.2-alpha - Small fixes
- Fixed: the Core and Tank Trap look-at text shows the real maximum health (including research) and is now translated
- Fixed: chat messages point to the "Start wave" button at the Core instead of a chat command
- Fixed: the shop text no longer lists the removed Tank Trap
- Performance: diagnostic tick measurements only run in developer mode
```

```
0.10.1-alpha - Game mode menu and crash fix
- New: own session settings category "FICSIT Tower Defense" with a "Game Mode" dropdown (Off / Operation) and a help text that explains each mode, in English and German. More modes will follow
- Fixed: crash when a save was loaded twice within a few seconds
- Fixed: right after loading, the wave start could discard valid enemy fronts because the navigation was not ready yet
- Fixed: the ADA greeting now follows the mod language setting
- Old saves keep working; the previous "FICSIT Tower Defense" checkbox is still recognized
```

```
0.10.0-alpha - The Pioneer: four roles, 20 abilities, Hard Drives and Coupon items
- New: Pioneer talent tree with four roles - Marksman, Commander, Engineer, Hunter - five abilities each, up to 15 points per operation
- New: target marking (+15 % turret damage), assists, rally order, overdrive button in the turret window, field repair, ammo resupply, bulwark, weak spots, execution
- New: talent tree with game icons, role badge in the HUD, visible repair beam, rally beam and overdrive glow
- Pioneer XP now rewards your own performance (damage share, repairs, kills, assists); wave XP halved; rank bonus every 3 ranks (+10 HP, +3 % speed)
- Changed: MAM research is paid with the game's Hard Drives instead of FICSIT Combat Data; the game's own Hard Drive research is locked during an operation
- Changed: FICSIT Coupons are now the game's Coupon item in your inventory (they wait at the Core if your inventory is full)
- Old saves: Combat Data become Hard Drives and your Coupon balance becomes Coupon items after loading
- Fixed: overdrive button and upgrade buttons on the white turret card were drawn as a thin bar
```

```
0.9.6-alpha – review fixes, operation end, shop and pathing
- After victory or defeat: retry the wave, restart from wave 1 or end the operation in the Core Menu
- Saving and autosave locked during a wave (no more loot repeats); wave enemies are no longer written to saves
- HUB milestones and blueprints locked during an operation
- Shop sorted by category, locked items hidden, proper building names
- MAM: excluded branch marked with a red X, clearer node icons
- Enemies no longer get stuck on cliff pillars: the blocking cliff is removed from navigation for following enemies
- Large enemies use the correct navigation mesh
- Drone Pad levels 1-3 (2-4 drones), shield blocks direct hits, true range preview, oil slick triggers on the plate
- Duty buildings (shield, radio mast, jammer, lure fire, oil slick) run at a fixed 100 % clock
- Hand weapons use turret ammo values; your damage share is shown after each wave
- Core site search retries another site if the terrain is not ready
```

## Checkliste (Marius)

- [x] Ko-fi-Konto anlegen, URL in diese Datei und in `docs/index.html` (`https://ko-fi.com/mariusspancken`) eintragen
- [x] Repo: `docs/` committen und pushen, GitHub Pages aus `/docs` eingeschaltet (Handbuch lädt seit 12.11.)
- [x] Prüfen, dass `https://spancken.github.io/FicsitTD/` und die Bilder laden
- [x] ficsit.app: Short und Full description ersetzt (12.11., von Claude im Browser), zweite Fassung mit Bekannte-Fehler-Link und Netzwerk-Hinweis
- [x] Vor dem Paketieren: `"Version"` in `FicsitTD.uplugin` = Hauptversion von `SemVersion` (bei 0.x also **0**), sonst lehnt ficsit.app ab („SemVer major version should match Version“, 12.11.)
- [x] Version 0.9.6 hochgeladen (12.11. 10:21, https://ficsit.app/mod/FicsitTD/version/BTHrrhrezg4mf7), Virenscan lief noch; Ko-fi-Beitrag dazu veröffentlicht
- [ ] **0.10.0 – vor dem Upload (Marius):**
  - [ ] G4 Handspiel 30–60 min: neues Spiel **und** einen alten Spielstand laden. Achten auf: ADA-Begrüßung (Satz 6 „Festplatten“, Ton und Untertitel synchron), Coupons und Festplatten im Inventar, Talentbaum im Reiter Pionier, Rollenabzeichen oben links, Effekte (grüner Reparaturstrahl, oranger Puls beim Übersteuern, Strahl beim Sammelbefehl), Upgrade-Knopf im Turmfenster mit genug Coupons
  - [x] Push (14.11., `master` = `ae43c4d`, Tags `v0.9.8`, `v0.10.0`); Bild `https://spancken.github.io/FicsitTD/img/pionier.jpg` lädt (HTTP 200), Handbuch zeigt es
- [x] **0.10.2 hochgeladen** (15.11. 11:16, https://ficsit.app/mod/FicsitTD/version/7ZJm86Vyrzghmn, Changelogs 0.10.2/0.10.1/0.10.0, Virenscan durch); Full description am 15.11. durch Claude ersetzt (What's new, Screenshots). Release-Log: `Release-Log.md`.
- [x] Ko-fi-Beitrag 0.10.2 veröffentlicht (15.11.)
- [ ] Discord-Post 0.10.2 (Entwurf `Ankuendigung-0.10.2.md`, Kanal klären)
- [ ] G4 Handspiel 0.10.2 (Marius) nachholen
- [ ] ~~**0.10.0 hochladen (14.11. abends):**~~ überholt durch 0.10.1: Zip `E:\Modding\SatisfactoryModLoader\Saved\ArchivedPlugins\FicsitTD\FicsitTD.zip`, Changelog 0.10.0 oben, Stability **Alpha**, danach Full description durch den Text oben ersetzen (Abschnitt „New in 0.10“)
- [x] Screenshots englisch (15.11., Plan `Screenshots-EN`): `hero`, `fight`, `wave-preview`, `core-menu`, `pioneer-tab`, `role-*` in `docs/img`, im Handbuch und oben in der Full description. Laden erst nach dem Push von `docs/`.
