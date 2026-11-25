"""Verlustkurve gegen Spielmessungen auf dem Balancestand 0.13 (24.11.2026, G3-Nachgang).

Befund: Q des Modells stimmt mit dem Spiel ueberein (gleiche LP, gleiche Nenn-DPS), der Kernverlust je Q liegt im Spiel
2-4-mal hoeher, am staerksten bei wenigen Tuermen. Dieses Werkzeug passt die Kurve des Modells (strategien.Lauf.kampf)
  Verlust = VerlustQ * qv^VerlustP * TurmFaktor(n) * (VerlustBezugKernLP / KernLP) [/ KernPanzer]
  qv = Q_nenn / max(1, Gegner/MaxGleichzeitig)^VerlustStreckung,  TurmFaktor(n) = 1 + VerlustWenigeTuerme / n^VerlustTurmExponent
an alle gueltigen Wellen der angegebenen Plaene an (Metriken.csv ab --ab), Exponent und Streckung fest.
Verglichen werden: alte Werte, nur VerlustQ neu, VerlustQ + Turmfaktor neu.

  python verlust_fit_013.py [--ab "2026-11-24 06:00"] [--schreiben]
"""
import argparse
import csv
import json
import math
import os
import shutil
import statistics

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HIER))
METRIKEN = os.path.join(os.environ.get("LOCALAPPDATA", ""), "FactoryGame", "Saved", "FicsitTD", "Metriken.csv")
ANNAHMEN = os.path.join(HIER, "strategie_annahmen.json")
# Plaene mit festem Ausbau (keine "frei"-Sonderfaelle, keine geschwaechten Gegner)
PLAENE = ("Strat-", "P12-K")
KERN_LP = {0: 30000, 1: 15000, 2: 15000, 3: 15000}   # TDBalance.cpp Stufen.*.KernLP (0.13)


def panzer_ab(plan):
    """Welle, ab der Kern-Panzerung wirkt (erforsche KernPanzer vor wellenlauf X -> ab Welle der vorigen + 1)"""
    p = os.path.join(REPO, "Tests", "Plaene", plan + ".json")
    if not os.path.exists(p):
        return 999
    letzte = 0
    for s in json.load(open(p, encoding="utf-8")).get("Schritte", []):
        if "wellenlauf" in s:
            letzte = int(s["wellenlauf"])
        if s.get("erforsche") == "KernPanzer":
            return letzte + 1
    return 999


OHNE = []


def lade(ab, Z, B):
    zeilen = []
    panzer = {}
    for r in csv.DictReader(open(METRIKEN, encoding="utf-8"), delimiter=";"):
        if r["Zeit"] < ab or not r.get("Plan", "").startswith(PLAENE) or any(v <= r["Zeit"] <= b for v, b in OHNE):
            continue
        try:
            q, verl, geg, n, w = float(r["Q"]), float(r["Kernverlust_Proz"]), int(r["Gegner"]), int(r["Tuerme"] or 0), int(r["Welle"])
            stufe, hung = int(r["Stufe"]), float(r["Hungrig_s"] or 0)
            takt = float(r.get("FabrikTakt_Anteil") or 1)
        except (ValueError, KeyError):
            continue
        # 24.11. nachmittags: Wellen, in denen der Kern fiel, sind abgeschnitten (Verlust = Rest, nicht der volle Verlust)
        if q <= 0.01 or n <= 0 or hung > 1 or takt < 0.95 or stufe not in KERN_LP or float(r.get("Kern_Proz") or 0) <= 0.5:
            continue
        if r["Plan"] not in panzer:
            panzer[r["Plan"]] = panzer_ab(r["Plan"])
        qv = q / max(1.0, geg / B) ** Z["VerlustStreckung"]
        skala = Z["VerlustBezugKernLP"] / KERN_LP[stufe] / (1.3 if w >= panzer[r["Plan"]] else 1.0)
        zeilen.append(dict(plan=r["Plan"], welle=w, n=n, qv=qv, skala=skala, verl=verl))
    return zeilen


EXP = [1.0]
KNICK = [99.0, 0.0]   # QKritisch, VerlustUeber (Zusatz (qv-QK)^2*100*U, wie strategien.Lauf.kampf)


def vorhersage(z, vq, p, a):
    basis = vq * z["qv"] ** p + KNICK[1] * max(0.0, z["qv"] - KNICK[0]) ** 2 * 100
    return basis * (1 + a / z["n"] ** EXP[0]) * z["skala"]


def fehler(zeilen, vq, p, a):
    # Quadratischer Fehler auf der Wurzel des Verlusts: grosse Verluste dominieren nicht allein, kleine zaehlen mit
    return sum((math.sqrt(max(z["verl"], 0)) - math.sqrt(vorhersage(z, vq, p, a))) ** 2 for z in zeilen)


def fit_vq(zeilen, p, a):
    best = None
    for i in range(1, 801):
        vq = i * 0.1
        f = fehler(zeilen, vq, p, a)
        if best is None or f < best[1]:
            best = (vq, f)
    return best


def tabelle(zeilen, vq, p, a, titel):
    print(f"\n{titel}: VerlustQ {vq:.2f}, VerlustWenigeTuerme {a:.2f}, Fehler {fehler(zeilen, vq, p, a):.1f}")
    for name, f in (("1", lambda n: n == 1), ("2-3", lambda n: 2 <= n <= 3), ("4-6", lambda n: 4 <= n <= 6), ("7-12", lambda n: 7 <= n <= 12), ("13+", lambda n: n >= 13)):
        w = [z for z in zeilen if f(z["n"])]
        if w:
            sp = sum(z["verl"] for z in w) / len(w)
            mo = sum(vorhersage(z, vq, p, a) for z in w) / len(w)
            print(f"  {name:>4} Tuerme: {len(w):3} Wellen, Spiel {sp:5.1f} %, Modell {mo:5.1f} %")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ab", default="2026-11-24 06:00")
    ap.add_argument("--schreiben", action="store_true")
    ap.add_argument("--ohne", action="append", default=[], help="Zeitraum VON..BIS ausschliessen (z. B. Laeufe mit Sonderfront)")
    a = ap.parse_args()
    roh = json.load(open(ANNAHMEN, encoding="utf-8"))
    Z = {k: (v["Wert"] if isinstance(v, dict) and "Wert" in v else v) for k, v in roh.items()}
    B = 60
    OHNE.extend(tuple(x.split("..")) for x in a.ohne)
    zeilen = lade(a.ab, Z, B)
    print(f"{len(zeilen)} gueltige Wellen aus {len(set(z['plan'] for z in zeilen))} Plaenen ab {a.ab}")
    p = Z["VerlustP"]
    tabelle(zeilen, Z["VerlustQ"], p, Z.get("VerlustWenigeTuerme", 0.0), "Alt")
    vq1, _ = fit_vq(zeilen, p, 0.0)
    tabelle(zeilen, vq1, p, 0.0, "Nur VerlustQ neu")
    best = None
    for b in (0.5, 0.75, 1.0, 1.5, 2.0):
        EXP[0] = b
        for j in range(0, 41):
            aa = j * 0.5
            vq, f = fit_vq(zeilen, p, aa)
            if best is None or f < best[2]:
                best = (vq, aa, f, b)
    EXP[0] = best[3]
    tabelle(zeilen, best[0], p, best[1], "VerlustQ + Turmfaktor neu")
    # Knickpunkt: ab QKritisch waechst der Verlust quadratisch (Abwehr raeumt die Welle nicht mehr vor dem Kern)
    EXP[0] = 1.0
    best2 = None
    for qk in (0.12, 0.15, 0.18, 0.2, 0.22, 0.25, 0.3, 0.35, 0.4, 0.5, 99.0):
        for u in (0.0, 0.5, 1.0, 2.0, 3.0, 5.0, 8.0):
            KNICK[0], KNICK[1] = qk, u
            for j in range(0, 13):
                aa = j * 0.5
                vq, f = fit_vq(zeilen, p, aa)
                if best2 is None or f < best2[0]:
                    best2 = (f, vq, aa, qk, u)
    KNICK[0], KNICK[1] = best2[3], best2[4]
    tabelle(zeilen, best2[1], p, best2[2], f"Mit Knick (QKritisch {best2[3]}, VerlustUeber {best2[4]})")
    best = (best2[1], best2[2], best2[0], 1.0)
    if a.schreiben:
        shutil.copy(ANNAHMEN, ANNAHMEN + ".vor_verlust_fit_013")
        quelle = f"verlust_fit_013.py 24.11.2026: {len(zeilen)} Wellen (Plaene Strat-*, P12-K*) ab {a.ab}, Balance 0.13; Exponent {p} und Streckung fest"
        roh["VerlustQ"] = {"Wert": round(best[0], 2), "Quelle": quelle}
        roh["VerlustWenigeTuerme"] = {"Wert": round(best[1], 2), "Quelle": quelle + "; Turmfaktor 1 + a/n^b (wenige Tuerme decken die Ankunft schlechter ab)"}
        roh["VerlustTurmExponent"] = {"Wert": best[3], "Quelle": quelle}
        roh["QKritisch"] = {"Wert": KNICK[0], "Quelle": quelle + "; Knick der Verlustkurve (vorher 1,1 als Annahme)"}
        roh["VerlustUeber"] = {"Wert": KNICK[1], "Quelle": quelle + "; Zusatzverlust (qv-QKritisch)^2*100*Faktor"}
        json.dump(roh, open(ANNAHMEN, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
        print("geschrieben:", ANNAHMEN)


if __name__ == "__main__":
    main()
