"""FicsitTD: alle Spielmodi und Stufen in einem Lauf rechnen (Schnittstelle fuer die Pipeline). Stand 16.11.2026.

  python sim_alle_modi.py --aus <ordner>

Schreibt:
  <ordner>/sim_ergebnis.json   je Modus und Stufe Kennzahlen, ok (in den Zielbaendern), code_stand (SHA-256 je Quelldatei)
  <ordner>/sim_ergebnis.md     Kurzfassung als Tabellen
  Tools/Simulator/sim_stand.json   dieselben Hashes und das Datum (wird eingecheckt; daran erkennt die Pipeline, ob die
                                   Simulation nach einer Code-Aenderung nachgezogen wurde)
Exit 0 = alles im Band, 1 = mindestens ein Band verletzt, 2 = Fehler (Modell passt nicht zum Code),
     3 = Modell nicht gegen Spiel bestaetigt (abgleich.py fehlt, nicht bestanden oder Annahmen seitdem geaendert; 24.11.2026).
     Bei 3 sind die Baender nicht verwendbar: erst das Modell eichen, dann `python abgleich.py`.

Modi: 1 Einsatz (strategien.py), 2 Endlos-Wellen und 3 Bohrung (sim_modi.py), 0 Aus (nichts zu rechnen).
Zielbaender: Tests/zielbaender.json (Einsatz Normal, Q je Phase) und sim_baender.json (alles andere, Begruendung im README).
"""
import argparse
import collections
import datetime
import hashlib
import json
import os
import random
import re
import statistics
import sys
import time
import traceback

import simulator as sim
import strategien as st
import sim_modi as sm

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = sim.REPO
STUFEN = sim.STUFEN
QUELLEN = ["Source/FicsitTD/Private/TDBalance.cpp", "Source/FicsitTD/Private/TDWaveSubsystem.cpp",
           "Source/FicsitTD/Private/TDEinsatz.cpp", "Source/FicsitTD/Private/TDBohrung.cpp",
           "Source/FicsitTD/Private/TDTurretBase.cpp", "Source/FicsitTD/Private/TDForschungInfo.inl",
           "Source/FicsitTD/Private/TDForschungVoraussetzungen.inl", "Scripts/create_research.py"]


class ModellFehler(Exception):
    """Das Modell passt nicht mehr zum Code (Exit 2)"""


def code_stand():
    aus = {}
    for rel in QUELLEN:
        p = os.path.join(REPO, *rel.split("/"))
        if not os.path.exists(p):
            raise ModellFehler(f"Quelldatei fehlt: {rel}")
        aus[rel] = hashlib.sha256(open(p, "rb").read()).hexdigest()
    return aus


def pruefe_modell(B, A, K, katalog, phasen):
    """Gegenprobe Modell <-> Code. Jede Abweichung ist ein Fehler, kein Bandverstoss."""
    fehler = []
    info = sim.lade_forschung_info(B)
    for k, kn in K.items():
        if k not in info:
            fehler.append(f"Forschung {k}: in create_research.py, fehlt in TDForschungInfo.inl")
        elif kn["fp"] != info[k]["FP"] or kn["ab"] != info[k]["AbWelle"]:
            fehler.append(f"Forschung {k}: Modell {kn['fp']} FP ab W{kn['ab']}, Code {info[k]['FP']} FP ab W{info[k]['AbWelle']}")
    fehler += [f"Forschung {k}: nur in TDForschungInfo.inl" for k in info if k not in K]
    std, _ = sim.lade_level_standards()
    for t in st.TURM_TYPEN:
        if not any(x[0] == t for x in std):
            fehler.append(f"Turmlevel: keine Level-Pakete fuer {t} in TDTurretBase.cpp")
    pflicht = [f"Stufen.{s}.{w}" for s in STUFEN for w in ("Budget", "GegnerLP", "KernLP", "NeueFrontAbWelle", "EliteAbWelle")]
    pflicht += [f"Endlos.{s}.{w}" for s in STUFEN for w in ("ErsteWelle", "Abstand")]
    pflicht += ["Endlos.MinPause", "Endlos.UeberlaufPause", "Endlos.BossPause", "Endlos.AbstandMin", "Endlos.AbstandJeWelle",
                "Endlos.NachschubTakt", "Endlos.NachschubPaket", "Endlos.StromAnteil", "Endlos.VorziehSekundenJeCoupon",
                "Endlos.Bonus.Coupons", "Endlos.Bonus.Kern", "Endlos.BudgetFaktor", "Endlos.LPWachstum", "Endlos.LPWachstumAbWelle", "Tuerme.Level.Preis2", "Tuerme.Level.Preis5",
                "Einsatz.EP.Grund", "Einsatz.EP.JeWelle", "Einsatz.FP.JeWelle", "Wellen.LPJeWelle", "Wellen.LPJeWelleAb31"]
    pflicht += [f"Bohrung.{s}.{w}" for s in sm.BOHR_STUFEN for w in ("VonWelle", "BisWelle", "LP")]
    fehler += [f"Balance-Schluessel fehlt in TDBalance.cpp: {k}" for k in pflicht if k not in B]
    for t in st.TURM_TYPEN:
        for m in st.MUNITION[t]:
            if t != "Teslaspule" and not any(k.startswith(f"Munition.{m[1]}.") for k in B):
                fehler.append(f"Munition {m[0]}: keine Werte Munition.{m[1]}.* in TDBalance.cpp")
    if len(katalog) < 10 or len(phasen) != 5:
        fehler.append(f"Gegnerkatalog/Phasen nicht gelesen ({len(katalog)} Arten, {len(phasen)} Phasen)")
    fehlend = [e["Name"] for e in katalog if e["Name"] not in A["Gegner_GrundLP"]] + [p["Boss"] for p in phasen if p["Boss"] not in A["Gegner_GrundLP"]]
    fehler += [f"Grund-LP fehlt in annahmen.json: {n}" for n in fehlend]
    # Endlos-Standardwerte im Modell = Standard in TDEinsatz.cpp Starte
    text = open(os.path.join(sim.QUELLE, "TDEinsatz.cpp"), encoding="utf-8-sig").read()
    def feld(name):
        m = re.search(name + r"\[\] = \{([^}]*)\}", text)
        return [float(x.strip().rstrip("f")) for x in m.group(1).split(",")] if m else None
    erste, abstand = feld("ErsteStandard"), feld("AbstandStandard")
    modell = [sm.ENDLOS_WERTE[s] for s in STUFEN]
    if erste != [e for e, _ in modell] or abstand != [a for _, a in modell]:
        fehler.append(f"TDEinsatz.cpp Starte: Endlos-Standardwerte {erste}/{abstand} != sim_modi.ENDLOS_WERTE {modell}")
    if fehler:
        raise ModellFehler("; ".join(fehler))


def median(xs, std=None):
    xs = list(xs)
    return statistics.median(xs) if xs else std


def im_band(x, band):
    return x is not None and band[0] <= x <= band[1]


# ---------------------------------------------------------------- Einsatz
def rechne_einsatz(stufe, Z, baender, zb_tests, liste):
    erg, _ = st.massenlauf(stufe, 50, 1, seed=7, z_ueber={"Verlauf": 1}, strategie_liste=liste, stand="091")
    erfolg = sum(r["Erfolg"] for r in erg) / len(erg)
    w30 = sum(1 for r in erg if r["Erfolg"] or r["Welle"] > 30) / len(erg)
    q_welle = {}
    for n in range(1, 51):
        qs = [v[6] for r in erg for v in r["Verlauf"] if v[0] == n]
        q_welle[n] = round(median(qs), 3) if qs else None
    kern_min = median((r["KernMin"] for r in erg if r["Erfolg"]), 0.0)
    zbild = st.zielbild(erg, Z)
    je_linie = {l: round(q, 3) for l, q in sorted(zbild["JeLinie"].items(), key=lambda kv: -kv[1])}
    scheitern = median(r["Welle"] for r in erg if not r["Erfolg"])
    k = {"Laeufe": len(erg), "Erfolg_W50": round(erfolg, 3), "Erfolg_W30": round(w30, 3), "Scheiterwelle_Median": scheitern,
         "KernMin_Median_Erfolg": round(kern_min, 1), "Erfolg_je_Linie": je_linie,
         "Q_Nenn_Median": {w: q_welle[w] for w in (1, 5, 10, 15, 20, 25, 30, 40, 50)}}
    pruef = {}
    be = baender["Einsatz"]
    pruef["Erfolg im Band"] = (im_band(erfolg, be["Erfolg"][stufe]), f"{erfolg:.0%} (Band {be['Erfolg'][stufe][0]:.0%}–{be['Erfolg'][stufe][1]:.0%})")
    if stufe in zb_tests:
        b = zb_tests[stufe]
        treffer, gezaehlt, raus = 0, 0, []
        for n in range(1, 31):
            band = b.get(f"Phase{(n - 1) // 10 + 1}", {}).get("Q_Boss" if n % 10 == 0 else "Q")
            if band and q_welle[n] is not None:
                gezaehlt += 1
                if im_band(q_welle[n], band):
                    treffer += 1
                else:
                    raus.append(f"W{n} {q_welle[n]:.2f}")
        anteil = treffer / max(1, gezaehlt)
        k["Q_Band_Anteil_W1_30"] = round(anteil, 3)
        pruef["Q je Phase im Band (Tests/zielbaender.json)"] = (anteil >= be["Q_Band_Anteil_min"]["Wert"],
                                                               f"{anteil:.0%} der Wellen im Band (Ziel ≥ {be['Q_Band_Anteil_min']['Wert']:.0%}); ausserhalb: {', '.join(raus[:8]) or '–'}")
        pruef["Kern-Tiefpunkt (Median Erfolgreiche)"] = (kern_min >= be["KernMin_Median_min"]["Wert"], f"{kern_min:.0f} % (Ziel ≥ {be['KernMin_Median_min']['Wert']} %)")
        pruef["Zielbild: mehrere Wege"] = (zbild["Ziel_Wege"], f"{len(zbild['Tragfaehig'])} Linien ≥ {Z['ZielLinieQuote']:.0%}")
        pruef["Zielbild: keine Linie dominiert"] = (zbild["Ziel_KeineDominanz"], ", ".join(zbild["Dominant"]) or "–")
        pruef["Zielbild: nicht zu leicht"] = (zbild["Ziel_NichtZuLeicht"], f"{zbild['ZuLeichtAnteil']:.0%} Siege mit Kern nie unter {Z['ZuLeichtKern']} %")
        pruef["Zielbild: keine tote Kombination"] = (zbild["Ziel_KeineToteLinie"], ", ".join(zbild["Tot"]) or "–")
    return k, pruef, q_welle


# ---------------------------------------------------------------- Endlos
def rechne_endlos(daten, stufe, baender, max_wellen):
    # 23.11. (Gegencheck 0.13): 3 Laeufe je Linie statt 1 - Endlos streute um 1-2 Wellen je Lauf
    erg = sm.endlos_lauf(daten, stufe, max_wellen=max_wellen, laeufe=3)
    ohne = [r for r in erg if not r["Vorziehen"]]
    mit = [r for r in erg if r["Vorziehen"]]
    w_med = median(r["Welle"] for r in ohne)
    kapp = sum(r["Kappung"] for r in erg) / len(erg)
    q_welle, lebend = {}, {}
    for n in range(1, max_wellen + 1):
        qs = [v["Q_Nenn"] for r in ohne for v in r["Verlauf"] if v["Welle"] == n]
        q_welle[n] = round(median(qs), 3) if qs else None
        lebend[n] = sum(1 for r in ohne if r["Welle"] >= n) / len(ohne)
    # Kipp-Welle: ab hier hat weniger als die Haelfte der Linien die Welle geschafft
    kipp = next((n for n in range(1, max_wellen + 1) if lebend[n] < 0.5), None)
    ueber = median((next((v["Welle"] for v in r["Verlauf"] if v["Ueberlauf"]), None) for r in ohne
                    if any(v["Ueberlauf"] for v in r["Verlauf"])), None)
    je_linie = {r["Linie"]: r["Welle"] for r in sorted(ohne, key=lambda r: -r["Welle"])}
    k = {"Laeufe": len(erg), "Welle_Median": w_med, "Welle_Median_Vorziehen": median(r["Welle"] for r in mit),
         "Welle_beste": max(r["Welle"] for r in erg), "Kipp_Welle": kipp, "Erster_Ueberlauf_Median": ueber,
         "Kappung_Anteil": round(kapp, 3), "Rechengrenze": max_wellen, "Welle_je_Linie": je_linie,
         "Q_Nenn_Median": {w: q_welle.get(w) for w in (1, 5, 10, 20, 30, 40, 50, 60, 80, 100) if w <= max_wellen}}
    be = baender["Endlos"]
    band = be["Welle_Median"][stufe]
    pruef = {"Erreichte Welle (Median) im Band": (im_band(w_med, band), f"W{w_med} (Band W{band[0]}–W{band[1]})"),
             "Modus kippt (Kappung)": (kapp <= be["Kappung_Anteil_max"]["Wert"], f"{kapp:.0%} der Läufe erreichen W{max_wellen} (Ziel ≤ {be['Kappung_Anteil_max']['Wert']:.0%})")}
    return k, pruef, q_welle


# ---------------------------------------------------------------- Bohrung
def rechne_bohrung(daten, stufe, baender):
    B = daten[0]
    aufbau = sm.bohrung_lauf(daten, stufe, laeufe=4)
    von, bis, lp = sm.bohr_werte(B, stufe)
    bolzen2 = next(a["Mindestzahl"] for a in aufbau if a["Typ"] == "Bolzenturm" and a["Level"] == 2)
    beste = min((a for a in aufbau if a["Mindestzahl"]), key=lambda a: a["Mindestzahl"], default=None)
    k = {"Wellen": f"{von}-{bis}", "Kern_LP": lp, "Erz_je_Welle": B.get(f"Bohrung.{stufe}.ErzJeWelle"),
         "Erstlohn_Festplatten": B.get(f"Bohrung.{stufe}.Festplatten"),
         "Mindestaufbau": {f"{a['Typ']} L{a['Level']} ({a['Munition']})": a["Mindestzahl"] for a in aufbau},
         "Bolzen_L2_Mindestzahl": bolzen2, "Bester_Aufbau": f"{beste['Mindestzahl']} x {beste['Typ']} L{beste['Level']}" if beste else None}
    bb = baender["Bohrung"]
    band = bb["Bolzen_L2_Mindestzahl"][stufe]
    pruef = {"Bolzenturm L2: Mindestzahl im Band": (im_band(bolzen2, band), f"{bolzen2 if bolzen2 else '> 60'} (Band {band[0]}–{band[1]})"),
             "Stufe nicht trivial": (beste is not None and beste["Mindestzahl"] >= bb["Beste_Mindestzahl_min"][stufe],
                                     f"bester Aufbau {k['Bester_Aufbau']} (Ziel ≥ {bb['Beste_Mindestzahl_min'][stufe]} Türme)")}
    return k, pruef


# ---------------------------------------------------------------- Ausgabe
def markdown(res, stand_datum):
    z = [f"# FicsitTD Simulation aller Modi ({stand_datum})", "",
         f"Ergebnis: **{'alles im Band' if res['ok'] else 'Bänder verletzt'}**, Laufzeit {res['laufzeit_s']:.0f} s. "
         "Modell und Grenzen: `Tools/Simulator/README.md`.", ""]
    if not res.get("modell_bestaetigt", True):
        z += [f"> **Modell nicht gegen Spiel bestätigt:** {res.get('abgleich')}. Die Bänder unten sind für die Balance **nicht verwendbar**.", ""]
    z += ["## Einsatz (Modus 1, 50 Wellen)", "", "| Stufe | Läufe | Erfolg W50 | Erfolg W30 | Scheitern (Median) | Kern-Tief | Q W10 | Q W20 | Q W30 | ok |",
          "|---|---|---|---|---|---|---|---|---|---|"]
    for s, e in res["modi"]["Einsatz"].items():
        k, q = e["kennzahlen"], e["kennzahlen"]["Q_Nenn_Median"]
        f = lambda x: f"{x:.2f}" if x is not None else "–"
        z.append(f"| {s} | {k['Laeufe']} | {k['Erfolg_W50']:.0%} | {k['Erfolg_W30']:.0%} | {k['Scheiterwelle_Median'] or '–'} | {k['KernMin_Median_Erfolg']:.0f} % | "
                 f"{f(q[10])} | {f(q[20])} | {f(q[30])} | {'ja' if e['ok'] else '**nein**'} |")
    if res.get("aktiver_spieler"):
        z += ["", "### Einsatz mit aktivem Spieler (Handwaffe und Talente, ohne Band: die Balance gilt für den passiven Spieler)", "",
              "| Stufe | Erfolg W50 | Spieleranteil Median | 10–90 % |", "|---|---|---|---|"]
        for s, e in res["aktiver_spieler"].items():
            z.append(f"| {s} | {e['Erfolg_W50']:.0%} | {e['Anteil_Median']:.0%} | {e['Anteil_P10']:.0%}–{e['Anteil_P90']:.0%} |")
    z += ["", "## Endlos-Wellen (Modus 2)", "", "| Stufe | Welle (Median) | mit Vorziehen | beste | kippt ab | erster Überlauf | Kappung | Q W10 | Q W20 | ok |",
          "|---|---|---|---|---|---|---|---|---|---|"]
    for s, e in res["modi"]["Endlos"].items():
        k, q = e["kennzahlen"], e["kennzahlen"]["Q_Nenn_Median"]
        f = lambda x: f"{x:.2f}" if x is not None else "–"
        z.append(f"| {s} | {k['Welle_Median']} | {k['Welle_Median_Vorziehen']} | {k['Welle_beste']} | {k['Kipp_Welle'] or '–'} | {k['Erster_Ueberlauf_Median'] or '–'} | "
                 f"{k['Kappung_Anteil']:.0%} | {f(q.get(10))} | {f(q.get(20))} | {'ja' if e['ok'] else '**nein**'} |")
    z += ["", "## Bohrung (Modus 3, Freies Spiel mit Abwehr): Mindestzahl Türme für 80 % Erfolg", ""]
    stufen = list(res["modi"]["Bohrung"])
    aufbauten = list(res["modi"]["Bohrung"][stufen[0]]["kennzahlen"]["Mindestaufbau"])
    z += ["| Aufbau | " + " | ".join(f"{s} (W{res['modi']['Bohrung'][s]['kennzahlen']['Wellen']})" for s in stufen) + " |", "|---|" + "---|" * len(stufen)]
    for a in aufbauten:
        z.append(f"| {a} | " + " | ".join(str(res["modi"]["Bohrung"][s]["kennzahlen"]["Mindestaufbau"][a] or "> 60") for s in stufen) + " |")
    z.append("| ok | " + " | ".join("ja" if res["modi"]["Bohrung"][s]["ok"] else "**nein**" for s in stufen) + " |")
    z += ["", "## Modus Aus", "", "Nichts zu simulieren.", "", "## Bandverletzungen", ""]
    verl = [(m, s, name, d) for m, ms in res["modi"].items() if isinstance(ms, dict) for s, e in ms.items() if isinstance(e, dict) and "pruefungen" in e
            for name, (ok, d) in e["pruefungen"].items() if not ok]
    z += [f"- {m} {s}: {name} – {d}" for m, s, name, d in verl] or ["keine"]
    return "\n".join(z) + "\n"


def abgleich_status():
    """24.11.2026 (Marius: nicht mehr nach einem unbestaetigten Modell einstellen): Stand des Abgleichs Modell gegen Spiel"""
    import abgleich
    if not os.path.exists(abgleich.STAND):
        return False, "kein Abgleich (python abgleich.py nach einem Testlauf der festen Plaene)"
    s = json.load(open(abgleich.STAND, encoding="utf-8"))
    if s.get("annahmen_hash") != abgleich.annahmen_hash():
        return False, f"Modell seit dem Abgleich vom {s.get('datum')} geaendert, Abgleich neu noetig"
    if not s.get("bestanden"):
        return False, f"Abgleich vom {s.get('datum')} nicht bestanden ({s.get('laeufe')} Laeufe, Ausgabe/Abgleich/abgleich.md)"
    return True, f"Abgleich vom {s.get('datum')} bestanden ({s.get('laeufe')} Laeufe)"


def main():
    ap = argparse.ArgumentParser(description="FicsitTD: alle Modi simulieren (Pipeline)")
    ap.add_argument("--aus", required=True, help="Ausgabeordner fuer sim_ergebnis.json/.md")
    ap.add_argument("--endlos-max", type=int, default=150, help="Rechengrenze Endlos-Wellen (Standard 150; 16.11.: LP-Wachstum ab W50 braucht Platz)")
    a = ap.parse_args()
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")   # Windows-Konsole (cp1252)
    except Exception:
        pass
    t0 = time.time()
    try:
        stand = code_stand()
        B, A, Z, katalog, phasen, K, gabeln, quelle = st.lade_alles(stand="091")
        pruefe_modell(B, A, K, katalog, phasen)
        baender = json.load(open(os.path.join(HIER, "sim_baender.json"), encoding="utf-8"))
        zb_tests = json.load(open(os.path.join(REPO, "Tests", "zielbaender.json"), encoding="utf-8"))
        daten = (B, A, Z, katalog, phasen, K, gabeln)
        liste = st.strategien(random.Random(7))[::2]
        res = {"stand": datetime.datetime.now().strftime("%Y-%m-%d %H:%M"), "balance": quelle, "code_stand": stand, "modi": {}}
        modi = res["modi"]
        modi["Einsatz"], modi["Endlos"], modi["Bohrung"] = {}, {}, {}
        for s in STUFEN:
            k, p, _ = rechne_einsatz(s, Z, baender, zb_tests, liste)
            modi["Einsatz"][s] = {"kennzahlen": k, "pruefungen": p, "ok": all(x[0] for x in p.values())}
        for s in STUFEN:
            k, p, _ = rechne_endlos(daten, s, baender, a.endlos_max)
            modi["Endlos"][s] = {"kennzahlen": k, "pruefungen": p, "ok": all(x[0] for x in p.values())}
        for s in sm.BOHR_STUFEN:
            k, p = rechne_bohrung(daten, s, baender)
            modi["Bohrung"][s] = {"kennzahlen": k, "pruefungen": p, "ok": all(x[0] for x in p.values())}
        # 23.11.2026 (Marius: Talentbaum in den Simulator): aktiver Spieler mit Handwaffe und Talenten als Zusatz, ohne Band -
        # die Balance gilt fuer den passiven Spieler (Marius 12.11.); Ziel Spieleranteil 10-20 %
        res["aktiver_spieler"] = {}
        for s in STUFEN:
            if s == "Erkundung":
                continue
            erg, _ = st.massenlauf(s, 50, 1, seed=7, z_ueber={"SpielerAktiv": 1}, strategie_liste=liste, stand="091")
            ant = sorted(r.get("SpielerAnteil", 0.0) for r in erg)
            res["aktiver_spieler"][s] = {"Erfolg_W50": round(sum(r["Erfolg"] for r in erg) / len(erg), 3),
                                         "Anteil_Median": ant[len(ant) // 2], "Anteil_P10": ant[len(ant) // 10], "Anteil_P90": ant[len(ant) * 9 // 10]}
        modi["Aus"] = {"ok": True, "hinweis": "Spielmodus 0: normales Satisfactory, nichts zu simulieren"}
        res["ok"] = all(e["ok"] for m in ("Einsatz", "Endlos", "Bohrung") for e in modi[m].values())
        res["modell_bestaetigt"], res["abgleich"] = abgleich_status()
        res["laufzeit_s"] = round(time.time() - t0, 1)
    except Exception as e:
        os.makedirs(a.aus, exist_ok=True)
        fehler = {"ok": False, "fehler": f"{type(e).__name__}: {e}", "trace": traceback.format_exc()}
        json.dump(fehler, open(os.path.join(a.aus, "sim_ergebnis.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
        print("FEHLER (Modell passt nicht zum Code):", fehler["fehler"], file=sys.stderr)
        return 2
    os.makedirs(a.aus, exist_ok=True)
    # Tupel (ok, Text) als Objekt schreiben
    for m in ("Einsatz", "Endlos", "Bohrung"):
        for e in res["modi"][m].values():
            e["pruefungen"] = {n: {"ok": ok, "wert": d} for n, (ok, d) in e["pruefungen"].items()}
    json.dump(res, open(os.path.join(a.aus, "sim_ergebnis.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    for m in ("Einsatz", "Endlos", "Bohrung"):
        for e in res["modi"][m].values():
            e["pruefungen"] = {n: (v["ok"], v["wert"]) for n, v in e["pruefungen"].items()}
    open(os.path.join(a.aus, "sim_ergebnis.md"), "w", encoding="utf-8").write(markdown(res, res["stand"]))
    json.dump({"datum": res["stand"], "hinweis": "SHA-256 der Quelldateien, gegen die sim_alle_modi.py zuletzt gerechnet hat",
               "code_stand": res["code_stand"]}, open(os.path.join(HIER, "sim_stand.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    print(open(os.path.join(a.aus, "sim_ergebnis.md"), encoding="utf-8").read())
    if not res.get("modell_bestaetigt"):
        print("MODELL NICHT BESTAETIGT:", res.get("abgleich"))
        return 3
    return 0 if res["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
