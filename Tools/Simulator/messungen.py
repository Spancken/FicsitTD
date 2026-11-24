"""Spielmessungen für Eichung und Abgleich - eine Quelle für alle Simulator-Werkzeuge (24.11.2026).

Vorher filterte jedes Eichwerkzeug (verlust_fit, verlust_fit_013, kalibriere, p12_fit, balance_kreislauf) selbst.
Hier stehen die Regeln einmal:
  - Welle gültig: Q > 0,01, mindestens ein Turm, Fabrik-Takt >= 0,95 (sonst Zeitraffer zu hoch), Munitionsmangel <= 1 s.
  - abgeschnitten: der Kern fiel in der Welle (Kern_Proz <= 0,5). Für die Fallwelle zählt sie, für die Verlustkurve nicht,
    weil der Verlust dann nur der Rest des Kerns ist.
  - Lauf: aufeinanderfolgende Zeilen desselben Plans mit steigender Welle.
  - Kernschaden je Gegnerart: Logzeilen "Kernschaden Welle N: ..." (seit 24.11.2026, TDWaveSubsystem.cpp).

  python messungen.py [--ab "2026-11-24 06:00"]    Übersicht der Läufe
"""
import argparse
import csv
import glob
import json
import os
import re

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HIER))
SAVED = os.path.join(os.environ.get("LOCALAPPDATA", ""), "FactoryGame", "Saved")
METRIKEN = os.path.join(SAVED, "FicsitTD", "Metriken.csv")
LOGS = os.path.join(SAVED, "Logs")
PLAENE = os.path.join(REPO, "Tests", "Plaene")
STUFEN = {0: "Erkundung", 1: "Normal", 2: "Hart", 3: "Albtraum"}
# Pläne mit festem Ausbau, die das Modell nachspielen kann
FESTE_PLAENE = ("Strat-", "P12-K")


def _zahl(x, std=0.0):
    try:
        return float(x)
    except (TypeError, ValueError):
        return std


def lade_wellen(ab="", plaene=FESTE_PLAENE, pfad=METRIKEN):
    """Alle Wellen ab Zeitpunkt `ab` (lokale Zeit wie in Metriken.csv) der Pläne mit diesen Anfängen"""
    aus = []
    if not os.path.exists(pfad):
        return aus
    for r in csv.DictReader(open(pfad, encoding="utf-8"), delimiter=";"):
        if r.get("Zeit", "") < ab or not r.get("Plan", "").startswith(plaene):
            continue
        try:
            w = dict(zeit=r["Zeit"], plan=r["Plan"], stufe=int(r["Stufe"]), welle=int(r["Welle"]), gegner=int(r["Gegner"]),
                     elite=int(r["Elite"] or 0), lp=_zahl(r["LP"]), ehp=_zahl(r["EHP"]), k=_zahl(r["K_DPS"]), tuerme=int(r["Tuerme"] or 0),
                     q=_zahl(r["Q"]), dauer=_zahl(r["Dauer_s"]), verlust=_zahl(r["Kernverlust_Proz"]), kern=_zahl(r["Kern_Proz"]),
                     uptime=_zahl(r["Uptime_Proz"]), hungrig=_zahl(r["Hungrig_s"]), takt=_zahl(r.get("FabrikTakt_Anteil"), 1.0))
        except (KeyError, ValueError):
            continue
        w["abgeschnitten"] = w["kern"] <= 0.5
        grund = ("kein Druck" if w["q"] <= 0.01 else "keine Türme" if w["tuerme"] <= 0 else
                 "Fabrik-Takt" if w["takt"] < 0.95 else "Munitionsmangel" if w["hungrig"] > 1 else "")
        w["gueltig"], w["grund"] = not grund, grund
        aus.append(w)
    return aus


def laeufe(wellen):
    """Wellen in Läufe gruppieren: neuer Lauf bei anderem Plan oder nicht steigender Welle"""
    aus = []
    for w in wellen:
        if not aus or aus[-1]["plan"] != w["plan"] or w["welle"] <= aus[-1]["wellen"][-1]["welle"]:
            aus.append({"plan": w["plan"], "stufe": w["stufe"], "start": w["zeit"], "wellen": []})
        aus[-1]["wellen"].append(w)
    for l in aus:
        letzte = l["wellen"][-1]
        l["gefallen"] = letzte["abgeschnitten"]
        l["fallwelle"] = letzte["welle"] if letzte["abgeschnitten"] else None
    return aus


_KS = re.compile(r"^\[(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d)[^\]]*\].*Kernschaden Welle (\d+): (.*)$")
_TEIL = re.compile(r"(\S+) (\d+) LP von (\d+) Gegnern \((\d+) Treffer\)")


def kernschaden(logs=None):
    """Logzeilen 'Kernschaden Welle N' -> Liste (Zeit UTC 'JJJJ-MM-TT hh:mm:ss', Welle, {Art: (LP, Gegner, Treffer)})"""
    aus = []
    for p in sorted(logs or glob.glob(os.path.join(LOGS, "FactoryGame*.log")), key=os.path.getmtime):
        for zeile in open(p, encoding="utf-8", errors="replace"):
            m = _KS.match(zeile.strip())
            if m:
                j, mo, t, h, mi, s, welle, rest = m.groups()
                arten = {a: (float(lp), int(g), int(tr)) for a, lp, g, tr in _TEIL.findall(rest)}
                aus.append((f"{j}-{mo}-{t} {h}:{mi}:{s}", int(welle), arten))
    return aus


def plan_ausbau(name):
    """Ausbau und Forschung eines Testplans wie in den Plänen geschrieben: {Abschnitt: [(Typ, Anzahl, Level, Rezept)]}.
    Abschnitt = letzte gespielte Welle vor dem Schritt (0 = vor Welle 1). Anzahl 0 = vorhandene Türme aufwerten.
    Dazu Balance-Overrides des Plans (Schritt "balance") und die höchste Welle."""
    p = os.path.join(PLAENE, name + ".json")
    if not os.path.exists(p):
        return None
    plan = json.load(open(p, encoding="utf-8-sig"))
    ausbau, forschung, balance, ab, bis = {}, {}, {}, 0, 0
    fabrik_typ = {"Bolzen": ("Bolzenturm", "Recipe_TD_Shoot_SpikedRebar"), "Gewehr": ("Gewehrturm", "Recipe_TD_Gun_Standard"),
                  "Nobelisk": ("Nobelisk-Werfer", "Recipe_TD_Nobelisk_Explosive"), "Tesla": ("Teslaspule", "Recipe_TD_Tesla_Discharge"),
                  "Flamme": ("Flammenwerfer", "Recipe_TD_Flame_Fuel")}
    for s in plan.get("Schritte", []):
        if not isinstance(s, dict):
            continue
        if "wellenlauf" in s:
            ab = bis = int(s["wellenlauf"])
        elif "baue" in s:
            ausbau.setdefault(ab, []).append((s["baue"], int(s.get("anzahl", 1)), int(s.get("level", 1)), s.get("rezept", "")))
        elif "aufwerten" in s:
            ausbau.setdefault(ab, []).append((s["aufwerten"], 0, int(s.get("level", 1)), s.get("rezept", "")))
        elif "fabrik" in s and s["fabrik"] in fabrik_typ:
            typ, rez = fabrik_typ[s["fabrik"]]
            ausbau.setdefault(ab, []).append((typ, int(s.get("tuerme", 1)), int(s.get("level", 1)), rez))
        elif "erforsche" in s:
            forschung.setdefault(ab, []).append(s["erforsche"])
        elif "balance" in s and ab == 0:
            balance.update(s["balance"])
    return {"ausbau": ausbau, "forschung": forschung, "balance": balance, "bis": bis}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ab", default="")
    a = ap.parse_args()
    ls = laeufe(lade_wellen(a.ab))
    for l in ls:
        g = sum(w["gueltig"] for w in l["wellen"])
        print(f"{l['start']} {l['plan']:<28} {STUFEN.get(l['stufe'], '?'):<9} Wellen {len(l['wellen']):2} (gültig {g:2}), "
              + (f"gefallen in W{l['fallwelle']}" if l["gefallen"] else f"bis W{l['wellen'][-1]['welle']} überstanden"))
    print(f"{len(ls)} Läufe; Kernschaden-Zeilen im Log: {len(kernschaden())}")


if __name__ == "__main__":
    main()
