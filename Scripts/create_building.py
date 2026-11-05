# Erzeugt eigene Gebaeude als Kopie eines Vanilla-Gebaeudes (Build/Desc/Recipe), haengt die Gebaeude-Blueprint auf
# eine eigene C++-Klasse um und kappt die Verknuepfungen, mit denen die Kopie das Original verdraengen wuerde.
# Aufruf ueber UnrealEditor-Cmd -run=pythonscript; welche Gebaeude erzeugt werden, steht unten in GEBAEUDE.
# Vorher die alten .uasset im Zielordner per Dateisystem loeschen (delete_asset greift im Commandlet nicht zuverlaessig).
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
