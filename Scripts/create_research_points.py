# Forschungspunkte (Spielablauf-EarlyGame.md, Schritt B): Item "FICSIT-Kampfdaten" = Waehrung fuer die MAM-Forschung.
# Kopie von Desc_IronRod (einfacher Item-Deskriptor) mit dem Symbol der Festplatte. Nach jeder Welle gibt der Einsatz FP ins Inventar.
# Vorher Content/Items/Desc_TD_Kampfdaten.uasset per Dateisystem loeschen; danach create_research.py.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
VORLAGE = "/Game/FactoryGame/Resource/Parts/IronRod/Desc_IronRod"
ICON_VON = "/Game/FactoryGame/Resource/Environment/CrashSites/Desc_HardDrive.Desc_HardDrive_C"
ZIEL = "/FicsitTD/Items/Desc_TD_Kampfdaten"

try:
    d = EAL.duplicate_asset(VORLAGE, ZIEL)
    if d is None:
        raise RuntimeError("Duplizieren fehlgeschlagen (alte Datei geloescht?)")
    unreal.BlueprintEditorLibrary.compile_blueprint(d)
    c = unreal.get_default_object(d.generated_class())
    c.set_editor_property("mDisplayName", "FICSIT-Kampfdaten")
    c.set_editor_property("mDescription", "Forschungspunkt: Aufzeichnungen aus abgewehrten Wellen. Im MAM fuer neue Gebaeude, Munition und Faehigkeiten einsetzen. Es gibt sie nach jeder Welle.")
    quelle = unreal.get_default_object(unreal.load_class(None, ICON_VON))
    for feld in ("mSmallIcon", "mPersistentBigIcon"):
        c.set_editor_property(feld, quelle.get_editor_property(feld))
    try:
        c.set_editor_property("mStackSize", unreal.StackSize.SS_HUGE)
        log("Stapel: SS_HUGE")
    except Exception as e:
        log(f"Stapelgroesse nicht gesetzt: {e}")
    unreal.BlueprintEditorLibrary.compile_blueprint(d)
    EAL.save_loaded_asset(d, False)
    log(f"{ZIEL}: FICSIT-Kampfdaten angelegt")
except Exception as e:
    log(f"FEHLER: {e}")

with open(r"E:\Modding\Logs\create_research_points.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
