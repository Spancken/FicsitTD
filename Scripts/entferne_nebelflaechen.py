# Optik-Durchgang 16.11.2026, Ursache U1: Die Bauplaene sind Kopien von Spielgebaeuden (Konstruktor, Aussichtsturm, Lager) und erben
# deren Nebelflaechen (AFGBuildable::mFogPlaneLocations). Das Spiel setzt an diese Stellen ueber den Komponenten-Pool (APoolRoot)
# 2 x 2 m grosse dunkle Flaechen "InputFogPlane" - bei unseren kleineren Sockeln stehen sie frei neben dem Turm ("schwarze Platte",
# offen seit 0.5). Dieses Skript leert die Liste an allen Build_TD_*. setup_turret_blueprint.py ruft dieselbe Funktion beim Neuaufbau.
# Headless: UnrealEditor-Cmd.exe FactoryGame.uproject -run=pythonscript -script=<dieses Skript> -unattended -nosplash
import os
import unreal

INHALT = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"


def leere_nebelflaechen(bp, log=print):
    """Entfernt die geerbte Dekorations-Vorlage (mDecoratorClass, liefert die Pool-Flaechen InputFogPlane) und leert die
    Nebelflaechen der Sparse-Daten; True, wenn etwas geaendert wurde."""
    cdo = unreal.get_default_object(bp.generated_class())
    geaendert = False
    deko = cdo.get_editor_property("mDecoratorClass")
    if deko is not None:
        cdo.set_editor_property("mDecoratorClass", None)
        log(f"  {bp.get_name()}: Dekorations-Vorlage {deko.get_name()} entfernt")
        geaendert = True
    sparse = cdo.get_editor_property("mBuildableSparseDataCDO")
    if sparse is not None:
        alt = sparse.get_editor_property("mFogPlaneLocations")
        if alt and len(alt) > 0:
            sparse.set_editor_property("mFogPlaneLocations", [])
            log(f"  {bp.get_name()}: {len(alt)} Nebelflaechen der Sparse-Daten entfernt")
            geaendert = True
        if sparse.get_editor_property("FogPlaneMesh") is not None:
            sparse.set_editor_property("FogPlaneMesh", None)
            geaendert = True
    return geaendert

if __name__ == "__main__":
    zeilen = []
    def log(m):
        zeilen.append(str(m)); unreal.log("FICSITTD| " + str(m))
    geaendert = 0
    for wurzel, _, dateien in os.walk(os.path.join(INHALT, "Buildings")):
        for d in sorted(dateien):
            if not (d.startswith("Build_TD_") and d.endswith(".uasset")):
                continue
            pfad = "/FicsitTD/" + os.path.relpath(os.path.join(wurzel, d[:-7]), INHALT).replace(os.sep, "/")
            bp = unreal.load_asset(pfad)
            if bp is None:
                log(f"  {pfad}: nicht ladbar")
                continue
            if leere_nebelflaechen(bp, log):
                unreal.BlueprintEditorLibrary.compile_blueprint(bp)
                unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
                # nachlesen: gespeichert ist es erst, wenn der Wert nach dem Kompilieren leer bleibt
                rest = unreal.get_default_object(bp.generated_class()).get_editor_property("mDecoratorClass")
                log(f"    nach dem Speichern: mDecoratorClass = {rest.get_name() if rest else None}")
                geaendert += 1
    log(f"ENDE {geaendert} Bauplaene geaendert")
    with open(r"E:\Modding\Logs\entferne_nebelflaechen.txt", "w", encoding="utf-8") as f:
        f.write("\n".join(zeilen))
