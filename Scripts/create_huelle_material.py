# T4 Elite-/Boss-Optik: Huellen-Material M_TD_Huelle als Overlay fuer Kreaturen (USkeletalMeshComponent::SetOverlayMaterial).
# Additiv, unbeleuchtet, Fresnel-Rand: leuchtet an den Kanten des Modells, die Mitte bleibt sichtbar.
# Parameter: Farbe (Vektor), Staerke (Skalar, Code laesst den Boss pulsieren), Rand (Fresnel-Exponent).
# Vorher M_TD_Huelle (.uasset) per Dateisystem loeschen.
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
    m = at.create_asset("M_TD_Huelle", FX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("used_with_skeletal_mesh", True)
    rand = ausdruck(m, unreal.MaterialExpressionScalarParameter, -900, 0, parameter_name="Rand", default_value=2.5)
    fres = ausdruck(m, unreal.MaterialExpressionFresnel, -650, 0, base_reflect_fraction=0.08)
    verbinde(rand, fres, "ExponentIn")
    farbe = ausdruck(m, unreal.MaterialExpressionVectorParameter, -900, 200, parameter_name="Farbe", default_value=unreal.LinearColor(0.55, 0.4, 1.0, 1.0))
    staerke = ausdruck(m, unreal.MaterialExpressionScalarParameter, -900, 400, parameter_name="Staerke", default_value=3.0)
    c1 = ausdruck(m, unreal.MaterialExpressionMultiply, -600, 260)
    verbinde(farbe, c1, "A"); verbinde(staerke, c1, "B")
    em = ausdruck(m, unreal.MaterialExpressionMultiply, -350, 100)
    verbinde(fres, em, "A"); verbinde(c1, em, "B")
    mel.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    EAL.save_loaded_asset(m, False)
    log("M_TD_Huelle angelegt")
    log("ERFOLG")
except Exception as e:
    import traceback
    log(f"FEHLER: {e}\n{traceback.format_exc()}")

with open(r"E:\Modding\Logs\create_huelle_material.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
