# Teslaspule: Blitz-Modell (Modelle\TD_Effekte.blend -> SM_TD_Lightning, 1 m lang entlang +X, Code streckt es) mit additivem
# Material M_TD_Lightning (Parameter Staerke) und Leuchtkanten-Material M_TD_TeslaGlow mit Parameter Leuchtkraft (Code laesst es
# beim Aufladen aufgluehen). Vorher M_TD_TeslaGlow, M_TD_Lightning und SM_TD_Lightning (.uasset) per Dateisystem loeschen.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MAT_DIR = "/FicsitTD/Buildings/BoltTurret/Materials"
FX_DIR = "/FicsitTD/Effects"

def ausdruck(m, klasse, x, y, **werte):
    e = mel.create_material_expression(m, klasse, x, y)
    for k, v in werte.items():
        e.set_editor_property(k, v)
    return e

def verbinde(a, b, eingang):
    mel.connect_material_expressions(a, "", b, eingang)

try:
    # Leuchtkanten: blau, Leuchtkraft per Parameter
    g = at.create_asset("M_TD_TeslaGlow", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    farbe = ausdruck(g, unreal.MaterialExpressionConstant3Vector, -600, 0, constant=unreal.LinearColor(0.3, 0.55, 1.0, 1.0))
    mel.connect_material_property(farbe, "", unreal.MaterialProperty.MP_BASE_COLOR)
    kraft = ausdruck(g, unreal.MaterialExpressionScalarParameter, -600, 150, parameter_name="Leuchtkraft", default_value=4.0)
    em = ausdruck(g, unreal.MaterialExpressionMultiply, -300, 80)
    verbinde(farbe, em, "A"); verbinde(kraft, em, "B")
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(ausdruck(g, unreal.MaterialExpressionConstant, -600, 300, r=0.2), "", unreal.MaterialProperty.MP_ROUGHNESS)
    g.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(g); EAL.save_loaded_asset(g, False)

    # Blitz: additiv, unbeleuchtet, beidseitig; quer zum Band hell in der Mitte
    m = at.create_asset("M_TD_Lightning", FX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    uv = ausdruck(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    u = ausdruck(m, unreal.MaterialExpressionComponentMask, -1000, 0, r=True, g=False, b=False, a=False)
    verbinde(uv, u, "")
    u2 = ausdruck(m, unreal.MaterialExpressionMultiply, -850, 0, const_b=2.0)
    verbinde(u, u2, "A")
    um = ausdruck(m, unreal.MaterialExpressionSubtract, -700, 0, const_b=1.0)
    verbinde(u2, um, "A")
    ab = ausdruck(m, unreal.MaterialExpressionAbs, -550, 0)
    verbinde(um, ab, "")
    rest = ausdruck(m, unreal.MaterialExpressionOneMinus, -420, 0)
    verbinde(ab, rest, "")
    kern = ausdruck(m, unreal.MaterialExpressionPower, -280, 0, const_exponent=2.0)
    verbinde(rest, kern, "Base")
    col = ausdruck(m, unreal.MaterialExpressionConstant3Vector, -420, 200, constant=unreal.LinearColor(0.55, 0.75, 1.0, 1.0))
    st = ausdruck(m, unreal.MaterialExpressionScalarParameter, -420, 320, parameter_name="Staerke", default_value=25.0)
    c1 = ausdruck(m, unreal.MaterialExpressionMultiply, -200, 220)
    verbinde(col, c1, "A"); verbinde(st, c1, "B")
    em2 = ausdruck(m, unreal.MaterialExpressionMultiply, 0, 100)
    verbinde(kern, em2, "A"); verbinde(c1, em2, "B")
    mel.connect_material_property(em2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m); EAL.save_loaded_asset(m, False)

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
    t = unreal.AssetImportTask()
    for k, v in (("filename", r"E:\Modding\Export\Effects\SM_TD_Lightning.fbx"), ("destination_path", FX_DIR), ("destination_name", "SM_TD_Lightning"),
                 ("automated", True), ("replace_existing", True), ("save", True), ("options", opts)):
        t.set_editor_property(k, v)
    at.import_asset_tasks([t])
    blitz = unreal.load_asset(f"{FX_DIR}/SM_TD_Lightning")
    blitz.set_material(0, m)
    b = blitz.get_bounding_box()
    log(f"SM_TD_Lightning: min={b.min} max={b.max}")
    EAL.save_loaded_asset(blitz, False)

    coil = unreal.load_asset("/FicsitTD/Buildings/TeslaCoil/Mesh/SM_TD_TeslaCoil_Coil")
    for i, s in enumerate(coil.get_editor_property("static_materials")):
        if str(s.material_slot_name).startswith("M_TD_TeslaGlow"):
            coil.set_material(i, g)
            log(f"Spule: Slot {i} {s.material_slot_name} -> M_TD_TeslaGlow (mit Leuchtkraft)")
    EAL.save_loaded_asset(coil, False)
    log("ERFOLG")
except Exception as e:
    import traceback
    log(f"FEHLER: {e}\n{traceback.format_exc()}")

with open(r"E:\Modding\Logs\import_tesla_effects.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
