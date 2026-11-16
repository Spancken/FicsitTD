"""FicsitTD: Optik-Pruefung ohne Editor und ohne Spiel (Optik-Durchgang 16.11.2026, P0/P2).

Bericht und Ursachen: Satisfactory\\Notizen\\Optik-Durchgang-2026-11-16.md
  python Tests\\pruefe_optik_statisch.py            # Ausgabe FEHLER/HINWEIS, Exit 1 bei Fehlern
  python Tests\\pruefe_optik_statisch.py --json     # zusaetzlich maschinenlesbar

Prueft
  A  Anbauten (C++-Konstruktoren, OriginalTeil/mLampenOrt/mKlarraumHalb) gegen den Klarraum der Bauvorschau:
     jedes Original-Anschlussteil muss im Klarraum liegen, der Bandeingang darf den Sockel nicht ueberragen
     (Sockelmasse aus Tests\\Modellmasse.json, geschrieben von Scripts\\pruefe_optik.py im Editor).
     Entscheidung Marius 16.11.: kein Original-Statusmast (eigener Lampentraeger), kein Wand-Strommast (Anschlusskasten im Sockel).
  B  Backfarben (Satisfactory\\Modelle\\td_backen.py, SLOTS): kein Slot dunkler als die Spielwerte (Gummi 0,036 linear).
  C  Gebackene Texturen (E:\\Modding\\Export\\*\\Gebacken): grosse fast schwarze Flaechen in der Albedo, Flaechen ohne AO.
"""
import argparse
import glob
import json
import math
import os
import re
import sys

MOD = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QUELLE = os.path.join(MOD, "Source", "FicsitTD", "Private")
MASSE = os.path.join(MOD, "Tests", "Modellmasse.json")
BASIS = os.path.join(MOD, "Tests", "optik_basis.json")
ANBAUTEN = os.path.join(MOD, "Tests", "Anbauten.json")
BACKEN = r"D:\OneDrive\Claude\Claude Modding\Satisfactory\Modelle\td_backen.py"
EXPORT = r"E:\Modding\Export"

# Masse der Spielmodelle im eigenen Raum (cm, vor der Gier), gemessen am FModel-Export / Blender-Vorschau 16.11.
ORIGINAL = {
    "Eingang": ((-109, 109), (-100, 38), (55, 281)),     # SM_Input_01, +Y nach aussen
    "Rohr": ((-100, 100), (-84, 50), (-100, 100)),       # SM_Pipe_InPut_01
    "Strom": ((10, 100), (-28, 28), (-28, 28)),          # SM_PowerPoleWall_MK1, ragt nach +X
    "Mast": ((-19, 19), (-42, 0), (0, 333)),             # SM_ProductionLight_01, Lampenkopf 2,9-3,33 m, ~28 cm neben dem Ursprung
}
KLARRAUM_STANDARD = (205.0, 205.0)
TOLERANZ = 5.0
GUMMI_MIN = 0.03          # Spiel: "Rubbe Colour" 0,036 linear (MI_InPuts), dunkles Metall 0,041
ALBEDO_MIN = 0.02
# gewollt dunkle Flaechen (Teil -> Grund)
DUNKEL_GEWOLLT = {"SlickPlate_Base": "Oelfilm M_TD_Oil"}

fehler, hinweise = [], []
def FEHLER(t): fehler.append(t)
def HINWEIS(t): hinweise.append(t)


def zahl(s, konst):
    s = s.strip().rstrip("f")
    try:
        return float(s)
    except ValueError:
        neg = s.startswith("-")
        w = konst.get(s.lstrip("-"))
        return None if w is None else (-w if neg else w)


def konstanten(text):
    k = {}
    for m in re.finditer(r"(?:static\s+)?(?:constexpr|const)\s+float\s+(\w+)\s*=\s*(-?[\d.]+)f?", text):
        k[m.group(1)] = float(m.group(2))
    return k


def konstruktoren():
    """Klasse -> {klarraum, lampe, teile[(art, ort, gier)]} aus den C++-Konstruktoren"""
    erg = {}
    for pfad in glob.glob(os.path.join(QUELLE, "*.cpp")):
        text = open(pfad, encoding="utf-8", errors="replace").read()
        konst = konstanten(text)
        for m in re.finditer(r"^ATD(\w+)::ATD\1\(\)\s*\{", text, re.M):
            # Rumpf bis zur schliessenden Klammer auf Zeilenanfang
            ende = text.find("\n}", m.end())
            rumpf = text[m.end():ende]
            name = m.group(1)
            d = {"datei": os.path.basename(pfad), "teile": []}
            tn = re.search(r'mTurmName\s*=\s*TEXT\("([^"]+)"\)', rumpf)
            d["turm"] = tn.group(1) if tn else None
            k = re.search(r"mKlarraumHalb\s*=\s*FVector2D\(([^,]+),([^)]+)\)", rumpf)
            if k:
                d["klarraum"] = (zahl(k.group(1), konst), zahl(k.group(2), konst))
            h = re.search(r"mKlarraumHoehe\s*=\s*([\d.]+)", rumpf)
            if h:
                d["hoehe"] = float(h.group(1))
            l = re.search(r"mLampenOrt\s*=\s*FVector\(([^,]+),([^,]+),([^)]+)\)", rumpf)
            if l:
                d["lampe"] = tuple(zahl(x, konst) for x in l.groups())
            for t in re.finditer(r"OriginalTeil\(TDOriginal::(\w+),\s*FVector\(([^,]+),([^,]+),([^)]+)\),\s*(-?[\d.]+)f?\)", rumpf):
                d["teile"].append((t.group(1), tuple(zahl(x, konst) for x in t.group(2, 3, 4)), float(t.group(5))))
            erg[name] = d
    return erg


def box(art, ort, gier):
    (x0, x1), (y0, y1), (z0, z1) = ORIGINAL[art]
    c, s = math.cos(math.radians(gier)), math.sin(math.radians(gier))
    ecken = [(x * c - y * s + ort[0], x * s + y * c + ort[1]) for x in (x0, x1) for y in (y0, y1)]
    return (min(e[0] for e in ecken), max(e[0] for e in ecken)), (min(e[1] for e in ecken), max(e[1] for e in ecken)), (z0 + ort[2], z1 + ort[2])


def anbau_daten():
    """mTurmName -> Eintrag aus Tests/Anbauten.json (Blender-Modell, Scripts/erzeuge_anbauten.py)"""
    if not os.path.exists(ANBAUTEN):
        return {}
    ns = {"__name__": "modul", "__file__": os.path.join(MOD, "Scripts", "erzeuge_anbauten.py")}
    exec(open(os.path.join(MOD, "Scripts", "erzeuge_anbauten.py"), encoding="utf-8").read(), ns)
    daten = json.load(open(ANBAUTEN, encoding="utf-8"))
    erg = {}
    for schl, e in daten.items():
        e = dict(e)
        if schl in ns["KLARRAUM_X"] and "klarraum_min_cm" in e:
            e["klarraum_min_cm"] = [ns["KLARRAUM_X"][schl][0], e["klarraum_min_cm"][1]]
            e["klarraum_max_cm"] = [ns["KLARRAUM_X"][schl][1], e["klarraum_max_cm"][1]]
        if schl in ns.get("KLARRAUM_FEST", {}):
            e["klarraum_min_cm"], e["klarraum_max_cm"] = map(list, ns["KLARRAUM_FEST"][schl])
        erg[ns["TURM"][schl]] = e
    # erzeugte Tabelle aktuell?
    import subprocess
    r = subprocess.run([sys.executable, os.path.join(MOD, "Scripts", "erzeuge_anbauten.py"), "--pruefen"], capture_output=True, text=True)
    if r.returncode != 0:
        FEHLER(f"TDAnbauten.inl / Tests/Anbauten.json passen nicht zu Modelle/Anbauten.json: {r.stdout.strip()} (python Scripts\\erzeuge_anbauten.py)")
    return erg


def pruefe_anbauten():
    masse = json.load(open(MASSE, encoding="utf-8")) if os.path.exists(MASSE) else {}
    if not masse:
        HINWEIS(f"{MASSE} fehlt: Sockelhoehen unbekannt (einmal Scripts\\pruefe_optik.py im Editor laufen lassen)")
    anbau = anbau_daten()
    # Bauten ohne ATDTurretBase (Tor): Lampe und Strom muessen im eigenen Klarraum liegen
    turmnamen = {d.get("turm") for d in konstruktoren().values()}
    for tn, a in sorted(anbau.items()):
        if tn in turmnamen or "klarraum_min_cm" not in a:
            continue
        kmin, kmax = a["klarraum_min_cm"], a["klarraum_max_cm"]
        for wo, p in (("Statuslampe", a.get("lampe")), ("Stromanschluss", a.get("strom"))):
            if not p:
                FEHLER(f"{tn}: Anbauten.json ohne {wo}")
            elif not (kmin[0] - TOLERANZ <= p[0] <= kmax[0] + TOLERANZ and kmin[1] - TOLERANZ <= p[1] <= kmax[1] + TOLERANZ):
                FEHLER(f"{tn}: {wo} {p} ausserhalb des Klarraums {kmin}..{kmax}")
    # Original-Strommast oder -Statusmast zur Laufzeit angebaut (z. B. NewObject + SetStaticMesh in BeginPlay)?
    for datei in sorted(glob.glob(os.path.join(MOD, "Source", "FicsitTD", "Private", "*.cpp"))):
        text = open(datei, encoding="utf-8", errors="replace").read()
        for m in re.finditer(r"(SetStaticMesh|OriginalTeil)\([^;]*TDOriginal::(Strom|Mast)", text):
            FEHLER(f"{os.path.basename(datei)}: baut TDOriginal::{m.group(2)} an (Entscheidung 16.11.: Anschlusskasten/Lampentraeger im Modell)")
    for name, d in sorted(konstruktoren().items()):
        a = anbau.get(d.get("turm") or "")
        if a:
            kmin, kmax = a["klarraum_min_cm"], a["klarraum_max_cm"]
            if not a.get("lampe") or not a.get("strom"):
                FEHLER(f"{name}: Anbauten.json ohne Lampe/Strom")
            else:
                for wo, p in (("Statuslampe", a["lampe"]), ("Stromanschluss", a["strom"])):
                    if not (kmin[0] - TOLERANZ <= p[0] <= kmax[0] + TOLERANZ and kmin[1] - TOLERANZ <= p[1] <= kmax[1] + TOLERANZ):
                        FEHLER(f"{name}: {wo} {p} ausserhalb des Klarraums {kmin}..{kmax}")
        else:
            if d["teile"]:
                HINWEIS(f"{name}: kein Eintrag in Anbauten.json, Klarraum aus dem Konstruktor")
            kx, ky = d.get("klarraum") or KLARRAUM_STANDARD
            if kx is None or ky is None:
                HINWEIS(f"{name}: Klarraum ueber Konstante, nicht auswertbar")
                continue
            kmin, kmax = [-kx, -ky], [kx, ky]
        if not d["teile"]:
            continue
        sockel = masse.get(f"Build_TD_{name}")
        for art, ort, gier in d["teile"]:
            if None in ort or art not in ORIGINAL:
                HINWEIS(f"{name}: {art} nicht auswertbar ({ort})")
                continue
            (x0, x1), (y0, y1), (z0, z1) = box(art, ort, gier)
            raus = max(kmin[0] - x0, x1 - kmax[0], kmin[1] - y0, y1 - kmax[1], 0.0)
            kx, ky = max(-kmin[0], kmax[0]), max(-kmin[1], kmax[1])
            if raus > TOLERANZ:
                FEHLER(f"{name}: {art} ragt {raus:.0f} cm aus dem Klarraum (+-{kx:.0f}/{ky:.0f}) - Nachbarn bauen hinein, am Fundamentrand haengt es in der Luft ({d['datei']})")
            if art == "Mast":
                FEHLER(f"{name}: Original-Statusmast SM_ProductionLight_01 (Entscheidung 16.11.: eigener Lampentraeger im Sockel; die Mastleuchte geht nie an)")
                if d.get("lampe") and None not in d["lampe"]:
                    lx, ly, lz = d["lampe"]
                    kopf = (ort[0], ort[1] + (28.0 if abs((gier % 360) - 180) < 1 else -28.0), 311.0)
                    abst = math.dist((lx, ly, lz), kopf)
                    if abst > 10:
                        FEHLER(f"{name}: Statuslampe {abst:.0f} cm neben dem Lampenkopf des Masts (haengt in der Luft)")
            if art == "Strom":
                FEHLER(f"{name}: Wand-Strommast SM_PowerPoleWall_MK1 ragt {x1 - x0 if abs(gier) % 180 < 1 else y1 - y0:.0f} cm aus der Wand (Entscheidung 16.11.: Anschlusskasten im Sockel)")
            if art == "Eingang" and sockel:
                oben = sockel["sockel_max"][2]
                if z1 > oben + 20:
                    FEHLER(f"{name}: Bandeingang {z1:.0f} cm hoch, Sockel nur {oben} cm (Entscheidung 16.11.: erhoehter Einlaufblock)")


def pruefe_backfarben():
    if not os.path.exists(BACKEN):
        HINWEIS(f"{BACKEN} nicht gefunden")
        return
    text = open(BACKEN, encoding="utf-8").read()
    for m in re.finditer(r'"(M_TD_\w+)":\s*\(\(([\d.]+),\s*([\d.]+),\s*([\d.]+)\)', text):
        slot, rgb = m.group(1), [float(x) for x in m.group(2, 3, 4)]
        hell = 0.2126 * rgb[0] + 0.7152 * rgb[1] + 0.0722 * rgb[2]
        grenze = GUMMI_MIN if "Rubber" in slot else ALBEDO_MIN
        if hell < grenze and slot != "M_TD_Oil":
            FEHLER(f"td_backen.py: {slot} Albedo {hell:.3f} linear < {grenze} (Spielwert Gummi 0,036) -> grosse Flaechen werden im Spiel schwarz")


def inseln(maske, b, h):
    """zusammenhaengende Flaechen (4er-Nachbarschaft) einer Maske; liefert Listen von Indizes"""
    gesehen = bytearray(b * h)
    for start in range(b * h):
        if not maske[start] or gesehen[start]:
            continue
        stapel, insel = [start], []
        gesehen[start] = 1
        while stapel:
            i = stapel.pop()
            insel.append(i)
            x, y = i % b, i // b
            for j in ((i - 1) if x > 0 else -1, (i + 1) if x < b - 1 else -1, (i - b) if y > 0 else -1, (i + b) if y < h - 1 else -1):
                if j >= 0 and maske[j] and not gesehen[j]:
                    gesehen[j] = 1
                    stapel.append(j)
        yield insel


def pruefe_texturen():
    try:
        from PIL import Image
    except ImportError:
        HINWEIS("Pillow fehlt: Texturpruefung uebersprungen (pip install pillow)")
        return
    for alb_pfad in sorted(glob.glob(os.path.join(EXPORT, "*", "Gebacken", "T_TD_*_Albedo.png"))):
        teil = os.path.basename(alb_pfad)[5:-11]
        aom_pfad = alb_pfad.replace("_Albedo.png", "_AOMasks.png")
        n = 256
        alb = Image.open(alb_pfad).convert("RGB").resize((n, n), Image.NEAREST).load()
        aom = Image.open(aom_pfad).convert("RGB").resize((n, n), Image.NEAREST).load() if os.path.exists(aom_pfad) else None
        # Flaechen = zusammenhaengende Texel mit der Eigenschaft selbst (UV-Inseln wachsen ueber den Bake-Rand zusammen)
        belegt = [alb[i % n, i // n] != (0, 0, 0) or (aom is not None and aom[i % n, i // n] != (0, 0, 0)) for i in range(n * n)]
        dunkel = [1 if belegt[i] and max(alb[i % n, i // n]) < 40 else 0 for i in range(n * n)]
        dunkel_gross, ohne_ao = [], []
        for flaeche in inseln(dunkel, n, n):
            anteil = len(flaeche) / (n * n)
            if anteil >= 0.008:
                dunkel_gross.append(f"{anteil * 100:.1f} %")
        if aom:
            ohne = [1 if belegt[i] and aom[i % n, i // n][0] < 25 else 0 for i in range(n * n)]
            for flaeche in inseln(ohne, n, n):
                anteil = len(flaeche) / (n * n)
                if anteil >= 0.01:
                    ohne_ao.append(f"{anteil * 100:.1f} %")
        if dunkel_gross and teil in DUNKEL_GEWOLLT:
            HINWEIS(f"{teil}: dunkle Flaechen gewollt ({DUNKEL_GEWOLLT[teil]})")
        elif dunkel_gross:
            FEHLER(f"{teil}: {len(dunkel_gross)} grosse fast schwarze Flaechen in der Albedo ({', '.join(dunkel_gross[:4])} des Atlas)")
        if ohne_ao:
            # Unterseiten haben zu Recht kein AO; Sägezahn-Fehler auf Deckflaechen ebenfalls hier -> Hinweis, im Foto bestaetigen
            HINWEIS(f"{teil}: {len(ohne_ao)} grosse Flaechen fast ohne AO: {', '.join(ohne_ao[:3])}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--ohne-texturen", action="store_true")
    ap.add_argument("--streng", action="store_true", help="Basis ignorieren, jeder Fehler zaehlt")
    ap.add_argument("--basis-schreiben", action="store_true")
    a = ap.parse_args()
    pruefe_anbauten()
    pruefe_backfarben()
    if not a.ohne_texturen:
        pruefe_texturen()
    # Sperrklinke: bekannte Fehler (Stand 16.11., werden im Optik-Durchgang abgebaut) machen die Pipeline nicht rot, neue schon.
    # --basis-schreiben legt den aktuellen Stand als Basis ab (nur wenn Fehler behoben wurden, nie um neue zu verstecken).
    bekannt = set(json.load(open(BASIS, encoding="utf-8"))) if os.path.exists(BASIS) and not a.streng else set()
    neu = [f for f in fehler if f not in bekannt]
    behoben = sorted(bekannt - set(fehler))
    for h in hinweise:
        print("HINWEIS", h)
    for f in fehler:
        print("FEHLER" if f in neu else "BEKANNT", f)
    for b in behoben:
        print("BEHOBEN", b)
    print(f"Optik statisch: {len(neu)} neue Fehler, {len(fehler) - len(neu)} bekannte, {len(behoben)} behoben, {len(hinweise)} Hinweise")
    if behoben:
        print("-> Basis verkleinern: python Tests\\pruefe_optik_statisch.py --basis-schreiben")
    if a.basis_schreiben:
        if neu and bekannt:
            print("Basis NICHT geschrieben: es gibt neue Fehler")
            return 1
        json.dump(sorted(fehler), open(BASIS, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
        print(f"Basis geschrieben: {len(fehler)} Eintraege")
    if a.json:
        print(json.dumps({"neu": neu, "bekannt": len(fehler) - len(neu), "behoben": behoben, "hinweise": hinweise}, ensure_ascii=False))
    return 1 if neu else 0


if __name__ == "__main__":
    sys.exit(main())
