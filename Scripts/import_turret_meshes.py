# Importiert die Bolzenturm-Modelle aus Blender (FBX mit UCX-Kollision) und legt einfache Materialien an.
# Hinweis: eigene Materialien = Farben fest, noch nicht ueber das Spiel lackierbar.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

SRC = r"E:\Modding\Export\BoltTurret"
MESH_DIR = "/FicsitTD/Buildings/BoltTurret/Mesh"
MAT_DIR = "/FicsitTD/Buildings/BoltTurret/Materials"
TEILE = ["SM_TD_BoltTurret_Base", "SM_TD_BoltTurret_Yaw", "SM_TD_BoltTurret_Head"]

# Name -> (Farbe linear, Metallic, Roughness, Leuchtstaerke)
MATERIALIEN = {
    "M_TD_PaintPrimary":   ((0.80, 0.30, 0.03), 0.2, 0.45, 0.0),
    "M_TD_PaintSecondary": ((0.06, 0.06, 0.065), 0.4, 0.5, 0.0),
    "M_TD_Metal":          ((0.35, 0.35, 0.37), 1.0, 0.35, 0.0),
    "M_TD_Hazard":         ((0.85, 0.65, 0.05), 0.1, 0.5, 0.0),
    "M_TD_Emissive":       ((1.0, 0.55, 0.1), 0.0, 0.3, 6.0),
    "M_TD_Rubber":         ((0.02, 0.02, 0.02), 0.0, 0.8, 0.0),
}

at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

def material(name, rgb, metal, rough, emit):
    path = f"{MAT_DIR}/{name}"
    if EAL.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = at.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    col = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 0)
    col.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for prop, val, y in ((unreal.MaterialProperty.MP_METALLIC, metal, 150), (unreal.MaterialProperty.MP_ROUGHNESS, rough, 250)):
        c = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, y)
        c.set_editor_property("r", val)
        mel.connect_material_property(c, "", prop)
    if emit > 0:
        e = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 400)
        e.set_editor_property("constant", unreal.LinearColor(rgb[0]*emit, rgb[1]*emit, rgb[2]*emit, 1.0))
        mel.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Sockel wird als Instanz gerendert (FGColoredInstanceMeshProxy) - ohne diese Freigabe faellt er auf das Standardmaterial zurueck
    m.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(m)
    EAL.save_loaded_asset(m, False)
    return m

try:
    mats = {n: material(n, *v) for n, v in MATERIALIEN.items()}
    log(f"Materialien: {list(mats)}")

    tasks = []
    for t in TEILE:
        opts = unreal.FbxImportUI()
        opts.set_editor_property("import_mesh", True)
        opts.set_editor_property("import_as_skeletal", False)
        opts.set_editor_property("import_materials", False)
        opts.set_editor_property("import_textures", False)
        opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        sm = opts.static_mesh_import_data
        sm.set_editor_property("combine_meshes", True)
        sm.set_editor_property("auto_generate_collision", False)
        sm.set_editor_property("import_uniform_scale", 100.0)  # Blender-Meter -> Unreal-Zentimeter
        sm.set_editor_property("generate_lightmap_u_vs", True)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", f"{SRC}\\{t}.fbx")
        task.set_editor_property("destination_path", MESH_DIR)
        task.set_editor_property("destination_name", t)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", opts)
        tasks.append(task)
    at.import_asset_tasks(tasks)

    for t in TEILE:
        mesh = unreal.load_asset(f"{MESH_DIR}/{t}")
        if mesh is None:
            log(f"FEHLER: {t} nicht importiert"); continue
        slots = mesh.get_editor_property("static_materials")
        for i, s in enumerate(slots):
            name = str(s.material_slot_name)
            key = next((k for k in mats if name.startswith(k)), None)
            if key:
                mesh.set_material(i, mats[key])
            else:
                log(f"  {t}: Slot {name} ohne Zuordnung")
        b = mesh.get_bounding_box()
        size = b.max - b.min
        body = mesh.get_editor_property("body_setup")
        n_col = len(body.get_editor_property("agg_geom").get_editor_property("box_elems")) + len(body.get_editor_property("agg_geom").get_editor_property("convex_elems")) if body else 0
        log(f"{t}: Dreiecke={mesh.get_num_triangles(0)} Groesse(cm)=({size.x:.0f}, {size.y:.0f}, {size.z:.0f}) min=({b.min.x:.0f}, {b.min.y:.0f}, {b.min.z:.0f}) Slots={len(slots)} Kollisionsteile={n_col}")
        EAL.save_loaded_asset(mesh, False)
    log("ERFOLG")
except Exception as e:
    log(f"FEHLER: {e}")

with open(r"E:\Modding\Logs\import_turret_meshes.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
