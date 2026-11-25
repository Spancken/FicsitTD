# 25.11.2026 (Flieger: Voegel und Mantas statt Krabben): nur die Beschreibung des Himmelsturms neu setzen, ohne das Gebaeude
# neu anzulegen (create_building.py wuerde es aus dem Konstruktor neu kopieren). Text wie in create_building.py (GEBAEUDE FlakTurret).
# Aufruf: UnrealEditor-Cmd.exe FactoryGame.uproject -run=pythonscript -script=<dieses Skript> -unattended -nosplash
import unreal

TEXT = ("Flugabwehr: schiesst Flak-Kanister (Band-Eingang) auf Flieger (Vögel, Mantas) in 60 m, 200 Schaden in 6 m. "
        "Greift keine Bodengegner an. Braucht Strom.")
PFAD = "/FicsitTD/Buildings/FlakTurret/Desc_TD_FlakTurret"
LOG = []

# Im Commandlet ist die Asset-Registry noch nicht eingelesen (Lauf 25.11.: "could not be found in the Asset Registry")
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/FicsitTD/Buildings/FlakTurret"], True)
bp = unreal.load_asset(PFAD + "." + PFAD.rsplit("/", 1)[1])
if not bp:
    LOG.append(f"FEHLER: {PFAD} nicht gefunden")
else:
    d = unreal.get_default_object(bp.generated_class())
    alt = d.get_editor_property("mDescription")
    d.set_editor_property("mDescription", TEXT)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    LOG.append(f"alt: {alt}")
    LOG.append(f"ERFOLG: {d.get_editor_property('mDescription')}")

with open(r"E:\Modding\Logs\setze_himmelsturm_text.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
