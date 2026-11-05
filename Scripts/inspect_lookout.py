# Vorlage fuer den Abwehrkern pruefen: Elternklasse, Komponenten, Hologramm, Instanzdaten des Aussichtsturms.
import unreal
LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))
for pfad in ("/Game/FactoryGame/Buildable/Factory/LookoutTower/Build_LookoutTower",):
    bp = unreal.load_asset(pfad)
    cls = bp.generated_class()
    cdo = unreal.get_default_object(cls)
    kette = []
    c = type(cdo)
    log(f"{pfad}: Python-Typ {c.__name__}, Basis {[b.__name__ for b in c.__mro__[:6]]}")
    for f in ("mHologramClass", "mInstanceDataCDO", "mManagedByLightweightBuildableSubsystem", "mDisplayName"):
        try:
            log(f"  {f} = {cdo.get_editor_property(f)}")
        except Exception as e:
            log(f"  {f}: {e}")
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        o = lib.get_object(lib.get_data(h))
        if o is not None:
            m = o.get_editor_property("static_mesh") if isinstance(o, unreal.StaticMeshComponent) else None
            log(f"  {o.get_name()} ({o.get_class().get_name()}) {m.get_name() if m else ''}")
with open(r"E:\Modding\Logs\inspect_lookout.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
