"""Gegencheck Einsatz (16.11.2026, Marius: Leiter Hart/Albtraum glaetten): Stufen-Varianten fuer den Einsatz durchrechnen.
  python varianten_einsatz.py [--stufen Hart Albtraum] [--budget 1.2 1.3 1.4] [--lp 1.1 1.2 1.3] [--kern 15000 20000]
Ausgabe: je Variante Erfolg W50, Erfolg W30, Scheiterwelle (Median). Ziel: sim_baender.json (Einsatz.Erfolg).
Die Bonuswahl des Phasenabschlusses (alle 10 Wellen) ist im Modell nicht enthalten - sie macht das Spiel etwas leichter.
"""
import argparse
import itertools
import json
import os
import statistics
import sys

import strategien as st

HIER = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stufen", nargs="+", default=["Hart", "Albtraum"])
    ap.add_argument("--budget", nargs="+", type=float, default=[1.2, 1.3, 1.4])
    ap.add_argument("--lp", nargs="+", type=float, default=[1.1, 1.2, 1.3])
    ap.add_argument("--kern", nargs="+", type=float, default=[0], help="Stufen.<Stufe>.KernLP, 0 = Balance-Wert")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    baender = json.load(open(os.path.join(HIER, "sim_baender.json"), encoding="utf-8"))["Einsatz"]["Erfolg"]
    for stufe in a.stufen:
        band = baender[stufe]
        print(f"\n== {stufe}: Band Erfolg W50 {band[0]:.0%}–{band[1]:.0%}")
        print("Budget | LP | Masse | KernLP | Erfolg W50 | Erfolg W30 | Scheitern (Median)")
        for bf, lp, kern in itertools.product(a.budget, a.lp, a.kern):
            ueber = {f"Stufen.{stufe}.Budget": bf, f"Stufen.{stufe}.GegnerLP": lp}
            if kern > 0:
                ueber[f"Stufen.{stufe}.KernLP"] = kern
            erg, _ = st.massenlauf(stufe, 50, 1, seed=7, balance_ueber=ueber, stand="091")
            erfolg = sum(r["Erfolg"] for r in erg) / len(erg)
            w30 = sum(1 for r in erg if r["Erfolg"] or r["Welle"] > 30) / len(erg)
            fail = [r["Welle"] for r in erg if not r["Erfolg"]]
            sch = statistics.median(fail) if fail else "-"
            ok = band[0] <= erfolg <= band[1]
            print(f"{bf:.2f} | {lp:.2f} | {bf * lp:.2f} | {kern or 'Balance':} | {erfolg:.0%} | {w30:.0%} | W{sch}{'  <- im Band' if ok else ''}", flush=True)


if __name__ == "__main__":
    main()
