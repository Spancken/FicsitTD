"""Verlustkurve und Mehrziel-Faktoren aus Spielmessungen (07.11.2026, Testplan 3.10).

Modell (strategien.Lauf.kampf):
    qv      = Q_wirksam / max(1, Gegner/MaxGleichzeitig)^VerlustStreckung
    Verlust = VerlustGrund + VerlustQ * qv^VerlustP + Zusatz über QKritisch
    Q_wirksam rechnet je Turm mit dem wirksamen DPS der Formel (Turmwerte.dps) mal Mehrziel_<Typ>.

Angepasst werden VerlustQ, VerlustP und Mehrziel_<Typ> gemeinsam über alle Wellen ohne Munitionsmangel.
Gewehrturm und Kerngeschütz bleiben bei 1 (reines Einzelziel, Bezug).
Die Messung liefert Nenn-Q (Einzelziel-DPS, wie NennDPS im Mod); die Anteile je Typ kommen aus dem
nachgespielten Ausbauplan im Modell.

  python verlust_fit.py                      Standardmessungen anpassen, nur anzeigen
  python verlust_fit.py --schreiben          ... und in strategie_annahmen.json übernehmen (mit Sicherung)
  python verlust_fit.py --bericht X_Metriken.csv:Paket[:Ablage] ...   eigene Messungen
"""
import argparse
import collections
import datetime
import json
import os
import random
import shutil
import statistics

import strategien as st

HIER = os.path.dirname(os.path.abspath(__file__))
TYPEN = ["Bolzenturm", "Gewehrturm", "Nobelisk-Werfer", "Teslaspule", "Flammenwerfer", "Kern"]
FREI = ["Bolzenturm", "Nobelisk-Werfer", "Teslaspule", "Flammenwerfer"]
# Messungen mit bekannten Ausbauplänen (Kreis-*, Referenz-*, Strat-*); dritter Eintrag: Ablage der Strat-Pläne,
# die zu dieser Messung gehörten (strategie_plaene.py überschreibt die aktuelle Ablage beim Neuerzeugen)
STANDARD = [("2026-11-06_1251_Streuung_Metriken.csv", "Streuung", None), ("2026-11-06_1519_Kreislauf_Metriken.csv", "Kreislauf", None),
            ("2026-11-07_0917_Vormittag_Metriken.csv", "Vormittag", "strategie_plaene_Vormittag_0711.json"),
            ("2026-11-07_1714_Gesamt082_Metriken.csv", "Gesamt082", "strategie_plaene_Gesamt082.json"),
            ("2026-11-08_0848_Nachweis0831_Metriken.csv", "Nachweis0831", "strategie_plaene_Nachweis0831.json")]


def _lauf_fuer(name, B, Z, K, gabeln, tw, plaene, strats):
    import balance_kreislauf as bk
    import strategie_plaene as sp
    if name.startswith("Strat-"):
        p = plaene.get(name)
        if not p:
            return None
        ausbau = {int(k): [e[:4] for e in v] for k, v in p["Ausbau"].items()}
        forsch = {int(k): v for k, v in p["Forschung"].items()}
        return sp.PlanLauf(strats[p["Strategie"]], ausbau, forsch, B, Z, K, gabeln, tw, st.Wirtschaft(B, Z), "Normal")
    ausbau = bk.ausbau_fuer(name)
    if ausbau is None:
        return None
    import erzeuge_referenzen as er
    return bk.FesterLauf(ausbau, er.FORSCHUNG, B, Z, K, gabeln, tw, st.Wirtschaft(B, Z), "Normal")


def _anteile(L, B, tw):
    """je Typ: wirksames DPS der Formel und Einzelziel-DPS (wie Lauf.kampf, ohne Versorgung und ohne Mehrziel-Faktor)"""
    E, N = collections.Counter(), collections.Counter()
    for typ, lv in L.tuerme:
        dokt = L.doktrin_aktiv() and L.s["Doktrin"] == typ
        ms = [L.beste_munition_fuer_wirtschaft(typ)] if typ != "Teslaspule" else L.munition(typ)
        m = max(ms, key=lambda m: tw.dps(typ, lv, m, dokt, L.s["Gabel"]))
        E[typ] += tw.dps(typ, lv, m, dokt, L.s["Gabel"])
        N[typ] += tw.dps(typ, lv, m, dokt, L.s["Gabel"], einzel=True)
    if "KernGeschuetz" in L.erforscht:
        kg = B.get("Forschung.KernGeschuetz.Schaden", 60) / 1.5
        E["Kern"] += kg
        N["Kern"] += kg
    return E, N


def lade_wellen(messungen):
    """[(Paket, Plan, Welle, Q_Messung, Verlust, Gegner, r je Typ, Türme)] für alle Wellen ohne Munitionsmangel"""
    import balance_kreislauf as bk
    B, A, Z, katalog, phasen, K, gabeln, _ = st.lade_alles()
    tw = st.Turmwerte(B)
    strats = list(st.strategien(random.Random(7)))
    zeilen = []
    for datei, paket, *ablage in messungen:
        plaene = bk.lade_json(os.path.join(bk.AUS, (ablage[0] if ablage and ablage[0] else "strategie_plaene.json")), {})
        pfad = datei if os.path.isabs(datei) else os.path.join(bk.BERICHTE, datei)
        p = bk.lade_json(os.path.join(bk.TESTS, "Pakete", paket + ".json"), None)
        if not p or not os.path.exists(pfad):
            print(f"übersprungen: {datei} ({paket})")
            continue
        for name, lauf in bk.laeufe_aus(pfad, p["Plaene"]):
            L = _lauf_fuer(name, B, Z, K, gabeln, tw, plaene, strats)
            if L is None:
                continue
            nach_welle = {int(z["Welle"]): z for z in lauf}
            for n in range(1, max(nach_welle) + 1):
                D = Z["WellenGrund"] + Z["WellenJe"] * (n - 1)
                L.forsche(n, gabeln)
                L.baue(n, st.zyklus_min(Z, n, D), D)
                z = nach_welle.get(n)
                if not z or float(z["Hungrig_s"]) > 1:
                    continue   # Munitionsmangel: Verlust kommt nicht aus dem Druck
                E, N = _anteile(L, B, tw)
                ntot = sum(N.values())
                if ntot > 0:
                    zeilen.append((paket, name, n, float(z["Q"]), float(z["Kernverlust_Proz"]), int(z["Gegner"]),
                                   [E[t] / ntot for t in TYPEN], len(L.tuerme)))
    return zeilen, B, Z


class Anpassung:
    def __init__(self, zeilen, obergrenze, streckung):
        self.z, self.og, self.e = zeilen, obergrenze, streckung
        self.V = [z[4] for z in zeilen]
        self.VM = statistics.mean(self.V)

    def C(self, faktoren):
        return [faktoren.get(t, 1.0) for t in TYPEN]

    def qv(self, faktoren):
        C = self.C(faktoren)
        return [z[3] / max(1e-3, sum(r * c for r, c in zip(z[6], C))) / max(1.0, z[5] / self.og) ** self.e for z in self.z]

    def vorhersage(self, a, p, faktoren):
        return [a * q ** p for q in self.qv(faktoren)]

    def r2(self, v):
        return 1 - sum((m - x) ** 2 for m, x in zip(self.V, v)) / sum((m - self.VM) ** 2 for m in self.V)

    def _par(self, x):
        return x[0], x[1], dict(zip(FREI, x[2:]))

    def fehler(self, x):
        if x[0] <= 0 or not 0.3 < x[1] <= 4 or any(not 0.05 < c <= 30 for c in x[2:]):
            return 1e12
        return sum((m - v) ** 2 for m, v in zip(self.V, self.vorhersage(*self._par(x))))

    def passe_an(self):
        best = None
        for start in ([40, 1.0, 1, 1, 1, 1], [60, 1.4, 1, 2, 1, 4], [80, 2.0, 1, 3, 1, 6]):
            x, f = nelder(self.fehler, start)
            x, f = nelder(self.fehler, x)
            if best is None or f < best[1]:
                best = (x, f)
        return self._par(best[0])


def nelder(f, x0, schritte=4000):
    """Nelder-Mead ohne numpy"""
    n = len(x0)
    pts = [list(map(float, x0))]
    for i in range(n):
        x = list(map(float, x0))
        x[i] = x[i] * 1.3 if x[i] else 0.1
        pts.append(x)
    werte = [f(x) for x in pts]
    for _ in range(schritte):
        o = sorted(range(n + 1), key=lambda i: werte[i])
        pts, werte = [pts[i] for i in o], [werte[i] for i in o]
        c = [sum(p[j] for p in pts[:-1]) / n for j in range(n)]
        xr = [c[j] + (c[j] - pts[-1][j]) for j in range(n)]
        fr = f(xr)
        if fr < werte[0]:
            xe = [c[j] + 2 * (c[j] - pts[-1][j]) for j in range(n)]
            fe = f(xe)
            pts[-1], werte[-1] = (xe, fe) if fe < fr else (xr, fr)
        elif fr < werte[-2]:
            pts[-1], werte[-1] = xr, fr
        else:
            xc = [c[j] + 0.5 * (pts[-1][j] - c[j]) for j in range(n)]
            fc = f(xc)
            if fc < werte[-1]:
                pts[-1], werte[-1] = xc, fc
            else:
                pts = [pts[0]] + [[pts[0][j] + 0.5 * (x[j] - pts[0][j]) for j in range(n)] for x in pts[1:]]
                werte = [f(x) for x in pts]
    i = min(range(n + 1), key=lambda i: werte[i])
    return pts[i], werte[i]


def kalibriere(messungen=None, schreiben=False, nur_besser=True):
    """passt an; schreibt nur, wenn die neue Kurve die Messung besser erklärt als das heutige Modell"""
    messungen = messungen or STANDARD
    zeilen, B, Z = lade_wellen(messungen)
    if len(zeilen) < 100:
        print(f"Nur {len(zeilen)} Wellen, keine Anpassung.")
        return None
    an = Anpassung(zeilen, B.get("Wellen.MaxGleichzeitig", 60), Z.get("VerlustStreckung", 1.5))
    heute_f = {t: Z.get(f"Mehrziel_{t}", 1.0) for t in FREI}
    v0 = [Z.get("VerlustGrund", 0) + x for x in an.vorhersage(Z["VerlustQ"], Z.get("VerlustP", 1.0), heute_f)]
    a, p, f = an.passe_an()
    v = an.vorhersage(a, p, f)
    r0, r1 = an.r2(v0), an.r2(v)
    # Streuung um die Kurve: Mittel je Zehntel von Messung / Kurve, auf Mittel 1 normiert (wie auswerten, 06.11.)
    rel = sorted(m / max(0.3, x) for m, x in zip(an.V, v))
    dez = [statistics.mean(rel[i * len(rel) // 10:(i + 1) * len(rel) // 10]) for i in range(10)]
    mm = statistics.mean(dez)
    dez = [round(d / mm, 2) for d in dez]
    erg = {"Wellen": len(zeilen), "R2_heute": r0, "R2_neu": r1, "VerlustQ": a, "VerlustP": p, "Mehrziel": f, "Streuung": dez,
           "JePlan": collections.defaultdict(lambda: [0, 0.0, 0.0, 0.0])}
    for z, m, x, x0 in zip(zeilen, an.V, v, v0):
        e = erg["JePlan"][z[1]]
        e[0] += 1; e[1] += m; e[2] += x; e[3] += x0
    print(f"{len(zeilen)} Wellen aus {len(messungen)} Messungen")
    print(f"heute:  {Z.get('VerlustGrund', 0)} + {Z['VerlustQ']} * qv^{Z.get('VerlustP', 1.0)}, R² {r0:.3f}")
    print(f"neu:    {a:.2f} * qv^{p:.2f}, R² {r1:.3f}; Mehrziel " + ", ".join(f"{t} {c:.2f}" for t, c in f.items()))
    if schreiben and (r1 > r0 + 0.01 or not nur_besser):
        Zp = os.path.join(HIER, "strategie_annahmen.json")
        shutil.copy(Zp, os.path.join(HIER, "Ausgabe", "Kreislauf", f"strategie_annahmen_vor_Verlustkurve_{datetime.date.today().isoformat()}.json"))
        J = json.load(open(Zp, encoding="utf-8"))
        heute = datetime.date.today().isoformat()
        quelle = f"Kalibrierung {heute} (verlust_fit.py): {len(zeilen)} Wellen aus " + ", ".join(m[1] for m in messungen)
        J["VerlustGrund"] = {"Wert": 0.0, "Quelle": quelle + "; Kurve geht durch 0"}
        J["VerlustQ"] = {"Wert": round(a, 2), "Quelle": quelle + f", R² {r1:.2f} (vorher {r0:.2f})"}
        J["VerlustP"] = {"Wert": round(p, 2), "Quelle": quelle + ": Exponent der Verlustkurve, Verlust wächst schneller als Q"}
        J["VerlustStreuung"] = {"Wert": dez, "Quelle": quelle + ": Dezile Messung / Kurve, auf Mittel 1 normiert"}
        for t, c in f.items():
            J[f"Mehrziel_{t}"] = {"Wert": round(c, 2), "Quelle": quelle + ": Faktor auf das wirksame DPS der Formel (Turmwerte.dps), Gewehr = 1"}
        for k in [k for k in J if k.startswith("Wirk_")]:
            del J[k]   # alte Wirkgrade (Schaden je Nenn-DPS) wurden nie im Modell benutzt
        json.dump(J, open(Zp, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
        print("übernommen in strategie_annahmen.json")
    elif schreiben:
        print("nicht übernommen: erklärt die Messung nicht besser als heute")
    return erg


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--schreiben", action="store_true")
    ap.add_argument("--bericht", nargs="*", help="Datei:Paket")
    a = ap.parse_args()
    ms = [tuple(b.split(":")) for b in a.bericht] if a.bericht else None
    e = kalibriere(ms, a.schreiben)
    if e:
        print(f"{'Plan':32s} Wellen  Messung  neu  heute")
        for name, (n, m, x, x0) in e["JePlan"].items():
            print(f"{name:32s} {n:5d} {m:8.0f} {x:5.0f} {x0:5.0f}")
