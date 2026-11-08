# 0.85 L1: Uebersetzungstabelle der Asset-Texte (Deutsch -> Englisch) fuer TDSprache.cpp erzeugen.
# Quelle: Scripts/uebersetzung_en.json  {"deutscher Text": "English text", ...}
# Ziel:   Source/FicsitTD/Private/TDTexteEn.inl  ({ TEXT("..."), TEXT("...") }, je Zeile)
# Fehlende Texte schreibt das Spiel bei englischer Sprache nach %LOCALAPPDATA%/FactoryGame/Saved/FicsitTD/Uebersetzung-fehlt.json.
# Aufruf: python erzeuge_uebersetzung.py            (Tabelle erzeugen)
#         python erzeuge_uebersetzung.py --fehlt    (fehlende Texte aus dem Spiel in die JSON uebernehmen, Englisch leer)
import json, os, sys

HIER = os.path.dirname(os.path.abspath(__file__))
QUELLE = os.path.join(HIER, "uebersetzung_en.json")
ZIEL = os.path.join(HIER, "..", "Source", "FicsitTD", "Private", "TDTexteEn.inl")
FEHLT = os.path.join(os.environ.get("LOCALAPPDATA", ""), "FactoryGame", "Saved", "FicsitTD", "Uebersetzung-fehlt.json")


def lade():
    if not os.path.exists(QUELLE):
        return {}
    with open(QUELLE, encoding="utf-8") as f:
        return json.load(f)


def literal(s):
    s = s.replace("\\", "\\\\").replace("\"", "\\\"").replace("\r", "\\r").replace("\n", "\\n").replace("\t", "\\t")
    return 'TEXT("' + s + '")'


def main():
    tab = lade()
    if "--fehlt" in sys.argv:
        with open(FEHLT, encoding="utf-8") as f:
            neu = json.load(f)
        n = 0
        for s in neu:
            if s not in tab:
                tab[s] = ""
                n += 1
        with open(QUELLE, "w", encoding="utf-8", newline="\n") as f:
            json.dump(tab, f, ensure_ascii=False, indent=1, sort_keys=True)
        print(f"{n} neue Texte uebernommen, {sum(1 for v in tab.values() if not v)} ohne Englisch")
        return
    zeilen = []
    leer = 0
    for de, en in sorted(tab.items()):
        if not en:
            leer += 1
            continue
        if de == en:
            continue
        zeilen.append("\t\t{ " + literal(de) + ", " + literal(en) + " },")
    with open(ZIEL, "w", encoding="utf-8-sig", newline="\r\n") as f:
        f.write("// erzeugt von Scripts/erzeuge_uebersetzung.py - nicht von Hand bearbeiten\n")
        f.write("\n".join(zeilen) + ("\n" if zeilen else ""))
    print(f"{len(zeilen)} Uebersetzungen geschrieben, {leer} ohne Englisch")


if __name__ == "__main__":
    main()
