# Tuerme lackierbar (0.59, Marius 04.11.2026, Skill satisfactory-design Abschnitt 3):
# Je Farbslot unserer Modelle (M_TD_PaintPrimary usw.) eine Instanz des gebackenen Fabrikmaterials des Spiels.
# Elternteil: MI_BlenderBaked (Mischer, statisch, CanBePainted, ohne Legacy-Paint) - Platzhalter im Starter Project,
# zur Laufzeit das Original. Die Instanz setzt nur Texturparameter (keine statischen Schalter -> keine eigene Shader-Permutation).
# Texturen sind 4x4 einfarbig, deshalb egal, wie die UVs der Modelle liegen:
#   Albedo (sRGB), AOMasks (linear: R = AO, G = Primaerlack, B = Sekundaerlack), ReflectionMap (linear: R = Metall, G = Rauheit, B = Leuchten)
# Danach werden in allen Modellen unter /FicsitTD die Slots mit M_TD_<Name> auf MI_TD_<Name> umgestellt.
# Leuchtende und per Code veraenderte Materialien (Emissive, StatusLight, TeslaGlow, RangeRing) bleiben.
import os, struct, zlib
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

ELTERN = "/Game/FactoryGame/Buildable/Factory/Blender/Material/MI_BlenderBaked"
ZIEL = "/FicsitTD/Materials/Factory"
QUELLE = r"E:\Modding\Export\FactoryTex"

# Slot -> (Albedo sRGB, AOMasks, ReflectionMap)
SLOTS = {
    "PaintPrimary":   ((225, 225, 225), (255, 255, 0), (255, 110, 0)),
    "PaintSecondary": ((225, 225, 225), (255, 0, 255), (255, 115, 0)),
    "Metal":          ((140, 140, 146), (255, 0, 0), (255, 95, 0)),
    "Chrome":         ((205, 205, 210), (255, 0, 0), (255, 35, 0)),
    "Brass":          ((205, 160, 70), (255, 0, 0), (255, 80, 0)),
    "Copper":         ((200, 112, 70), (255, 0, 0), (255, 85, 0)),
    "Rubber":         ((22, 22, 22), (255, 0, 0), (0, 210, 0)),
    "Hazard":         ((215, 168, 30), (255, 0, 0), (0, 140, 0)),
    "Warning":        ((170, 30, 25), (255, 0, 0), (0, 140, 0)),
}

def png(pfad, rgb, n=4):
    roh = b"".join(b"\x00" + bytes(rgb) * n for _ in range(n))
    def block(art, daten):
        return struct.pack(">I", len(daten)) + art + daten + struct.pack(">I", zlib.crc32(art + daten) & 0xFFFFFFFF)
    with open(pfad, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + block(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 2, 0, 0, 0)) + block(b"IDAT", zlib.compress(roh)) + block(b"IEND", b""))

def importiere(name, rgb, art):
    datei = os.path.join(QUELLE, name + ".png")
    png(datei, rgb)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", datei)
    task.set_editor_property("destination_path", ZIEL + "/Textures")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(f"{ZIEL}/Textures/{name}")
    if art == "farbe":
        tex.set_editor_property("srgb", True)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    elif art == "normal":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    else:
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
    return tex

os.makedirs(QUELLE, exist_ok=True)
mel = unreal.MaterialEditingLibrary
normal = importiere("T_TD_Fabrik_Normal", (128, 128, 255), "normal")
texturen = {slot: (importiere(f"T_TD_{slot}_Albedo", alb, "farbe"), importiere(f"T_TD_{slot}_AOMasks", aom, "maske"),
                   importiere(f"T_TD_{slot}_MRE", refl, "maske")) for slot, (alb, aom, refl) in SLOTS.items()}

def satz(endung, eltern_pfad):
    """Je Slot eine Instanz MI_TD_<Slot><endung> unter eltern_pfad"""
    eltern = unreal.load_asset(eltern_pfad)
    log(f"Elternteil {eltern_pfad}: {eltern}")
    ergebnis = {}
    for slot, (t_alb, t_aom, t_ref) in texturen.items():
        name = f"MI_TD_{slot}{endung}"
        pfad = f"{ZIEL}/{name}"
        mi = unreal.load_asset(pfad)   # does_asset_exist greift im Commandlet fuer Plugin-Inhalte nicht
        if mi is None:
            mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ZIEL, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, eltern)
        # alle Texturparameter des Elternteils ueberschreiben (neue und alte Namen), sonst saehe man dessen Texturen
        werte = []
        for pname, tex in (("Albedo", t_alb), ("PM_Diffuse", t_alb), ("AOMasks", t_aom), ("ReflectionMap", t_ref),
                           ("PM_SpecularMasks", t_ref), ("Normal", normal), ("PM_Normals", normal)):
            if not mel.set_material_instance_texture_parameter_value(mi, pname, tex):
                # Platzhalter-Eltern kennen die Parameter nicht: Wert direkt eintragen
                tpv = unreal.TextureParameterValue()
                info = unreal.MaterialParameterInfo()
                info.set_editor_property("name", pname)
                tpv.set_editor_property("parameter_info", info)
                tpv.set_editor_property("parameter_value", tex)
                werte.append(tpv)
        if werte:
            namen = {str(w.get_editor_property("parameter_info").get_editor_property("name")) for w in werte}
            alt = [w for w in mi.get_editor_property("texture_parameter_values") if str(w.get_editor_property("parameter_info").get_editor_property("name")) not in namen]
            mi.set_editor_property("texture_parameter_values", alt + werte)
        unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
        gelesen = [str(w.get_editor_property("parameter_info").get_editor_property("name")) for w in mi.get_editor_property("texture_parameter_values")]
        log(f"{name}: Eltern {mi.get_editor_property('parent').get_name() if mi.get_editor_property('parent') else None}, {len(gelesen)} Texturparameter")
        ergebnis[f"M_TD_{slot}"] = mi
    return ergebnis

# Sockel laufen als Instanz (FGColoredInstanceMeshProxy, Farbe je Instanz); Kopf, Arm, Kern-Aufbau sind normale Komponenten
# und brauchen ein Elternteil mit bUsePrimitiveCustomData (Farbe aus den Primitivdaten der Komponente).
instanzen = satz("", ELTERN)
komponenten = satz("_K", "/Game/FactoryGame/Buildable/Factory/BlueprintDesigner/Material/MI_BlueprintDesigner_Computer_01")
originale = {}
for slot in SLOTS:
    originale[f"MI_TD_{slot}"] = originale[f"MI_TD_{slot}_K"] = unreal.load_asset(f"/FicsitTD/Buildings/BoltTurret/Materials/M_TD_{slot}")

# Modelle umstellen. list_assets und die Asset Registry sehen Plugin-Inhalte im Commandlet nicht zuverlaessig -> Dateisystem
INHALT = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"
def pakete(unterordner=""):
    for wurzel, _, dateien in os.walk(os.path.join(INHALT, unterordner)):
        for d in dateien:
            if d.endswith(".uasset"):
                rel = os.path.relpath(os.path.join(wurzel, d[:-7]), INHALT).replace(os.sep, "/")
                yield "/FicsitTD/" + rel
geaendert = 0
for pfad in pakete():
    if "/SM_" not in pfad:
        continue
    mesh = unreal.load_asset(pfad)
    if not isinstance(mesh, unreal.StaticMesh):
        continue
    slots = mesh.get_editor_property("static_materials")
    wechsel = []
    effekt = "/Effects/" in pfad          # kein Gebaeude, bekommt keine Lackdaten -> eigene Materialien behalten
    ziel = instanzen if pfad.endswith("_Base") else komponenten
    for i, s in enumerate(slots):
        m = s.get_editor_property("material_interface")
        name = m.get_name() if m else ""
        if effekt and name in originale:
            mesh.set_material(i, originale[name])
            wechsel.append(f"{name} -> zurueck")
        elif not effekt:
            schluessel = name.replace("MI_TD_", "M_TD_").replace("_K", "") if name.startswith("MI_TD_") else name
            if schluessel in ziel and ziel[schluessel] != m:
                mesh.set_material(i, ziel[schluessel])
                wechsel.append(f"{name} -> {ziel[schluessel].get_name()}")
    if wechsel:
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        geaendert += 1
        log(f"{mesh.get_name()}: {wechsel}")
log(f"Modelle umgestellt: {geaendert}")

# Faerbbarkeit der Gebaeude pruefen (nur Log)
for pfad in pakete("Buildings"):
    if "/Build_TD_" not in pfad:
        continue
    bp = unreal.load_asset(pfad)
    try:
        cdo = unreal.get_default_object(bp.generated_class())
    except Exception as e:
        continue
    werte = {}
    for prop in ("mAllowColoring", "mShouldApplyCustomizationData"):
        try:
            werte[prop] = cdo.get_editor_property(prop)
        except Exception as e:
            werte[prop] = f"nicht lesbar ({type(e).__name__})"
    log(f"{bp.get_name()}: {werte}")
log("ENDE")
with open(r"E:\Modding\Logs\create_factory_materials.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
