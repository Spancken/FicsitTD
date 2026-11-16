"""FicsitTD Optik-Durchgang 16.11.2026: AO-Loecher auf sichtbaren Flaechen der gebackenen Teile (Sperrklinke).

Misst mit Blender (Satisfactory\\Modelle\\td_ao_sichtbar.py) je gebackenem Teil unter E:\\Modding\\Export\\*\\Gebacken den
Anteil sichtbarer Flaeche mit AO < 0,1. Kontaktflaechen und schmale Spalte gelten als verdeckt. Vergleich mit Tests\\ao_basis.json:
  FEHLER  ein Teil ist mehr als 3 Prozentpunkte schlechter als in der Basis, oder ein neues Teil liegt ueber 20 %
          (so haette der Fehler mit den mitgebackenen UCX-Koerpern angeschlagen: Bolzensockel 1,1 % -> 24,9 %)
  BEHOBEN ein Teil ist mehr als 3 Prozentpunkte besser -> Basis neu schreiben

Aufruf: python Tests\\pruefe_ao.py [--basis-schreiben]
Ohne Blender oder ohne Exportordner: Hinweis, kein Fehler (Rechner ohne Modellquellen).
"""
import json, os, subprocess, sys, tempfile

MOD = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASIS = os.path.join(MOD, "Tests", "ao_basis.json")
BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.1\blender.exe"
SKRIPT = r"D:\OneDrive\Claude\Claude Modding\Satisfactory\Modelle\td_ao_sichtbar.py"
SCHRITT, NEU_GRENZE = 3.0, 20.0
# gestrichene Bauten (Optik-Durchgang 16.11.: unveraendert gelassen)
IGNORIERT = ("RepairStation_", "DimensionalDepot_")


def messen():
    aus = os.path.join(tempfile.gettempdir(), "ficsittd_ao.json")
    r = subprocess.run([BLENDER, "-b", "-P", SKRIPT, "--", "--alle", "--json", aus], capture_output=True, text=True, errors="replace")
    if r.returncode != 0 or not os.path.exists(aus):
        raise RuntimeError((r.stderr or r.stdout)[-400:])
    return {e["teil"]: e["sichtbar_loch_pct"] for e in json.load(open(aus, encoding="utf-8"))
            if "fehler" not in e and not e["teil"].startswith(IGNORIERT)}


def main():
    if not (os.path.exists(BLENDER) and os.path.exists(SKRIPT)):
        print("HINWEIS Blender oder td_ao_sichtbar.py fehlt - AO nicht gemessen")
        print("AO: nicht gemessen")
        return 0
    ist = messen()
    if "--basis-schreiben" in sys.argv:
        json.dump(dict(sorted(ist.items())), open(BASIS, "w", encoding="utf-8"), indent=1)
        print(f"Basis geschrieben: {len(ist)} Teile")
        return 0
    basis = json.load(open(BASIS, encoding="utf-8")) if os.path.exists(BASIS) else {}
    fehler = behoben = 0
    for teil, pct in sorted(ist.items()):
        alt = basis.get(teil)
        if alt is None:
            if pct > NEU_GRENZE:
                print(f"FEHLER {teil}: {pct:.1f} % der sichtbaren Flaeche mit AO < 0,1 (neues Teil, Grenze {NEU_GRENZE:.0f} %)")
                fehler += 1
        elif pct > alt + SCHRITT:
            print(f"FEHLER {teil}: {pct:.1f} % sichtbare AO-Loecher, Basis {alt:.1f} % (mitgebackene UCX-Koerper? td_backen.py)")
            fehler += 1
        elif pct < alt - SCHRITT:
            print(f"BEHOBEN {teil}: {pct:.1f} % statt {alt:.1f} %")
            behoben += 1
    schlimmste = max(ist.items(), key=lambda e: e[1]) if ist else ("-", 0)
    print(f"AO: {len(ist)} Teile, {fehler} Fehler, {behoben} besser; hoechster Wert {schlimmste[0]} {schlimmste[1]:.1f} %")
    if behoben:
        print("-> Basis verkleinern: python Tests\\pruefe_ao.py --basis-schreiben")
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
