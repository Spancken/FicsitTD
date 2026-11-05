# Vollstaendiges Knotenformat eines MAM-Forschungsbaums (fuer create_research.py)
import unreal

baum = unreal.load_class(None, "/Game/FactoryGame/Schematics/Research/BPD_ResearchTree_Caterium.BPD_ResearchTree_Caterium_C")
cdo = unreal.get_default_object(baum)
zeilen = []
for i, k in enumerate(cdo.get_editor_property("mNodes")[:3]):
    zeilen.append(f"--- Knoten {i}")
    zeilen.append(k.get_editor_property("mNodeDataStruct").export_text())
schema = unreal.get_default_object(unreal.load_class(None, "/Game/FactoryGame/Schematics/Research/Caterium_RS/Research_Caterium_1.Research_Caterium_1_C"))
zeilen.append("--- Schematic-Eigenschaften")
for n in sorted(dir(schema)):
    if n.startswith("m") or n.startswith("b"):
        try:
            zeilen.append(f"{n} = {str(schema.get_editor_property(n))[:200]}")
        except Exception:
            pass
with open(r"E:\Modding\Logs\inspect_research_tree.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(zeilen))
