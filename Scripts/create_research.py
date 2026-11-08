# MAM-Forschungsbaeume fuer den Einsatz (Einsatz-Konzept Abschnitt 5).
# Vorlage: Caterium-Baum des Spiels (Knotenklasse BPD_ResearchTreeNode, Knotendaten als BP-Struct mNodeDataStruct).
# Erzeugt je Baum ein BPD_ResearchTree_TD_* und je Knoten ein Schematic Research_TD_* (Typ MAM), dazu
# Source/FicsitTD/Private/TDForschungListe.inl mit allen Pfaden (TDWorldModule registriert sie).
# Vorher alte Dateien unter Content/Research per Dateisystem loeschen; erst create_einsatz_recipes.py und create_research_points.py; danach Build-Mod.ps1.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
G = "/Game/FactoryGame"
P = f"{G}/Resource/Parts"
R = f"{G}/Recipes"
TD = "/FicsitTD/Buildings"
BAUM_VORLAGE = f"{G}/Schematics/Research/BPD_ResearchTree_Caterium"
KNOTEN_KLASSE = f"{G}/Schematics/Research/BPD_ResearchTreeNode.BPD_ResearchTreeNode_C"
SCHEMA_VORLAGE = f"{G}/Schematics/Research/Caterium_RS/Research_Caterium_1"
ZIEL = "/FicsitTD/Research"

def item(pfad):
    return f"{pfad}.{pfad.rsplit('/', 1)[1]}_C"

EISENBARREN = f"{P}/IronIngot/Desc_IronIngot"
EISENPLATTE = f"{P}/IronPlate/Desc_IronPlate"
EISENSTANGE = f"{P}/IronRod/Desc_IronRod"
BOLZEN = f"{P}/SpikedRebar/Desc_SpikedRebar"
KUPFERBARREN = f"{P}/CopperIngot/Desc_CopperIngot"
DRAHT = f"{P}/Wire/Desc_Wire"
KABEL = f"{P}/Cable/Desc_Cable"
KOHLE = f"{G}/Resource/RawResources/Coal/Desc_Coal"
SCHRAUBE = f"{P}/IronScrew/Desc_IronScrew"

KUPFER = [f"{R}/Smelter/Recipe_IngotCopper", f"{R}/Constructor/Recipe_Wire", f"{R}/Constructor/Recipe_Cable", f"{R}/Constructor/Recipe_CopperSheet"]
EISEN2 = [f"{R}/Constructor/Recipe_IronPlate", f"{R}/Constructor/Recipe_Screw", f"{R}/Assembler/Recipe_IronPlateReinforced", f"{R}/Assembler/Recipe_Rotor", f"{R}/Buildings/Recipe_AssemblerMk1"]

# Baum: (Schluessel, Anzeigename, Beschreibung, Icon-Item, Knoten)
# Knoten: (Schluessel, Name, Beschreibung, (x, y), [Eltern-Schluessel], Kosten [(Item, n)], Zeit s, Rezepte, Icon-Item)
# Abzweigung: Knoten mit demselben Elternteil in einer Zeile; dass nur einer erforscht werden darf, regelt der Mod (Einsatz-Konzept 11a)
# Schritt B (Spielablauf-EarlyGame.md, 02.11.2026): Kosten nur in Forschungspunkten (FP = FICSIT-Kampfdaten), Zeile y = Stufe
# (Stufe 1: 2 FP, Stufe 2: 3 FP). Jeder Knoten bringt ein Gebaeude, eine Munition oder eine Faehigkeit - keine Prozentboni.
FP = "/FicsitTD/Items/Desc_TD_Kampfdaten"
ZEIT = 3.0   # Marius 02.11.: keine Forschung laenger als 3 s
DURCH = "/FicsitTD/Items/Desc_TD_Durchschlagbolzen"
TDR = "/FicsitTD/Recipes"
STAHL = [f"{R}/Buildings/Recipe_SmelterMk1", f"{R}/Smelter/Recipe_IngotSteel", f"{R}/Constructor/Recipe_SteelPipe", f"{R}/Constructor/Recipe_SteelBeam"]
# 0.83.1 (Marius 07.11.: "soll sich je Seite von 2-3 Ursprungsknoten raus entwickeln", gesperrt ohne Icon und mit Verbindung):
# jede Seite hat 2-3 Wurzeln, alle anderen Knoten haengen mit Linie an einem Elternknoten. Das MAM zeigt Kinder grau ohne Icon,
# bis der Elternknoten erforscht ist. Eltern so gewaehlt, dass Freigabe-Welle und FP-Preis jedes Knotens gleich bleiben.
# 0.72 Forschung 2.0 (Konzept-Forschung-2.md, Marius 04.11.2026: "lieber weniger Fenster mit komplexen Baeumen und Entscheidungswegen"):
# 4 Baeume statt 9, Zeilen nach Phasen (oben frueh, unten spaet), Gabelungen je Linie. Die Schematics behalten ihren alten Ordner
# (ORDNER), damit alte Spielstaende ihre Forschung behalten. Kosten je Knoten aus der Phase (preis), nicht mehr fest.
# Turmlevel-Knoten je Turm entfallen: Level 2 gibt es mit dem Turm (ATDTurretBase::UpgradeFreigeschaltet).
GABEL = "GABELUNG: FICSIT finanziert je Einsatz nur eine Entwicklungslinie. "
DOKTRIN = "DOKTRIN (Hauptlinie, nur eine je Einsatz): "
BAEUME = [
    # Spalten: Strom 0 | Fertigung 1-2 | Logistik 3-4
    ("Industrie", "FICSIT-Abwehr: Industrie", "Was deine Fabrik herstellen kann: Teile, Stahl, Öl, Strom und schnellere Bänder. Hier wird nichts ausgeschlossen.", f"{P}/IronPlateReinforced/Desc_IronPlateReinforced", [
        ("Kohlekraft", "Kohlekraftwerk", "Kohlekraftwerk (75 MW statt 30 MW), Wasserextraktor und Rohre. Öffnet den Baum Elementar (Teslaspule, Flammenwerfer).",
         (0, 0), [], [], ZEIT,
         [f"{R}/Buildings/Recipe_GeneratorCoal", f"{R}/Buildings/Recipe_WaterPump", f"{R}/Buildings/Recipe_Pipeline", f"{R}/Buildings/Recipe_PipeSupport", f"{R}/Buildings/Recipe_PipelineJunction_Cross"], KOHLE),
        ("Assembler", "Assembler und Eisenteile", "Assembler, Eisenplatten, Schrauben, verstärkte Eisenplatten und Rotoren. Grundlage für fast jede bessere Munition. Öffnet den Baum Ballistik.",
         (1, 0), [], [], ZEIT, EISEN2, f"{P}/IronPlateReinforced/Desc_IronPlateReinforced"),
        ("Kupfer", "Kupferverarbeitung", "Kupferbarren, Draht, Kabel und Kupferblech: der zweite Rohstoff. Für Gewehrmunition, Schockbolzen und Stromtechnik.",
         (3, 0), [], [], ZEIT, KUPFER, KUPFERBARREN),
        ("BandMk2", "Förderband Mk2", "Bänder und Lifte mit 120 statt 60 Teilen pro Minute. Alle vorhandenen Bänder und Lifte werden automatisch aufgerüstet.",
         (3, 1), ["Kupfer"], [], ZEIT, [f"{R}/Buildings/Recipe_ConveyorBeltMk2", f"{R}/Buildings/Recipe_ConveyorLiftMk2"], f"{P}/IronPlateReinforced/Desc_IronPlateReinforced"),
        ("Munitionspuffer", "Munitionspuffer", "Der Munitionseingang jedes Turms fasst doppelt so viel. Türme halten länger durch, wenn das Band stockt.",
         (4, 2), ["BandMk2"], [], ZEIT, [], BOLZEN),
        ("Giesserei", "Gießerei und Stahl", "Gießerei (im Laden kaufbar), Stahlbarren aus Eisenerz und Kohle, Stahlrohre und Stahlträger. Für Nobelisken, Turbomunition und Band Mk3.",
         (1, 1), ["Assembler"], [], ZEIT, STAHL, f"{P}/SteelIngot/Desc_SteelIngot"),
        ("BandMk3", "Förderband Mk3", "Bänder und Lifte mit 270 Teilen pro Minute (Baumaterial: Stahlträger). Alle Bänder und Lifte werden automatisch aufgerüstet.",
         (3, 2), ["BandMk2"], [], ZEIT, [f"{R}/Buildings/Recipe_ConveyorBeltMk3", f"{R}/Buildings/Recipe_ConveyorLiftMk3"], f"{P}/SteelPlate/Desc_SteelPlate"),
        ("Raffinerie", "Raffinerie und Öl", "Ölpumpe und Raffinerie (beide im Laden kaufbar), Kunststoff und Gummi. Voraussetzung für Flammenwerfer und Treibstoffkraftwerk.",
         (1, 2), ["Giesserei"], [], ZEIT,
         [f"{R}/Buildings/Recipe_OilPump", f"{R}/Buildings/Recipe_OilRefinery", f"{R}/OilRefinery/Recipe_Plastic", f"{R}/OilRefinery/Recipe_Rubber"], f"{P}/Plastic/Desc_Plastic"),
        ("Treibstoffkraft", "Treibstoffkraftwerk", "Treibstoffkraftwerk (150 MW, im Laden kaufbar): verbrennt Treibstoff oder Biotreibstoff aus der Raffinerie.",
         (2, 3), ["Raffinerie"], [], ZEIT,
         [f"{R}/Buildings/Recipe_GeneratorFuel", f"{R}/OilRefinery/Recipe_LiquidFuel"], f"{P}/Fuel/Desc_LiquidFuel"),
        # 0.82 Rohstoff-Knoten (Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07): jede Gabelung der Munition braucht einen davon
        ("Schwefel", "Schwefel", "Schwefel-Vorkommen abbauen (Miner). Schwarzpulver wie im Originalspiel aus Kohle und Schwefel. Grundlage für Nobelisk, Brandmunition, Gas-Nobelisk und Kompaktkohle.",
         (0, 2), ["Giesserei"], [], ZEIT, [], f"{G}/Resource/RawResources/Sulfur/Desc_Sulfur"),
        ("Quarz", "Quarzverarbeitung", "Rohquarz abbauen (Miner) und im Konstruktor zu Quarzkristall verarbeiten. Für Streubolzen und Puls-Nobelisk.",
         (2, 1), ["Assembler"], [], ZEIT, [f"{R}/Constructor/Recipe_QuartzCrystal"], f"{P}/QuartzCrystal/Desc_QuartzCrystal"),
        ("Caterium", "Caterium", "Caterium-Erz abbauen, Caterium-Barren im Schmelzofen, Turbodraht im Konstruktor. Für Schockbolzen und Zielsuchmunition.",
         (5, 1), ["Kupfer"], [], ZEIT, [f"{R}/Smelter/Recipe_IngotCaterium", f"{R}/Constructor/Recipe_Quickwire"], f"{P}/HighSpeedWire/Desc_HighSpeedWire"),
        ("Bauxit", "Bauxit und Aluminium", "Bauxit abbauen, Aluminiumbarren in der Gießerei aus Bauxit und Kohle (FICSIT-Feldrezept, ohne Wasser), Aluminiumgehäuse im Konstruktor. Für Turbomunition.",
         (1, 3), ["Raffinerie"], [], ZEIT, [f"{TDR}/Recipe_TD_Aluminiumbarren", f"{R}/Constructor/Recipe_AluminumCasing"], f"{P}/AluminumCasing/Desc_AluminumCasing"),
    ]),
    # 0.73 Spezialisierung, 0.78.3 schmaler (Marius: MAM scrollt nicht nach rechts): Bolzenturm 0-2 | Gewehrturm 3-5 | Nobelisk-Werfer 0-2 ab Zeile 3
    ("Ballistik", "FICSIT-Abwehr: Ballistik", "Türme und Munition, die Projektile verschießen: Bolzenturm, Gewehrturm, Nobelisk-Werfer. Jede Linie hat eine Gabelung.", BOLZEN, [
        ("Durchschlagbolzen", "Durchschlagbolzen", "Bolzenturm Stufe 2: Armierungsbolzen + Schrauben im Assembler. Durchschlägt das Ziel und trifft bis zu 2 weitere Gegner in der Schusslinie.",
         (1, 0), [], [], ZEIT, [f"{TD}/BoltTurret/Recipe_TD_Shoot_Durchschlag", f"{TDR}/Recipe_TD_Durchschlagbolzen"], DURCH),
        ("Gewehrturm", "Gewehrturm", "Neuer Turm mit Dauerfeuer, stark gegen flinke Gegner. Munition: Kupferblech + Eisenstange im Assembler. Level 2 im Turmfenster ist damit freigeschaltet.",
         (4, 0), [], [], ZEIT,
         [f"{TD}/GunTurret/Recipe_TD_GunTurret", f"{TD}/GunTurret/Recipe_TD_Gun_Standard", f"{TDR}/Recipe_TD_Patrone"], f"{P}/CartridgeStandard/Desc_CartridgeStandard"),
        ("Explosivbolzen", "Explosivbolzen", "Bolzenturm Stufe 3 (spät): Durchschlagbolzen + rauchschwaches Pulver (Raffinerie: Schwarzpulver + Schwerölrückstand). Großer Flächenschaden (Explosion), stark gegen Hogs und Gruppen.",
         (1, 2), ["Durchschlagbolzen"], [], ZEIT,
         [f"{TD}/BoltTurret/Recipe_TD_Shoot_Explosive", f"{TDR}/Recipe_TD_Explosivbolzen", f"{R}/Equipment/Recipe_GunpowderMK2"], f"{G}/Equipment/RebarGun/Ammo/Desc_Rebar_Explosive"),
        ("Streubolzen", "Streubolzen", GABEL + "Bolzenturm Stufe 2: Durchschlagbolzen + Quarzkristall. Zerfällt in 3 Splitter auf weitere Ziele, stark gegen Schwärme.",
         (0, 1), ["Durchschlagbolzen"], [], ZEIT,
         [f"{TD}/BoltTurret/Recipe_TD_Shoot_Spreadshot", f"{TDR}/Recipe_TD_Streubolzen"], f"{G}/Equipment/RebarGun/Ammo/Desc_Rebar_Spreadshot"),
        ("Zielsuchmunition", "Zielsuchmunition", GABEL + "Gewehrturm Stufe 2: Gewehrmunition + Turbodraht (Caterium). Trifft immer, auch schnelle Ziele, doppelter Schaden je Schuss.",
         (3, 1), ["Gewehrturm"], [], ZEIT,
         [f"{TD}/GunTurret/Recipe_TD_Gun_Homing", f"{TDR}/Recipe_TD_Zielsuchmunition"], f"{G}/Equipment/Rifle/Ammo/Desc_CartridgeSmartProjectile"),
        ("Panzerbrechend", "Panzerbrechende Munition", GABEL + "Gewehrturm Stufe 2: 2 Gewehrmunition + Stahlrohr im Assembler. Durchschlag-Schaden statt Kugel: geht durch die Panzerung der Hogs.",
         (5, 1), ["Gewehrturm"], [], ZEIT,
         [f"{TD}/GunTurret/Recipe_TD_Gun_Panzer", f"{TDR}/Recipe_TD_PatronePanzer"], f"{P}/CartridgeStandard/Desc_CartridgeStandard"),
        ("NobeliskWerfer", "Nobelisk-Werfer", "Neuer Turm: wirft Nobelisken im Bogen, großer Flächenschaden (Explosion). Schwarzpulver: Kohle + Schwefel; Nobelisk: 2 Schwarzpulver + Stahlrohr (Assembler). Level 2 im Turmfenster ist damit freigeschaltet.",
         (1, 3), [], [], ZEIT,
         [f"{TD}/NobeliskLauncher/Recipe_TD_NobeliskLauncher", f"{TD}/NobeliskLauncher/Recipe_TD_Nobelisk_Explosive", f"{TDR}/Recipe_TD_Schwarzpulver", f"{TDR}/Recipe_TD_Nobelisk"], f"{P}/NobeliskExplosive/Desc_NobeliskExplosive"),
        ("Schockbolzen", "Schockbolzen", GABEL + "Bolzenturm Stufe 2: Durchschlagbolzen + Turbodraht (Caterium). Strom-Schaden statt Durchschlag und betäubt jeden Getroffenen 3 Sekunden: die Antwort des Bolzenturms auf Stinger.",
         (2, 1), ["Durchschlagbolzen"], [], ZEIT,
         [f"{TD}/BoltTurret/Recipe_TD_Shoot_Stunshot", f"{TDR}/Recipe_TD_Schockbolzen"], f"{G}/Equipment/RebarGun/Ammo/Desc_Rebar_Stunshot"),
        ("Brandmunition", "Brandmunition", "Gewehrturm Stufe 2: 2 Gewehrmunition + Schwefel im Assembler. Jeder Treffer setzt das Ziel kurz in Brand (zusätzlicher Feuer-Schaden).",
         (4, 2), ["Gewehrturm"], [], ZEIT,
         [f"{TD}/GunTurret/Recipe_TD_Gun_Brand", f"{TDR}/Recipe_TD_PatroneBrand"], f"{P}/CartridgeStandard/Desc_CartridgeStandard"),
        ("GasNobelisk", "Gas-Nobelisk", GABEL + "Nobelisk + 2 Schwefel: Giftwolke, die Gegner darin über Zeit schädigt. Gift-Schaden, gegen den keine Gegnerart geschützt ist.",
         (0, 4), ["NobeliskWerfer"], [], ZEIT,
         [f"{TD}/NobeliskLauncher/Recipe_TD_Nobelisk_Gas", f"{TDR}/Recipe_TD_NobeliskGas"], f"{G}/Equipment/NobeliskDetonator/Ammo/Desc_NobeliskGas"),
        ("PulsNobelisk", "Puls-Nobelisk", GABEL + "Nobelisk + 2 Quarzkristall: Druckwelle, die Gegner zurückwirft und betäubt.",
         (2, 4), ["NobeliskWerfer"], [], ZEIT,
         [f"{TD}/NobeliskLauncher/Recipe_TD_Nobelisk_Pulse", f"{TDR}/Recipe_TD_NobeliskPuls"], f"{G}/Equipment/NobeliskDetonator/Ammo/Desc_NobeliskShockwave"),
        ("Turbomunition", "Turbomunition", "Gewehrturm Stufe 3 (spät): 2 Gewehrmunition + Aluminiumgehäuse. Doppelte Feuerrate, Magazin mit 75 Schuss.",
         (4, 3), ["Brandmunition"], [], ZEIT,
         [f"{TD}/GunTurret/Recipe_TD_Gun_Turbo", f"{TDR}/Recipe_TD_Turbomunition"], f"{G}/Equipment/Rifle/Ammo/Desc_CartridgeChaos"),
        ("StreuNobelisk", "Streu-Nobelisk", "Nobelisk-Werfer Stufe 3 (spät): 2 Nobelisken + 2 rauchschwaches Pulver (Raffinerie). Zerfällt in kleine Sprengsätze über eine große Fläche.",
         (1, 5), ["NobeliskWerfer"], [], ZEIT,
         [f"{TD}/NobeliskLauncher/Recipe_TD_Nobelisk_Cluster", f"{TDR}/Recipe_TD_NobeliskStreu", f"{R}/Equipment/Recipe_GunpowderMK2"], f"{G}/Equipment/NobeliskDetonator/Ammo/Desc_NobeliskCluster"),
        # 0.83 Bauten P3
        ("Himmelsturm", "Himmelsturm", "Neuer Turm: Flugabwehr gegen fliegende Krabben (60 m, 200 Schaden in 6 m). Munition: Flak-Kanister = Aluminiumgehäuse + 2 Schwarzpulver im Assembler. Greift keine Bodengegner an.",
         (3, 4), ["NobeliskWerfer"], [], ZEIT, [f"{TD}/FlakTurret/Recipe_TD_FlakTurret", f"{TD}/FlakTurret/Recipe_TD_Flak_Shoot", f"{TDR}/Recipe_TD_FlakKanister"], f"{P}/AluminumCasing/Desc_AluminumCasing"),
        ("Singularitaet", "Singularitätswerfer", "Neuer Turm (Endspiel): Ficsonium-Ladung hält alle Gegner in 12 m fest und sprengt sie nach 1,5 s mit 1500 Explosionsschaden. Ficsonium aus dem Teilchenbeschleuniger.",
         (3, 5), ["Himmelsturm"], [], ZEIT, [f"{TD}/SingularityLauncher/Recipe_TD_SingularityLauncher", f"{TD}/SingularityLauncher/Recipe_TD_Singularitaet_Ficsonium"], f"{P}/Ficsonium/Desc_Ficsonium"),
    ]),
    # Spalten: Teslaspule 0-2 | Flammenwerfer 4
    ("Elementar", "FICSIT-Abwehr: Elementar", "Türme ohne Projektile: Strom (Teslaspule) und Feuer (Flammenwerfer). Erscheint mit dem Kohlekraftwerk.", DRAHT, [
        ("Teslaspule", "Teslaspule", "Neuer Turm: Kettenblitz aus Strom, ohne Munition, springt auf weitere Gegner. Braucht beim Aufladen 75 MW. Level 2 im Turmfenster ist damit freigeschaltet.",
         (1, 0), [], [], ZEIT,
         [f"{TD}/TeslaCoil/Recipe_TD_TeslaCoil", f"{TD}/TeslaCoil/Recipe_TD_Tesla_Discharge"], DRAHT),
        ("Kettenblitz", "Kettenblitz", GABEL + "Jede Entladung springt auf 3 weitere Gegner. Stark gegen Brut und Schwärme.",
         (0, 1), ["Teslaspule"], [], ZEIT, [], DRAHT),
        ("Ueberlast", "Überlast", GABEL + "Die ganze Ladung geht in das Hauptziel: Schaden x2,5, dafür keine Sprünge. Stark gegen Elite und Bosse.",
         (2, 1), ["Teslaspule"], [], ZEIT, [], DRAHT),
        ("Akkubank", "Akkubank", "Teslaspulen laden aus einem Puffer: beim Aufladen ziehen sie nur noch die Hälfte der Leistung aus dem Netz.",
         (1, 2), ["Teslaspule"], [], ZEIT, [], KABEL),
        # 0.82 (Marius 07.11.): Flamme startet mit Oel, kein Biotreibstoff mehr (Biomasse kommt im Einsatz nicht aus der Fabrik)
        ("Flammenwerfer", "Flammenwerfer", "Neuer Turm: Flammenkegel mit Nachbrennen, trifft alles im Kegel. Treibstoff aus Rohöl (Ölpumpe, Raffinerie) per Rohr. Level 2 im Turmfenster ist damit freigeschaltet.",
         (4, 0), [], [], ZEIT,
         [f"{TD}/FlameTurret/Recipe_TD_FlameTurret", f"{TD}/FlameTurret/Recipe_TD_Flame_Fuel", f"{R}/OilRefinery/Recipe_LiquidFuel"], f"{P}/Fuel/Desc_LiquidFuel"),
        ("Turbotreibstoff", "Turbotreibstoff", "Flammenwerfer Stufe 2: Treibstoff + Kompaktkohle in der Raffinerie, 50 % mehr Schaden. Kompaktkohle: 5 Kohle + 5 Schwefel im Assembler.",
         (4, 3), ["Flammenwerfer"], [], ZEIT,
         [f"{TD}/FlameTurret/Recipe_TD_Flame_Turbo", f"{R}/AlternateRecipes/Parts/Recipe_Alternate_Turbofuel", f"{TDR}/Recipe_TD_Kompaktkohle"], f"{P}/Turbofuel/Desc_LiquidTurboFuel"),
        # 0.81 passive Bauten P1
        ("Oelschlick", "Ölschlick-Feld", "Neues Gebäude: Bodenplatte 8 x 8 m, per Rohr mit Schwerölrückstand aus der Raffinerie versorgt. Gegner darauf laufen 35 % langsamer; trifft ein Flammenwerfer einen Gegner darauf, brennt die Platte 5 s.",
         (5, 2), ["Flammenwerfer"], [], ZEIT, [f"{TD}/SlickPlate/Recipe_TD_SlickPlate", f"{TD}/SlickPlate/Recipe_TD_Slick_HeavyOil"], f"{P}/HeavyOilResidue/Desc_HeavyOilResidue"),
        # 0.83 Bauten P2 (Gabel G mit dem Oelschlick: Kontrolle durch Feuer oder durch Strom)
        ("Elektrozaun", "Elektrozaun", GABEL + "Neues Gebäude: Mauerstück 8 x 4 m mit Kupferstäben. Für Gegner eine Mauer; wer ihn angreift, bekommt einen Stromschlag (15 Schaden, 1 s Betäubung). Braucht 2 MW.",
         (3, 2), ["Teslaspule"], [], ZEIT, [f"{TD}/ConductorWall/Recipe_TD_ConductorWall", f"{TD}/ConductorWall/Recipe_TD_Zaun_Strom"], f"{P}/CopperSheet/Desc_CopperSheet"),
    ]),
    # 0.78.3 fuenf Spalten: Kern 0-3 | Mauern 4 (Zeilen 0-1) | Reparaturstation 3 und Turmfertigung 4 ab Zeile 2 | Doktrinen Zeile 5
    ("Festung", "FICSIT-Abwehr: Festung", "Kern, Doktrin, Turmfertigung und Mauern: aushalten, reparieren, eine Hauptlinie wählen, Türme auf höhere Level bringen.", f"{P}/ModularFrame/Desc_ModularFrame", [
        ("KernPanzer", "Kern-Panzerung", "Der Kern hält 30 % mehr aus (höchste Lebenspunkte x1,3).",
         (1, 0), [], [], ZEIT, [], f"{P}/ModularFrame/Desc_ModularFrame"),
        ("KernNotrep", "Notreparatur", "Nach jeder abgewehrten Welle repariert sich der Kern um 10 % seiner Lebenspunkte.",
         (1, 1), ["KernPanzer"], [], ZEIT, [], EISENPLATTE),
        ("MauerMk2", "Verstärkte Mauern", "Alle Mauern haben doppelte Lebenspunkte.",
         (4, 0), [], [], ZEIT, [], f"{P}/Cement/Desc_Cement"),
        ("Reparaturstation", "Reparaturstation", "Neues Gebäude: repariert Mauern, Türme und Kern im Umkreis von 25 m. Verbraucht Eisenplatten oder verstärkte Platten.",
         (1, 2), ["KernNotrep"], [], ZEIT,
         [f"{TD}/RepairStation/Recipe_TD_RepairStation", f"{TD}/RepairStation/Recipe_TD_Repair_IronPlate", f"{TD}/RepairStation/Recipe_TD_Repair_Reinforced"], EISENPLATTE),
        ("KernSchild", "Kern-Schild", GABEL + "Ein Schild fängt in jeder Welle die ersten 2000 Schaden am Kern ab und lädt sich zum Start der nächsten Welle neu.",
         (0, 1), ["KernPanzer"], [], ZEIT, [], f"{P}/IronPlateReinforced/Desc_IronPlateReinforced"),
        ("KernGeschuetz", "Kern-Geschütz", GABEL + "Der Kern schießt selbst: alle 1,5 s ein Bolzen mit 60 Schaden auf den nächsten Gegner in 30 m. Die letzte Linie, kein Ersatz für Türme.",
         (2, 1), ["KernPanzer"], [], ZEIT, [], BOLZEN),
        ("Turmfertigung2", "Turmfertigung II", "Alle Türme können auf Level 3 gebracht werden (im Turmfenster, 15 EP je Turm).",
         (2, 4), [], [], ZEIT, [], f"{P}/IronPlateReinforced/Desc_IronPlateReinforced"),
        ("Stachelmauer", "Stachelmauern", "Gegner, die eine Mauer angreifen, nehmen ein Viertel ihres Schadens selbst (mindestens 15).",
         (4, 1), ["MauerMk2"], [], ZEIT, [], f"{P}/SteelPlate/Desc_SteelPlate"),
        ("Turmfertigung3", "Turmfertigung III", "Alle Türme können auf Level 4 gebracht werden (im Turmfenster, 25 EP je Turm). Die Türme der Doktrin bekommen schon Level 5.",
         (5, 5), ["Turmfertigung2"], [], ZEIT, [], f"{P}/ModularFrame/Desc_ModularFrame"),
        ("DoktrinBolzen", "Doktrin: Bolzenturm", DOKTRIN + "Bolzentürme machen 25 % mehr Schaden, ihre Level kosten 25 % weniger EP und Level 5 gibt es schon mit Turmfertigung III.",
         (0, 5), ["Turmfertigung2"], [], ZEIT, [], BOLZEN),
        ("DoktrinGewehr", "Doktrin: Gewehrturm", DOKTRIN + "Gewehrtürme machen 25 % mehr Schaden, ihre Level kosten 25 % weniger EP und Level 5 gibt es schon mit Turmfertigung III.",
         (1, 5), ["Turmfertigung2"], [], ZEIT, [], f"{P}/CartridgeStandard/Desc_CartridgeStandard"),
        ("DoktrinNobelisk", "Doktrin: Nobelisk-Werfer", DOKTRIN + "Nobelisk-Werfer machen 25 % mehr Schaden, ihre Level kosten 25 % weniger EP und Level 5 gibt es schon mit Turmfertigung III.",
         (2, 5), ["Turmfertigung2"], [], ZEIT, [], f"{P}/NobeliskExplosive/Desc_NobeliskExplosive"),
        ("DoktrinTesla", "Doktrin: Teslaspule", DOKTRIN + "Teslaspulen machen 25 % mehr Schaden, ihre Level kosten 25 % weniger EP und Level 5 gibt es schon mit Turmfertigung III.",
         (3, 5), ["Turmfertigung2"], [], ZEIT, [], DRAHT),
        ("DoktrinFlamme", "Doktrin: Flammenwerfer", DOKTRIN + "Flammenwerfer machen 25 % mehr Schaden, ihre Level kosten 25 % weniger EP und Level 5 gibt es schon mit Turmfertigung III.",
         (4, 5), ["Turmfertigung2"], [], ZEIT, [], f"{P}/BioFuel/Desc_LiquidBiofuel"),
        ("Turmfertigung4", "Turmfertigung IV: Meisterstufe", "Alle Türme können auf Level 5 gebracht werden (40 EP je Turm). Jeder Turm bekommt eine eigene Fähigkeit: Bolzenturm Doppellauf, Gewehrturm Dauerfeuer, Nobelisk-Werfer Streuschuss, Teslaspule Spannungsfeld, Flammenwerfer Feuerteppich.",
         (5, 6), ["Turmfertigung3"], [], ZEIT, [], f"{P}/ModularFrame/Desc_ModularFrame"),
        # 0.81 passive Bauten P1 (Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07)
        ("Tor", "Tor", "Neues Gebäude: Tor für die Mauerlinie (8 m breit, Öffnung 4 x 4 m). Öffnet sich für dich und Fahrzeuge, Gegner kommen nie durch und greifen es wie eine Mauer an. Ein Förderband passt unten durch.",
         (3, 1), ["MauerMk2"], [], ZEIT, [f"{TD}/Gate/Recipe_TD_Gate"], f"{P}/Cement/Desc_Cement"),
        ("Lockfeuer", "Lockfeuer", "Neues Gebäude: verbrennt Kohle und lockt dabei 15 s lang alle Gegner in 40 m an, danach 30 s Pause. Lenkt Wellen auf eine Turmgruppe. Braucht Strom.",
         (3, 2), ["Tor"], [], ZEIT, [f"{TD}/LureBeacon/Recipe_TD_LureBeacon", f"{TD}/LureBeacon/Recipe_TD_Lure_Coal", f"{TD}/LureBeacon/Recipe_TD_Lure_CompactedCoal"], KOHLE),
        # 0.83 Bauten P2/P3 (Gabel H: Schildgenerator oder Stoerfeld-Projektor)
        ("Funkmast", "Funkmast", "Neues Gebäude: Radarmast mit Leitsender. Türme in 30 m bekommen +10 % Reichweite. Braucht dauernd 15 MW.",
         (5, 1), ["MauerMk2"], [], ZEIT, [f"{TD}/RadioMast/Recipe_TD_RadioMast", f"{TD}/RadioMast/Recipe_TD_Funk_Strom"], f"{P}/CrystalOscillator/Desc_CrystalOscillator"),
        ("Schildgenerator", "Schildgenerator", GABEL + "Neues Gebäude: Kuppel 20 m schluckt Schaden an Mauern, Türmen und Bauten (nicht am Kern), 3000 Schildpunkte, gebrochen 20 s Ladezeit. Braucht dauernd 60 MW.",
         (1, 3), ["Reparaturstation"], [], ZEIT, [f"{TD}/ShieldGenerator/Recipe_TD_ShieldGenerator", f"{TD}/ShieldGenerator/Recipe_TD_Schild_Strom"], f"{P}/Battery/Desc_Battery"),
        ("Stoerfeld", "Störfeld-Projektor", GABEL + "Neues Gebäude: verbraucht reanimiertes SAM (1 Barren je 60 s), solange Gegner in 15 m sind, und bremst sie alle um 40 %.",
         (4, 7), ["Turmfertigung4"], [], ZEIT, [f"{TD}/FieldProjector/Recipe_TD_FieldProjector", f"{TD}/FieldProjector/Recipe_TD_Feld_SAM", f"{R}/Smelter/Recipe_IngotSAM"], f"{P}/SAMIngot/Desc_SAMIngot"),
        ("Dimensionslager", "Dimensionslager", "Neues Gebäude: Lager mit Mercer-Kugel, versorgt alle Türme in 30 m direkt mit der passenden Munition aus seinem Inhalt. Kostet eine Mercer-Kugel.",
         (3, 7), ["Turmfertigung4"], [], ZEIT, [f"{TD}/DimensionalDepot/Recipe_TD_DimensionalDepot"], "/Game/FactoryGame/Prototype/WAT/Desc_WAT2"),
    ]),
]

# Alter Ordner je Knoten: Pfad des Schematics bleibt gleich (Spielstaende)
ORDNER = {}
for _o, _ks in {
    "Produktion": ["Assembler", "Kupfer", "Giesserei", "BandMk2", "Raffinerie", "Schwefel", "Quarz", "Caterium", "Bauxit"],
    "Ballistik": ["Durchschlagbolzen", "Gewehrturm", "Explosivbolzen", "Streubolzen", "Zielsuchmunition", "Schockbolzen", "Turbomunition", "Panzerbrechend", "Brandmunition"],
    "Energie": ["Kohlekraft", "Teslaspule", "Treibstoffkraft", "Kettenblitz", "Ueberlast", "Akkubank"],
    "Doktrin": ["DoktrinBolzen", "DoktrinGewehr", "DoktrinNobelisk", "DoktrinTesla", "DoktrinFlamme"],
    "Befestigung": ["Reparaturstation", "Tor", "Lockfeuer", "Elektrozaun", "Funkmast", "Schildgenerator", "Stoerfeld", "Dimensionslager"],
    "Feuer": ["Flammenwerfer", "Turbotreibstoff", "Oelschlick"],
    "Chemie": ["NobeliskWerfer", "GasNobelisk", "PulsNobelisk", "StreuNobelisk", "Himmelsturm", "Singularitaet"],
    "Upgrades": ["Turmfertigung2", "Turmfertigung3", "Turmfertigung4"],
    "Kern": ["KernPanzer", "KernNotrep", "KernSchild", "KernGeschuetz"],
    "Logistik": ["Munitionspuffer", "BandMk3", "MauerMk2", "Stachelmauer"],
}.items():
    for _k in _ks:
        ORDNER[_k] = _o

# Kurzwirkung fuer den Kartentitel (Marius 04.11.: "null Information", wenn man auf einen Knoten geht) - ohne Sonderzeichen ausser Umlauten
KURZ = {
    "Kohlekraft": "Kohlekraftwerk 75 MW", "Assembler": "Assembler, Eisenplatten, Rotoren", "Kupfer": "Kupfer, Draht, Kabel",
    "BandMk2": "Bänder Mk2 (120/min), Umbau automatisch", "Munitionspuffer": "Turmeingang fasst doppelt", "Giesserei": "Gießerei und Stahl",
    "BandMk3": "Bänder Mk3 (270/min), Umbau automatisch", "Raffinerie": "Ölpumpe, Raffinerie, Kunststoff", "Treibstoffkraft": "Treibstoffkraftwerk 150 MW",
    "Durchschlagbolzen": "Bolzen St. 2: trifft 3 in Reihe", "Gewehrturm": "Neuer Turm: Dauerfeuer", "Explosivbolzen": "Bolzen St. 3: großer Flächenschaden",
    "Streubolzen": "Bolzen St. 2: 3 Splitter (Quarz)", "Zielsuchmunition": "Gewehr St. 2: trifft immer (Caterium)", "Schockbolzen": "Bolzen St. 2: Strom, betäubt 3 s (Caterium)",
    "Schwefel": "Schwefel: Schwarzpulver, Kompaktkohle", "Quarz": "Quarzkristall", "Caterium": "Caterium, Turbodraht", "Bauxit": "Aluminiumgehäuse",
    "NobeliskWerfer": "Neuer Turm: Nobelisk im Bogen", "Turbomunition": "Gewehr St. 4: doppelte Feuerrate", "GasNobelisk": "Giftwolke, Schaden über Zeit",
    "PulsNobelisk": "Druckwelle wirft zurück", "StreuNobelisk": "Nobelisk St. 3: Streusprengsätze", "Teslaspule": "Neuer Turm: Kettenblitz, 75 MW",
    "Flammenwerfer": "Neuer Turm: Flammenkegel", "Turbotreibstoff": "Flamme St. 2: Turbotreibstoff",
    "KernPanzer": "Kern +30 % Lebenspunkte", "KernNotrep": "Kern heilt 10 % je Welle", "Panzersperre": "Sperre lenkt Gegnerwege",
    "KernSchild": "Schild fängt 2000 Schaden je Welle", "KernGeschuetz": "Kern schießt selbst (30 m)", "Turmfertigung2": "Alle Türme bis Level 3",
    "MauerMk2": "Mauern LP x2", "Reparaturstation": "Repariert alles in 25 m", "Turmfertigung3": "Alle Türme bis Level 4",
    "Stachelmauer": "Angreifer nehmen 25 % zurück", "Turmfertigung4": "Level 5 mit Meisterfähigkeit",
    "Panzerbrechend": "Gewehr St. 2: Durchschlag statt Kugel", "Brandmunition": "Gewehr St. 3: Treffer brennen nach",
    "Kettenblitz": "Tesla: +3 Sprünge", "Ueberlast": "Tesla: Hauptziel x2,5, keine Sprünge", "Akkubank": "Tesla lädt mit halber Netzlast",
    "DoktrinBolzen": "Hauptlinie Bolzenturm: +25 % Schaden", "DoktrinGewehr": "Hauptlinie Gewehrturm: +25 % Schaden",
    "DoktrinNobelisk": "Hauptlinie Nobelisk-Werfer: +25 % Schaden", "DoktrinTesla": "Hauptlinie Teslaspule: +25 % Schaden",
    "DoktrinFlamme": "Hauptlinie Flammenwerfer: +25 % Schaden",
    "Tor": "Tor: du kommst durch, Gegner nicht", "Lockfeuer": "Lockt Gegner in 40 m an", "Oelschlick": "Bodenplatte bremst um 35 %, brennt",
    "Elektrozaun": "Mauer mit Stromschlag", "Funkmast": "Türme in 30 m +10 % Reichweite", "Schildgenerator": "Kuppel schluckt 3000 Schaden",
    "Stoerfeld": "Bremst alles in 15 m um 40 % (SAM)", "Dimensionslager": "Munition ohne Band in 30 m", "Himmelsturm": "Neuer Turm: Flugabwehr",
    "Singularitaet": "Neuer Turm: Ficsonium-Singularität",
}

# 0.58: einheitlicher Aufbau der Beschreibung (Marius: MAM nicht transparent genug). Laden = was danach im Laden kaufbar ist,
# Gegen = wofuer der Knoten gut ist. "Ab Welle" und "Braucht" ergaenzt das Skript selbst.
LADEN = {
    "Assembler": "Assembler", "Giesserei": "Gießerei", "Raffinerie": "Ölpumpe, Raffinerie", "Gewehrturm": "Gewehrturm",
    "Kohlekraft": "Kohlekraftwerk, Wasserpumpe", "Teslaspule": "Teslaspule", "Treibstoffkraft": "Treibstoffkraftwerk",
    "Panzersperre": "Panzersperre", "Reparaturstation": "Reparaturstation", "Flammenwerfer": "Flammenwerfer", "NobeliskWerfer": "Nobelisk-Werfer",
    "Tor": "Tor", "Lockfeuer": "Lockfeuer", "Oelschlick": "Ölschlick-Feld",
    "Elektrozaun": "Elektrozaun", "Funkmast": "Funkmast", "Schildgenerator": "Schildgenerator", "Stoerfeld": "Störfeld-Projektor",
    "Dimensionslager": "Dimensionslager", "Himmelsturm": "Himmelsturm", "Singularitaet": "Singularitätswerfer",
}
GEGEN = {
    "Durchschlagbolzen": "Gegner in einer Reihe (Hogs)", "Gewehrturm": "schnelle Gegner (Stinger)", "Explosivbolzen": "Hogs und Gruppen",
    "Streubolzen": "Schwärme", "Zielsuchmunition": "schnelle Stinger", "Schockbolzen": "Stinger, Wasserspucker (Strom)", "Panzerbrechend": "Hogs (Panzer)", "Brandmunition": "Waldspucker (Feuer)", "Kettenblitz": "Brut und Schwärme", "Ueberlast": "Elite und Bosse", "Turbomunition": "Massen",
    "Teslaspule": "Gruppen, Wasserspucker", "Flammenwerfer": "Schwärme, Waldspucker", "Turbotreibstoff": "Schwärme",
    "NobeliskWerfer": "Gruppen auf Entfernung", "GasNobelisk": "dichte Pulks, Stinger (Gift)", "PulsNobelisk": "Durchbrüche (zurückwerfen)", "StreuNobelisk": "große Wellen",
    "Panzersperre": "Wege lenken", "Reparaturstation": "lange Wellen", "KernSchild": "Durchbrüche am Kern", "KernGeschuetz": "einzelne Durchbrüche",
    "Stachelmauer": "Gegner an Mauern", "MauerMk2": "Angriffe auf Mauern", "Tor": "eingemauerte Basis", "Lockfeuer": "Wellen auf die Türme lenken", "Oelschlick": "Massen vor Flammenwerfern", "Elektrozaun": "Gegner an Mauern (Strom)", "Funkmast": "Türme mit knapper Reichweite", "Schildgenerator": "Spucker und Durchbrüche an Bauten", "Stoerfeld": "Bosse und Massen", "Dimensionslager": "Türme ohne Bandanschluss", "Himmelsturm": "fliegende Krabben", "Singularitaet": "Endwellen", "Turmfertigung2": "stärkere Wellen", "Turmfertigung3": "späte Wellen", "Turmfertigung4": "Bosse und Endwellen",
}

# 0.72: Preis aus der Phase der effektiven Freigabe-Welle (Konzept-Forschung-2.md 4.1): P1 bis W4, P2 ab W5, P3 ab W15, P4 ab W25, P5 ab W35
PHASEN_WELLE = [0, 5, 15, 25, 35]
PHASEN_PREIS = [3, 4, 6, 8, 10]

# Baeume, die erst nach einem Knoten eines anderen Baums erscheinen (das MAM zeigt gesperrte Einzelknoten trotzdem an)
# 0.83.1 (Marius 07.11.: neue Seite erst nach Schliessen und Oeffnen des MAM sichtbar): das offene MAM baut die Liste der Baeume
# nicht neu auf. Alle Baeume sind deshalb ab Start da; ihre Wurzeln bleiben grau ohne Icon, bis die Voraussetzung erforscht ist
# (Durchschlagbolzen/Gewehrturm <- Assembler, Teslaspule <- Kohlekraftwerk, siehe VORAUSSETZUNGEN).
BAUM_VORAUSSETZUNG = {}

# Voraussetzungen ausserhalb der Eltern im Baum: (Baum, Knoten) -> [(Baum, Knoten)]. Der Knoten bleibt verborgen, bis sie erforscht sind.
VORAUSSETZUNGEN = {
    ("Industrie", "BandMk3"): [("Industrie", "Giesserei")],
    ("Industrie", "Treibstoffkraft"): [("Industrie", "Kohlekraft")],
    ("Ballistik", "Durchschlagbolzen"): [("Industrie", "Assembler")],
    ("Ballistik", "Gewehrturm"): [("Industrie", "Assembler"), ("Industrie", "Kupfer")],
    ("Ballistik", "NobeliskWerfer"): [("Industrie", "Giesserei"), ("Industrie", "Schwefel")],
    ("Ballistik", "Streubolzen"): [("Industrie", "Quarz")],
    ("Ballistik", "Schockbolzen"): [("Industrie", "Caterium")],
    ("Ballistik", "Zielsuchmunition"): [("Industrie", "Caterium")],
    ("Ballistik", "Brandmunition"): [("Industrie", "Schwefel")],
    ("Ballistik", "Explosivbolzen"): [("Industrie", "Raffinerie")],
    ("Ballistik", "StreuNobelisk"): [("Industrie", "Raffinerie")],
    ("Elementar", "Turbotreibstoff"): [("Industrie", "Schwefel")],
    ("Ballistik", "Turbomunition"): [("Industrie", "Bauxit")],
    ("Ballistik", "PulsNobelisk"): [("Industrie", "Quarz")],
    ("Ballistik", "Panzerbrechend"): [("Industrie", "Giesserei")],
    ("Festung", "DoktrinGewehr"): [("Ballistik", "Gewehrturm")],
    ("Festung", "DoktrinNobelisk"): [("Ballistik", "NobeliskWerfer")],
    ("Festung", "DoktrinTesla"): [("Elementar", "Teslaspule")],
    ("Festung", "DoktrinFlamme"): [("Elementar", "Flammenwerfer")],
    ("Elementar", "Teslaspule"): [("Industrie", "Kohlekraft")],
    ("Elementar", "Flammenwerfer"): [("Industrie", "Kohlekraft"), ("Industrie", "Raffinerie")],
    ("Festung", "Reparaturstation"): [("Industrie", "Assembler")],
    ("Elementar", "Oelschlick"): [("Industrie", "Raffinerie")],
    # 0.83 Bauten P2/P3
    ("Festung", "Funkmast"): [("Industrie", "Quarz")],
    ("Festung", "Schildgenerator"): [("Elementar", "Akkubank")],
    ("Ballistik", "Himmelsturm"): [("Industrie", "Bauxit")],
    ("Ballistik", "Singularitaet"): [("Festung", "Turmfertigung4")],   # 0.81: als Voraussetzung statt Elternlinie (die Linie liefe durch Notreparatur)
}

# Freigabe-Welle (unveraendert gegenueber 0.71, ueberschreibbar mit Forschung.<Knoten>.AbWelle in Balance.json)
AB_WELLE = {
    ("Industrie", "Schwefel"): 5,
    ("Industrie", "Quarz"): 10,
    ("Industrie", "Caterium"): 12,
    ("Industrie", "Bauxit"): 25,
    ("Ballistik", "Explosivbolzen"): 25,
    ("Festung", "Tor"): 6,
    ("Festung", "Lockfeuer"): 8,
    ("Elementar", "Oelschlick"): 15,
    ("Elementar", "Elektrozaun"): 12, ("Festung", "Funkmast"): 12, ("Festung", "Schildgenerator"): 25, ("Ballistik", "Himmelsturm"): 25,
    ("Festung", "Stoerfeld"): 40, ("Ballistik", "Singularitaet"): 45, ("Festung", "Dimensionslager"): 45,
    ("Industrie", "Kohlekraft"): 6,
    ("Industrie", "Raffinerie"): 15,
    ("Industrie", "Treibstoffkraft"): 15,
    ("Industrie", "BandMk3"): 10,
    ("Ballistik", "Durchschlagbolzen"): 5,
    ("Ballistik", "Gewehrturm"): 5,
    ("Ballistik", "NobeliskWerfer"): 10,   # 0.73: Artillerie-Linie vor der Doktrin waehlbar
    ("Ballistik", "Brandmunition"): 15,
    ("Ballistik", "GasNobelisk"): 15,
    ("Ballistik", "PulsNobelisk"): 15,
    ("Ballistik", "Turbomunition"): 25,
    ("Ballistik", "StreuNobelisk"): 25,
    ("Elementar", "Teslaspule"): 5,
    ("Elementar", "Akkubank"): 15,
    ("Elementar", "Flammenwerfer"): 15,
    ("Elementar", "Turbotreibstoff"): 25,
    ("Festung", "KernSchild"): 8,
    ("Festung", "KernGeschuetz"): 10,
    ("Festung", "MauerMk2"): 6,
    ("Festung", "Stachelmauer"): 12,
    ("Festung", "Turmfertigung2"): 10,
    ("Festung", "Turmfertigung3"): 20,
    ("Festung", "Turmfertigung4"): 30,
    ("Festung", "DoktrinBolzen"): 15, ("Festung", "DoktrinGewehr"): 15, ("Festung", "DoktrinNobelisk"): 15,
    ("Festung", "DoktrinTesla"): 15, ("Festung", "DoktrinFlamme"): 15,
}

# Gabelungen (nur eine Seite je Einsatz); dieselben Paare stehen in TDForschungVoraussetzungen.inl (TDForschungAbzweigungen)
GRUPPEN = [("Ballistik", ["Streubolzen", "Schockbolzen"]), ("Ballistik", ["Zielsuchmunition", "Panzerbrechend"]), ("Ballistik", ["GasNobelisk", "PulsNobelisk"]),
           ("Elementar", ["Kettenblitz", "Ueberlast"]), ("Elementar", ["Oelschlick", "Elektrozaun"]), ("Festung", ["Schildgenerator", "Stoerfeld"]), ("Festung", ["KernSchild", "KernGeschuetz"]),
           ("Festung", ["DoktrinBolzen", "DoktrinGewehr", "DoktrinNobelisk", "DoktrinTesla", "DoktrinFlamme"])]
def andere_von(b_key, k_key):
    for g_b, glieder in GRUPPEN:
        if g_b == b_key and k_key in glieder:
            return [g for g in glieder if g != k_key]
    return []

def eff_welle(b_key, k_key):
    """Freigabe-Welle einschliesslich aller Eltern und Voraussetzungen"""
    knoten = {kk[0]: kk for bb in BAEUME for kk in bb[4]}
    baum_von = {kk[0]: bb[0] for bb in BAEUME for kk in bb[4]}
    w = AB_WELLE.get((b_key, k_key), 0)
    for e in knoten[k_key][4]:
        w = max(w, eff_welle(b_key, e))
    for vb, vk in VORAUSSETZUNGEN.get((b_key, k_key), []):
        w = max(w, eff_welle(baum_von[vk], vk))
    return w

def preis(b_key, k_key):
    w = eff_welle(b_key, k_key)
    phase = max(i for i, ab in enumerate(PHASEN_WELLE) if w >= ab)
    return PHASEN_PREIS[phase]

GX = "X_2_3FF107F84D30EB52DD50898C7D2CAD67"
GY = "Y_4_F18C5B824136D7759146338CAA496F0A"
def punkt(x, y):
    return f"({GX}={x},{GY}={y})"

def knoten_text(schema_klasse, xy, eltern_xy, kinder_xy):
    x, y = xy
    eltern = ",".join(punkt(*e) for e in eltern_xy)
    strassen = []
    for cx, cy in kinder_xy:
        # Linie: zuerst waagerecht in der Zeile des Elternknotens, dann senkrecht nach unten (wie im Spiel)
        weg = [] if cx == x else [(cx, y)]
        weg += [(cx, yy) for yy in range(y + 1, cy + 1)]
        strassen.append(f"({punkt(cx, cy)}, (Points_10_9533B9104470D8E053E7ACA5C4C9F865=({','.join(punkt(*w) for w in weg)})))")
    kinder = ",".join(punkt(*k) for k in kinder_xy)
    return (f"(Schematic_27_3663A42446FDB4387D0C81AFC23E225B=\"/Script/Engine.BlueprintGeneratedClass'{schema_klasse}'\","
            f"Coordinates_23_5A3DE6C040C7026EDEA49E9CE8612292={punkt(x, y)},"
            f"Parents_20_7A099B96409362536B743BA1CC77C234={'(' + eltern + ')' if eltern else ''},"
            f"ChildrenAndRoads_34_758C9E0D4F09DAF4BBAD309358952A0A={'(' + ','.join(strassen) + ')' if strassen else ''},"
            f"UnhiddenBy_38_909B07D7461225A33C48A68A7139FE63={'(' + eltern + ')' if eltern else ''},"
            f"NodesToUnhide_33_A6E465554D49C98EE2A0ECB493BE5CBA={'(' + kinder + ')' if kinder else ''})")

NAMEN = {(bb[0], kk[0]): kk[1] for bb in BAEUME for kk in bb[4]}
TITEL = {bb[0]: bb[1].replace("FICSIT-Abwehr: ", "") for bb in BAEUME}

def braucht(b_key, k_key, eltern):
    """Alle Voraussetzungen als (Baum, Knoten): Eltern im Baum und Knoten aus anderen Baeumen"""
    return [(b_key, e) for e in eltern] + VORAUSSETZUNGEN.get((b_key, k_key), [])

def beschreibung(b_key, k_key, k_text, eltern):
    zeilen = [k_text, ""]
    if k_key in LADEN:
        zeilen.append(f"Im Laden: {LADEN[k_key]}")
    if k_key in GEGEN:
        zeilen.append(f"Gut gegen: {GEGEN[k_key]}")
    if eff_welle(b_key, k_key) > 0:
        zeilen.append(f"Ab Welle: {eff_welle(b_key, k_key)}")
    vor = braucht(b_key, k_key, eltern)
    if vor:
        zeilen.append("Braucht: " + ", ".join(f"{TITEL[vb]} > {NAMEN[(vb, vk)]}" for vb, vk in vor))
    if andere_von(b_key, k_key):
        zeilen.append("Abzweigung: schließt " + ", ".join(NAMEN[(b_key, a)] for a in andere_von(b_key, k_key)) + " für diesen Einsatz aus")
    return "\n".join(z for z in zeilen).strip()

def icon_von(item_pfad):
    cdo = unreal.get_default_object(unreal.load_class(None, item(item_pfad)))
    return cdo.get_editor_property("mPersistentBigIcon") or cdo.get_editor_property("mSmallIcon")

def kosten(liste):
    out = []
    for pfad, n in liste:
        ia = unreal.ItemAmount()
        ia.set_editor_property("item_class", unreal.load_class(None, item(pfad)))
        ia.set_editor_property("amount", n)
        out.append(ia)
    return out

INFO_KLASSE = unreal.load_class(None, "/Game/FactoryGame/Unlocks/BP_UnlockInfoOnly.BP_UnlockInfoOnly_C")
# Ueberschrift der Wirkungskarte (sonst der Knotenname)
KARTE_ALT = {
    "UpgBolzen": "Bolzentürme: Level 2", "UpgGewehr": "Gewehrtürme: Level 2", "UpgTesla": "Teslaspulen: Level 2",
    "UpgFlammen": "Flammenwerfer: Level 2", "UpgNobelisk": "Nobelisk-Werfer: Level 2",
    "Turmfertigung2": "Alle Türme: Level 3", "Turmfertigung3": "Alle Türme: Level 4", "Turmfertigung4": "Alle Türme: Level 5 (Meisterstufe)",
}
GEBAEUDE_REZEPTE = ("Recipe_TD_GunTurret", "Recipe_TD_TeslaCoil", "Recipe_TD_FlameTurret", "Recipe_TD_NobeliskLauncher", "Recipe_TD_RepairStation", "Recipe_TD_Barrier", "Recipe_TD_BoltTurret",
                    "Recipe_TD_Gate", "Recipe_TD_LureBeacon", "Recipe_TD_SlickPlate",
                    # 0.83: ohne diese Eintraege galten die Baurezepte als stille Aktionsrezepte, die Knoten zeigten "0 Rezepte"
                    "Recipe_TD_ConductorWall", "Recipe_TD_RadioMast", "Recipe_TD_ShieldGenerator", "Recipe_TD_FlakTurret",
                    "Recipe_TD_FieldProjector", "Recipe_TD_SingularityLauncher", "Recipe_TD_DimensionalDepot")
def ist_aktion(r):
    return r.startswith(TD + "/") and r.rsplit("/", 1)[1] not in GEBAEUDE_REZEPTE
# 0.83.1 (Marius 07.11.: "was brauche ich fuer die Herstellung der Munition, wo wird das gebaut und welche Forschung brauche ich
# vorher - nicht transparent genug"): je Knoten bis zu drei Infokarten neben den Rezeptkarten, Text aus den Rezept-Assets selbst
# (Zutaten, Maschine, Dauer) und den Voraussetzungen des Knotens. Deutsche Namen der Spielgegenstaende aus der Spieldoku de.json.
import io as _io, json as _json
_DOKU = r"C:\Program Files (x86)\Steam\steamapps\common\Satisfactory\CommunityResources\Docs\de.json"
DE_NAMEN = {}
try:
    for _g in _json.load(_io.open(_DOKU, encoding="utf-16")):
        for _c in _g.get("Classes", []):
            if _c.get("mDisplayName"):
                DE_NAMEN[_c["ClassName"]] = _c["mDisplayName"]
except Exception as _e:
    log(f"WARNUNG Spieldoku nicht lesbar: {_e}")
MASCHINE_NAME = {"Build_AssemblerMk1_C": "Fabrikator (Assembler)", "BP_BuildGun_C": "Baupistole", "BP_WorkBenchComponent_C": "Werkbank"}

def name_von(cls):
    if cls is None:
        return "?"
    n = cls.get_name()
    if n in MASCHINE_NAME:
        return MASCHINE_NAME[n]
    if DE_NAMEN.get(n):
        return DE_NAMEN[n]
    try:
        t = str(unreal.get_default_object(cls).get_editor_property("mDisplayName"))
        if t:
            return t
    except Exception:
        pass
    return n.replace("Desc_", "").replace("Build_", "").replace("_C", "")

def rezept_daten(pfad):
    cls = unreal.load_class(None, item(pfad))
    if cls is None:
        return None
    d = unreal.get_default_object(cls)
    menge = lambda liste: [(i.get_editor_property("item_class"), i.get_editor_property("amount")) for i in liste]
    return {"zutaten": menge(d.get_editor_property("mIngredients")), "produkte": menge(d.get_editor_property("mProduct")),
            "maschinen": [m for m in d.get_editor_property("mProducedIn") if m and "WorkBench" not in m.get_name() and "WorkBench" not in str(m)],
            "dauer": d.get_editor_property("mManufactoringDuration")}

def ist_fluessig(cls):
    try:
        form = str(unreal.get_default_object(cls).get_editor_property("mForm")).upper()
        return "LIQUID" in form or "GAS" in form
    except Exception:
        return False

def menge(c, n):
    # Fluessigkeiten stehen in den Rezepten in Litern (1000 = 1 m3), das Spiel zeigt m3
    return f"{n / 1000:g}".replace(".", ",") + f" m³ {name_von(c)}" if ist_fluessig(c) else f"{n} {name_von(c)}"

def menge_text(liste):
    return " + ".join(menge(c, n) for c, n in liste)

ORT = {"Build_OilRefinery_C": "in der Raffinerie", "Build_FoundryMk1_C": "in der Gießerei", "Build_Packager_C": "in der Abfüllanlage",
       "Build_Blender_C": "im Mischer", "Build_SmelterMk1_C": "im Schmelzofen", "Build_ConstructorMk1_C": "im Konstruktor",
       "Build_AssemblerMk1_C": "im Fabrikator (Assembler)", "Build_ManufacturerMk1_C": "im Hersteller"}
def ort(cls):
    return ORT.get(cls.get_name(), "im " + name_von(cls)) if cls else "?"

# Wer stellt welchen Gegenstand her (alle Rezepte der Baeume), fuer die Zutaten eine Stufe tiefer
HERSTELLER = {}
for _b in BAEUME:
    for _k in _b[4]:
        for _r in _k[7]:
            if ist_aktion(_r):
                continue
            _d = rezept_daten(_r)
            if _d and _d["maschinen"] and _d["maschinen"][0].get_name() != "BP_BuildGun_C":
                for _c, _n in _d["produkte"]:
                    HERSTELLER.setdefault(_c.get_name(), (_r, _d, _k[0]))

def herstellungs_karten(b_key, k_key, k_name, rezepte, eltern, icon):
    """Infokarten (Name, Text, Icon-Item) fuer Herstellung, Bauen und Vorher erforschen"""
    karten = []
    aktionen = [rezept_daten(r) for r in rezepte if ist_aktion(r)]
    munition = []   # (Munitions-Klasse, Turmname)
    for a in aktionen:
        if a:
            for c, n in a["zutaten"]:
                munition.append((c, name_von(a["maschinen"][0]) if a["maschinen"] else "Turm"))
    for c, turm in munition:
        h = HERSTELLER.get(c.get_name())
        if not h:
            continue
        r, d, von = h
        m = d["maschinen"][0] if d["maschinen"] else None
        prod = sum(n for cc, n in d["produkte"] if cc.get_name() == c.get_name())
        pro_min = prod * 60.0 / d["dauer"] if d["dauer"] else 0
        einheit = f"{pro_min / 1000:g}".replace(".", ",") + " m³" if ist_fluessig(c) else f"{pro_min:.0f}"
        zeilen = [f"{menge_text(d['zutaten'])} ergibt {menge(c, prod)} in {d['dauer']:.0f} s ({einheit} pro Minute) {ort(m)}."]
        for zc, zn in d["zutaten"]:
            zh = HERSTELLER.get(zc.get_name())
            if zh and zh[0] != r:
                zm = zh[1]["maschinen"][0] if zh[1]["maschinen"] else None
                zeilen.append(f"{name_von(zc)}: {menge_text(zh[1]['zutaten'])} {ort(zm)} (Forschung {NAMEN.get(_knoten_baum(zh[2]), zh[2])}).")
        zeilen.append(f"Per Förderband oder Rohr in den {turm}.")
        karten.append((f"Herstellung: {name_von(m) if m else '?'}", "\n".join(zeilen), icon))
    for r in rezepte:
        if ist_aktion(r) or r.rsplit("/", 1)[1] not in GEBAEUDE_REZEPTE:
            continue
        d = rezept_daten(r)
        if d:
            karten.append(("Bauen: " + ", ".join(f"{n} {name_von(c)}" for c, n in d["zutaten"]),
                           f"Baukosten {menge_text(d['zutaten'])} (Baupistole), oder fertig im Laden am Kern.", icon))
    vor = braucht(b_key, k_key, eltern)
    welle = eff_welle(b_key, k_key)
    if vor or welle:
        namen = [NAMEN[(vb, vk)] for vb, vk in vor]
        titel = ("Vorher: " + ", ".join(namen)) if namen else f"Ab Welle {welle}"
        text = [f"{TITEL[vb]} > {NAMEN[(vb, vk)]}" for vb, vk in vor] + ([f"Erforschbar ab Welle {welle}."] if welle else [])
        icon_vor = next((kk[8] for bb in BAEUME for kk in bb[4] if vor and kk[0] == vor[0][1]), icon)
        karten.append((titel, "\n".join(text), icon_vor))
    # 0.84.1 Turmkombinationen (Konzept Abschnitt F): auf beiden beteiligten Knoten eine Karte "Kombination: X mit Y".
    # Erst einschalten, wenn der Modus Kombo.Aktiv Standard wird - sonst beschreibt die Karte etwas, das nicht wirkt.
    if MIT_KOMBO_KARTEN:
        for name, knoten_paar, partner, text in KOMBOS:
            if k_key in knoten_paar:
                karten.append((f"Kombination: {name} mit {partner[k_key]}", text, icon))
    return karten

MIT_KOMBO_KARTEN = True
KOMBOS = [
    ("Verpuffung", ("GasNobelisk", "Flammenwerfer"), {"GasNobelisk": "Flammenwerfer", "Flammenwerfer": "Gas-Nobelisk"},
     "Trifft die Flamme eine Gaswolke, entzündet sie sich: Das restliche Gas verpufft auf einen Schlag als Explosion (anderthalbfacher Gasschaden), die Wolke ist weg. Erkennbar an der Stichflamme."),
    ("Blitzableiter", ("Teslaspule",), {"Teslaspule": "Bolzenturm"},
     "Bolzen stecken einige Sekunden im Gegner. Der Tesla-Blitz springt bevorzugt auf solche Gegner, ungeschwächt und einmal weiter. Erkennbar am steckenden Bolzen und blauen Funken."),
    ("Hitzeriss", ("Flammenwerfer",), {"Flammenwerfer": "Bolzen- und Gewehrturm"},
     "Brennender Panzer wird spröde: Kugeln und Bolzen ignorieren solange die Panzerung (z. B. der Hogs). Erkennbar am orangen Funken."),
]

def _knoten_baum(k_key):
    return next(((bb[0], k_key) for bb in BAEUME for kk in bb[4] if kk[0] == k_key), None)

STILL = []   # (Schematic-Pfad, Rezept-Pfad)
NACHHOLEN = []   # (Asset, CDO, Knoten, Voraussetzungen), deren Voraussetzung erst in einem spaeteren Baum entsteht
liste_baeume, liste_schemata = [], []
for b_key, b_name, b_text, b_icon, knoten in BAEUME:
    try:
        # 1) Schematics
        schemata = {}
        for k_key, k_name, k_text, xy, eltern, k_kosten, zeit, rezepte, icon in knoten:
            pfad = f"{ZIEL}/{ORDNER[k_key]}/Research_TD_{k_key}"
            s = EAL.duplicate_asset(SCHEMA_VORLAGE, pfad)
            if s is None:
                raise RuntimeError(f"{pfad}: Duplizieren fehlgeschlagen (alte Datei geloescht?)")
            unreal.BlueprintEditorLibrary.compile_blueprint(s)
            c = unreal.get_default_object(s.generated_class())
            c.set_editor_property("mDisplayName", k_name)
            vor = VORAUSSETZUNGEN.get((b_key, k_key), [])
            c.set_editor_property("mDescription", beschreibung(b_key, k_key, k_text, eltern))
            c.set_editor_property("mCost", kosten([(FP, preis(b_key, k_key))]))
            c.set_editor_property("mTimeToComplete", zeit)
            klassen = []
            for r in rezepte:
                if ist_aktion(r):
                    STILL.append((f"{pfad}.Research_TD_{k_key}_C", item(r)))
                    continue
                cls = unreal.load_class(None, item(r))
                if cls is None:
                    log(f"  WARNUNG Rezept fehlt: {r}")
                else:
                    klassen.append(cls)
            # Voraussetzung aus einem anderen Baum als Abhaengigkeit des Spiels: Knoten bleibt verborgen/gesperrt, bis sie erforscht ist
            # (Marius 02.11.: Knoten erst zeigen, wenn sie sinnvoll erforscht werden koennen). Die Baeume werden in Reihenfolge
            # erzeugt, Produktion zuerst - die noetigen Schematics existieren also schon.
            abhs = []
            if vor:
                noetig = [unreal.load_class(None, f"{ZIEL}/{ORDNER[vk]}/Research_TD_{vk}.Research_TD_{vk}_C") for vb, vk in vor]
                if None in noetig:
                    # 0.83: Voraussetzung in einem spaeter erzeugten Baum (Singularitaet <- Festung/Turmfertigung4, Festung braucht
                    # aber Ballistik fuer die Doktrinen) - nach allen Baeumen nachholen
                    NACHHOLEN.append((s, c, k_key, vor))
                    log(f"  Voraussetzung fuer {k_key} noch nicht da, wird nachgeholt: {vor}")
                else:
                    abh = unreal.new_object(unreal.load_class(None, "/Game/FactoryGame/AvailabilityDependencies/BP_SchematicPurchasedDependency.BP_SchematicPurchasedDependency_C"), outer=c)
                    abh.set_editor_property("mSchematics", noetig)
                    abh.set_editor_property("mRequireAllSchematicsToBePurchased", True)
                    abhs.append(abh)
                    # Knoten aus einem anderen Baum: verborgen, bis alles erfuellt ist (dann auch die Welle)
                    c.set_editor_property("mHiddenUntilDependenciesMet", True)
                    log(f"  Abhaengigkeit {k_key} <- {[v[1] for v in vor]}")
            # Freigabe je Phase und Abzweigung als eigene Abhaengigkeiten: das MAM zeigt den Knoten gesperrt (Test 0.53)
            if (b_key, k_key) in AB_WELLE:
                abh = unreal.new_object(unreal.load_class(None, "/Script/FicsitTD.TDWellenAbhaengigkeit"), outer=c)
                abh.set_editor_property("mSchluessel", k_key)
                abh.set_editor_property("mWelle", AB_WELLE[(b_key, k_key)])
                abhs.append(abh)
                log(f"  ab Welle {AB_WELLE[(b_key, k_key)]}: {k_key}")
            for andere in andere_von(b_key, k_key):
                if True:
                    abh = unreal.new_object(unreal.load_class(None, "/Script/FicsitTD.TDAbzweigungAbhaengigkeit"), outer=c)
                    abh.set_editor_property("mAndereSeite", unreal.SoftClassPath(f"{ZIEL}/{ORDNER[andere]}/Research_TD_{andere}.Research_TD_{andere}_C"))
                    abhs.append(abh)
                    log(f"  Abzweigung {k_key} <-> {andere}")
            if abhs:
                c.set_editor_property("mSchematicDependencies", abhs)
            freischaltung = c.get_editor_property("mUnlocks")[0]
            freischaltung.set_editor_property("mRecipes", klassen)
            # 0.65 (Marius 04.11.: im MAM steht nicht, was ein Knoten bringt und ob er freischaltbar ist): Infokarten (BP_UnlockInfoOnly).
            # Karte 1 = Status, der Mod schreibt sie zur Laufzeit neu (TDForschung::AktualisiereMamKarten); Karte 2 = was der Knoten bringt.
            textur = icon_von(icon)
            # Nur Knoten ohne sichtbares Rezept (reine Wirkung wie Kern-Panzerung) bekommen eine Infokarte mit der Wirkung
            karten = []
            if not klassen:
                karte = unreal.new_object(INFO_KLASSE, outer=c)
                karte.set_editor_property("mUnlockName", KURZ.get(k_key, k_name))
                karte.set_editor_property("mUnlockDescription", k_text)
                karte.set_editor_property("mUnlockIconBig", textur)
                karte.set_editor_property("mUnlockIconSmall", textur)
                karten.append(karte)
            for t_name, t_text, t_icon in herstellungs_karten(b_key, k_key, k_name, rezepte, eltern, icon):
                karte = unreal.new_object(INFO_KLASSE, outer=c)
                karte.set_editor_property("mUnlockName", t_name)
                karte.set_editor_property("mUnlockDescription", t_text)
                karte.set_editor_property("mUnlockIconBig", icon_von(t_icon))
                karte.set_editor_property("mUnlockIconSmall", icon_von(t_icon))
                karten.append(karte)
                log(f"  Karte {k_key}: {t_name} | {t_text}")
            c.set_editor_property("mUnlocks", ([freischaltung] if klassen else []) + karten)
            pinsel = c.get_editor_property("mSchematicIcon")
            pinsel.set_editor_property("resource_object", icon_von(icon))
            c.set_editor_property("mSchematicIcon", pinsel)
            unreal.BlueprintEditorLibrary.compile_blueprint(s)
            EAL.save_loaded_asset(s, False)
            schemata[k_key] = (f"{pfad}.Research_TD_{k_key}_C", xy)
            liste_schemata.append(f"{pfad}.Research_TD_{k_key}_C")
            log(f"{pfad}: {k_name}, {len(klassen)} Rezepte")
        # 2) Baum mit Knoten
        b_pfad = f"{ZIEL}/BPD_ResearchTree_TD_{b_key}"
        t = EAL.duplicate_asset(BAUM_VORLAGE, b_pfad)
        if t is None:
            raise RuntimeError(f"{b_pfad}: Duplizieren fehlgeschlagen (alte Datei geloescht?)")
        unreal.BlueprintEditorLibrary.compile_blueprint(t)
        cdo = unreal.get_default_object(t.generated_class())
        cdo.set_editor_property("mDisplayName", b_name)
        cdo.set_editor_property("mPreUnlockDisplayName", b_name)
        cdo.set_editor_property("mPreUnlockDescription", b_text)
        cdo.set_editor_property("mPostUnlockDescription", b_text)
        # Sichtbar ab Start, ausser der Baum braucht erst einen Knoten aus einem anderen Baum (Marius 02.11.: Ballistik nach Assembler)
        baum_vor = BAUM_VORAUSSETZUNG.get(b_key, [])
        if baum_vor:
            abh = unreal.new_object(unreal.load_class(None, "/Game/FactoryGame/AvailabilityDependencies/BP_SchematicPurchasedDependency.BP_SchematicPurchasedDependency_C"), outer=cdo)
            abh.set_editor_property("mSchematics", [unreal.load_class(None, f"{ZIEL}/{ORDNER[vk]}/Research_TD_{vk}.Research_TD_{vk}_C") for vb, vk in baum_vor])
            abh.set_editor_property("mRequireAllSchematicsToBePurchased", True)
            cdo.set_editor_property("mUnlockDependencies", [abh])
            log(f"  Baum {b_key} erscheint nach {baum_vor}")
        else:
            cdo.set_editor_property("mUnlockDependencies", [])
        baum_pinsel = cdo.get_editor_property("mResearchTreeIcon")
        baum_pinsel.set_editor_property("resource_object", icon_von(b_icon))
        cdo.set_editor_property("mResearchTreeIcon", baum_pinsel)
        knoten_klasse = unreal.load_class(None, KNOTEN_KLASSE)
        neue = []
        for k_key, k_name, k_text, xy, eltern, k_kosten, zeit, rezepte, icon in knoten:
            kinder = [kk[3] for kk in knoten if k_key in kk[4]]
            n = unreal.new_object(knoten_klasse, outer=cdo)
            daten = n.get_editor_property("mNodeDataStruct")
            text = knoten_text(schemata[k_key][0], xy, [schemata[e][1] for e in eltern], kinder)
            daten.import_text(text)
            n.set_editor_property("mNodeDataStruct", daten)
            neue.append(n)
        cdo.set_editor_property("mNodes", neue)
        unreal.BlueprintEditorLibrary.compile_blueprint(t)
        EAL.save_loaded_asset(t, False)
        liste_baeume.append(f"{b_pfad}.BPD_ResearchTree_TD_{b_key}_C")
        log(f"{b_pfad}: {len(neue)} Knoten")
    except Exception as e:
        log(f"FEHLER Baum {b_key}: {e}")

for s, c, k_key, vor in NACHHOLEN:
    noetig = [unreal.load_class(None, f"{ZIEL}/{ORDNER[vk]}/Research_TD_{vk}.Research_TD_{vk}_C") for vb, vk in vor]
    if None in noetig:
        log(f"  WARNUNG Voraussetzung fehlt fuer {k_key}: {vor}")
        continue
    abh = unreal.new_object(unreal.load_class(None, "/Game/FactoryGame/AvailabilityDependencies/BP_SchematicPurchasedDependency.BP_SchematicPurchasedDependency_C"), outer=c)
    abh.set_editor_property("mSchematics", noetig)
    abh.set_editor_property("mRequireAllSchematicsToBePurchased", True)
    c.set_editor_property("mSchematicDependencies", list(c.get_editor_property("mSchematicDependencies")) + [abh])
    c.set_editor_property("mHiddenUntilDependenciesMet", True)
    unreal.BlueprintEditorLibrary.compile_blueprint(s)
    EAL.save_loaded_asset(s, False)
    log(f"  Abhaengigkeit {k_key} <- {[v[1] for v in vor]} (nachgeholt)")

# Pfadliste fuer TDWorldModule (Registrierung) und das Einsatz-Subsystem (Abzweigungen)
inl = ["// Erzeugt von Scripts/create_research.py - nicht von Hand bearbeiten", "static const TCHAR* TDForschungsBaeume[] = {"]
inl += [f'\tTEXT("{p}"),' for p in liste_baeume] + ["};", "static const TCHAR* TDForschungsSchemata[] = {"]
inl += [f'\tTEXT("{p}"),' for p in liste_schemata] + ["};"]
with open(r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Source\FicsitTD\Private\TDForschungListe.inl", "w", encoding="utf-8") as f:
    f.write("\n".join(inl) + "\n")
def schema_pfad(bk, kk):
    return f"{ZIEL}/{ORDNER[kk]}/Research_TD_{kk}.Research_TD_{kk}_C"
vor_inl = ["// Erzeugt von Scripts/create_research.py - nicht von Hand bearbeiten",
           "// Voraussetzungen aus anderen MAM-Baeumen: Knoten -> noetiger Knoten (FicsitTDModule.cpp sperrt die Forschung)",
           "static const TPair<const TCHAR*, const TCHAR*> TDForschungVoraussetzungen[] = {"]
for (bk, kk), liste in VORAUSSETZUNGEN.items():
    for vb, vk in liste:
        vor_inl.append(f'\t{{ TEXT("{schema_pfad(bk, kk)}"), TEXT("{schema_pfad(vb, vk)}") }},')
vor_inl.append("};")
vor_inl += ["// Freigabe je Phase: Knoten -> (Schluessel fuer Forschung.<Schluessel>.AbWelle, Standardwelle)",
            "struct FTDForschungAbWelle { const TCHAR* Pfad; const TCHAR* Schluessel; int32 Welle; };",
            "static const FTDForschungAbWelle TDForschungAbWelle[] = {"]
for (bk, kk), w in AB_WELLE.items():
    vor_inl.append(f'\t{{ TEXT("{schema_pfad(bk, kk)}"), TEXT("{kk}"), {w} }},')
vor_inl.append("};")
vor_inl += ["// Turm-Aktionen, die der Mod mit dem Knoten freischaltet (nicht im MAM sichtbar): Knoten -> Rezept",
            "static const TPair<const TCHAR*, const TCHAR*> TDForschungStilleRezepte[] = {"]
for sp, rp in STILL:
    vor_inl.append(f'\t{{ TEXT("{sp}"), TEXT("{rp}") }},')
vor_inl.append("};")
vor_inl += ["// Gabelungen (nur eine Seite je Einsatz)", "static const TPair<const TCHAR*, const TCHAR*> TDForschungAbzweigungen[] = {"]
for g_b, glieder in GRUPPEN:
    for i, links in enumerate(glieder):
        for rechts in glieder[i + 1:]:
            vor_inl.append(f'\t{{ TEXT("{schema_pfad(g_b, links)}"), TEXT("{schema_pfad(g_b, rechts)}") }},')
vor_inl.append("};")
with open(r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Source\FicsitTD\Private\TDForschungVoraussetzungen.inl", "w", encoding="utf-8") as f:
    f.write("\n".join(vor_inl) + "\n")
def c_str(t):
    return t.replace("\\", "\\\\").replace('"', '\\"').replace("\n", " ")
info = ["// Erzeugt von Scripts/create_research.py - nicht von Hand bearbeiten",
        "// Alle Knoten fuer die Forschungsuebersicht im Kern-Menue (TDForschung.cpp): Baum, Baumname, Schluessel, Name, Text, Laden, Gut gegen,",
        "// FP, Standard-Welle (0 = sofort), Voraussetzungen (Schluessel, Komma), andere Seite der Abzweigung, Schematic-Pfad",
        "struct FTDForschungInfoRoh { const TCHAR* Baum; const TCHAR* BaumName; const TCHAR* Key; const TCHAR* Name; const TCHAR* Text; const TCHAR* Laden;",
        "\tconst TCHAR* Gegen; int32 FP; int32 AbWelle; const TCHAR* Braucht; const TCHAR* Andere; const TCHAR* Pfad; };",
        "static const FTDForschungInfoRoh TDForschungInfoRoh[] = {"]
for b_key, b_name, b_text, b_icon, knoten in BAEUME:
    for k_key, k_name, k_text, xy, eltern, k_kosten, zeit, rezepte, icon in knoten:
        fp = preis(b_key, k_key)
        andere = ""
        andere = ",".join(andere_von(b_key, k_key))
        vor = ",".join(vk for vb, vk in braucht(b_key, k_key, eltern))
        info.append(f'\t{{ TEXT("{b_key}"), TEXT("{c_str(TITEL[b_key])}"), TEXT("{k_key}"), TEXT("{c_str(k_name)}"), TEXT("{c_str(k_text)}"), '
                    f'TEXT("{c_str(LADEN.get(k_key, ""))}"), TEXT("{c_str(GEGEN.get(k_key, ""))}"), {fp}, {eff_welle(b_key, k_key)}, '
                    f'TEXT("{vor}"), TEXT("{andere}"), TEXT("{schema_pfad(b_key, k_key)}") }},')
info.append("};")
with open(r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Source\FicsitTD\Private\TDForschungInfo.inl", "w", encoding="utf-8-sig") as f:
    f.write("\n".join(info) + "\n")
log(f"Forschungsinfo: {len(info) - 6} Knoten")
log(f"Voraussetzungen: {sum(len(v) for v in VORAUSSETZUNGEN.values())}")
log("ENDE")
with open(r"E:\Modding\Logs\create_research.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
