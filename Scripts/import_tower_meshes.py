# Importiert die Blender-Entwuerfe der weiteren Tuerme (Modelle\td_bau.py -> E:\Modding\Export\<Turm>) und die Effekt-Modelle
# (Flammenkegel, Muendungsfeuer) samt Materialien. FBX kommen mit apply_unit_scale -> import_uniform_scale 1.
# Vorhandene .uasset der Ziel-Ordner vorher per Dateisystem loeschen (create_asset fragt sonst im Commandlet nicht nach).
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EXPORT = r"E:\Modding\Export"
MAT_DIR = "/FicsitTD/Buildings/BoltTurret/Materials"   # gemeinsame Turm-Materialien
FX_DIR = "/FicsitTD/Effects"
TUERME = {
    "GunTurret": ["Base", "Yaw", "Head", "Barrels"],
    "FlameTurret": ["Base", "Yaw", "Head"],
    "TeslaCoil": ["Base", "Coil"],
    "NobeliskLauncher": ["Base", "Yaw", "Head"],
}
# neue Turm-Materialien (die alten legt import_turret_meshes.py an): Farbe linear, Metallic, Rauheit, Leuchtstaerke
NEUE_MATERIALIEN = {
    "M_TD_Copper":    ((0.75, 0.35, 0.15), 1.0, 0.3, 0.0),
    "M_TD_Chrome":    ((0.8, 0.8, 0.82), 1.0, 0.08, 0.0),
    "M_TD_TeslaGlow": ((0.3, 0.55, 1.0), 0.0, 0.2, 10.0),
    "M_TD_Warning":   ((0.6, 0.03, 0.02), 0.1, 0.5, 0.0),
    "M_TD_Brass":     ((0.7, 0.5, 0.15), 1.0, 0.3, 0.0),
}

at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

def neues_material(name, ordner):
    return at.create_asset(name, ordner, unreal.Material, unreal.MaterialFactoryNew())

def ausdruck(m, klasse, x, y, **werte):
    e = mel.create_material_expression(m, klasse, x, y)
    for k, v in werte.items():
        e.set_editor_property(k, v)
    return e

def turm_material(name, rgb, metal, rough, emit):
    m = neues_material(name, MAT_DIR)
    col = ausdruck(m, unreal.MaterialExpressionConstant3Vector, -400, 0, constant=unreal.LinearColor(*rgb, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    for prop, val, y in ((unreal.MaterialProperty.MP_METALLIC, metal, 150), (unreal.MaterialProperty.MP_ROUGHNESS, rough, 250)):
        mel.connect_material_property(ausdruck(m, unreal.MaterialExpressionConstant, -400, y, r=val), "", prop)
    if emit > 0:
        e = ausdruck(m, unreal.MaterialExpressionConstant3Vector, -400, 400, constant=unreal.LinearColor(rgb[0] * emit, rgb[1] * emit, rgb[2] * emit, 1.0))
        mel.connect_material_property(e, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(m)
    EAL.save_loaded_asset(m, False)
    return m

def effekt_grund(name):
    m = neues_material(name, FX_DIR)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    return m

def flammen_material():
    """Additiver Flammenkegel: aufsteigendes 3D-Rauschen entlang V, gelb-weiss an der Duese -> rot am Ende, blendet zum Ende aus."""
    m = effekt_grund("M_TD_FlameJet")
    uv = ausdruck(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    skal = ausdruck(m, unreal.MaterialExpressionConstant2Vector, -1400, 120, r=3.0, g=4.0)
    uv_s = ausdruck(m, unreal.MaterialExpressionMultiply, -1200, 40)
    mel.connect_material_expressions(uv, "", uv_s, "A"); mel.connect_material_expressions(skal, "", uv_s, "B")
    zeit = ausdruck(m, unreal.MaterialExpressionTime, -1400, 260)
    tempo = ausdruck(m, unreal.MaterialExpressionConstant2Vector, -1400, 360, r=0.0, g=5.0)
    fluss = ausdruck(m, unreal.MaterialExpressionMultiply, -1200, 300)
    mel.connect_material_expressions(zeit, "", fluss, "A"); mel.connect_material_expressions(tempo, "", fluss, "B")
    pos2 = ausdruck(m, unreal.MaterialExpressionSubtract, -1000, 100)
    mel.connect_material_expressions(uv_s, "", pos2, "A"); mel.connect_material_expressions(fluss, "", pos2, "B")
    brodeln = ausdruck(m, unreal.MaterialExpressionMultiply, -1000, 300, const_b=0.7)
    mel.connect_material_expressions(zeit, "", brodeln, "A")
    pos3 = ausdruck(m, unreal.MaterialExpressionAppendVector, -800, 150)
    mel.connect_material_expressions(pos2, "", pos3, "A"); mel.connect_material_expressions(brodeln, "", pos3, "B")
    rauschen = ausdruck(m, unreal.MaterialExpressionNoise, -600, 150, scale=1.0, levels=4, output_min=0.0, output_max=1.0)
    mel.connect_material_expressions(pos3, "", rauschen, "Position")
    v = ausdruck(m, unreal.MaterialExpressionComponentMask, -1200, 500, r=False, g=True, b=False, a=False)
    mel.connect_material_expressions(uv, "", v, "")
    rest = ausdruck(m, unreal.MaterialExpressionOneMinus, -1000, 500)
    mel.connect_material_expressions(v, "", rest, "")
    fade = ausdruck(m, unreal.MaterialExpressionPower, -800, 500, const_exponent=0.8)
    mel.connect_material_expressions(rest, "", fade, "Base")
    hell = ausdruck(m, unreal.MaterialExpressionConstant3Vector, -800, 650, constant=unreal.LinearColor(1.0, 0.75, 0.35, 1.0))
    rot = ausdruck(m, unreal.MaterialExpressionConstant3Vector, -800, 760, constant=unreal.LinearColor(1.0, 0.18, 0.02, 1.0))
    farbe = ausdruck(m, unreal.MaterialExpressionLinearInterpolate, -600, 650)
    mel.connect_material_expressions(hell, "", farbe, "A"); mel.connect_material_expressions(rot, "", farbe, "B"); mel.connect_material_expressions(v, "", farbe, "Alpha")
    a1 = ausdruck(m, unreal.MaterialExpressionMultiply, -400, 300)
    mel.connect_material_expressions(rauschen, "", a1, "A"); mel.connect_material_expressions(fade, "", a1, "B")
    a2 = ausdruck(m, unreal.MaterialExpressionMultiply, -250, 400)
    mel.connect_material_expressions(a1, "", a2, "A"); mel.connect_material_expressions(farbe, "", a2, "B")
    staerke = ausdruck(m, unreal.MaterialExpressionScalarParameter, -400, 550, parameter_name="Staerke", default_value=6.0)
    em = ausdruck(m, unreal.MaterialExpressionMultiply, -100, 400)
    mel.connect_material_expressions(a2, "", em, "A"); mel.connect_material_expressions(staerke, "", em, "B")
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m); EAL.save_loaded_asset(m, False)
    return m

def muendungs_material():
    """Additiver Stern: radialer Verlauf um die UV-Mitte."""
    m = effekt_grund("M_TD_MuzzleFlash")
    uv = ausdruck(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    mitte = ausdruck(m, unreal.MaterialExpressionConstant2Vector, -900, 120, r=0.5, g=0.5)
    abst = ausdruck(m, unreal.MaterialExpressionDistance, -700, 50)
    mel.connect_material_expressions(uv, "", abst, "A"); mel.connect_material_expressions(mitte, "", abst, "B")
    doppelt = ausdruck(m, unreal.MaterialExpressionMultiply, -550, 50, const_b=2.0)
    mel.connect_material_expressions(abst, "", doppelt, "A")
    rest = ausdruck(m, unreal.MaterialExpressionOneMinus, -400, 50)
    mel.connect_material_expressions(doppelt, "", rest, "")
    sat = ausdruck(m, unreal.MaterialExpressionSaturate, -280, 50)
    mel.connect_material_expressions(rest, "", sat, "")
    pw = ausdruck(m, unreal.MaterialExpressionPower, -160, 50, const_exponent=2.0)
    mel.connect_material_expressions(sat, "", pw, "Base")
    col = ausdruck(m, unreal.MaterialExpressionConstant3Vector, -400, 200, constant=unreal.LinearColor(1.0, 0.7, 0.3, 1.0))
    staerke = ausdruck(m, unreal.MaterialExpressionScalarParameter, -400, 300, parameter_name="Staerke", default_value=25.0)
    c1 = ausdruck(m, unreal.MaterialExpressionMultiply, -150, 200)
    mel.connect_material_expressions(col, "", c1, "A"); mel.connect_material_expressions(staerke, "", c1, "B")
    em = ausdruck(m, unreal.MaterialExpressionMultiply, 0, 100)
    mel.connect_material_expressions(pw, "", em, "A"); mel.connect_material_expressions(c1, "", em, "B")
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m); EAL.save_loaded_asset(m, False)
    return m

def importiere(fbx, ordner, name):
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
    for k, v in (("filename", fbx), ("destination_path", ordner), ("destination_name", name), ("automated", True),
                 ("replace_existing", True), ("save", True), ("options", opts)):
        t.set_editor_property(k, v)
    at.import_asset_tasks([t])
    return unreal.load_asset(f"{ordner}/{name}")

def zuordnen(mesh, mats):
    for i, s in enumerate(mesh.get_editor_property("static_materials")):
        name = str(s.material_slot_name)
        key = next((k for k in mats if name.startswith(k)), None)
        if key:
            mesh.set_material(i, mats[key])
        else:
            log(f"  {mesh.get_name()}: Slot {name} ohne Material")
    b = mesh.get_bounding_box()
    log(f"{mesh.get_name()}: Dreiecke={mesh.get_num_triangles(0)} min=({b.min.x:.0f},{b.min.y:.0f},{b.min.z:.0f}) max=({b.max.x:.0f},{b.max.y:.0f},{b.max.z:.0f}) Slots={len(mesh.get_editor_property('static_materials'))}")
    EAL.save_loaded_asset(mesh, False)

try:
    mats = {}
    for n in ("M_TD_PaintPrimary", "M_TD_PaintSecondary", "M_TD_Metal", "M_TD_Hazard", "M_TD_Emissive", "M_TD_Rubber"):
        mats[n] = unreal.load_asset(f"{MAT_DIR}/{n}")
    for n, v in NEUE_MATERIALIEN.items():
        mats[n] = turm_material(n, *v)
    mats["M_TD_FlameJet"] = flammen_material()
    mats["M_TD_MuzzleFlash"] = muendungs_material()
    log(f"Materialien: {[k for k, v in mats.items() if v]}")
    for turm, teile in TUERME.items():
        for teil in teile:
            name = f"SM_TD_{turm}_{teil}"
            mesh = importiere(rf"{EXPORT}\{turm}\{name}.fbx", f"/FicsitTD/Buildings/{turm}/Mesh", name)
            zuordnen(mesh, mats) if mesh else log(f"FEHLER: {name} nicht importiert")
    for name in ("SM_TD_FlameJet", "SM_TD_MuzzleFlash"):
        mesh = importiere(rf"{EXPORT}\Effects\{name}.fbx", FX_DIR, name)
        zuordnen(mesh, mats) if mesh else log(f"FEHLER: {name} nicht importiert")
    log("ERFOLG")
except Exception as e:
    import traceback
    log(f"FEHLER: {e}\n{traceback.format_exc()}")

with open(r"E:\Modding\Logs\import_tower_meshes.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
