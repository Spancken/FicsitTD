# Changelog 0.13 (Entwurf)

> Entwurf vom 23.11.2026, noch kein Release. Zusammengefasst aus den Änderungszetteln in `Aenderungen\` (sichtbar: ja) auf dem **Endstand**. Überholte Zwischenstände fehlen: 40-m-Ring, Shop nach Welle 2, MAM nach Welle 1. 0.12 erscheint nicht einzeln, ihr Inhalt ist hier enthalten. Rohliste: `python Tests\pruefe_konsistenz.py --changelog`. Der englische Teil geht als `changelog.md` zu ficsit.app.

## English (ficsit.app)

**FICSIT Tower Defense 0.13: AWESOME Shop, a new start, core access rules, an untangled research tree with a new Power tree, and a harder difficulty.**

### New
- **AWESOME Shop:** after wave 1 ADA asks you to build the game's AWESOME Shop (free, one per operation). Quotas, gear, core repairs and Hard Drives are bought there in six sections. The Core no longer sells, and base game items are hidden during an operation.
- **New start order:** ADA first asks for the HUB (power and respawn point), after wave 1 for the AWESOME Shop, after wave 2 for the MAM. She voices each task. The second line of the status bar shows the current task.
- **Boss waves:** a window shows the phase report and three bonus cards. You get two Talent Points per boss wave, including wave 50 (10 in an operation).
- **Core access:** if your buildings seal off the Core, the wave (or the drilling) does not start until a gap is open. The blocking spot is marked on the map. A building that would close the last path is red in the hologram. Defense walls may still close it.
- **Unstable ground:** during an operation and in Endless waves, factory buildings cannot be placed within 60 m of the Defense Core. Defenses and logistics still can. In build mode, small orange lights mark the ring.
- Enemies walk around machines and turrets instead of getting stuck on them.
- **Return Hard Drives:** in the AWESOME Shop (Pioneer and Core) you trade a spare Hard Drive for 12 Coupons, also without Coupons in your inventory.
- **Power tree:** coal, oil and (after wave 50) nuclear power have their own MAM tree. Nothing locks anything out, and coal and oil can run side by side. Building up a line pays off: better fuels give every generator more output (Compacted Coal 100 MW, Petroleum Coke 125 MW, Turbofuel 325 MW, Rocket Fuel 400 MW, Ionized Fuel 500 MW, Plutonium 3250 MW). The Shop sells Nuclear Power Plants and Particle Accelerators from wave 51.
- **Where is the oil?** When an operation starts, the chat tells you how far away the nearest crude oil is and in which direction (Forest under ~1 km, Grass Fields over 2 km).
- **Flying crabs** from wave 43: they fly straight to the Core over walls; ground turrets can't hit them, the Sky Tower and your hand weapons can.
- **Player guide:** the full research tree sits behind a spoiler button, plus a planner for Hard Drives and Talent Points by wave.

### Changed
- **MAM research tree untangled:**
  - Industry has six entry points and no forced chain. Belt Mk.2 needs no copper, the Refinery follows Coal Power, and Sulfur brings Black Powder.
  - The Bolt and Tesla lines open from wave 3, the Rifle Turret from wave 5.
  - Shield Generator and Jammer Projector are no longer a branch.
- **Free Play with Defense:** the other side of a research branch can be researched too, at double the Coupon price.
- The Core platform cannot be dismantled during an operation.

### Balance
- **Harder difficulty:** on Normal, Hard and Nightmare, enemies have more health, and from wave 31 they grow much faster, so the second half of an operation stays dangerous. The Defense Core on Normal has 15,000 HP. Endless waves stay as before.
- **Starting funds:** every operation starts with **40 FICSIT Coupons** for your first turrets (AWESOME Shop after wave 1), so the first waves are less of a wall.
- **Hand weapons:** weaker at the start, stronger later – your own share of the wave damage now stays around 10–20 % instead of dominating the first waves and fading later.

### Fixed
- The status bar replaces the mod's block in the To-do notes, which could stay in German and show outdated values.
- The Caterium research node now says that it unlocks the Nobelisk Detonator.
- The Fuel-Powered Generator node said 150 MW; it produces 250 MW.
- The Oil Fire combo now shows its announcement and counts like the other combos.
- Off mode: the mod no longer changes your quick bar and skips its background work.
- Clearer messages: why building is locked during a wave, where to get more quotas (AWESOME Shop) and why blueprints and base-game foundations are not available during an operation.
- The core menu's research tab is in English on English games.
- A Core sealed off in an older save is reported shortly after loading, not only when the wave should start.
- The path check covers the whole way to the fronts and the full hologram outline.
- Talents of older saves are reset once to the new boss points, with a chat note.
- Free Play with Defense: missing industry nodes (Belt Mk.4/Mk.5, Miner Mk.2/Mk.3) are added, and the other side of a branch no longer shows as locked.
- Right after the Core platform is built, its own foundations no longer count as sealing off the Core while the navigation mesh catches up.
- The enemy path preview shows the final route before you start the wave: cliffs are checked during the build phase.
- Grass Fields: the first front of one core site no longer leads through a canyon where big enemies got stuck.
- The Oil Slick Field slows enemies again – in the 0.13 test builds they walked around it.
- Enemies find their way through mazes of defense walls. Before, a long winding path made the path search give up: enemies got stuck at the outer wall and the Core counted as sealed off.

### Saves
- Saves from 0.11.x load. A Core that an existing ring of machines seals off is reported after loading. The wave starts once a path is open.

## Deutsch (README)

**0.13: AWESOME-Shop, neuer Einstieg, Regeln für den Kern-Zugang, entflochtener Forschungsbaum und mehr Herausforderung.**

- AWESOME-Shop nach Welle 1: Kontingente, Ausrüstung, Kernreparatur und Festplatten. Der Kern verkauft nicht mehr.
- Einstieg: HUB, nach Welle 1 der Shop, nach Welle 2 das MAM. ADA sagt die Aufgaben an, die Statusleiste zeigt sie.
- Bosswellen mit Phasenbericht, drei Bonus-Karten und einem Talentpunkt.
- Kern-Zugang: Ein abgeriegelter Kern startet keine Welle. Das Hologramm wird rot, wenn ein Bau den letzten Weg schließen würde.
- Instabiler Boden: keine Fabrikgebäude in 60 m um den Kern.
- Gegner laufen um Maschinen und Türme herum.
- Festplatten abgeben: 12 Coupons je Stück, auch ohne Coupons im Inventar.
- Neuer Baum Energie: Kohle, Öl und nach Welle 50 Kernkraft, ohne Sperre. Bessere Brennstoffe geben jedem Kraftwerk mehr MW (Kompaktkohle 100, Petrolkoks 125, Turbotreibstoff 325, Raketentreibstoff 400). Im Shop gibt es Atomkraftwerk und Teilchenbeschleuniger ab Welle 51.
- MAM-Baum entflochten: sechs Einstiege, Bolzen und Tesla ab Welle 3, Gewehrturm ab Welle 5.
- Im Freien Spiel mit Abwehr ist die Gegenseite einer Gabel zum doppelten Coupon-Preis erforschbar.
- Mehr Herausforderung: mehr Gegnerleben in Normal, Hart und Albtraum, ab Welle 31 wachsen sie deutlich schneller. Der Kern hat in Normal 15 000 LP. Die Endlos-Wellen bleiben wie bisher.
- Startkapital: Jeder Einsatz beginnt mit 40 FICSIT-Coupons für die ersten Türme (AWESOME-Shop nach Welle 1).
- Wo ist Öl? Beim Einsatzstart sagt der Chat Entfernung und Richtung zum nächsten Rohöl (Wald unter etwa 1 km, Grasfelder über 2 km).
- Fliegende Krabben ab Welle 43: Sie fliegen über Mauern zum Kern, nur Himmelsturm und Handwaffen treffen sie.
- Handwaffen anfangs schwächer, später stärker: Dein Anteil bleibt bei etwa 10–20 %.
- Talentpunkte: zwei je Bosswelle, auch für Welle 50.
- Fehlerbehebungen:
  - Forschungsreiter auf Englisch
  - Ladeprüfung des Kern-Zugangs
  - Wegprüfung bis zur Front
  - Talente alter Stände
  - fehlende Industrie-Knoten im Freien Spiel
  - Die eigene Kernplattform gilt kurz nach dem Bau nicht mehr als Abriegelung, solange das Netz nachzieht
  - Die Wegvorschau zeigt vor dem Wellenstart den endgültigen Weg (Klippenprüfung in der Bauphase)
  - Grasfelder: Die erste Front eines Kernplatzes führt nicht mehr durch eine Schlucht, in der große Gegner hängen blieben
  - Das Ölschlick-Feld bremst wieder (in den 0.13-Testständen liefen Gegner darum herum)
  - Gegner finden durch Labyrinthe aus Mauern (vorher gab die Wegsuche bei langen Umwegen auf, Gegner hingen an der äußeren Mauer)
