"""FicsitTD Optik-Durchgang 16.11.2026: eine Quelle fuer Anbauten.
Liest Satisfactory\\Modelle\\Anbauten.json (geschrieben von Modelle\\td_anbauten.py und td_klarraum.py aus den Blender-Modellen)
und erzeugt daraus
  Tests\\Anbauten.json                           Kopie im Repo (fuer die Pruefungen, Stand der Modelle)
  Source\\FicsitTD\\Private\\TDAnbauten.inl      Tabelle fuer ATDTurretBase (Statuslampe, Stromanschluss, Klarraum)
Aufruf: python Scripts\\erzeuge_anbauten.py [--pruefen]   (--pruefen: nur vergleichen, Exit 1 wenn veraltet)
"""
import json, os, sys

MOD = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QUELLE = r"D:\OneDrive\Claude\Claude Modding\Satisfactory\Modelle\Anbauten.json"
KOPIE = os.path.join(MOD, "Tests", "Anbauten.json")
INL = os.path.join(MOD, "Source", "FicsitTD", "Private", "TDAnbauten.inl")

# Schluessel in Anbauten.json -> mTurmName im Code
TURM = {"Bolzen": "Bolzenturm", "Gewehr": "Gewehrturm", "Nobelisk": "Nobelisk-Werfer", "Flamme": "Flammenwerfer", "Tesla": "Teslaspule",
        "Himmel": "Himmelsturm", "Singu": "Singularitaet", "Stoerfeld": "Stoerfeld", "Funkmast": "Funkmast", "Schild": "Schildgenerator",
        "Drohnenplatz": "Drohnenplatz", "Elektrozaun": "Elektrozaun", "Lockfeuer": "Lockfeuer", "Oelschlick": "Oelschlick"}
# Ausnahmen beim Klarraum: Zaunstuecke stossen der Laenge nach aneinander (Endpfosten ragen 10 cm, wie bisher)
KLARRAUM_X = {"Elektrozaun": (-400.0, 400.0)}


def v(x):
    return f"FVector({x[0]:.1f}f, {x[1]:.1f}f, {x[2]:.1f}f)"


def v2(x):
    return f"FVector2D({x[0]:.1f}f, {x[1]:.1f}f)"


def erzeuge(daten):
    zeilen = ["// Erzeugt von Scripts/erzeuge_anbauten.py aus Satisfactory/Modelle/Anbauten.json (Blender: td_anbauten.py, td_klarraum.py).",
              "// Nicht von Hand aendern. Optik-Durchgang 16.11.2026: Statuslampe auf der Konsole im Sockel, Stromanschluss im Anschlusskasten,",
              "// Klarraum = Sockel + Anbauten + Original-Eingang (asymmetrisch, cm, Bauteilraum).",
              "#pragma once", "",
              "struct FTDAnbau { const TCHAR* Turm; FVector Lampe; FVector Strom; FVector2D KlarraumMin; FVector2D KlarraumMax; };", "",
              "static const FTDAnbau TDAnbauTabelle[] = {"]
    for schl, e in sorted(daten.items()):
        if not e.get("strom") or not e.get("lampe") or "klarraum_min_cm" not in e:
            raise SystemExit(f"{schl}: unvollstaendig (strom/lampe/klarraum fehlen) - td_anbauten.py und td_klarraum.py laufen lassen")
        kmin, kmax = list(e["klarraum_min_cm"]), list(e["klarraum_max_cm"])
        if schl in KLARRAUM_X:
            kmin[0], kmax[0] = KLARRAUM_X[schl]
        zeilen.append(f'\t{{ TEXT("{TURM[schl]}"), {v(e["lampe"])}, {v(e["strom"])}, {v2(kmin)}, {v2(kmax)} }},')
    zeilen += ["};", ""]
    return "\n".join(zeilen)


def main():
    daten = json.load(open(QUELLE, encoding="utf-8"))
    inl = erzeuge(daten)
    kopie = json.dumps(daten, indent=1, ensure_ascii=False) + "\n"
    if "--pruefen" in sys.argv:
        alt_inl = open(INL, encoding="utf-8").read() if os.path.exists(INL) else ""
        alt_kopie = open(KOPIE, encoding="utf-8").read() if os.path.exists(KOPIE) else ""
        veraltet = [n for n, a, b in (("TDAnbauten.inl", alt_inl, inl), ("Tests/Anbauten.json", alt_kopie, kopie)) if a != b]
        print("Anbauten: " + ("veraltet: " + ", ".join(veraltet) if veraltet else "aktuell"))
        return 1 if veraltet else 0
    open(INL, "w", encoding="utf-8", newline="\r\n").write(inl)
    open(KOPIE, "w", encoding="utf-8").write(kopie)
    print(f"Anbauten: {len(daten)} Gebaeude -> {INL}, {KOPIE}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
