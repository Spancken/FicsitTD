# Rezepte aller Tuerme: Zutat (Munition) -> 1 "Turm-Aktion" (Wegwerf-Produkt, das der Turm sofort leert), hergestellt im Turm.
# Ein abgeschlossener Zyklus = eine Aktion des Turms (ATDTurretBase::Feuern): Bolzen = 1 Schuss, Gewehr = 1 Magazin,
# Teslaspule = 1 Entladung (keine Zutat, Zyklus = Aufladen), Flammenwerfer = 1 s Flamme (Fluessigkeit in Litern, 1000 = 1 m³).
# Test 0.7.1: Rezepte ohne Produkt erscheinen nicht in der Rezeptauswahl; Test 0.8.0: das Produkt braucht einen Ausgang (Output0).
# Nach create_building.py ausfuehren; alte Recipe_TD_*-Rezepte und Desc_TD_BoltShot.uasset vorher per Dateisystem loeschen.
import os
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
# Alternativrezept als Vorlage: hat bereits mDisplayNameOverride=True (per Python nicht setzbar), damit der eigene Name erscheint
VORLAGE = "/Game/FactoryGame/Recipes/AlternateRecipes/New_Update3/Recipe_Alternate_SteelRod"
SCHUSS_VORLAGE = "/Game/FactoryGame/Resource/Parts/IronRod/Desc_IronRod"
SCHUSS_ICON_VON = "/Game/FactoryGame/Resource/Parts/SpikedRebar/Desc_SpikedRebar.Desc_SpikedRebar_C"
SCHUSS = "/FicsitTD/Buildings/BoltTurret/Desc_TD_BoltShot"
G = "/Game/FactoryGame"
REBAR = f"{G}/Equipment/RebarGun/Ammo"
RIFLE = f"{G}/Equipment/Rifle/Ammo"
PARTS = f"{G}/Resource/Parts"

def item(pfad):
    return f"{pfad}.{pfad.rsplit('/', 1)[1]}_C"

# Turm -> [(Rezeptname, Zutat oder None, Menge, Anzeigename, Dauer s, Prioritaet)]
REZEPTE = {
    "BoltTurret": [
        ("Shoot_SpikedRebar", item(f"{PARTS}/SpikedRebar/Desc_SpikedRebar"), 1, "Armierungsbolzen verschießen", 1.2, 1.0),
        ("Shoot_Durchschlag", item("/FicsitTD/Items/Desc_TD_Durchschlagbolzen"), 1, "Durchschlagbolzen verschießen", 1.2, 1.5),
        ("Shoot_Stunshot", item(f"{REBAR}/Desc_Rebar_Stunshot"), 1, "Betäubungsbolzen verschießen", 1.2, 2.0),
        ("Shoot_Spreadshot", item(f"{REBAR}/Desc_Rebar_Spreadshot"), 1, "Splitterbolzen verschießen", 1.2, 3.0),
        ("Shoot_Explosive", item(f"{REBAR}/Desc_Rebar_Explosive"), 1, "Explosionsbolzen verschießen", 1.2, 4.0),
    ],
    "GunTurret": [
        ("Gun_Standard", item(f"{PARTS}/CartridgeStandard/Desc_CartridgeStandard"), 1, "Gewehrmunition laden (15 Schuss)", 0.6, 1.0),
        ("Gun_Homing", item(f"{RIFLE}/Desc_CartridgeSmartProjectile"), 1, "Zielsuchende Munition laden (20 Schuss)", 0.6, 2.0),
        ("Gun_Turbo", item(f"{RIFLE}/Desc_CartridgeChaos"), 1, "Turbo-Munition laden (75 Schuss)", 0.6, 3.0),
    ],
    "NobeliskLauncher": [
        ("Nobelisk_Explosive", item(f"{PARTS}/NobeliskExplosive/Desc_NobeliskExplosive"), 1, "Nobelisk abfeuern", 4.0, 1.0),
        ("Nobelisk_Gas", item(f"{G}/Equipment/NobeliskDetonator/Ammo/Desc_NobeliskGas"), 1, "Gas-Nobelisk abfeuern", 4.0, 2.0),
        ("Nobelisk_Pulse", item(f"{G}/Equipment/NobeliskDetonator/Ammo/Desc_NobeliskShockwave"), 1, "Puls-Nobelisk abfeuern", 4.0, 3.0),
        ("Nobelisk_Cluster", item(f"{G}/Equipment/NobeliskDetonator/Ammo/Desc_NobeliskCluster"), 1, "Streu-Nobelisk abfeuern", 4.0, 4.0),
        ("Nobelisk_Nuke", item(f"{G}/Equipment/NobeliskDetonator/Ammo/Desc_NobeliskNuke"), 1, "Nuke-Nobelisk abfeuern", 30.0, 5.0),
    ],
    "RepairStation": [
        ("Repair_IronPlate", item(f"{PARTS}/IronPlate/Desc_IronPlate"), 2, "Reparieren mit Eisenplatten (800 LP)", 4.0, 1.0),
        ("Repair_Reinforced", item(f"{PARTS}/IronPlateReinforced/Desc_IronPlateReinforced"), 1, "Reparieren mit verstärkten Eisenplatten (3000 LP)", 4.0, 2.0),
        ("Repair_SteelBeam", item(f"{PARTS}/SteelPlate/Desc_SteelPlate"), 1, "Reparieren mit Stahlträgern (2000 LP)", 4.0, 3.0),
    ],
    "TeslaCoil": [
        ("Tesla_Discharge", None, 0, "Entladung (Aufladen mit Strom)", 4.0, 1.0),
    ],
    # 0.81 passive Bauten P1: Lockfeuer = 15 s Lockruf je 2 Kohle, Oelschlick = 1 m³ Schweroelrueckstand je 6 s (10 m³/min)
    "LureBeacon": [
        ("Lure_Coal", item(f"{G}/Resource/RawResources/Coal/Desc_Coal"), 2, "Lockruf mit Kohle (15 s)", 15.0, 1.0),
        ("Lure_CompactedCoal", item(f"{PARTS}/CompactedCoal/Desc_CompactedCoal"), 1, "Lockruf mit Kompaktkohle (15 s)", 15.0, 2.0),
    ],
    "SlickPlate": [
        ("Slick_HeavyOil", item(f"{PARTS}/HeavyOilResidue/Desc_HeavyOilResidue"), 1000, "Schwerölrückstand verteilen", 6.0, 1.0),
    ],
    # 0.83 Bauten P2/P3: Strom-Bauten ohne Zutat (Zyklus = Laufzeit), Flak-Kanister, reanimiertes SAM, Ficsonium
    "ConductorWall": [("Zaun_Strom", None, 0, "Spannung halten", 1.0, 1.0)],
    "RadioMast": [("Funk_Strom", None, 0, "Senden", 2.0, 1.0)],
    "ShieldGenerator": [("Schild_Strom", None, 0, "Schild halten", 2.0, 1.0)],
    "FlakTurret": [("Flak_Shoot", item("/FicsitTD/Items/Desc_TD_FlakKanister"), 1, "Flak-Kanister abfeuern", 2.0, 1.0)],
    "FieldProjector": [("Feld_SAM", item(f"{PARTS}/SAMIngot/Desc_SAMIngot"), 1, "Störfeld mit reanimiertem SAM (60 s)", 60.0, 1.0)],
    "SingularityLauncher": [("Singularitaet_Ficsonium", item(f"{PARTS}/Ficsonium/Desc_Ficsonium"), 1, "Ficsonium-Ladung abfeuern", 8.0, 1.0)],
    "FlameTurret": [
        ("Flame_Fuel", item(f"{PARTS}/Fuel/Desc_LiquidFuel"), 100, "Treibstoff verbrennen", 1.0, 1.0),
        ("Flame_Biofuel", item(f"{PARTS}/BioFuel/Desc_LiquidBiofuel"), 100, "Flüssigen Biotreibstoff verbrennen", 1.0, 2.0),
        ("Flame_Turbo", item(f"{PARTS}/Turbofuel/Desc_LiquidTurboFuel"), 67, "Turbotreibstoff verbrennen", 1.0, 3.0),
        ("Flame_Rocket", item(f"{PARTS}/RocketFuel/Desc_RocketFuel"), 67, "Raketentreibstoff verbrennen", 1.0, 4.0),
        ("Flame_Ionized", item(f"{PARTS}/IonizedFuel/Desc_IonizedFuel"), 50, "Ionisierten Treibstoff verbrennen", 1.0, 5.0),
    ],
}

def erzeuge_schuss():
    d = EAL.duplicate_asset(SCHUSS_VORLAGE, SCHUSS)
    if d is None:
        raise RuntimeError("Desc_TD_BoltShot: Duplizieren fehlgeschlagen (alte Datei geloescht?)")
    unreal.BlueprintEditorLibrary.compile_blueprint(d)
    c = unreal.get_default_object(d.generated_class())
    c.set_editor_property("mDisplayName", "Turm-Aktion")
    c.set_editor_property("mDescription", "Ein Schuss, ein Magazin, eine Entladung oder eine Sekunde Flamme eines FICSIT-Verteidigungsturms. Wird sofort verbraucht und nie ausgegeben.")
    quelle = unreal.get_default_object(unreal.load_class(None, SCHUSS_ICON_VON))
    for feld in ("mSmallIcon", "mPersistentBigIcon"):
        c.set_editor_property(feld, quelle.get_editor_property(feld))
    unreal.BlueprintEditorLibrary.compile_blueprint(d)
    EAL.save_loaded_asset(d, False)
    log(f"{SCHUSS}: Turm-Aktion")
    return d.generated_class()

# Nur fehlende Rezepte anlegen (M1): bestehende Turm-Rezepte und die Turm-Aktion bleiben unveraendert
INHALT = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"
def vorhanden(spielpfad):
    return os.path.exists(os.path.join(INHALT, *spielpfad.replace("/FicsitTD/", "").split("/")) + ".uasset")

SCHUSS_KLASSE = unreal.load_class(None, f"{SCHUSS}.{SCHUSS.rsplit('/', 1)[1]}_C") if vorhanden(SCHUSS) else erzeuge_schuss()

for turm, liste in REZEPTE.items():
    klasse = unreal.load_class(None, f"/FicsitTD/Buildings/{turm}/Build_TD_{turm}.Build_TD_{turm}_C")
    if klasse is None:
        log(f"FEHLER {turm}: Gebaeude nicht gefunden (create_building.py gelaufen?)")
        continue
    for key, zutat, menge, name, dauer, prio in liste:
        try:
            path = f"/FicsitTD/Buildings/{turm}/Recipe_TD_{key}"
            if vorhanden(path):
                continue
            r = EAL.duplicate_asset(VORLAGE, path)
            if r is None:
                raise RuntimeError("Duplizieren fehlgeschlagen (alte Datei geloescht?)")
            unreal.BlueprintEditorLibrary.compile_blueprint(r)
            c = unreal.get_default_object(r.generated_class())
            zutaten = []
            if zutat:
                cls = unreal.load_class(None, zutat)
                if cls is None:
                    raise RuntimeError(f"Zutat nicht gefunden: {zutat}")
                ia = unreal.ItemAmount()
                ia.set_editor_property("item_class", cls)
                ia.set_editor_property("amount", menge)
                zutaten.append(ia)
            c.set_editor_property("mIngredients", zutaten)
            produkt = unreal.ItemAmount()
            produkt.set_editor_property("item_class", SCHUSS_KLASSE)
            produkt.set_editor_property("amount", 1)
            c.set_editor_property("mProduct", [produkt])
            c.set_editor_property("mProducedIn", [klasse])
            c.set_editor_property("mManufactoringDuration", dauer)
            c.set_editor_property("mDisplayName", name)
            c.set_editor_property("mManufacturingMenuPriority", prio)
            unreal.BlueprintEditorLibrary.compile_blueprint(r)
            EAL.save_loaded_asset(r, False)
            log(f"{path}: {name}, Zutat {zutat.rsplit('.', 1)[-1] if zutat else '-'} x{menge}, {dauer} s")
        except Exception as e:
            log(f"FEHLER {turm}/{key}: {e}")
log("ENDE")
with open(r"E:\Modding\Logs\create_turret_recipes.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
