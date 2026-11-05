# 0.65 (Marius 04.11.2026: der Gewehrturm zeigt in der Produktion das Bolzen-Logo): eigenes Wegwerf-Produkt "Turm-Aktion" je Gebaeude
# mit dem Icon des Turms. Kopie von Desc_TD_BoltShot (create_turret_recipes.py); die Rezepte des Turms bekommen es als Produkt.
# Vorher alte Desc_TD_Aktion_*.uasset per Dateisystem loeschen. Danach Build-Mod.ps1 -SkipEditor.
import os
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
SCHUSS = "/FicsitTD/Buildings/BoltTurret/Desc_TD_BoltShot"
INHALT = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content\Buildings"
NAMEN = {"BoltTurret": "Bolzenturm", "GunTurret": "Gewehrturm", "TeslaCoil": "Teslaspule", "FlameTurret": "Flammenwerfer",
         "NobeliskLauncher": "Nobelisk-Werfer", "RepairStation": "Reparaturstation"}

# Im Commandlet kennt die Asset-Registry die Plugin-Inhalte erst nach einem Scan (sonst "could not be found in the Asset Registry")
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/FicsitTD/Buildings", "/FicsitTD/UI"], True)

for turm, name in NAMEN.items():
    try:
        ziel = f"/FicsitTD/Buildings/{turm}/Desc_TD_Aktion_{turm}"
        d = EAL.duplicate_asset(SCHUSS, ziel)
        if d is None:
            raise RuntimeError(f"{ziel}: Duplizieren fehlgeschlagen (alte Datei geloescht?)")
        unreal.BlueprintEditorLibrary.compile_blueprint(d)
        c = unreal.get_default_object(d.generated_class())
        c.set_editor_property("mDisplayName", f"{name}: Aktion")
        tex = unreal.load_asset(f"/FicsitTD/UI/Icons/Icon_TD_{turm}_512")
        if tex is None:
            raise RuntimeError(f"Icon_TD_{turm}_512 fehlt")
        c.set_editor_property("mSmallIcon", tex)
        c.set_editor_property("mPersistentBigIcon", tex)
        unreal.BlueprintEditorLibrary.compile_blueprint(d)
        EAL.save_loaded_asset(d, False)
        klasse = d.generated_class()
        # alle Munitionsrezepte des Turms (nicht das Baurezept Recipe_TD_<Turm>)
        n = 0
        for datei in sorted(os.listdir(os.path.join(INHALT, turm))):
            if not datei.startswith("Recipe_TD_") or datei == f"Recipe_TD_{turm}.uasset":
                continue
            r_name = datei[:-7]
            r = unreal.load_asset(f"/FicsitTD/Buildings/{turm}/{r_name}")
            rc = unreal.get_default_object(r.generated_class())
            produkte = rc.get_editor_property("mProduct")
            if not produkte:
                log(f"  {r_name}: kein Produkt, uebersprungen")
                continue
            p = produkte[0]
            p.set_editor_property("item_class", klasse)
            rc.set_editor_property("mProduct", [p] + list(produkte[1:]))
            unreal.BlueprintEditorLibrary.compile_blueprint(r)
            EAL.save_loaded_asset(r, False)
            n += 1
        log(f"{turm}: Desc_TD_Aktion_{turm}, {n} Rezepte umgestellt")
    except Exception as e:
        log(f"FEHLER {turm}: {e}")
log("ENDE")
with open(r"E:\Modding\Logs\create_turm_aktionen.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
