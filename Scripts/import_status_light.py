# Importiert die eigene Statuslampe des Bolzenturms (Blender-Szene TD_StatusLight) und legt ihr Leuchtmaterial an.
# Ersetzt BP_ProductionIndicatorInstanced des Spiels: dessen Schild wurde am Turm schwarz gezeichnet (Test 0.8.1).
# Farbe setzt der Turm zur Laufzeit ueber den Parameter "Farbe" (ATDBoltTurret::AktualisiereLampe). Vor setup_turret_blueprint.py.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

FBX = r"E:\Modding\Export\StatusLight\SM_TD_StatusLight.fbx"
MESH_DIR = "/FicsitTD/Buildings/BoltTurret/Mesh"
MAT_DIR = "/FicsitTD/Buildings/BoltTurret/Materials"

at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

try:
    # Alte Datei vorher per Dateisystem loeschen (create_asset fragt sonst im unbeaufsichtigten Lauf nicht nach)
    m = at.create_asset("M_TD_StatusLight", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    farbe = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, 0)
    farbe.set_editor_property("parameter_name", "Farbe")
    farbe.set_editor_property("default_value", unreal.LinearColor(0.02, 0.02, 0.02, 1.0))
    mel.connect_material_property(farbe, "", unreal.MaterialProperty.MP_BASE_COLOR)
    staerke = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 200)
    staerke.set_editor_property("parameter_name", "Leuchtkraft")
    staerke.set_editor_property("default_value", 0.0)
    mal = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 100)
    mel.connect_material_expressions(farbe, "", mal, "A")
    mel.connect_material_expressions(staerke, "", mal, "B")
    mel.connect_material_property(mal, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    r = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 300)
    r.set_editor_property("r", 0.2)
    mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
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
    sm.set_editor_property("import_uniform_scale", 1.0)  # Export mit apply_unit_scale kommt schon in Zentimetern an
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", FBX)
    task.set_editor_property("destination_path", MESH_DIR)
    task.set_editor_property("destination_name", "SM_TD_StatusLight")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    task.set_editor_property("options", opts)
    at.import_asset_tasks([task])

    mesh = unreal.load_asset(f"{MESH_DIR}/SM_TD_StatusLight")
    for i, s in enumerate(mesh.get_editor_property("static_materials")):
        name = str(s.material_slot_name)
        ziel = m if name.startswith("M_TD_StatusLight") else unreal.load_asset(f"{MAT_DIR}/M_TD_PaintSecondary")
        mesh.set_material(i, ziel)
        log(f"Slot {i} {name} -> {ziel.get_name()}")
    b = mesh.get_bounding_box()
    log(f"SM_TD_StatusLight: min={b.min} max={b.max}")
    EAL.save_loaded_asset(mesh, False)
    log("ERFOLG")
except Exception as ex:
    log(f"FEHLER: {ex}")

with open(r"E:\Modding\Logs\import_status_light.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
