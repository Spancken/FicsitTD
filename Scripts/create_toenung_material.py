# T4c Faerbung: Toenungs-Material M_TD_Toenung fuer eine zweite, mitlaufende Kopie des Kreaturmodells (Leader-Pose).
# Blend Modulate, unbeleuchtet: multipliziert das Bild darunter mit lerp(weiss, Farbe, Staerke) - faerbt und dunkelt
# die Haut ein, ohne die Original-Texturen zu veraendern. Parameter: Farbe (Vektor), Staerke (0..1).
# Vorher M_TD_Toenung (.uasset) per Dateisystem loeschen.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
FX_DIR = "/FicsitTD/Effects"

def ausdruck(m, klasse, x, y, **werte):
    e = mel.create_material_expression(m, klasse, x, y)
    for k, v in werte.items():
        e.set_editor_property(k, v)
    return e

def verbinde(a, b, eingang):
    mel.connect_material_expressions(a, "", b, eingang)

try:
    m = at.create_asset("M_TD_Toenung", FX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MODULATE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("used_with_skeletal_mesh", True)
    weiss = ausdruck(m, unreal.MaterialExpressionConstant3Vector, -700, 0, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    farbe = ausdruck(m, unreal.MaterialExpressionVectorParameter, -700, 150, parameter_name="Farbe", default_value=unreal.LinearColor(0.75, 0.35, 0.25, 1.0))
    staerke = ausdruck(m, unreal.MaterialExpressionScalarParameter, -700, 320, parameter_name="Staerke", default_value=0.5)
    mix = ausdruck(m, unreal.MaterialExpressionLinearInterpolate, -400, 120)
    verbinde(weiss, mix, "A"); verbinde(farbe, mix, "B"); verbinde(staerke, mix, "Alpha")
    mel.connect_material_property(mix, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    EAL.save_loaded_asset(m, False)
    log("M_TD_Toenung angelegt")
    log("ERFOLG")
except Exception as e:
    import traceback
    log(f"FEHLER: {e}\n{traceback.format_exc()}")

with open(r"E:\Modding\Logs\create_toenung_material.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
