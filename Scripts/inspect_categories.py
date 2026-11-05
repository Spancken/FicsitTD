# Liest Baumenue-Kategorien (BC_*) und Unterkategorien (SC_*) aus, um eine eigene anzulegen.
import unreal
out = []
def dump(path):
    bp = unreal.load_asset(path)
    if bp is None:
        out.append(f"{path}: nicht gefunden"); return
    cdo = unreal.get_default_object(bp.generated_class())
    out.append(f"== {path}  Klasse={cdo.get_class().get_name()}")
    for p in ["mDisplayName", "mCategoryIcon", "mMenuPriority", "mIconColor", "mSubCategories", "mCategory"]:
        try:
            out.append(f"  {p} = {cdo.get_editor_property(p)}")
        except Exception as e:
            pass
dump("/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/BC_Organization")
dump("/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/BC_Power")
dump("/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/Sub_Organisation/SC_Storage")
ar = unreal.AssetRegistryHelpers.get_asset_registry()
names = sorted(str(a.package_name) for a in ar.get_assets_by_path("/Game/FactoryGame/Interface/UI/InGame/BuildMenu/BuildCategories/Sub_Organisation", recursive=True))
out.append("Sub_Organisation: " + ", ".join(names))
d = unreal.get_default_object(unreal.load_asset("/FicsitTD/Buildings/BoltTurret/Desc_TD_BoltTurret").generated_class())
out.append(f"Turm-Desc mCategory={d.get_editor_property('mCategory')} mSubCategories={d.get_editor_property('mSubCategories')} mMenuPriority={d.get_editor_property('mMenuPriority')}")
with open(r"E:\Modding\Logs\inspect_categories.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
