# 0.11 (Gegencheck Einsatz, Marius 16.11.: Stufe vom Startort loesen, waehlbar, Standard "nach Startort"):
# zweite Auswahl "Stufe" unter "FICSIT Tower Defense / Spielmodus". StrId FicsitTD.StufeInt:
# 0 = nach Startort (Standard, alte Staende unveraendert), 1 = Erkundung, 2 = Normal, 3 = Hart, 4 = Albtraum.
# Vorlage ist SessionSetting_Modus (gleiche Kategorie). Texte DE/EN setzt UTDGameInstanceModule zur Laufzeit.
# Vorher eine alte Content/SessionSettings/SessionSetting_Stufe* per Dateisystem loeschen.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/SML/SessionSettings", "/FicsitTD"], True)
ORDNER = "/FicsitTD/SessionSettings"

try:
    a = EAL.duplicate_asset(f"{ORDNER}/SessionSetting_Modus", f"{ORDNER}/SessionSetting_Stufe")
    if a is None:
        raise RuntimeError("Duplizieren der Einstellung fehlgeschlagen (alte Datei geloescht?)")
    a.set_editor_property("StrId", "FicsitTD.StufeInt")
    a.set_editor_property("DisplayName", "Stufe")
    a.set_editor_property("ToolTip", "Schwierigkeit des Einsatzes. Nach Startort: Grasfelder Erkundung, Felsenwueste Normal, Wald Hart, Duenen Albtraum.")
    wahl = unreal.new_object(unreal.FGUserSetting_IntSelector, outer=a)
    werte = []
    for wert, name in ((0, "Nach Startort"), (1, "Erkundung"), (2, "Normal"), (3, "Hart"), (4, "Albtraum")):
        e = unreal.IntegerSelection()
        e.set_editor_property("name", name)
        e.set_editor_property("value", wert)
        werte.append(e)
    wahl.set_editor_property("integer_selection_values", werte)
    wahl.set_editor_property("default_value", 0)
    wahl.set_editor_property("show_as_dropdown", True)
    a.set_editor_property("ValueSelector", wahl)
    EAL.save_loaded_asset(a, False)
    pruef = a.get_editor_property("ValueSelector")
    log(f"Einstellung {ORDNER}/SessionSetting_Stufe: StrId {a.get_editor_property('StrId')}, "
        f"{len(pruef.get_editor_property('integer_selection_values'))} Werte, Orte {len(a.get_editor_property('WidgetsToCreate'))}")
except Exception as e:
    log(f"FEHLER {e}")
log("ENDE")
with open(r"E:\Modding\Logs\create_session_stufe.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
