# 0.11 (Modus Aus, Marius 15.11.: nur der Spielmodus zaehlt): die vier TD-Forschungsbaeume bekommen die Bedingung
# UTDModusAbhaengigkeit (C++, ModAktiv) in mVisibilityDependencies und mUnlockDependencies. Bestehende Unlock-Bedingungen
# (z. B. Ballistik nach Assembler) bleiben. Idempotent: vorhandene TDModusAbhaengigkeit werden nicht doppelt eingetragen.
# Voraussetzung: Editor mit der Klasse gebaut (Build-Mod.ps1 ohne -SkipEditor). Danach Build-Mod.ps1 (Assets neu kochen).
# create_research.py traegt die Bedingung beim Neuerzeugen ebenfalls ein.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
# Test 15.11.: ohne Scan findet der Commandlet die Plugin-Assets nicht ("nicht gefunden")
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/FicsitTD/Research"], True)
BAEUME = ["Industrie", "Ballistik", "Elementar", "Festung"]
try:
    klasse = unreal.load_class(None, "/Script/FicsitTD.TDModusAbhaengigkeit")
    if klasse is None:
        raise RuntimeError("Klasse /Script/FicsitTD.TDModusAbhaengigkeit fehlt (Editor neu bauen)")
    for b in BAEUME:
        pfad = f"/FicsitTD/Research/BPD_ResearchTree_TD_{b}"
        bp = EAL.load_asset(pfad)
        if bp is None:
            log(f"FEHLER {pfad} nicht gefunden"); continue
        cdo = unreal.get_default_object(bp.generated_class())
        for feld in ("mVisibilityDependencies", "mUnlockDependencies"):
            liste = list(cdo.get_editor_property(feld) or [])
            if any(x is not None and x.get_class() == klasse for x in liste):
                log(f"{b}.{feld}: schon vorhanden ({len(liste)} Eintraege)"); continue
            liste.append(unreal.new_object(klasse, outer=cdo))
            cdo.set_editor_property(feld, liste)
            log(f"{b}.{feld}: Modus-Bedingung ergaenzt ({len(liste)} Eintraege)")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        EAL.save_loaded_asset(bp, False)
        pruef = unreal.get_default_object(bp.generated_class())
        log(f"{b}: Sichtbarkeit {[x.get_class().get_name() for x in pruef.get_editor_property('mVisibilityDependencies')]}, "
            f"Freigabe {[x.get_class().get_name() for x in pruef.get_editor_property('mUnlockDependencies')]}")
except Exception as e:
    log(f"FEHLER {e}")
log("ENDE")
with open(r"E:\Modding\Logs\setze_modus_abhaengigkeit.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
