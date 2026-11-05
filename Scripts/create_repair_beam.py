# Reparaturstation: gruenes Strahl-Material M_TD_RepairBeam (wie M_TD_Lightning, additiv, Parameter Staerke), Marius 02.11.2026.
# Das Modell bleibt SM_TD_Lightning; ATDRepairStation setzt das Material in BeginPlay.
# Vorher Content/Effects/M_TD_RepairBeam.uasset per Dateisystem loeschen.
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
    m = at.create_asset("M_TD_RepairBeam", FX_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    # quer zum Band hell in der Mitte: (1 - |2u - 1|)^2
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
    col = ausdruck(m, unreal.MaterialExpressionConstant3Vector, -420, 200, constant=unreal.LinearColor(0.25, 1.0, 0.35, 1.0))
    st = ausdruck(m, unreal.MaterialExpressionScalarParameter, -420, 320, parameter_name="Staerke", default_value=25.0)
    c1 = ausdruck(m, unreal.MaterialExpressionMultiply, -200, 220)
    verbinde(col, c1, "A"); verbinde(st, c1, "B")
    em2 = ausdruck(m, unreal.MaterialExpressionMultiply, 0, 100)
    verbinde(kern, em2, "A"); verbinde(c1, em2, "B")
    mel.connect_material_property(em2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    EAL.save_loaded_asset(m, False)
    log("M_TD_RepairBeam angelegt")
except Exception as e:
    log(f"FEHLER: {e}")

with open(r"E:\Modding\Logs\create_repair_beam.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
