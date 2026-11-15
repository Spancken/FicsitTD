# 0.10.1 (Marius 15.11.: Modus-Auswahl beim Spielstart transparenter, mit Erlaeuterung, spaeter weitere Modi):
# eigene Kategorie "FICSIT Tower Defense" mit Unterkategorie "Spielmodus" und eine Auswahlliste (Integer Selector,
# Dropdown) statt der Ankreuzbox. StrId FicsitTD.ModusInt: 0 = Aus, 1 = Einsatz. Weitere Modi = weiterer Eintrag.
# Texte DE/EN setzt UTDGameInstanceModule zur Laufzeit (Spielsprache), hier nur Platzhalter auf Deutsch.
# Vorher alte Dateien per Dateisystem loeschen (Content/SessionSettings/SSC_FicsitTD*, SSSubC_FicsitTD*, SessionSetting_Modus*).
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/SML/SessionSettings", "/FicsitTD"], True)
ORDNER = "/FicsitTD/SessionSettings"

def kategorie(quelle, ziel, name, prio):
    bp = EAL.duplicate_asset(quelle, ziel)
    if bp is None:
        raise RuntimeError(f"Duplizieren {quelle} -> {ziel} fehlgeschlagen (alte Datei geloescht?)")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("mDisplayName", name)
    try:
        cdo.set_editor_property("mMenuPriority", prio)
    except Exception as e:
        log(f"mMenuPriority nicht gesetzt: {e}")
    EAL.save_loaded_asset(bp, False)
    log(f"Kategorie {ziel}: {name}")
    return bp.generated_class()

try:
    haupt = kategorie("/SML/SessionSettings/SSC_SML", f"{ORDNER}/SSC_FicsitTD", "FICSIT Tower Defense", 5.0)
    unter = kategorie("/SML/SessionSettings/SSC_Cheats", f"{ORDNER}/SSSubC_FicsitTD_Modus", "Spielmodus", 0.0)

    a = EAL.duplicate_asset(f"{ORDNER}/SessionSetting_TowerDefense", f"{ORDNER}/SessionSetting_Modus")
    if a is None:
        raise RuntimeError("Duplizieren der Einstellung fehlgeschlagen (alte Datei geloescht?)")
    a.set_editor_property("StrId", "FicsitTD.ModusInt")
    a.set_editor_property("DisplayName", "Spielmodus")
    a.set_editor_property("ToolTip", "Aus: normales Satisfactory. Einsatz: Tower Defense am Abwehrkern.")

    wahl = unreal.new_object(unreal.FGUserSetting_IntSelector, outer=a)
    werte = []
    for wert, name in ((0, "Aus - normales Satisfactory"), (1, "Einsatz - Abwehrkern über 50 Wellen")):
        e = unreal.IntegerSelection()
        e.set_editor_property("name", name)
        e.set_editor_property("value", wert)
        werte.append(e)
    wahl.set_editor_property("integer_selection_values", werte)
    wahl.set_editor_property("default_value", 0)
    wahl.set_editor_property("show_as_dropdown", True)
    a.set_editor_property("ValueSelector", wahl)

    orte = list(a.get_editor_property("WidgetsToCreate"))
    log(f"WidgetsToCreate vorher: {len(orte)}")
    # CategoryClass ist EditDefaultsOnly und per set_editor_property gesperrt (15.11.) -> Struktur als Text setzen
    ort = unreal.SettingsWidgetLocationDescriptor()
    ort.import_text(f'(CategoryClass="{haupt.get_path_name()}",SubCategoryClass="{unter.get_path_name()}",SubOptionTo=None,MenuPriority=0.000000)')
    a.set_editor_property("WidgetsToCreate", [ort])
    EAL.save_loaded_asset(a, False)
    pruef = a.get_editor_property("ValueSelector")
    log(f"Einstellung {ORDNER}/SessionSetting_Modus: StrId {a.get_editor_property('StrId')}, "
        f"Auswahl {pruef.get_class().get_name()}, {len(pruef.get_editor_property('integer_selection_values'))} Werte, "
        f"Ort {a.get_editor_property('WidgetsToCreate')[0].get_editor_property('category_class')}")
except Exception as e:
    log(f"FEHLER {e}")
log("ENDE")
with open(r"E:\Modding\Logs\create_session_modus.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
