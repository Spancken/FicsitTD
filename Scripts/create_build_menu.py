# Eigene Baumenue-Kategorie "Verteidigung" mit Abschnitten "Mauern" und "Tuerme", Icons aus Blender (E:\Modding\Export\Icons).
# Nach create_building.py ausfuehren (setzt Kategorie/Abschnitt/Icons in den Beschreibungen).
# Vorher die alten .uasset unter Content\UI per Dateisystem loeschen.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
UI = "/FicsitTD/UI"
BM = "/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories"

def cdo(bp):
    return unreal.get_default_object(bp.generated_class())

def import_icon(name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", rf"E:\Modding\Export\Icons\{name}.png")
    task.set_editor_property("destination_path", f"{UI}/Icons")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    at.import_asset_tasks([task])
    tex = unreal.load_asset(f"{UI}/Icons/{name}")
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("srgb", True)
    EAL.save_loaded_asset(tex, False)
    log(f"Icon {name}: {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}")
    return tex

def category(src, name, anzeige, prio, icon=None):
    bp = EAL.duplicate_asset(src, f"{UI}/{name}")
    if bp is None:
        raise RuntimeError(f"Duplizieren fehlgeschlagen: {src}")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    c = cdo(bp)
    c.set_editor_property("mDisplayName", anzeige)
    c.set_editor_property("mMenuPriority", prio)
    if icon is not None:
        brush = c.get_editor_property("mCategoryIcon")
        brush.set_editor_property("resource_object", icon)
        c.set_editor_property("mCategoryIcon", brush)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_loaded_asset(bp, False)
    log(f"{name}: '{c.get_editor_property('mDisplayName')}' Prioritaet {prio}")
    return bp

try:
    EAL.make_directory(f"{UI}/Icons")
    icon_turm = import_icon("Icon_TD_BoltTurret_512")
    icon_mauer = import_icon("Icon_TD_DefenseWall_512")
    icon_kat = import_icon("Icon_TD_Category_256")
    icons = {n: import_icon(f"Icon_TD_{n}_512") for n in ("GunTurret", "TeslaCoil", "FlameTurret", "DefenseCore", "NobeliskLauncher", "Barrier", "RepairStation")}

    kat = category(f"{BM}/BC_Organization", "BC_TD_Defense", "Verteidigung", 4.5, icon_kat)
    sc_mauern = category(f"{BM}/Sub_Organisation/SC_Storage", "SC_TD_Walls", "Mauern", 1.0)
    sc_tuerme = category(f"{BM}/Sub_Organisation/SC_Storage", "SC_TD_Turrets", "Türme", 2.0)

    for desc_path, sub, icon, prio in [
        ("/FicsitTD/Buildings/DefenseWall/Desc_TD_DefenseWall", sc_mauern, icon_mauer, 1.0),
        ("/FicsitTD/Buildings/Barrier/Desc_TD_Barrier", sc_mauern, icons["Barrier"], 2.0),
        ("/FicsitTD/Buildings/RepairStation/Desc_TD_RepairStation", sc_tuerme, icons["RepairStation"], 9.0),
        ("/FicsitTD/Buildings/BoltTurret/Desc_TD_BoltTurret", sc_tuerme, icon_turm, 1.0),
        ("/FicsitTD/Buildings/DefenseCore/Desc_TD_DefenseCore", sc_tuerme, icons["DefenseCore"], 0.5),
        ("/FicsitTD/Buildings/GunTurret/Desc_TD_GunTurret", sc_tuerme, icons["GunTurret"], 2.0),
        ("/FicsitTD/Buildings/TeslaCoil/Desc_TD_TeslaCoil", sc_tuerme, icons["TeslaCoil"], 3.0),
        ("/FicsitTD/Buildings/FlameTurret/Desc_TD_FlameTurret", sc_tuerme, icons["FlameTurret"], 4.0),
        ("/FicsitTD/Buildings/NobeliskLauncher/Desc_TD_NobeliskLauncher", sc_tuerme, icons["NobeliskLauncher"], 5.0),
    ]:
        desc = unreal.load_asset(desc_path)
        d = cdo(desc)
        d.set_editor_property("mCategory", kat.generated_class())
        d.set_editor_property("mSubCategories", [sub.generated_class()])
        d.set_editor_property("mMenuPriority", prio)
        d.set_editor_property("mSmallIcon", icon)
        d.set_editor_property("mPersistentBigIcon", icon)
        unreal.BlueprintEditorLibrary.compile_blueprint(desc)
        EAL.save_loaded_asset(desc, False)
        log(f"{desc_path.split('/')[-1]}: Kategorie Verteidigung / {cdo(sub).get_editor_property('mDisplayName')}, Icon {icon.get_name()}")
    log("ERFOLG")
except Exception as e:
    log(f"FEHLER: {e}")

with open(r"E:\Modding\Logs\create_build_menu.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
