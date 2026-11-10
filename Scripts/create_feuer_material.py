# 0.9.3 Feuer-Flipbook (Marius 09.11.2026: "Blender-Flipbook", dann "Ja, einbauen").
# Importiert E:\Modding\Export\Effects\Feuer\T_TD_FeuerFlipbook.png (8x8 Bilder je 256 px; R Flamme, G Hitze, B Dichte;
# erzeugt mit Satisfactory\Modelle\td_feuer_flipbook.py + td_feuer_atlas.py) und legt M_TD_FeuerWolke an:
#   additiv, unlit, beidseitig. Parameter je Wolke (ATDFlameTurret setzt sie ueber eine dynamische Instanz):
#   Bild 0..1 (Flipbook-Stelle), Alter 0..1 (Abkuehlung), Hell/Rot (Treibstoff-Farben), Staerke, SpiegelU/SpiegelV (0/1, Varianten).
#   Farbe = lerp(Rot, Hell, saturate(G * (1.15 - 0.95 * Alter))^2) * R * (0.75 - 0.45 * Alter) * Staerke * DepthFade
# Wie td_feuer_atlas.py (Vorschau_Feuer.gif). Vorher Content/Effects/M_TD_FeuerWolke.uasset und T_TD_FeuerFlipbook.uasset loeschen.
import os
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
FX_DIR = "/FicsitTD/Effects"
PNG = r"E:\Modding\Export\Effects\Feuer\T_TD_FeuerFlipbook.png"


def a(m, klasse, x, y, **werte):
    e = mel.create_material_expression(m, klasse, x, y)
    for k, v in werte.items():
        e.set_editor_property(k, v)
    return e


def c(x, z, eingang=""):
    mel.connect_material_expressions(x, "", z, eingang)


def c2(x, ausgang, z, eingang=""):
    mel.connect_material_expressions(x, ausgang, z, eingang)


def textur():
    t = unreal.AssetImportTask()
    for k, v in (("filename", PNG), ("destination_path", FX_DIR), ("destination_name", "T_TD_FeuerFlipbook"), ("automated", True),
                 ("replace_existing", True), ("save", False)):
        t.set_editor_property(k, v)
    at.import_asset_tasks([t])
    tex = unreal.load_asset(f"{FX_DIR}/T_TD_FeuerFlipbook")
    tex.set_editor_property("srgb", False)   # Kanaele sind Daten (Flamme, Hitze, Dichte)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    EAL.save_loaded_asset(tex, False)
    log(f"Textur {tex.blueprint_get_size_x()} x {tex.blueprint_get_size_y()}")
    return tex


def material(tex):
    m = at.create_asset("M_TD_FeuerWolke", FX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    # --- Flipbook-UV: Zelle = floor(Bild * 63,99); Spalte = fmod(Zelle, 8), Zeile = floor(Zelle / 8)
    uv = a(m, unreal.MaterialExpressionTextureCoordinate, -2200, 0)
    u = a(m, unreal.MaterialExpressionComponentMask, -2000, -60, r=True, g=False, b=False, a=False); c(uv, u)
    v = a(m, unreal.MaterialExpressionComponentMask, -2000, 60, r=False, g=True, b=False, a=False); c(uv, v)
    su = a(m, unreal.MaterialExpressionScalarParameter, -2000, -200, parameter_name="SpiegelU", default_value=0.0)
    sv = a(m, unreal.MaterialExpressionScalarParameter, -2000, 200, parameter_name="SpiegelV", default_value=0.0)
    du = a(m, unreal.MaterialExpressionSubtract, -1800, -100); c(su, du, "A"); c(u, du, "B")
    au = a(m, unreal.MaterialExpressionAbs, -1650, -100); c(du, au)
    dv = a(m, unreal.MaterialExpressionSubtract, -1800, 100); c(sv, dv, "A"); c(v, dv, "B")
    av = a(m, unreal.MaterialExpressionAbs, -1650, 100); c(dv, av)
    lok = a(m, unreal.MaterialExpressionAppendVector, -1500, 0); c(au, lok, "A"); c(av, lok, "B")
    bild = a(m, unreal.MaterialExpressionScalarParameter, -2000, 400, parameter_name="Bild", default_value=0.3)
    sat = a(m, unreal.MaterialExpressionSaturate, -1850, 400); c(bild, sat)
    mal = a(m, unreal.MaterialExpressionMultiply, -1700, 400, const_b=63.99); c(sat, mal, "A")
    zelle = a(m, unreal.MaterialExpressionFloor, -1550, 400); c(mal, zelle)
    acht = a(m, unreal.MaterialExpressionConstant, -1550, 520, r=8.0)
    spalte = a(m, unreal.MaterialExpressionFmod, -1400, 380); c(zelle, spalte, "A"); c(acht, spalte, "B")
    durch = a(m, unreal.MaterialExpressionDivide, -1400, 500); c(zelle, durch, "A"); c(acht, durch, "B")
    zeile = a(m, unreal.MaterialExpressionFloor, -1250, 500); c(durch, zeile)
    versatz = a(m, unreal.MaterialExpressionAppendVector, -1100, 420); c(spalte, versatz, "A"); c(zeile, versatz, "B")
    summe = a(m, unreal.MaterialExpressionAdd, -950, 200); c(lok, summe, "A"); c(versatz, summe, "B")
    uv8 = a(m, unreal.MaterialExpressionDivide, -800, 200, const_b=8.0); c(summe, uv8, "A")
    probe = a(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, 200, parameter_name="Flipbook", texture=tex,
              sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    c(uv8, probe, "UVs")
    # --- Farbe
    alter = a(m, unreal.MaterialExpressionScalarParameter, -600, 600, parameter_name="Alter", default_value=0.3)
    kuehl = a(m, unreal.MaterialExpressionMultiply, -450, 600, const_b=-0.95); c(alter, kuehl, "A")
    kuehl2 = a(m, unreal.MaterialExpressionAdd, -300, 600, const_b=1.15); c(kuehl, kuehl2, "A")
    hitze = a(m, unreal.MaterialExpressionMultiply, -300, 450); c2(probe, "G", hitze, "A"); c(kuehl2, hitze, "B")
    hs = a(m, unreal.MaterialExpressionSaturate, -150, 450); c(hitze, hs)
    h2 = a(m, unreal.MaterialExpressionMultiply, 0, 450); c(hs, h2, "A"); c(hs, h2, "B")
    hell = a(m, unreal.MaterialExpressionVectorParameter, -150, 750, parameter_name="Hell", default_value=unreal.LinearColor(1.0, 0.85, 0.45, 1.0))
    rot = a(m, unreal.MaterialExpressionVectorParameter, -150, 900, parameter_name="Rot", default_value=unreal.LinearColor(1.0, 0.35, 0.05, 1.0))
    mix = a(m, unreal.MaterialExpressionLinearInterpolate, 150, 750); c(rot, mix, "A"); c(hell, mix, "B"); c(h2, mix, "Alpha")
    dunkel = a(m, unreal.MaterialExpressionMultiply, -450, 750, const_b=-0.45); c(alter, dunkel, "A")
    dunkel2 = a(m, unreal.MaterialExpressionAdd, -300, 750, const_b=0.75); c(dunkel, dunkel2, "A")
    fl = a(m, unreal.MaterialExpressionMultiply, 150, 250); c2(probe, "R", fl, "A"); c(dunkel2, fl, "B")
    st = a(m, unreal.MaterialExpressionScalarParameter, 150, 400, parameter_name="Staerke", default_value=8.0)
    fl2 = a(m, unreal.MaterialExpressionMultiply, 300, 300); c(fl, fl2, "A"); c(st, fl2, "B")
    tiefe = a(m, unreal.MaterialExpressionDepthFade, 300, 450, fade_distance_default=80.0, opacity_default=1.0)
    fl3 = a(m, unreal.MaterialExpressionMultiply, 450, 350); c(fl2, fl3, "A"); c(tiefe, fl3, "B")
    em = a(m, unreal.MaterialExpressionMultiply, 600, 500); c(mix, em, "A"); c(fl3, em, "B")
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    EAL.save_loaded_asset(m, False)
    log(f"Material M_TD_FeuerWolke: {mel.get_num_material_expressions(m)} Knoten")


try:
    tex = textur()
    material(tex)
    log("ERFOLG")
except Exception as e:
    log(f"FEHLER: {e}")
with open(r"E:\Modding\Logs\create_feuer_material.txt", "w", encoding="utf-8") as fh:
    fh.write("\n".join(LOG))
