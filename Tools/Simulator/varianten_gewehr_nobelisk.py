# 23.11.2026 (Marius, Entscheidung 2D): Gewehr+Nobelisk war bei der schwereren Balance fast tot. Erst die Varianten rechnen:
#   B  Nobelisk-Munition billiger (je Item ein Schuss mehr, alle Nobelisk-Sorten)
#   C  Kombi-Vorteil (Linien mit zwei Turmarten: Fabrikgebaeude 25 % billiger)
# Grundlage ist das korrigierte Spielermodell (Kohlekraftwerk bei knapper Biomasse vorgezogen, strategien.strom_knapp).
# Rechnet nur im Speicher. Aufruf im Simulator-Ordner: python varianten_gewehr_nobelisk.py [Normal Hart Albtraum]
import random, statistics, sys
import strategien as st

NOBELISK = ["Nobelisk_Explosive", "Nobelisk_Gas", "Nobelisk_Pulse", "Nobelisk_Cluster"]


def rechne(stufe, ueber=None, kombi_rabatt=None):
    liste = st.strategien(random.Random(7))[::2]       # wie sim_alle_modi
    alt = st.Lauf.kosten
    if kombi_rabatt:
        def kosten(self, fehl):
            c = alt(self, fehl)
            return st.sim.runde(c * (1 - kombi_rabatt)) if len(self.s["Gewichte"]) == 2 else c
        st.Lauf.kosten = kosten
    try:
        erg, _ = st.massenlauf(stufe, 50, 1, seed=7, balance_ueber=ueber or {}, strategie_liste=liste, stand="091")
    finally:
        st.Lauf.kosten = alt
    je = {}
    for r in erg:
        je.setdefault(r["Linie"], []).append(r["Erfolg"])
    q = {l: sum(v) / len(v) for l, v in je.items()}
    return sum(r["Erfolg"] for r in erg) / len(erg), q


def main():
    stufen = sys.argv[1:] or ["Normal", "Hart", "Albtraum"]
    B = st.lade_alles(stand="091")[0]
    for stufe in stufen:
        print(f"== {stufe}", flush=True)
        for name, ueber, rabatt in [
            ("heute (Modell korrigiert)", None, None),
            ("B Nobelisk je Item +1 Schuss", {f"Munition.SchussJeItem.{k}": B[f"Munition.SchussJeItem.{k}"] + 1 for k in NOBELISK}, None),
            ("C Kombi-Rabatt Fabrik 25 %", None, 0.25),
        ]:
            gesamt, q = rechne(stufe, ueber, rabatt)
            zeile = ", ".join(f"{l} {v:.0%}" for l, v in sorted(q.items(), key=lambda x: -x[1]))
            print(f"  {name:32s} gesamt {gesamt:.0%} | G+N {q['Gewehr+Nobelisk']:.0%}, Mono Nobelisk {q['Mono Nobelisk']:.0%} | {zeile}", flush=True)


if __name__ == "__main__":
    main()
