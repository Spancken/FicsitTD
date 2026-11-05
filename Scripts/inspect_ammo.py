# Fahrplan-Fragen: Magazingroesse/Schaden der Spielmunition auslesen (Platzhalter des Starter Project tragen die echten Werte).
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

KLASSEN = [
    "/Game/FactoryGame/Resource/Parts/CartridgeStandard/Desc_CartridgeStandard.Desc_CartridgeStandard_C",
    "/Game/FactoryGame/Equipment/Rifle/Ammo/Desc_CartridgeSmartProjectile.Desc_CartridgeSmartProjectile_C",
    "/Game/FactoryGame/Equipment/Rifle/Ammo/Desc_CartridgeChaos.Desc_CartridgeChaos_C",
    "/Game/FactoryGame/Resource/Parts/SpikedRebar/Desc_SpikedRebar.Desc_SpikedRebar_C",
    "/Game/FactoryGame/Resource/Parts/NobeliskExplosive/Desc_NobeliskExplosive.Desc_NobeliskExplosive_C",
]
FELDER = ["mMagazineSize", "mReloadTimeMultiplier", "mFireRate", "mWeaponDamageMultiplier", "mMaxAmmoEffectiveRange",
          "mStackSize", "mDisplayName"]
for k in KLASSEN:
    try:
        c = unreal.load_class(None, k)
        cdo = unreal.get_default_object(c)
        werte = []
        for f in FELDER:
            try:
                werte.append(f"{f}={cdo.get_editor_property(f)}")
            except Exception:
                pass
        log(f"{k.rsplit('.', 1)[-1]} ({type(cdo).__name__}): " + ", ".join(werte))
    except Exception as e:
        log(f"FEHLER {k}: {e}")
with open(r"E:\Modding\Logs\inspect_ammo.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
