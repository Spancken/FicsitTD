# Importiert Abwehrkern, Panzersperre und Nobelisk-Geschoss (Blender-Exporte unter E:\Modding\Export) mit den vorhandenen Turm-Materialien.
# Vorhandene SM_TD_DefenseCore_*.uasset werden ersetzt (replace_existing).
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

at = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MAT_DIR = "/FicsitTD/Buildings/BoltTurret/Materials"
TEILE = [("DefenseCore", "SM_TD_DefenseCore_Base", "/FicsitTD/Buildings/DefenseCore/Mesh"),
         ("DefenseCore", "SM_TD_DefenseCore_Top", "/FicsitTD/Buildings/DefenseCore/Mesh"),
         ("DefenseCore", "SM_TD_DefenseCore_Core", "/FicsitTD/Buildings/DefenseCore/Mesh"),
         ("Barrier", "SM_TD_Barrier_Base", "/FicsitTD/Buildings/Barrier/Mesh"),
         ("Effects", "SM_TD_Effects_NobeliskShell", "/FicsitTD/Effects"),
         ("RepairStation", "SM_TD_RepairStation_Base", "/FicsitTD/Buildings/RepairStation/Mesh"),
         ("RepairStation", "SM_TD_RepairStation_Yaw", "/FicsitTD/Buildings/RepairStation/Mesh")]
MATS = ["M_TD_PaintPrimary", "M_TD_PaintSecondary", "M_TD_Metal", "M_TD_Hazard", "M_TD_Emissive", "M_TD_Rubber",
        "M_TD_Copper", "M_TD_Chrome", "M_TD_TeslaGlow", "M_TD_Warning", "M_TD_Brass"]

try:
    mats = {n: unreal.load_asset(f"{MAT_DIR}/{n}") for n in MATS}
    for ordner, name, ZIEL in TEILE:
        EAL.make_directory(ZIEL)
        opts = unreal.FbxImportUI()
        opts.set_editor_property("import_mesh", True)
        opts.set_editor_property("import_as_skeletal", False)
        opts.set_editor_property("import_materials", False)
        opts.set_editor_property("import_textures", False)
        opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        sm = opts.static_mesh_import_data
        sm.set_editor_property("combine_meshes", True)
        sm.set_editor_property("auto_generate_collision", False)
        sm.set_editor_property("import_uniform_scale", 1.0)
        sm.set_editor_property("generate_lightmap_u_vs", True)
        t = unreal.AssetImportTask()
        for k, v in (("filename", rf"E:\Modding\Export\{ordner}\{name}.fbx"), ("destination_path", ZIEL), ("destination_name", name),
                     ("automated", True), ("replace_existing", True), ("save", True), ("options", opts)):
            t.set_editor_property(k, v)
        at.import_asset_tasks([t])
        mesh = unreal.load_asset(f"{ZIEL}/{name}")
        for i, s in enumerate(mesh.get_editor_property("static_materials")):
            key = next((k for k in mats if str(s.material_slot_name).startswith(k)), None)
            if key and mats[key]:
                mesh.set_material(i, mats[key])
            else:
                log(f"  {name}: Slot {s.material_slot_name} ohne Material")
        b = mesh.get_bounding_box()
        log(f"{name}: Dreiecke={mesh.get_num_triangles(0)} min=({b.min.x:.0f},{b.min.y:.0f},{b.min.z:.0f}) max=({b.max.x:.0f},{b.max.y:.0f},{b.max.z:.0f})")
        EAL.save_loaded_asset(mesh, False)
    log("ERFOLG")
except Exception as e:
    log(f"FEHLER: {e}")
with open(r"E:\Modding\Logs\import_core_meshes.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
