"""0.83.1: Optionen fuer die schwache Bolzen-Linie (Modell bis W30: 17 %, alle anderen Linien 98-100 %).
Nur Vorschlaege fuer Marius, keine Balance-Aenderung im Mod. Rechnet je Variante den Strategie-Pruefstand (alle Linien),
damit sichtbar ist, ob eine Bolzen-Staerkung andere Linien oder das Zielbild kippt.

  python bolzen_optionen.py [--laeufe 2]
"""
import argparse

import strategien as st

VARIANTEN = [
    ("Stand", {}),
    ("Armierung +20 % (40 -> 48)", {"Munition.Bolzen.Armierungsbolzen.Schaden": 48}),
    ("Armierung +35 % (40 -> 54)", {"Munition.Bolzen.Armierungsbolzen.Schaden": 54}),
    ("Reichweite 45 -> 55 m", {"Tuerme.Bolzenturm.Reichweite": 55}),
    ("Turm billiger (Grundpreis 4 -> 3)", {"Einsatz.Laden.Bolzenturm.Grundpreis": 3}),
    ("Level 2 +35 % statt +20 %", {"Tuerme.Level.Bolzenturm.L2.Schaden": 0.35}),
    ("Armierung +20 % und Reichweite 50 m", {"Munition.Bolzen.Armierungsbolzen.Schaden": 48, "Tuerme.Bolzenturm.Reichweite": 50}),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stufe", default="Normal")
    ap.add_argument("--laeufe", type=int, default=2)
    a = ap.parse_args()
    Z0 = st.lade_alles()[2]
    for name, bal in VARIANTEN:
        erg, _ = st.massenlauf(a.stufe, 50, a.laeufe, 7, balance_ueber=bal, z_ueber={})
        zb = st.zielbild(erg, Z0)
        erfuellt = sum(zb[k] for k in ("Ziel_Wege", "Ziel_NichtAlles", "Ziel_NichtZuLeicht", "Ziel_KeineDominanz", "Ziel_KeineToteLinie"))
        print(f"{name:38s} Ziele {erfuellt}/5  Erfolg {zb['Quote'] * 100:3.0f} %  " + "  ".join(f"{l} {q * 100:.0f}" for l, q in sorted(zb["JeLinie"].items())), flush=True)


if __name__ == "__main__":
    main()
