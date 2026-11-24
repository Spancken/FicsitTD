"""Abgleich Modell gegen Spiel - Pflichtprüfung, bevor Simulator-Ergebnisse die Balance bestimmen (24.11.2026).

Anlass: Am 24.11. schlug der Simulator 40 Start-Coupons mit 63 % mehr Gegnerleben vor. Im Spiel fielen damit alle Läufe
in W6, weil das Modell die frühen Wellen um den Faktor 2 zu günstig rechnete. Seitdem gilt (Marius 24.11.): Der Simulator
stellt die Balance nur ein, wenn er die Testläufe im Spiel richtig vorhersagt.

Ablauf je Lauf eines festen Testplans (Strat-*, P12-K*), gespielt nach der letzten Änderung seiner Plandatei:
  1. Ausbau, Forschung und Balance-Overrides aus der Plandatei (messungen.plan_ausbau)
  2. Modell spielt genau diesen Plan auf 40 Wellen-Zufällen (strategie_plaene.PlanLauf), Median des Kerns je Welle
  3. Vergleich mit dem Spiel: Fallwelle und mittlere Abweichung des Kerns (Prozentpunkte)
Bestanden je Plan: Fallwelle höchstens GRENZE_FALL Wellen daneben und Kern im Mittel höchstens GRENZE_KERN Punkte.
Bestanden gesamt: mindestens MIN_LAEUFE Läufe und jeder Plan bestanden.

Ergebnis: Ausgabe/Abgleich/abgleich.md und abgleich_stand.json (einchecken). sim_alle_modi.py liest abgleich_stand.json
und meldet "Modell nicht bestätigt", solange der Abgleich fehlt, durchgefallen ist oder die Modellannahmen seitdem geändert wurden.

  python abgleich.py [--ab "2026-11-24 06:00"] [--laeufe 40]
Exit 0 bestanden, 1 nicht bestanden, 2 zu wenige Läufe.
"""
import argparse
import datetime
import hashlib
import json
import os
import random
import statistics
import sys

import messungen as me
import simulator as sim
import strategie_plaene as sp
import strategien as st

HIER = os.path.dirname(os.path.abspath(__file__))
AUS = os.path.join(HIER, "Ausgabe", "Abgleich")
STAND = os.path.join(HIER, "abgleich_stand.json")
ANNAHMEN = [os.path.join(HIER, n) for n in ("strategie_annahmen.json", "annahmen.json")]
GRENZE_FALL = 2      # Wellen
GRENZE_KERN = 15.0   # Prozentpunkte, Mittel über die Wellen des Laufs
MIN_LAEUFE = 3


def annahmen_hash():
    h = hashlib.sha256()
    for p in ANNAHMEN + [os.path.join(HIER, "strategien.py"), os.path.join(HIER, "simulator.py")]:
        if os.path.exists(p):
            h.update(open(p, "rb").read())
    return h.hexdigest()


def plan_strategie(name):
    """Doktrin und Gabeln der Quellstrategie für Strat-Pläne, sonst None (FesterLauf-Standard)"""
    ablage = os.path.join(sp.bk.AUS, "strategie_plaene.json")
    if name.startswith("Strat-") and os.path.exists(ablage):
        e = json.load(open(ablage, encoding="utf-8")).get(name)
        if e is not None:
            return st.strategien(random.Random(7))[e["Strategie"]]
    return None


def modell_kurve(name, stufe, laeufe=40):
    """Median-Kern (%) nach jeder Welle und Median-Fallwelle (bis + 1 = überlebt) des Modells für diesen Plan"""
    pa = me.plan_ausbau(name)
    if not pa or not pa["bis"]:
        return None
    B, A, Z, katalog, phasen, K, gabeln, _ = st.lade_alles(pa["balance"])
    s = plan_strategie(name)
    wellen = sim.Wellen(B, katalog, phasen, A["Gegner_GrundLP"], stufe)
    kurven, ende = [], []
    for i in range(laeufe):
        rng_w = random.Random(7919 + i)
        if s is not None:
            L = sp.PlanLauf(s, pa["ausbau"], pa["forschung"], B, Z, K, gabeln, st.Turmwerte(B), st.Wirtschaft(B, Z), stufe)
        else:
            L = sp.bk.FesterLauf(pa["ausbau"], pa["forschung"], B, Z, K, gabeln, st.Turmwerte(B), st.Wirtschaft(B, Z), stufe)
        rng = random.Random(i)
        k, fall = [], pa["bis"] + 1
        for n in range(1, pa["bis"] + 1):
            D = Z["WellenGrund"] + Z["WellenJe"] * (n - 1)
            zyklus = st.zyklus_min(Z, n, D)
            L.forsche(n, gabeln)
            L.baue(n, zyklus, D)
            L.verbrenne(n, zyklus, D)
            gegner, elite = wellen.erzeuge(n, rng_w)
            L.kampf(gegner, elite, n, rng)
            k.append(max(0.0, L.kern))
            if L.kern <= 0:
                fall = n
                k += [0.0] * (pa["bis"] - n)
                break
            L.belohnung(n, len(gegner), elite, n % 10 == 0)
        kurven.append(k)
        ende.append(fall)
    med = [statistics.median(c[n] for c in kurven) for n in range(pa["bis"])]
    return {"kern": med, "fall": statistics.median(ende), "bis": pa["bis"]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ab", default="", help="nur Läufe ab dieser lokalen Zeit (Metriken.csv); ohne: ab der letzten Änderung von TDBalance.cpp")
    ap.add_argument("--laeufe", type=int, default=40, help="Wellen-Zufälle je Modellplan")
    a = ap.parse_args()
    # Nur Läufe mit der heutigen Balance: ab der letzten Änderung von TDBalance.cpp (sonst rechnet das Modell andere Werte als das Spiel)
    balance = os.path.join(me.REPO, "Source", "FicsitTD", "Private", "TDBalance.cpp")
    # --ab ersetzt diese Regel (z. B. wenn TDBalance.cpp nur an Kommentaren oder Schaltern mit Standard 0 geaendert wurde)
    ab = a.ab or datetime.datetime.fromtimestamp(os.path.getmtime(balance)).strftime("%Y-%m-%d %H:%M:%S")
    alle = me.laeufe(me.lade_wellen(ab))
    zeilen, ergebnisse, cache = [], [], {}
    for l in alle:
        pfad = os.path.join(me.PLAENE, l["plan"] + ".json")
        if not os.path.exists(pfad):
            continue
        geaendert = datetime.datetime.fromtimestamp(os.path.getmtime(pfad)).strftime("%Y-%m-%d %H:%M:%S")
        if l["start"] < geaendert:
            continue   # gespielt mit einer älteren Fassung des Plans
        if not any(w["tuerme"] > 0 for w in l["wellen"]):
            continue   # Aufbau fehlgeschlagen (keine Türme), kein Modellfall
        if l["wellen"][0]["welle"] != 1:
            continue   # Plan startet nicht bei Welle 1 (Sprung), Kernverlauf nicht vergleichbar
        stufe = me.STUFEN.get(l["stufe"], "Normal")
        schl = (l["plan"], stufe)
        if schl not in cache:
            cache[schl] = modell_kurve(l["plan"], stufe, a.laeufe)
        m = cache[schl]
        if m is None:
            continue
        spiel_fall = l["fallwelle"] if l["gefallen"] else (m["bis"] + 1 if l["wellen"][-1]["welle"] >= m["bis"] else None)
        if spiel_fall is None:
            continue   # abgebrochen, weder gefallen noch durchgespielt
        abw = [abs(w["kern"] - m["kern"][w["welle"] - 1]) for w in l["wellen"] if w["welle"] <= m["bis"]]
        e = {"plan": l["plan"], "start": l["start"], "stufe": stufe, "spiel_fall": spiel_fall, "modell_fall": m["fall"],
             "kern_abw": statistics.mean(abw) if abw else 0.0,
             "spiel_kern": [round(w["kern"]) for w in l["wellen"]], "modell_kern": [round(x) for x in m["kern"][:len(l["wellen"])]]}
        e["ok"] = abs(e["spiel_fall"] - e["modell_fall"]) <= GRENZE_FALL and e["kern_abw"] <= GRENZE_KERN
        ergebnisse.append(e)
    je_plan = {}
    for e in ergebnisse:
        je_plan.setdefault(e["plan"], []).append(e)
    plan_ok = {}
    for name, es in je_plan.items():
        df = statistics.median(e["spiel_fall"] for e in es) - statistics.median(e["modell_fall"] for e in es)
        ka = statistics.median(e["kern_abw"] for e in es)
        plan_ok[name] = (abs(df) <= GRENZE_FALL and ka <= GRENZE_KERN, df, ka)
    genug = len(ergebnisse) >= MIN_LAEUFE
    bestanden = genug and all(v[0] for v in plan_ok.values())
    jetzt = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    zeilen = [f"# Abgleich Modell gegen Spiel ({jetzt})", "",
              f"Ergebnis: **{'bestanden' if bestanden else 'nicht bestanden' if genug else 'zu wenige Läufe'}** "
              f"({len(ergebnisse)} Läufe ab {ab}, Grenzen: Fallwelle ±{GRENZE_FALL}, Kern im Mittel ±{GRENZE_KERN:.0f} Punkte).", "",
              "| Plan | Läufe | Fallwelle Spiel | Fallwelle Modell | Kern-Abweichung (Median) | ok |", "|---|---|---|---|---|---|"]
    for name, es in je_plan.items():
        ok, df, ka = plan_ok[name]
        zeilen.append(f"| {name} | {len(es)} | {', '.join(str(e['spiel_fall']) for e in es)} | {es[0]['modell_fall']:g} | {ka:.0f} | {'ja' if ok else '**nein**'} |")
    zeilen += ["", "## Kern je Welle (Spiel / Modell, %)", ""]
    for e in ergebnisse:
        zeilen.append(f"- {e['plan']} ({e['start']}): " + " ".join(f"W{i + 1} {s}/{m}" for i, (s, m) in enumerate(zip(e["spiel_kern"], e["modell_kern"]))))
    zeilen += ["", "Bestanden heißt: Die Bänder von sim_alle_modi.py sind für die Balance verwendbar. Nicht bestanden: erst das Modell eichen."]
    os.makedirs(AUS, exist_ok=True)
    open(os.path.join(AUS, "abgleich.md"), "w", encoding="utf-8").write("\n".join(zeilen) + "\n")
    json.dump({"datum": jetzt, "bestanden": bestanden, "laeufe": len(ergebnisse), "annahmen_hash": annahmen_hash(),
               "hinweis": "abgleich.py; sim_alle_modi.py meldet 'Modell nicht bestaetigt', solange bestanden false ist oder der Hash nicht passt",
               "plaene": {n: {"ok": v[0], "fallwelle_differenz": v[1], "kern_abweichung": round(v[2], 1)} for n, v in plan_ok.items()}},
              open(STAND, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    print("\n".join(zeilen))
    return 0 if bestanden else 1 if genug else 2


if __name__ == "__main__":
    sys.exit(main())
