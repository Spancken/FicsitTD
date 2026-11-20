"""P3 (Fahrplan 0.11): Bohrung-Balance - Kern-LP und Reparatur zwischen den Bohrwellen durchrechnen.
  python varianten_bohrung.py [--stufen Mittel Schwer] [--lp 1.0 1.5 2.0] [--reparatur 0 0.1 0.2]
Ausgabe: Mindestzahl Bolzenturm L2 und bester Aufbau je Variante, Ziel sim_baender.json (Bohrung).
"""
import argparse
import itertools
import json
import os
import sys

import sim_modi as sm
import strategien as st

HIER = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stufen", nargs="+", default=["Mittel", "Schwer"])
    ap.add_argument("--lp", nargs="+", type=float, default=[1.0, 1.5, 2.0], help="Faktor auf Bohrung.<Stufe>.LP")
    ap.add_argument("--reparatur", nargs="+", type=float, default=[0.0, 0.1, 0.2])
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    B, A, Z, katalog, phasen, K, gabeln, _ = st.lade_alles(stand="091")
    bb = json.load(open(os.path.join(HIER, "sim_baender.json"), encoding="utf-8"))["Bohrung"]
    grund = dict(B)
    for stufe in a.stufen:
        band, mind = bb["Bolzen_L2_Mindestzahl"][stufe], bb["Beste_Mindestzahl_min"][stufe]
        lp0 = sm.bohr_werte(grund, stufe)[2]
        print(f"\n== {stufe}: Bolzen L2 Band {band[0]}-{band[1]}, bester Aufbau >= {mind} Tuerme")
        print("Kern-LP | Reparatur | Bolzen L2 | Gewehr L2 | Tesla L2 | bester Aufbau")
        for f, rep in itertools.product(a.lp, a.reparatur):
            B.clear()
            B.update(grund)
            B[f"Bohrung.{stufe}.LP"], B["Bohrung.Reparatur"] = lp0 * f, rep
            auf = sm.bohrung_lauf((B, A, Z, katalog, phasen, K, gabeln), stufe, laeufe=4)
            m = {(x["Typ"], x["Level"]): x["Mindestzahl"] for x in auf}
            beste = min((x for x in auf if x["Mindestzahl"]), key=lambda x: x["Mindestzahl"], default=None)
            b2 = m[("Bolzenturm", 2)]
            ok = b2 is not None and band[0] <= b2 <= band[1]
            print(f"{lp0 * f:.0f} | {rep:.0%} | {b2} | {m[('Gewehrturm', 2)]} | {m[('Teslaspule', 2)]} | "
                  f"{beste['Mindestzahl']} x {beste['Typ']} L{beste['Level']}{'  <- im Band' if ok else ''}", flush=True)


if __name__ == "__main__":
    main()
