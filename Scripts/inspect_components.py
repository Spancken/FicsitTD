# Listet die Komponenten (Typ, Position, Rotation, Mesh) von Vanilla-Gebaeuden, um Foerderband-Anschluesse zu verstehen.
import unreal

BPS = [
    "/Game/FactoryGame/Buildable/Factory/StorageContainerMk1/Build_StorageContainerMk1",
    "/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1",
]
out = []
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for p in BPS:
    bp = unreal.load_asset(p)
    out.append(f"== {p}")
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is None or not isinstance(obj, unreal.SceneComponent):
            continue
        line = f"  {obj.get_name()} ({obj.get_class().get_name()}) loc={obj.get_editor_property('relative_location')} rot={obj.get_editor_property('relative_rotation')}"
        if isinstance(obj, unreal.StaticMeshComponent):
            line += f" mesh={obj.get_editor_property('static_mesh')}"
        try:
            line += f" dir={obj.get_editor_property('mDirection')} type={obj.get_editor_property('mConnectorClearance') if False else ''}"
        except Exception:
            pass
        out.append(line)
with open(r"E:\Modding\Logs\inspect_components.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
