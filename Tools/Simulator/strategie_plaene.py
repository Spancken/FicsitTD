"""Spielpläne aus den besten Modellstrategien je Linie (Balance-Kreislauf, Testplan 3.3).

Die festen Kreislauf-Pläne bauen 2 Türme je Abschnitt, die Strategien des Modells etwa 20. Dieses Werkzeug
1. sucht je Linie die Strategie mit der höchsten W30-Quote im Modell,
2. zeichnet ihren Ausbau auf (Türme, Level, Munition, Forschung) und verdichtet ihn auf die Abschnitte des Selbsttests
   (0, 4, 8, 12, 16, 20, 25: dort baut und forscht der Plan; der Selbsttest kann nur neu bauen, nicht aufwerten),
3. spielt genau diesen Plan im Modell nach (Vorhersage) und schreibt Spielplan und Paket `Strategie`.

    python strategie_plaene.py              # Pläne + Paket, 2 Wiederholungen
    python strategie_plaene.py --nur-modell # nur Auswahl und Vorhersage, nichts schreiben
"""
import argparse
import collections
import json
import os
import random
import statistics

import balance_kreislauf as bk
import simulator as sim
import strategien as st

ABSCHNITTE = [0, 4, 8, 12, 16, 20, 25]
BIS = 30
REZEPT = {"Armierungsbolzen": "Recipe_TD_Shoot_SpikedRebar", "Durchschlagbolzen": "Recipe_TD_Shoot_Durchschlag",
          "Explosivbolzen": "Recipe_TD_Shoot_Explosive", "Streubolzen": "Recipe_TD_Shoot_Spreadshot", "Schockbolzen": "Recipe_TD_Shoot_Stunshot",
          "Standard": "Recipe_TD_Gun_Standard", "Zielsuch": "Recipe_TD_Gun_Homing", "Panzerbrechend": "Recipe_TD_Gun_Panzer",
          "Brand": "Recipe_TD_Gun_Brand", "Turbo": "Recipe_TD_Gun_Turbo",
          "Nobelisk": "Recipe_TD_Nobelisk_Explosive", "Gas": "Recipe_TD_Nobelisk_Gas", "Puls": "Recipe_TD_Nobelisk_Pulse", "Streu": "Recipe_TD_Nobelisk_Cluster",
          "Entladung": "Recipe_TD_Tesla_Discharge",
          "Biotreibstoff": "Recipe_TD_Flame_Biofuel", "Treibstoff": "Recipe_TD_Flame_Fuel", "Turbotreibstoff": "Recipe_TD_Flame_Turbo"}
# Bauplätze des Selbsttests (TDAutotest.cpp "baue"): 16 außen (16 m), 8 innen (8 m, für Flammenwerfer)
PLAETZE = {"aussen": 16, "innen": 8}
VORZIEHEN = False


def lade(p):
    """Planliste eines bestehenden Pakets (einmal, ohne Wiederholung)"""
    try:
        return list(dict.fromkeys(json.load(open(p, encoding="utf-8"))["Plaene"]))
    except OSError:
        return []


def ring(typ):
    return "innen" if typ == "Flammenwerfer" else "aussen"


def wellen_listen(B, katalog, phasen, A, laeufe, seed=7):
    wellen = sim.Wellen(B, katalog, phasen, A["Gegner_GrundLP"], "Normal")
    out = []
    for s_i in range(laeufe):
        rng = random.Random(seed * 1009 + s_i)
        out.append([wellen.erzeuge(n, rng) for n in range(1, BIS + 1)])
    return out


def aufzeichnen(s, wl, B, Z, K, gabeln, tw, wi, rng):
    """Strategie wie st.spiele laufen lassen; Zustand jeweils nach Forschung und Bau vor Welle abschnitt+1"""
    L = st.Lauf(s, B, Z, K, gabeln, tw, wi, "Normal")
    aufbau, pause = B["Stufen.Normal.AufbauSekunden"], B["Stufen.Normal.PauseSekunden"]
    stand = {}
    for n in range(1, BIS + 1):
        D = Z["WellenGrund"] + Z["WellenJe"] * (n - 1)
        zyklus = st.zyklus_min(Z, n, D)
        L.forsche(n, gabeln)
        L.baue(n, zyklus, D)
        L.verbrenne(n, zyklus, D)
        # Zustand je Abschnitt: VORZIEHEN=False -> vor der ersten Welle des Abschnitts, True -> vor seiner letzten Welle
        # (die Strategie baut auch zwischen den Abschnitten, der Selbsttest nur an ihnen)
        ziel = [x for x in ABSCHNITTE if (x == n - 1 if not VORZIEHEN else n - 1 == min([y for y in ABSCHNITTE[1:] + [BIS] if y > x]) - 1)]
        for x in ziel:
            stand[x] = {"tuerme": [list(t) for t in L.tuerme], "erforscht": set(L.erforscht),
                            "mun": {t: L.beste_munition_fuer_wirtschaft(t)[0] for t in {t for t, _ in L.tuerme}}}
        gegner, elite = wl[n - 1]
        L.kampf(gegner, elite, n, rng)
        if L.kern <= 0:
            return stand, False
        L.belohnung(n, len(gegner), elite, n % 10 == 0)
    return stand, True


def verdichten(stand):
    """Abschnitts-Zustände -> Ausbau {abschnitt: [(typ, anzahl, level, rezept, ring)]} und Forschung {abschnitt: [knoten]}.
    Neue Türme je Typ entstehen auf dem höchsten Level, das der Typ im Zustand hat; vorhandene werden mit Anzahl 0 (Selbsttest "aufwerten", 0.84) nachgezogen."""
    ausbau, forschung, gebaut, erforscht = {}, {}, collections.Counter(), set()
    belegt = collections.Counter()
    stufe, munition = {}, {}   # 0.84: Level und Munition der schon gebauten Tuerme je Typ
    for a in ABSCHNITTE:
        if a not in stand:
            break
        z = stand[a]
        neu = sorted(z["erforscht"] - erforscht)
        if neu:
            forschung[a] = neu
        erforscht |= z["erforscht"]
        anzahl = collections.Counter(t for t, _ in z["tuerme"])
        for typ in sorted(anzahl):
            lv_jetzt = max(lv for t, lv in z["tuerme"] if t == typ)
            rez_jetzt = REZEPT[z["mun"][typ]]
            if gebaut[typ] and (lv_jetzt > stufe.get(typ, 1) or rez_jetzt != munition.get(typ)):
                ausbau.setdefault(a, []).append((typ, 0, lv_jetzt, rez_jetzt, ring(typ)))
            stufe[typ], munition[typ] = max(stufe.get(typ, 1), lv_jetzt), rez_jetzt
            fehl = anzahl[typ] - gebaut[typ]
            frei = PLAETZE[ring(typ)] - belegt[ring(typ)]
            fehl = min(fehl, frei)
            if fehl <= 0:
                continue
            lv = max(lv for t, lv in z["tuerme"] if t == typ)
            ausbau.setdefault(a, []).append((typ, fehl, lv, REZEPT[z["mun"][typ]], ring(typ)))
            gebaut[typ] += fehl
            belegt[ring(typ)] += fehl
    return ausbau, forschung


class PlanLauf(bk.FesterLauf):
    """Fester Plan mit der Forschung je Abschnitt und Doktrin/Gabel der Quellstrategie"""
    def __init__(self, s, ausbau, forschung, *a, **k):
        st.Lauf.__init__(self, s, *a, **k)
        self.ausbau, self.feste_forschung, self.mun, self.tuerme = ausbau, forschung, {}, []

    def forsche(self, n, gabeln):
        for a, knoten in self.feste_forschung.items():
            if a <= n - 1:
                self.erforscht.update(knoten)


def nachspielen(s, ausbau, forschung, B, Z, K, gabeln, listen):
    ende = []
    for i, wl in enumerate(listen):
        L = PlanLauf(s, {a: [e[:4] for e in v] for a, v in ausbau.items()}, forschung, B, Z, K, gabeln, st.Turmwerte(B), st.Wirtschaft(B, Z), "Normal")
        rng = random.Random(i)
        aufbau, pause = B["Stufen.Normal.AufbauSekunden"], B["Stufen.Normal.PauseSekunden"]
        n_ende = BIS + 1
        for n in range(1, BIS + 1):
            D = Z["WellenGrund"] + Z["WellenJe"] * (n - 1)
            zyklus = st.zyklus_min(Z, n, D)
            L.forsche(n, gabeln)
            L.baue(n, zyklus, D)
            L.verbrenne(n, zyklus, D)
            gegner, elite = wl[n - 1]
            L.kampf(gegner, elite, n, rng)
            if L.kern <= 0:
                n_ende = n
                break
            L.belohnung(n, len(gegner), elite, n % 10 == 0)
        ende.append(n_ende)
    return ende


def spielplan(name, hinweis, ausbau, forschung, stufe="normal"):
    # 17.11. (P12-Bericht: Hart/Albtraum-Plaene starteten auf Normal): die Stufe des Plans auch im Startbefehl
    import erzeuge_referenzen as er
    s = [{"pruefe": "Version", "soll": "{VERSION}", "text": "richtige Mod-Version geladen"}, {"warte": 15},
         {"befehl": "td einsatz stop"}, {"befehl": "td kern stop"}, {"befehl": "td einsatz start %s 50" % stufe}, {"befehl": "td einsatz frei an"},
         {"stromfrei": True}, {"versorgung": er.SATT}]
    for a in ABSCHNITTE + [BIS]:
        if a > 0:
            s.append({"wellenlauf": a, "zeitraffer": 4, "max": 500 if a <= 30 else 1200})
            if a >= BIS:
                break
        s += [{"erforsche": k} for k in forschung.get(a, [])]
        for typ, anz, lv, rez, rg in ausbau.get(a, []):
            if anz == 0:
                s.append({"aufwerten": typ, "level": lv, "rezept": rez})
            else:
                s.append({"baue": typ, "anzahl": anz, "level": lv, "rezept": rez, **({"ring": "innen"} if rg == "innen" else {})})
        if a in (0, 16):
            s += [{"ueberblick": True}, {"foto": f"{name}_W{a:02d}"}]
    s += [{"ueberblick": True}, {"foto": f"{name}_W{BIS}"},
          {"pruefe": "KernLebt", "text": f"Kern hat Welle {BIS} überstanden"},
          {"pruefe": "Sicherheitsnetze", "text": "kein Sicherheitsnetz ausgelöst (Versetzen, Absturz, 0 LP)"},
          {"pruefe": "KeineWarnungen", "text": "keine Warnungen oder Fehler des Mods im Log"}]
    return {"Name": name, "_Hinweis": hinweis + " Erzeugt von Werkzeuge\\FicsitTD-Simulator\\strategie_plaene.py.",
            "NeuesSpiel": {"Startort": "Rocky Desert"}, "ErweiterteEinstellungen": False, "WarteNachLaden": 60, "MaxMinuten": 45 if BIS <= 30 else 130, "Schritte": s}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--laeufe", type=int, default=20, help="Wellen-Zufälle für die Auswahl")
    ap.add_argument("--wiederholungen", type=int, default=2)
    ap.add_argument("--nur-modell", action="store_true")
    ap.add_argument("--vorziehen", action="store_true", help="Türme eines Abschnitts schon an seinem Anfang bauen")
    ap.add_argument("--nur", default="", help="nur Linien mit diesem Anfang, z. B. Mono")
    ap.add_argument("--bis", type=int, default=30, help="letzte Welle (50 = ganzer Einsatz, Abschnitte bis W45)")
    # 0.83.1 (Gesamt082: Mono-Pläne bis W30 hatten in W1-4 nur einen Turm, der Kern fiel bis W4 auf 10 %): die Strategie kauft
    # in W1-3 nach, der Plan konnte das erst an W4 - frühe Abschnitte einzeln abbilden
    ap.add_argument("--grob", action="store_true", help="alte Abschnitte ohne W1-3")
    a = ap.parse_args()
    global VORZIEHEN, BIS, ABSCHNITTE
    VORZIEHEN = a.vorziehen
    BIS = a.bis
    ABSCHNITTE = [x for x in ([0, 4] if a.grob else [0, 1, 2, 3, 4]) + [8, 12, 16, 20, 25, 30, 35, 40, 45] if x < BIS]
    B, A, Z, katalog, phasen, K, gabeln, _ = st.lade_alles()
    tw, wi = st.Turmwerte(B), st.Wirtschaft(B, Z)
    listen = wellen_listen(B, katalog, phasen, A, a.laeufe)
    pruef = wellen_listen(B, katalog, phasen, A, 100, seed=11)   # andere Zufälle für die Vorhersage
    # Auswahl nach dem Plan, nicht nach der Strategie: die beste von 48 Strategien auf wenigen Zufällen ist überschätzt,
    # und der Selbsttest kann nur neu bauen (Prüfung 06.11.: Strategie 100 %, als Plan 0-88 %)
    beste = {}
    for i, s in enumerate(st.strategien(random.Random(7))):
        stand = None
        for j, wl in enumerate(listen[:3]):
            stand, ok = aufzeichnen(s, wl, B, Z, K, gabeln, tw, wi, random.Random(7 * 31 + i * 7 + j))
            if ok:
                break
        if not ok:
            continue
        ausbau, forschung = verdichten(stand)
        ende = nachspielen(s, ausbau, forschung, B, Z, K, gabeln, listen)
        wert = (sum(e > BIS for e in ende) / len(ende), statistics.mean(ende))
        if not s["Linie"].startswith(a.nur):
            continue
        if s["Linie"] not in beste or wert > beste[s["Linie"]][0]:
            beste[s["Linie"]] = (wert, i, s, ausbau, forschung)
    zeilen, namen = [], []
    for linie, (wert, i, s, ausbau, forschung) in beste.items():
        # Vorhersage auf frischen Zufällen
        ende = nachspielen(s, ausbau, forschung, B, Z, K, gabeln, pruef)
        quote = sum(e > BIS for e in ende) / len(ende)
        tot = sorted(e for e in ende if e <= BIS)
        median = tot[len(tot) // 2] if tot else "-"
        tuerme = sum(e[1] for v in ausbau.values() for e in v)
        kurz = linie.replace(" ", "").replace("+", "-")
        name = f"Strat-{kurz}" + ("-Vor" if VORZIEHEN else "") + (f"-W{BIS}" if BIS != 30 else "")
        beschr = (f"Beste Modellstrategie der Linie {linie} (Nr. {i}, Doktrin {s['Doktrin'] or 'keine'}, Stil {s['Stil']}, Kern {'ja' if s['Kern'] else 'nein'}, "
                  f"Gabel {'/'.join(s['Gabel'].values())}): Plan bei der Auswahl {wert[0] * 100:.0f} % bis W30, Vorhersage {quote * 100:.0f} %, {tuerme} Türme.")
        zeilen.append(f"| {linie} | {wert[0] * 100:.0f} % | {quote * 100:.0f} % | {median} | {tuerme} | {s['Doktrin'] or '-'} | {s['Stil']} |")
        print(beschr)
        for ab, v in sorted(ausbau.items()):
            print(f"    W{ab:2d}: " + ", ".join(f"{n}x {t} L{lv} ({r.replace('Recipe_TD_', '')})" for t, n, lv, r, _ in v) + (f"; Forschung {', '.join(forschung.get(ab, []))}" if forschung.get(ab) else ""))
        if not a.nur_modell:
            p = spielplan(name, beschr, ausbau, forschung)
            json.dump(p, open(os.path.join(bk.TESTS, "Plaene", name + ".json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
            namen.append(name)
            ablage = os.path.join(bk.AUS, "strategie_plaene.json")
            alle = json.load(open(ablage, encoding="utf-8")) if os.path.exists(ablage) else {}
            alle[name] = {"Ausbau": {str(k): v for k, v in ausbau.items()}, "Forschung": {str(k): v for k, v in forschung.items()},
                          "Strategie": i, "Vorhersage_W30": quote, "Fall_Median": median}
            json.dump(alle, open(ablage, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    tabelle = ["| Linie | Auswahl bis W30 | Vorhersage bis W30 | Plan: Fall-Welle Median | Türme | Doktrin | Stil |", "|---|---|---|---|---|---|---|"] + zeilen
    print("\n".join(tabelle))
    open(os.path.join(bk.AUS, "strategie_plaene" + ("_vor" if VORZIEHEN else "") + (f"_w{BIS}" if BIS != 30 else "") + ".md"), "w", encoding="utf-8").write(
        "# Spielpläne aus Modellstrategien\n\nVorhersage mit 100 Wellen-Zufällen (Seed 11), Auswahl mit "
        f"{a.laeufe} Zufällen (Seed 7).\n\n" + "\n".join(tabelle) + "\n")
    if not a.nur_modell:
        paket = {"Name": "Strategie", "_Hinweis": "Beste Modellstrategie je Linie als fester Plan bis W30 (Balance-Kreislauf 3.3).",
                 "Plaene": namen}
        pp = os.path.join(bk.TESTS, "Pakete", "Strategie.json")
        alt = lade(pp) if VORZIEHEN else []
        paket["Plaene"] = [n for n in alt if n not in namen] + paket["Plaene"]
        paket["Plaene"] = sorted(set(paket["Plaene"]), key=paket["Plaene"].index) * a.wiederholungen
        json.dump(paket, open(pp, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
        print(f"Paket Strategie: {len(paket['Plaene'])} Läufe (etwa {len(paket['Plaene']) * 12 / 60:.1f} h, bis W30 etwa 12 min je Lauf)")


if __name__ == "__main__":
    main()
