# Einsatz (Einsatz-Konzept Abschnitt 6): kurze Einstiegsrezepte fuer Munition, "FICSIT-Notrezepte".
# Rohbolzen ist von Anfang an frei, die uebrigen schaltet das MAM frei (create_research.py).
# Alte Recipe_TD_*.uasset unter Content/Recipes vorher per Dateisystem loeschen; danach Build-Mod.ps1.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
# Alternativrezept als Vorlage: mDisplayNameOverride=True ist dort schon gesetzt (per Python nicht setzbar)
VORLAGE = "/Game/FactoryGame/Recipes/AlternateRecipes/New_Update3/Recipe_Alternate_SteelRod"
G = "/Game/FactoryGame"
P = f"{G}/Resource/Parts"
KONSTRUKTOR = f"{G}/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1"
ASSEMBLER = f"{G}/Buildable/Factory/AssemblerMk1/Build_AssemblerMk1"
DURCHSCHLAG = "/FicsitTD/Items/Desc_TD_Durchschlagbolzen"
GIESSEREI = f"{G}/Buildable/Factory/FoundryMk1/Build_FoundryMk1"
RAW = f"{G}/Resource/RawResources"
# 0.82 Munitionsstufen (Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07, Marius 07.11.): Gabelungen brauchen je einen neuen Rohstoff
# aus dem Originalrezept (Quarz, Caterium, Schwefel), Stufe 3 Raffinerie- bzw. Aluminiumketten. Biomasse nicht mehr in Munition.
SCHWEFEL = f"{RAW}/Sulfur/Desc_Sulfur"
QUARZ = f"{P}/QuartzCrystal/Desc_QuartzCrystal"
TURBODRAHT = f"{P}/HighSpeedWire/Desc_HighSpeedWire"
PULVER2 = f"{P}/GunPowder/Desc_GunpowderMK2"

def item(pfad):
    return f"{pfad}.{pfad.rsplit('/', 1)[1]}_C"

REZEPTE = [
    # (Name, Zutaten [(Item, Menge)], Produkt (Item, Menge), Gebaeude, Dauer s, Anzeigename)
    ("Rohbolzen", [(f"{P}/IronIngot/Desc_IronIngot", 1)], (f"{P}/SpikedRebar/Desc_SpikedRebar", 1),
     KONSTRUKTOR, 4.0, "Rohbolzen (FICSIT-Notrezept)"),
    # Munitionsleitern (Munitions-Konzept.md, M1): jede Stufe braucht eine Komponente mehr als ihr Vorgaenger.
    # Der Assembler hat nur zwei Eingaenge - deshalb hoechstens zwei Zutaten je Rezept.
    ("Durchschlagbolzen", [(f"{P}/SpikedRebar/Desc_SpikedRebar", 2), (f"{P}/IronScrew/Desc_IronScrew", 4)],
     (DURCHSCHLAG, 2), ASSEMBLER, 8.0, "Durchschlagbolzen"),
    ("Explosivbolzen", [(DURCHSCHLAG, 2), (PULVER2, 1)],
     (f"{G}/Equipment/RebarGun/Ammo/Desc_Rebar_Explosive", 2), ASSEMBLER, 8.0, "Explosivbolzen (FICSIT)"),
    ("Streubolzen", [(DURCHSCHLAG, 2), (QUARZ, 1)],
     (f"{G}/Equipment/RebarGun/Ammo/Desc_Rebar_Spreadshot", 2), ASSEMBLER, 8.0, "Streubolzen (FICSIT)"),
    ("Schockbolzen", [(DURCHSCHLAG, 2), (TURBODRAHT, 2)],
     (f"{G}/Equipment/RebarGun/Ammo/Desc_Rebar_Stunshot", 2), ASSEMBLER, 8.0, "Schockbolzen (FICSIT)"),
    ("Patrone", [(f"{P}/CopperSheet/Desc_CopperSheet", 1), (f"{P}/IronRod/Desc_IronRod", 1)],
     (f"{P}/CartridgeStandard/Desc_CartridgeStandard", 1), ASSEMBLER, 4.0, "Gewehrmunition (FICSIT)"),
    ("Zielsuchmunition", [(f"{P}/CartridgeStandard/Desc_CartridgeStandard", 1), (TURBODRAHT, 1)],
     (f"{G}/Equipment/Rifle/Ammo/Desc_CartridgeSmartProjectile", 1), ASSEMBLER, 6.0, "Zielsuchmunition (FICSIT)"),
    ("Turbomunition", [(f"{P}/CartridgeStandard/Desc_CartridgeStandard", 2), (f"{P}/AluminumCasing/Desc_AluminumCasing", 1)],
     (f"{G}/Equipment/Rifle/Ammo/Desc_CartridgeChaos", 2), ASSEMBLER, 8.0, "Turbomunition (FICSIT)"),
    # 0.82: wie im Original aus Kohle und Schwefel (Schwefel liegt an 39 von 47 Kernplaetzen unter 300 m, Median 153 m)
    ("Schwarzpulver", [(f"{RAW}/Coal/Desc_Coal", 1), (SCHWEFEL, 1)],
     (f"{P}/GunPowder/Desc_Gunpowder", 2), ASSEMBLER, 4.0, "Schwarzpulver (FICSIT)"),
    ("Nobelisk", [(f"{P}/GunPowder/Desc_Gunpowder", 2), (f"{P}/SteelPipe/Desc_SteelPipe", 1)],
     (f"{P}/NobeliskExplosive/Desc_NobeliskExplosive", 1), ASSEMBLER, 6.0, "Nobelisk (FICSIT)"),
    ("NobeliskGas", [(f"{P}/NobeliskExplosive/Desc_NobeliskExplosive", 1), (SCHWEFEL, 2)],
     (f"{G}/Equipment/NobeliskDetonator/Ammo/Desc_NobeliskGas", 1), ASSEMBLER, 6.0, "Gas-Nobelisk (FICSIT)"),
    ("NobeliskPuls", [(f"{P}/NobeliskExplosive/Desc_NobeliskExplosive", 1), (QUARZ, 2)],
     (f"{G}/Equipment/NobeliskDetonator/Ammo/Desc_NobeliskShockwave", 1), ASSEMBLER, 6.0, "Puls-Nobelisk (FICSIT)"),
    ("NobeliskStreu", [(f"{P}/NobeliskExplosive/Desc_NobeliskExplosive", 2), (PULVER2, 2)],
     (f"{G}/Equipment/NobeliskDetonator/Ammo/Desc_NobeliskCluster", 1), ASSEMBLER, 8.0, "Streu-Nobelisk (FICSIT)"),
    # Feuer-Baum: Kompaktkohle fuer Turbotreibstoff, 0.82 wie im Original aus Kohle und Schwefel
    ("Kompaktkohle", [(f"{RAW}/Coal/Desc_Coal", 5), (SCHWEFEL, 5)],
     (f"{P}/CompactedCoal/Desc_CompactedCoal", 5), ASSEMBLER, 12.0, "Kompaktkohle (FICSIT)"),
    # 0.82 Bauxit-Knoten: Aluminium ohne Wasser und Raffinerie (Wasser liegt an keinem Kernplatz unter 300 m), FICSIT-Feldrezept
    ("Aluminiumbarren", [(f"{RAW}/OreBauxite/Desc_OreBauxite", 2), (f"{RAW}/Coal/Desc_Coal", 1)],
     (f"{P}/AluminumIngot/Desc_AluminumIngot", 1), GIESSEREI, 4.0, "Aluminiumbarren (FICSIT-Feldrezept)"),
    # 0.83 Himmelsturm: Aluminiumhuelse + Schwarzpulver
    ("FlakKanister", [(f"{P}/AluminumCasing/Desc_AluminumCasing", 1), (f"{P}/GunPowder/Desc_Gunpowder", 2)],
     ("/FicsitTD/Items/Desc_TD_FlakKanister", 2), ASSEMBLER, 6.0, "Flak-Kanister"),
]

def menge(pfad, n):
    cls = unreal.load_class(None, item(pfad))
    if cls is None:
        raise RuntimeError(f"Klasse nicht gefunden: {pfad}")
    ia = unreal.ItemAmount()
    ia.set_editor_property("item_class", cls)
    ia.set_editor_property("amount", n)
    return ia

for name, zutaten, produkt, gebaeude, dauer, anzeige in REZEPTE:
    pfad = f"/FicsitTD/Recipes/Recipe_TD_{name}"
    try:
        r = EAL.duplicate_asset(VORLAGE, pfad)
        if r is None:
            raise RuntimeError("Duplizieren fehlgeschlagen (alte Datei geloescht?)")
        unreal.BlueprintEditorLibrary.compile_blueprint(r)
        c = unreal.get_default_object(r.generated_class())
        c.set_editor_property("mIngredients", [menge(p, n) for p, n in zutaten])
        c.set_editor_property("mProduct", [menge(*produkt)])
        bau = unreal.load_class(None, item(gebaeude))
        if bau is None:
            raise RuntimeError(f"Gebaeude nicht gefunden: {gebaeude}")
        c.set_editor_property("mProducedIn", [bau])
        c.set_editor_property("mManufactoringDuration", dauer)
        c.set_editor_property("mDisplayName", anzeige)
        unreal.BlueprintEditorLibrary.compile_blueprint(r)
        EAL.save_loaded_asset(r, False)
        log(f"{pfad}: {anzeige}, {dauer} s")
    except Exception as e:
        log(f"FEHLER {pfad}: {e}")
log("ENDE")
with open(r"E:\Modding\Logs\create_einsatz_recipes.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
