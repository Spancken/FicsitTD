"""Vergleich Strategie-Prüfstand 0.9.0 gegen 0.9.1 (Endgame-Pakete E1-E3), Normal, 50 Wellen.

  python endgame_vergleich.py --laeufe 3      -> Ausgabe/Endgame_091.json

Varianten: 090 (ohne E1-E3, Forschungsbaum 0.9.0), 091 (mit E1-E3, Spieler stellt Munition nur um, wenn die Kette sofort
bezahlbar ist) und 091_sparen (wie 091, Spieler spart bis zu SparWellen = 10 Wellen-Einkommen für Raketentreibstoff/Plasmaflamme).
Alle Annahmen in strategie_annahmen.json; die Kombo-Faktoren sind im Spiel nicht gemessen.
"""
import argparse
import collections
import json
import os
import random
import statistics
import time

import strategien as st

HIER = os.path.dirname(os.path.abspath(__file__))
GRUPPE = {l: ("Mono" if l.startswith("Mono") else "Breit" if l == "Breit" else "Duo") for l in st.LINIEN}
NUTZ = ["Raketentreibstoff", "Ionisiert", "Hochspannung", "Tor", "Lockfeuer", "Elektrozaun", "Oelschlick"]
ANNAHME_SCHLUESSEL = ["KomboAnteilEngstelle", "KomboAnteilGelockt", "KomboGegnerAmZaun", "KomboAnteilPlatte", "KomboPlatteVerweilSekunden",
                      "KomboTuermeJeBau", "LockfeuerKohleJeRuf", "LockfeuerRufSekunden", "OelschlickSchweroelJeMin", "StickstoffJeFoerderanlage",
                      "DruckerzeugerStrom", "SplitterFuerTreibstoff", "MunitionSparen", "SparWellen", "TreibstoffkraftNutzen", "BeuteAnteil"]


def med(xs, nd=2):
    xs = [x for x in xs if x is not None]
    return round(statistics.median(xs), nd) if xs else None


def kennzahlen(rs, stand, name):
    fehl = [r for r in rs if not r["Erfolg"]]
    verl = [v for r in rs for (_, _, v, *_rest) in r["Verlauf"]]
    verl_spaet = [v for r in rs for (n, _, v, *_rest) in r["Verlauf"] if n > 30]
    nutz = {}
    spaet = [r for r in rs if r["Welle"] > 35] or rs
    for k in NUTZ:
        nutz[k] = {"anteil_laeufe": round(sum(1 for r in rs if r.get(f"Nutzung_{k}", 0) > 0) / len(rs), 3),
                   "anteil_laeufe_ueber_w35": round(sum(1 for r in spaet if r.get(f"Nutzung_{k}", 0) > 0) / len(spaet), 3),
                   "wellen_mittel": round(statistics.mean(r.get(f"Nutzung_{k}", 0) for r in rs), 2),
                   "ab_welle_median": med([r.get(f"NutzungAb_{k}") for r in rs if r.get(f"NutzungAb_{k}")], 1)}
    nutz["Sparen_Wellen_mittel"] = round(statistics.mean(r.get("Nutzung_SparWellen", 0) for r in rs), 2)
    return {"linie": name, "stand": stand, "laeufe": len(rs),
            "quote_w30": round(sum(1 for r in rs if r["Erfolg"] or r["Welle"] > 30) / len(rs), 3),
            "quote_w50": round(sum(r["Erfolg"] for r in rs) / len(rs), 3),
            "verlust_median": med(verl), "verlust_median_w31_50": med(verl_spaet),
            "kern_tiefpunkt_median_erfolg": med([r["KernMin"] for r in rs if r["Erfolg"]], 1),
            "fall_welle_median": med([r["Welle"] for r in fehl], 1),
            "fall_wellen_bereich": {"bis_w30": sum(1 for r in fehl if r["Welle"] <= 30), "w31_40": sum(1 for r in fehl if 30 < r["Welle"] <= 40),
                                    "w41_50": sum(1 for r in fehl if r["Welle"] > 40)},
            "ursachen": dict(collections.Counter(r["Ursache"] for r in fehl).most_common()),
            "coupons_rest_median": med([r["EP_Rest"] for r in rs], 0), "coupons_summe_median": med([r["EP_Summe"] for r in rs], 0),
            "ausgaben_median": {k: med([r.get(f"EP_{k}", 0) for r in rs], 0) for k in ("Fabrik", "Tuerme", "Level", "Kern", "Bauten")},
            "forschung_median": med([r["Forschung"] for r in rs], 0),
            "nutzung": nutz}


def wellen(rs, stand, name, laenge):
    aus = []
    for n in range(1, laenge + 1):
        z = [v for r in rs for v in r["Verlauf"] if v[0] == n]
        if not z:
            continue
        aus.append({"welle": n, "stand": stand, "linie": name, "lebend": round(len(z) / len(rs), 3),
                    "q_median": med([v[1] for v in z], 3), "kernverlust": round(statistics.mean(v[2] for v in z), 2),
                    "kern_mittel": round(statistics.mean(v[3] for v in z), 1),
                    "coupons_median": med([v[4] for v in z], 0), "fp_summe_median": med([v[5] for v in z], 0)})
    return aus


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--laeufe", type=int, default=3)
    ap.add_argument("--wellen", type=int, default=50)
    ap.add_argument("--seed", type=int, default=7)
    a = ap.parse_args()
    liste = st.strategien(random.Random(a.seed))
    varianten = [("090", "090", {}), ("091", "091", {}), ("091_sparen", "091", {"MunitionSparen": 1})]
    alle, ziel, t0 = {}, {}, time.time()
    for name, stand, ueber in varianten:
        erg, quelle = st.massenlauf("Normal", a.wellen, a.laeufe, a.seed, z_ueber={"Verlauf": 1, **ueber}, strategie_liste=liste, stand=stand)
        alle[name] = erg
        Z = st.lade_alles(stand=stand)[2]
        Z.update(ueber)
        zb = st.zielbild(erg, Z)
        ziel[name] = {k: (round(v, 3) if isinstance(v, float) else v) for k, v in zb.items() if k != "JeLinie"}
        print(f"{name}: {len(erg)} Läufe, Erfolg {zb['Quote'] * 100:.0f} %, {time.time() - t0:.0f} s")
    Z = st.lade_alles(stand="091")[2]
    out = {"erzeugt": time.strftime("%Y-%m-%d %H:%M"), "stufe": "Normal", "wellen_anzahl": a.wellen, "laeufe_je_strategie": a.laeufe,
           "strategien": len(liste), "balance": quelle,
           "annahmen": {k: {"wert": Z[k], "quelle": json.load(open(os.path.join(HIER, "strategie_annahmen.json"), encoding="utf-8"))[k]["Quelle"]} for k in ANNAHME_SCHLUESSEL},
           "zielbild": ziel, "linien": [], "gruppen": [], "wellen": [], "befunde": [], "empfehlungen": []}
    for name, erg in alle.items():
        je = collections.defaultdict(list)
        for r in erg:
            je[r["Linie"]].append(r)
        for l in st.LINIEN:
            out["linien"].append(kennzahlen(je[l], name, l))
            out["wellen"].extend(wellen(je[l], name, l, a.wellen))
        for g in ("Mono", "Duo", "Breit"):
            rs = [r for r in erg if GRUPPE[r["Linie"]] == g]
            out["gruppen"].append(kennzahlen(rs, name, g))
            out["wellen"].extend(wellen(rs, name, g, a.wellen))
    pfad = os.path.join(HIER, "Ausgabe", "Endgame_091.json")
    alt = json.load(open(pfad, encoding="utf-8")) if os.path.exists(pfad) else {}
    out["befunde"], out["empfehlungen"] = alt.get("befunde", []), alt.get("empfehlungen", [])   # Texte von Hand, bleiben beim Neurechnen
    json.dump(out, open(pfad, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print("geschrieben:", pfad)
    for name in alle:
        print(name, " ".join(f"{k['linie']}: {k['quote_w30'] * 100:.0f}/{k['quote_w50'] * 100:.0f}" for k in out["linien"] if k["stand"] == name))


if __name__ == "__main__":
    main()
