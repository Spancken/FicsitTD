# Liest die Vanilla-Mauer-Assets (Platzhalter im Starter Project) aus und schreibt die relevanten Werte ins Log.
import unreal

OUT = []

def log(msg):
    OUT.append(str(msg))
    unreal.log("FICSITTD| " + str(msg))

def dump_cdo(path, props):
    bp = unreal.load_asset(path)
    if bp is None:
        log(f"{path}: NICHT GEFUNDEN")
        return
    gen = bp.generated_class() if hasattr(bp, "generated_class") else None
    log(f"{path}: Asset {bp.get_class().get_name()}, GeneratedClass {gen.get_name() if gen else None}")
    if gen is None:
        return
    cdo = unreal.get_default_object(gen)
    log(f"  CDO-Klasse {cdo.get_class().get_name()}, Elternklassen:")
    cls = gen
    for _ in range(8):
        try:
            cls = unreal.SystemLibrary.get_super_class(cls) if hasattr(unreal.SystemLibrary, "get_super_class") else None
        except Exception:
            cls = None
        if cls is None:
            break
        log(f"    -> {cls.get_name()}")
    for p in props:
        try:
            log(f"  {p} = {cdo.get_editor_property(p)}")
        except Exception as e:
            log(f"  {p} = <Fehler: {e}>")

dump_cdo("/Game/FactoryGame/Buildable/Building/Wall/Build_Wall_8x4_01",
         ["mDisplayName", "mHologramClass", "mDefaultSwatchCustomizationOverride", "mSwatchGroup",
          "mAllowColoring", "mManagedByLightweightBuildableSubsystem"])
dump_cdo("/Game/FactoryGame/Buildable/Building/Wall/Desc_Wall_8x4_01",
         ["mDisplayName", "mDescription", "mBuildableClass", "mBuildMenuCategory", "mSubCategories", "mMenuPriority",
          "mSmallIcon", "mPersistentBigIcon"])
dump_cdo("/Game/FactoryGame/Recipes/Buildings/Walls/Recipe_Wall_8x4_01",
         ["mDisplayName", "mIngredients", "mProduct", "mProducedIn", "mManufactoringDuration"])

# Komponenten der Mauer-Blueprint
bp = unreal.load_asset("/Game/FactoryGame/Buildable/Building/Wall/Build_Wall_8x4_01")
if bp:
    try:
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
        for h in handles:
            data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
            obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
            if obj:
                extra = ""
                if isinstance(obj, unreal.StaticMeshComponent):
                    extra = f" Mesh={obj.get_editor_property('static_mesh')}"
                log(f"  Komponente {obj.get_name()} ({obj.get_class().get_name()}){extra}")
    except Exception as e:
        log(f"  Komponenten: Fehler {e}")

# Swatch- und Material-Kandidaten fuer die Unterscheidungsfarbe
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for path in ["/Game/FactoryGame/Buildable/-Shared/Customization"]:
    assets = ar.get_assets_by_path(path, recursive=True)
    names = sorted(str(a.asset_name) for a in assets if "Swatch" in str(a.asset_name) or "Material" in str(a.asset_name) or "Caution" in str(a.asset_name))
    log(f"Customization-Assets ({len(names)}): " + ", ".join(names[:120]))

with open(r"E:\Modding\Logs\inspect_wall.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(OUT))
