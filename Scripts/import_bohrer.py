# Abwehrkern als Tiefbohrer (0.61, Marius 04.11.2026): importiert die fuenf Teile aus Satisfactory\Modelle\TD_DefenseCore_Bohrer.blend
# (Export E:\Modding\Export\DefenseCore\SM_TD_DefenseCore_{Base,Drill,Lift,Gear,Ring}.fbx) und das neue Icon.
# Danach create_factory_materials.py ausfuehren (Spielmaterial, lackierbar) und Build-Mod.ps1 ohne -SkipEditor (C++ geaendert).
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

at = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MAT_DIR = "/FicsitTD/Buildings/BoltTurret/Materials"
ZIEL = "/FicsitTD/Buildings/DefenseCore/Mesh"
TEILE = ["SM_TD_DefenseCore_Base", "SM_TD_DefenseCore_Drill", "SM_TD_DefenseCore_Lift", "SM_TD_DefenseCore_Gear", "SM_TD_DefenseCore_Ring"]
MATS = ["M_TD_PaintPrimary", "M_TD_PaintSecondary", "M_TD_Metal", "M_TD_Hazard", "M_TD_Emissive", "M_TD_Rubber", "M_TD_Chrome", "M_TD_StatusLight"]

try:
    mats = {n: unreal.load_asset(f"{MAT_DIR}/{n}") for n in MATS}
    for name in TEILE:
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
        for k, v in (("filename", rf"E:\Modding\Export\DefenseCore\{name}.fbx"), ("destination_path", ZIEL), ("destination_name", name),
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
        log(f"{name}: Dreiecke={mesh.get_num_triangles(0)} Slots={len(mesh.get_editor_property('static_materials'))} "
            f"min=({b.min.x:.0f},{b.min.y:.0f},{b.min.z:.0f}) max=({b.max.x:.0f},{b.max.y:.0f},{b.max.z:.0f})")
        EAL.save_loaded_asset(mesh, False)
    # Icon an derselben Stelle ersetzen (Beschreibung verweist schon darauf)
    task = unreal.AssetImportTask()
    for k, v in (("filename", r"E:\Modding\Export\Icons\Icon_TD_DefenseCore_512.png"), ("destination_path", "/FicsitTD/UI/Icons"),
                 ("destination_name", "Icon_TD_DefenseCore_512"), ("automated", True), ("replace_existing", True), ("save", False)):
        task.set_editor_property(k, v)
    at.import_asset_tasks([task])
    tex = unreal.load_asset("/FicsitTD/UI/Icons/Icon_TD_DefenseCore_512")
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("srgb", True)
    EAL.save_loaded_asset(tex, False)
    log(f"Icon: {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}")
    log("ERFOLG")
except Exception as e:
    log(f"FEHLER: {e}")
with open(r"E:\Modding\Logs\import_bohrer.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
