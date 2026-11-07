# 0.73 Spezialisierung S2 (Konzept-Spezialisierung.md 3.2): Gewehrturm-Gabelung Panzerbrechend und Brandmunition.
# Legt nur fehlende Assets an (vorhandene bleiben): zwei Patronen (Kopie von Desc_CartridgeStandard), ihre Assembler-Rezepte
# und die Turm-Rezepte des Gewehrturms (Zutat Patrone -> Desc_TD_Aktion_GunTurret). Danach create_research.py, dann Build-Mod.ps1.
import os
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
G = "/Game/FactoryGame"
P = f"{G}/Resource/Parts"
INHALT = r"E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"
VORLAGE_REZEPT = f"{G}/Recipes/AlternateRecipes/New_Update3/Recipe_Alternate_SteelRod"
VORLAGE_PATRONE = f"{P}/CartridgeStandard/Desc_CartridgeStandard"
ASSEMBLER = f"{G}/Buildable/Factory/AssemblerMk1/Build_AssemblerMk1"
GEWEHR = "/FicsitTD/Buildings/GunTurret/Build_TD_GunTurret"
AKTION = "/FicsitTD/Buildings/GunTurret/Desc_TD_Aktion_GunTurret"

def item(pfad):
    return f"{pfad}.{pfad.rsplit('/', 1)[1]}_C"

def vorhanden(spielpfad):
    return os.path.exists(os.path.join(INHALT, *spielpfad.replace("/FicsitTD/", "").split("/")) + ".uasset")

def menge(pfad, n):
    cls = unreal.load_class(None, item(pfad))
    if cls is None:
        raise RuntimeError(f"Klasse nicht gefunden: {pfad}")
    ia = unreal.ItemAmount()
    ia.set_editor_property("item_class", cls)
    ia.set_editor_property("amount", n)
    return ia

unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/FicsitTD/Items", "/FicsitTD/Recipes", "/FicsitTD/Buildings"], True)

PANZER = "/FicsitTD/Items/Desc_TD_PatronePanzer"
BRAND = "/FicsitTD/Items/Desc_TD_PatroneBrand"
PATRONEN = [
    (PANZER, "Panzerbrechende Munition",
     "Gewehrmunition mit Stahlkern: macht Durchschlag-Schaden statt Kugel-Schaden und geht durch die Panzerung der Hogs. Munition für den Gewehrturm."),
    (BRAND, "Brandmunition",
     "Gewehrmunition mit Brandsatz: jeder Treffer setzt das Ziel kurz in Brand (zusätzlicher Feuer-Schaden). Munition für den Gewehrturm."),
]
for ziel, name, text in PATRONEN:
    try:
        if vorhanden(ziel):
            log(f"{ziel}: vorhanden")
            continue
        d = EAL.duplicate_asset(VORLAGE_PATRONE, ziel)
        if d is None:
            raise RuntimeError("Duplizieren fehlgeschlagen")
        unreal.BlueprintEditorLibrary.compile_blueprint(d)
        c = unreal.get_default_object(d.generated_class())
        c.set_editor_property("mDisplayName", name)
        c.set_editor_property("mDescription", text)
        unreal.BlueprintEditorLibrary.compile_blueprint(d)
        EAL.save_loaded_asset(d, False)
        log(f"{ziel}: {name}")
    except Exception as e:
        log(f"FEHLER {ziel}: {e}")

STAHLROHR = f"{P}/SteelPipe/Desc_SteelPipe"
KOHLE = f"{G}/Resource/RawResources/Coal/Desc_Coal"
SCHWEFEL = f"{G}/Resource/RawResources/Sulfur/Desc_Sulfur"
STANDARD = VORLAGE_PATRONE
REZEPTE = [
    # (Pfad, Zutaten, Produkt, Gebaeude, Dauer, Name, Prioritaet)
    ("/FicsitTD/Recipes/Recipe_TD_PatronePanzer", [(STANDARD, 2), (STAHLROHR, 1)], (PANZER, 2), ASSEMBLER, 8.0, "Panzerbrechende Munition (FICSIT)", None),
    ("/FicsitTD/Recipes/Recipe_TD_PatroneBrand", [(STANDARD, 2), (SCHWEFEL, 1)], (BRAND, 2), ASSEMBLER, 8.0, "Brandmunition (FICSIT)", None),   # 0.82: Schwefel statt Kohle
    ("/FicsitTD/Buildings/GunTurret/Recipe_TD_Gun_Panzer", [(PANZER, 1)], (AKTION, 1), GEWEHR, 0.6, "Panzerbrechende Munition laden (15 Schuss)", 2.5),
    ("/FicsitTD/Buildings/GunTurret/Recipe_TD_Gun_Brand", [(BRAND, 1)], (AKTION, 1), GEWEHR, 0.6, "Brandmunition laden (15 Schuss)", 2.7),
]
for pfad, zutaten, produkt, gebaeude, dauer, name, prio in REZEPTE:
    try:
        if vorhanden(pfad):
            log(f"{pfad}: vorhanden")
            continue
        r = EAL.duplicate_asset(VORLAGE_REZEPT, pfad)
        if r is None:
            raise RuntimeError("Duplizieren fehlgeschlagen")
        unreal.BlueprintEditorLibrary.compile_blueprint(r)
        c = unreal.get_default_object(r.generated_class())
        c.set_editor_property("mIngredients", [menge(p, n) for p, n in zutaten])
        c.set_editor_property("mProduct", [menge(*produkt)])
        bau = unreal.load_class(None, item(gebaeude))
        if bau is None:
            raise RuntimeError(f"Gebaeude nicht gefunden: {gebaeude}")
        c.set_editor_property("mProducedIn", [bau])
        c.set_editor_property("mManufactoringDuration", dauer)
        c.set_editor_property("mDisplayName", name)
        if prio is not None:
            c.set_editor_property("mManufacturingMenuPriority", prio)
        unreal.BlueprintEditorLibrary.compile_blueprint(r)
        EAL.save_loaded_asset(r, False)
        log(f"{pfad}: {name}, {dauer} s")
    except Exception as e:
        log(f"FEHLER {pfad}: {e}")
log("ENDE")
with open(r"E:\Modding\Logs\create_spezialisierung.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
