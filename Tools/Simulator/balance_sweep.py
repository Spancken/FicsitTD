"""FicsitTD Balance-Sweep: Balance-Varianten gegen das Zielbild des Strategie-Prüfstands rechnen.

  python balance_sweep.py            alle Varianten aus VARIANTEN, je 2 Startwerte -> Ausgabe/Balance_Sweep.html
  python balance_sweep.py --laeufe 3

Jede Variante überschreibt Balance-Schlüssel (wie Balance.json) und/oder Sweep-Regler aus strategie_annahmen.json
(EPFaktor, FPFaktor, PreisFaktor, FPPreisFaktor). Ausgabe: je Variante Erfüllung des Zielbilds und Erfolg je Turmlinie.
Ergebnisse sind Vorschläge für den Prüfstand im Spiel, keine fertige Balance (Kampfmodell ist eine Annahme).
"""
import argparse
import os
import random

import strategien as st

HIER = os.path.dirname(os.path.abspath(__file__))

# (Name, Balance-Überschreibungen, Regler-Überschreibungen)
VARIANTEN = [
    ("Stand", {}, {}),
    ("Tesla −25 % Schaden", {"Munition.Tesla.Schaden": 225}, {}),
    ("Tesla −25 %, Laden 100 MW", {"Munition.Tesla.Schaden": 225}, {"TeslaLaden": 100}),
    ("Flamme +50 % Schaden", {"Munition.Treibstoff.Biotreibstoff.SchadenProSekunde": 52, "Munition.Treibstoff.Treibstoff.SchadenProSekunde": 120,
                               "Munition.Treibstoff.Turbotreibstoff.SchadenProSekunde": 120}, {}),
    ("Tesla −25 % + Flamme +50 %", {"Munition.Tesla.Schaden": 225, "Munition.Treibstoff.Biotreibstoff.SchadenProSekunde": 52,
                                     "Munition.Treibstoff.Treibstoff.SchadenProSekunde": 120, "Munition.Treibstoff.Turbotreibstoff.SchadenProSekunde": 120}, {}),
    ("EP −15 %", {}, {"EPFaktor": 0.85}),
    ("EP +15 %", {}, {"EPFaktor": 1.15}),
    ("FP +25 %", {}, {"FPFaktor": 1.25}),
    ("Ladenpreise +20 %", {}, {"PreisFaktor": 1.2}),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stufe", default="Normal")
    ap.add_argument("--laeufe", type=int, default=2)
    a = ap.parse_args()
    Z0 = st.lade_alles()[2]
    zeilen = []
    for name, bal, reg in VARIANTEN:
        erg, quelle = st.massenlauf(a.stufe, 50, a.laeufe, 7, balance_ueber=bal, z_ueber=reg)
        zb = st.zielbild(erg, {**Z0, **reg})
        erfuellt = sum(zb[k] for k in ("Ziel_Wege", "Ziel_NichtAlles", "Ziel_NichtZuLeicht", "Ziel_KeineDominanz", "Ziel_KeineToteLinie"))
        zeilen.append((name, zb, erfuellt))
        print(f"{name:32s} Ziele {erfuellt}/5  Erfolg {zb['Quote'] * 100:3.0f} %  " + "  ".join(f"{l} {q * 100:.0f}" for l, q in sorted(zb["JeLinie"].items())))
    linien = sorted(zeilen[0][1]["JeLinie"])
    html = ["<!doctype html><meta charset=utf-8><title>Balance-Sweep</title><style>body{font:14px/1.5 system-ui,sans-serif;max-width:1200px;margin:24px auto;padding:0 16px}"
            "table{border-collapse:collapse;font-size:13px}th,td{padding:4px 8px;border-bottom:1px solid #ddd;text-align:right;white-space:nowrap}th:first-child,td:first-child{text-align:left}"
            ".g{background:#dcefdc}.w{background:#f6ebc9}.s{background:#f6d9d3}</style>",
            f"<h1>Balance-Sweep {a.stufe}</h1><p>Erfolg je Turmlinie in %, Zielbild-Kriterien erfüllt von 5. {a.laeufe} Startwerte je Strategie.</p><div style='overflow-x:auto'><table>",
            "<tr><th>Variante</th><th>Ziele</th><th>gesamt</th>" + "".join(f"<th>{l}</th>" for l in linien) + "</tr>"]
    for name, zb, erf in zeilen:
        zellen = "".join(f"<td class='{'s' if q < 0.2 or q > 0.9 else 'g' if q >= 0.6 else 'w'}'>{q * 100:.0f}</td>" for q in (zb["JeLinie"][l] for l in linien))
        html.append(f"<tr><td>{name}</td><td>{erf}/5</td><td>{zb['Quote'] * 100:.0f}</td>{zellen}</tr>")
    html.append("</table></div>")
    os.makedirs(os.path.join(HIER, "Ausgabe"), exist_ok=True)
    open(os.path.join(HIER, "Ausgabe", "Balance_Sweep.html"), "w", encoding="utf-8").write("\n".join(html))


if __name__ == "__main__":
    main()
