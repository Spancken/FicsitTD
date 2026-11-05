# 0.64 (Marius 04.11.2026: Munition sichtbar am Effekt, keine Anzeigelampe): Effekt-Materialien mit Farbparameter.
#   M_TD_FlameJetFarbe  wie M_TD_FlameJet, Farben "Hell" (Duese) und "Rot" (Ende) als Parameter -> Flamme je Treibstoff
#   M_TD_EffektFarbe    wie M_TD_MuzzleFlash (radialer Stern), Parameter "Farbe" -> Muendungsfeuer, Einschlaege, Gaswolke, Feuerteppich
#   M_TD_SpurFarbe      wie M_TD_Lightning/RepairBeam (Band, hell in der Mitte), Parameter "Farbe" -> Leuchtspuren, Splitter, Feldblitze
# Alle additiv, unlit, Parameter "Staerke". Vorher die drei .uasset in Content/Effects per Dateisystem loeschen.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
FX_DIR = "/FicsitTD/Effects"

def a(m, klasse, x, y, **werte):
    e = mel.create_material_expression(m, klasse, x, y)
    for k, v in werte.items():
        e.set_editor_property(k, v)
    return e

def v(x, y, eingang, z):
    mel.connect_material_expressions(x, "", z, eingang) if y is None else None

def c(x, z, eingang=""):
    mel.connect_material_expressions(x, "", z, eingang)

def grund(name):
    m = at.create_asset(name, FX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    return m

def farbe(m, x, y, name, wert):
    return a(m, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*wert, 1.0))

def flamme():
    m = grund("M_TD_FlameJetFarbe")
    uv = a(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    skal = a(m, unreal.MaterialExpressionConstant2Vector, -1400, 120, r=3.0, g=4.0)
    uv_s = a(m, unreal.MaterialExpressionMultiply, -1200, 40); c(uv, uv_s, "A"); c(skal, uv_s, "B")
    zeit = a(m, unreal.MaterialExpressionTime, -1400, 260)
    tempo = a(m, unreal.MaterialExpressionScalarParameter, -1400, 360, parameter_name="Tempo", default_value=5.0)
    null = a(m, unreal.MaterialExpressionConstant, -1500, 360, r=0.0)
    tv = a(m, unreal.MaterialExpressionAppendVector, -1300, 360); c(null, tv, "A"); c(tempo, tv, "B")
    fluss = a(m, unreal.MaterialExpressionMultiply, -1200, 300); c(zeit, fluss, "A"); c(tv, fluss, "B")
    pos2 = a(m, unreal.MaterialExpressionSubtract, -1000, 100); c(uv_s, pos2, "A"); c(fluss, pos2, "B")
    brodeln = a(m, unreal.MaterialExpressionMultiply, -1000, 300, const_b=0.7); c(zeit, brodeln, "A")
    pos3 = a(m, unreal.MaterialExpressionAppendVector, -800, 150); c(pos2, pos3, "A"); c(brodeln, pos3, "B")
    rauschen = a(m, unreal.MaterialExpressionNoise, -600, 150, scale=1.0, levels=4, output_min=0.0, output_max=1.0); c(pos3, rauschen, "Position")
    vv = a(m, unreal.MaterialExpressionComponentMask, -1200, 500, r=False, g=True, b=False, a=False); c(uv, vv)
    rest = a(m, unreal.MaterialExpressionOneMinus, -1000, 500); c(vv, rest)
    fade = a(m, unreal.MaterialExpressionPower, -800, 500, const_exponent=0.8); c(rest, fade, "Base")
    hell = farbe(m, -800, 650, "Hell", (1.0, 0.75, 0.35))
    rot = farbe(m, -800, 800, "Rot", (1.0, 0.18, 0.02))
    mix = a(m, unreal.MaterialExpressionLinearInterpolate, -600, 650); c(hell, mix, "A"); c(rot, mix, "B"); c(vv, mix, "Alpha")
    a1 = a(m, unreal.MaterialExpressionMultiply, -400, 300); c(rauschen, a1, "A"); c(fade, a1, "B")
    a2 = a(m, unreal.MaterialExpressionMultiply, -250, 400); c(a1, a2, "A"); c(mix, a2, "B")
    st = a(m, unreal.MaterialExpressionScalarParameter, -400, 550, parameter_name="Staerke", default_value=6.0)
    em = a(m, unreal.MaterialExpressionMultiply, -100, 400); c(a2, em, "A"); c(st, em, "B")
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m); EAL.save_loaded_asset(m, False)

def stern():
    m = grund("M_TD_EffektFarbe")
    uv = a(m, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    mitte = a(m, unreal.MaterialExpressionConstant2Vector, -900, 120, r=0.5, g=0.5)
    abst = a(m, unreal.MaterialExpressionDistance, -700, 50); c(uv, abst, "A"); c(mitte, abst, "B")
    dop = a(m, unreal.MaterialExpressionMultiply, -550, 50, const_b=2.0); c(abst, dop, "A")
    rest = a(m, unreal.MaterialExpressionOneMinus, -400, 50); c(dop, rest)
    sat = a(m, unreal.MaterialExpressionSaturate, -280, 50); c(rest, sat)
    pw = a(m, unreal.MaterialExpressionPower, -160, 50, const_exponent=2.0); c(sat, pw, "Base")
    col = farbe(m, -400, 200, "Farbe", (1.0, 0.7, 0.3))
    st = a(m, unreal.MaterialExpressionScalarParameter, -400, 320, parameter_name="Staerke", default_value=25.0)
    c1 = a(m, unreal.MaterialExpressionMultiply, -150, 200); c(col, c1, "A"); c(st, c1, "B")
    em = a(m, unreal.MaterialExpressionMultiply, 0, 100); c(pw, em, "A"); c(c1, em, "B")
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m); EAL.save_loaded_asset(m, False)

def spur():
    m = grund("M_TD_SpurFarbe")
    uv = a(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    u = a(m, unreal.MaterialExpressionComponentMask, -1000, 0, r=True, g=False, b=False, a=False); c(uv, u)
    u2 = a(m, unreal.MaterialExpressionMultiply, -850, 0, const_b=2.0); c(u, u2, "A")
    um = a(m, unreal.MaterialExpressionSubtract, -700, 0, const_b=1.0); c(u2, um, "A")
    ab = a(m, unreal.MaterialExpressionAbs, -550, 0); c(um, ab)
    rest = a(m, unreal.MaterialExpressionOneMinus, -420, 0); c(ab, rest)
    kern = a(m, unreal.MaterialExpressionPower, -280, 0, const_exponent=2.0); c(rest, kern, "Base")
    col = farbe(m, -420, 200, "Farbe", (1.0, 0.8, 0.3))
    st = a(m, unreal.MaterialExpressionScalarParameter, -420, 320, parameter_name="Staerke", default_value=25.0)
    c1 = a(m, unreal.MaterialExpressionMultiply, -200, 220); c(col, c1, "A"); c(st, c1, "B")
    em = a(m, unreal.MaterialExpressionMultiply, 0, 100); c(kern, em, "A"); c(c1, em, "B")
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m); EAL.save_loaded_asset(m, False)

for f in (flamme, stern, spur):
    try:
        f(); log(f"{f.__name__}: angelegt")
    except Exception as e:
        log(f"FEHLER {f.__name__}: {e}")
log("ENDE")
with open(r"E:\Modding\Logs\create_effekt_farben.txt", "w", encoding="utf-8") as fh:
    fh.write("\n".join(LOG))
