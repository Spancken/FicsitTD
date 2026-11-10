# 0.81 passive Bauten P1 (Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07): Tor, Lockfeuer, Oelschlick-Feld.
# Schritt "modelle" (vor create_building.py): FBX aus E:\Modding\Export\<Bau> importieren, fehlende Materialien anlegen.
# Schritt "menue" (nach create_building.py): Icons importieren, Beschreibungen in die Baumenue-Kategorie Verteidigung haengen.
# Aufruf: FICSITTD_SCHRITT=modelle|menue, UnrealEditor-Cmd -run=pythonscript -script=create_p1_bauten.py
import os
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EXPORT = r"E:\Modding\Export"
MAT_DIR = "/FicsitTD/Buildings/BoltTurret/Materials"
INHALT = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"
BAUTEN = {
    "Gate": ["Base", "LeafL", "LeafR", "BeaconL", "BeaconR"],
    "LureBeacon": ["Base", "Horn", "Beacon", "Shutter0", "Shutter1", "Shutter2", "Shutter3"],
    "SlickPlate": ["Base", "Valve"],
    # 0.83 P2/P3
    "ConductorWall": ["Base", "Door0", "Door1"],
    "RadioMast": ["Base", "Dish", "Antenna", "SmallDish"],
    "ShieldGenerator": ["Base", "Ring0", "Ring1", "Ring2", "Cover0", "Cover1", "Cover2", "Cover3"],
    "FlakTurret": ["Base", "Yaw", "Pitch", "Radar"],
    "FieldProjector": ["Base", "Core", "Ring0", "Ring1", "Ring2"],
    "SingularityLauncher": ["Base", "Yaw", "Pitch"],
    "DimensionalDepot": ["Base", "Sphere"],
    "DronePort": ["Base", "Tower", "Drone"],   # 0.9.3
}
# 0.83: FICSITTD_NUR=ConductorWall,... begrenzt Import und Menue auf diese Bauten
_NUR = [n for n in os.environ.get("FICSITTD_NUR", "").split(",") if n]
if _NUR:
    BAUTEN = {k: v for k, v in BAUTEN.items() if k in _NUR}
# Farbe linear, Metallic, Rauheit, Leuchtstaerke (wie import_tower_meshes.py)
NEUE_MATERIALIEN = {
    "M_TD_Oil":     ((0.012, 0.012, 0.015), 0.0, 0.04, 0.0),
    "M_TD_Ceramic": ((0.85, 0.85, 0.82), 0.0, 0.35, 0.0),
    "M_TD_Glass":   ((0.55, 0.75, 0.95), 0.0, 0.05, 1.5),
    "M_TD_SAM":     ((0.45, 0.15, 0.85), 0.0, 0.25, 8.0),
}
at = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

def vorhanden(spielpfad):
    return os.path.exists(os.path.join(INHALT, *spielpfad.replace("/FicsitTD/", "").split("/")) + ".uasset")

def ausdruck(m, klasse, x, y, **werte):
    e = mel.create_material_expression(m, klasse, x, y)
    for k, v in werte.items():
        e.set_editor_property(k, v)
    return e

def turm_material(name, rgb, metal, rough, emit):
    m = at.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
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
    log(f"Material {name} angelegt")
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

def modelle():
    mats = {}
    for n in ("M_TD_PaintPrimary", "M_TD_PaintSecondary", "M_TD_Metal", "M_TD_Hazard", "M_TD_Emissive", "M_TD_Rubber",
              "M_TD_Copper", "M_TD_Chrome", "M_TD_TeslaGlow", "M_TD_Warning", "M_TD_Brass"):
        mats[n] = unreal.load_asset(f"{MAT_DIR}/{n}")
    for n, v in NEUE_MATERIALIEN.items():
        mats[n] = unreal.load_asset(f"{MAT_DIR}/{n}") if vorhanden(f"{MAT_DIR}/{n}") else turm_material(n, *v)
    log(f"Materialien: {[k for k, v in mats.items() if v]}")
    for bau, teile in BAUTEN.items():
        for teil in teile:
            name = f"SM_TD_{bau}_{teil}"
            mesh = importiere(rf"{EXPORT}\{bau}\{name}.fbx", f"/FicsitTD/Buildings/{bau}/Mesh", name)
            zuordnen(mesh, mats) if mesh else log(f"FEHLER: {name} nicht importiert")

def menue():
    def cdo(bp):
        return unreal.get_default_object(bp.generated_class())
    def import_icon(name):
        task = unreal.AssetImportTask()
        for k, v in (("filename", rf"{EXPORT}\Icons\{name}.png"), ("destination_path", "/FicsitTD/UI/Icons"), ("destination_name", name),
                     ("automated", True), ("replace_existing", True), ("save", False)):
            task.set_editor_property(k, v)
        at.import_asset_tasks([task])
        tex = unreal.load_asset(f"/FicsitTD/UI/Icons/{name}")
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        tex.set_editor_property("srgb", True)
        EAL.save_loaded_asset(tex, False)
        log(f"Icon {name}: {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}")
        return tex
    kat = unreal.load_asset("/FicsitTD/UI/BC_TD_Defense")
    sc_mauern = unreal.load_asset("/FicsitTD/UI/SC_TD_Walls")
    sc_tuerme = unreal.load_asset("/FicsitTD/UI/SC_TD_Turrets")
    MENUE = (("Gate", sc_mauern, 1.5), ("LureBeacon", sc_mauern, 3.0), ("SlickPlate", sc_mauern, 4.0),
             ("ConductorWall", sc_mauern, 1.7), ("RadioMast", sc_mauern, 5.0), ("ShieldGenerator", sc_mauern, 6.0),
             ("DimensionalDepot", sc_mauern, 7.0), ("DronePort", sc_mauern, 7.5), ("FieldProjector", sc_mauern, 8.0),
             ("FlakTurret", sc_tuerme, 20.0), ("SingularityLauncher", sc_tuerme, 21.0))
    for bau, sub, prio in MENUE:
        if bau not in BAUTEN:
            continue
        icon = import_icon(f"Icon_TD_{bau}_512")
        desc = unreal.load_asset(f"/FicsitTD/Buildings/{bau}/Desc_TD_{bau}")
        d = cdo(desc)
        d.set_editor_property("mCategory", kat.generated_class())
        d.set_editor_property("mSubCategories", [sub.generated_class()])
        d.set_editor_property("mMenuPriority", prio)
        d.set_editor_property("mSmallIcon", icon)
        d.set_editor_property("mPersistentBigIcon", icon)
        unreal.BlueprintEditorLibrary.compile_blueprint(desc)
        EAL.save_loaded_asset(desc, False)
        log(f"Desc_TD_{bau}: Verteidigung / {cdo(sub).get_editor_property('mDisplayName')}, Prioritaet {prio}")

schritt = os.environ.get("FICSITTD_SCHRITT", "modelle")
try:
    modelle() if schritt == "modelle" else menue()
    log(f"ERFOLG {schritt}")
except Exception as e:
    import traceback
    log(f"FEHLER {schritt}: {e}\n{traceback.format_exc()}")

with open(rf"E:\Modding\Logs\create_p1_bauten_{schritt}.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
