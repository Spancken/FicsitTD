# Liest Groesse und Detailgrad von Spielmodellen aus (Platzhalter im Starter Project spiegeln die echten Pfade).
import unreal

PFADE = [
    "/Game/FactoryGame/Buildable/Factory/FactoryInputs/Mesh/SM_Input_01",
    "/Game/FactoryGame/Buildable/Factory/FactoryInputs/Mesh/SM_Pipe_InPut_01",
    "/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Mesh/SM_VAT_Constructor_MK1",
]
out = []
for p in PFADE:
    m = unreal.load_asset(p)
    if m is None:
        out.append(f"{p}: nicht gefunden")
        continue
    b = m.get_bounding_box()
    size = b.max - b.min
    try:
        tris = m.get_num_triangles(0)
    except Exception:
        tris = "?"
    mats = [str(s.material_slot_name) for s in m.get_editor_property("static_materials")]
    out.append(f"{p}: min={b.min} max={b.max} groesse(cm)={size} dreiecke={tris} slots={mats}")
with open(r"E:\Modding\Logs\inspect_meshes.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
