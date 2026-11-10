# 0.9.2 Farbgebung (Marius 09.11.2026: "alle Gebaeude als 3D-Modell inkl. Farbgebung"):
# importiert die in Blender gebackenen Teile (Satisfactory\Modelle\td_backen.py) aus E:\Modding\Export\<Ordner>\Gebacken\
#   SM_TD_<Teil>.fbx                       neue UVs, ein Slot M_TD_Gebacken + Leucht-/Code-Slots (Emissive, TeslaGlow ...)
#   T_TD_<Teil>_{Albedo,AOMasks,MRE,Normal}.png
# Je Teil eine Materialinstanz MI_TD_Geb_<Teil> des Fabrikmaterials des Spiels (wie create_factory_materials.py):
#   Sockel (_Base, laufen im Instanz-Proxy) -> MI_BlenderBaked, alle anderen Teile -> MI_BlueprintDesigner_Computer_01 (Primitivdaten-Farbe)
# Das Modell wird an seinem bisherigen Ort ersetzt (gleicher Pfad -> Blueprints und Code finden es weiter).
import glob, os
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EXPORT = r"E:\Modding\Export"
INHALT = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"
TEX_ZIEL = "/FicsitTD/Materials/Gebacken"
ELTERN_INSTANZ = "/Game/FactoryGame/Buildable/Factory/Blender/Material/MI_BlenderBaked"
ELTERN_KOMPONENTE = "/Game/FactoryGame/Buildable/Factory/BlueprintDesigner/Material/MI_BlueprintDesigner_Computer_01"
ALTE_MATS = "/FicsitTD/Buildings/BoltTurret/Materials"   # Leucht- und Code-Materialien (M_TD_Emissive, M_TD_TeslaGlow ...)
NUR = os.environ.get("TD_GEBACKEN_NUR", "")              # z. B. "GunTurret" fuer eine Probe

at = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

def mesh_ort(name):
    for wurzel, _, dateien in os.walk(INHALT):
        if name + ".uasset" in dateien:
            rel = os.path.relpath(wurzel, INHALT).replace(os.sep, "/")
            return "/FicsitTD/" + rel
    return None

def importiere_textur(datei, ordner, art):
    name = os.path.splitext(os.path.basename(datei))[0]
    t = unreal.AssetImportTask()
    for k, v in (("filename", datei), ("destination_path", ordner), ("destination_name", name), ("automated", True), ("replace_existing", True), ("save", False)):
        t.set_editor_property(k, v)
    at.import_asset_tasks([t])
    tex = unreal.load_asset(f"{ordner}/{name}")
    if art == "Albedo":
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    elif art == "Normal":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("flip_green_channel", True)   # Blender (OpenGL) -> Unreal (DirectX)
    else:
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_loaded_asset(tex, False)
    return tex

def instanz(teil, ordner, texturen, eltern_pfad):
    name = f"MI_TD_Geb_{teil}"
    pfad = f"{ordner}/{name}"
    mi = unreal.load_asset(pfad)
    if mi is None:
        mi = at.create_asset(name, ordner, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset(eltern_pfad))
    werte = []
    for pname, tex in (("Albedo", texturen["Albedo"]), ("PM_Diffuse", texturen["Albedo"]), ("AOMasks", texturen["AOMasks"]), ("ReflectionMap", texturen["MRE"]),
                       ("PM_SpecularMasks", texturen["MRE"]), ("Normal", texturen["Normal"]), ("PM_Normals", texturen["Normal"])):
        if not mel.set_material_instance_texture_parameter_value(mi, pname, tex):
            tpv = unreal.TextureParameterValue()
            info = unreal.MaterialParameterInfo(); info.set_editor_property("name", pname)
            tpv.set_editor_property("parameter_info", info); tpv.set_editor_property("parameter_value", tex)
            werte.append(tpv)
    if werte:
        namen = {str(w.get_editor_property("parameter_info").get_editor_property("name")) for w in werte}
        alt = [w for w in mi.get_editor_property("texture_parameter_values") if str(w.get_editor_property("parameter_info").get_editor_property("name")) not in namen]
        mi.set_editor_property("texture_parameter_values", alt + werte)
    EAL.save_loaded_asset(mi, False)
    return mi

try:
    zaehler = 0
    for fbx in sorted(glob.glob(os.path.join(EXPORT, "*", "Gebacken", "SM_TD_*.fbx"))):
        ordner_export = os.path.basename(os.path.dirname(os.path.dirname(fbx)))
        if NUR and ordner_export not in NUR.split(","):
            continue
        name = os.path.splitext(os.path.basename(fbx))[0]
        teil = name.replace("SM_TD_", "")
        ziel = mesh_ort(name)
        if not ziel:
            # 0.9.3: neue Teile (Kanister des Himmelsturms, Stufenteile der Spezialtuerme) in den Mesh-Ordner des Turms
            ziel = f"/FicsitTD/Buildings/{ordner_export}/Mesh"
            log(f"NEU {name} -> {ziel}")
        quelle = os.path.dirname(fbx)
        tex_ordner = f"{TEX_ZIEL}/{ordner_export}"
        texturen = {k: importiere_textur(os.path.join(quelle, f"T_TD_{teil}_{k}.png"), tex_ordner, k) for k in ("Albedo", "AOMasks", "MRE", "Normal")}
        mi = instanz(teil, tex_ordner, texturen, ELTERN_INSTANZ if name.endswith("_Base") else ELTERN_KOMPONENTE)
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
        for k, v in (("filename", fbx), ("destination_path", ziel), ("destination_name", name), ("automated", True), ("replace_existing", True), ("save", True), ("options", opts)):
            t.set_editor_property(k, v)
        at.import_asset_tasks([t])
        mesh = unreal.load_asset(f"{ziel}/{name}")
        slots = []
        for i, s in enumerate(mesh.get_editor_property("static_materials")):
            slot = str(s.material_slot_name)
            if slot.startswith("M_TD_Gebacken"):
                mesh.set_material(i, mi); slots.append("gebacken")
            else:
                alt = unreal.load_asset(f"{ALTE_MATS}/{slot.split('.')[0]}")
                if alt:
                    mesh.set_material(i, alt); slots.append(slot)
                else:
                    slots.append(f"{slot}?")
        EAL.save_loaded_asset(mesh, False)
        b = mesh.get_bounding_box()
        log(f"{ziel}/{name}: {mesh.get_num_triangles(0)} Dreiecke, Slots {slots}, Box ({b.min.x:.0f},{b.min.y:.0f},{b.min.z:.0f})-({b.max.x:.0f},{b.max.y:.0f},{b.max.z:.0f})")
        zaehler += 1
    log(f"ERFOLG: {zaehler} Modelle")
except Exception as e:
    log(f"FEHLER: {e}")
with open(r"E:\Modding\Logs\import_gebacken.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
