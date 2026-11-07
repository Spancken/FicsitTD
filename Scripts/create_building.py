# Erzeugt eigene Gebaeude als Kopie eines Vanilla-Gebaeudes (Build/Desc/Recipe), haengt die Gebaeude-Blueprint auf
# eine eigene C++-Klasse um und kappt die Verknuepfungen, mit denen die Kopie das Original verdraengen wuerde.
# Aufruf ueber UnrealEditor-Cmd -run=pythonscript; welche Gebaeude erzeugt werden, steht unten in GEBAEUDE.
# Vorher die alten .uasset im Zielordner per Dateisystem loeschen (delete_asset greift im Commandlet nicht zuverlaessig).
import os
import unreal

LOG = []
def log(m):
    LOG.append(str(m))
    unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary

def item(path, amount):
    cls = unreal.load_class(None, path)
    if cls is None:
        raise RuntimeError(f"Item nicht gefunden: {path}")
    ia = unreal.ItemAmount()
    ia.set_editor_property("item_class", cls)
    ia.set_editor_property("amount", amount)
    return ia

GEBAEUDE = [
    dict(
        name="DefenseWall",
        cpp="/Script/FicsitTD.TDDefenseWall",
        src_build="/Game/FactoryGame/Buildable/Building/Wall/Build_Wall_8x4_01",
        src_desc="/Game/FactoryGame/Buildable/Building/Wall/Desc_Wall_8x4_01",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Walls/Recipe_Wall_8x4_01",
        anzeige="Verteidigungsmauer",
        text="Mauer mit Lebenspunkten. Gegnerwellen greifen sie an; bei 0 Lebenspunkten wird sie zerstoert. Nicht durchlaufbar.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement.Desc_Cement_C", 6),
                 ("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C", 4)],
    ),
    dict(
        name="BoltTurret",
        cpp="/Script/FicsitTD.TDBoltTurret",
        # Produktionsmaschine als Basis: Maschinen-Menue, Standby, Uebertaktung, Betriebsleuchte, Stromanschluss
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        anzeige="Bolzenturm",
        entferne=["FGVertexAnimatedMeshComponent"],
        text="Verteidigungsturm. Verschiesst alle Bolzenarten (Rezept waehlen), braucht Strom. Wird mit jedem Abschuss erfahrener: mehr Schaden, Reichweite, Feuerrate und Lebenspunkte.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C", 10),
                 ("/Game/FactoryGame/Resource/Parts/IronRod/Desc_IronRod.Desc_IronRod_C", 10)],
    ),    dict(
        name="GunTurret",
        cpp="/Script/FicsitTD.TDGunTurret",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Gewehrturm",
        text="Schneller Verteidigungsturm gegen Schwaerme. Verschiesst Gewehrmunition (1 Stueck = 1 Magazin), braucht Strom. Laeufe drehen beim Feuern hoch.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C", 5),
                 ("/Game/FactoryGame/Resource/Parts/Rotor/Desc_Rotor.Desc_Rotor_C", 2),
                 ("/Game/FactoryGame/Resource/Parts/IronRod/Desc_IronRod.Desc_IronRod_C", 10)],
    ),
    dict(
        name="TeslaCoil",
        cpp="/Script/FicsitTD.TDTeslaCoil",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Teslaspule",
        text="Laedt sich mit viel Strom auf und entlaedt sich in einem Blitz, der auf weitere Gegner ueberspringt. Braucht keine Munition.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/Wire/Desc_Wire.Desc_Wire_C", 60),
                 ("/Game/FactoryGame/Resource/Parts/Cable/Desc_Cable.Desc_Cable_C", 20),
                 ("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C", 5)],
    ),
    dict(
        name="FlameTurret",
        cpp="/Script/FicsitTD.TDFlameTurret",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Flammenwerfer",
        text="Kurze Reichweite, trifft alle Gegner im Flammenkegel und setzt sie in Brand. Treibstoff per Rohr (Anschluss hinten), braucht Strom.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C", 6),
                 ("/Game/FactoryGame/Resource/Parts/ModularFrame/Desc_ModularFrame.Desc_ModularFrame_C", 2),
                 ("/Game/FactoryGame/Resource/Parts/Rotor/Desc_Rotor.Desc_Rotor_C", 2)],
    ),
    dict(
        name="NobeliskLauncher",
        cpp="/Script/FicsitTD.TDNobeliskLauncher",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Nobelisk-Werfer",
        text="Schiesst Nobelisken im hohen Bogen: grosse Reichweite, Flaechenschaden, Mindestabstand 10 m. Alle Nobelisk-Arten (Rezept waehlen), braucht Strom.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/SteelPlate/Desc_SteelPlate.Desc_SteelPlate_C", 10),
                 ("/Game/FactoryGame/Resource/Parts/Motor/Desc_Motor.Desc_Motor_C", 2),
                 ("/Game/FactoryGame/Resource/Parts/ModularFrame/Desc_ModularFrame.Desc_ModularFrame_C", 4)],
    ),
    dict(
        name="RepairStation",
        cpp="/Script/FicsitTD.TDRepairStation",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Reparaturstation",
        text="Repariert mit Strom und Material (Rezept waehlen) beschaedigte Mauern, Sperren, Tuerme und den Abwehrkern im Umkreis von 25 m. Laeuft nur, wenn es etwas zu reparieren gibt.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C", 5),
                 ("/Game/FactoryGame/Resource/Parts/Rotor/Desc_Rotor.Desc_Rotor_C", 2),
                 ("/Game/FactoryGame/Resource/Parts/Wire/Desc_Wire.Desc_Wire_C", 20)],
    ),
    dict(
        name="Barrier",
        cpp="/Script/FicsitTD.TDBarrier",
        src_build="/Game/FactoryGame/Buildable/Factory/LookoutTower/Build_LookoutTower",
        src_desc="/Game/FactoryGame/Buildable/Factory/LookoutTower/Desc_LookoutTower",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_LookoutTower",
        anzeige="Panzersperre",
        text="Passive Sperre aus Stahltraegern (2 x 2 m): Gegnerwellen muessen sie erst zerstoeren. Macht selbst keinen Schaden.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/SteelPlate/Desc_SteelPlate.Desc_SteelPlate_C", 4),
                 ("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement.Desc_Cement_C", 6)],
    ),
    dict(
        name="DefenseCore",
        cpp="/Script/FicsitTD.TDDefenseCore",
        # Aussichtsturm: einfaches Gebaeude ohne Maschinenlogik (Proxy-Modell, Standbeine, Leiter)
        src_build="/Game/FactoryGame/Buildable/Factory/LookoutTower/Build_LookoutTower",
        src_desc="/Game/FactoryGame/Buildable/Factory/LookoutTower/Desc_LookoutTower",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_LookoutTower",
        anzeige="Abwehrkern",
        text="Herz der Kern-Verteidigung: Gegnerwellen marschieren hierher. Faellt der Kern, ist der Lauf verloren. Start per Chat: /td kern start",
        zutaten=[("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C", 10),
                 ("/Game/FactoryGame/Resource/Parts/Wire/Desc_Wire.Desc_Wire_C", 30),
                 ("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement.Desc_Cement_C", 20)],
    ),
]

GEBAEUDE += [
    # 0.81 passive Bauten P1 (Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07, Modelle aus Blender: td_tor.py, td_lockfeuer.py, td_oelschlick.py)
    dict(
        name="Gate",
        cpp="/Script/FicsitTD.TDGate",
        src_build="/Game/FactoryGame/Buildable/Factory/LookoutTower/Build_LookoutTower",
        src_desc="/Game/FactoryGame/Buildable/Factory/LookoutTower/Desc_LookoutTower",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_LookoutTower",
        anzeige="Tor",
        text="Tor fuer die Mauerlinie (8 m breit, Oeffnung 4 x 4 m). Oeffnet sich fuer Spieler und Fahrzeuge, Gegner kommen nie durch und greifen es wie eine Mauer an. Ein Foerderband passt unten durch den Schlitz.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement.Desc_Cement_C", 10),
                 ("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C", 4),
                 ("/Game/FactoryGame/Resource/Parts/Rotor/Desc_Rotor.Desc_Rotor_C", 2)],
    ),
    dict(
        name="LureBeacon",
        cpp="/Script/FicsitTD.TDLureBeacon",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Lockfeuer",
        text="Verbrennt Kohle (Band-Eingang) und lockt dabei 15 s lang alle Gegnerwellen in 40 m an: Sie greifen das Lockfeuer an statt ihren Weg zu laufen. Danach 30 s Pause. Braucht Strom, macht keinen Schaden.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C", 4),
                 ("/Game/FactoryGame/Resource/Parts/Wire/Desc_Wire.Desc_Wire_C", 20),
                 ("/Game/FactoryGame/Resource/Parts/IronRod/Desc_IronRod.Desc_IronRod_C", 10)],
    ),
    dict(
        name="SlickPlate",
        cpp="/Script/FicsitTD.TDSlickPlate",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Oelschlick-Feld",
        text="Bodenplatte 8 x 8 m, per Rohr mit Schweroelrueckstand aus der Raffinerie versorgt. Gegner darauf laufen um 35 % langsamer; trifft ein Flammenwerfer einen Gegner auf der Platte, brennt sie 5 s. Braucht Strom.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/SteelPlate/Desc_SteelPlate.Desc_SteelPlate_C", 6),
                 ("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement.Desc_Cement_C", 10),
                 ("/Game/FactoryGame/Resource/Parts/SteelPipe/Desc_SteelPipe.Desc_SteelPipe_C", 4)],
    ),
]
GEBAEUDE += [
    # 0.83 weitere Bauten P2/P3 (Modelle td_elektrozaun.py ... td_dimensionslager.py)
    dict(
        name="ConductorWall",
        cpp="/Script/FicsitTD.TDConductorWall",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Elektrozaun",
        text="Mauerstueck 8 x 4 m mit Kupferstaeben. Fuer Gegner eine Mauer; wer ihn angreift, bekommt einen Stromschlag (15 Schaden, 1 s Betaeubung). Braucht Strom (2 MW), ohne Strom nur Mauer.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/Cement/Desc_Cement.Desc_Cement_C", 10),
                 ("/Game/FactoryGame/Resource/Parts/CopperSheet/Desc_CopperSheet.Desc_CopperSheet_C", 10),
                 ("/Game/FactoryGame/Resource/Parts/Cable/Desc_Cable.Desc_Cable_C", 20)],
    ),
    dict(
        name="RadioMast",
        cpp="/Script/FicsitTD.TDRadioMast",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Funkmast",
        text="Radarmast mit Leitsender: Tuerme im Umkreis von 30 m erhalten +10 % Reichweite (mehrere Masten wirken nicht zusammen). Braucht dauernd Strom (15 MW).",
        zutaten=[("/Game/FactoryGame/Resource/Parts/SteelPlate/Desc_SteelPlate.Desc_SteelPlate_C", 6),
                 ("/Game/FactoryGame/Resource/Parts/Cable/Desc_Cable.Desc_Cable_C", 30),
                 ("/Game/FactoryGame/Resource/Parts/CrystalOscillator/Desc_CrystalOscillator.Desc_CrystalOscillator_C", 2)],
    ),
    dict(
        name="ShieldGenerator",
        cpp="/Script/FicsitTD.TDShieldGenerator",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Schildgenerator",
        text="Kuppel 20 m: schluckt Schaden an Mauern, Tuermen und Bauten (nicht am Abwehrkern), 3000 Schildpunkte. Gebrochen laedt er 20 s nach. Braucht dauernd Strom (60 MW).",
        zutaten=[("/Game/FactoryGame/Resource/Parts/Battery/Desc_Battery.Desc_Battery_C", 10),
                 ("/Game/FactoryGame/Resource/Parts/AluminumPlateReinforced/Desc_AluminumPlateReinforced.Desc_AluminumPlateReinforced_C", 5),
                 ("/Game/FactoryGame/Resource/Parts/Computer/Desc_Computer.Desc_Computer_C", 2)],
    ),
    dict(
        name="FlakTurret",
        cpp="/Script/FicsitTD.TDFlakTurret",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Himmelsturm",
        text="Flugabwehr: schiesst Flak-Kanister (Band-Eingang) auf fliegende Krabben in 60 m, 200 Schaden in 6 m. Greift keine Bodengegner an. Braucht Strom.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/AluminumCasing/Desc_AluminumCasing.Desc_AluminumCasing_C", 20),
                 ("/Game/FactoryGame/Resource/Parts/ModularFrameHeavy/Desc_ModularFrameHeavy.Desc_ModularFrameHeavy_C", 2),
                 ("/Game/FactoryGame/Resource/Parts/Motor/Desc_Motor.Desc_Motor_C", 4)],
    ),
    dict(
        name="FieldProjector",
        cpp="/Script/FicsitTD.TDFieldProjector",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Stoerfeld-Projektor",
        text="Verbraucht reanimiertes SAM (Band-Eingang, 1 Barren je 60 s), solange Gegner in 15 m sind, und bremst sie alle um 40 %. Braucht Strom.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/SAMFluctuator/Desc_SAMFluctuator.Desc_SAMFluctuator_C", 5),
                 ("/Game/FactoryGame/Resource/Parts/ModularFrameFused/Desc_ModularFrameFused.Desc_ModularFrameFused_C", 2),
                 ("/Game/FactoryGame/Resource/Parts/Computer/Desc_Computer.Desc_Computer_C", 4)],
    ),
    dict(
        name="SingularityLauncher",
        cpp="/Script/FicsitTD.TDSingularityLauncher",
        src_build="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Desc_ConstructorMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1",
        entferne=["FGVertexAnimatedMeshComponent"],
        anzeige="Singularitaetswerfer",
        text="Feuert Ficsonium-Ladungen (Band-Eingang, alle 8 s): Gegner in 12 m werden festgehalten und nach 1,5 s mit 1500 Explosionsschaden gesprengt. Braucht viel Strom.",
        zutaten=[("/Game/FactoryGame/Resource/Parts/SingularityCell/Desc_SingularityCell.Desc_SingularityCell_C", 10),
                 ("/Game/FactoryGame/Resource/Parts/ModularFrameFused/Desc_ModularFrameFused.Desc_ModularFrameFused_C", 4),
                 ("/Game/FactoryGame/Resource/Parts/TimeCrystal/Desc_TimeCrystal.Desc_TimeCrystal_C", 10)],
    ),
    dict(
        name="DimensionalDepot",
        cpp="/Script/FicsitTD.TDDimensionalDepot",
        src_build="/Game/FactoryGame/Buildable/Factory/StorageContainerMk1/Build_StorageContainerMk1",
        src_desc="/Game/FactoryGame/Buildable/Factory/StorageContainerMk1/Desc_StorageContainerMk1",
        src_recipe="/Game/FactoryGame/Recipes/Buildings/Recipe_StorageContainerMk1",
        hologramm="/Script/FactoryGame.FGFactoryHologram",
        anzeige="Dimensionslager",
        text="Lager mit Mercer-Kugel: verteilt seinen Inhalt jede Sekunde an die Tuerme in 30 m, deren Rezept genau diesen Gegenstand verbraucht. Munition ohne Foerderband.",
        zutaten=[("/Game/FactoryGame/Prototype/WAT/Desc_WAT2.Desc_WAT2_C", 1),
                 ("/Game/FactoryGame/Resource/Parts/AluminumPlate/Desc_AluminumPlate.Desc_AluminumPlate_C", 10),
                 ("/Game/FactoryGame/Resource/Parts/Computer/Desc_Computer.Desc_Computer_C", 2)],
    ),
]
# 0.81: nur bestimmte Gebaeude neu anlegen (FICSITTD_NUR=Gate,LureBeacon), die anderen bleiben unberuehrt
_NUR = [n for n in os.environ.get("FICSITTD_NUR", "").split(",") if n]
if _NUR:
    GEBAEUDE = [g for g in GEBAEUDE if g["name"] in _NUR]

def duplicate(src, dst):
    asset = EAL.duplicate_asset(src, dst)
    if asset is None:
        raise RuntimeError(f"Duplizieren fehlgeschlagen: {src} -> {dst} (alte .uasset geloescht?)")
    log(f"dupliziert {src} -> {dst}")
    return asset

def cdo(bp):
    return unreal.get_default_object(bp.generated_class())

def setp(obj, name, value):
    obj.set_editor_property(name, value)
    log(f"  {name} = {obj.get_editor_property(name)}")

def erzeuge(g):
    d_dir = f"/FicsitTD/Buildings/{g['name']}"
    BUILD, DESC, RECIPE = (f"{d_dir}/Build_TD_{g['name']}", f"{d_dir}/Desc_TD_{g['name']}", f"{d_dir}/Recipe_TD_{g['name']}")
    EAL.make_directory(d_dir)

    # 1) Gebaeude
    build = duplicate(g["src_build"], BUILD)
    parent = unreal.load_class(None, g["cpp"])
    if parent is None:
        raise RuntimeError(f"C++-Klasse {g['cpp']} nicht gefunden - Editor neu bauen?")
    src_cdo = unreal.get_default_object(unreal.load_asset(g["src_build"]).generated_class())
    unreal.BlueprintEditorLibrary.reparent_blueprint(build, parent)
    log(f"umgehaengt auf {parent.get_name()}")
    b = cdo(build)
    setp(b, "mManagedByLightweightBuildableSubsystem", False)
    setp(b, "mAlternativeMaterialRecipes", [])
    # Beim Umhaengen geht die Instanz-Datenquelle (Modell) verloren: aus dem Original neu aufbauen
    src_data = src_cdo.get_editor_property("mInstanceDataCDO")
    if src_data is not None:
        new_data = unreal.new_object(type(src_data), outer=b)
        new_data.set_editor_property("instances", src_data.get_editor_property("instances"))
        b.set_editor_property("mInstanceDataCDO", new_data)
        log(f"  mInstanceDataCDO neu mit {len(new_data.get_editor_property('instances'))} Instanz(en)")
    else:
        log("  Original hat keine mInstanceDataCDO (Modell kommt aus Komponenten)")
    unreal.BlueprintEditorLibrary.compile_blueprint(build)
    # Bauteile entfernen, die auf private Objekte der Vorlage verweisen (z. B. Konstruktor-Animation mit Sound-Notifies),
    # sonst laesst sich die Kopie nicht speichern ("Illegal reference to private object")
    if g.get("entferne"):
        sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        lib = unreal.SubobjectDataBlueprintFunctionLibrary
        handles = sub.k2_gather_subobject_data_for_blueprint(build)
        weg = [h for h in handles if (o := lib.get_object(lib.get_data(h))) is not None and o.get_class().get_name() in g["entferne"]]
        if weg:
            log(f"  entfernt: {sub.delete_subobjects(handles[0], weg, build)} Bauteil(e) {g['entferne']}")
        unreal.BlueprintEditorLibrary.compile_blueprint(build)
    b = cdo(build)
    setp(b, "mDisplayName", g["anzeige"])
    setp(b, "mDescription", g["text"])
    # 0.83 Lager-Kopien: Stapel-Hologramm stuerzt ohne Standbeine ab -> Fabrik-Hologramm wie beim Konstruktor
    if g.get("hologramm"):
        setp(b, "mHologramClass", unreal.load_class(None, g["hologramm"]))

    # 2) Beschreibung (Baumenue-Eintrag)
    desc = duplicate(g["src_desc"], DESC)
    unreal.BlueprintEditorLibrary.compile_blueprint(desc)
    d = cdo(desc)
    setp(d, "mBuildableClass", build.generated_class())
    setp(d, "mQuickSwitchGroup", None)
    setp(d, "mDisplayName", g["anzeige"])
    setp(d, "mDescription", g["text"])

    # 3) Rezept
    recipe = duplicate(g["src_recipe"], RECIPE)
    unreal.BlueprintEditorLibrary.compile_blueprint(recipe)
    r = cdo(recipe)
    setp(r, "mMaterialCustomizationRecipe", None)
    setp(r, "mIngredients", [item(p, a) for p, a in g["zutaten"]])
    product = unreal.ItemAmount()
    product.set_editor_property("item_class", desc.generated_class())
    product.set_editor_property("amount", 1)
    setp(r, "mProduct", [product])
    setp(r, "mDisplayName", g["anzeige"])

    for a in (build, desc, recipe):
        unreal.BlueprintEditorLibrary.compile_blueprint(a)
        EAL.save_loaded_asset(a, False)
    log(f"ERFOLG: {g['name']} gespeichert")

for g in GEBAEUDE:
    try:
        erzeuge(g)
    except Exception as e:
        log(f"FEHLER bei {g['name']}: {e}")

with open(r"E:\Modding\Logs\create_building.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
