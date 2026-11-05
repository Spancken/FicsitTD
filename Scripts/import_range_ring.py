# Importiert den Reichweitenkreis des Bolzenturms (Blender-Szene TD_Ring, Radius 1 m, wird im Spiel auf die Reichweite skaliert)
# und legt ein leuchtendes Material an. Einmalig ausfuehren; vor setup_turret_blueprint.py.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

FBX = r"E:\Modding\Export\RangeRing\SM_TD_RangeRing.fbx"
MESH_DIR = "/FicsitTD/Buildings/BoltTurret/Mesh"
MAT = "/FicsitTD/Buildings/BoltTurret/Materials/M_TD_RangeRing"

at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

try:
    if EAL.does_asset_exist(MAT):
        m = unreal.load_asset(MAT)
        mel.delete_all_material_expressions(m)
    else:
        m = at.create_asset("M_TD_RangeRing", MAT.rsplit("/", 1)[0], unreal.Material, unreal.MaterialFactoryNew())
    # Unbeleuchtet und kraeftig cyan, damit der Kreis auch nachts und auf Gras gut zu sehen ist
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    e = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 0)
    e.set_editor_property("constant", unreal.LinearColor(0.1, 1.5, 2.5, 1.0))
    mel.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    EAL.save_loaded_asset(m, False)

    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", False)
    opts.set_editor_property("import_materials", False)
    opts.set_editor_property("import_textures", False)
    opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = opts.static_mesh_import_data
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("import_uniform_scale", 1.0)  # dieses FBX kommt schon in Zentimetern an (mit 100 war der Ring 101 m gross)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", FBX)
    task.set_editor_property("destination_path", MESH_DIR)
    task.set_editor_property("destination_name", "SM_TD_RangeRing")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", opts)
    at.import_asset_tasks([task])

    mesh = unreal.load_asset(f"{MESH_DIR}/SM_TD_RangeRing")
    for i in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(i, m)
    b = mesh.get_bounding_box()
    log(f"SM_TD_RangeRing: Dreiecke={mesh.get_num_triangles(0)} min={b.min} max={b.max}")
    EAL.save_loaded_asset(mesh, False)
    log("ERFOLG")
except Exception as ex:
    log(f"FEHLER: {ex}")

with open(r"E:\Modding\Logs\import_range_ring.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
