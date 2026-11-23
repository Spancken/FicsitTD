"""FicsitTD-Simulator: Druckquote Q und Kurven von Gegnerdruck und Abwehrkapazitaet je Welle, ohne Spiel.

Liest die Standardwerte aus dem Mod-Code (TDBalance.cpp, Gegnerkatalog in TDWaveSubsystem.cpp), legt die eigene
Balance.json des Spiels darueber und ergaenzt, was nicht im Code steht, aus annahmen.json (Grund-LP der Kreaturen,
Modell-Spieler, Forschungsreihenfolge). Die Wellen werden wie im Spiel zusammengestellt (gleicher Zufallsablauf),
der Modell-Spieler kauft mit seinen Coupons Tuerme und Turmlevel und erforscht mit Festplatten bessere Munition (Stand 16.11.2026).

Aufruf:  python simulator.py [--stufe Normal] [--balance PFAD] [--aus ORDNER]
Ausgabe: Ausgabe/Simulation.html (Diagramme + Tabellen), Ausgabe/Simulation.csv
Recherche-Bericht Abschnitt 3: Notizen/Recherche-Progression-Endgame-Balancing.md
"""
import argparse
import csv
import datetime
import glob
import json
import math
import os
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # Repo-Wurzel (Tools/Simulator/..)
import random
import re
import statistics

HIER = os.path.dirname(os.path.abspath(__file__))
QUELLE = os.path.join(REPO, "Source", "FicsitTD", "Private")
SAVED = os.path.join(os.environ.get("LOCALAPPDATA", ""), "FactoryGame", "Saved")
STUFEN = ["Erkundung", "Normal", "Hart", "Albtraum"]
# Spiel: KernStufen[] in TDWaveSubsystem.cpp (nicht in der Balance-Tabelle)
STUFE_FEST = {"Erkundung": 0, "Normal": 1, "Hart": 2, "Albtraum": 3}
KERN_MAX_FRONTEN = 4
FARBEN = {"Erkundung": "#4C9A6A", "Normal": "#3A6EA5", "Hart": "#D08A2E", "Albtraum": "#B23A48"}


# ---------------------------------------------------------------- Daten laden
def lade_balance(pfad_json):
    """Standards aus TDBalance.cpp, darueber die Balance.json des Spiels (falls vorhanden)."""
    text = open(os.path.join(QUELLE, "TDBalance.cpp"), encoding="utf-8-sig").read()
    werte = {k: float(v) for k, v in re.findall(r'\{\s*TEXT\("([^"]+)"\),\s*([-0-9.]+)f?\s*\}', text)}
    quelle = "Standardwerte aus TDBalance.cpp"
    if pfad_json and os.path.exists(pfad_json):
        def flach(obj, vorne=""):
            for k, v in obj.items():
                if k.startswith("_"):
                    continue
                name = f"{vorne}.{k}" if vorne else k
                if isinstance(v, dict):
                    flach(v, name)
                elif isinstance(v, (int, float)):
                    werte[name] = float(v)
        flach(json.load(open(pfad_json, encoding="utf-8-sig")))
        quelle += f" + {pfad_json}"
    return werte, quelle


def lade_katalog(B):
    """KernKatalog (Kurz, Klasse, Punkte, AbWelle, Rolle, Phase) und KernPhasen (Boss je Phase) aus TDWaveSubsystem.cpp"""
    text = open(os.path.join(QUELLE, "TDWaveSubsystem.cpp"), encoding="utf-8-sig").read()
    block = text[text.index("KernKatalog[]"):text.index("AbWelleVon")]
    katalog = [{"Kurz": k, "Name": n, "Punkte": int(p), "AbWelle": int(B.get(f"Katalog.{k}.AbWelle", int(a))),
                "Rolle": int(r), "Phase": int(ph)}
               for k, n, p, a, r, ph in re.findall(
                   r'\{\s*TEXT\("(\w+)"\),\s*TEXT\("[^"]*Char_(\w+)\.Char_\w+_C"\),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)', block)]
    block = text[text.index("KernPhasen[]"):text.index("KernPhasenAnzahl")]
    phasen = [{"Name": nm, "Boss": b, "BossName": bn, "Anzahl": int(B.get(f"Phasen.{i + 1}.BossAnzahl", int(an))),
               "LP": B.get(f"Phasen.{i + 1}.BossLP", float(lp))}
              for i, (nm, b, bn, an, lp) in enumerate(re.findall(
                  r'\{\s*TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*TEXT\("[^"]*"\),\s*TEXT\("[^"]*Char_(\w+)\.Char_\w+_C"\),\s*TEXT\("([^"]+)"\),\s*(\d+),\s*([0-9.]+)f', block))]
    return katalog, phasen


def lade_forschung_ab_welle(B):
    """Freigabe je Phase aus TDForschungVoraussetzungen.inl (create_research.py), Balance Forschung.<Knoten>.AbWelle"""
    text = open(os.path.join(QUELLE, "TDForschungVoraussetzungen.inl"), encoding="utf-8-sig").read()
    return {k: int(B.get(f"Forschung.{k}.AbWelle", int(w))) for k, w in re.findall(r'TEXT\("[^"]+"\),\s*TEXT\("(\w+)"\),\s*(\d+)\s*\}', text)}


def lade_forschung_info(B):
    """16.11.2026: alle MAM-Knoten aus TDForschungInfo.inl (erzeugt von create_research.py): Kosten in Festplatten (FP),
    Standard-Welle (Balance Forschung.<Knoten>.AbWelle geht vor, wie TDForschung.cpp), Voraussetzungen, andere Seite der Gabelung"""
    text = open(os.path.join(QUELLE, "TDForschungInfo.inl"), encoding="utf-8-sig").read()
    info = {}
    for zeile in text.splitlines():
        if not zeile.strip().startswith("{ TEXT("):
            continue
        key = re.match(r'\s*\{\s*TEXT\("[^"]*"\),\s*TEXT\("[^"]*"\),\s*TEXT\("(\w+)"\)', zeile)
        rest = re.search(r'\),\s*(\d+),\s*(\d+),\s*TEXT\("([^"]*)"\),\s*TEXT\("([^"]*)"\),\s*TEXT\("(/FicsitTD[^"]*)"\)\s*\},?\s*$', zeile)
        if not key or not rest:
            raise ValueError(f"TDForschungInfo.inl: Zeile nicht lesbar (Format geaendert?): {zeile[:80]}")
        k = key.group(1)
        info[k] = {"FP": int(rest.group(1)), "AbWelle": int(B.get(f"Forschung.{k}.AbWelle", int(rest.group(2)))),
                   "Braucht": [v for v in rest.group(3).split(",") if v], "Andere": [v for v in rest.group(4).split(",") if v]}
    return info


def lade_level_standards():
    """Turmlevel (0.63+): Standard-Pakete je Turm und Level aus TDLevelStandards[] in TDTurretBase.cpp.
    Balance Tuerme.Level.<Turm>.L<n>.<Wert> geht vor (LevelWertBis)."""
    text = open(os.path.join(QUELLE, "TDTurretBase.cpp"), encoding="utf-8-sig").read()
    block = text[text.index("TDLevelStandards[]"):text.index("TDLevelWerte[]")]
    std = re.findall(r'\{\s*TEXT\("([\w-]+)"\),\s*(\d+),\s*TEXT\("(\w+)"\),\s*([0-9.]+)f\s*\}', block)
    if not std:
        raise ValueError("TDTurretBase.cpp: TDLevelStandards nicht gefunden (Format geaendert?)")
    m = re.search(r'Standard\[\]\s*=\s*\{([^}]*)\}', text[text.index("ATDTurretBase::LevelPreis"):])
    preise = [int(x) for x in m.group(1).split(",")] if m else [0, 0, 8, 15, 25, 40]
    return [(t, int(l), w, float(b)) for t, l, w, b in std], preise


def level_wert_bis(B, standards, turm, level, wert):
    """Wie ATDTurretBase::LevelWertBis: Summe der Pakete L2..Level fuer diesen Wert (nur Werte mit Standard)"""
    s = 0.0
    for t, l, w, betrag in standards:
        if t == turm and w == wert and 2 <= l <= min(level, 5):
            s += B.get(f"Tuerme.Level.{turm}.L{l}.{wert}", betrag)
    return s


def familie(name):
    """Wie ATDTurretBase::Resistenz"""
    if "Aquatic" in name: return "WasserSpitter"
    if "Desert" in name: return "WuestenSpitter"
    if "Spitter" in name: return "Spitter"
    if "Stinger" in name: return "Stinger"
    if "Crab" in name: return "Krabbe"
    if "Hog" in name or "Johnny" in name: return "Hog"
    return None


def runde(x):
    """FMath::RoundToInt (kaufmaennisch)"""
    return int(math.floor(x + 0.5))


# ---------------------------------------------------------------- Wellen wie im Spiel
class Wellen:
    def __init__(self, B, katalog, phasen, grund_lp, stufe, endlos=False):
        self.B, self.katalog, self.phasen, self.lp, self.stufe = B, katalog, phasen, grund_lp, stufe
        # 0.11 Endlos-Wellen: immer genau eine Front (UTDWaveSubsystem::AktiveFrontenFuer)
        self.endlos = endlos

    def w(self, k, std=0.0):
        return self.B.get(k, std)

    def s(self, k):
        return self.B[f"Stufen.{self.stufe}.{k}"]

    def lp_faktor(self, n):
        """LPFaktor in UTDWaveSubsystem (StarteWelle): bis W31 je Welle Wellen.LPJeWelle, danach Wellen.LPJeWelleAb31 (0.9.1)"""
        je = self.w("Endlos.LPJeWelle", self.w("Wellen.LPJeWelle", 0.08)) if self.endlos else self.w("Wellen.LPJeWelle", 0.08)   # 23.11.
        f = self.s("GegnerLP") * (1 + je * min(n - 1, 30)
                                  + self.w("Wellen.LPJeWelleAb31", 0.1) * max(n - 31, 0))
        if self.endlos:
            # 23.11.: eigener LP-Faktor je Stufe im Endlos (wie TDWaveSubsystem.cpp), damit Einsatz und Endlos getrennt einstellbar sind
            f *= self.w(f"Endlos.{self.stufe}.LPFaktor", 1.0)
            # 16.11. (P2): Endlos - ab Endlos.LPWachstumAbWelle wachsen die Gegnerleben je Welle um Endlos.LPWachstum (zinsartig),
            # sonst gibt es nach W50 keinen Druck mehr (Gegnerzahl gedeckelt, Coupons wachsen weiter). Wie in TDWaveSubsystem.cpp.
            ab = self.w("Endlos.LPWachstumAbWelle", 50)
            f *= (1 + self.w("Endlos.LPWachstum", 0.0)) ** max(0, n - ab)
        return f

    def fronten(self, n):
        if self.endlos:
            return 1
        neu = int(self.s("NeueFrontAbWelle"))
        z = int(self.s("FrontenStart")) + (n >= neu) + (n >= 2 * neu - 1)
        return max(1, min(z, KERN_MAX_FRONTEN))

    def erzeuge(self, n, rng):
        """UTDWaveSubsystem::PlanFuer (0.51+): Einfuehrungsgruppe, Phasengewicht, Boss je Phase. Liste (Name, LP) und Zahl der Elite"""
        budget = max(2, runde(self.s("Budget") * (self.w("Wellen.BudgetGrund", 4) + self.w("Wellen.BudgetJeWelle", 2.5) * n
                                                   + self.w("Wellen.BudgetQuadrat", 0.12) * n * n)))
        if n % 10 == 0:
            # 0.84: Bosswellen mit weniger Begleitern (Wellen.BossBudget, 1 = volles Budget wie bis 0.83)
            budget = max(2, runde(budget * self.w("Wellen.BossBudget", 1.0)))
        if self.endlos:
            # 16.11. (P2): Endlos - eine Front ohne Bauphase nach Wunsch; Budget je Welle mit Endlos.BudgetFaktor (wie TDWaveSubsystem.cpp)
            budget = max(2, runde(budget * self.w("Endlos.BudgetFaktor", 1.0)))
        lp_faktor = self.lp_faktor(n)
        max_gegner = int(self.w("Wellen.MaxGesamt", 120))
        mehr_ab = int(self.w("Endlos.MehrGegnerAbWelle", 60))
        if self.endlos and n > mehr_ab:
            # 17.11. (Marius: nach W60 mehr Gegner und Elite) wie TDWaveSubsystem.cpp PlanFuer
            max_gegner = min(int(self.w("Endlos.MaxGesamt", 240)), max_gegner + runde(self.w("Endlos.MehrGegnerJeWelle", 2) * (n - mehr_ab)))
        art = n % 5
        rolle = {1: 0, 2: 1, 4: 2, 0: 3}.get(art, -1)
        phase = max(0, n - 1) // 10
        gewicht = self.w("Katalog.PhasenGewicht", 3) if phase < len(self.phasen) else 1.0
        plan, anzahl, erlaubt = [], 0, []
        for e in self.katalog:
            if e["AbWelle"] == n:
                k = max(2, min(6, runde(6 / e["Punkte"])))
                plan += [(e["Name"], lp_faktor, False)] * k
                budget -= k * e["Punkte"]
                anzahl += k
            elif e["AbWelle"] < n:
                erlaubt.append(e)

        def waehle(liste):
            gw = [gewicht if e["Phase"] - 1 == phase else 1.0 for e in liste]
            return rng.choices(liste, weights=gw)[0]
        versuche = 0
        while budget > 0 and anzahl < max_gegner and erlaubt and versuche < 5000:
            versuche += 1
            wahl = waehle(erlaubt)
            if rolle >= 0 and rng.random() < 0.65:
                passend = [e for e in erlaubt if e["Rolle"] == rolle]
                if passend:
                    wahl = waehle(passend)
            if wahl["Punkte"] * 12 < n and rng.random() < 0.6:
                continue
            plan.append((wahl["Name"], lp_faktor, True))
            budget -= wahl["Punkte"]
            anzahl += 1
        elite_ab = int(self.s("EliteAbWelle"))
        chance = min(0.3, 0.1 + 0.01 * (n - elite_ab)) if n >= elite_ab else 0.0
        if self.endlos and n > mehr_ab:
            chance = min(self.w("Endlos.EliteMax", 0.6), 0.3 + 0.01 * (n - mehr_ab))
        gegner, elite = [], 0
        for name, f, darf_elite in plan:
            e = darf_elite and rng.random() < chance
            elite += e
            gegner.append((name, self.lp[name] * f * (self.w("Wellen.EliteLP", 2.5) if e else 1.0)))
        if n % 10 == 0 and self.phasen:
            b = self.phasen[(n // 10 - 1) % len(self.phasen)]
            gegner += [(b["Boss"], self.lp[b["Boss"]] * lp_faktor * b["LP"])] * max(1, b["Anzahl"])
        return gegner, elite


# ---------------------------------------------------------------- Modell-Spieler
class Spieler:
    """Modell-Spieler (Stand 16.11.2026): Coupons aus der Wellenabrechnung (UTDEinsatzSubsystem::WelleBeendet) gehen zum Anteil
    TurmAnteil in die Abwehr - neue Tuerme (Ladenpreis steigt je Kauf) oder Turmlevel 2-5 (Tuerme.Level.Preis<n>, im Turmfenster).
    Festplatten (FP) gehen in die Forschung, Kosten und Freigabewelle aus TDForschungInfo.inl. Seit 0.63 gibt es keine Stufen nach
    Abschuessen mehr: Level 2 mit dem Turm-Knoten, 3/4 mit Turmfertigung II/III, 5 mit Turmfertigung IV oder Doktrin (LevelMax)."""
    DOKTRIN = {"Bolzenturm": "DoktrinBolzen", "Gewehrturm": "DoktrinGewehr", "Teslaspule": "DoktrinTesla",
               "Flammenwerfer": "DoktrinFlamme", "Nobelisk-Werfer": "DoktrinNobelisk"}

    def __init__(self, B, A, stufe, ab_welle=None, mono=None, info=None, level=None):
        self.B, self.A, self.stufe = B, A, stufe
        # 0.73 Spezialisierung: nur diese Turmlinie kaufen (bis sie frei ist: Bolzenturm), mit Doktrin sobald erforscht
        self.mono = mono
        self.welle = 1
        self.ab_welle = ab_welle or {}
        self.info = info if info is not None else lade_forschung_info(B)
        self.level_std, self.level_preise = level if level is not None else lade_level_standards()
        self.ep_turm = float(A["Spieler"].get("StartEP", 0)) * A["Spieler"]["TurmAnteil"]
        self.ep_rest = 0.0
        self.fp = 0
        self.erforscht = set()
        self.gekauft = {t: 0 for t in A["Tuerme"] if not t.startswith("_")}
        # Startkontingent (Einsatz.Start.*): kostenlos, zaehlt nicht als gekauft; jeder Turm steht auf Level 1
        self.tuerme = []
        for t in self.gekauft:
            for _ in range(int(B.get(f"Einsatz.Start.{t}", 0))):
                self.tuerme.append({"Typ": t, "Level": 1})
        self.ep_ausgegeben = 0
        self.ep_level = 0
        self.ep_verfall = 0.0
        self._lw = {}
        self.reihenfolge = self._reihenfolge()

    def _reihenfolge(self):
        """Forschungsreihenfolge aus annahmen.json, Voraussetzungen (TDForschungInfo.inl) werden davor eingereiht"""
        plan = []

        def add(k):
            if k in plan or k not in self.info:
                return
            for v in self.info[k]["Braucht"]:
                add(v)
            plan.append(k)
        for eintrag in self.A["Forschung"]["Reihenfolge"]:
            add(eintrag[0] if isinstance(eintrag, list) else eintrag)
        if self.mono and self.DOKTRIN.get(self.balance_name(self.mono)):
            add(self.DOKTRIN[self.balance_name(self.mono)])
        return plan

    def balance_name(self, typ):
        """Turmname im Code (mTurmName, Tuerme.Level.*), weicht beim Flammenturm vom Ladenschluessel ab"""
        return self.A["Tuerme"][typ].get("Turmname", typ)

    def frei(self, forschung):
        return not forschung or forschung in self.erforscht

    def preis(self, typ):
        g = self.B.get(f"Einsatz.Laden.{typ}.Grundpreis", 99)
        a = self.B.get(f"Einsatz.Laden.{typ}.Aufschlag", 0.5)
        return runde(g * (1 + a * self.gekauft[typ]))

    def munition(self, typ):
        frei = [m for m in self.A["Munition"][typ] if self.frei(m["Forschung"])]
        return frei[-1] if frei else None

    def munitionen(self, typ):
        return [m for m in self.A["Munition"][typ] if self.frei(m["Forschung"])]

    def hat_doktrin(self, typ):
        k = self.DOKTRIN.get(self.balance_name(typ))
        return bool(k) and self.mono == typ and k in self.erforscht

    def level_max(self, typ):
        """ATDTurretBase::LevelMax"""
        if not self.frei(self.A["Tuerme"][typ]["Forschung"]):
            return 1
        if "Turmfertigung2" not in self.erforscht:
            return 2
        if "Turmfertigung3" not in self.erforscht:
            return 3
        return 5 if "Turmfertigung4" in self.erforscht or self.hat_doktrin(typ) else 4

    def level_preis(self, typ, naechstes):
        """ATDTurretBase::LevelPreis (Coupons), Doktrin-Linie mit Doktrin.LevelRabatt"""
        p = int(self.B.get(f"Tuerme.Level.Preis{naechstes}", self.level_preise[naechstes]))
        return max(1, runde(p * (1 - self.B.get("Doktrin.LevelRabatt", 0.25)))) if self.hat_doktrin(typ) else p

    def lw(self, typ, level, wert):
        k = (typ, level, wert)
        if k not in self._lw:
            self._lw[k] = level_wert_bis(self.B, self.level_std, self.balance_name(typ), level, wert)
        return self._lw[k]

    def dps(self, turm, level=None, m=None):
        """Nenn-DPS gegen ein Einzelziel wie ATD*::NennDPS: Schaden x SchadenBonus / Zykluszeit, SchadenBonus = (1 + Level-Schaden)
        x Doktrin, Takt = 1 + Level-Takt. Meisterfaehigkeiten (Level 5) stecken wie im Spiel nicht in NennDPS."""
        typ = turm["Typ"]
        m = m or self.munition(typ)
        if not m:
            return 0.0
        lv = turm["Level"] if level is None else level
        sb = (1 + self.lw(typ, lv, "Schaden")) * (1 + self.B.get("Doktrin.Schaden", 0.25) if self.hat_doktrin(typ) else 1.0)
        tb = 1 + self.lw(typ, lv, "Takt")
        k = "Munition." + m["Schluessel"]
        zyklus = self.A["Tuerme"][typ]["Zyklus"] / tb
        ziele = self.A["Tuerme"][typ].get("Ziele", 1.0)
        if typ == "Gewehrturm":
            return (self.B[k + ".Schaden"] + self.B.get(k + ".BrandSchaden", 0.0)) * sb * self.B[k + ".SchussProSekunde"] * tb
        if typ == "Flammenturm":
            # ATDFlameTurret::NennDPS: Schaden/s je Gegner im Kegel, "Ziele" = mittlere Zahl im Kegel (Annahme)
            return self.B[k + ".SchadenProSekunde"] * sb * ziele
        return self.B[k + ".Schaden"] * sb / zyklus * ziele

    def wert_je_ep(self, typ):
        return self.dps({"Typ": typ, "Level": 1}) / max(self.preis(typ), 1)

    def welle_beendet(self, n, kern_prozent):
        """UTDEinsatzSubsystem::WelleBeendet: Coupons (Schluessel weiter Einsatz.EP.*) und Festplatten (Einsatz.FP.*)"""
        B = self.B
        p = int(B["Einsatz.EP.Grund"]) + runde(B["Einsatz.EP.JeWelle"] * n) + (
            int(B["Einsatz.EP.KernVoll"]) if kern_prozent >= 99.5 else int(B["Einsatz.EP.KernAb75"]) if kern_prozent >= 75 else 0)
        if n % 10 == 0:
            p = runde(p * B["Einsatz.EP.BossFaktor"]) + int(B["Einsatz.EP.BossBonus"])
        anteil = self.A["Spieler"]["TurmAnteil"]
        self.ep_turm += p * anteil
        self.ep_rest += p * (1 - anteil)
        self.fp += int(B["Einsatz.FP.JeWelle"]) + (int(B["Einsatz.FP.KernAb75"]) if kern_prozent >= 75 else 0) + (
            int(B["Einsatz.FP.Boss"]) if n % 10 == 0 else 0)
        return p

    def plane(self, n=1):
        """Bauphase vor Welle n: forschen (Reihenfolge, Freigabewelle und Voraussetzungen), dann Tuerme oder Level kaufen"""
        # A6 (22.11.): Messgroessen fuer "Wahl" - offene Knoten (freigegeben, Voraussetzungen erfuellt, keine Gegenseite gewaehlt)
        # und davon mit den Festplatten zu Beginn der Bauphase bezahlbare; nach dem Forschen bleibt self.fp ungenutzt
        offen = [k for k, kn in self.info.items() if k not in self.erforscht and n >= kn["AbWelle"]
                 and all(v in self.erforscht for v in kn["Braucht"]) and not any(v in self.erforscht for v in kn["Andere"])]
        self.knoten_offen = len(offen)
        self.knoten_bezahlbar = sum(1 for k in offen if self.info[k]["FP"] <= self.fp)
        for name in self.reihenfolge:
            if name in self.erforscht:
                continue
            kn = self.info[name]
            if n < kn["AbWelle"] or any(v not in self.erforscht for v in kn["Braucht"]) or any(v in self.erforscht for v in kn["Andere"]):
                continue
            if self.fp < kn["FP"]:
                break
            self.fp -= kn["FP"]
            self.erforscht.add(name)
        self.welle = n
        typen = [t for t in self.gekauft if self.frei(self.A["Tuerme"][t]["Forschung"])]
        if self.mono:
            typen = [self.mono] if self.mono in typen else ["Bolzenturm"]
        while True:
            # Optionen: neuer Turm (Level 1) oder ein Level mehr fuer einen stehenden Turm; Wert = Nenn-DPS-Gewinn je Coupon
            if self.A["Spieler"]["Turmwahl"] == "gemischt":
                typ = min(typen, key=lambda t: (sum(1 for x in self.tuerme if x["Typ"] == t), self.preis(t)))
            else:
                typ = max(typen, key=self.wert_je_ep)
            optionen = [(self.wert_je_ep(typ), self.preis(typ), "Turm", typ)]
            for t in self.tuerme:
                if t["Level"] < self.level_max(t["Typ"]):
                    p = self.level_preis(t["Typ"], t["Level"] + 1)
                    g = self.dps(t, t["Level"] + 1) - self.dps(t)
                    optionen.append((g / max(p, 1), p, "Level", t))
            optionen = [o for o in optionen if o[1] <= self.ep_turm]
            if not optionen:
                break
            wert, p, art, ziel = max(optionen, key=lambda o: o[0])
            self.ep_turm -= p
            if art == "Turm":
                self.ep_ausgegeben += p
                self.gekauft[ziel] += 1
                self.tuerme.append({"Typ": ziel, "Level": 1})
            else:
                self.ep_level += p
                ziel["Level"] += 1


# ---------------------------------------------------------------- Simulation
def resistenz(B, art, name):
    f = familie(name)
    return B.get(f"Resistenzen.{f}.{art}", 1.0) if f else 1.0


def simuliere(B, A, katalog, phasen, ab_welle, stufe, laeufe, seed, mono=None):
    wellen = Wellen(B, katalog, phasen, A["Gegner_GrundLP"], stufe)
    info, level = lade_forschung_info(B), lade_level_standards()
    ref = A["Q"]["Referenzzeit"]
    N = A["Simulation"]["Wellen"]
    reihen = {n: [] for n in range(1, N + 1)}
    for lauf in range(laeufe):
        rng = random.Random(seed * 100003 + lauf)
        sp = Spieler(B, A, stufe, ab_welle, mono, info, level)
        for n in range(1, N + 1):
            sp.plane(n)
            gegner, elite = wellen.erzeuge(n, rng)
            lp_ges = sum(l for _, l in gegner) or 1.0
            # 0.73: jeder Turm laedt die Munition, die gegen diese Welle am meisten bewirkt (Art je Munition, sonst die des Turms)
            # LP je Gegnerart zusammengefasst; Resistenzmittel je Schadensart einmal je Welle (schneller, gleiches Ergebnis)
            lp_art = {}
            for name, l in gegner:
                lp_art[name] = lp_art.get(name, 0.0) + l
            res_mittel = {}

            def mittel(art):
                if art not in res_mittel:
                    res_mittel[art] = sum(l * resistenz(B, art, name) for name, l in lp_art.items()) / lp_ges
                return res_mittel[art]
            tuerme, k_art = [], {}
            for t in sp.tuerme:
                best = None
                for m in sp.munitionen(t["Typ"]) or [None]:
                    art = (m or {}).get("Art", A["Tuerme"][t["Typ"]]["Art"])
                    d = sp.dps(t, m=m)
                    wirk = d * mittel(art)
                    if best is None or wirk > best[0]:
                        best = (wirk, art, d)
                tuerme.append((best[1], best[2], t))
                k_art[best[1]] = k_art.get(best[1], 0.0) + best[2]
            K = sum(d for _, d, _ in tuerme)
            lp = sum(l for _, l in gegner)
            ehp = 0.0
            for name, l in lp_art.items():
                f = sum(d * resistenz(B, art, name) for art, d in k_art.items()) / K if K else 1.0
                ehp += l / max(f, 0.05)
            q = ehp / (K * ref) if K else float("inf")
            stufen = [t["Level"] for t in sp.tuerme]
            reihen[n].append({
                "Gegner": len(gegner), "Elite": elite, "LP": lp, "EHP": ehp, "K": K, "Q": q,
                "Tuerme": len(sp.tuerme), "Fronten": wellen.fronten(n),
                "Mix": "/".join(f"{sum(1 for t in sp.tuerme if t['Typ'] == typ)}" for typ in sp.gekauft),
                "Level": statistics.mean(stufen) if stufen else 0, "Coupons_Level": sp.ep_level,
                "Forschung": len(sp.erforscht), "EP_Turm_offen": sp.ep_turm,
                "FP_ungenutzt": sp.fp, "Knoten_offen": sp.knoten_offen, "Knoten_bezahlbar": sp.knoten_bezahlbar,
            })
            sp.welle_beendet(n, A["Spieler"]["KernAnteilProzent"])
    return reihen


def zusammenfassen(reihen):
    aus = []
    for n, rs in reihen.items():
        qs = sorted(r["Q"] for r in rs)
        z = {"Welle": n}
        for k in ("Gegner", "Elite", "LP", "EHP", "K", "Tuerme", "Fronten", "Level", "Coupons_Level", "Forschung", "EP_Turm_offen",
                  "FP_ungenutzt", "Knoten_offen", "Knoten_bezahlbar"):
            z[k] = statistics.mean(r[k] for r in rs)
        z["Q"] = statistics.median(qs)
        z["Q_p10"] = qs[int(0.1 * (len(qs) - 1))]
        z["Q_p90"] = qs[int(0.9 * (len(qs) - 1))]
        z["Mix"] = max(set(r["Mix"] for r in rs), key=[r["Mix"] for r in rs].count)
        aus.append(z)
    return aus


def steigung(xs, ys):
    """Exponent b in y ~ x^b (Kleinste Quadrate in log-log)"""
    pts = [(math.log(x), math.log(y)) for x, y in zip(xs, ys) if x > 0 and y > 0]
    mx = statistics.mean(p[0] for p in pts)
    my = statistics.mean(p[1] for p in pts)
    return sum((a - mx) * (b - my) for a, b in pts) / sum((a - mx) ** 2 for a, _ in pts)


# ---------------------------------------------------------------- Abgleich mit dem Spiel
def gemessene_wellengroessen():
    """Ansagen 'WELLE n/m (Art): k Gegner' aus allen Spiel-Logs (Einsatz Normal, 30 Wellen)"""
    werte = {}
    for p in glob.glob(os.path.join(SAVED, "Logs", "*.log")):
        for z in open(p, encoding="utf-8", errors="ignore"):
            m = re.search(r"Ansage: WELLE (\d+)/\d+ \(\w+\): (\d+) Gegner", z)
            if m:
                werte.setdefault(int(m.group(1)), []).append(int(m.group(2)))
    return werte


def gemessene_metriken():
    p = os.path.join(SAVED, "FicsitTD", "Metriken.csv")
    if not os.path.exists(p):
        return []
    with open(p, encoding="utf-8") as f:
        return list(csv.DictReader(f, delimiter=";"))


# ---------------------------------------------------------------- Ausgabe
def svg_linien(reihen, titel, y_name, log=False, baender=None, punkte=None, breite=760, hoehe=300):
    """reihen: [(Name, Farbe, [(x, y)], gestrichelt)]"""
    L, R, O, U = 58, 16, 30, 36
    xs = [x for _, _, d, _ in reihen for x, _ in d]
    ys = [y for _, _, d, _ in reihen for _, y in d if y > 0 and math.isfinite(y)]
    if baender:
        ys += [v for _, _, a, b in baender for v in (a, b)]
    if punkte:
        ys += [y for _, _, d in punkte for _, y in d if y > 0]
    x0, x1 = min(xs), max(xs)
    x1 = x1 if x1 > x0 else x0 + 1   # nur ein Punkt (kurzer Lauf)
    if log:
        y0, y1 = 10 ** math.floor(math.log10(min(ys))), 10 ** math.ceil(math.log10(max(ys)))
        ty = lambda y: O + (hoehe - O - U) * (1 - (math.log10(y) - math.log10(y0)) / (math.log10(y1) - math.log10(y0)))
        ticks = [10 ** e for e in range(int(math.log10(y0)), int(math.log10(y1)) + 1)]
    else:
        y0, y1 = 0, max(ys) * 1.08
        ty = lambda y: O + (hoehe - O - U) * (1 - (y - y0) / (y1 - y0))
        schritt = 10 ** math.floor(math.log10(y1 / 4))
        schritt *= 2 if y1 / schritt > 10 else 1
        ticks = [i * schritt for i in range(int(y1 / schritt) + 1)]
    tx = lambda x: L + (breite - L - R) * (x - x0) / (x1 - x0)
    s = [f'<svg viewBox="0 0 {breite} {hoehe}" role="img" aria-label="{titel}">',
         f'<text x="{L}" y="18" class="t">{titel}</text>']
    for t in ticks:
        s.append(f'<line x1="{L}" x2="{breite - R}" y1="{ty(t):.1f}" y2="{ty(t):.1f}" class="g"/>'
                 f'<text x="{L - 6}" y="{ty(t) + 4:.1f}" class="a" text-anchor="end">{t:g}</text>')
    for x in range(int(x0), int(x1) + 1):
        if x % 5 == 0 or x == x0:
            s.append(f'<text x="{tx(x):.1f}" y="{hoehe - U + 16}" class="a" text-anchor="middle">{x}</text>')
    s.append(f'<text x="{breite - R}" y="{hoehe - 4}" class="a" text-anchor="end">Welle</text>'
             f'<text x="12" y="{O - 8}" class="a">{y_name}</text>')
    for name, farbe, a, b in baender or []:
        s.append(f'<rect x="{L}" y="{ty(b):.1f}" width="{breite - L - R}" height="{ty(a) - ty(b):.1f}" fill="{farbe}" opacity="0.10"/>')
    for name, farbe, d, gestr in reihen:
        pts = " ".join(f"{tx(x):.1f},{ty(y):.1f}" for x, y in d if y > 0 and math.isfinite(y))
        s.append(f'<polyline points="{pts}" fill="none" stroke="{farbe}" stroke-width="2"'
                 f'{" stroke-dasharray=\"5 4\"" if gestr else ""}><title>{name}</title></polyline>')
    for name, farbe, d in punkte or []:
        for x, y in d:
            if y > 0:
                s.append(f'<circle cx="{tx(x):.1f}" cy="{ty(y):.1f}" r="4" fill="{farbe}"><title>{name}: Welle {x}, {y:.2f}</title></circle>')
    # Legende
    lx = L + 8
    for name, farbe, _, gestr in reihen:
        s.append(f'<line x1="{lx}" x2="{lx + 18}" y1="{O + 8}" y2="{O + 8}" stroke="{farbe}" stroke-width="2"'
                 f'{" stroke-dasharray=\"5 4\"" if gestr else ""}/><text x="{lx + 22}" y="{O + 12}" class="a">{name}</text>')
        lx += 30 + 7 * len(name)
    for name, farbe, _ in punkte or []:
        s.append(f'<circle cx="{lx + 6}" cy="{O + 8}" r="4" fill="{farbe}"/><text x="{lx + 14}" y="{O + 12}" class="a">{name}</text>')
        lx += 22 + 7 * len(name)
    s.append("</svg>")
    return "\n".join(s)


def html_bericht(ergebnisse, B, A, quelle, gemessen, metriken, exponenten, wert_tabelle):
    jetzt = datetime.datetime.now().strftime("%d.%m.%Y %H:%M")
    q_reihen = [(st, FARBEN[st], [(z["Welle"], z["Q"]) for z in ergebnisse[st]], False) for st in ergebnisse]
    baender = [(st, FARBEN[st], *A["Zielbaender_Q"][st]) for st in ergebnisse if st in A["Zielbaender_Q"]]
    punkte = []
    if metriken:
        punkte.append(("gemessen", "#222", [(int(m["Welle"]), float(m["Q"])) for m in metriken]))
    q_svg = svg_linien(q_reihen, "Druckquote Q je Welle (Median), Baender = Zielbereich", "Q", baender=baender, punkte=punkte)
    n = ergebnisse.get("Normal") or next(iter(ergebnisse.values()))
    kurve_svg = svg_linien([
        ("Gegner-EHP (Normal)", "#B23A48", [(z["Welle"], z["EHP"]) for z in n], False),
        ("K x 60 s (Normal)", "#3A6EA5", [(z["Welle"], z["K"] * A["Q"]["Referenzzeit"]) for z in n], False),
    ], "Gegnerdruck gegen Abwehrkapazitaet (logarithmisch)", "LP", log=True)

    zeilen = []
    for st, zs in ergebnisse.items():
        lo, hi = A["Zielbaender_Q"].get(st, [0, 9])
        for z in zs:
            klasse = "ok" if lo <= z["Q"] <= hi else ("hoch" if z["Q"] > hi else "tief")
            zeilen.append(f'<tr data-s="{st}"><td>{st}</td><td>{z["Welle"]}</td><td>{z["Gegner"]:.1f}</td><td>{z["Elite"]:.1f}</td>'
                          f'<td>{z["Fronten"]:.0f}</td><td>{z["LP"]:.0f}</td><td>{z["EHP"]:.0f}</td><td>{z["Tuerme"]:.1f}</td>'
                          f'<td>{z["Mix"]}</td><td>{z["Level"]:.1f}</td><td>{z["K"]:.0f}</td>'
                          f'<td class="{klasse}">{z["Q"]:.2f}</td><td>{z["Q_p10"]:.2f}–{z["Q_p90"]:.2f}</td></tr>')
    vergleich = []
    norm = {z["Welle"]: z for z in ergebnisse.get("Normal", [])}
    for w in sorted(gemessen):
        if w in norm:
            vergleich.append(f'<tr><td>{w}</td><td>{", ".join(map(str, gemessen[w]))}</td><td>{norm[w]["Gegner"]:.1f}</td></tr>')
    exp_zeilen = "".join(f"<tr><td>{st}</td><td>{e[0]:.2f}</td><td>{e[1]:.2f}</td><td>{e[2]:.2f}</td></tr>" for st, e in exponenten.items())
    wert_zeilen = "".join(f"<tr><td>{a}</td><td>{b}</td><td>{c:.0f}</td><td>{d}</td><td>{e:.1f}</td></tr>" for a, b, c, d, e in wert_tabelle)
    typen = "/".join(t for t in A["Tuerme"] if not t.startswith("_"))
    return f"""<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>FicsitTD Simulation</title>
<style>
:root {{ --bg:#fbfaf7; --fg:#1d1d1f; --mut:#6b6b70; --rand:#e2e0da; --ok:#2f7d4f; --hoch:#b23a48; --tief:#8a6d1f; }}
@media (prefers-color-scheme: dark) {{ :root {{ --bg:#18181a; --fg:#ececee; --mut:#a0a0a8; --rand:#34343a; --ok:#6cc08b; --hoch:#ef7a86; --tief:#d9b45a; }} }}
body {{ background:var(--bg); color:var(--fg); font:15px/1.5 system-ui,Segoe UI,sans-serif; margin:0 auto; max-width:1000px; padding:24px 16px; }}
h1 {{ font-size:24px; margin:0 0 4px; }} h2 {{ font-size:18px; margin:28px 0 8px; }}
.m {{ color:var(--mut); font-size:13px; }}
svg {{ width:100%; height:auto; display:block; margin:8px 0; }}
svg .g {{ stroke:var(--rand); }} svg .a {{ fill:var(--mut); font-size:11px; }} svg .t {{ fill:var(--fg); font-size:13px; font-weight:600; }}
table {{ border-collapse:collapse; width:100%; font-size:13px; }} th,td {{ border-bottom:1px solid var(--rand); padding:4px 6px; text-align:right; }}
th:first-child,td:first-child {{ text-align:left; }} .ok {{ color:var(--ok); }} .hoch {{ color:var(--hoch); font-weight:600; }} .tief {{ color:var(--tief); }}
.scroll {{ overflow-x:auto; }} select {{ font:inherit; }}
</style></head><body>
<h1>FicsitTD Simulation</h1>
<p class="m">Erzeugt {jetzt} · {quelle} · {A["Simulation"]["Durchlaeufe"]} Durchläufe je Stufe · Modell-Spieler: {int(100 * A["Spieler"]["TurmAnteil"])} % der Coupons in Türme und Level ({A["Spieler"]["Turmwahl"]}), Kern {A["Spieler"]["KernAnteilProzent"]} % · Q = EHP / (Σ Nenn-DPS × {A["Q"]["Referenzzeit"]} s)</p>
{q_svg}
{kurve_svg}
<h2>Wachstum (Exponent b in y ~ Welle<sup>b</sup>, ab Welle 5)</h2>
<table><tr><th>Stufe</th><th>Gegner-EHP</th><th>Abwehr K</th><th>Q</th></tr>{exp_zeilen}</table>
<p class="m">b ≈ 1 linear, ≈ 2 quadratisch, ≈ 3 kubisch. Wächst EHP schneller als K, steigt Q.</p>
<h2>Wert je Coupon beim ersten Kauf (Startmunition, Level 1)</h2>
<table><tr><th>Turm</th><th>Munition</th><th>Nenn-DPS</th><th>Preis Coupons</th><th>DPS je Coupon</th></tr>{wert_zeilen}</table>
<h2>Abgleich Wellengröße mit dem Spiel (Normal)</h2>
<table><tr><th>Welle</th><th>Spiel (Logs)</th><th>Simulation (Mittel)</th></tr>{"".join(vergleich) or '<tr><td colspan="3">keine Ansagen in den Logs</td></tr>'}</table>
<h2>Je Welle</h2>
<p class="m">Mix = Anzahl {typen}. Q-Farbe: grün im Zielband, rot darüber, gelb darunter. Level = mittleres Turmlevel (1–5, mit Coupons gekauft).
<label>Stufe <select id="f"><option value="">alle</option>{"".join(f"<option>{s}</option>" for s in ergebnisse)}</select></label></p>
<div class="scroll"><table><thead><tr><th>Stufe</th><th>Welle</th><th>Gegner</th><th>Elite</th><th>Fronten</th><th>LP</th><th>EHP</th><th>Türme</th><th>Mix</th><th>Level</th><th>K DPS</th><th>Q</th><th>Q p10–p90</th></tr></thead>
<tbody>{"".join(zeilen)}</tbody></table></div>
<script>
document.getElementById('f').addEventListener('change', e => {{
  for (const r of document.querySelectorAll('tbody tr')) r.style.display = !e.target.value || r.dataset.s === e.target.value ? '' : 'none';
}});
</script>
</body></html>"""


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--stufe", choices=STUFEN, action="append", help="nur diese Stufe(n), Standard alle")
    ap.add_argument("--balance", default=os.path.join(SAVED, "FicsitTD", "Balance.json"))
    ap.add_argument("--annahmen", default=os.path.join(HIER, "annahmen.json"))
    ap.add_argument("--aus", default=os.path.join(HIER, "Ausgabe"))
    ap.add_argument("--turmanteil", type=float, help="Anteil der EP fuer Tuerme (ueberschreibt annahmen.json)")
    ap.add_argument("--turmwahl", choices=["bester_wert", "gemischt"])
    ap.add_argument("--mono", action="store_true", help="0.73: zusaetzlich jede Turmlinie allein (mit Doktrin) gegen den Mischaufbau rechnen")
    args = ap.parse_args()

    B, quelle = lade_balance(args.balance)
    A = json.load(open(args.annahmen, encoding="utf-8"))
    if args.turmanteil is not None:
        A["Spieler"]["TurmAnteil"] = args.turmanteil
    if args.turmwahl:
        A["Spieler"]["Turmwahl"] = args.turmwahl
    A["Tuerme"] = {k: v for k, v in A["Tuerme"].items() if not k.startswith("_")}
    A["Munition"] = {k: v for k, v in A["Munition"].items() if not k.startswith("_")}
    katalog, phasen = lade_katalog(B)
    ab_welle = lade_forschung_ab_welle(B)
    fehlend = [e["Name"] for e in katalog if e["Name"] not in A["Gegner_GrundLP"]] + [
        p["Boss"] for p in phasen if p["Boss"] not in A["Gegner_GrundLP"]]
    if fehlend:
        raise SystemExit(f"Grund-LP fehlt in annahmen.json fuer: {fehlend}")

    ergebnisse, exponenten = {}, {}
    for st in args.stufe or STUFEN:
        reihen = simuliere(B, A, katalog, phasen, ab_welle, st, A["Simulation"]["Durchlaeufe"], A["Simulation"]["Seed"])
        z = zusammenfassen(reihen)
        ergebnisse[st] = z
        ab5 = [r for r in z if r["Welle"] >= 5]
        exponenten[st] = (steigung([r["Welle"] for r in ab5], [r["EHP"] for r in ab5]),
                          steigung([r["Welle"] for r in ab5], [r["K"] for r in ab5]),
                          steigung([r["Welle"] for r in ab5], [r["Q"] for r in ab5]))

    sp = Spieler(B, A, "Normal", ab_welle)
    wert = []
    for typ in A["Tuerme"]:
        m = A["Munition"][typ][0]
        d = sp.dps({"Typ": typ, "Level": 1}, m=m)
        wert.append((typ, m["Name"], d, sp.preis(typ), d / max(sp.preis(typ), 1)))

    os.makedirs(args.aus, exist_ok=True)
    with open(os.path.join(args.aus, "Simulation.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter=";")
        spalten = ["Welle", "Gegner", "Elite", "Fronten", "LP", "EHP", "Tuerme", "Mix", "Level", "Forschung", "K", "Q", "Q_p10", "Q_p90", "FP_ungenutzt", "Knoten_offen", "Knoten_bezahlbar"]
        w.writerow(["Stufe"] + spalten)
        for st, zs in ergebnisse.items():
            for z in zs:
                w.writerow([st] + [f"{z[s]:.3f}" if isinstance(z[s], float) else z[s] for s in spalten])
    html = html_bericht(ergebnisse, B, A, quelle, gemessene_wellengroessen(), gemessene_metriken(), exponenten, wert)
    open(os.path.join(args.aus, "Simulation.html"), "w", encoding="utf-8").write(html)

    # Kurzfassung auf der Konsole
    print(quelle)
    for st, zs in ergebnisse.items():
        lo, hi = A["Zielbaender_Q"].get(st, [0, 9])
        ueber = [z["Welle"] for z in zs if z["Q"] > hi]
        e = exponenten[st]
        qs = " ".join(f"W{w} {zs[w - 1]['Q']:.2f}" for w in (1, 10, 20, 30, 40, 50) if w <= len(zs))
        print(f"{st:10s} EHP~W^{e[0]:.2f}  K~W^{e[1]:.2f}  Q {qs}"
              f"  ueber Band ({hi}): {ueber[0] if ueber else '-'}{'+' if ueber else ''}")
    print("Ausgabe:", os.path.join(args.aus, "Simulation.html"))

    if args.mono:
        # Konzept-Spezialisierung 9: jede Linie allein, Ziel Q <= 1,1 an den Bossen; Mischaufbau zum Vergleich
        zeilen = []
        for st in args.stufe or ["Normal"]:
            for linie in [None] + list(A["Tuerme"]):
                z = zusammenfassen(simuliere(B, A, katalog, phasen, ab_welle, st, max(20, A["Simulation"]["Durchlaeufe"] // 10), A["Simulation"]["Seed"], linie))
                q = {w: z[w - 1]["Q"] for w in (10, 20, 30, 40, 50) if w <= len(z)}
                zeilen.append((st, linie or "Mischaufbau", q))
                print(f"{st:8s} {linie or 'Mischaufbau':16s} " + " ".join(f"W{w} {v:.2f}" for w, v in q.items())
                      + ("  ZU SCHWER" if max(q.values()) > 1.1 else ""))
        with open(os.path.join(args.aus, "Mono.csv"), "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f, delimiter=";")
            w.writerow(["Stufe", "Linie", "Q_W10", "Q_W20", "Q_W30", "Q_W40", "Q_W50"])
            for st, linie, q in zeilen:
                w.writerow([st, linie] + [f"{q.get(x, 0):.3f}" for x in (10, 20, 30, 40, 50)])


if __name__ == "__main__":
    main()
