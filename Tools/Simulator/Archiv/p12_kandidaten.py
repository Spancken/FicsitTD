"""P12 Kandidaten A/B (17.11.2026) mit dem kalibrierten Modell (p12_fit.py), Einsatz Normal, 3 Startwerte.
  A: Stufen.Normal.EliteAbWelle 13 -> 16 (Buckel glaetten)
  B: wie A, dazu Wellen.LPJeWelleAb31 0,1 -> 0,6 / 0,8 / 1,0 (hinten anziehen)
Empfindlichkeit: der B-Wert +-10 %.
Ausgabe: Ausgabe/P12-Kandidaten.md
"""
import collections
import os
import statistics
import sys

import strategien as st

HIER = os.path.dirname(os.path.abspath(__file__))
SEEDS = [7, 11, 13]
KLASSE = {"Mono": lambda l: l.startswith("Mono"), "Duo": lambda l: "+" in l, "Breit": lambda l: l == "Breit"}
VARIANTEN = [("Stand 17.11.", {}),
             ("A Elite ab W16", {"Stufen.Normal.EliteAbWelle": 16}),
             ("B LP ab W31 0,6", {"Stufen.Normal.EliteAbWelle": 16, "Wellen.LPJeWelleAb31": 0.6}),
             ("B LP ab W31 0,8", {"Stufen.Normal.EliteAbWelle": 16, "Wellen.LPJeWelleAb31": 0.8}),
             ("B LP ab W31 1,0", {"Stufen.Normal.EliteAbWelle": 16, "Wellen.LPJeWelleAb31": 1.0}),
             ("B 0,8 −10 %", {"Stufen.Normal.EliteAbWelle": 16, "Wellen.LPJeWelleAb31": 0.72}),
             ("B 0,8 +10 %", {"Stufen.Normal.EliteAbWelle": 16, "Wellen.LPJeWelleAb31": 0.88})]


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    aus = ["# P12 Kandidaten A/B im kalibrierten Modell (Einsatz Normal, 50 Wellen)", "",
           f"Startwerte {SEEDS}; Modell nach `p12_fit.py`. Erfolg = Anteil der Strategien, die W50 bestehen. "
           "Ziel (P12-Bericht 4.5): Mono 20–60 % und keine Mono-Linie über 80 %, Duo/Breit 50–85 %, gesamt 40–70 %, Kern-Tiefpunkt der Sieger 30–60 %.", "",
           "| Variante | gesamt | Mono | Duo | Breit | beste Mono-Linie | Niederlagen Phase 1–5 | Kern-Tiefpunkt Sieger (Median) |",
           "|---|---|---|---|---|---|---|---|"]
    for name, ueber in VARIANTEN:
        erg = []
        for seed in SEEDS:
            e, _ = st.massenlauf("Normal", 50, 1, seed=seed, balance_ueber=ueber, stand="091")
            erg += e
        q = lambda rs: sum(r["Erfolg"] for r in rs) / max(1, len(rs))
        je_linie = collections.defaultdict(list)
        for r in erg:
            je_linie[r["Linie"]].append(r)
        kl = {k: q([r for r in erg if f(r["Linie"])]) for k, f in KLASSE.items()}
        mono = max(((l, q(rs)) for l, rs in je_linie.items() if l.startswith("Mono")), key=lambda x: x[1])
        phasen = [sum(1 for r in erg if not r["Erfolg"] and (r["Welle"] - 1) // 10 + 1 == ph) for ph in range(1, 6)]
        kmin = [r["KernMin"] for r in erg if r["Erfolg"]]
        zeile = (f"| {name} | {q(erg):.0%} | {kl['Mono']:.0%} | {kl['Duo']:.0%} | {kl['Breit']:.0%} | {mono[0]} {mono[1]:.0%} | "
                 f"{' / '.join(map(str, phasen))} | {statistics.median(kmin) if kmin else 0:.0f} % |")
        aus.append(zeile)
        print(zeile, flush=True)
    aus.append("")
    ziel = os.path.join(HIER, "Ausgabe", "P12-Kandidaten.md")
    open(ziel, "w", encoding="utf-8").write("\n".join(aus) + "\n")
    print("Ausgabe:", ziel)


if __name__ == "__main__":
    main()
