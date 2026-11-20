"""FicsitTD Balance-Kreislauf (Notizen/Testplan-Balance-Kreislauf.md): Simulation und automatisches Spielen im Wechsel.

  python balance_kreislauf.py simuliere --gruppe Wirtschaft    Kandidat suchen (Koordinatensuche über stellschrauben.json)
  python balance_kreislauf.py pakete                           Spielpläne je Turmlinie, Stand und Kandidat -> Paket Kreislauf
  python balance_kreislauf.py auswerten [Bericht]              Messung gegen Modell, Kampfmodell gemeinsam kalibrieren (verlust_fit.py)
  python balance_kreislauf.py status                           Iterationen und Stand

Zustand und Verlauf: Ausgabe/Kreislauf/zustand.json, kandidat.json, Iteration_<n>.md.
Ändert nie den Mod: Kandidaten wirken im Spiel nur über den Selbsttest-Schritt "balance".
"""
import argparse
import collections
import csv
import datetime
import glob
import json
import os
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # Repo-Wurzel (Tools/Simulator/..)
import random
import statistics
import sys

import simulator as sim
import strategien as st

HIER = os.path.dirname(os.path.abspath(__file__))
AUS = os.path.join(HIER, "Ausgabe", "Kreislauf")
TESTS = os.path.join(REPO, "Tests")
BERICHTE = r"D:\OneDrive\Claude\Claude Modding\Satisfactory\Testberichte"
sys.path.insert(0, TESTS)

KRITERIEN = ["Ziel_Wege", "Ziel_NichtAlles", "Ziel_NichtZuLeicht", "Ziel_KeineDominanz", "Ziel_KeineToteLinie",
             "Ziel_W1", "Ziel_W2", "Ziel_W3", "Ziel_W4", "Ziel_W5", "Ziel_W6"]
REZEPT_MUNITION = {"Recipe_TD_Shoot_SpikedRebar": "Armierungsbolzen", "Recipe_TD_Shoot_Durchschlag": "Durchschlagbolzen",
                   "Recipe_TD_Gun_Standard": "Standard", "Recipe_TD_Tesla_Discharge": "Entladung",
                   "Recipe_TD_Nobelisk_Explosive": "Nobelisk", "Recipe_TD_Flame_Fuel": "Treibstoff"}
# Spielpläne: gemeinsame Startbasis bis W4 (Bolzen + Gewehr; reine Bolzenbasen fielen in W8-9, Gegenprüfung 06.11.), danach je Linie
START = {0: [("Bolzenturm", 2, 1, "Recipe_TD_Shoot_SpikedRebar")],
         4: [("Bolzenturm", 1, 2, "Recipe_TD_Shoot_Durchschlag"), ("Gewehrturm", 1, 2, "Recipe_TD_Gun_Standard")]}
LINIEN_PLAN = {
    "Bolzen": ("Bolzenturm", "Recipe_TD_Shoot_Durchschlag", 8, "DoktrinBolzen", None),
    "Gewehr": ("Gewehrturm", "Recipe_TD_Gun_Standard", 8, "DoktrinGewehr", None),
    "Nobelisk": ("Nobelisk-Werfer", "Recipe_TD_Nobelisk_Explosive", 12, "DoktrinNobelisk", None),
    "Tesla": ("Teslaspule", "Recipe_TD_Tesla_Discharge", 8, "DoktrinTesla", None),
    "Flamme": ("Flammenwerfer", "Recipe_TD_Flame_Fuel", 16, "DoktrinFlamme", "innen"),
}


def lade_json(p, std):
    try:
        return json.load(open(p, encoding="utf-8"))
    except (OSError, ValueError):
        return std


def speichere_json(p, x):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    json.dump(x, open(p, "w", encoding="utf-8"), ensure_ascii=False, indent=2)


# ---------------------------------------------------------------- feste Pläne im Modell (W1 und Abgleich mit dem Spiel)
def ausbau_linie(name):
    """Ausbau je Welle wie die Spielpläne: Startbasis, ab Freigabe je Abschnitt 2 Einheiten der Linie, Level steigend"""
    typ, rez, ab, _, ring = LINIEN_PLAN[name]
    a = {w: list(v) for w, v in START.items()}
    stufen = [(8, 2), (12, 3), (16, 3), (20, 4), (25, 4)]
    for w, lv in stufen:
        if w >= ab:
            eintrag = (typ, 2, lv, rez) + ((ring,) if ring else ())
            a.setdefault(w, []).append(eintrag)
        elif typ != "Gewehrturm":
            a.setdefault(w, []).append(("Gewehrturm", 1, min(lv, 3), "Recipe_TD_Gun_Standard"))
    return a


def ausbau_misch():
    import erzeuge_referenzen as er
    return {w: [tuple(e[:4]) for e in v] for w, v in er.MISCH.items()}


class FesterLauf(st.Lauf):
    """Lauf mit festem Ausbauplan und fester Forschung (wie die Spielpläne); EP dürfen ins Minus (zeigt W1)"""
    def __init__(self, ausbau, forschung, *a, **k):
        super().__init__({"Linie": "fest", "Gewichte": {t: 1 for t in st.TURM_TYPEN}, "Doktrin": None, "Stil": "Tuerme", "Kern": False,
                          "Gabel": {"Bolzen": "Explosivbolzen", "Gewehr": "Zielsuchmunition", "Nobelisk": "GasNobelisk", "Tesla": "Kettenblitz", "Kern": "KernSchild"}},
                         *a, **k)
        self.ausbau, self.feste_forschung, self.mun = ausbau, forschung, {}
        self.tuerme = []

    def forsche(self, n, gabeln):
        # wie plan() in erzeuge_referenzen: im Abschnitt vor Welle n wird alles mit ab <= Abschnitt + 5 erforscht (ohne Kosten)
        abschnitt = max([0] + [a for a in (4, 8, 12, 16, 20, 25, 30) if a < n])
        for w, knoten in self.feste_forschung.items():
            if w <= abschnitt + 5:
                # nur, was im Spiel schon freigegeben ist (Knoten "ab"): sonst stand Kohlekraft (ab W6) schon vor W1 bereit
                # und W1 rechnete mit einem Kohlekraftwerk im Startkauf (Hinweis Marius 06.11.)
                self.erforscht.update(k for k in knoten if self.K.get(k, {}).get("ab", 0) <= n)

    def beste_munition_fuer_wirtschaft(self, typ):
        name = self.mun.get(typ)
        for m in st.MUNITION[typ]:
            if m[0] == name:
                return m
        return super().beste_munition_fuer_wirtschaft(typ)

    def baue(self, n, zyklus, D):
        self.welle_n = n
        for typ, anz, lv, rez, *_ in self.ausbau.get(n - 1, []):
            self.mun[typ] = REZEPT_MUNITION.get(rez, self.mun.get(typ))
            if anz == 0:
                # 0.84: Aufwerten vorhandener Tuerme (Selbsttest "aufwerten"), Level-Differenz bezahlen
                for t in self.tuerme:
                    if t[0] == typ and t[1] < lv:
                        p = self.levelpreis(typ, t[1], lv)
                        self.ep -= p
                        self.ep_ausgegeben["Level"] += p
                        t[1] = lv
            for _ in range(anz):
                self.tuerme.append([typ, lv])
        anzahl = collections.Counter(t for t, _ in self.tuerme)
        for t, z in anzahl.items():
            fehl = [t] * max(0, z - self.besitz.get(t, 0))
            self.kaufe(fehl, "Tuerme")
        bed, strom = self.bedarf(self.tuerme, zyklus, D)
        geb, _, _ = self.wi.gebaeude(bed, strom, n, self.erforscht)
        self.kaufe(self.fehlende_kontingente(geb), "Fabrik")
        # Level einmalig bezahlen (je Turm von 1 auf sein Level)
        bezahlt = getattr(self, "_bezahlt", 0)
        for i in range(bezahlt, len(self.tuerme)):
            p = self.levelpreis(self.tuerme[i][0], 1, self.tuerme[i][1])
            self.ep -= p
            self.ep_ausgegeben["Level"] += p
        self._bezahlt = len(self.tuerme)
        self.versorgung = 1.0


def spiele_fest(ausbau, B, Z, K, gabeln, wellen_liste, stufe="Normal", laenge=30, seed=1):
    import erzeuge_referenzen as er
    L = FesterLauf(ausbau, er.FORSCHUNG, B, Z, K, gabeln, st.Turmwerte(B), st.Wirtschaft(B, Z), stufe)
    aufbau, pause = B[f"Stufen.{stufe}.AufbauSekunden"], B[f"Stufen.{stufe}.PauseSekunden"]
    rng = random.Random(seed)
    zeilen = []
    for n in range(1, laenge + 1):
        D = Z["WellenGrund"] + Z["WellenJe"] * (n - 1)
        zyklus = st.zyklus_min(Z, n, D)
        L.forsche(n, gabeln)
        L.baue(n, zyklus, D)
        L.verbrenne(n, zyklus, D)
        ep_vor = L.ep
        gegner, elite = wellen_liste[n - 1]
        q, k, verlust = L.kampf(gegner, elite, n, rng)
        zeilen.append({"Welle": n, "EP_vor": ep_vor, "Q": q, "K": k, "QN": L.q_nenn, "KN": L.k_nenn, "Verlust": verlust, "Kern": max(0.0, L.kern)})
        if L.kern <= 0:
            break
        L.belohnung(n, len(gegner), elite, n % 10 == 0)
    return zeilen


def w1_misch(B, Z, K, gabeln, katalog, phasen, A):
    """W1: Referenz-Misch früh bezahlbar - EP vor jeder Welle >= 0 bis W10 (Kampf ignoriert, nur Wirtschaft)"""
    wellen = sim.Wellen(B, katalog, phasen, A["Gegner_GrundLP"], "Normal")
    rng = random.Random(3)
    liste = [wellen.erzeuge(n, rng) for n in range(1, 11)]
    z = spiele_fest(ausbau_misch(), B, {**Z, "VerlustGrund": 0, "VerlustQ": 0, "VerlustUeber": 0, "AusreisserChance": 0}, K, gabeln, liste, laenge=10)
    tief = min(r["EP_vor"] for r in z)
    return tief, tief >= 0


# ---------------------------------------------------------------- Simulieren und Kandidat suchen
def bewerte(bal, reg, laeufe=1):
    B, A, Z, katalog, phasen, K, gabeln, _ = st.lade_alles(bal)
    Z.update(reg)
    erg, _ = st.massenlauf("Normal", int(Z.get("BewertungWellen", 50)), laeufe, 7, balance_ueber=bal, z_ueber=reg)
    zb = st.zielbild(erg, Z)
    tief, ok = w1_misch(B, Z, K, gabeln, katalog, phasen, A)
    zb["W1_Tiefpunkt"], zb["Ziel_W1"] = tief, ok
    erfuellt = sum(bool(zb[k]) for k in KRITERIEN)
    # Abstand zum Ziel als Gleichstandsbrecher (kleiner ist besser)
    linien = list(zb["JeLinie"].values())
    abstand = max(0, max(linien) - Z["ZielLinieMax"]) + max(0, Z["ZielLinieMin"] - min(linien)) + max(0, zb["Quote"] - Z["ZielQuoteMax"]) + \
        max(0, zb["W_Ueberschuss"] - Z["ZielUeberschussMax"]) + max(0, -tief) / 50
    return erfuellt, abstand, zb


def logik_ok(werte, logik):
    ns = {}
    for k, v in werte.items():
        ns[k.replace(".", "_").replace("-", "_")] = v
    for regel in logik:
        ausdruck = regel["Regel"]
        for k in sorted(werte, key=len, reverse=True):
            ausdruck = ausdruck.replace(k, k.replace(".", "_").replace("-", "_"))
        try:
            if not eval(ausdruck, {}, ns):
                return False, regel["Text"]
        except NameError:
            continue
    return True, ""


def simuliere(gruppe, laeufe, runden):
    S = lade_json(os.path.join(HIER, "stellschrauben.json"), {})
    schrauben = S["Gruppen"][gruppe]
    B0, A, Z0, *_ = st.lade_alles()
    zustand = lade_json(os.path.join(AUS, "zustand.json"), {"Iterationen": [], "Basis": {"Balance": {}, "Regler": {}}})
    bal, reg = dict(zustand["Basis"]["Balance"]), dict(zustand["Basis"]["Regler"])

    def wert(s):
        return reg.get(s["Regler"], Z0.get(s["Regler"])) if "Regler" in s else bal.get(s["Schluessel"], B0.get(s["Schluessel"]))

    def setze(s, v, b, r):
        b, r = dict(b), dict(r)
        if "Regler" in s:
            r[s["Regler"]] = v
        else:
            b[s["Schluessel"]] = v
            # Kopplung: abhängige Werte ziehen mit, damit Logikregeln wie "Turbo >= Treibstoff" halten
            for k in s.get("ziehtMit", []):
                b[k] = max(b.get(k, B0.get(k, 0)), v)
        return b, r
    start = bewerte(bal, reg, laeufe)
    bester = (start[0], -start[1], bal, reg, start[2])
    print(f"Ausgang: {start[0]}/{len(KRITERIEN)} Kriterien, Abstand {start[1]:.3f}")
    verlauf = []
    for runde in range(runden):
        verbessert = False
        for s in schrauben:
            for richtung in (+1, -1):
                v0 = wert(s)
                v = min(s["max"], max(s["min"], round(v0 * (1 + richtung * s["schritt"]), 4)))
                if abs(v - v0) < 1e-9:
                    continue
                b, r = setze(s, v, bester[2], bester[3])
                alle = {**B0, **b}
                ok, regel = logik_ok(alle, S.get("Logik", []))
                if not ok:
                    verlauf.append(f"{s.get('Schluessel', s.get('Regler'))} -> {v}: verworfen ({regel})")
                    continue
                e, ab, zb = bewerte(b, r, laeufe)
                verlauf.append(f"{s.get('Schluessel', s.get('Regler'))} {v0} -> {v}: {e} Kriterien, Abstand {ab:.3f}")
                print(verlauf[-1])
                if (e, -ab) > bester[:2]:
                    bester = (e, -ab, b, r, zb)
                    verbessert = True
                    break
        if not verbessert:
            break
    kandidat = {"Gruppe": gruppe, "Zeit": datetime.datetime.now().isoformat(timespec="minutes"), "Balance": bester[2], "Regler": bester[3],
                "Kriterien": {k: bool(bester[4].get(k)) for k in KRITERIEN}, "Erfuellt": bester[0], "JeLinie": bester[4]["JeLinie"],
                "Wirtschaft": {k: bester[4][k] for k in ("W_Ueberschuss", "W_Fabrik", "W_Forschung", "W_Verzug", "W_Not", "W1_Tiefpunkt")},
                "Ausgang": {"Erfuellt": start[0], "JeLinie": start[2]["JeLinie"]}, "Verlauf": verlauf}
    speichere_json(os.path.join(AUS, "kandidat.json"), kandidat)
    zustand["Iterationen"].append({"Nr": len(zustand["Iterationen"]) + 1, "Schritt": "simuliert", "Gruppe": gruppe, "Erfuellt": bester[0], "Zeit": kandidat["Zeit"]})
    speichere_json(os.path.join(AUS, "zustand.json"), zustand)
    print(f"Kandidat: {bester[0]}/{len(KRITERIEN)} Kriterien -> {os.path.join(AUS, 'kandidat.json')}")


# ---------------------------------------------------------------- Spielpläne
def pakete(wiederholungen):
    import erzeuge_referenzen as er
    kand = lade_json(os.path.join(AUS, "kandidat.json"), None)
    if not kand:
        sys.exit("Kein Kandidat: erst 'simuliere' laufen lassen.")
    B0 = st.lade_alles()[0]
    stand = {k: B0[k] for k in kand["Balance"] if k in B0}
    # Im Selbsttest ersetzt "versorgung" die Fabrik und Strom ist frei: dort wirken nur Turm- und Munitionswerte.
    # EP, Laden und Sim-Regler (PreisFaktor, TeslaLaden) prüft das Spiel so nicht -> nur Stand messen (Kalibrierung des Kampfmodells).
    kampf = {k: v for k, v in kand["Balance"].items() if k.startswith(("Munition.", "Tuerme.", "Gegner.", "Wellen."))}
    varianten = [("Stand", stand)] + ([("Kandidat", kampf)] if kampf else [])
    if not kampf:
        print(f"Kandidat der Gruppe {kand['Gruppe']} ändert nichts, was der Selbsttest misst: nur Stand-Läufe zur Kalibrierung.")
    namen = []
    for linie in list(LINIEN_PLAN) + ["Misch"]:
        ausbau = ausbau_misch() if linie == "Misch" else ausbau_linie(linie)
        extra_dok = {} if linie == "Misch" else {16: [{"erforsche": LINIEN_PLAN[linie][3]}]}
        for var, werte in varianten:
            name = f"Kreis-{linie}-{var}"
            extra = {0: [{"balance": werte}]} if werte else {}
            for w, v in extra_dok.items():
                extra.setdefault(w, []).extend(v)
            p = er.plan(name, f"Balance-Kreislauf: Linie {linie}, {var} ({kand['Gruppe']}).", ausbau, er.SATT, extra=extra)
            json.dump(p, open(os.path.join(TESTS, "Plaene", name + ".json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
            namen.append(name)
    paket = {"Name": "Kreislauf", "_Hinweis": f"Balance-Kreislauf {kand['Zeit']}: je Linie Stand und Kandidat, {wiederholungen}-mal.",
             "Plaene": namen * wiederholungen}
    json.dump(paket, open(os.path.join(TESTS, "Pakete", "Kreislauf.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    # Streuung 05./06.11.: etwa 12 min je Lauf bis W30
    print(f"{len(namen)} Pläne, Paket Kreislauf mit {len(paket['Plaene'])} Läufen (etwa {len(paket['Plaene']) * 12 / 60:.1f} h)")


# ---------------------------------------------------------------- Auswerten und kalibrieren
def ausbau_fuer(name):
    """Ausbauplan zu einem Spielplan-Namen (für die Umrechnung Nenn-Q -> wirksames Q)"""
    import erzeuge_referenzen as er
    if name.startswith("Kreis-"):
        linie = name.split("-")[1]
        return ausbau_misch() if linie == "Misch" else ausbau_linie(linie) if linie in LINIEN_PLAN else None
    if name.startswith("Strat-"):
        # Pläne aus Modellstrategien (strategie_plaene.py): Ausbau liegt in der Ablage
        alle = lade_json(os.path.join(AUS, "strategie_plaene.json"), {})
        if name in alle:
            return {int(k): [tuple(e[:4]) for e in v] for k, v in alle[name]["Ausbau"].items()}
        return None
    for schluessel, plan in (("Gewehr", er.GEWEHR), ("Misch", er.MISCH), ("Faul", er.MISCH)):
        if schluessel in name:
            return {w: [tuple(e[:4]) for e in v] for w, v in plan.items()}
    return None


def wirk_verhaeltnis(ausbau, bal, laenge=30, laeufe=10):
    """je Welle: Nenn-K / wirksames K im Modell (Mehrfachziel-Faktoren), Mittel über einige Wellen-Zufälle"""
    B, A, Z, katalog, phasen, K, gabeln, _ = st.lade_alles(bal)
    Z0 = dict(Z, VerlustGrund=0, VerlustQ=0, VerlustUeber=0, VerlustStreuung=None, AusreisserChance=0)
    wellen = sim.Wellen(B, katalog, phasen, A["Gegner_GrundLP"], "Normal")
    summe = collections.defaultdict(lambda: [0.0, 0.0])
    for seed in range(laeufe):
        liste = [wellen.erzeuge(n, random.Random(seed * 13 + n)) for n in range(1, laenge + 1)]
        for r in spiele_fest(ausbau, B, Z0, K, gabeln, liste, laenge=laenge, seed=seed):
            summe[r["Welle"]][0] += r["KN"]
            summe[r["Welle"]][1] += r["K"]
    return {w: kn / k for w, (kn, k) in summe.items() if k > 0}


def spielt_wellen(name):
    """Pläne ohne Welle (z. B. Neustart) liefern keine Metrik-Zeilen und dürfen die Zuordnung nicht verschieben (07.11.)"""
    plan = lade_json(os.path.join(TESTS, "Plaene", f"{name}.json"), None)
    if not plan:
        return True
    schritte = plan.get("Schritte") or plan.get("schritte") or []
    return any("wellenlauf" in s or "kern weiter" in str(s.get("befehl", "")) for s in schritte if isinstance(s, dict))


def laeufe_aus(csv_pfad, namen):
    namen = [n for n in namen if spielt_wellen(n)]
    zeilen = list(csv.DictReader(open(csv_pfad, encoding="utf-8"), delimiter=";"))
    laeufe, vorher = [], 10 ** 6
    for z in zeilen:
        if int(z["Welle"]) <= vorher:
            laeufe.append([])
        laeufe[-1].append(z)
        vorher = int(z["Welle"])
    return list(zip(namen, laeufe))


def auswerten(bericht):
    if not bericht:
        kandidaten = sorted(glob.glob(os.path.join(BERICHTE, "*_Kreislauf_Metriken.csv")), key=os.path.getmtime)
        if not kandidaten:
            sys.exit("Kein Kreislauf-Bericht gefunden.")
        bericht = kandidaten[-1]
    # Paket aus dem Berichtsnamen: <Datum>_<Zeit>_<Paket>_Metriken.csv
    paket_name = os.path.basename(bericht).split("_")[2]
    paket = lade_json(os.path.join(TESTS, "Pakete", f"{paket_name}.json"), None)
    if not paket:
        sys.exit(f"Paket {paket_name} nicht gefunden.")
    daten = laeufe_aus(bericht, paket["Plaene"])
    mit_welle = [n for n in paket["Plaene"] if spielt_wellen(n)]
    if len(daten) != len(mit_welle):
        print(f"Achtung: {len(daten)} Läufe in der Messung, {len(mit_welle)} Pläne mit Wellen im Paket")
    je = collections.defaultdict(list)
    paare = []   # (wirksames Q, Verlust) für die Verlustkurve
    wirk = collections.defaultdict(list)
    kand = lade_json(os.path.join(AUS, "kandidat.json"), {"Balance": {}})
    verh_cache = {}
    # Streckung wie im Modell (strategien.kampf): Wellen über der Lebend-Obergrenze verteilen den Druck
    B0, _, Z0, *_ = st.lade_alles()
    obergrenze, streckung = B0.get("Wellen.MaxGleichzeitig", 60), Z0.get("VerlustStreckung", 0.0)
    for name, lauf in daten:
        # Das Spiel misst Q mit Einzelziel-DPS; das Modell rechnet den Verlust mit wirksamem Q (Mehrfachtreffer).
        # Umrechnung je Welle mit dem Verhältnis Nenn-K / wirksames K des Modells für denselben Ausbau (Prüfung 06.11.: R² 0,28 statt 0,11).
        ausbau = ausbau_fuer(name)
        if ausbau is None:
            continue
        schl = (name.replace("-Kandidat", "-Stand"), name.endswith("-Kandidat"))
        if schl not in verh_cache:
            verh_cache[schl] = wirk_verhaeltnis(ausbau, kand["Balance"] if schl[1] else {})
        verh = verh_cache[schl]
        je[name].append({"Welle": int(lauf[-1]["Welle"]), "Tief": min(float(z["Kern_Proz"]) for z in lauf),
                         "Q": statistics.mean(float(z["Q"]) for z in lauf)})
        linie = name.split("-")[1] if name.startswith("Kreis-") else None
        for z in lauf:
            if float(z["Hungrig_s"]) > 1:
                continue   # Munitionsmangel: Verlust kommt nicht aus dem Druck (Q), das rechnet das Modell getrennt
            q, v = float(z["Q"]) * verh.get(int(z["Welle"]), 1.0), float(z["Kernverlust_Proz"])
            q /= max(1.0, float(z["Gegner"]) / obergrenze) ** streckung
            paare.append((q, v))
            k, d, schaden = float(z["K_DPS"]), float(z["Dauer_s"]), float(z["Schaden"])
            ab = LINIEN_PLAN.get(linie, (None, None, 99))[2]
            if linie in LINIEN_PLAN and int(z["Welle"]) > ab + 4 and k > 0 and d > 0:
                wirk[LINIEN_PLAN[linie][0]].append(schaden / (k * d))
    # Verlustkurve: Gerade a + b*Q (mittlerer Verlust) durch kleinste Quadrate über alle Wellen; a >= 0
    a = b = r2 = None
    if len(paare) >= 30:
        xs, ys = [q for q, _ in paare], [v for _, v in paare]
        mx, my = statistics.mean(xs), statistics.mean(ys)
        b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / max(1e-9, sum((x - mx) ** 2 for x in xs))
        a = my - b * mx
        if a < 0:
            a, b = 0.0, sum(x * y for x, y in zip(xs, ys)) / sum(x * x for x in xs)
        r2 = 1 - sum((y - a - b * x) ** 2 for x, y in zip(xs, ys)) / max(1e-9, sum((y - my) ** 2 for y in ys))
    # Streuung um die Grundkurve: Dezile von Messung / Grundkurve (ersetzt das Ausreissermodell)
    dezile = None
    if a is not None:
        r = sorted(v / max(0.3, a + b * q) for q, v in paare)
        # Mittel je Zehntel (nicht die Mitte), damit der mittlere Verlust erhalten bleibt (Verteilung ist schief)
        dezile = [statistics.mean(r[i * len(r) // 10:(i + 1) * len(r) // 10]) for i in range(10)]
        # auf Mittel 1 normieren: die Gerade trägt den mittleren Verlust, die Dezile nur die Form der Streuung
        # (ohne Normierung lag das Mittel bei 0,85 und das Modell verlor 15 % zu wenig, Prüfung 06.11.)
        m = statistics.mean(dezile)
        dezile = [round(d / m, 2) for d in dezile]
    wirkgrad = {t: statistics.median(v) for t, v in wirk.items() if len(v) >= 5}
    ref = wirkgrad.get("Gewehrturm")
    faktoren = {t: round(w / ref, 3) for t, w in wirkgrad.items()} if ref else {}
    # Kalibrierung seit 07.11. gemeinsam über alle bekannten Messungen (verlust_fit.py: Kurve a*qv^p + Mehrziel je Typ),
    # übernommen nur, wenn sie die Messungen besser erklärt. Die Gerade oben bleibt nur als Vergleich im Bericht.
    import verlust_fit as vf
    neu = (bericht, paket_name, None)
    fit = vf.kalibriere([m for m in vf.STANDARD if m[0] != os.path.basename(bericht)] + [neu], schreiben=True)
    # Bericht
    zustand = lade_json(os.path.join(AUS, "zustand.json"), {"Iterationen": [], "Basis": {"Balance": {}, "Regler": {}}})
    nr = len(zustand["Iterationen"])
    zeilen = [f"# Balance-Kreislauf: Auswertung Iteration {nr}", "", f"Messung `{os.path.basename(bericht)}`.", "",
              "| Plan | Läufe | höchste Welle | Kern-Tiefpunkt (Median) | Q (Mittel) |", "|---|---|---|---|---|"]
    for name in dict.fromkeys(paket["Plaene"]):
        rs = je.get(name, [])
        if rs:
            zeilen.append(f"| {name} | {len(rs)} | {', '.join(str(r['Welle']) for r in rs)} | {statistics.median(r['Tief'] for r in rs):.0f} % | {statistics.mean(r['Q'] for r in rs):.2f} |")
    zeilen += ["", f"**Verlustkurve:** Kernverlust ≈ {a:.2f} + {b:.2f} × wirksames Q (Mittel, R² {r2:.2f}, {len(paare)} Wellen ohne Munitionsmangel), Streuung als Faktor (Dezile) {dezile}." if a is not None else "**Verlustkurve:** zu wenige Daten.",
               "", "**Wirkgrad je Turm relativ zum Gewehrturm (Schaden je Nenn-DPS, nur Bericht):** " + (", ".join(f"{t} {f}" for t, f in faktoren.items()) or "zu wenige Daten"), "",
               (f"**Gemeinsame Kalibrierung (verlust_fit.py):** {fit['Wellen']} Wellen, Verlust = {fit['VerlustQ']:.2f} × qv^{fit['VerlustP']:.2f}, "
                f"R² {fit['R2_neu']:.2f} (bisher {fit['R2_heute']:.2f}); Mehrziel " + ", ".join(f"{t} {c:.2f}" for t, c in fit["Mehrziel"].items()) +
                (". Übernommen." if fit["R2_neu"] > fit["R2_heute"] + 0.01 else ". Nicht übernommen, erklärt die Messung nicht besser.")) if fit else "**Gemeinsame Kalibrierung:** zu wenige Daten.",
               "", "Nächster Schritt: `simuliere` mit dem Modell."]
    pfad = os.path.join(AUS, f"Iteration_{nr}.md")
    os.makedirs(AUS, exist_ok=True)
    open(pfad, "w", encoding="utf-8").write("\n".join(zeilen) + "\n")
    zustand["Iterationen"].append({"Nr": nr, "Schritt": "ausgewertet", "Bericht": os.path.basename(bericht), "Zeit": datetime.datetime.now().isoformat(timespec="minutes")})
    speichere_json(os.path.join(AUS, "zustand.json"), zustand)
    print("\n".join(zeilen))


def status():
    z = lade_json(os.path.join(AUS, "zustand.json"), {"Iterationen": []})
    k = lade_json(os.path.join(AUS, "kandidat.json"), None)
    for it in z["Iterationen"]:
        print(it)
    if k:
        print(f"Kandidat ({k['Gruppe']}, {k['Zeit']}): {k['Erfuellt']}/{len(KRITERIEN)} Kriterien, Balance {k['Balance']}, Regler {k['Regler']}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("befehl", choices=["simuliere", "pakete", "auswerten", "status"])
    ap.add_argument("--gruppe", default="Wirtschaft")
    ap.add_argument("--laeufe", type=int, default=1)
    ap.add_argument("--runden", type=int, default=3)
    ap.add_argument("--wiederholungen", type=int, default=2)
    ap.add_argument("bericht", nargs="?")
    a = ap.parse_args()
    if a.befehl == "simuliere":
        simuliere(a.gruppe, a.laeufe, a.runden)
    elif a.befehl == "pakete":
        pakete(a.wiederholungen)
    elif a.befehl == "auswerten":
        auswerten(a.bericht)
    else:
        status()


if __name__ == "__main__":
    main()
