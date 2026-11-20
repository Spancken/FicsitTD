"""0.84 Sweep 08.11.2026: Varianten gegen das Zielbild (alle Strategien bis W50, Normal). Ausgabe Ausgabe/sweep_0811.txt"""
import json, sys, collections, statistics
import strategien as st
LAEUFE = int(sys.argv[1]) if len(sys.argv) > 1 else 3
B = {"Wellen.BossBudget": 0.6}
T = {"Munition.Tesla.Schaden": 240}
N = {"Munition.Nobelisk.Nobelisk.Schaden": 125}
BO = {"Munition.Bolzen.Durchschlagbolzen.Schaden": 38, "Munition.Bolzen.Explosivbolzen.Flaechenschaden": 110}
R1 = {"Wellen.BossBudget": 0.6, "Munition.Tesla.Schaden": 240, "Munition.Nobelisk.Streu.Schaden": 180, "Munition.Bolzen.Explosivbolzen.Flaechenschaden": 120, "Munition.Bolzen.Streubolzen.Splitterschaden": 55, "Munition.Bolzen.Betaeubungsbolzen.Schaden": 70}
R7 = {**R1, "Munition.Nobelisk.Nobelisk.Schaden": 110, "Munition.Nobelisk.Gas.GasSchaden": 15, "Stufen.Normal.GegnerLP": 1.05}
NOB75 = {"Munition.Nobelisk.Nobelisk.Schaden": 112, "Munition.Nobelisk.Streu.Schaden": 135, "Munition.Nobelisk.Gas.GasSchaden": 15, "Munition.Nobelisk.Gas.Schaden": 15, "Munition.Nobelisk.Puls.Schaden": 68}
BO2 = {"Munition.Bolzen.Armierungsbolzen.Schaden": 48, "Munition.Bolzen.Durchschlagbolzen.Schaden": 42, "Munition.Bolzen.Explosivbolzen.Flaechenschaden": 120, "Munition.Bolzen.Streubolzen.Splitterschaden": 55}
VARIANTEN = [
    ("Stand (neue Ziele)", {}),
    ("R7", R7),
    ("R7 + Nobelisk -25 %", {**R7, **NOB75}),
    ("R7 + Nobelisk -25 % + GegnerLP 1,0", {**R7, **NOB75, "Stufen.Normal.GegnerLP": 1.0}),
]
KRIT = ["Ziel_NichtAlleTuerme", "Ziel_Wege", "Ziel_NichtAlles", "Ziel_NichtZuLeicht", "Ziel_KeineDominanz", "Ziel_KeineToteLinie", "Ziel_W2", "Ziel_W3", "Ziel_W4", "Ziel_W5", "Ziel_W6"]
with open("Ausgabe/sweep_0811_5.txt", "w", encoding="utf-8") as f:
    for name, bal in VARIANTEN:
        Z0 = st.lade_alles(bal)[2]
        erg, _ = st.massenlauf("Normal", 50, LAEUFE, 7, balance_ueber=bal)
        zb = st.zielbild(erg, Z0)
        fall = collections.defaultdict(list)
        for e in erg:
            if not e["Erfolg"]:
                fall[e["Linie"]].append(e["Welle"])
        zeile = (f"{name:34} Ziele {sum(bool(zb[k]) for k in KRIT)}/11  gesamt {zb['Quote']*100:3.0f} %  "
                 + "  ".join(f"{l} {q*100:.0f}" for l, q in sorted(zb["JeLinie"].items()))
                 + f"  | Verzug {zb['W_Verzug']} Ueberschuss {zb['W_Ueberschuss']:.2f} | fehlt: " + ",".join(k[5:] for k in KRIT if not zb[k])
                 + " | Fall-Median " + " ".join(f"{l.split()[-1]}:{statistics.median(v):.0f}" for l, v in sorted(fall.items())))
        print(zeile, flush=True); f.write(zeile + "\n"); f.flush()
