# 22.11.2026 (Marius: vollstaendiger Baum im Handbuch hinter einem Spoiler, dazu ein Planer mit Wellen-Schieber fuer MAM und Talente):
# Daten fuer docs/baeume.js aus denselben Quellen wie das Spiel, damit das Handbuch nie vom Code abweicht.
#   - MAM: Scripts/create_research.py (Plaetze, Eltern, Voraussetzungen, Freigabe-Welle, Preis, Gabelungen, Texte)
#   - Englisch: Scripts/uebersetzung_en.json (wie TDTexteEn.inl), Knotentexte ueber den Anfang der vollen Beschreibung
#   - Talente: Source/FicsitTD/Private/TDPionier.cpp (TDZweigeDe/TDZweigeEn)
#   - Zahlen: Source/FicsitTD/Private/TDBalance.cpp (Festplatten je Welle, Talentpunkte je Boss, hoechstens)
# Aufruf: python Scripts/erzeuge_doku_baeume.py           (schreibt docs/baeume-daten.js)
#         python Scripts/erzeuge_doku_baeume.py --pruefe  (Exit 1, wenn die Datei veraltet ist; fuer pruefe_mod.py)
import io, json, os, re, sys

HIER = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HIER)
ZIEL = os.path.join(MOD, "docs", "baeume-daten.js")
sys.path.insert(0, HIER)
import ergaenze_en_forschung as en   # lade_baum (unreal als Attrappe), BAUM_EN, NEU_EN

# Turm- und Gebaeudebilder im Handbuch (docs/img, 256 px)
BILD = {
    "Durchschlagbolzen": "BoltTurret", "Gewehrturm": "GunTurret", "NobeliskWerfer": "NobeliskLauncher", "Himmelsturm": "FlakTurret",
    "Singularitaet": "SingularityLauncher", "Teslaspule": "TeslaCoil", "Flammenwerfer": "FlameTurret", "Oelschlick": "SlickPlate",
    "Elektrozaun": "ConductorWall", "Tor": "Gate", "Lockfeuer": "LureBeacon", "Funkmast": "RadioMast", "Schildgenerator": "ShieldGenerator",
    "Stoerfeld": "FieldProjector", "Reparaturdrohnen": "DronePort", "Kampfdrohnen": "DronePort", "MauerMk2": "DefenseWall", "KernPanzer": "DefenseCore",
}


def balance(text, schluessel, standard):
    m = re.search(r'\{ TEXT\("' + re.escape(schluessel) + r'"\), ([-\d.]+)f \}', text)
    return float(m.group(1)) if m else standard


def talente(quelle):
    """Vier Zweige je Sprache: (Key, Name, Untertitel, [5 Stufen])"""
    aus = {}
    for sprache, funktion in (("de", "TDZweigeDe"), ("en", "TDZweigeEn")):
        block = quelle[quelle.index(f"static const TArray<FTDTalentZweig>& {funktion}()"):]
        block = block[:block.index("return Liste;")]
        zweige = []
        for m in re.finditer(r'\{ TEXT\("(\w+)"\), TEXT\("([^"]+)"\), TEXT\("([^"]+)"\),\s*\{(.*?)\} \}', block, re.S):
            stufen = re.findall(r'TEXT\("((?:[^"\\]|\\.)*)"\)', m.group(4))
            zweige.append({"key": m.group(1), "name": m.group(2), "unter": m.group(3), "stufen": stufen})
        aus[sprache] = zweige
    if len(aus["de"]) != 4 or any(len(z["stufen"]) != 5 for z in aus["de"] + aus["en"]):
        sys.exit("FEHLER: Talenttabelle in TDPionier.cpp nicht wie erwartet (4 Zweige mit je 5 Stufen)")
    return aus


def daten():
    ns = en.lade_baum()
    tab = json.load(io.open(os.path.join(HIER, "uebersetzung_en.json"), encoding="utf-8"))
    anfang = {}
    for de, e in tab.items():
        if e and "\n\n" in de and "\n\n" in e:
            anfang.setdefault(de.split("\n\n")[0], e.split("\n\n")[0])

    def auf_en(de):
        return tab.get(de) or en.NEU_EN.get(de) or anfang.get(de) or de

    gruppe_von = {}
    for nr, (g_b, glieder) in enumerate(ns["GRUPPEN"]):
        for g in glieder:
            gruppe_von[(g_b, g)] = nr
    baeume = []
    for b_key, b_name, b_text, _, knoten in ns["BAEUME"]:
        liste = []
        for k_key, k_name, k_text, (x, y), eltern, _, _, _, _ in knoten:
            liste.append({
                "key": k_key, "x": x, "y": y, "eltern": eltern,
                "braucht": [vk for vb, vk in ns["VORAUSSETZUNGEN"].get((b_key, k_key), [])],
                "welle": ns["eff_welle"](b_key, k_key), "fp": ns["preis"](b_key, k_key),
                "gabel": gruppe_von.get((b_key, k_key)),
                "bild": BILD.get(k_key),
                "de": {"name": k_name, "kurz": ns["KURZ"].get(k_key, ""), "text": k_text.replace(ns["GABEL"], "").replace(ns["DOKTRIN"], "")},
                "en": {"name": auf_en(k_name), "kurz": tab.get(ns["KURZ"].get(k_key, "")) or en.NEU_EN.get(ns["KURZ"].get(k_key, ""), ""),
                       "text": re.sub(r"^(BRANCH|DOCTRINE)[^:]*: (FICSIT funds only one development line per operation\. )?", "", auf_en(k_text))},
            })
        baeume.append({"key": b_key, "de": {"name": ns["TITEL"][b_key], "text": b_text},
                       "en": {"name": en.BAUM_EN[b_key], "text": auf_en(b_text)}, "knoten": liste})
    bal = io.open(os.path.join(MOD, "Source", "FicsitTD", "Private", "TDBalance.cpp"), encoding="utf-8-sig").read()
    pionier = io.open(os.path.join(MOD, "Source", "FicsitTD", "Private", "TDPionier.cpp"), encoding="utf-8-sig").read()
    return {
        "quelle": "Scripts/erzeuge_doku_baeume.py",
        "baeume": baeume,
        "gabeln": [glieder for _, glieder in ns["GRUPPEN"]],
        "zahlen": {
            "fpJeWelle": balance(bal, "Einsatz.FP.JeWelle", 2), "fpKern75": balance(bal, "Einsatz.FP.KernAb75", 1),
            "fpBoss": balance(bal, "Einsatz.FP.Boss", 3), "bonusFestplatten": balance(bal, "Endlos.Bonus.Festplatten", 5),
            "festplattePreis": balance(bal, "Einsatz.Laden.Festplatte.Grundpreis", 40), "festplatteAufschlag": balance(bal, "Einsatz.Laden.Festplatte.Aufschlag", 0.25),
            "festplatteRueckgabe": balance(bal, "Einsatz.Laden.Festplatte.Rueckgabe", 12),
            "punkteJeBoss": balance(bal, "Pionier.PunkteJeBoss", 1), "punkteMax": balance(bal, "Pionier.MaxPunkte", 15),
            "bossAlle": 10, "wellenEinsatz": 50,
        },
        "talente": talente(pionier),
    }


def inhalt():
    d = json.dumps(daten(), ensure_ascii=False, separators=(",", ":"))
    return ("// Erzeugt von Scripts/erzeuge_doku_baeume.py aus create_research.py, TDPionier.cpp und TDBalance.cpp - nicht von Hand bearbeiten\n"
            "window.TD_DATEN = " + d + ";\n")


def main():
    neu = inhalt()
    alt = io.open(ZIEL, encoding="utf-8").read() if os.path.exists(ZIEL) else ""
    if "--pruefe" in sys.argv:
        if alt != neu:
            print("docs/baeume-daten.js ist veraltet: python Scripts/erzeuge_doku_baeume.py")
            sys.exit(1)
        print("docs/baeume-daten.js aktuell")
        return
    io.open(ZIEL, "w", encoding="utf-8", newline="\n").write(neu)
    d = json.loads(neu[neu.index("=") + 1:].rstrip().rstrip(";"))
    print(f"docs/baeume-daten.js: {sum(len(b['knoten']) for b in d['baeume'])} Knoten in {len(d['baeume'])} Baeumen, "
          f"{len(d['talente']['de'])} Talentzweige, {len(neu) // 1024} KB")


if __name__ == "__main__":
    main()
