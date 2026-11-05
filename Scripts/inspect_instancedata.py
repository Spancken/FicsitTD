# Prueft, ob die Platzhalter-Mauer ihr Modell ueber mInstanceDataCDO mitbringt (Voraussetzung fuers Duplizieren).
import unreal

out = []
bp = unreal.load_asset("/Game/FactoryGame/Buildable/Building/Wall/Build_Wall_8x4_01")
cdo = unreal.get_default_object(bp.generated_class())
try:
    data = cdo.get_editor_property("mInstanceDataCDO")
    out.append(f"mInstanceDataCDO = {data}")
    if data:
        for name in ["instances", "Instances", "mInstances"]:
            try:
                inst = data.get_editor_property(name)
                out.append(f"  {name}: {len(inst)} Eintraege")
                for i in inst[:4]:
                    out.append(f"    {i}")
                break
            except Exception as e:
                out.append(f"  {name}: {e}")
except Exception as e:
    out.append(f"Fehler: {e}")
out.append("Elternklasse: " + str(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp) if hasattr(unreal.BlueprintEditorLibrary, "get_blueprint_parent_class") else "?"))
out.append("reparent vorhanden: " + str(hasattr(unreal.BlueprintEditorLibrary, "reparent_blueprint")))
with open(r"E:\Modding\Logs\inspect_instancedata.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
