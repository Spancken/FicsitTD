# 0.12 E4 (Marius 14.11. und 21.11.): Shop-Posten fuer den AWESOME-Shop, je Eintrag der Ladenliste einer.
# Eine Quelle: Ladenliste, Spieler-Posten, Kategorien und Namen werden aus Source/FicsitTD/Private/TDEinsatz.cpp gelesen.
# Erzeugt in /FicsitTD/Shop:
#   22.11. (Handspiel H5, Marius: "Shop nach Bereichen aufteilen, so ist es sehr unuebersichtlich"): sechs Hauptkategorien
#   SC_TD_<Bereich> (Tuerme, Verteidigung, Produktion, Strom, Logistik, Pionier) statt einer "FICSIT-Abwehr", darin
#   Unterkategorien SC_TD_Sub_Kontingente / _Ausruestung / _Kern
#   Shop_TD_<AssetName> je Posten: Typ EST_ResourceSink, Grundpreis in Coupons, Freischaltung UTDUnlockLaden (Schluessel,
#   wiederholbar), Sichtbarkeit UTDShopAbhaengigkeit (Einsatz aktiv, Rezept da, Hoechstzahl nicht erreicht), Symbol des Gebaeudes/Items.
# Texte deutsch im Asset, englisch in Scripts/uebersetzung_en.json (TDSprache::UebersetzeAssets).
# Vorher: Editor mit den Klassen aus TDShop.h bauen, alte Dateien Content/Shop/*.uasset per Dateisystem loeschen.
# Danach paketieren (Build-Mod.ps1 -SkipEditor). Ergebnis: E:/Modding/Logs/create_shop.txt
import unreal, re, json, io, os

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

MOD = "E:/Modding/SatisfactoryModLoader/Mods/FicsitTD"
EAL = unreal.EditorAssetLibrary
G = "/Game/FactoryGame"
VORLAGE = f"{G}/Schematics/ResourceSink/ResourceSink_FactoryBarrier"
KAT_VORLAGE = f"{G}/Schematics/ResourceSinkShopCategories/SC_RSS_Pioneering"
COUPON = f"{G}/Resource/Parts/ResourceSinkCoupon/Desc_ResourceSinkCoupon.Desc_ResourceSinkCoupon_C"
ZIEL = "/FicsitTD/Shop"

UNTER = [  # Schluessel, Deutsch, Englisch, Reihenfolge (wie LadenKategorie in TDEinsatz.cpp)
    ("Tuerme", "Türme", "Turrets", 0), ("Verteidigung", "Verteidigung", "Defense", 1), ("Produktion", "Produktion", "Production", 2),
    ("Strom", "Strom", "Power", 3), ("Logistik", "Logistik", "Logistics", 4), ("Pionier", "Pionier und Kern", "Pioneer and Core", 5),
]

def asset_name(key):
    for a, b in (("ä", "ae"), ("ö", "oe"), ("ü", "ue"), ("Ä", "Ae"), ("Ö", "Oe"), ("Ü", "Ue"), ("ß", "ss")):
        key = key.replace(a, b)
    return "".join(c for c in key if c.isalnum())

def lies_quelle():
    q = io.open(f"{MOD}/Source/FicsitTD/Private/TDEinsatz.cpp", encoding="utf-8").read()
    block = q[q.index("static const FTDLadenEintrag LadenListe[]"):q.index("};", q.index("static const FTDLadenEintrag LadenListe[]"))]
    laden = [(m[0], int(m[1]), float(m[2]), int(m[3]), m[4]) for m in
             re.findall(r'\{ TEXT\("([^"]+)"\), (\d+), ([\d.]+)f, (\d+), TEXT\("([^"]+)"\) \}', block)]
    sblock = q[q.index("static const FTDSpielerPosten SpielerPosten[]"):q.index("};", q.index("static const FTDSpielerPosten SpielerPosten[]"))]
    spieler = {m[0]: (int(m[1]), int(m[2]), m[3]) for m in re.findall(r'\{ TEXT\("([^"]+)"\), (\d), (\d+), TEXT\("([^"]+)"\)', sblock)}
    kblock = q[q.index("static int32 LadenKategorie("):q.index("return SpielerFuer(Key) ? 5 : 2;")]
    kat = {}
    for name, idx in (("Tuerme", 0), ("Abwehr", 1), ("Strom", 3), ("Logistik", 4)):
        m = re.search(r'static const TCHAR\* ' + name + r'\[\] = \{([^}]*)\}', kblock)
        for k in re.findall(r'TEXT\("([^"]+)"\)', m.group(1)):
            kat[k] = idx
    ablock = q[q.index("static FString TDAnzeige(const FString& Key)\n{"):]
    de_block = ablock[:ablock.index("static const TMap<FString, FString> En")]
    en_block = ablock[ablock.index("static const TMap<FString, FString> En"):ablock.index("const FString* Gefunden = En.Find(Key);")]
    de = dict(re.findall(r'\{ TEXT\("([^"]+)"\), TEXT\("([^"]+)"\) \}', de_block))
    en = dict(re.findall(r'\{ TEXT\("([^"]+)"\), TEXT\("([^"]+)"\) \}', en_block))
    return laden, spieler, kat, de, en

def texte(key, paket, sp, name_de, name_en):
    """Beschreibung DE und EN"""
    if sp is None:
        return (f"Kontingent +{paket} {name_de}, das Baumaterial kommt mit ins Inventar. Der Preis steigt mit jedem Kauf.",
                f"Quota +{paket} {name_en}, the building materials go to your inventory. The price rises with every purchase.")
    art = sp[0]
    if art == 1:
        return (f"+{paket} Inventarplätze, höchstens {sp[1]} Käufe. Der Preis steigt mit jedem Kauf.",
                f"+{paket} inventory slots, at most {sp[1]} purchases. The price rises with every purchase.")
    if art == 2:
        return (f"+{paket} Handplatz, höchstens {sp[1]} Käufe. Der Preis steigt mit jedem Kauf.",
                f"+{paket} hand slot, at most {sp[1]} purchases. The price rises with every purchase.")
    if art == 3:
        return (f"Repariert {paket} % der Kern-LP. Nur da, wenn der Kern beschädigt ist. Jeder Kauf wird teurer.",
                f"Repairs {paket} % of the Core HP. Only shown while the Core is damaged. Every purchase costs more.")
    if key == "Festplatte":
        return ("Eine Festplatte für das MAM. Jede weitere kostet 10 Coupons mehr.",
                "One Hard Drive for the MAM. Each further one costs 10 Coupons more.")
    if key == "Nobelisken-Zünder":
        return ("Nobelisken-Zünder mit 10 Nobelisken. Damit sprengst du Felsen über Rohstoffknoten.",
                "Nobelisk Detonator with 10 Nobelisks. Blast boulders off resource nodes with it.")
    if key == "Nobelisken":
        return (f"{paket} Nobelisken für den Zünder. Der Preis steigt mit jedem Kauf.",
                f"{paket} Nobelisks for the Detonator. The price rises with every purchase.")
    return (f"{name_de}, kommt ins Inventar. Einmal kaufbar.", f"{name_en}, goes to your inventory. One purchase.")

def symbol_fuer(key, rezept, sp):
    try:
        if sp is not None:
            item = unreal.load_class(None, sp[2])
        else:
            r = unreal.load_class(None, rezept)
            prod = unreal.get_default_object(r).get_editor_property("mProduct") if r else []
            item = prod[0].get_editor_property("item_class") if prod else None
        if not item:
            return None
        cdo = unreal.get_default_object(item)
        for feld in ("mSmallIcon", "mPersistentBigIcon"):
            try:
                t = cdo.get_editor_property(feld)
                if t:
                    return t
            except Exception:
                pass
    except Exception as e:
        log(f"Symbol {key}: {e}")
    return None

def kategorie(pfad, name, prio):
    k = EAL.duplicate_asset(KAT_VORLAGE, pfad)
    if k is None:
        raise RuntimeError(f"Kategorie {pfad} nicht angelegt (alte Datei geloescht?)")
    unreal.BlueprintEditorLibrary.compile_blueprint(k)
    c = unreal.get_default_object(k.generated_class())
    c.set_editor_property("mDisplayName", name)
    try:
        c.set_editor_property("mMenuPriority", float(prio))
    except Exception as e:
        log(f"mMenuPriority {pfad}: {e}")
    unreal.BlueprintEditorLibrary.compile_blueprint(k)
    EAL.save_loaded_asset(k, False)
    return k.generated_class()

try:
    laden, spieler, kat, de, en = lies_quelle()
    log(f"Quelle: {len(laden)} Laden-Eintraege, {len(spieler)} Spieler-Posten, {len(kat)} Kategorie-Zuordnungen")
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([f"{G}/Schematics", "/FicsitTD"], True)
    unlock_klasse = unreal.load_class(None, "/Script/FicsitTD.TDUnlockLaden")
    abh_klasse = unreal.load_class(None, "/Script/FicsitTD.TDShopAbhaengigkeit")
    coupon = unreal.load_class(None, COUPON)
    if not unlock_klasse or not abh_klasse:
        raise RuntimeError("TDUnlockLaden / TDShopAbhaengigkeit fehlen - Editor nicht neu gebaut?")
    uebers = {}
    # Bereiche als Hauptkategorien (links im Shop), Prioritaet negativ: vor den (im Einsatz ausgeblendeten) Spielkategorien
    bereich = {}
    for sk, nde, nen, prio in UNTER:
        bereich[prio] = kategorie(f"{ZIEL}/SC_TD_{sk}", nde, prio - 20)
        uebers[nde] = nen
    sub = {
        "kont": kategorie(f"{ZIEL}/SC_TD_Sub_Kontingente", "Kontingente", 0),
        "aus": kategorie(f"{ZIEL}/SC_TD_Sub_Ausruestung", "Ausrüstung", 1),
        "kern": kategorie(f"{ZIEL}/SC_TD_Sub_Kern", "Kern und Festplatten", 2),
    }
    uebers["Kontingente"] = "Quotas"; uebers["Ausrüstung"] = "Equipment"; uebers["Kern und Festplatten"] = "Core and Hard Drives"
    n = 0
    for nr, (key, grund, aufschlag, paket, rezept) in enumerate(laden):
        if key == "Panzersperre":
            continue
        sp = spieler.get(key)
        name_de = de.get(key, key)
        name_en = en.get(key, name_de)
        idx = 5 if sp is not None else kat.get(key, 2)
        text_de, text_en = texte(key, paket, sp, name_de, name_en)
        pfad = f"{ZIEL}/Shop_TD_{asset_name(key)}"
        s = EAL.duplicate_asset(VORLAGE, pfad)
        if s is None:
            raise RuntimeError(f"Duplizieren -> {pfad} fehlgeschlagen (alte Datei geloescht?)")
        unreal.BlueprintEditorLibrary.compile_blueprint(s)
        c = unreal.get_default_object(s.generated_class())
        c.set_editor_property("mType", unreal.SchematicType.EST_RESOURCE_SINK)
        c.set_editor_property("mDisplayName", name_de)
        c.set_editor_property("mDescription", text_de)
        c.set_editor_property("mMenuPriority", float(nr))
        ia = unreal.ItemAmount(); ia.set_editor_property("item_class", coupon); ia.set_editor_property("amount", int(grund))
        c.set_editor_property("mCost", [ia])
        frei = unreal.new_object(unlock_klasse, outer=c); frei.set_editor_property("mSchluessel", key)
        c.set_editor_property("mUnlocks", [frei])
        abh = unreal.new_object(abh_klasse, outer=c); abh.set_editor_property("mSchluessel", key)
        c.set_editor_property("mSchematicDependencies", [abh])
        c.set_editor_property("mDependenciesBlocksSchematicAccess", True)
        c.set_editor_property("mHiddenUntilDependenciesMet", True)
        c.set_editor_property("mRelevantShopSchematics", [])
        c.set_editor_property("mSchematicCategory", bereich[idx])
        art = "aus" if sp is not None else ("kern" if key in ("Kernreparatur", "Festplatte") else "kont")
        c.set_editor_property("mSubCategories", [sub[art]])
        textur = symbol_fuer(key, rezept, sp)
        if textur:
            pinsel = c.get_editor_property("mSchematicIcon"); pinsel.set_editor_property("resource_object", textur)
            c.set_editor_property("mSchematicIcon", pinsel)
            c.set_editor_property("mSmallSchematicIcon", textur)
        unreal.BlueprintEditorLibrary.compile_blueprint(s)
        EAL.save_loaded_asset(s, False)
        uebers[name_de] = name_en
        uebers[text_de] = text_en
        n += 1
        log(f"OK {pfad}: {name_de} / {name_en}, {grund} Coupons, Bereich {idx}, Unterkategorie {art}, Symbol {'ja' if textur else 'NEIN'}")
    # Uebersetzungen ergaenzen (vorhandene Eintraege bleiben)
    pj = f"{MOD}/Scripts/uebersetzung_en.json"
    alt = json.load(io.open(pj, encoding="utf-8"))
    neu = 0
    for k, v in uebers.items():
        if k not in alt:
            alt[k] = v; neu += 1
    io.open(pj, "w", encoding="utf-8", newline="\n").write(json.dumps(alt, ensure_ascii=False, indent=2) + "\n")
    log(f"Uebersetzung: {neu} neue Eintraege in uebersetzung_en.json")
    log(f"FERTIG: {n} Posten")
except Exception as e:
    import traceback
    log("FEHLER " + str(e)); log(traceback.format_exc())

with open("E:/Modding/Logs/create_shop.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
