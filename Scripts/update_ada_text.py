# 0.85: Text und Untertitel der vorhandenen ADA-Nachrichten aus saetze_<n>.json und zeiten.json neu setzen (ohne Wwise-Kette)
import json, os
import unreal
SKRIPTE = os.path.dirname(os.path.abspath(__file__))
LOG = []
EAL = unreal.EditorAssetLibrary
for n in ("begruessung",):
    saetze = json.load(open(os.path.join(SKRIPTE, "Audio", f"saetze_{n}.json"), encoding="utf-8"))
    for sprache in ("de", "en"):
        zeiten = json.load(open(rf"E:\Modding\Audio\FicsitTD\VO\{sprache}\{n}\zeiten.json", encoding="utf-8"))["zeiten"]
        m = unreal.load_asset(f"/FicsitTD/Narrative/MSG_TD_{n.capitalize()}_{sprache}")
        liste = []
        for s, z in zip(saetze[sprache], zeiten):
            st = unreal.MessageSubtitle()
            t = s.replace('"', "'")
            st.import_text(f'(Subtitle=INVTEXT("{t}"),SenderClass="/Game/FactoryGame/Interface/UI/Message/Sender/Sender_ADA.Sender_ADA_C",TimeStamp={z:.2f})')
            liste.append(st)
        m.set_editor_property("mMessage", "\n\n".join(saetze[sprache]))
        m.set_editor_property("mSubtitles", liste)
        EAL.save_loaded_asset(m, False)
        LOG.append(f"{n} {sprache}: {len(liste)} Untertitel, Text {len(saetze[sprache])} Saetze")
open(r"E:\Modding\Logs\update_ada_text.txt", "w", encoding="utf-8").write("\n".join(LOG))
