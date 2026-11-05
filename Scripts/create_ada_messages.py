# 0.69.5 (Marius 04.11.2026): ADA-Nachrichten mit Sprachausgabe (Windows-Stimmen Hedda/Zira + Computerfilter, Scripts/Audio).
# Ablauf vorher: Scripts/Audio/tts_erzeugen.ps1, vo_filter.py, wwise_import.py, wwise_bank.py, WwiseConsole generate-soundbank.
# Dieses Skript: je Sprache ein AkAudioEvent (/FicsitTD/Audio/Play_TD_<Name>_<sprache>, Name = Wwise-Event) und eine Nachricht
# (/FicsitTD/Narrative/MSG_TD_<Name>_<sprache>) mit Ton und Untertiteln zu den Zeitstempeln aus zeiten.json.
# Vorher alte Dateien unter Content/Audio und Content/Narrative per Dateisystem loeschen.
import json, os
import unreal

LOG = []
def log(m):
    LOG.append(str(m)); unreal.log("FICSITTD| " + str(m))

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/Game/FactoryGame/Narrative", "/FicsitTD"], True)
VORLAGE = "/Game/FactoryGame/Narrative/Alien/AlienCommunication/Stage1/MSG_Alien_Communication_S1_M1"
SKRIPTE = os.path.dirname(os.path.abspath(__file__))
BANKINFO = r"E:\Modding\SatisfactoryModLoader\SatisfactoryModLoader_WwiseProject\GeneratedSoundBanks\Windows\FicsitTD.json"
TITEL = {"begruessung": {"de": "FICSIT Tower Defense: Einsatzbeginn", "en": "FICSIT Tower Defense: Deployment"}}

with open(BANKINFO, encoding="utf-8") as fh:
    bank = json.load(fh)["SoundBanksInfo"]["SoundBanks"][0]
EVENTS = {e["Name"]: e for e in bank["Events"]}
log(f"Bank {bank['ShortName']} ({bank['Id']}), Events {list(EVENTS)}")

def untertitel(text, zeit):
    st = unreal.MessageSubtitle()
    t = text.replace('"', "'")
    st.import_text(f'(Subtitle=INVTEXT("{t}"),SenderClass="/Game/FactoryGame/Interface/UI/Message/Sender/Sender_ADA.Sender_ADA_C",TimeStamp={zeit:.2f})')
    return st

def audio_event(name):
    e = EVENTS[name]
    a = AT.create_asset(name, "/FicsitTD/Audio", unreal.AkAudioEvent, unreal.AkAudioEventFactory())
    # Felder je nach Integrationsversion: EventInfo (WwiseEventInfo) mit Guid, ShortId, Name
    try:
        info = a.get_editor_property("EventInfo")
        info.import_text(f'(WwiseGuid={e["GUID"].strip("{}").replace("-", "")},WwiseShortId={e["Id"]},WwiseName="{name}")')
        a.set_editor_property("EventInfo", info)
        log(f"  {name}: EventInfo {a.get_editor_property('EventInfo').export_text()}")
    except Exception as ex:
        log(f"  {name}: EventInfo nicht gesetzt: {ex}")
        for p in ("WwiseShortId", "ShortId", "WwiseName"):
            try:
                log(f"    {p} = {a.get_editor_property(p)}")
            except Exception:
                pass
    EAL.save_loaded_asset(a, False)
    return a

for n, titel in TITEL.items():
    with open(os.path.join(SKRIPTE, "Audio", f"saetze_{n}.json"), encoding="utf-8") as fh:
        saetze = json.load(fh)
    for sprache in ("de", "en"):
        try:
            with open(rf"E:\Modding\Audio\FicsitTD\VO\{sprache}\{n}\zeiten.json", encoding="utf-8") as fh:
                zeiten = json.load(fh)["zeiten"]
            ev = audio_event(f"Play_TD_{n.capitalize()}_{sprache}")
            name = f"MSG_TD_{n.capitalize()}_{sprache}"
            m = EAL.duplicate_asset(VORLAGE, f"/FicsitTD/Narrative/{name}")
            if m is None:
                raise RuntimeError("Duplizieren fehlgeschlagen (alte Datei geloescht?)")
            liste = [untertitel(s, z) for s, z in zip(saetze[sprache], zeiten)]
            for feld, wert in (("mTitle", titel[sprache]), ("mMessage", "\n\n".join(saetze[sprache])), ("mSubtitles", liste),
                               ("mAudioEvent", ev), ("mCanBeQueued", True), ("mCanBeRepeated", True), ("mTrigger", None), ("mConditions", [])):
                try:
                    m.set_editor_property(feld, wert)
                except Exception as ex:
                    log(f"  {feld}: {ex}")
            EAL.save_loaded_asset(m, False)
            log(f"{name}: {len(liste)} Untertitel, Ton {m.get_editor_property('mAudioEvent')}")
        except Exception as ex:
            log(f"FEHLER {n} {sprache}: {ex}")
log("ENDE")
with open(r"E:\Modding\Logs\create_ada_messages.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
