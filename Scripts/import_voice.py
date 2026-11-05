# 0.69.10: Sprachdateien der ADA-Nachrichten als Unreal-Sounds (USoundWave) importieren.
# Hintergrund: Mod-Soundbanks werden in Satisfactory 1.2.5 von Wwise abgelehnt (Fehler 92), die Unreal-Audioengine ist seit
# Patch 1.2.4.1 wieder aktiv (FicsIt-Networks PR #543, geschlossen 16.10.2026) -> Ton ueber Unreal, Untertitel ueber die Nachricht.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

AT = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
aufgaben = []
for n in ("begruessung",):
    for sprache in ("de", "en"):
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", rf"E:\Modding\Audio\FicsitTD\VO\{sprache}\{n}\TD_{n.capitalize()}_{sprache}.wav")
        t.set_editor_property("destination_path", "/FicsitTD/Audio")
        t.set_editor_property("destination_name", f"SW_TD_{n.capitalize()}_{sprache}")
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        aufgaben.append(t)
AT.import_asset_tasks(aufgaben)
for t in aufgaben:
    for pfad in t.get_editor_property("imported_object_paths"):
        a = unreal.load_asset(pfad)
        log(f"{pfad}: {a.get_class().get_name()} {a.get_editor_property('duration'):.1f} s")
log("ENDE")
with open(r"E:\Modding\Logs\import_voice.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
