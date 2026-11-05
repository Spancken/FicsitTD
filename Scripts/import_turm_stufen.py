# Turm-Modelle mit Stufen-Anbauteilen (0.62, Marius 04.11.2026: Stufe am Turm sichtbar).
# Importiert alle E:\Modding\Export\<Ordner>\SM_TD_<Turm>_*.fbx nach /FicsitTD/Buildings/<Ordner>/Mesh:
#   SM_TD_<Turm>_Base / _Yaw / _Head / _Barrels  = Grundmodell (Stufe 1), je relativ zum Drehpunkt modelliert
#   SM_TD_<Turm>_<Teil>_S2 .. _S5                = Anbauteile, ATDTurretBase blendet sie ab der Stufe ein
# Quelle Gewehrturm: Satisfactory\Modelle\TD_GunTurret_Stufen.blend. Danach create_factory_materials.py und Build-Mod.ps1.
import glob, os
import unreal

# (Exportordner, Name im Mesh, nur Anbauteile _S<n>: Grundmodell bleibt wie es ist)
TUERME = [("GunTurret", "GunTurret", False), ("BoltTurret", "BoltTurret", True), ("NobeliskLauncher", "NobeliskLauncher", False), ("TeslaCoil", "TeslaCoil", False)]   # 0.64: Level-5-Teile
# frueher: ("GunTurret", "GunTurret", False), ("BoltTurret", "BoltTurret", True)
LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

at = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
MAT_DIR = "/FicsitTD/Buildings/BoltTurret/Materials"
MATS = ["M_TD_PaintPrimary", "M_TD_PaintSecondary", "M_TD_Metal", "M_TD_Hazard", "M_TD_Emissive", "M_TD_Rubber", "M_TD_Chrome",
        "M_TD_Brass", "M_TD_Copper", "M_TD_StatusLight", "M_TD_Warning", "M_TD_TeslaGlow"]

try:
    mats = {n: unreal.load_asset(f"{MAT_DIR}/{n}") for n in MATS}
    for ordner, turm, nur_stufen in TUERME:
        ziel = f"/FicsitTD/Buildings/{ordner}/Mesh"
        for datei in sorted(glob.glob(rf"E:\Modding\Export\{ordner}\SM_TD_{turm}_*_S?.fbx" if nur_stufen else rf"E:\Modding\Export\{ordner}\SM_TD_{turm}_*.fbx")):
            name = os.path.splitext(os.path.basename(datei))[0]
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
            for k, v in (("filename", datei), ("destination_path", ziel), ("destination_name", name),
                         ("automated", True), ("replace_existing", True), ("save", True), ("options", opts)):
                t.set_editor_property(k, v)
            at.import_asset_tasks([t])
            mesh = unreal.load_asset(f"{ziel}/{name}")
            for i, s in enumerate(mesh.get_editor_property("static_materials")):
                key = next((k for k in mats if str(s.material_slot_name).startswith(k)), None)
                if key and mats[key]:
                    mesh.set_material(i, mats[key])
                else:
                    log(f"  {name}: Slot {s.material_slot_name} ohne Material")
            b = mesh.get_bounding_box()
            log(f"{name}: Dreiecke={mesh.get_num_triangles(0)} min=({b.min.x:.0f},{b.min.y:.0f},{b.min.z:.0f}) max=({b.max.x:.0f},{b.max.y:.0f},{b.max.z:.0f})")
            EAL.save_loaded_asset(mesh, False)
        # Icon an derselben Stelle ersetzen
        icon = rf"E:\Modding\Export\Icons\Icon_TD_{turm}_512.png"
        if os.path.exists(icon) and not nur_stufen:
            task = unreal.AssetImportTask()
            for k, v in (("filename", icon), ("destination_path", "/FicsitTD/UI/Icons"), ("destination_name", f"Icon_TD_{turm}_512"),
                         ("automated", True), ("replace_existing", True), ("save", False)):
                task.set_editor_property(k, v)
            at.import_asset_tasks([task])
            tex = unreal.load_asset(f"/FicsitTD/UI/Icons/Icon_TD_{turm}_512")
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property("srgb", True)
            EAL.save_loaded_asset(tex, False)
            log(f"Icon {turm}: {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}")
    log("ERFOLG")
except Exception as e:
    log(f"FEHLER: {e}")
with open(r"E:\Modding\Logs\import_turm_stufen.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
