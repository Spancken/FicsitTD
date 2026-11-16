"""FicsitTD: Konsistenz von Code, Doku, Simulation und Tests (Deploy-Straße, ENTWICKLUNG.md Abschnitt 4).

  python pruefe_konsistenz.py --stufe schnell|voll|release   # Exit 0 ok, 1 Fehler
  python pruefe_konsistenz.py --changelog                    # Changelog-Entwurf (EN/DE) aus den Änderungszetteln

In Stufe "schnell" sind fehlende Doku/Simulation nur HINWEIS, in "voll" und "release" FEHLER.
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

MOD = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZETTEL = os.path.join(MOD, "Aenderungen")
QUELLE = os.path.join(MOD, "Source", "FicsitTD")
SIM_STAND = os.path.join(MOD, "Tools", "Simulator", "sim_stand.json")
PLAENE = os.path.join(MOD, "Tests", "Plaene")
PFLICHT = ["titel", "typ", "modi", "sichtbar", "doku", "simulation", "tests"]
TYPEN = {"neu", "geaendert", "behoben", "balance", "intern"}
MODI = {"alle", "einsatz", "endlos", "abwehr", "aus"}

fehler, hinweise = [], []


def git(*args):
    r = subprocess.run(["git", "-C", MOD, *args], capture_output=True, text=True, encoding="utf-8")
    return r.stdout.strip() if r.returncode == 0 else ""


def lies_zettel():
    alle = []
    for name in sorted(os.listdir(ZETTEL)):
        p = os.path.join(ZETTEL, name)
        if not name.endswith(".md") or name.startswith("_") or name == "README.md" or not os.path.isfile(p):
            continue
        text = open(p, encoding="utf-8").read()
        m = re.match(r"---\n(.*?)\n---\n(.*)", text, re.S)
        if not m:
            fehler.append(f"Zettel {name}: Kopf (--- ... ---) fehlt")
            continue
        kopf = {}
        for zeile in m.group(1).splitlines():
            if ":" in zeile:
                k, v = zeile.split(":", 1)
                kopf[k.strip()] = v.split("#")[0].strip()
        kopf["_name"], kopf["_text"] = name, m.group(2)
        alle.append(kopf)
    return alle


def pruefe_zettel(zettel, streng):
    melde = fehler.append if streng else hinweise.append
    for z in zettel:
        n = z["_name"]
        for k in PFLICHT:
            if not z.get(k):
                fehler.append(f"Zettel {n}: Feld '{k}' fehlt")
        if z.get("typ") and z["typ"] not in TYPEN:
            fehler.append(f"Zettel {n}: typ '{z['typ']}' unbekannt ({', '.join(sorted(TYPEN))})")
        for m in [x.strip() for x in z.get("modi", "").split(",") if x.strip()]:
            if m not in MODI:
                fehler.append(f"Zettel {n}: Modus '{m}' unbekannt")
        if z.get("doku") == "nein" and z.get("sichtbar") == "ja":
            melde(f"Zettel {n}: Doku noch nicht nachgezogen")
        if z.get("simulation") == "nein":
            melde(f"Zettel {n}: Simulation noch nicht nachgezogen")
        for plan in [x.strip() for x in z.get("tests", "").split(",") if x.strip()]:
            if not os.path.exists(os.path.join(PLAENE, plan + ".json")):
                fehler.append(f"Zettel {n}: Testplan '{plan}' fehlt in Tests/Plaene")
        if z.get("sichtbar") == "ja" and "**Changelog EN:**" not in z["_text"]:
            melde(f"Zettel {n}: Changelog-Zeile EN fehlt")


def pruefe_code_ohne_zettel(zettel):
    tag = git("describe", "--tags", "--abbrev=0")
    if not tag:
        hinweise.append("kein Tag gefunden - Vergleich Code/Zettel uebersprungen")
        return
    geaendert = [f for f in git("diff", "--name-only", tag, "--", "Source", "Content", "Config").splitlines() if f]
    geaendert += [f for f in git("status", "--porcelain", "--", "Source", "Content", "Config").splitlines() if f]
    if geaendert and not zettel:
        fehler.append(f"Code seit {tag} geaendert ({len(geaendert)} Dateien), aber kein Aenderungszettel in Aenderungen/")


def quell_hashes(namen):
    # sim_stand.json fuehrt Pfade relativ zur Repo-Wurzel (z. B. Source/FicsitTD/Private/TDBalance.cpp)
    h = {}
    for name in namen:
        p = os.path.join(MOD, *name.replace("\\", "/").split("/"))
        if os.path.exists(p):
            h[name] = hashlib.sha256(open(p, "rb").read()).hexdigest()
    return h


def pruefe_simulation(streng):
    melde = fehler.append if streng else hinweise.append
    if not os.path.exists(SIM_STAND):
        melde("Simulation: Tools/Simulator/sim_stand.json fehlt - Simulator nie mit sim_alle_modi.py gelaufen")
        return
    stand = json.load(open(SIM_STAND, encoding="utf-8"))
    alt = stand.get("code_stand", stand)
    jetzt = quell_hashes(list(alt.keys()))
    anders = [n for n, w in alt.items() if jetzt.get(n) != w]
    if anders:
        melde(f"Simulation nicht nachgezogen: seit dem letzten Lauf geaendert {', '.join(anders)} (python Tools/Simulator/sim_alle_modi.py)")


def pruefe_experimente():
    balance = open(os.path.join(QUELLE, "Private", "TDBalance.cpp"), encoding="utf-8").read()
    schalter = {m.group(1): float(m.group(2)) for m in re.finditer(r'TEXT\("Experiment\.([A-Za-z0-9_]+)"\),\s*([0-9.]+)f', balance)}
    liste = open(os.path.join(MOD, "Experimente.md"), encoding="utf-8").read()
    for name, wert in schalter.items():
        zeile = next((l for l in liste.splitlines() if l.startswith(f"| {name} ")), None)
        if not zeile:
            fehler.append(f"Experiment.{name}: Schalter im Code, aber nicht in Experimente.md")
        elif wert != 0.0 and "uebernommen" not in zeile:
            fehler.append(f"Experiment.{name}: Standard {wert:g}, aber nicht als 'uebernommen' eingetragen")


def pruefe_versionen():
    up = json.load(open(os.path.join(MOD, "FicsitTD.uplugin"), encoding="utf-8"))
    v = up.get("SemVersion")
    if up.get("VersionName") != v:
        fehler.append(f"uplugin: VersionName {up.get('VersionName')} != SemVersion {v}")
    html = open(os.path.join(MOD, "docs", "index.html"), encoding="utf-8").read()
    m = re.search(r'class="ver"[^>]*>v([0-9][^ <]*)', html)
    if not m or m.group(1) != v:
        fehler.append(f"Handbuch-Abzeichen {m.group(1) if m else '?'} != uplugin {v}")
    log = open(os.path.join(MOD, "Doku", "Release", "Release-Log.md"), encoding="utf-8").read()
    if f"**{v}**" not in log:
        fehler.append(f"Release-Log: kein Eintrag fuer {v}")
    seite = open(os.path.join(MOD, "Doku", "Release", "ModSeite-ficsit-app.md"), encoding="utf-8").read()
    if v not in seite:
        fehler.append(f"Mod-Seite: Version {v} nicht erwaehnt")
    return v


def pruefe_handbuch():
    html = open(os.path.join(MOD, "docs", "index.html"), encoding="utf-8").read()
    en, de = len(re.findall(r'class="en"', html)), len(re.findall(r'class="de"', html))
    if en != de:
        fehler.append(f"Handbuch: {en} englische, aber {de} deutsche Abschnitte")


def changelog(zettel):
    print("## Changelog-Entwurf (EN, ficsit.app)\n")
    for typ, titel in [("neu", "New"), ("geaendert", "Changed"), ("balance", "Balance"), ("behoben", "Fixed")]:
        zeilen = [re.search(r"\*\*Changelog EN:\*\*\s*(.+)", z["_text"]) for z in zettel if z.get("typ") == typ]
        zeilen = [m.group(1).strip() for m in zeilen if m]
        if zeilen:
            print(f"**{titel}**")
            for l in zeilen:
                print(f"- {l}")
            print()
    print("## Changelog-Entwurf (DE, README)\n")
    for z in zettel:
        m = re.search(r"\*\*Changelog DE:\*\*\s*(.+)", z["_text"])
        if m and z.get("sichtbar") == "ja":
            print(f"- {m.group(1).strip()}")


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser()
    ap.add_argument("--stufe", default="schnell", choices=["schnell", "voll", "release"])
    ap.add_argument("--changelog", action="store_true")
    a = ap.parse_args()
    zettel = lies_zettel()
    if a.changelog:
        changelog(zettel)
        return 0
    streng = a.stufe != "schnell"
    pruefe_zettel(zettel, streng)
    pruefe_code_ohne_zettel(zettel)
    pruefe_simulation(streng)
    pruefe_experimente()
    pruefe_handbuch()
    if a.stufe == "release":
        pruefe_versionen()
    print(f"Konsistenz ({a.stufe}): {len(zettel)} Aenderungszettel, {len(fehler)} Fehler, {len(hinweise)} Hinweise")
    for f in fehler:
        print("FEHLER:", f)
    for h in hinweise:
        print("HINWEIS:", h)
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
