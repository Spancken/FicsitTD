# 23.11.2026 (Marius, Entscheidung 4): Woran scheitert Endlos? „Es soll schwer sein, aber nicht unschaffbar – liegt es an den
# Gegnern, der Zeit oder der Munition?“ Rechnet je Stufe den heutigen Stand und Varianten, die jeweils EINEN Druck wegnehmen,
# dazu die Lage in den letzten Wellen vor dem Kernverlust. Rechnet nur im Speicher.
# Aufruf im Simulator-Ordner: python diagnose_endlos.py [Normal Hart ...]
import statistics, sys
import strategien as st, sim_modi as sm


def lauf(daten, stufe, B_ueber=None, Z_ueber=None):
    B, A, Z, katalog, phasen, K, gabeln = daten
    B2, Z2 = dict(B), dict(Z)
    for k, v in (B_ueber or {}).items():
        B2[k] = v(B2.get(k)) if callable(v) else v
    for k, v in (Z_ueber or {}).items():
        Z2[k] = v(Z2.get(k)) if callable(v) else v
    erg = sm.endlos_lauf((B2, A, Z2, katalog, phasen, K, gabeln), stufe, max_wellen=150, laeufe=3)
    return [r for r in erg if not r["Vorziehen"]]


def lage(erg):
    """Lage in den letzten 3 geschafften Wellen vor dem Ende (nur Laeufe, die nicht gekappt wurden)"""
    ue, kern, q, bau, abst, dauer = [], [], [], [], [], []
    for r in erg:
        if r["Kappung"]:
            continue
        v = r["Verlauf"][-4:]
        ue.append(sum(x["Ueberlauf"] for x in v) / len(v))
        q.append(statistics.mean(x["Q"] for x in v))
        bau.append(statistics.mean(x["Bauzeit_ungenutzt"] for x in v))
        abst.append(v[-1]["Abstand"])
        dauer.append(statistics.mean(x["Dauer"] for x in v))
    m = lambda xs: statistics.median(xs) if xs else float("nan")
    return f"Ueberlauf {m(ue):.0%}, Q {m(q):.2f}, Bauzeit frei {m(bau):.0f} s, Abstand {m(abst):.0f} s, Wellendauer {m(dauer):.0f} s"


def main():
    stufen = sys.argv[1:] or ["Normal", "Hart", "Albtraum"]
    B, A, Z, katalog, phasen, K, gabeln, _ = st.lade_alles(stand="091")
    daten = (B, A, Z, katalog, phasen, K, gabeln)
    for stufe in stufen:
        lp = f"Endlos.{stufe}.LPFaktor"
        abstand = f"Endlos.{stufe}.Abstand"
        varianten = [
            ("heute", {}, {}),
            ("Gegner: Leben x0,5", {lp: lambda x: (x or 1.0) * 0.5}, {}),
            ("Gegner: Budget x0,5 (weniger Gegner)", {"Endlos.BudgetFaktor": lambda x: (x or 0.9) * 0.5}, {}),
            ("Zeit: Wellenabstand x2", {abstand: lambda x: (x or sm.ENDLOS_WERTE[stufe][1]) * 2,
                                        "Endlos.AbstandMin": lambda x: (x or 90) * 2}, {}),
            ("Zeit: Bauzeit je Gebaeude halb", {}, {"BauSekundenJeGebaeude": lambda x: x * 0.5}),
            ("Zeit: Bauzeit unbegrenzt", {}, {"BauSekundenJeGebaeude": 0.0}),
            ("Modell: Bauzeit ansparen ohne Grenze", {}, {"BauzeitSpeicherSekunden": 10 ** 6}),
            ("Modell: Bauzeit ansparen bis 900 s", {}, {"BauzeitSpeicherSekunden": 900}),
            ("Munition/Wirtschaft: Coupons x3", {"Einsatz.EP.Grund": lambda x: (x or 0) * 3, "Einsatz.EP.JeWelle": lambda x: (x or 0) * 3}, {}),
            ("Munition/Wirtschaft: Gebaeude kostenlos", {}, {"PreisFaktor": 0.0}),
            ("Kern: Leben x2", {f"Stufen.{stufe}.KernLP": lambda x: x * 2}, {}),
        ]
        print(f"== {stufe}", flush=True)
        for name, bu, zu in varianten:
            erg = lauf(daten, stufe, bu, zu)
            w = statistics.median(r["Welle"] for r in erg)
            je = {}
            for r in erg:
                je.setdefault(r["Linie"], []).append(r["Welle"])
            beste = max(je, key=lambda l: statistics.median(je[l]))
            print(f"  {name:42s} Median W{w:5.1f}  beste Linie {beste} W{statistics.median(je[beste]):.0f}  | {lage(erg)}", flush=True)


if __name__ == "__main__":
    main()
