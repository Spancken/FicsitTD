# 0.12 Etappe E1 (Fahrplan-0.12, Bericht A2 Abschnitt 5.3): ein Posten fuer den AWESOME-Shop als Prototyp.
# Erzeugt /FicsitTD/Shop/Shop_TD_Bolzenturm als Kopie eines Shop-Schematics des Spiels:
#   Typ EST_ResourceSink, Preis 4 Coupons, eine Freischaltung UTDUnlockLaden (Schluessel "Bolzenturm", wiederholbar),
#   sichtbar und kaufbar nur mit Balance-Schalter Experiment.ShopPrototyp (UTDExperimentAbhaengigkeit).
# Vorher: Editor mit den Klassen aus TDShop.h bauen (Build-Mod.ps1 ohne -SkipEditor) und die alte Datei
# Content/Shop/Shop_TD_Bolzenturm.uasset per Dateisystem loeschen. Danach Build-Mod.ps1 -SkipEditor.
# Ergebnis steht in E:/Modding/Logs/create_shop_prototyp.txt.
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
G = "/Game/FactoryGame"
VORLAGE = f"{G}/Schematics/ResourceSink/ResourceSink_FactoryBarrier"
COUPON = f"{G}/Resource/Parts/ResourceSinkCoupon/Desc_ResourceSinkCoupon.Desc_ResourceSinkCoupon_C"
ZIEL = "/FicsitTD/Shop"
POSTEN = [
    # Schluessel, Name, Beschreibung, Startpreis, Symbol (Baumenue-Icon des Gebaeudes), Reihenfolge
    ("Bolzenturm", "Kontingent Bolzenturm",
     "Erhöht das Kontingent um einen Bolzenturm. Der Preis steigt mit jedem Kauf.",
     4, "/FicsitTD/Buildings/BoltTurret/Desc_TD_BoltTurret.Desc_TD_BoltTurret_C", 1.0),
]

try:
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([f"{G}/Schematics/ResourceSink", "/FicsitTD"], True)
    unlock_klasse = unreal.load_class(None, "/Script/FicsitTD.TDUnlockLaden")
    schalter_klasse = unreal.load_class(None, "/Script/FicsitTD.TDExperimentAbhaengigkeit")
    coupon = unreal.load_class(None, COUPON)
    if not unlock_klasse or not schalter_klasse:
        raise RuntimeError("TDUnlockLaden / TDExperimentAbhaengigkeit fehlen - Editor nicht neu gebaut?")
    if not coupon:
        raise RuntimeError("Coupon-Item nicht gefunden")
    for key, name, text, preis, symbol, prio in POSTEN:
        pfad = f"{ZIEL}/Shop_TD_{key}"
        s = EAL.duplicate_asset(VORLAGE, pfad)
        if s is None:
            raise RuntimeError(f"Duplizieren {VORLAGE} -> {pfad} fehlgeschlagen (alte Datei geloescht?)")
        unreal.BlueprintEditorLibrary.compile_blueprint(s)
        c = unreal.get_default_object(s.generated_class())
        log(f"Vorlage: Typ {c.get_editor_property('mType')}, Kategorie {c.get_editor_property('mSchematicCategory')}, "
            f"{len(c.get_editor_property('mUnlocks'))} Freischaltungen, Kosten {[(str(k.get_editor_property('item_class')), k.get_editor_property('amount')) for k in c.get_editor_property('mCost')]}")
        c.set_editor_property("mType", unreal.SchematicType.EST_RESOURCE_SINK)
        c.set_editor_property("mDisplayName", name)
        c.set_editor_property("mDescription", text)
        c.set_editor_property("mMenuPriority", prio)
        ia = unreal.ItemAmount()
        ia.set_editor_property("item_class", coupon)
        ia.set_editor_property("amount", preis)
        c.set_editor_property("mCost", [ia])
        frei = unreal.new_object(unlock_klasse, outer=c)
        frei.set_editor_property("mSchluessel", key)
        c.set_editor_property("mUnlocks", [frei])
        abh = unreal.new_object(schalter_klasse, outer=c)
        abh.set_editor_property("mSchalter", "ShopPrototyp")
        c.set_editor_property("mSchematicDependencies", [abh])
        c.set_editor_property("mDependenciesBlocksSchematicAccess", True)
        c.set_editor_property("mHiddenUntilDependenciesMet", True)
        c.set_editor_property("mRelevantShopSchematics", [])
        try:
            desc = unreal.load_class(None, symbol)
            textur = unreal.get_default_object(desc).get_editor_property("mSmallIcon") if desc else None
            if textur:
                pinsel = c.get_editor_property("mSchematicIcon")
                pinsel.set_editor_property("resource_object", textur)
                c.set_editor_property("mSchematicIcon", pinsel)
                c.set_editor_property("mSmallSchematicIcon", textur)
                log(f"Symbol: {textur.get_name()}")
            else:
                log("Symbol nicht gefunden - Bild der Vorlage bleibt")
        except Exception as e:
            log(f"Symbol nicht gesetzt: {e}")
        unreal.BlueprintEditorLibrary.compile_blueprint(s)
        EAL.save_loaded_asset(s, False)
        c2 = unreal.get_default_object(s.generated_class())
        log(f"OK {pfad}: Typ {c2.get_editor_property('mType')}, Preis {c2.get_editor_property('mCost')[0].get_editor_property('amount')}, "
            f"Freischaltung {[type(u).__name__ + ':' + str(u.get_editor_property('mSchluessel')) for u in c2.get_editor_property('mUnlocks')]}, "
            f"Abhaengigkeiten {[type(a).__name__ for a in c2.get_editor_property('mSchematicDependencies')]}, "
            f"Kategorie {c2.get_editor_property('mSchematicCategory')}, Unterkategorien {len(c2.get_editor_property('mSubCategories'))}")
    log("FERTIG")
except Exception as e:
    import traceback
    log("FEHLER " + str(e)); log(traceback.format_exc())

with open("E:/Modding/Logs/create_shop_prototyp.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
