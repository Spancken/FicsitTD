"""P2 (Fahrplan 0.11): Endlos-Balance - Varianten von Abstand, Budget und spaetem LP-Wachstum durchrechnen.
  python varianten_endlos.py [--stufen Normal Erkundung] [--max 100]
Ausgabe: je Variante Welle (Median), Kipp-Welle, Anteil an der Rechengrenze. Ziel: sim_baender.json (Endlos).
"""
import argparse
import itertools
import json
import os
import statistics
import sys

import sim_modi as sm
import strategien as st

HIER = os.path.dirname(os.path.abspath(__file__))


def kennzahlen(daten, stufe, max_wellen):
    erg = sm.endlos_lauf(daten, stufe, max_wellen=max_wellen, laeufe=1)
    ohne = [r for r in erg if not r["Vorziehen"]]
    lebend = {n: sum(1 for r in ohne if r["Welle"] >= n) / len(ohne) for n in range(1, max_wellen + 1)}
    kipp = next((n for n in range(1, max_wellen + 1) if lebend[n] < 0.5), None)
    return (statistics.median(r["Welle"] for r in ohne), kipp, sum(r["Kappung"] for r in erg) / len(erg),
            statistics.median(r["Welle"] for r in erg if r["Vorziehen"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stufen", nargs="+", default=["Normal", "Erkundung"])
    ap.add_argument("--max", type=int, default=100)
    ap.add_argument("--budget", nargs="+", type=float, default=[1.0, 0.85, 0.7])
    ap.add_argument("--abstand", nargs="+", type=float, default=[0, 30, 60])
    ap.add_argument("--wachstum", nargs="+", type=float, default=[0.0, 0.03, 0.05])
    ap.add_argument("--erste", nargs="+", type=float, default=[0], help="Zuschlag auf Endlos.<Stufe>.ErsteWelle in s")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    B, A, Z, katalog, phasen, K, gabeln, _ = st.lade_alles(stand="091")
    baender = json.load(open(os.path.join(HIER, "sim_baender.json"), encoding="utf-8"))["Endlos"]
    grund = dict(B)
    for stufe in a.stufen:
        band = baender["Welle_Median"][stufe]
        a0 = grund.get(f"Endlos.{stufe}.Abstand", sm.ENDLOS_WERTE[stufe][1])
        e0 = grund.get(f"Endlos.{stufe}.ErsteWelle", sm.ENDLOS_WERTE[stufe][0])
        print(f"\n== {stufe}: Band W{band[0]}-W{band[1]}, Kappung <= {baender['Kappung_Anteil_max']['Wert']:.0%}")
        print("Budget | Erste | Abstand | Wachstum | Welle Median | mit Vorziehen | kippt ab | Kappung")
        for bf, de, da, wa in itertools.product(a.budget, a.erste, a.abstand, a.wachstum):
            B.clear()
            B.update(grund)
            B["Endlos.BudgetFaktor"], B[f"Endlos.{stufe}.Abstand"], B["Endlos.LPWachstum"] = bf, a0 + da, wa
            B[f"Endlos.{stufe}.ErsteWelle"] = e0 + de
            w, k, kapp, wv = kennzahlen((B, A, Z, katalog, phasen, K, gabeln), stufe, a.max)
            ok = band[0] <= w <= band[1] and kapp <= baender["Kappung_Anteil_max"]["Wert"]
            print(f"{bf:.2f} | {e0 + de:.0f} s | {a0 + da:.0f} s | {wa:.0%} | W{w} | W{wv} | {k} | {kapp:.0%}{'  <- im Band' if ok else ''}", flush=True)


if __name__ == "__main__":
    main()
