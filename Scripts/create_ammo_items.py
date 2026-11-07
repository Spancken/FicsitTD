# Neue Munition (Munitions-Konzept.md, Teil M1): Durchschlagbolzen = Stufe 2 des Bolzenturms.
# Kopie von Desc_SpikedRebar (Munitionsklasse mit Projektil, damit der Turm das Geschoss zeigen kann).
# Vorher die .uasset unter Content/Items per Dateisystem loeschen; danach create_einsatz_recipes.py und create_turret_recipes.py.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
G = "/Game/FactoryGame"

ITEMS = [
    # (Ziel, Vorlage, Name, Beschreibung)
    ("/FicsitTD/Items/Desc_TD_Durchschlagbolzen", f"{G}/Resource/Parts/SpikedRebar/Desc_SpikedRebar", "Durchschlagbolzen",
     "Gehaerteter Bolzen mit Schraubspitze: durchschlaegt das erste Ziel und trifft bis zu zwei weitere Gegner in der Schusslinie. Munition fuer den Bolzenturm."),
    # 0.83 Himmelsturm
    ("/FicsitTD/Items/Desc_TD_FlakKanister", f"{G}/Resource/Parts/AluminumCasing/Desc_AluminumCasing", "Flak-Kanister",
     "Aluminiumhuelse mit Schwarzpulver und Zeitzuender: zerplatzt in der Luft. Munition fuer den Himmelsturm (nur gegen Flieger)."),
]
import os
INHALT = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"
# 0.83: vorhandene Gegenstaende bleiben unveraendert, nur fehlende anlegen
ITEMS = [i for i in ITEMS if not os.path.exists(os.path.join(INHALT, *i[0].replace("/FicsitTD/", "").split("/")) + ".uasset")]

for ziel, vorlage, name, text in ITEMS:
    try:
        d = EAL.duplicate_asset(vorlage, ziel)
        if d is None:
            raise RuntimeError("Duplizieren fehlgeschlagen (alte Datei geloescht?)")
        unreal.BlueprintEditorLibrary.compile_blueprint(d)
        c = unreal.get_default_object(d.generated_class())
        c.set_editor_property("mDisplayName", name)
        c.set_editor_property("mDescription", text)
        unreal.BlueprintEditorLibrary.compile_blueprint(d)
        EAL.save_loaded_asset(d, False)
        log(f"{ziel}: {name}")
    except Exception as e:
        log(f"FEHLER {ziel}: {e}")

with open(r"E:\Modding\Logs\create_ammo_items.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
