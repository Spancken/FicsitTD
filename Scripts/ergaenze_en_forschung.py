# A6 (22.11.2026): englische Texte der Forschung nach einem Umbau des Baums ergaenzen, ohne Spiel und ohne Editor.
# Die Uebersetzung (uebersetzung_en.json) haengt am vollen deutschen Text. create_research.py haengt an jede Beschreibung
# "Ab Welle", "Braucht" und "Abzweigung" an, deshalb aendern neue Eltern, Voraussetzungen oder Wellen viele Texte auf einmal.
# Dieses Skript rechnet die Texte des aktuellen Baums nach (create_research.py bis vor die Asset-Erzeugung, unreal als Attrappe)
# und baut das Englische aus den Teilen, die schon uebersetzt sind:
#   - Knotentext und Baumtext: Eintrag der JSON, sonst NEU_EN unten (fehlt beides: Fehler)
#   - Zeilen "Im Laden" und "Gut gegen": aus vorhandenen Paaren Zeile fuer Zeile
#   - "Ab Welle", "Braucht", "Abzweigung", Karte "Vorher": aus den englischen Knoten- und Baumnamen
# Herstellungskarten brauchen die Rezept-Assets (Editor); was sich dort aendert, steht in KARTEN_ERSETZEN.
# Aufruf: python Scripts/ergaenze_en_forschung.py          (zeigt, was fehlt)
#         python Scripts/ergaenze_en_forschung.py --schreiben (traegt es in uebersetzung_en.json ein)
# Danach: python Scripts/erzeuge_uebersetzung.py (TDTexteEn.inl)
import io, json, os, sys
from unittest import mock

HIER = os.path.dirname(os.path.abspath(__file__))
JSON = os.path.join(HIER, "uebersetzung_en.json")

BAUM_EN = {"Industrie": "Industry", "Ballistik": "Ballistics", "Elementar": "Elemental", "Festung": "Fortress"}

# Neue oder geaenderte deutsche Knoten-, Baum- und Kurztexte (A6 22.11.)
NEU_EN = {
    "Was deine Fabrik herstellen kann: Teile, Stahl, Öl, Strom und schnellere Bänder. Hier wird nichts ausgeschlossen. Sechs Einstiege: Fertigung, Kupfer, Bänder, Strom und zwei Rohstoffe.":
        "What your factory can produce: parts, steel, oil, power and faster belts. Nothing is excluded here. Six entry points: manufacturing, copper, belts, power and two raw resources.",
    "Kohlekraftwerk (75 MW statt 30 MW), Wasserextraktor und Rohre. Voraussetzung für Teslaspule und Raffinerie.":
        "Coal-Powered Generator (75 MW instead of 30 MW), Water Extractor and pipes. Prerequisite for the Tesla Coil and the Refinery.",
    "Gießerei (im Laden kaufbar), Stahlbarren aus Eisenerz und Kohle, Stahlrohre und Stahlträger. Für Nobelisken, panzerbrechende Munition, Aluminium und Band Mk3.":
        "Foundry (available in the Shop), Steel Ingot from Iron Ore and Coal, Steel Pipes and Steel Beams. For Nobelisks, Armor-Piercing Rifle Ammo, aluminum and Belt Mk.3.",
    "Schwefel abbauen und Schwarzpulver herstellen (Kohle + Schwefel im Assembler). Schwefel steckt außerdem in Brandmunition, Gas-Nobelisk und Kompaktkohle; diese Rezepte kommen mit den jeweiligen Knoten.":
        "Mine Sulfur and make Black Powder (Coal + Sulfur in the Assembler). Sulfur also goes into Incendiary Ammo, Gas Nobelisk and Compacted Coal; those recipes come with their respective nodes.",
    "Türme ohne Projektile: Strom (Teslaspule) und Feuer (Flammenwerfer). Tesla braucht das Kohlekraftwerk, der Flammenwerfer die Raffinerie.":
        "Turrets without projectiles: power (Tesla Coil) and fire (Flamethrower). Tesla requires Coal Power, the Flamethrower requires the Refinery.",
    "Neues Gebäude: Kuppel 20 m schluckt Schaden an Mauern, Türmen und Bauten (nicht am Kern), 3000 Schildpunkte, gebrochen 20 s Ladezeit. Braucht Schwerindustrie und dauernd 60 MW.":
        "New building: a 20 m dome absorbs damage to walls, turrets and buildings (not the Core), 3000 shield points, 20 s recharge when broken. Requires Heavy Industry and constantly needs 60 MW.",
    "Neues Gebäude: verbraucht reanimiertes SAM (1 Barren je 60 s), solange Gegner in 15 m sind, und bremst sie alle um 40 %. Braucht Mischtechnik und 80 MW.":
        "New building: consumes Reanimated SAM (1 ingot per 60 s) while enemies are within 15 m, and slows them all by 40%. Requires Blending Technology and 80 MW.",
    "Schwefel und Schwarzpulver": "Sulfur and Black Powder",
}

# Herstellungskarten: Schwarzpulver kommt jetzt mit dem Knoten Schwefel (HERSTELLER nimmt den ersten Knoten mit dem Rezept)
KARTEN_ERSETZEN = [
    ("Schwarzpulver: 1 Kohle + 1 Schwefel im Fabrikator (Assembler) (Forschung Nobelisk-Werfer).",
     "Schwarzpulver: 1 Kohle + 1 Schwefel im Fabrikator (Assembler) (Forschung Schwefel).",
     "Black Powder: 1 Coal + 1 Sulfur in the Assembler (Research: Nobelisk Launcher).",
     "Black Powder: 1 Coal + 1 Sulfur in the Assembler (Research: Sulfur)."),
]


def lade_baum():
    sys.modules["unreal"] = mock.MagicMock()
    src = io.open(os.path.join(HIER, "create_research.py"), encoding="utf-8").read()
    ns = {}
    exec(src[:src.index("def icon_von(")], ns)
    return ns


def zeilen_paare(tab):
    """Deutsche Zeile -> englische Zeile aus mehrzeiligen Paaren gleicher Zeilenzahl (Im Laden, Gut gegen)"""
    paare = {}
    for de, en in tab.items():
        if not en:
            continue
        d, e = de.split("\n"), en.split("\n")
        if len(d) == len(e):
            for a, b in zip(d, e):
                if a.startswith(("Im Laden: ", "Gut gegen: ")):
                    paare.setdefault(a, b)
    return paare


def main():
    tab = json.load(io.open(JSON, encoding="utf-8"))
    ns = lade_baum()
    paare = zeilen_paare(tab)
    fehlt, neu = [], {}

    # Knotentext allein steht oft nur als Anfang einer vollen Beschreibung in der Tabelle (vor der ersten Leerzeile)
    anfang = {}
    for de, en in tab.items():
        if en and "\n\n" in de and "\n\n" in en:
            anfang.setdefault(de.split("\n\n")[0], en.split("\n\n")[0])

    def en_von(de, melden=True):
        if de in neu:
            return neu[de]
        if tab.get(de):
            return tab[de]
        if de in NEU_EN:
            return NEU_EN[de]
        if de in anfang:
            return anfang[de]
        if melden:
            fehlt.append(de)
        return None

    def name_en(b, k):
        return en_von(ns["NAMEN"][(b, k)])

    for b_key, b_name, b_text, _, knoten in ns["BAEUME"]:
        if en_von(b_text):
            neu.setdefault(b_text, en_von(b_text))
        for k_key, k_name, k_text, _, eltern, _, _, _, _ in knoten:
            haupt_en = en_von(k_text)
            # Knotentext allein nur fuer neue Texte eintragen (Forschungsuebersicht im Kern-Menue)
            if haupt_en and k_text in NEU_EN:
                neu.setdefault(k_text, haupt_en)
            kurz = ns["KURZ"].get(k_key)
            if kurz and kurz in NEU_EN:
                neu.setdefault(kurz, NEU_EN[kurz])
            # Beschreibung im MAM
            de = ns["beschreibung"](b_key, k_key, k_text, eltern)
            if not tab.get(de) and haupt_en:
                zeilen = de.split("\n")
                aus = [haupt_en, ""]
                for z in zeilen[2:]:
                    if z.startswith(("Im Laden: ", "Gut gegen: ")):
                        if z not in paare:
                            fehlt.append(z)
                            aus = None
                            break
                        aus.append(paare[z])
                    elif z.startswith("Ab Welle: "):
                        aus.append("From wave: " + z[len("Ab Welle: "):])
                    elif z.startswith("Braucht: "):
                        teile = [f"{BAUM_EN[vb]} > {name_en(vb, vk)}" for vb, vk in ns["braucht"](b_key, k_key, eltern)]
                        aus.append("Requires: " + ", ".join(teile))
                    elif z.startswith("Abzweigung: "):
                        aus.append("Branch: excludes " + ", ".join(name_en(b_key, a) for a in ns["andere_von"](b_key, k_key)) + " for this operation")
                    else:
                        fehlt.append(z)
                        aus = None
                        break
                if aus and None not in aus:
                    neu[de] = "\n".join(aus)
            # Karte "Vorher" (create_research.herstellungs_karten)
            vor = ns["braucht"](b_key, k_key, eltern)
            welle = ns["eff_welle"](b_key, k_key)
            if vor or welle:
                namen = [ns["NAMEN"][(vb, vk)] for vb, vk in vor]
                titel = ("Vorher: " + ", ".join(namen)) if namen else f"Ab Welle {welle}"
                titel_en = ("Requires: " + ", ".join(name_en(vb, vk) or "?" for vb, vk in vor)) if namen else f"From wave {welle}"
                text = [f"{ns['TITEL'][vb]} > {ns['NAMEN'][(vb, vk)]}" for vb, vk in vor] + ([f"Erforschbar ab Welle {welle}."] if welle else [])
                text_en = [f"{BAUM_EN[vb]} > {name_en(vb, vk) or '?'}" for vb, vk in vor] + ([f"Researchable from wave {welle}."] if welle else [])
                if not tab.get(titel):
                    neu[titel] = titel_en
                if not tab.get("\n".join(text)):
                    neu["\n".join(text)] = "\n".join(text_en)
    # Herstellungskarten
    for de_alt, de_neu, en_alt, en_neu in KARTEN_ERSETZEN:
        for de, en in list(tab.items()):
            if de_alt in de and en and en_alt in en:
                k = de.replace(de_alt, de_neu)
                if not tab.get(k):
                    neu[k] = en.replace(en_alt, en_neu)
    neu = {k: v for k, v in neu.items() if not tab.get(k) and v}
    for k, v in neu.items():
        if "> ?" in v or ": ?" in v:
            fehlt.append("Name fehlt in: " + k)
    print(f"{len(neu)} neue englische Texte, {len(set(fehlt))} deutsche Teile ohne Englisch")
    for f in sorted(set(fehlt)):
        print("  FEHLT:", f[:140])
    for k, v in list(neu.items())[:400]:
        print("  +", k.replace("\n", " | ")[:110], "\n      =>", v.replace("\n", " | ")[:110])
    if "--schreiben" in sys.argv and not fehlt:
        tab.update(neu)
        io.open(JSON, "w", encoding="utf-8", newline="\n").write(json.dumps(tab, ensure_ascii=False, indent=2) + "\n")   # Format wie create_shop.py (kleiner Diff)
        print(f"eingetragen in {JSON}")
    elif "--schreiben" in sys.argv:
        print("NICHT geschrieben: erst die fehlenden Teile in NEU_EN eintragen")
        sys.exit(1)


if __name__ == "__main__":
    main()
