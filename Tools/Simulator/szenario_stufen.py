"""ERLEDIGT 07.11.2026: seit Mod 0.82 ist das der Grundstand in strategien.py (Rezepte, Munition, Gabel A). Nur noch als Vergleich.

Szenario Munitionsstufen (Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07.md, Vorgabe Marius 07.11.2026).

S1 Grundmunition = Leitrohstoff, S2 = + ein Rohstoff mit einem Schritt (Quarz, Caterium, Schwefel),
S3 ab W25 = Raffinerie-/Aluminium-Ketten. Ändert nur den Simulator im Speicher, nie den Mod.

  python szenario_stufen.py [--wellen 30 50] [--laeufe 2]
"""
import argparse
import collections
import statistics

import strategien as st

# ---------------------------------------------------------------- Vorschlag
REZEPTE = {
    # Armierungsbolzen bleibt aus Eisenbarren: aus Eisenstange (Original) kostet je Bolzenturm einen Konstruktor mehr,
    # das allein senkte die Erfolgsquote bis W30 von 68 auf 48 % (Ablation 07.11.). Früh muss es einfach bleiben.
    "Quarzkristall": ("Konstruktor", 8, {"Rohquarz": 5}, 3),
    "Caterium-Barren": ("Schmelzofen", 4, {"Caterium-Erz": 3}, 1),
    "Turbodraht": ("Konstruktor", 5, {"Caterium-Barren": 1}, 5),
    "Streubolzen": ("Assembler", 8, {"Durchschlagbolzen": 2, "Quarzkristall": 1}, 2),
    "Schockbolzen": ("Assembler", 8, {"Durchschlagbolzen": 2, "Turbodraht": 2}, 2),
    "Zielsuchmunition": ("Assembler", 6, {"Gewehrmunition": 1, "Turbodraht": 1}, 1),
    "Puls-Nobelisk": ("Assembler", 6, {"Nobelisk": 1, "Quarzkristall": 2}, 1),
    "Schwarzpulver": ("Assembler", 4, {"Kohle": 1, "Schwefel": 1}, 2),
    "Brandmunition": ("Assembler", 8, {"Gewehrmunition": 2, "Schwefel": 1}, 2),
    "Schwerölrückstand": ("Raffinerie", 6, {"Rohöl": 3}, 4),
    "Rauchschwaches Pulver": ("Raffinerie", 6, {"Schwarzpulver": 2, "Schwerölrückstand": 1}, 2),
    "Explosivbolzen": ("Assembler", 8, {"Durchschlagbolzen": 2, "Rauchschwaches Pulver": 1, "Stahlrohr": 1}, 2),
    "Streu-Nobelisk": ("Assembler", 8, {"Nobelisk": 2, "Rauchschwaches Pulver": 2}, 1),
    # Annahme Feldrezept: Original braucht Raffinerie + Wasser + Silica, Wasser liegt an keinem Kernplatz unter 300 m
    "Aluminiumbarren": ("Giesserei", 4, {"Bauxit": 2, "Kohle": 1}, 1),
    "Aluminiumgehäuse": ("Konstruktor", 2, {"Aluminiumbarren": 3}, 2),
    "Turbomunition": ("Assembler", 8, {"Gewehrmunition": 2, "Aluminiumgehäuse": 1}, 2),
    # ohne Biomasse (Marius 07.11.: Biomasse kommt nur aus Startkit und Geschenk je Welle, nicht aus der Fabrik)
    "Kompaktkohle": ("Assembler", 12, {"Kohle": 5, "Schwefel": 5}, 5),      # wie im Original
    "Gas-Nobelisk": ("Assembler", 6, {"Nobelisk": 1, "Schwefel": 2}, 1),    # Vorschlag statt 10 Biomasse
}
ROH = {"Rohquarz": "Miner", "Caterium-Erz": "Miner", "Schwefel": "Miner", "Bauxit": "Miner"}
# Schaden auf die Zielwerte (Wirkung je Turm, 100 % = Grundmunition)
BALANCE = {
    "Munition.Patronen.Zielsuch.Schaden": 20.0,          # 69 % -> 139 %
    "Munition.Patronen.Brand.BrandSchaden": 9.0,         # 133 % -> 158 %
    "Munition.Patronen.Turbo.Schaden": 15.0,             # 117 % -> 250 %
    "Munition.Nobelisk.Streu.Schaden": 225.0,            # 89 % -> 250 %
    # Flamme startet mit Treibstoff (100 % = Treibstoff 80/s), Turbotreibstoff ist ihre Stufe 2
    "Munition.Treibstoff.Turbotreibstoff.SchadenProSekunde": 120.0,   # 100 % -> 150 % des Treibstoffs
    "Munition.Bolzen.Betaeubungsbolzen.Schaden": 32.0,   # 38 % -> 80 % (plus Betäubung, die der Simulator nicht rechnet)
    "Munition.Nobelisk.Puls.Schaden": 90.0,              # 27 % -> 60 % (plus Betäubung)
}
# Variante B: S2 so stark wie heute der Explosivbolzen auf W5 (300 %), S3 darüber; sonst wird das Spiel schwerer (68 -> 45 % bis W30)
BALANCE_B = {
    "Munition.Bolzen.Streubolzen.Schaden": 43.0, "Munition.Bolzen.Streubolzen.Splitterschaden": 43.0,   # 175 % -> 300 %
    "Munition.Bolzen.Betaeubungsbolzen.Schaden": 59.0,       # 38 % -> 150 % plus Betäubung
    "Munition.Bolzen.Explosivbolzen.Flaechenschaden": 87.0,  # 300 % -> 400 % (S3)
}
NEUE_KNOTEN = {
    "Quarz": {"name": "Quarzverarbeitung", "baum": "Industrie", "ab": 10, "fp": 4, "vor": ["Assembler"]},
    "Caterium": {"name": "Caterium", "baum": "Industrie", "ab": 12, "fp": 4, "vor": ["Kupfer"]},
    "Schwefel": {"name": "Schwefel", "baum": "Industrie", "ab": 5, "fp": 3, "vor": []},
    "Bauxit": {"name": "Bauxit und Aluminium", "baum": "Industrie", "ab": 25, "fp": 8, "vor": ["Giesserei"]},
}
ZUSATZ_VOR = {"Streubolzen": ["Quarz"], "Schockbolzen": ["Caterium"], "Zielsuchmunition": ["Caterium"], "PulsNobelisk": ["Quarz"],
              "NobeliskWerfer": ["Schwefel"], "Brandmunition": ["Schwefel"], "Turbomunition": ["Bauxit"],
              "Explosivbolzen": ["Raffinerie"], "StreuNobelisk": ["Raffinerie"]}
NEUE_AB = {"Explosivbolzen": 25}


def anwenden(variante="A"):
    """Patcht strategien im Speicher; gibt die Balance-Überschreibung zurück"""
    st.REZEPTE.update(REZEPTE)
    st.ROH.update(ROH)
    alt_lade = st.lade_forschung

    def lade_forschung():
        K, gabeln = alt_lade()
        K.update({k: dict(v) for k, v in NEUE_KNOTEN.items()})
        for k, vs in ZUSATZ_VOR.items():
            K[k]["vor"] = list(K[k]["vor"]) + [v for v in vs if v not in K[k]["vor"]]
        for k, ab in NEUE_AB.items():
            K[k]["ab"] = ab
        # Gabel A: Streubolzen (Quarz) gegen Schockbolzen (Caterium); Explosiv wird S3 für alle
        gabeln = [["Streubolzen", "Schockbolzen"] if g == ["Explosivbolzen", "Streubolzen"] else g for g in gabeln]
        return K, gabeln
    st.lade_forschung = lade_forschung
    st.GABEL_OPTIONEN["Bolzen"] = ["Streubolzen", "Schockbolzen"]
    # bevorzugt wird die zuletzt freigeschaltete Munition: Reihenfolge nach Stufe
    # Flamme ohne Biotreibstoff: Treibstoff kommt mit dem Flammenwerfer (Raffinerie ist Voraussetzung)
    f = {m[0]: m for m in st.MUNITION["Flammenwerfer"]}
    if "Treibstoff" in f:
        st.MUNITION["Flammenwerfer"] = [f["Treibstoff"][:2] + ("",) + f["Treibstoff"][3:], f["Turbotreibstoff"]]
    b = {m[0]: m for m in st.MUNITION["Bolzenturm"]}
    st.MUNITION["Bolzenturm"] = [b[n] for n in ("Armierungsbolzen", "Durchschlagbolzen", "Schockbolzen", "Streubolzen", "Explosivbolzen")]
    return {**BALANCE, **(BALANCE_B if variante == "B" else {})}


def auswerten(ergebnisse):
    je = collections.defaultdict(list)
    for r in ergebnisse:
        je[r["Linie"]].append(r)
    linien = {l: sum(r["Erfolg"] for r in rs) / len(rs) for l, rs in je.items()}
    erfolg = [r for r in ergebnisse if r["Erfolg"]] or ergebnisse
    fabrik = statistics.median(r.get("EP_Fabrik", 0) for r in erfolg)
    welle = statistics.median(r["Welle"] for r in ergebnisse)
    return linien, sum(r["Erfolg"] for r in ergebnisse) / len(ergebnisse), fabrik, welle


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wellen", type=int, nargs="*", default=[30, 50])
    ap.add_argument("--laeufe", type=int, default=2)
    ap.add_argument("--variante", nargs="*", default=["A", "B"])
    a = ap.parse_args()
    Z = st.lade_alles()[2]
    stand = {n: st.massenlauf("Normal", n, a.laeufe, 7)[0] for n in a.wellen}
    neu_v = {}
    for v in a.variante:
        bal = anwenden(v)
        neu_v[v] = {n: st.massenlauf("Normal", n, a.laeufe, 7, balance_ueber=bal)[0] for n in a.wellen}
    for n in a.wellen:
        ls, qs, fs, ws = auswerten(stand[n])
        neu = {v: auswerten(r[n]) for v, r in neu_v.items()}
        print(f"\n## bis W{n}")
        print("| | Stand | " + " | ".join(f"Variante {v}" for v in neu) + " |")
        print("|---|---|" + "---|" * len(neu))
        print(f"| Erfolg gesamt | {qs*100:.0f} % | " + " | ".join(f"{x[1]*100:.0f} %" for x in neu.values()) + " |")
        for l in ls:
            print(f"| {l} | {ls[l]*100:.0f} % | " + " | ".join(f"{x[0].get(l, 0)*100:.0f} %" for x in neu.values()) + " |")
        print(f"| EP in Fabrik (Median) | {fs:.0f} | " + " | ".join(f"{x[2]:.0f}" for x in neu.values()) + " |")
        zs = st.zielbild(stand[n], Z)
        for k in ("W_Ueberschuss", "W_Fabrik", "W_Forschung", "W_Not"):
            print(f"| {k} | {zs[k]:.2f} | " + " | ".join(f"{st.zielbild(r[n], Z)[k]:.2f}" for r in neu_v.values()) + " |")
        for v, r in neu_v.items():
            g = collections.Counter(x.get("Gabel", "").split("/")[0] for x in r[n])
            e = collections.Counter(x.get("Gabel", "").split("/")[0] for x in r[n] if x["Erfolg"])
            print(f"Variante {v}, Erfolg nach Bolzen-Gabel: " + ", ".join(f"{k} {e[k]/g[k]*100:.0f} %" for k in g))


if __name__ == "__main__":
    main()
