# 0.69.1 (Konzept-Kernplaetze S4): Spieleinstellung "FICSIT Tower Defense" fuer neue Spielstaende (SML Session Setting).
# Vorlage: SML/SessionSettings/SessionSetting_ForceAllowCheats (Ja/Nein-Schalter). Vorher alte Datei per Dateisystem loeschen.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/SML/SessionSettings", "/FicsitTD"], True)
QUELLE = "/SML/SessionSettings/SessionSetting_ForceAllowCheats"
ZIEL = "/FicsitTD/SessionSettings/SessionSetting_TowerDefense"
try:
    q = unreal.load_asset(QUELLE)
    log(f"Quelle {q} Klasse {q.get_class().get_name()}")
    for name in ("StrId", "DisplayName", "ToolTip", "Category", "SubCategory", "ShowInBuilds", "ManagerTypeAvailability", "ValueSelector", "MenuPriority", "MapNameWhitelist"):
        try:
            log(f"  {name} = {q.get_editor_property(name)}")
        except Exception as e:
            log(f"  {name}: {e}")
    a = EAL.duplicate_asset(QUELLE, ZIEL)
    if a is None:
        raise RuntimeError("Duplizieren fehlgeschlagen (alte Datei geloescht?)")
    a.set_editor_property("StrId", "FicsitTD.TowerDefense")
    a.set_editor_property("DisplayName", "FICSIT Tower Defense")
    try:
        a.set_editor_property("ToolTip", "Startet einen Tower-Defense-Einsatz: Du landest auf einem festen Kernplatz deines Startorts, Plattform und Abwehrkern stehen schon. Die Schwierigkeit folgt dem Startort (Grasfelder leicht ... Duenenwueste Albtraum).")
    except Exception as e:
        log(f"ToolTip nicht gesetzt: {e}")
    EAL.save_loaded_asset(a, False)
    log(f"angelegt: {ZIEL}, StrId {a.get_editor_property('StrId')}")
    try:
        vs = a.get_editor_property("ValueSelector")
        log(f"ValueSelector {vs} {vs.get_class().get_name() if vs else ''}")
        if vs:
            for n in ("DefaultValue", "bDefaultValue", "Default"):
                try:
                    log(f"  {n} = {vs.get_editor_property(n)}")
                except Exception:
                    pass
    except Exception as e:
        log(f"ValueSelector: {e}")
except Exception as e:
    log(f"FEHLER {e}")
log("ENDE")
with open(r"E:\Modding\Logs\create_session_setting.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
