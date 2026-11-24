"""Verlustkurve an Spielmessungen eichen - das eine Eichwerkzeug (24.11.2026, ersetzt verlust_fit_013/kalibriere/p12_fit).

Modell (strategien.Lauf.kampf):
  Verlust % = VerlustQ * qv^VerlustP * (1 + VerlustWenigeTuerme / Tuerme^VerlustTurmExponent) * (VerlustBezugKernLP / KernLP) [/ KernPanzer]
  qv = Q_wirksam / max(1, Gegner / MaxGleichzeitig)^VerlustStreckung
  Q_wirksam = Q_nenn / (1 - Overkill)      seit 24.11.: Schuesse sind ganzzahlig (VerlustOverkill 1)
Im Spiel liefert Metriken.csv Q_nenn und den gemessenen Overkill je Welle. Daten: messungen.py (gueltige, nicht abgeschnittene
Wellen fester Plaene). Fehlermass: kleinste Quadrate auf dem Verlust (Mittelwert treffen, der Kernverlauf summiert).

  python eiche_verlust.py [--ab "2026-11-24 06:00"] [--schreiben]
Danach Pflicht: python abgleich.py
"""
import argparse
import csv
import json
import math
import os
import shutil

import messungen as me

HIER = os.path.dirname(os.path.abspath(__file__))
ANNAHMEN = os.path.join(HIER, "strategie_annahmen.json")
KERN_LP = {0: 30000, 1: 15000, 2: 15000, 3: 15000}   # Stufen.*.KernLP (0.13)


def daten(ab, Z, max_gleichzeitig=60):
    overkill = {}
    for r in csv.DictReader(open(me.METRIKEN, encoding="utf-8"), delimiter=";"):
        overkill[(r["Zeit"], r.get("Plan", ""))] = me._zahl(r.get("Overkill_Proz"))
    zeilen = []
    for l in me.laeufe(me.lade_wellen(ab)):
        pa = me.plan_ausbau(l["plan"]) or {"forschung": {}}
        panzer = min([a + 1 for a, k in pa["forschung"].items() if "KernPanzer" in k] or [999])
        for w in l["wellen"]:
            if not w["gueltig"] or w["abgeschnitten"] or w["stufe"] not in KERN_LP:
                continue
            ok = overkill.get((w["zeit"], w["plan"]), 0.0)
            q = w["q"] / max(0.05, 1 - ok / 100)
            qv = q / max(1.0, w["gegner"] / max_gleichzeitig) ** Z["VerlustStreckung"]
            skala = Z["VerlustBezugKernLP"] / KERN_LP[w["stufe"]] / (1.3 if w["welle"] >= panzer else 1.0)
            zeilen.append(dict(plan=w["plan"], welle=w["welle"], n=w["tuerme"], qv=qv, skala=skala, verl=w["verlust"]))
    return zeilen


def vorhersage(z, vq, p, a, c, qk=99.0, u=0.0):
    # wie strategien.Lauf.kampf: Grundkurve plus Knick (VerlustUeber * (qv - QKritisch)^2 * 100), mal Turmfaktor
    return (vq * z["qv"] ** p + u * max(0.0, z["qv"] - qk) ** 2 * 100) * (1 + a / z["n"] ** c) * z["skala"]


def fehler(zl, *k):
    # Kleinste Quadrate auf dem Verlust selbst: der Kernverlauf summiert die Verluste, es zaehlt der Mittelwert
    # (die Wurzel-Variante bis verlust_fit_013 unterschaetzte den Mittelwert bei 4-6 Tuermen um ein Drittel)
    return sum((max(z["verl"], 0) - vorhersage(z, *k)) ** 2 for z in zl)


def tabelle(zl, k, titel):
    print(f"\n{titel}: VerlustQ {k[0]:.2f}, VerlustP {k[1]:.2f}, VerlustWenigeTuerme {k[2]:.2f}, Exponent {k[3]:.2f}, QKritisch {k[4]:.2f}, VerlustUeber {k[5]:.2f}, Fehler {fehler(zl, *k):.1f}")
    for name, f in (("1", lambda n: n == 1), ("2-3", lambda n: 2 <= n <= 3), ("4-6", lambda n: 4 <= n <= 6), ("7-12", lambda n: 7 <= n <= 12), ("13+", lambda n: n >= 13)):
        w = [z for z in zl if f(z["n"])]
        if w:
            print(f"  {name:>4} Tuerme: {len(w):3} Wellen, Spiel {sum(z['verl'] for z in w) / len(w):5.1f} %, Modell {sum(vorhersage(z, *k) for z in w) / len(w):5.1f} %")
    for lo, hi in ((0, .15), (.15, .25), (.25, .4), (.4, .6), (.6, 9)):
        w = [z for z in zl if lo <= z["qv"] < hi]
        if w:
            print(f"  qv {lo}-{hi}: {len(w):3} Wellen, Spiel {sum(z['verl'] for z in w) / len(w):5.1f} %, Modell {sum(vorhersage(z, *k) for z in w) / len(w):5.1f} %")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ab", default="2026-11-24 06:00")
    ap.add_argument("--schreiben", action="store_true")
    a = ap.parse_args()
    roh = json.load(open(ANNAHMEN, encoding="utf-8"))
    Z = {k: (v["Wert"] if isinstance(v, dict) and "Wert" in v else v) for k, v in roh.items()}
    zl = daten(a.ab, Z)
    print(f"{len(zl)} gueltige Wellen aus {len(set(z['plan'] for z in zl))} Plaenen ab {a.ab}")
    alt = (Z["VerlustQ"], Z["VerlustP"], Z.get("VerlustWenigeTuerme", 0.0), Z.get("VerlustTurmExponent", 1.0), Z.get("QKritisch", 99.0), Z.get("VerlustUeber", 0.0))
    tabelle(zl, alt, "Bisher")
    best = None
    # Knick: unter QKritisch kleine Grundkurve, darueber quadratischer Anstieg (Abwehr raeumt die Welle nicht mehr vor dem Kern)
    for qk in (0.15, 0.18, 0.2, 0.22, 0.25, 0.28, 0.3, 0.35, 99.0):
        for u in ((0.0,) if qk >= 99 else (0.5, 1, 2, 3, 5, 8, 12)):
            for p in (0.8, 1.0, 1.37, 2.0):
                for c in (0.5, 1.0):
                    for aa in (0, 1, 2, 4, 6, 9):
                        for i in range(0, 121):
                            vq = i * 0.5
                            f = fehler(zl, vq, p, aa, c, qk, u)
                            if best is None or f < best[0]:
                                best = (f, vq, p, aa, c, qk, u)
    k = best[1:]
    tabelle(zl, k, "Neu (mit Overkill)")
    if a.schreiben:
        shutil.copy(ANNAHMEN, os.path.join(HIER, "Archiv", "strategie_annahmen_vor_eiche_verlust.json"))
        quelle = f"eiche_verlust.py {len(zl)} Wellen fester Plaene ab {a.ab} (messungen.py), Q um den gemessenen Overkill bereinigt"
        roh["VerlustQ"] = {"Wert": round(k[0], 2), "Quelle": quelle}
        roh["VerlustP"] = {"Wert": k[1], "Quelle": quelle}
        roh["VerlustWenigeTuerme"] = {"Wert": k[2], "Quelle": quelle}
        roh["VerlustTurmExponent"] = {"Wert": k[3], "Quelle": quelle}
        roh["QKritisch"] = {"Wert": k[4], "Quelle": quelle + "; Knick der Verlustkurve"}
        roh["VerlustUeber"] = {"Wert": k[5], "Quelle": quelle + "; Zusatzverlust (qv-QKritisch)^2*100*Faktor"}
        roh["VerlustOverkill"] = {"Wert": 1, "Quelle": "24.11.2026: Schuesse ganzzahlig (strategien.Lauf.kampf), Ursache des Einbruchs in W5 (Hog 53 LP gegen Armierungsbolzen 48)"}
        json.dump(roh, open(ANNAHMEN, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
        print("geschrieben:", ANNAHMEN, "- jetzt python abgleich.py")


if __name__ == "__main__":
    main()
