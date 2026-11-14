# 0.10.0 (Marius 14.11.2026): Forschung mit der Festplatte des Spiels statt FICSIT-Kampfdaten.
# Tauscht in allen Schematics unter /FicsitTD/Research die Kosten-Zeile Desc_TD_Kampfdaten gegen Desc_HardDrive (gleiche Menge)
# und speichert. Aufruf headless:
#   UnrealEditor-Cmd.exe FactoryGame.uproject -run=pythonscript -script="...\Scripts\umstellen_festplatte_mam.py" -unattended -nosplash
# Ergebnis zusaetzlich in E:\Modding\Logs\umstellen_festplatte_mam.txt
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

ALT = "/FicsitTD/Items/Desc_TD_Kampfdaten.Desc_TD_Kampfdaten_C"
NEU = "/Game/FactoryGame/Resource/Environment/CrashSites/Desc_HardDrive.Desc_HardDrive_C"

alt_klasse = unreal.load_class(None, ALT)
neu_klasse = unreal.load_class(None, NEU)
log(f"alt {alt_klasse}, neu {neu_klasse}")
geaendert, unveraendert, fehler = 0, 0, 0
if alt_klasse and neu_klasse:
    # list_assets findet im Commandlet keine Plugin-Inhalte (erprobt 14.11.: 0 Treffer) - Pfade aus dem Dateisystem
    import glob, os
    wurzel = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content\Research"
    pfade = []
    for datei in glob.glob(os.path.join(wurzel, "**", "Research_TD_*.uasset"), recursive=True):
        rel = os.path.relpath(datei, wurzel).replace("\\", "/")[:-len(".uasset")]
        name = rel.rsplit("/", 1)[-1]
        pfade.append(f"/FicsitTD/Research/{rel}")
    log(f"{len(pfade)} Forschungsknoten gefunden")
    for pfad in pfade:
        try:
            bp = unreal.load_asset(pfad)   # Paketpfad ohne ".Name" (Muster der create_*.py)
            cdo = unreal.get_default_object(bp.generated_class())
            kosten = list(cdo.get_editor_property("mCost"))
            neu, treffer = [], 0
            for k in kosten:
                if k.get_editor_property("item_class") == alt_klasse:
                    ia = unreal.ItemAmount()
                    ia.set_editor_property("item_class", neu_klasse)
                    ia.set_editor_property("amount", k.get_editor_property("amount"))
                    neu.append(ia)
                    treffer += 1
                else:
                    neu.append(k)
            if treffer:
                cdo.set_editor_property("mCost", neu)
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
                unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
                geaendert += 1
                log(f"umgestellt: {pfad}")
            else:
                unveraendert += 1
        except Exception as e:
            fehler += 1
            log(f"FEHLER {pfad}: {e}")
log(f"Ergebnis: {geaendert} umgestellt, {unveraendert} ohne Kampfdaten, {fehler} Fehler")
with open(r"E:\Modding\Logs\umstellen_festplatte_mam.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG) + "\n")
