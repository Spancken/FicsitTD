# Diagnose der schwarzen Flaechen (Test 0.9.0): Materialien von SM_Input_01 auflisten und das Modell als FBX nach Blender exportieren.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

try:
    mesh = unreal.load_asset("/Game/FactoryGame/Buildable/Factory/FactoryInputs/Mesh/SM_Input_01")
    b = mesh.get_bounding_box()
    log(f"Grenzen {b.min} {b.max}, LODs {mesh.get_num_lods()}")
    for i, s in enumerate(mesh.get_editor_property("static_materials")):
        mat = s.material_interface
        info = ""
        if mat:
            base = mat.get_base_material()
            info = f"{mat.get_path_name()} | Basis {base.get_path_name() if base else '-'}"
            if base:
                info += f" | blend={base.get_editor_property('blend_mode')} twosided={base.get_editor_property('two_sided')} shading={base.get_editor_property('shading_model')}"
        log(f"Slot {i} {s.material_slot_name}: {info}")
    for lod in range(mesh.get_num_lods()):
        log(f"LOD {lod}: Abschnitte {mesh.get_num_sections(lod)}")
    task = unreal.AssetExportTask()
    task.set_editor_property("object", mesh)
    task.set_editor_property("filename", r"E:\Modding\Export\Inspect\SM_Input_01.fbx")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_identical", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("exporter", unreal.StaticMeshExporterFBX())
    log(f"Export: {unreal.Exporter.run_asset_export_task(task)}")
except Exception as e:
    log(f"FEHLER: {e}")

with open(r"E:\Modding\Logs\inspect_input_mesh.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
