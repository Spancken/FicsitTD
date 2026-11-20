"""P12-Fit (17.11.2026): Simulator gegen die Kalibrierläufe P12-K und P12-K-Nachlauf.

Drei Schritte wie im P12-Bericht (Abschnitt 5), jede Größe an ihrer eigenen Messung:
  1. Feueranteil je Wellenbereich und Wellendauer (WellenGrund, WellenJe)  an K1 und K3
  2. Mehrziel-Faktor je Turmart                                            an K4 (je 6 Türme L2, W15–19)
  3. Skala der Verlustkurve VerlustQ (Exponent 1,37 und Streckung 1,5 fest) an K1 und K3
Prüfung (nicht gefittet): K5 mit Ausbau W31–50, Kern-Tiefpunkt auf ±15 Punkte.

Nur gültige Wellen (FabrikTakt_Anteil >= 0,95) ohne Munitionsmangel (Hungrig_s <= 1).

  python p12_fit.py [--schreiben]     --schreiben übernimmt die Werte in strategie_annahmen.json (mit Sicherung)
Ausgabe: Ausgabe/P12-Fit.md
"""
import argparse
import collections
import csv
import datetime
import json
import os
import shutil
import statistics

import strategien as st
from kalibriere import basis_je_welle

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HIER))
BERICHTE = r"D:\OneDrive\Claude\Claude Modding\Satisfactory\Testberichte"
MESSUNGEN = ["2026-11-17_1428_P12-K_Metriken.csv", "2026-11-17_1627_P12-K-Nachlauf_Metriken.csv"]
TYPEN = ["Bolzenturm", "Gewehrturm", "Nobelisk-Werfer", "Teslaspule", "Flammenwerfer"]
REZEPT_MUN = {"Recipe_TD_Shoot_SpikedRebar": "Armierungsbolzen", "Recipe_TD_Shoot_Durchschlag": "Durchschlagbolzen",
              "Recipe_TD_Gun_Standard": "Standard", "Recipe_TD_Tesla_Discharge": "Entladung",
              "Recipe_TD_Nobelisk_Explosive": "Nobelisk", "Recipe_TD_Flame_Fuel": "Treibstoff"}
# K1/K2 bauen ueber den Fabrik-Schritt (keine baue-Schritte): 4 Bolzentuerme L2 mit Armierungsbolzen
FEST = {"P12-K1-Bolzen": [("Bolzenturm", 2, "Recipe_TD_Shoot_SpikedRebar")] * 4,
        "P12-K2-Bolzen-ohnePause": [("Bolzenturm", 2, "Recipe_TD_Shoot_SpikedRebar")] * 4}
K3 = "Strat-Gewehr-Tesla"
K5 = "P12-K5-Misch-W31-50"
STANDARDREZEPT = {"Bolzenturm": "Recipe_TD_Shoot_SpikedRebar", "Gewehrturm": "Recipe_TD_Gun_Standard",
                  "Teslaspule": "Recipe_TD_Tesla_Discharge", "Nobelisk-Werfer": "Recipe_TD_Nobelisk_Explosive",
                  "Flammenwerfer": "Recipe_TD_Flame_Fuel"}


def f(x):
    return float(str(x).replace(",", "."))


def lade_laeufe():
    """[(Datei, Plan, Lauf, [Zeilen])] in Reihenfolge der Messungen"""
    laeufe = []
    for d in MESSUNGEN:
        kennung = None
        for z in csv.DictReader(open(os.path.join(BERICHTE, d), encoding="utf-8"), delimiter=";"):
            k = (z["Plan"], z["Lauf"])
            if k != kennung:
                kennung = k
                laeufe.append((d, z["Plan"], z["Lauf"], []))
            laeufe[-1][3].append(z)
    return laeufe


def gueltig(z):
    return f(z["FabrikTakt_Anteil"]) >= 0.95 and f(z["Hungrig_s"]) <= 1


def tuerme_je_welle(plan):
    if plan in FEST:
        return {n: FEST[plan] for n in range(1, 51)}
    p = json.load(open(os.path.join(REPO, "Tests", "Plaene", plan + ".json"), encoding="utf-8-sig"))
    je = basis_je_welle(p)
    return {n: [(t, l, r or STANDARDREZEPT.get(t)) for t, l, r in v] for n, v in je.items()}


def mun(typ, rezept):
    name = REZEPT_MUN.get(rezept)
    return next((m for m in st.MUNITION[typ] if m[0] == name), st.MUNITION[typ][0])


def formel(tw, tuerme):
    """je Typ: wirksames DPS der Formel (E) und Einzelziel-DPS (N), wie verlust_fit._anteile"""
    E, N = collections.Counter(), collections.Counter()
    for t, lv, r in tuerme:
        m = mun(t, r)
        E[t] += tw.dps(t, lv, m, False, {})
        N[t] += tw.dps(t, lv, m, False, {}, einzel=True)
    return E, N


def qv(z, tuerme, tw, C, og, streckung):
    E, N = formel(tw, tuerme)
    nt = sum(N.values())
    if nt <= 0:
        return None
    k = sum(E[t] * C.get(t, 1.0) for t in E) / nt
    return f(z["Q"]) / max(1e-3, k) / max(1.0, int(z["Gegner"]) / og) ** streckung


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--schreiben", action="store_true")
    a = ap.parse_args()
    B, A, Z, *_ = st.lade_alles()
    tw = st.Turmwerte(B)
    og, streckung, p = B.get("Wellen.MaxGleichzeitig", 60), Z.get("VerlustStreckung", 1.5), Z.get("VerlustP", 1.37)
    laeufe = lade_laeufe()
    aus = ["# P12-Fit: Simulator gegen P12-K und P12-K-Nachlauf", "",
           f"Stand {datetime.date.today().isoformat()}, erzeugt von `Tools/Simulator/p12_fit.py`. Messungen: " + ", ".join(f"`{m}`" for m in MESSUNGEN) + ".",
           "Gewertet nur gültige Wellen (Fabrik-Takt ≥ 0,95) ohne Munitionsmangel. Fest: Exponent "
           f"{p}, Streckung {streckung}, Wellenaufbau, Nenn-DPS.", ""]

    # ---------- Schritt 1: Feueranteil und Wellendauer (K1, K3)
    anker = [l for l in laeufe if l[1] in ("P12-K1-Bolzen", K3)]
    bereiche = [(1, 4), (5, 9), (10, 99)]
    up = {b: [] for b in bereiche}
    dauer = []
    for _, plan, _, zeilen in anker:
        for z in zeilen:
            if not gueltig(z):
                continue
            n = int(z["Welle"])
            for b in bereiche:
                if b[0] <= n <= b[1]:
                    up[b].append(f(z["Uptime_Proz"]) / 100)
            if plan == K3:
                dauer.append((n, f(z["Dauer_s"])))
    feuer_neu = [[b[1], round(statistics.median(up[b]), 2)] for b in bereiche]
    feuer_alt = Z["FeueranteilJeWelle"]
    nx = [n for n, _ in dauer]
    dy = [d for _, d in dauer]
    mx, my = statistics.mean(nx), statistics.mean(dy)
    je = sum((x - mx) * (y - my) for x, y in zip(nx, dy)) / sum((x - mx) ** 2 for x in nx)
    grund = my - je * (mx - 1)
    aus += ["## Schritt 1: Feueranteil und Wellendauer", "",
            "| Wellen | Feueranteil heute | gemessen (Median Uptime K1+K3) | Wellen gewertet |", "|---|---|---|---|"]
    for (b, alt), (_, neu) in zip(zip(bereiche, feuer_alt), feuer_neu):
        aus.append(f"| W{b[0]}–{b[1] if b[1] < 99 else '50'} | {alt[1]:.2f} | {neu:.2f} | {len(up[b])} |")
    aus += ["", f"Wellendauer K3 (`{K3}`, W1–30, lineare Anpassung): **{grund:.1f} s + {je:.2f} s je Welle** "
            f"(heute {Z['WellenGrund']} + {Z['WellenJe']}).", ""]

    # ---------- Schritt 2: Mehrziel je Turmart (K4)
    m = {}
    aus += ["## Schritt 2: Mehrziel-Faktoren (K4, je 6 Türme L2, W15–19)", "",
            "Wirkung je Nenn-DPS beim Feuern = Schaden ÷ (Dauer × Uptime × K_DPS). Mehrziel = Wirkung ÷ Formel-Mehrziel (wirksam ÷ Einzelziel), auf Gewehr = 1.", "",
            "| Turm | Wellen | Wirkung je Nenn-DPS | Formel wirksam/einzel | Mehrziel neu | heute |", "|---|---|---|---|---|---|"]
    roh = {}
    for _, plan, _, zeilen in laeufe:
        if not plan.startswith("P12-K4-"):
            continue
        tj = tuerme_je_welle(plan)
        w = []
        for z in zeilen:
            if not gueltig(z) or not 15 <= int(z["Welle"]) <= 19:
                continue
            nenn = f(z["K_DPS"]) * f(z["Dauer_s"]) * max(0.05, f(z["Uptime_Proz"]) / 100)
            if nenn > 0:
                w.append(f(z["Schaden"]) / nenn)
        typ = tj[15][0][0]
        E, N = formel(tw, tj[15])
        roh[typ] = (len(w), statistics.median(w) if w else None, E[typ] / N[typ])
    bezug = roh["Gewehrturm"][1] / roh["Gewehrturm"][2]
    for typ in TYPEN:
        n, wirk, fm = roh.get(typ, (0, None, 1))
        if wirk is None:
            continue
        m[typ] = round(wirk / fm / bezug, 2)
        aus.append(f"| {typ} | {n} | {wirk:.2f} | {fm:.2f} | **{m[typ]:.2f}** | {Z.get('Mehrziel_' + typ, 1.0):.2f} |")
    m["Gewehrturm"] = 1.0
    aus.append("")

    # ---------- Schritt 3: Skala der Verlustkurve (K1, K3)
    punkte = []
    for _, plan, lauf, zeilen in anker:
        tj = tuerme_je_welle(plan)
        for z in zeilen:
            n = int(z["Welle"])
            if not gueltig(z) or not tj.get(n):
                continue
            # K1: W9-10 schaukeln sich bei tiefem Kern auf (Nachlauf 17.11.), gewertet bis W8
            if plan == "P12-K1-Bolzen" and n > 8:
                continue
            q = qv(z, tj[n], tw, m, og, streckung)
            q_alt = qv(z, tj[n], tw, {t: Z.get("Mehrziel_" + t, 1.0) for t in TYPEN}, og, streckung)
            if q is not None:
                punkte.append((plan, n, f(z["Kernverlust_Proz"]), q, q_alt))
    x = [q ** p for _, _, _, q, _ in punkte]
    v = [vv for _, _, vv, _, _ in punkte]
    a_neu = sum(vi * xi for vi, xi in zip(v, x)) / sum(xi * xi for xi in x)
    vm = statistics.mean(v)
    r2 = lambda vor: 1 - sum((mm - vv) ** 2 for mm, vv in zip(v, vor)) / sum((mm - vm) ** 2 for mm in v)
    vor_neu = [a_neu * xi for xi in x]
    vor_alt = [Z["VerlustQ"] * qa ** p for *_, qa in punkte]
    aus += ["## Schritt 3: Skala der Verlustkurve (K1 W1–8, K3 W1–30)", "",
            f"{len(punkte)} Wellen. **VerlustQ {a_neu:.1f}** (heute {Z['VerlustQ']}), R² {r2(vor_neu):.2f} (heutiges Modell {r2(vor_alt):.2f}).", "",
            "| Plan | Phase | Wellen | Verlust gemessen Median | Modell neu | Modell heute |", "|---|---|---|---|---|---|"]
    gruppen = collections.defaultdict(list)
    for (plan, n, vv, _, _), vn, va in zip(punkte, vor_neu, vor_alt):
        gruppen[(plan, (n - 1) // 10 + 1)].append((vv, vn, va))
    for (plan, ph), g in sorted(gruppen.items()):
        aus.append(f"| {plan} | {ph} | {len(g)} | {statistics.median(e[0] for e in g):.1f} % | {statistics.median(e[1] for e in g):.1f} % | {statistics.median(e[2] for e in g):.1f} % |")
    aus.append("")

    # ---------- Prüfung K5 (nicht gefittet)
    k5 = [l for l in laeufe if l[1] == K5][-1]   # der Nachlauf mit Ausbau
    tj = tuerme_je_welle(K5)
    aus += ["## Prüfung: K5 mit Ausbau (W31–50, nicht gefittet)", "",
            "Kern im Modell: Vorwelle − vorhergesagter Verlust + gemessene Erholung zwischen den Wellen (Reparatur), damit nur die Verlustkurve geprüft wird.", "",
            "| Welle | Türme | Q | Verlust gemessen | Modell neu | Modell heute | Kern gemessen | Kern Modell neu | Kern Modell heute |", "|---|---|---|---|---|---|---|---|---|"]
    kern_m, kern_n, kern_h = None, None, None
    tief = {"m": 100, "n": 100, "h": 100}
    sv = {"m": 0, "n": 0, "h": 0}
    for z in k5[3]:
        n = int(z["Welle"])
        q = qv(z, tj[n], tw, m, og, streckung)
        qa = qv(z, tj[n], tw, {t: Z.get("Mehrziel_" + t, 1.0) for t in TYPEN}, og, streckung)
        vn, vh, vmeas = a_neu * q ** p, Z["VerlustQ"] * qa ** p, f(z["Kernverlust_Proz"])
        km = f(z["Kern_Proz"])
        if kern_m is None:
            kern_n = kern_h = km + vmeas   # Start wie gemessen
            erhol = 0.0
        else:
            erhol = km - kern_m + vmeas
        kern_n = min(100.0, kern_n + erhol - vn) if kern_m is not None else kern_n - vn
        kern_h = min(100.0, kern_h + erhol - vh) if kern_m is not None else kern_h - vh
        kern_m = km
        for k, val in (("m", km), ("n", kern_n), ("h", kern_h)):
            tief[k] = min(tief[k], val)
        sv["m"] += vmeas; sv["n"] += vn; sv["h"] += vh
        aus.append(f"| {n} | {len(tj[n])} | {f(z['Q']):.2f} | {vmeas:.1f} | {vn:.1f} | {vh:.1f} | {km:.0f} | {kern_n:.0f} | {kern_h:.0f} |")
    ok = abs(tief["n"] - tief["m"]) <= 15
    aus += ["", f"Summe Verlust W31–50: gemessen {sv['m']:.0f}, Modell neu {sv['n']:.0f}, heute {sv['h']:.0f} Punkte. "
            f"Kern-Tiefpunkt: gemessen {tief['m']:.0f} %, Modell neu {tief['n']:.0f} %, heute {tief['h']:.0f} %. "
            f"Ziel ±15 Punkte: **{'erreicht' if ok else 'verfehlt'}**.", ""]

    ziel = os.path.join(HIER, "Ausgabe", "P12-Fit.md")
    os.makedirs(os.path.dirname(ziel), exist_ok=True)
    open(ziel, "w", encoding="utf-8").write("\n".join(aus) + "\n")
    print("\n".join(aus))
    if a.schreiben:
        zp = os.path.join(HIER, "strategie_annahmen.json")
        shutil.copy(zp, os.path.join(HIER, "Ausgabe", f"strategie_annahmen_vor_P12-Fit_{datetime.date.today().isoformat()}.json"))
        J = json.load(open(zp, encoding="utf-8"))
        q = f"P12-Fit {datetime.date.today().isoformat()} (p12_fit.py, P12-K + Nachlauf)"
        J["FeueranteilJeWelle"] = {"Wert": feuer_neu, "Quelle": q + ": Median Uptime K1+K3 je Wellenbereich"}
        J["WellenGrund"] = {"Wert": round(grund, 1), "Quelle": q + ": Wellendauer K3 W1-30, linear"}
        J["WellenJe"] = {"Wert": round(je, 2), "Quelle": q + ": Wellendauer K3 W1-30, linear"}
        for t, c in m.items():
            if t != "Gewehrturm":
                J["Mehrziel_" + t] = {"Wert": c, "Quelle": q + ": K4 W15-19, Wirkung je Nenn-DPS / Formel, Gewehr = 1"}
        J["VerlustQ"] = {"Wert": round(a_neu, 2), "Quelle": q + f": K1 W1-8 + K3 W1-30, {len(punkte)} Wellen, Exponent und Streckung fest, K5-Pruefung {'erreicht' if ok else 'verfehlt'}"}
        json.dump(J, open(zp, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
        print("übernommen in strategie_annahmen.json")


if __name__ == "__main__":
    main()
