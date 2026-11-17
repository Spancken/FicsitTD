"""FicsitTD P12 Fenster 2 (Bericht-P12-Balancing-Methode-2026-11-17.md, Abschnitt 8 Donnerstag): Paket P12-E erzeugen.

  python erzeuge_p12e.py   -> Tests\\Plaene\\P12-E-*.json und Tests\\Pakete\\P12-E.json

Kandidaten aus dem kalibrierten Modell (Tools\\Simulator\\p12_kandidaten.py, 17.11.):
  A: Stufen.Normal.EliteAbWelle 13 -> 16
  B: wie A, dazu Wellen.LPJeWelleAb31 0,1 -> 0,6
Die Werte kommen ueber den Plan-Schritt `balance` (kein neuer Build).
R2 = Referenz-Misch W1-30 wie bisher, danach der Ausbau aus K5 (erzeuge_p12k.MISCH_SPAET) bis W50.
R3 (Referenz-Gewehr) und R6 (Referenz-Faul) laufen bis W30 und sehen von B nichts, deshalb nur mit A.
"""
import copy
import json
import os

import erzeuge_p12k as k
import erzeuge_referenzen as er

HIER = os.path.dirname(os.path.abspath(__file__))
KANDIDAT = {"A": {"Stufen.Normal.EliteAbWelle": 16},
            "B": {"Stufen.Normal.EliteAbWelle": 16, "Wellen.LPJeWelleAb31": 0.6}}


def lade(name):
    return json.load(open(os.path.join(k.PLAENE, name + ".json"), encoding="utf-8-sig"))


def mit_balance(schritte, werte):
    """balance-Schritt direkt nach der Versorgung, also vor dem ersten Bau und der ersten Welle"""
    i = next(n for n, s in enumerate(schritte) if "versorgung" in s)
    return schritte[:i + 1] + [{"balance": dict(werte)}] + schritte[i + 1:]


def r2(kand):
    p = lade("Referenz-Misch")
    s = p["Schritte"]
    ende30 = next(n for n, x in enumerate(s) if x.get("wellenlauf") == 30)
    s = mit_balance(s[:ende30 + 1], KANDIDAT[kand])
    belegt = {"aussen": 0, "innen": 0}
    for welle in sorted(er.MISCH):
        if welle <= 30:
            for t, n, lvl, rez, *ring in er.MISCH[welle]:
                k.ring_fuer(belegt, ring[0] if ring else None, n)

    def baue(t, n, lvl, rez, *ring):
        r = k.ring_fuer(belegt, ring[0] if ring else None, n)
        return {"baue": t, "anzahl": n, "level": lvl, "rezept": rez, **({"ring": r} if r else {})}
    s += [{"ueberblick": True}, {"foto": f"P12-E-R2{kand}_W30"}]
    s += [baue(*e) for e in k.MISCH_SPAET[30]]
    for bis in (35, 40, 45, 50):
        s.append({"wellenlauf": bis, "zeitraffer": 4, "max": 900})
        s += [baue(*e) for e in k.MISCH_SPAET.get(bis, [])]
    s += [{"ueberblick": True}, {"foto": f"P12-E-R2{kand}_W50"},
          {"pruefe": "KernLebt", "text": "Kern hat Welle 50 ueberstanden"},
          {"pruefe": "KeineWarnungen", "text": "keine Warnungen oder Fehler des Mods im Log"}]
    q = copy.deepcopy(p)
    q.update({"Name": f"P12-E-R2-Misch-{kand}", "MaxMinuten": 80, "Schritte": s,
              "_Hinweis": f"R2 (P12 Fenster 2): Referenz-Misch bis W30, danach Ausbau wie K5 bis W50, Kandidat {kand} "
                          f"({', '.join(f'{a} {b}' for a, b in KANDIDAT[kand].items())}). Erzeugt von Tests\\erzeuge_p12e.py."})
    return q


def bis30(vorlage, name, kand, rolle):
    p = lade(vorlage)
    q = copy.deepcopy(p)
    q.update({"Name": name, "Schritte": mit_balance(p["Schritte"], KANDIDAT[kand]),
              "_Hinweis": f"{rolle} (P12 Fenster 2): {vorlage} bis W30 mit Kandidat {kand}. Erzeugt von Tests\\erzeuge_p12e.py."})
    return q


def main():
    plaene = [r2("A"), r2("B"), bis30("Referenz-Gewehr", "P12-E-R3-Gewehr-A", "A", "R3"),
              bis30("Referenz-Faul", "P12-E-R6-Faul-A", "A", "R6")]
    for p in plaene:
        json.dump(p, open(os.path.join(k.PLAENE, p["Name"] + ".json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    json.dump({"Name": "P12-E", "_Hinweis": "Fenster 2 (P12): R2 mit Kandidat A und B bis W50, R3 und R6 bis W30 mit A. "
               "Erwartung: R2 besteht W50 mit Kern-Tiefpunkt 30-60 %, R3 tiefer als R2, R6 scheitert vor W35. Erzeugt von Tests\\erzeuge_p12e.py.",
               "Plaene": [p["Name"] for p in plaene]},
              open(os.path.join(HIER, "Pakete", "P12-E.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    print(f"{len(plaene)} Plaene, Paket P12-E")


if __name__ == "__main__":
    main()
