# Baut die Turm-Bauplaene (Kopien des Konstruktors, create_building.py) auf die eigenen Modelle um:
# Sockelmodell auf den Instanz-Proxy, Konstruktor-Animation/Leiter/Standbeine/Betriebsleuchte entfernen, Ausgang versteckt in den Sockel,
# Foerderband-Eingang an die Rueckseite (nur Tuerme mit Bandmunition), Stromanschluss auf den Isolator, Modelle fuer Drehteil/Kanzel.
# Effekt-Modelle (Muendungsfeuer, Blitz, Flamme) laedt der Code zur Laufzeit, damit das Hologramm sie nicht zeigt.
# Nach create_building.py ausfuehren.
import os
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

def mesh(turm, teil):
    return f"/FicsitTD/Buildings/{turm}/Mesh/SM_TD_{turm}_{teil}"

# Turm -> Komponentenname -> Modell; Strom in MW; Band-Eingang ja/nein
TUERME = {
    "BoltTurret": dict(modelle={"TurretYaw": mesh("BoltTurret", "Yaw"), "TurretHead": mesh("BoltTurret", "Head")}, strom=8.0, band=True),
    "GunTurret": dict(modelle={"TurretYaw": mesh("GunTurret", "Yaw"), "TurretHead": mesh("GunTurret", "Head"),
                               "Laufbuendel": mesh("GunTurret", "Barrels")}, strom=12.0, band=True),
    # Teslaspule: Spule steht fest in TurretYaw, keine Kanzel; Strom fliesst nur beim Aufladen (= Produzieren)
    "TeslaCoil": dict(modelle={"TurretYaw": mesh("TeslaCoil", "Coil")}, strom=75.0, band=False),
    # Flammenwerfer: Treibstoff ueber PipeInput (C++-Komponente) hinten am Sockel
    "FlameTurret": dict(modelle={"TurretYaw": mesh("FlameTurret", "Yaw"), "TurretHead": mesh("FlameTurret", "Head")}, strom=6.0, band=False),
    "NobeliskLauncher": dict(modelle={"TurretYaw": mesh("NobeliskLauncher", "Yaw"), "TurretHead": mesh("NobeliskLauncher", "Head")}, strom=20.0, band=True),
    "Barrier": dict(modelle={}, strom=None, band=False),
    "RepairStation": dict(modelle={"TurretYaw": mesh("RepairStation", "Yaw")}, strom=10.0, band=True),
    # Abwehrkern: kein Strom, keine Anschluesse
    "DefenseCore": dict(modelle={"KernAufbau": mesh("DefenseCore", "Top"), "KernKristall": mesh("DefenseCore", "Core")}, strom=None, band=False),
    # 0.81 passive Bauten P1: Tor (Aussichtsturm-Kopie, Teile laedt der Code), Lockfeuer und Oelschlick (Konstruktor-Kopien)
    "Gate": dict(modelle={}, strom=None, band=False, ohne_gemeinsam=True),
    "LureBeacon": dict(modelle={"TurretYaw": mesh("LureBeacon", "Horn")}, strom=5.0, band=True,
                       eingang=(-102.0, 0.0, 45.0), strom_ort=(90.0, -90.0, 92.0)),
    "SlickPlate": dict(modelle={"TurretYaw": mesh("SlickPlate", "Valve")}, strom=2.0, band=False,
                       strom_ort=(430.0, -150.0, 58.0), ausgang=(0.0, 0.0, 15.0)),
    # 0.83 Bauten P2/P3: Lage der Anschluesse aus den Blender-Skripten (Stromdose, Band-Eingang hinten -X)
    "ConductorWall": dict(modelle={"TurretYaw": mesh("ConductorWall", "Door0")}, strom=2.0, band=False,
                          strom_ort=(330.0, -45.0, 60.0), ausgang=(0.0, 0.0, 40.0)),
    "RadioMast": dict(modelle={"TurretYaw": mesh("RadioMast", "Dish")}, strom=15.0, band=False,
                      strom_ort=(119.0, -30.0, 70.0), ausgang=(0.0, 0.0, 40.0)),
    "ShieldGenerator": dict(modelle={"TurretYaw": mesh("ShieldGenerator", "Ring0")}, strom=60.0, band=False,
                            strom_ort=(250.0, -250.0, 70.0), ausgang=(0.0, 0.0, 30.0)),
    "FlakTurret": dict(modelle={"TurretYaw": mesh("FlakTurret", "Yaw"), "TurretHead": mesh("FlakTurret", "Pitch")}, strom=25.0, band=True),
    "FieldProjector": dict(modelle={"TurretYaw": mesh("FieldProjector", "Core")}, strom=80.0, band=True),
    "SingularityLauncher": dict(modelle={"TurretYaw": mesh("SingularityLauncher", "Yaw"), "TurretHead": mesh("SingularityLauncher", "Pitch")},
                                strom=150.0, band=True, eingang=(-336.0, 0.0, 125.0), strom_ort=(250.0, -280.0, 120.0)),
    # Lager: Kugel laedt der Code, Band-Eingang am Kern-Anschluss hinten, Ausgang versteckt
    "DimensionalDepot": dict(modelle={}, strom=None, band=True, ohne_gemeinsam=True, eingang=(-220.0, 0.0, 70.0), ausgang=(0.0, 0.0, 40.0)),
}
_NUR = [n for n in os.environ.get("FICSITTD_NUR", "").split(",") if n]
if _NUR:
    TUERME = {k: v for k, v in TUERME.items() if k in _NUR}
GEMEINSAM = {
    "RangeRing": "/FicsitTD/Buildings/BoltTurret/Mesh/SM_TD_RangeRing",     # im Bauplan sichtbar -> Hologramm zeigt die Reichweite
    "StatusLampe": "/FicsitTD/Buildings/BoltTurret/Mesh/SM_TD_StatusLight",
}

def richte_ein(turm, cfg):
    bp = unreal.load_asset(f"/FicsitTD/Buildings/{turm}/Build_TD_{turm}")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    root_handle = handles[0]
    modelle = dict({} if cfg.get("ohne_gemeinsam") else GEMEINSAM, **cfg["modelle"])
    weg = []
    for h in handles:
        obj = lib.get_object(lib.get_data(h))
        if obj is None:
            continue
        n, c = obj.get_name(), obj.get_class().get_name()
        name = n.replace("_GEN_VARIABLE", "")
        if name in modelle:
            obj.set_editor_property("static_mesh", unreal.load_asset(modelle[name]))
            log(f"  {name}: {modelle[name].rsplit('/', 1)[1]}")
        elif c == "FGColoredInstanceMeshProxy":
            obj.set_editor_property("static_mesh", unreal.load_asset(mesh(turm, "Base")))
            log(f"  Sockel: {mesh(turm, 'Base').rsplit('/', 1)[1]}")
        elif c == "FGFactoryConnectionComponent" and n.startswith("Input"):
            if cfg["band"]:
                obj.set_editor_property("relative_location", unreal.Vector(*cfg.get("eingang", (-198.0, 0.0, 100.0))))
                # Achtung: unreal.Rotator(roll, pitch, yaw) - deshalb mit Schluesselwoertern
                obj.set_editor_property("relative_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0))
                log(f"  Band-Eingang {n} -> (-198, 0, 100), Blick -X")
            else:
                weg.append((h, n))
        elif c == "FGFactoryConnectionComponent" and n.startswith("Output"):
            # Rezepte mit Produkt brauchen einen Ausgang ("not enough factory outputs"); unerreichbar mitten im Sockel
            obj.set_editor_property("relative_location", unreal.Vector(*cfg.get("ausgang", (0.0, 0.0, 60.0))))
            obj.set_editor_property("relative_rotation", unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))
        elif c == "FGPowerConnectionComponent":
            # Oben auf dem Isolator des Strom-Auslegers (wie beim Konstruktor)
            obj.set_editor_property("relative_location", unreal.Vector(*cfg.get("strom_ort", (-160.0, -160.0, 301.0))))
        elif c == "FGPipeConnectionFactory":
            try:
                obj.set_editor_property("mFluidBoxVolume", 4.0)
                log(f"  Rohr-Eingang {n}: Puffer {obj.get_editor_property('mFluidBoxVolume')} m³, Lage {obj.get_editor_property('relative_location')}")
            except Exception as e:
                log(f"  Rohr-Eingang {n}: Puffer nicht setzbar ({e})")
        elif c.startswith("BP_ProductionIndicator") or c in ("FGFactoryLegsComponent", "FGVertexAnimatedMeshComponent") or c.startswith("BP_LadderComponent"):
            weg.append((h, n))
    if weg:
        geloescht = sub.delete_subobjects(root_handle, [h for h, _ in weg], bp)
        log(f"  entfernt ({geloescht}): {[n for _, n in weg]}")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    if cfg["strom"] is not None:
        cdo.set_editor_property("mPowerConsumption", cfg["strom"])
        log(f"  Strom {cdo.get_editor_property('mPowerConsumption')} MW")
    log(f"  Hologramm {cdo.get_editor_property('mHologramClass').get_name()}")
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(h))
        if obj is not None and isinstance(obj, unreal.SceneComponent):
            m = obj.get_editor_property("static_mesh") if isinstance(obj, unreal.StaticMeshComponent) else None
            log(f"    {obj.get_name()} ({obj.get_class().get_name()}) {m.get_name() if m else ''}")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    log(f"ERFOLG {turm}")

for turm, cfg in TUERME.items():
    log(f"== {turm}")
    try:
        richte_ein(turm, cfg)
    except Exception as e:
        log(f"FEHLER {turm}: {e}")

with open(r"E:\Modding\Logs\setup_turret_blueprint.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
