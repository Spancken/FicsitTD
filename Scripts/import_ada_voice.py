# 0.12-dev: Sprachdateien der ADA-Ansagen (Scripts/Audio/Erzeuge-ADA-Ansagen.ps1) als Unreal-Sounds importieren:
# E:\Modding\Audio\FicsitTD\VO\<sprache>\ada_<Key>\TD_<Ada_key>_<sprache>.wav -> /FicsitTD/Audio/SW_TD_ADA_<Key>_<sprache>.
# Gespielt ueber die Unreal-Audioengine wie die Begruessung (Wwise lehnt Mod-Banken in 1.2.5 ab), Text im Chat (TDAda.cpp).
import os
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

BASIS = r"E:\Modding\Audio\FicsitTD\VO"
AT = unreal.AssetToolsHelpers.get_asset_tools()
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/FicsitTD"], True)
aufgaben = []
for sprache in ("de", "en"):
    wurzel = os.path.join(BASIS, sprache)
    for ordner in sorted(os.listdir(wurzel)):
        if not ordner.startswith("ada_"):
            continue
        key = ordner[len("ada_"):]
        wav = os.path.join(wurzel, ordner, f"TD_{ordner.capitalize()}_{sprache}.wav")   # Name wie in vo_filter.py
        if not os.path.exists(wav):
            log(f"FEHLT {wav}")
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", wav)
        t.set_editor_property("destination_path", "/FicsitTD/Audio")
        t.set_editor_property("destination_name", f"SW_TD_ADA_{key}_{sprache}")
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        aufgaben.append(t)
AT.import_asset_tasks(aufgaben)
n = 0
for t in aufgaben:
    for pfad in t.get_editor_property("imported_object_paths"):
        a = unreal.load_asset(pfad)
        log(f"{pfad}: {a.get_editor_property('duration'):.1f} s")
        n += 1
log(f"ENDE {n} von {len(aufgaben)} importiert")
with open(r"E:\Modding\Logs\import_ada_voice.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
