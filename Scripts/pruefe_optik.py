# Optik-Pruefung P1 (Optik-Durchgang 16.11.2026, Satisfactory\Notizen\Optik-Durchgang-2026-11-16.md): liest alle Bauplaene Build_TD_*
# und die von ihnen genutzten Modelle, aendert NICHTS. Headless:
#   UnrealEditor-Cmd.exe FactoryGame.uproject -run=pythonscript -script=<dieses Skript> -unattended -nosplash
# Ergebnis: E:\Modding\Logs\pruefe_optik.json (fehler/hinweise je Gebaeude) und pruefe_optik.txt; Tests\Modellmasse.json (Sockelmasse
# fuer die statische Pruefung Tests\pruefe_optik_statisch.py).
#
# Geprueft:
#  U1  geerbte Nebelflaechen (mFogPlaneLocations / FogPlaneMesh) der kopierten Spielgebaeude
#  U1  verbliebene Komponenten des Spiel-Vorbilds (Betriebsleuchte, Standbeine, VAT, Leiter)
#  U4  Material-Slots ohne Material, mit Standardmaterial, Sockel-Material ohne Instanz-Freigabe
#  U2  Anschluss-Komponenten (Band, Rohr, Strom) ausserhalb der Sockel-Grundflaeche bzw. tief im Sockel
import json
import os
import unreal

INHALT = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"
TESTS = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Tests"
LOGS = r"E:\Modding\Logs"
VERBOTENE_KOMPONENTEN = ("BP_ProductionIndicator", "FGFactoryLegsComponent", "FGVertexAnimatedMeshComponent", "BP_LadderComponent")
STANDARD_MATERIALIEN = ("WorldGridMaterial", "DefaultMaterial", "DefaultDeferredDecalMaterial")
# Anschluss bis so weit ausserhalb der Sockelflaeche (Kabel/Band rastet an der Aussenseite ein) bzw. so tief innen (Wanddicke) erlaubt
AUSSEN_TOLERANZ = 100.0
INNEN_TOLERANZ = 30.0
# Ausgang der Maschinen-Kopien ist absichtlich versteckt im Sockel (Rezepte brauchen einen Ausgang, setup_turret_blueprint.py)
VERSTECKT_ERLAUBT = ("Output",)

zeilen = []
def log(m):
    zeilen.append(str(m))
    unreal.log("FICSITTD-OPTIK| " + str(m))


def bauplaene():
    for wurzel, _, dateien in os.walk(os.path.join(INHALT, "Buildings")):
        for d in sorted(dateien):
            if d.startswith("Build_TD_") and d.endswith(".uasset"):
                rel = os.path.relpath(os.path.join(wurzel, d[:-7]), INHALT).replace(os.sep, "/")
                yield "/FicsitTD/" + rel


def material_name(m):
    return m.get_name() if m else None


def pruefe_modell(mesh, ist_sockel, fehler, hinweise, geprueft):
    if mesh is None or mesh.get_path_name() in geprueft:
        return
    geprueft.add(mesh.get_path_name())
    for i, s in enumerate(mesh.get_editor_property("static_materials")):
        m = s.get_editor_property("material_interface")
        slot = str(s.get_editor_property("material_slot_name"))
        if m is None:
            fehler.append(f"{mesh.get_name()} Slot {i} ({slot}): kein Material")
            continue
        if m.get_name() in STANDARD_MATERIALIEN:
            fehler.append(f"{mesh.get_name()} Slot {i} ({slot}): Standardmaterial {m.get_name()}")
            continue
        if ist_sockel:
            basis = m.get_base_material() if hasattr(m, "get_base_material") else None
            try:
                ok = basis.get_editor_property("used_with_instanced_static_meshes") if basis else True
            except Exception:
                ok = True   # Platzhalter des Spiels: Wert nicht lesbar, im Spiel gesetzt
            if basis and not ok and not basis.get_path_name().startswith("/Game/"):
                fehler.append(f"{mesh.get_name()} Slot {i}: {m.get_name()} (Basis {basis.get_name()}) ohne used_with_instanced_static_meshes -> im Proxy schwarz")


def lage(obj):
    try:
        return obj.get_editor_property("relative_location")
    except Exception:
        return None


ergebnis = {}
masse = {}
for pfad in bauplaene():
    name = pfad.rsplit("/", 1)[1]
    bp = unreal.load_asset(pfad)
    if bp is None:
        ergebnis[name] = {"fehler": [f"Bauplan {pfad} nicht ladbar"], "hinweise": []}
        continue
    fehler, hinweise, geprueft = [], [], set()
    cdo = unreal.get_default_object(bp.generated_class())

    # U1 Nebelflaechen: liegen im Sparse-Data-Objekt (AFGBuildable::mBuildableSparseDataCDO, UFGBuildableSparseDataObject)
    try:
        sparse = cdo.get_editor_property("mBuildableSparseDataCDO")
    except Exception as e:
        sparse = None
        hinweise.append(f"mBuildableSparseDataCDO nicht lesbar: {e}")
    if sparse is not None:
        nebel = sparse.get_editor_property("mFogPlaneLocations")
        if nebel and len(nebel) > 0:
            orte = ", ".join(f"({t.translation.x:.0f}/{t.translation.y:.0f}/{t.translation.z:.0f})" for t in nebel)
            fehler.append(f"{len(nebel)} geerbte Nebelflaechen (mFogPlaneLocations): {orte}")
        if sparse.get_editor_property("FogPlaneMesh"):
            hinweise.append(f"FogPlaneMesh gesetzt: {sparse.get_editor_property('FogPlaneMesh').get_name()}")
        aussen = sparse.get_outer()
        if aussen is None or not aussen.get_path_name().startswith("/FicsitTD/"):
            fehler.append(f"Sparse-Daten gehoeren nicht dem Bauplan, sondern {aussen.get_path_name() if aussen else None} (geteilt mit dem Spiel-Vorbild)")

    # U1 Dekorations-Vorlage des Spiel-Vorbilds (AFGBuildable::mDecoratorClass): ihre Pool-Komponenten (u. a. InputFogPlane)
    # setzt das Spiel zur Laufzeit an das Gebaeude - beim Konstruktor 4 Nebelflaechen an dessen Ein-/Ausgaengen
    try:
        deko = cdo.get_editor_property("mDecoratorClass")
        if deko is not None:
            fehler.append(f"geerbte Dekorations-Vorlage mDecoratorClass = {deko.get_path_name()} (Nebelflaechen/Leuchten des Spiel-Vorbilds)")
    except Exception as e:
        hinweise.append(f"mDecoratorClass nicht lesbar: {e}")

    # Komponenten des Bauplans
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    sockel_box = None
    anschluesse = []
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(h))
        if obj is None:
            continue
        n, c = obj.get_name().replace("_GEN_VARIABLE", ""), obj.get_class().get_name()
        if any(c.startswith(v) for v in VERBOTENE_KOMPONENTEN):
            fehler.append(f"Komponente des Spiel-Vorbilds uebrig: {n} ({c})")
        if isinstance(obj, unreal.StaticMeshComponent):
            mesh = obj.get_editor_property("static_mesh")
            ist_sockel = c == "FGColoredInstanceMeshProxy"
            pruefe_modell(mesh, ist_sockel, fehler, hinweise, geprueft)
            if ist_sockel and mesh:
                box = mesh.get_bounding_box()
                sockel_box = (box.min, box.max)
        if c in ("FGFactoryConnectionComponent", "FGPipeConnectionFactory", "FGPowerConnectionComponent", "FGPipeConnectionComponent") or c.startswith("TD") and "Connection" in c:
            ort = lage(obj)
            if ort is not None:
                anschluesse.append((n, c, ort))

    # U2 Anschluesse gegen die Sockel-Grundflaeche
    if sockel_box:
        mn, mx = sockel_box
        masse[name] = {"sockel_min": [round(mn.x), round(mn.y), round(mn.z)], "sockel_max": [round(mx.x), round(mx.y), round(mx.z)],
                       "anschluesse": {n: [round(o.x), round(o.y), round(o.z)] for n, _, o in anschluesse}}
        for n, c, o in anschluesse:
            aussen = max(mn.x - o.x, o.x - mx.x, mn.y - o.y, o.y - mx.y, 0.0)
            innen = min(o.x - mn.x, mx.x - o.x, o.y - mn.y, mx.y - o.y)
            if aussen > AUSSEN_TOLERANZ:
                fehler.append(f"Anschluss {n} ({c}) {aussen:.0f} cm ausserhalb der Sockelflaeche bei ({o.x:.0f}/{o.y:.0f}/{o.z:.0f})")
            elif aussen == 0.0 and innen > INNEN_TOLERANZ and not n.startswith(VERSTECKT_ERLAUBT):
                fehler.append(f"Anschluss {n} ({c}) {innen:.0f} cm innerhalb der Grundflaeche bei ({o.x:.0f}/{o.y:.0f}/{o.z:.0f}), nicht erreichbar")
            if o.z > mx.z + 50.0:
                hinweise.append(f"Anschluss {n} {o.z - mx.z:.0f} cm ueber der Sockeloberkante")
    else:
        hinweise.append("kein Sockel-Proxy (FGColoredInstanceMeshProxy) - Anschlusspruefung entfaellt")

    ergebnis[name] = {"fehler": fehler, "hinweise": hinweise}
    log(f"== {name}: {len(fehler)} Fehler, {len(hinweise)} Hinweise")
    for f in fehler:
        log(f"   FEHLER {f}")
    for h in hinweise:
        log(f"   Hinweis {h}")

summe = sum(len(v["fehler"]) for v in ergebnis.values())
log(f"ENDE {len(ergebnis)} Bauplaene, {summe} Fehler")
os.makedirs(LOGS, exist_ok=True)
with open(os.path.join(LOGS, "pruefe_optik.json"), "w", encoding="utf-8") as f:
    json.dump({"fehler_gesamt": summe, "gebaeude": ergebnis}, f, ensure_ascii=False, indent=1)
with open(os.path.join(LOGS, "pruefe_optik.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(zeilen))
with open(os.path.join(TESTS, "Modellmasse.json"), "w", encoding="utf-8") as f:
    json.dump(masse, f, ensure_ascii=False, indent=1)
