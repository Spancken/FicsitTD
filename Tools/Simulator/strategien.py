"""FicsitTD Strategie-Prüfstand: viele Spielweisen automatisch durchspielen (Wirtschaft + Forschung + Kampf).

  python strategien.py                 alle Strategien x Startwerte, Normal 50 Wellen -> Ausgabe/Strategien.html, .csv
  python strategien.py --laeufe 3      Startwerte je Strategie
  python strategien.py --stufe Hart
  python strategien.py --stand 091     mit den Endgame-Paketen E1-E3 von 0.9.1 -> Ausgabe/Strategien_Normal_091.*
  python endgame_vergleich.py          090 und 091 im selben Lauf -> Ausgabe/Endgame_091.json

Ziel (Marius 06.11.2026): Es soll mehrere erfolgreiche Wege geben, aber nicht alles darf funktionieren und es darf nicht
zu leicht sein. Der Prüfstand zeigt je Strategie Erfolg, Scheiterwelle und Ursache; danach werden EP, FP, Beute und
Preise gezielt gedreht (balance_sweep.py).

Modell (Annahmen in strategie_annahmen.json, alles andere aus dem Mod-Code):
- Wellen: wie simulator.Wellen (Nachbau von PlanFuer), Grund-LP aus annahmen.json; kalibriert LP 0,93, K 1,00, Q 0,90.
- Türme: Nenn-DPS wie im Spiel (Munition, Level-Boni Tuerme.Level.*, Doktrin), Resistenzen je Gegnerfamilie.
- Wirtschaft: Munitionsbedarf je Welle -> Fabrik bis zum Erz (wie Werkbilanz) -> Kontingente mit Ladenpreisen; Strom.
- Spieler: Strategie = Turmlinie(n), Doktrin, Gabelungen, Ausgabestil, Kern-Ausbau. Forscht nach Bedarf, kauft
  Fabrik-Kontingente vor Türmen, dann Türme oder Level nach Stil.
- Kampf: Druckquote Q = EHP / (K * 60 s). Kernverlust je Welle aus Q (Annahme, Messdaten zeigen nur schwachen
  Zusammenhang), mit seltenen Ausreißern; Reparatur nach der Welle.
"""
import argparse
import collections
import csv
import itertools
import json
import math
import os
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # Repo-Wurzel (Tools/Simulator/..)
import random
import statistics
import sys
import unittest.mock

import simulator as sim

HIER = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = os.path.join(REPO, "Scripts")
TURM_TYPEN = ["Bolzenturm", "Gewehrturm", "Nobelisk-Werfer", "Teslaspule", "Flammenwerfer"]
TURM_FORSCHUNG = {"Bolzenturm": "", "Gewehrturm": "Gewehrturm", "Nobelisk-Werfer": "NobeliskWerfer", "Teslaspule": "Teslaspule", "Flammenwerfer": "Flammenwerfer"}
ART = {"Bolzenturm": "Durchschlag", "Gewehrturm": "Kugel", "Teslaspule": "Strom", "Flammenwerfer": "Feuer", "Nobelisk-Werfer": "Explosion"}
ZYKLUS = {"Bolzenturm": 1.2, "Teslaspule": 4.0, "Nobelisk-Werfer": 4.0}

# Munition je Turm: (Name, Balance-Schlüssel, Forschung, Schadensart oder None, Wirtschafts-Gegenstand, Verbrauch/min bei Dauerfeuer)
MUNITION = {
    "Bolzenturm": [("Armierungsbolzen", "Bolzen.Armierungsbolzen", "", None, "Armierungsbolzen", 50),
                   ("Durchschlagbolzen", "Bolzen.Durchschlagbolzen", "Durchschlagbolzen", None, "Durchschlagbolzen", 50),
                   # 0.82: Gabel A Schock (Caterium) / Streu (Quarz) auf Stufe 2, Explosiv Stufe 3 ab W25
                   ("Schockbolzen", "Bolzen.Betaeubungsbolzen", "Schockbolzen", "Strom", "Schockbolzen", 50),
                   ("Streubolzen", "Bolzen.Streubolzen", "Streubolzen", None, "Streubolzen", 50),
                   ("Explosivbolzen", "Bolzen.Explosivbolzen", "Explosivbolzen", "Explosion", "Explosivbolzen", 50)],
    "Gewehrturm": [("Standard", "Patronen.Standard", "", None, "Gewehrmunition", 24),
                   ("Zielsuch", "Patronen.Zielsuch", "Zielsuchmunition", None, "Zielsuchmunition", 15),
                   ("Panzerbrechend", "Patronen.Panzerbrechend", "Panzerbrechend", "Durchschlag", "Panzermunition", 24),
                   ("Brand", "Patronen.Brand", "Brandmunition", None, "Brandmunition", 24),
                   ("Turbo", "Patronen.Turbo", "Turbomunition", None, "Turbomunition", 9.6)],
    "Nobelisk-Werfer": [("Nobelisk", "Nobelisk.Nobelisk", "", None, "Nobelisk", 15),
                        ("Gas", "Nobelisk.Gas", "GasNobelisk", "Gift", "Gas-Nobelisk", 15),
                        ("Puls", "Nobelisk.Puls", "PulsNobelisk", None, "Puls-Nobelisk", 15),
                        ("Streu", "Nobelisk.Streu", "StreuNobelisk", None, "Streu-Nobelisk", 15)],
    "Teslaspule": [("Entladung", "Tesla", "", None, None, 0)],
    # 0.82 (Marius 07.11.): Flamme startet mit Treibstoff aus Oel, kein Biotreibstoff
    "Flammenwerfer": [("Treibstoff", "Treibstoff.Treibstoff", "", None, "Treibstoff", 6),
                      ("Turbotreibstoff", "Treibstoff.Turbotreibstoff", "Turbotreibstoff", None, "Turbotreibstoff", 4),
                      # 0.9.1 E2: Verbrauch aus create_turret_recipes.py (Flame_Rocket 67 l/s = 4 m³/min, Flame_Ionized 50 l/s = 3 m³/min).
                      # Die Forschungsknoten gibt es erst im Baum von 0.9.1, mit --stand 090 bleiben sie gesperrt.
                      ("Raketentreibstoff", "Treibstoff.Raketentreibstoff", "Raketentreibstoff", None, "Raketentreibstoff", 4),
                      ("Ionisiert", "Treibstoff.Ionisiert", "Plasmaflamme", None, "Ionisierter Treibstoff", 3)],
}
# 17.11. (P10 Munition je Item): Rezept je Munition fuer den Balance-Schluessel Munition.SchussJeItem.<Rezept> (Aktionen je Item,
# Gewehr: Magazine je Item). Der Verbrauch oben gilt fuer 1 Aktion je Item und wird durch diesen Wert geteilt.
SCHUSS_REZEPT = {
    ("Bolzenturm", "Armierungsbolzen"): "Shoot_SpikedRebar", ("Bolzenturm", "Durchschlagbolzen"): "Shoot_Durchschlag",
    ("Bolzenturm", "Schockbolzen"): "Shoot_Stunshot", ("Bolzenturm", "Streubolzen"): "Shoot_Spreadshot", ("Bolzenturm", "Explosivbolzen"): "Shoot_Explosive",
    ("Gewehrturm", "Standard"): "Gun_Standard", ("Gewehrturm", "Zielsuch"): "Gun_Homing", ("Gewehrturm", "Panzerbrechend"): "Gun_Panzer",
    ("Gewehrturm", "Brand"): "Gun_Brand", ("Gewehrturm", "Turbo"): "Gun_Turbo",
    ("Nobelisk-Werfer", "Nobelisk"): "Nobelisk_Explosive", ("Nobelisk-Werfer", "Gas"): "Nobelisk_Gas",
    ("Nobelisk-Werfer", "Puls"): "Nobelisk_Pulse", ("Nobelisk-Werfer", "Streu"): "Nobelisk_Cluster",
}
# Rezepte wie Werkbilanz: Gebäude, Dauer s, Zutaten, Produkt (Flüssigkeiten in m³)
REZEPTE = {
    "Eisenbarren": ("Schmelzofen", 2, {"Eisenerz": 1}, 1), "Kupferbarren": ("Schmelzofen", 2, {"Kupfererz": 1}, 1),
    "Eisenplatte": ("Konstruktor", 6, {"Eisenbarren": 3}, 2), "Eisenstange": ("Konstruktor", 4, {"Eisenbarren": 1}, 1),
    "Schraube": ("Konstruktor", 6, {"Eisenstange": 1}, 4), "Draht": ("Konstruktor", 4, {"Kupferbarren": 1}, 2),
    "Kabel": ("Konstruktor", 2, {"Draht": 2}, 1), "Kupferblech": ("Konstruktor", 6, {"Kupferbarren": 2}, 1),
    "Stahlbarren": ("Giesserei", 4, {"Eisenerz": 3, "Kohle": 3}, 3), "Stahlrohr": ("Konstruktor", 6, {"Stahlbarren": 3}, 2),
    # 17.11. (P12-Bericht F9): wie im Spiel (Recipe_SpikedRebar) ueber die Eisenstange - zwei Konstrukteure je 15 Bolzen/min
    "Armierungsbolzen": ("Konstruktor", 4, {"Eisenstange": 1}, 1),
    "Durchschlagbolzen": ("Assembler", 8, {"Armierungsbolzen": 2, "Schraube": 4}, 2),
    # 0.82 Munitionsstufen: Nebenzutat aus dem Originalrezept (Quarz, Caterium, Schwefel), Stufe 3 mit rauchschwachem Pulver/Aluminium
    "Quarzkristall": ("Konstruktor", 8, {"Rohquarz": 5}, 3),
    "Caterium-Barren": ("Schmelzofen", 4, {"Caterium-Erz": 3}, 1),
    "Turbodraht": ("Konstruktor", 5, {"Caterium-Barren": 1}, 5),
    "Schwerölrückstand": ("Raffinerie", 6, {"Rohöl": 3}, 4),
    "Rauchschwaches Pulver": ("Raffinerie", 6, {"Schwarzpulver": 2, "Schwerölrückstand": 1}, 2),
    "Aluminiumbarren": ("Giesserei", 4, {"Bauxit": 2, "Kohle": 1}, 1),
    "Aluminiumgehäuse": ("Konstruktor", 2, {"Aluminiumbarren": 3}, 2),
    "Explosivbolzen": ("Assembler", 8, {"Durchschlagbolzen": 2, "Rauchschwaches Pulver": 1}, 2),
    "Streubolzen": ("Assembler", 8, {"Durchschlagbolzen": 2, "Quarzkristall": 1}, 2),
    "Schockbolzen": ("Assembler", 8, {"Durchschlagbolzen": 2, "Turbodraht": 2}, 2),
    "Gewehrmunition": ("Assembler", 4, {"Kupferblech": 1, "Eisenstange": 1}, 1),
    "Zielsuchmunition": ("Assembler", 6, {"Gewehrmunition": 1, "Turbodraht": 1}, 1),
    "Turbomunition": ("Assembler", 8, {"Gewehrmunition": 2, "Aluminiumgehäuse": 1}, 2),
    "Panzermunition": ("Assembler", 8, {"Gewehrmunition": 2, "Stahlrohr": 1}, 2),
    "Brandmunition": ("Assembler", 8, {"Gewehrmunition": 2, "Schwefel": 1}, 2),
    "Schwarzpulver": ("Assembler", 4, {"Kohle": 1, "Schwefel": 1}, 2),
    "Nobelisk": ("Assembler", 6, {"Schwarzpulver": 2, "Stahlrohr": 1}, 1),
    "Gas-Nobelisk": ("Assembler", 6, {"Nobelisk": 1, "Schwefel": 2}, 1),
    "Puls-Nobelisk": ("Assembler", 6, {"Nobelisk": 1, "Quarzkristall": 2}, 1),
    "Streu-Nobelisk": ("Assembler", 8, {"Nobelisk": 2, "Rauchschwaches Pulver": 2}, 1),
    "Treibstoff": ("Raffinerie", 6, {"Rohöl": 6}, 4),
    "Kompaktkohle": ("Assembler", 12, {"Kohle": 5, "Schwefel": 5}, 5),
    "Turbotreibstoff": ("Raffinerie", 16, {"Treibstoff": 6, "Kompaktkohle": 4}, 5),
    # 0.9.1 E2, Mengen aus dem Spiel-Datenexport (CommunityResources/Docs/en-US.json): Recipe_RocketFuel, Recipe_NitricAcid,
    # Recipe_IonizedFuel. Nebenprodukt Kompaktkohle (1 bzw. 2 je Takt) wird nicht verwertet.
    "Raketentreibstoff": ("Mischer", 6, {"Turbotreibstoff": 6, "Salpetersäure": 1}, 10),
    "Salpetersäure": ("Mischer", 6, {"Stickstoff": 12, "Wasser": 3, "Eisenplatte": 1}, 3),
    "Ionisierter Treibstoff": ("Raffinerie", 24, {"Raketentreibstoff": 16, "Energiesplitter": 1}, 16),
}
# Munitionsvorrat je Turm (Eingangsslot = Stapelgröße, de.json: SS_SMALL 50, SS_MEDIUM 100, SS_HUGE 500; Munitionspuffer x2, TDTurretBase.cpp:989)
# Flammenwerfer: Rohr-Eingang, Puffer nicht gemessen (Annahme FlammePuffer in strategie_annahmen.json)
STAPEL = {"Armierungsbolzen": 100, "Durchschlagbolzen": 100, "Explosivbolzen": 100, "Streubolzen": 100, "Schockbolzen": 100,
          "Gewehrmunition": 500, "Zielsuchmunition": 500, "Panzermunition": 500, "Brandmunition": 500, "Turbomunition": 500,
          "Nobelisk": 50, "Gas-Nobelisk": 50, "Puls-Nobelisk": 50, "Streu-Nobelisk": 50}
# Strom beim Feuern je Turmtyp (Scripts/setup_turret_blueprint.py), Teslaspule getrennt (Grund + Laden)
TURM_STROM = {"Bolzenturm": 8, "Gewehrturm": 12, "Flammenwerfer": 6, "Nobelisk-Werfer": 20}


def zyklus_min(Z, n, D):
    """Minuten Fabriklaufzeit je Welle: Bauphase des Spielers + Welle. Im Einsatz ist die Bauphase unbegrenzt
    (Welle startet von Hand), deshalb zählt die tatsächlich gewartete Zeit (Live-Messung 03./04.11.: Median 195 s)"""
    return ((Z["BauphaseErsteSekunden"] if n == 1 else Z["BauphaseSekunden"]) + D) / 60


def feueranteil(Z, n):
    """Anteil der Wellenzeit mit Ziel (Uptime, Kreislauf-Messung 06.11.) je Wellenbereich"""
    for bis, f in Z["FeueranteilJeWelle"]:
        if n <= bis:
            return f
    return Z["FeueranteilJeWelle"][-1][1]


ROH = {"Rohquarz": "Miner", "Caterium-Erz": "Miner", "Schwefel": "Miner", "Bauxit": "Miner", "Eisenerz": "Miner", "Kupfererz": "Miner", "Kohle": "Miner", "Rohöl": "Oelpumpe", "Wasser": "Wasserpumpe", "Biomasse": None,
       # 0.9.1: Stickstoff aus dem Ressourcenbrunnen (Laden "Foerderanlage", Paket 3), Energiesplitter nur als Beute (Lauf.splitter)
       "Stickstoff": "Foerderanlage", "Energiesplitter": None}
GEB_STROM = {"Miner": 5, "Schmelzofen": 4, "Konstruktor": 4, "Assembler": 15, "Giesserei": 16, "Raffinerie": 30, "Oelpumpe": 40, "Wasserpumpe": 20,
             "Mischer": 75, "Fertiger": 55, "Foerderanlage": 0}
GEB_FORSCHUNG = {"Assembler": "Assembler", "Giesserei": "Giesserei", "Raffinerie": "Raffinerie", "Oelpumpe": "Raffinerie",
                 "Kohlekraftwerk": "Kohlekraft", "Wasserpumpe": "Kohlekraft", "Treibstoffkraftwerk": "Treibstoffkraft",
                 "Fertiger": "Schwerindustrie", "Mischer": "Mischtechnik", "Foerderanlage": "Mischtechnik"}
# 0.9.1 E3 Stützbauten (Kombos): Turmtyp -> (Bau im Laden, Forschung, Kombo)
STUETZBAU = {"Bolzenturm": ("Tor", "Tor", "Engstelle"), "Nobelisk-Werfer": ("Lockfeuer", "Lockfeuer", "Sammelpunkt"),
             "Teslaspule": ("Elektrozaun", "Elektrozaun", "Leiter"), "Flammenwerfer": ("Oelschlick", "Oelschlick", "Oelbrand")}
BAU_STROM = {"Tor": 0, "Lockfeuer": 5, "Elektrozaun": 2, "Oelschlick": 2}   # Beschreibungen in create_research.py
NEU_091 = ["Schwerindustrie", "Mischtechnik", "Raketentreibstoff", "Plasmaflamme", "Hochspannung"]
ITEM_FORSCHUNG = {"Kupferbarren": "Kupfer", "Stahlbarren": "Giesserei"}


def lade_forschung(stand="090"):
    """Knoten, Preise, Freischaltwellen, Voraussetzungen und Gabelungen direkt aus create_research.py.
    stand "091": aktuelle Datei. stand "090": Baum von 0.9.0 aus dem Mod-Git (Commit HEAD, nur lesen); geht das nicht,
    werden die Knoten von 0.9.1 entfernt und die alte Gabel Ölschlick/Elektrozaun wieder gesetzt."""
    sys.modules.setdefault("unreal", unittest.mock.MagicMock())
    src = None
    if stand == "090":
        import subprocess
        try:
            src = subprocess.run(["git", "-C", os.path.dirname(SCRIPTS), "show", "HEAD:Scripts/create_research.py"],
                                 capture_output=True, check=True, timeout=30).stdout.decode("utf-8")
            if "Mischtechnik" in src:   # HEAD ist schon 0.9.1 -> Rückfall unten
                src = None
        except Exception:
            src = None
    rueckfall = stand == "090" and src is None
    if src is None:
        src = open(os.path.join(SCRIPTS, "create_research.py"), encoding="utf-8").read()
    ns = {}
    exec(src[:src.find("\nNAMEN = ")], ns)
    knoten = {}
    for bb in ns["BAEUME"]:
        for kk in bb[4]:
            vor = list(kk[4]) + [vk for _, vk in ns["VORAUSSETZUNGEN"].get((bb[0], kk[0]), [])]
            knoten[kk[0]] = {"name": kk[1], "baum": bb[0], "ab": ns["eff_welle"](bb[0], kk[0]), "fp": ns["preis"](bb[0], kk[0]), "vor": vor}
    gruppen = [g for _, g in ns["GRUPPEN"]]
    if rueckfall:
        for k in NEU_091:
            knoten.pop(k, None)
        for kn in knoten.values():
            kn["vor"] = [v for v in kn["vor"] if v not in NEU_091]
        if ["Oelschlick", "Elektrozaun"] not in gruppen:
            gruppen.insert(4, ["Oelschlick", "Elektrozaun"])
    return knoten, gruppen


# ---------------------------------------------------------------- Wirtschaft
def kette(bedarf):
    """Bedarf je Minute -> Maschinen je Rezept (Bruchteile) und Rohstoffe je Minute"""
    masch, roh = collections.Counter(), collections.Counter()

    def brauche(item, rate):
        if rate <= 1e-9:
            return
        if item in ROH:
            roh[item] += rate
            return
        geb, t, zut, aus = REZEPTE[item]
        masch[item] += rate / (aus * 60 / t)
        for z, m in zut.items():
            brauche(z, rate * m / aus)
    for i, r in bedarf.items():
        brauche(i, r)
    return masch, roh


class Wirtschaft:
    """Nötige Gebäude für einen Munitionsbedarf, inklusive Strom und Kraftwerken; mit Zwischenspeicher"""
    def __init__(self, B, Z):
        self.B, self.Z, self.cache = B, Z, {}

    def gebaeude(self, bedarf, strom_tuerme, n, erforscht):
        key = (tuple(sorted((k, round(v, 2)) for k, v in bedarf.items())), round(strom_tuerme), "Kohlekraft" in erforscht, "Treibstoffkraft" in erforscht)
        if key in self.cache:
            return self.cache[key]
        Z = self.Z
        masch, roh_basis = kette(bedarf)
        geb = collections.Counter()
        for item, x in masch.items():
            geb[REZEPTE[item][0]] += math.ceil(x - 1e-9)
        kraft = collections.Counter()
        for _ in range(3):
            # Rohstoffe = Produktion + Brennstoff der Kraftwerke aus dem letzten Durchgang
            roh = collections.Counter(roh_basis)
            roh["Biomasse"] += kraft["Biomassegenerator"] * Z["BiomasseJeGenerator"]
            roh["Kohle"] += kraft["Kohlekraftwerk"] * 15
            roh["Wasser"] += kraft["Kohlekraftwerk"] * 45
            g = collections.Counter(geb)
            if kraft["Treibstoffkraftwerk"]:
                m2, r2 = kette({"Treibstoff": kraft["Treibstoffkraftwerk"] * 12})
                roh.update(r2)
                g["Raffinerie"] += math.ceil(m2["Treibstoff"] - 1e-9)
            g["Miner"] = sum(math.ceil(roh[r] / Z["MinerRate"] - 1e-9) for r, art in ROH.items() if art == "Miner")   # alle Erze (07.11.: Quarz, Caterium, Schwefel, Bauxit im Szenario)
            g["Oelpumpe"] = math.ceil(roh["Rohöl"] / 120 - 1e-9)
            g["Wasserpumpe"] = math.ceil(roh["Wasser"] / 120 - 1e-9)
            druck = 0.0
            if roh["Stickstoff"] > 1e-9:
                # 0.9.1: Kontingent "Foerderanlage" = Paket aus 3 Brunnen-Fördertürmen; der Druckerzeuger zählt nicht als Kontingent, braucht aber Strom
                g["Foerderanlage"] = math.ceil(roh["Stickstoff"] / (Z["StickstoffJeFoerderanlage"] * self.B.get("Einsatz.Laden.Foerderanlage.Paket", 3)) - 1e-9)
                druck = Z["DruckerzeugerStrom"]
            last = sum(z * GEB_STROM.get(k, 0) for k, z in g.items()) + strom_tuerme + druck
            kraft = collections.Counter()
            rest = last
            if Z["TreibstoffkraftNutzen"] and "Treibstoffkraft" in erforscht and rest > 150:
                kraft["Treibstoffkraftwerk"] = math.ceil(rest / 150)
            elif "Kohlekraft" in erforscht:
                kraft["Biomassegenerator"] = min(int(self.B.get("Einsatz.Start.Biomassegenerator", 1)), math.floor(rest / 30))
                kraft["Kohlekraftwerk"] = max(0, math.ceil((rest - kraft["Biomassegenerator"] * 30) / 75 - 1e-9))
            else:
                kraft["Biomassegenerator"] = math.ceil(rest / 30 - 1e-9)
        g.update(kraft)
        ergebnis = ({k: v for k, v in g.items() if v > 0}, dict(roh), last)
        self.cache[key] = ergebnis
        return ergebnis


# ---------------------------------------------------------------- Türme
class Turmwerte:
    def __init__(self, B):
        self.B = B

    def bonus(self, typ, level, wert):
        return sum(self.B.get(f"Tuerme.Level.{typ}.L{k}.{wert}", 0.0) for k in range(2, level + 1))

    def dps(self, typ, level, mun, doktrin, gabel, einzel=False):
        """Nenn-DPS gegen ein Ziel, mit grober Mehrfachziel-Wirkung (Annahme), wie kalibriere.nenn_dps + Flächen.
        einzel=True: nur ein Ziel wie NennDPS() im Mod (K_DPS/Q der Messung), ohne Sprünge, Kegel, Fläche, Durchschlag"""
        B = self.B
        k = "Munition." + mun[1]
        sb = (1 + self.bonus(typ, level, "Schaden")) * (1 + B.get("Doktrin.Schaden", 0.25) if doktrin else 1.0)
        tb = 1 + self.bonus(typ, level, "Takt")
        if typ == "Gewehrturm":
            return (B[k + ".Schaden"] + B.get(k + ".BrandSchaden", 0.0)) * sb * B[k + ".SchussProSekunde"] * tb
        if typ == "Flammenwerfer":
            return B[k + ".SchadenProSekunde"] * sb * (1.0 if einzel else 2.0 * (1 + self.bonus(typ, level, "Kegel") * 0.5))
        if typ == "Teslaspule":
            grund = B.get("Munition.Tesla.Schaden", B.get("Tesla.Schaden", 300))
            if gabel.get("Tesla") == "Ueberlast":
                return grund * 2.5 * sb * tb / ZYKLUS[typ]
            if einzel:
                return grund * sb * tb / ZYKLUS[typ]
            spr = B.get("Munition.Tesla.Spruenge", B.get("Tesla.Spruenge", 3)) + self.bonus(typ, level, "Spruenge") + (3 if gabel.get("Tesla") == "Kettenblitz" else 0)
            return grund * sb * tb / ZYKLUS[typ] * (1 + spr * B.get("Munition.Tesla.SprungFaktor", B.get("Tesla.SprungFaktor", 0.35)))
        schaden = B[k + ".Schaden"]
        if einzel:
            return schaden * sb * tb / ZYKLUS[typ]
        if mun[0] == "Durchschlagbolzen":
            schaden *= 1 + B.get("Munition.Bolzen.Durchschlagbolzen.WeitereZiele", 2) * B.get("Munition.Bolzen.Durchschlagbolzen.Folgeschaden", 0.5) * 0.5
        if mun[0] == "Explosivbolzen":
            schaden += B.get("Munition.Bolzen.Explosivbolzen.Flaechenschaden", 60) * 1.5
        if mun[0] == "Streubolzen":
            schaden += B.get("Munition.Bolzen.Streubolzen.Splitter", 3) * B.get("Munition.Bolzen.Streubolzen.Splitterschaden", 25) * 0.6
        if typ == "Nobelisk-Werfer":
            schaden *= 3.0
            if mun[0] == "Streu":
                schaden *= B.get("Munition.Nobelisk.Streu.Streu", 5) / 3
            if mun[0] == "Gas":
                schaden += B.get("Munition.Nobelisk.Gas.GasSchaden", 20) * B.get("Munition.Nobelisk.Gas.GasDauer", 10) * 3
        return schaden * sb * tb / ZYKLUS[typ]


# ---------------------------------------------------------------- Strategien
LINIEN = {
    "Mono Bolzen": {"Bolzenturm": 1}, "Mono Gewehr": {"Gewehrturm": 1}, "Mono Nobelisk": {"Nobelisk-Werfer": 1},
    "Mono Tesla": {"Teslaspule": 1}, "Mono Flamme": {"Flammenwerfer": 1},
    "Bolzen+Gewehr": {"Bolzenturm": 1, "Gewehrturm": 1}, "Gewehr+Tesla": {"Gewehrturm": 1, "Teslaspule": 1},
    "Gewehr+Flamme": {"Gewehrturm": 1, "Flammenwerfer": 1}, "Gewehr+Nobelisk": {"Gewehrturm": 1, "Nobelisk-Werfer": 1},
    "Breit": {t: 1 for t in TURM_TYPEN},
}
GABEL_OPTIONEN = {"Bolzen": ["Streubolzen", "Schockbolzen"], "Gewehr": ["Zielsuchmunition", "Panzerbrechend"], "Nobelisk": ["GasNobelisk", "PulsNobelisk"],
                  "Tesla": ["Kettenblitz", "Ueberlast"], "Kern": ["KernSchild", "KernGeschuetz"]}


def strategien(rng_auswahl):
    """Alle Linien x Doktrin (keine / Hauptlinie) x Ausgabestil x Kern-Ausbau x 4 zufällige Gabel-Kombinationen"""
    alle = []
    gabel_kombis = [dict(zip(GABEL_OPTIONEN, k)) for k in itertools.product(*GABEL_OPTIONEN.values())]
    for linie, gew in LINIEN.items():
        haupt = max(gew, key=lambda t: (gew[t], -TURM_TYPEN.index(t)))
        for doktrin in ([None, haupt] if len(gew) < 5 else [None]):
            for stil in ("Tuerme", "Level", "Wert"):
                for kern in (False, True):
                    for gabel in rng_auswahl.sample(gabel_kombis, 4):
                        alle.append({"Linie": linie, "Gewichte": gew, "Doktrin": doktrin, "Stil": stil, "Kern": kern, "Gabel": gabel})
    return alle


class Lauf:
    def __init__(self, s, B, Z, K, gabeln, tw, wi, stufe):
        self.s, self.B, self.Z, self.K, self.tw, self.wi = s, B, Z, K, tw, wi
        self.stufe = stufe
        self.ep = Z["StartEP"]
        self.fp = 0
        self.erforscht = set()
        self.gesperrt = set()
        self.besitz = {k: int(B.get(f"Einsatz.Start.{k}", 0)) for k in self._laden_keys()}
        self.gekauft = collections.Counter()
        self.tuerme = []   # [typ, level]
        for t in TURM_TYPEN:
            for _ in range(int(B.get(f"Einsatz.Start.{t}", 0))):
                self.tuerme.append([t, 1])
        self.kern = 100.0
        self.kern_min = 100.0
        # 16.11.2026: Kern-LP der Stufe (Stufen.<Stufe>.KernLP; Bohrung setzt Bohrung.<Stufe>.LP). Die Verlustkurve ist in %
        # eines Normal-Kerns (VerlustBezugKernLP) kalibriert, der Schaden der Gegner haengt nicht an den Kern-LP.
        self.kern_lp = B.get(f"Stufen.{stufe}.KernLP", Z.get("VerlustBezugKernLP", 20000))
        # Brennstoff der Biomassegeneratoren: Startkit 300 Biomasse + 200 Blätter (15 MJ) + 100 Holz (100 MJ), je Welle +100 (TDEinsatz.cpp:493)
        self.biomasse = Z["BiomasseStart"]
        self.biomasse_mangel = 0
        self.reparaturen = 0
        self.versorgung = 1.0
        self.ep_fehlt = 0
        self.ep_fehlt_wellen = []
        self.versorgung_min = 1.0
        self.mun_aktiv = {}   # Turmtyp -> Munition, die die Fabrik herstellt
        self.plan_forschung = self._forschungsplan()
        self.ep_summe = self.fp_summe = 0.0
        self.ep_ausgegeben = collections.Counter()
        self.haupt = max(s["Gewichte"], key=lambda t: (s["Gewichte"][t], -TURM_TYPEN.index(t)))
        self.haupt_ab = None   # Welle, vor der die erste Einheit der Hauptlinie stand
        # 0.9.1 (nur mit Z["Stand"] == "091" wirksam): Energiesplitter aus Beute, Stützbauten, Nutzungszähler, Verlauf je Welle
        self.splitter = 0.0
        self.bauten = collections.Counter()
        self.nutzung = collections.Counter()
        self.nutzung_ab = {}
        self.verlauf = []
        self.welle_D = Z["WellenGrund"]
        self.letzte_ep = 0.0

    @property
    def neu(self):
        return self.Z.get("Stand", "090") == "091"

    def extra(self):
        """Zusatzfelder nur für 0.9.1 oder den Verlauf (die CSV von --stand 090 bleibt wie bisher)"""
        e = {}
        if self.neu:
            e.update({f"Nutzung_{k}": v for k, v in self.nutzung.items()})
            e.update({f"NutzungAb_{k}": v for k, v in self.nutzung_ab.items()})
            e.update({f"Bauten_{k}": v for k, v in self.bauten.items() if v})
            e["SplitterRest"] = round(self.splitter, 2)
        if self.Z.get("Verlauf"):
            e["Verlauf"] = self.verlauf
        return e

    def _laden_keys(self):
        return sorted({k.split(".")[2] for k in self.B if k.startswith("Einsatz.Laden.") and k.endswith(".Grundpreis")})

    def preis(self, g):
        return sim.runde(self.B.get(f"Einsatz.Laden.{g}.Grundpreis", 99) * self.Z["PreisFaktor"] * (1 + self.B.get(f"Einsatz.Laden.{g}.Aufschlag", .5) * self.gekauft[g]))

    def _forschungsplan(self):
        """Reihenfolge: Turmlinien und ihre Munition, Gabelwahl, Turmfertigung, Doktrin, Kern-Ausbau, Industrie"""
        s, plan = self.s, []

        def add(k):
            if k and k not in plan and k in self.K:
                for v in self.K[k]["vor"]:
                    add(v)
                plan.append(k)
        add("Assembler"); add("Kupfer"); add("Kohlekraft")   # Strom: ohne Kohlekraft nur Biomasse-Generatoren
        for t in sorted(s["Gewichte"], key=lambda t: -s["Gewichte"][t]):
            add(TURM_FORSCHUNG[t])
            if t == "Bolzenturm":
                add("Durchschlagbolzen"); add(s["Gabel"]["Bolzen"])
            if t == "Gewehrturm":
                add(s["Gabel"]["Gewehr"]); add("Brandmunition")
            if t == "Nobelisk-Werfer":
                add(s["Gabel"]["Nobelisk"])
            if t == "Teslaspule":
                add(s["Gabel"]["Tesla"])
        if self.neu:
            # 0.9.1 E3: Kombos sind Standard, der Spieler forscht den Stützbau seiner Linie früh (4-6 FP)
            for t in sorted(s["Gewichte"], key=lambda t: -s["Gewichte"][t]):
                if t in STUETZBAU:
                    add(STUETZBAU[t][1])
        add("Turmfertigung2")
        if s["Kern"]:
            add("KernPanzer"); add("KernNotrep"); add(s["Gabel"]["Kern"])
        if s["Doktrin"]:
            add({"Bolzenturm": "DoktrinBolzen", "Gewehrturm": "DoktrinGewehr", "Nobelisk-Werfer": "DoktrinNobelisk",
                 "Teslaspule": "DoktrinTesla", "Flammenwerfer": "DoktrinFlamme"}[s["Doktrin"]])
        add("Munitionspuffer"); add("Turmfertigung3")
        for t in s["Gewichte"]:
            if t == "Gewehrturm":
                add("Turbomunition")
            if t == "Bolzenturm":
                add("Explosivbolzen")
            if t == "Nobelisk-Werfer":
                add("StreuNobelisk")
            if t == "Flammenwerfer":
                add("Turbotreibstoff")
            if t == "Teslaspule":
                add("Akkubank")
        # 0.9.1 E2 (Knoten fehlen im Baum von 0.9.0, add() überspringt sie dort): späte Munition und Hochspannung
        for t in s["Gewichte"]:
            if t == "Flammenwerfer":
                add("Raketentreibstoff"); add("Plasmaflamme")
            if t == "Teslaspule":
                add("Hochspannung")
        add("Turmfertigung4")
        return plan

    def forsche(self, n, gabeln):
        for k in self.plan_forschung:
            if k in self.erforscht or k in self.gesperrt:
                continue
            kn = self.K[k]
            if kn["ab"] > n or any(v not in self.erforscht for v in kn["vor"]):
                continue
            preis = kn["fp"] * self.Z["FPPreisFaktor"]
            if self.fp < preis:
                return
            self.fp -= preis
            self.erforscht.add(k)
            for g in gabeln:
                if k in g:
                    self.gesperrt.update(x for x in g if x != k)
        if all(k in self.erforscht or k in self.gesperrt for k in self.plan_forschung):
            while True:
                frei = [k for k, kn in self.K.items() if k not in self.erforscht and k not in self.gesperrt and kn["ab"] <= n
                        and all(v in self.erforscht for v in kn["vor"]) and not any(k in g for g in gabeln)]
                if not frei:
                    return
                k = min(frei, key=lambda k: self.K[k]["fp"])
                preis = self.K[k]["fp"] * self.Z["FPPreisFaktor"]
                if self.fp < preis:
                    return
                self.fp -= preis
                self.erforscht.add(k)

    def munition(self, typ):
        frei = [m for m in MUNITION[typ] if not m[2] or m[2] in self.erforscht]
        if typ == "Flammenwerfer" and any(m[0] == "Ionisiert" for m in frei) and self.splitter < self.splitter_bedarf():
            # 0.9.1: ionisierter Treibstoff nur, wenn die Energiesplitter aus der Beute für die ganze Welle reichen
            frei = [m for m in frei if m[0] != "Ionisiert"]
        return frei

    def splitter_bedarf(self):
        """Energiesplitter für eine Welle Plasmaflamme: Verbrauch aller Flammenwerfer / 16 m³ je Splitter (Recipe_IonizedFuel)"""
        m = next(m for m in MUNITION["Flammenwerfer"] if m[0] == "Ionisiert")
        feuer = feueranteil(self.Z, getattr(self, "welle_n", 10))
        m3 = sum(m[5] * (1 + self.tw.bonus(t, lv, "Takt")) * feuer * self.welle_D / 60 for t, lv in self.tuerme if t == "Flammenwerfer")
        return m3 / 16

    def dps_l(self, typ, lv, m, dokt, gabel, einzel=False):
        """Turmwerte.dps plus 0.9.1: Hochspannung (Tesla x1,3) und die Kombos der Stützbauten (nur wirksames DPS, nicht Nenn-DPS)"""
        d = self.tw.dps(typ, lv, m, dokt, gabel, einzel)
        if not self.neu:
            return d
        B, Z = self.B, self.Z
        hs = B.get("Tuerme.Teslaspule.Hochspannung", 1.3) if typ == "Teslaspule" and "Hochspannung" in self.erforscht else 1.0
        d *= hs
        if einzel or typ not in STUETZBAU:
            return d
        # Explosionen: Sammelpunkt am Lockfeuer (Nobelisk ausser Gas, Explosivbolzen)
        if self.bauten["Lockfeuer"] and ((typ == "Nobelisk-Werfer" and m[3] is None) or m[0] == "Explosivbolzen"):
            d *= 1 + (B.get("Kombo.Sammelpunkt.Faktor", 1.25) - 1) * Z["KomboAnteilGelockt"] * self.abdeckung("Lockfeuer")
        bau = STUETZBAU[typ][0]
        abd = self.abdeckung(bau)
        if not abd:
            return d
        e = self.tw.dps(typ, lv, m, dokt, gabel, einzel=True) * hs
        if typ == "Bolzenturm":
            # Engstelle: +Kombo.Engstelle.Ziele Durchschlag; Folgeschaden kumuliert (TDBoltTurret.cpp:244-252): Durchschlagbolzen 0,5^3, sonst 0,6
            if m[0] == "Durchschlagbolzen":
                folge = B.get("Munition.Bolzen.Durchschlagbolzen.Folgeschaden", 0.5) ** (B.get("Munition.Bolzen.Durchschlagbolzen.WeitereZiele", 2) + 1)
            else:
                folge = 0.6
            d += e * folge * B.get("Kombo.Engstelle.Ziele", 1) * Z["KomboAnteilEngstelle"] * abd
        elif typ == "Teslaspule":
            # Leiter: alle Gegner am Zaun (4 m) bekommen Kombo.Leiter.Anteil des Grundschadens (TDTeslaCoil.cpp:163-188)
            d += e * B.get("Kombo.Leiter.Anteil", 0.5) * Z["KomboGegnerAmZaun"] * abd
        elif typ == "Flammenwerfer":
            # Ölbrand: Gegner auf der Platte sind 35 % langsamer (länger im Kegel) und brennen 20/s, danach 10/s für 3 s
            brems = B.get("Tuerme.Oelschlick.Bremse", 0.65)
            brand = B.get("Tuerme.Oelschlick.BrandSchaden", 20) + B.get("Tuerme.Oelschlick.Nachbrennen", 10) * B.get("Tuerme.Oelschlick.NachbrennDauer", 3) / Z["KomboPlatteVerweilSekunden"]
            ziele = 2.0   # wie Turmwerte.dps: Flamme trifft im Mittel 2 Ziele
            d += Z["KomboAnteilPlatte"] * abd * (d * (1 / brems - 1) + ziele * brand)
        return d

    def abdeckung(self, bau):
        """Anteil der Türme der Bau-Art, die einen Stützbau in Reichweite haben (Annahme KomboTuermeJeBau)"""
        if not self.bauten[bau]:
            return 0.0
        typ = next(t for t, v in STUETZBAU.items() if v[0] == bau)
        n = sum(1 for t, _ in self.tuerme if t == typ)
        return min(1.0, self.bauten[bau] * self.Z["KomboTuermeJeBau"] / max(1, n))

    def max_level(self, typ):
        lv = 2
        if "Turmfertigung2" in self.erforscht: lv = 3
        if "Turmfertigung3" in self.erforscht: lv = 4
        if "Turmfertigung4" in self.erforscht: lv = 5
        if self.s["Doktrin"] == typ and "Turmfertigung3" in self.erforscht and self.doktrin_aktiv():
            lv = 5
        return lv

    def doktrin_aktiv(self):
        d = self.s["Doktrin"]
        return bool(d) and {"Bolzenturm": "DoktrinBolzen", "Gewehrturm": "DoktrinGewehr", "Nobelisk-Werfer": "DoktrinNobelisk",
                            "Teslaspule": "DoktrinTesla", "Flammenwerfer": "DoktrinFlamme"}[d] in self.erforscht

    def frei_typen(self):
        return [t for t in self.s["Gewichte"] if not TURM_FORSCHUNG[t] or TURM_FORSCHUNG[t] in self.erforscht] or ["Bolzenturm"]

    def bedarf(self, tuerme, zyklus, D):
        """Munition je Minute für die Fabrik (Welle ÷ Zyklus) und Strom der Türme"""
        bed, strom = collections.Counter(), 0.0
        n = getattr(self, "welle_n", 10)
        feuer = feueranteil(self.Z, n)
        bau_min = max(0.0, zyklus - D / 60)   # Bauphase in Minuten
        puffer = self.B.get("Forschung.Munitionspuffer.Faktor", 2) if "Munitionspuffer" in self.erforscht else 1
        for typ, lv in tuerme:
            if typ == "Teslaspule":
                # Spitze wie bei den anderen Türmen: Laden 75 MW (mPowerConsumption, Akkubank x0,5), sonst Leerlauf
                # Tuerme.Teslaspule.Grundverbrauch (TDTeslaCoil.cpp:207-215). Vorher: Grund + Laden x Feueranteil (Mittel, inkonsistent)
                akku = self.B.get("Forschung.Akkubank.Faktor", 0.5) if "Akkubank" in self.erforscht else 1.0
                strom += max(self.B.get("Tuerme.Teslaspule.Grundverbrauch", self.Z["TeslaGrund"]), self.Z.get("TeslaLadenUeber", self.B.get("Tuerme.Teslaspule.Ladeverbrauch", self.Z["TeslaLaden"])) * akku)
                continue
            # Strom: Spitze, wenn alle Türme zugleich feuern (der Spieler legt das Netz darauf aus)
            strom += TURM_STROM.get(typ, self.Z["TurmStrom"])
            m = self.beste_munition_fuer_wirtschaft(typ)
            if m and m[4]:
                takt = 1 + self.tw.bonus(typ, lv, "Takt")
                n_item = max(1.0, self.B.get("Munition.SchussJeItem." + SCHUSS_REZEPT.get((typ, m[0]), ""), 1))
                v = m[5] / n_item * takt * feuer * D / 60              # Verbrauch je Welle und Turm (17.11.: Aktionen je Item)
                s = STAPEL.get(m[4], self.Z["FlammePuffer"]) * (puffer if m[4] in STAPEL else 1)
                # Vorrat aus der Bauphase höchstens bis zum vollen Slot, der Rest muss während der Welle nachkommen
                bed[m[4]] += max(v / zyklus, (v - s) / (D / 60))
        # 0.9.1 Stützbauten: Strom, Kohle fürs Lockfeuer, Schwerölrückstand fürs Ölschlick-Feld (in 0.9.0 kauft der Spieler keine)
        for bau, z in self.bauten.items():
            strom += BAU_STROM.get(bau, 0) * z
            if bau == "Lockfeuer":
                # 2 Kohle je Lockruf, Lockruf 15 s + Pause 30 s, nur während der Welle
                bed["Kohle"] += z * self.Z["LockfeuerKohleJeRuf"] * 60 / (self.Z["LockfeuerRufSekunden"] + self.B.get("Tuerme.Lockfeuer.Pause", 30)) * D / 60 / zyklus
            if bau == "Oelschlick":
                bed["Schwerölrückstand"] += z * self.Z["OelschlickSchweroelJeMin"]
        return bed, strom

    def beste_munition_fuer_wirtschaft(self, typ):
        """Munition, die die Fabrik gerade herstellt. Umgestellt wird in baue(), sobald die neue Kette bezahlbar ist"""
        frei = self.munition(typ)
        if not frei:
            return None
        aktiv = self.mun_aktiv.get(typ)
        if aktiv is not None and aktiv not in frei:
            # 0.9.1: Munition wieder gesperrt (Plasmaflamme ohne Splitter) -> die beste freie darunter, die Kette steht meist schon
            darunter = [m for m in frei if MUNITION[typ].index(m) < MUNITION[typ].index(aktiv)]
            return darunter[-1] if darunter else frei[0]
        return aktiv if aktiv in frei else frei[0]

    def stelle_munition_um(self, zyklus, D, n):
        """Neue Munition erst, wenn ihre Kette (Gebäude) bezahlbar ist - ein Spieler reißt die laufende Fabrik nicht vorher ab"""
        for typ in sorted({t for t, _ in self.tuerme}):
            frei = self.munition(typ)
            if not frei or self.beste_munition_fuer_wirtschaft(typ) is frei[-1]:
                continue
            alt = self.mun_aktiv.get(typ)
            self.mun_aktiv[typ] = frei[-1]
            bed, strom = self.bedarf(self.tuerme, zyklus, D)
            geb, _, _ = self.wi.gebaeude(bed, strom, n, self.erforscht)
            if not self.gebaeude_ok(geb) or self.kosten(self.fehlende_kontingente(geb)) > self.ep - self.Z["Reserve"]:
                if alt is None:
                    self.mun_aktiv.pop(typ)
                else:
                    self.mun_aktiv[typ] = alt

    def gebaeude_ok(self, geb):
        """Gebäude, die eine Forschung brauchen, die fehlt -> nicht baubar"""
        return all(not GEB_FORSCHUNG.get(g) or GEB_FORSCHUNG[g] in self.erforscht for g in geb)

    def fehlende_kontingente(self, geb):
        fehl = []
        for g, z in geb.items():
            for _ in range(max(0, z - self.besitz.get(g, 0))):
                fehl.append(g)
        return fehl

    def kosten(self, fehl):
        c, tmp = 0, collections.Counter()
        for g in fehl:
            c += sim.runde(self.B.get(f"Einsatz.Laden.{g}.Grundpreis", 99) * self.Z["PreisFaktor"] * (1 + self.B.get(f"Einsatz.Laden.{g}.Aufschlag", .5) * (self.gekauft[g] + tmp[g])))
            tmp[g] += 1
        return c

    def kaufe(self, fehl, posten):
        for g in fehl:
            p = self.preis(g)
            self.ep -= p
            self.ep_ausgegeben[posten] += p
            self.gekauft[g] += 1
            self.besitz[g] = self.besitz.get(g, 0) + 1

    def baue(self, n, zyklus, D):
        """Bauphase vor Welle n: erst Fabrik für die vorhandenen Türme, dann Türme oder Level nach Stil"""
        self.welle_n = n
        self.welle_D = D
        def fabrik_fuer(tuerme):
            bed, strom = self.bedarf(tuerme, zyklus, D)
            geb, _, _ = self.wi.gebaeude(bed, strom, n, self.erforscht)
            return geb
        self.stelle_munition_um(zyklus, D, n)
        geb = fabrik_fuer(self.tuerme)
        fehl = self.fehlende_kontingente(geb)
        c = self.kosten(fehl)
        if c <= self.ep:
            self.kaufe(fehl, "Fabrik")
            self.versorgung = 1.0
        else:
            # Teilversorgung: was bezahlbar ist, kaufen; Versorgung = Anteil der gedeckten Gebäude
            gekauft = []
            for g in fehl:
                if self.kosten(gekauft + [g]) <= self.ep:
                    gekauft.append(g)
            self.kaufe(gekauft, "Fabrik")
            noetig = sum(geb.values())
            self.versorgung = max(0.2, 1 - (len(fehl) - len(gekauft)) / max(1, noetig))
            self.ep_fehlt += 1
            self.ep_fehlt_wellen.append(n)
            self.versorgung_min = min(self.versorgung_min, self.versorgung)
            return
        self.repariere()
        # Vorhalt: die nächste Welle dauert länger und braucht mehr Munition je Zyklus -> Fabrikausbau dafür zurücklegen
        D2 = D + self.Z["WellenJe"]
        zyklus2 = zyklus_min(self.Z, n + 1, D2)
        bed2, strom2 = self.bedarf(self.tuerme, zyklus2, D2)
        vorhalt = self.kosten(self.fehlende_kontingente(self.wi.gebaeude(bed2, strom2, n + 1, self.erforscht)[0])) * self.Z.get("Vorhalt", 1.0)
        if self.neu:
            vorhalt += self.spar_vorhalt(zyklus, D, n)
            self.kaufe_stuetzbauten(fabrik_fuer, vorhalt)
        # Türme und Level
        for _ in range(60):
            optionen = []
            typen = self.frei_typen()
            gew = self.s["Gewichte"]
            anzahl = collections.Counter(t for t, _ in self.tuerme)
            ziel = min(typen, key=lambda t: (anzahl[t] / gew.get(t, 1), TURM_TYPEN.index(t)))
            neu = self.tuerme + [[ziel, 2 if TURM_FORSCHUNG[ziel] in self.erforscht or ziel == "Bolzenturm" else 1]]
            fehl_neu = [g for g in self.fehlende_kontingente(fabrik_fuer(neu))] + ([ziel] if anzahl[ziel] + 1 > self.besitz.get(ziel, 0) else [])
            kosten_turm = self.kosten(fehl_neu) + self.levelpreis(ziel, 1, neu[-1][1])
            gewinn_turm = self.dps_l(ziel, neu[-1][1], (self.beste_munition_fuer_wirtschaft(ziel) or self.munition(ziel)[0]), self.doktrin_aktiv() and self.s["Doktrin"] == ziel, self.s["Gabel"]) * self.Z.get(f"Mehrziel_{ziel}", 1.0)
            if self.gebaeude_ok(fabrik_fuer(neu)):
                optionen.append(("Turm", kosten_turm, gewinn_turm, (neu, fehl_neu)))
            for i, (typ, lv) in enumerate(self.tuerme):
                if lv < self.max_level(typ) and typ in gew:
                    p = self.levelpreis(typ, lv, lv + 1)
                    m = self.beste_munition_fuer_wirtschaft(typ) or self.munition(typ)[0]
                    g = self.dps_l(typ, lv + 1, m, self.doktrin_aktiv() and self.s["Doktrin"] == typ, self.s["Gabel"]) - self.dps_l(typ, lv, m, self.doktrin_aktiv() and self.s["Doktrin"] == typ, self.s["Gabel"])
                    g *= self.Z.get(f"Mehrziel_{typ}", 1.0)
                    optionen.append(("Level", p, g, i))
            optionen = [o for o in optionen if o[1] <= self.ep - self.Z["Reserve"] - vorhalt]
            if not optionen:
                break
            stil = self.s["Stil"]
            if stil == "Tuerme":
                wahl = next((o for o in optionen if o[0] == "Turm"), None) or max(optionen, key=lambda o: o[2] / max(o[1], 1))
            elif stil == "Level":
                wahl = next((o for o in sorted(optionen, key=lambda o: -o[2] / max(o[1], 1)) if o[0] == "Level"), None) or optionen[0]
            else:
                wahl = max(optionen, key=lambda o: o[2] / max(o[1], 1))
            if wahl[0] == "Turm":
                neu, fehl_neu = wahl[3]
                self.kaufe([g for g in fehl_neu if g not in TURM_TYPEN], "Fabrik")
                self.kaufe([g for g in fehl_neu if g in TURM_TYPEN], "Tuerme")
                p = self.levelpreis(neu[-1][0], 1, neu[-1][1])
                self.ep -= p
                self.ep_ausgegeben["Level"] += p
                self.tuerme = neu
            else:
                i = wahl[3]
                self.ep -= wahl[1]
                self.ep_ausgegeben["Level"] += wahl[1]
                self.tuerme[i][1] += 1

    def spar_vorhalt(self, zyklus, D, n):
        """0.9.1, Schalter MunitionSparen: ist Raketentreibstoff/Plasmaflamme erforscht, aber die Kette zu teuer, legt der
        Spieler Coupons dafür zurück, wenn sie höchstens SparWellen Wellen-Einkommen kostet (sonst kauft er weiter Türme)"""
        if not self.Z.get("MunitionSparen") or not any(t == "Flammenwerfer" for t, _ in self.tuerme):
            return 0.0
        frei = self.munition("Flammenwerfer")
        ziel = frei[-1] if frei else None
        if not ziel or ziel[0] not in ("Raketentreibstoff", "Ionisiert") or self.beste_munition_fuer_wirtschaft("Flammenwerfer") is ziel:
            return 0.0
        alt = self.mun_aktiv.get("Flammenwerfer")
        self.mun_aktiv["Flammenwerfer"] = ziel
        bed, strom = self.bedarf(self.tuerme, zyklus, D)
        geb, _, _ = self.wi.gebaeude(bed, strom, n, self.erforscht)
        if alt is None:
            self.mun_aktiv.pop("Flammenwerfer")
        else:
            self.mun_aktiv["Flammenwerfer"] = alt
        if not self.gebaeude_ok(geb):
            return 0.0
        c = self.kosten(self.fehlende_kontingente(geb))
        if c > self.Z.get("SparWellen", 3) * max(1.0, self.letzte_ep):
            return 0.0
        self.nutzung["SparWellen"] += 1
        return c

    def kaufe_stuetzbauten(self, fabrik_fuer, vorhalt):
        """0.9.1 E3: steht der Turm der Linie und ist die Forschung da, kauft der Spieler den passenden Bau (ein Bau je
        KomboTuermeJeBau Türme), samt Versorgung (Kohle, Schweröl, Strom), sofern die Coupons nach dem Vorhalt reichen"""
        anzahl = collections.Counter(t for t, _ in self.tuerme)
        for typ, (bau, fo, _) in STUETZBAU.items():
            if typ not in self.s["Gewichte"] or not anzahl[typ] or fo not in self.erforscht:
                continue
            while self.bauten[bau] * self.Z["KomboTuermeJeBau"] < anzahl[typ]:
                self.bauten[bau] += 1
                fehl = self.fehlende_kontingente(fabrik_fuer(self.tuerme))
                if not self.gebaeude_ok(fabrik_fuer(self.tuerme)):
                    self.bauten[bau] -= 1
                    break
                eigen = [bau] if self.bauten[bau] > self.besitz.get(bau, 0) else []
                if self.kosten(fehl + eigen) > self.ep - self.Z["Reserve"] - vorhalt:
                    self.bauten[bau] -= 1
                    break
                self.kaufe(fehl, "Fabrik")
                self.kaufe(eigen, "Bauten")
                self.nutzung_ab.setdefault(bau, self.welle_n)

    def repariere(self):
        """Kernreparatur im Laden (TDEinsatz.cpp:784-818): 25 % der Kern-LP, Preis 15 x (1 + 0,5 x bisher), nur in der Bauphase.
        Regel des Spielers: kaufen, solange der Kern unter ReparaturUnter steht und die EP reichen"""
        while self.kern < self.Z["ReparaturUnter"]:
            p = sim.runde(self.B.get("Einsatz.Laden.Kernreparatur.Grundpreis", 15) * self.Z["PreisFaktor"]
                          * (1 + self.B.get("Einsatz.Laden.Kernreparatur.Aufschlag", 0.5) * self.reparaturen))
            if p > self.ep:
                return
            self.ep -= p
            self.ep_ausgegeben["Kern"] += p
            self.reparaturen += 1
            self.kern = min(100.0, self.kern + self.B.get("Einsatz.Laden.Kernreparatur.Paket", 25))

    def verbrenne(self, n, zyklus, D):
        """Biomasse für die Biomassegeneratoren dieser Welle verbrauchen; fehlt sie, fehlt Strom -> Versorgung sinkt.
        Energie = Spitzenlast x mittlere Auslastung (Live 03./04.11.: Verbrauch/Spitze im Mittel 0,75) x Zykluszeit"""
        bed, strom = self.bedarf(self.tuerme, zyklus, D)
        geb, _, last = self.wi.gebaeude(bed, strom, n, self.erforscht)
        bio_mw = min(last, geb.get("Biomassegenerator", 0) * 30)
        if bio_mw <= 0:
            return
        brauche = bio_mw * self.Z["StromMittelAnteil"] * zyklus * 60 / self.Z["BiomasseMJ"]
        if brauche <= self.biomasse:
            self.biomasse -= brauche
            return
        anteil = self.biomasse / brauche        # so viel Biomassestrom ist gedeckt
        self.biomasse = 0.0
        self.biomasse_mangel += 1
        self.versorgung *= 1 - (1 - anteil) * bio_mw / max(last, 1e-9)

    def levelpreis(self, typ, von, bis):
        rabatt = self.doktrin_aktiv() and self.s["Doktrin"] == typ
        p = 0
        for l in range(von + 1, bis + 1):
            x = self.B.get(f"Tuerme.Level.Preis{l}", [0, 0, 8, 15, 25, 40][l])
            # wie TDTurretBase: max(1, RoundToInt(Preis x (1 - LevelRabatt))) je Stufe
            p += max(1, sim.runde(x * (1 - self.B.get("Doktrin.LevelRabatt", 0.25)))) if rabatt else x
        return p * self.Z["PreisFaktor"]

    def kampf(self, gegner, elite, n, rng):
        """K mit der wirksamsten freien Munition je Turm gegen diese Welle, Versorgung; Q -> Kernverlust"""
        lp_ges = sum(l for _, l in gegner) or 1.0
        tuerme = []
        for typ, lv in self.tuerme:
            best = None
            dokt = self.doktrin_aktiv() and self.s["Doktrin"] == typ
            # verschossen wird, was die Fabrik herstellt (beste freie Munition, wie in bedarf())
            for m in [self.beste_munition_fuer_wirtschaft(typ)] if typ != "Teslaspule" else self.munition(typ):
                art = m[3] or ART[typ]
                # Mehrziel_<Typ>: an Spielmessungen angepasster Faktor auf die Formel (verlust_fit.py, 07.11.)
                d = self.dps_l(typ, lv, m, dokt, self.s["Gabel"]) * (self.versorgung if typ != "Teslaspule" else 1.0) * self.Z.get(f"Mehrziel_{typ}", 1.0)
                wirk = d * sum(l * sim.resistenz(self.B, art, name) for name, l in gegner) / lp_ges
                if best is None or wirk > best[0]:
                    best = (wirk, art, d, self.dps_l(typ, lv, m, dokt, self.s["Gabel"], einzel=True) * (self.versorgung if typ != "Teslaspule" else 1.0))
            tuerme.append(best)
        if "KernGeschuetz" in self.erforscht:
            kg = self.B.get("Forschung.KernGeschuetz.Schaden", 60) / 1.5
            tuerme.append((0, "Durchschlag", kg, kg))
        K = sum(t[2] for t in tuerme)
        KN = sum(t[3] for t in tuerme)
        ehp = ehpn = 0.0
        for name, l in gegner:
            f = sum(t[2] * sim.resistenz(self.B, t[1], name) for t in tuerme) / K if K else 1.0
            fn = sum(t[3] * sim.resistenz(self.B, t[1], name) for t in tuerme) / KN if KN else 1.0
            ehp += l / max(f, 0.05)
            ehpn += l / max(fn, 0.05)
        q = ehp / (K * 60) if K else 99.0
        # Nenn-Q wie die Messung im Mod (K_DPS = Einzelziel-DPS); wirksames Q mit Mehrfachtreffern
        self.q_nenn, self.k_nenn = (ehpn / (KN * 60) if KN else 99.0), KN
        if self.Z.get("VerlustBasis") == "Nenn":
            q = self.q_nenn
        Z = self.Z
        # Streckung: mehr Gegner als gleichzeitig erlaubt kommen nacheinander, der Druck verteilt sich über die längere Welle
        # (Messung 06.11.: R² 0,22 -> 0,45 mit Exponent 1,5)
        qv = q / max(1.0, len(gegner) / self.B.get("Wellen.MaxGleichzeitig", 60)) ** Z.get("VerlustStreckung", 0.0)
        # Kurve statt Gerade (07.11.: Verlust wächst schneller als Q, verlust_fit.py)
        verlust = Z["VerlustGrund"] + Z["VerlustQ"] * qv ** Z.get("VerlustP", 1.0) + Z["VerlustUeber"] * max(0.0, qv - Z["QKritisch"]) ** 2 * 100
        if Z.get("VerlustStreuung"):
            # gemessene Verteilung Verlust / Grundkurve (Dezile, Streuung-Messung)
            # unter hohem Druck streut der Verlust weniger (07.11.: Std. von Messung/Kurve 0,8 bis qv 0,3, 0,56 bei 0,5, 0,38 darüber)
            f = rng.choice(Z["VerlustStreuung"])
            ab = Z.get("StreuungAbQ", 0.0)
            if ab and qv > ab:
                f = 1 + (f - 1) * (ab / qv) ** Z.get("StreuungExponent", 0.7)
            verlust *= f
        elif rng.random() < Z["AusreisserChance"]:
            verlust *= Z["AusreisserFaktor"]
        verlust *= Z.get("VerlustBezugKernLP", 20000) / max(1.0, self.kern_lp)
        if "KernPanzer" in self.erforscht:
            verlust /= self.B.get("Forschung.KernPanzer.Faktor", 1.3)
        if "KernSchild" in self.erforscht:
            verlust = max(0.0, verlust - Z["SchildProzent"])
        self.kern -= verlust
        self.kern_min = min(self.kern_min, self.kern)
        if self.neu:
            self.zaehle_nutzung(n)
        return q, K, verlust

    def zaehle_nutzung(self, n):
        """0.9.1: Wellen mit Raketentreibstoff, Plasmaflamme, Hochspannung und je Stützbau; Splitterverbrauch der Plasmaflamme"""
        if any(t == "Flammenwerfer" for t, _ in self.tuerme):
            m = self.beste_munition_fuer_wirtschaft("Flammenwerfer")
            if m and m[0] in ("Raketentreibstoff", "Ionisiert"):
                self.nutzung[m[0]] += 1
                self.nutzung_ab.setdefault(m[0], n)
            if m and m[0] == "Ionisiert":
                self.splitter = max(0.0, self.splitter - self.splitter_bedarf())
        if "Hochspannung" in self.erforscht and any(t == "Teslaspule" for t, _ in self.tuerme):
            self.nutzung["Hochspannung"] += 1
            self.nutzung_ab.setdefault("Hochspannung", n)
        for bau, z in self.bauten.items():
            if z:
                self.nutzung[bau] += 1

    def belohnung(self, n, gegner_zahl, elite, boss):
        B = self.B
        p = int(B["Einsatz.EP.Grund"]) + sim.runde(B["Einsatz.EP.JeWelle"] * n)
        p += int(B["Einsatz.EP.KernVoll"]) if self.kern >= 99.5 else int(B["Einsatz.EP.KernAb75"]) if self.kern >= 75 else 0
        if boss:
            p = sim.runde(p * B["Einsatz.EP.BossFaktor"]) + int(B["Einsatz.EP.BossBonus"])
        normal = gegner_zahl - elite - (1 if boss else 0)
        beute_ep = normal * B.get("Loot.EP.Normal", 0) * B.get("Loot.EPMenge.Normal", 1) + elite * B.get("Loot.EP.Elite", 0) * B.get("Loot.EPMenge.Elite", 2) + \
            (B.get("Loot.EP.Boss", 1) * B.get("Loot.EPMenge.Boss", 10) if boss else 0)
        p = (p + beute_ep * self.Z["BeuteAnteil"]) * self.Z["EPFaktor"]
        f = int(B["Einsatz.FP.JeWelle"]) + (int(B["Einsatz.FP.KernAb75"]) if self.kern >= 75 else 0) + (int(B["Einsatz.FP.Boss"]) if boss else 0)
        beute_fp = normal * B.get("Loot.Kampfdaten.Normal", 0) + elite * B.get("Loot.Kampfdaten.Elite", 0) + (B.get("Loot.Kampfdaten.Boss", 1) if boss else 0)
        f = (f + beute_fp * self.Z["BeuteAnteil"]) * self.Z["FPFaktor"]
        self.ep += p
        self.fp += f
        self.letzte_ep = p
        # 0.9.1: Energiesplitter aus der Beute (Loot.Splitter.* = Chance, Loot.SplitterMenge.* = Anzahl), Anteil für Treibstoff ist eine Annahme
        self.splitter += (elite * B.get("Loot.Splitter.Elite", 0) * B.get("Loot.SplitterMenge.Elite", 1) + normal * B.get("Loot.Splitter.Normal", 0) * B.get("Loot.SplitterMenge.Normal", 1)
                          + (B.get("Loot.Splitter.Boss", 0) * B.get("Loot.SplitterMenge.Boss", 1) if boss else 0)) * self.Z["BeuteAnteil"] * self.Z.get("SplitterFuerTreibstoff", 1.0)
        self.biomasse += self.B.get("Einsatz.Biomasse.JeWelle", 100)
        self.ep_summe += p
        self.fp_summe += f
        rep = B.get("Kern.Grundreparatur", 0.05) * 100 + (B.get("Forschung.KernNotrep.Anteil", 0.1) * 100 if "KernNotrep" in self.erforscht else 0)
        self.kern = min(100.0, self.kern + rep)


def spiele(s, wellen_liste, B, Z, K, gabeln, tw, wi, stufe, laenge, rng):
    L = Lauf(s, B, Z, K, gabeln, tw, wi, stufe)
    aufbau, pause = B[f"Stufen.{stufe}.AufbauSekunden"], B[f"Stufen.{stufe}.PauseSekunden"]
    qs, ursache = [], ""
    for n in range(1, laenge + 1):
        D = Z["WellenGrund"] + Z["WellenJe"] * (n - 1)
        zyklus = zyklus_min(Z, n, D)
        L.forsche(n, gabeln)
        L.baue(n, zyklus, D)
        L.verbrenne(n, zyklus, D)
        if L.haupt_ab is None and any(t == L.haupt for t, _ in L.tuerme) and (L.haupt != "Bolzenturm" or n > 1):
            L.haupt_ab = n
        gegner, elite = wellen_liste[n - 1]
        q, Kd, verl = L.kampf(gegner, elite, n, rng)
        qs.append(q)
        if Z.get("Verlauf"):
            L.verlauf.append((n, round(q, 4), round(verl, 3), round(max(0.0, L.kern), 2), round(L.ep, 1), round(L.fp_summe, 1), round(L.q_nenn, 4)))   # 16.11.: Nenn-Q angehaengt
        if L.kern <= 0:
            if L.versorgung < 1:
                ursache = "Munition/Fabrik (EP)"
            elif q > Z["QKritisch"]:
                ursache = "Abwehr zu schwach"
            else:
                ursache = "Ausreißer"
            return {"Erfolg": False, "Welle": n, "Ursache": ursache, "KernMin": 0, "Q_max": max(qs), "Q_Mittel": statistics.mean(qs),
                    "Tuerme": len(L.tuerme), "Level": statistics.mean(lv for _, lv in L.tuerme), "EP_Rest": L.ep, "EP_Summe": L.ep_summe,
                    "Forschung": len(L.erforscht), "EPFehlt": L.ep_fehlt, "EPFehltWellen": L.ep_fehlt_wellen, "VersorgungMin": L.versorgung_min, "BiomasseMangel": L.biomasse_mangel, "Reparaturen": L.reparaturen, "HauptAb": L.haupt_ab or 99,
            "HauptFrei": K[TURM_FORSCHUNG[L.haupt]]["ab"] if TURM_FORSCHUNG[L.haupt] else 1, **{f"EP_{k}": v for k, v in L.ep_ausgegeben.items()}, **L.extra()}
        L.belohnung(n, len(gegner), elite, n % 10 == 0)
    return {"Erfolg": True, "Welle": laenge, "Ursache": "", "KernMin": L.kern_min, "Q_max": max(qs), "Q_Mittel": statistics.mean(qs),
            "Tuerme": len(L.tuerme), "Level": statistics.mean(lv for _, lv in L.tuerme), "EP_Rest": L.ep, "EP_Summe": L.ep_summe,
            "Forschung": len(L.erforscht), "EPFehlt": L.ep_fehlt, "EPFehltWellen": L.ep_fehlt_wellen, "VersorgungMin": L.versorgung_min, "BiomasseMangel": L.biomasse_mangel, "Reparaturen": L.reparaturen, "HauptAb": L.haupt_ab or 99,
            "HauptFrei": K[TURM_FORSCHUNG[L.haupt]]["ab"] if TURM_FORSCHUNG[L.haupt] else 1, **{f"EP_{k}": v for k, v in L.ep_ausgegeben.items()}, **L.extra()}


def lade_alles(balance_ueber=None, stand="090"):
    B, quelle = sim.lade_balance(os.path.join(sim.SAVED, "FicsitTD", "Balance.json"))
    if balance_ueber:
        B.update(balance_ueber)
    A = json.load(open(os.path.join(HIER, "annahmen.json"), encoding="utf-8"))
    Z = json.load(open(os.path.join(HIER, "strategie_annahmen.json"), encoding="utf-8"))
    Z = {k: v["Wert"] if isinstance(v, dict) else v for k, v in Z.items() if not k.startswith("_")}
    katalog, phasen = sim.lade_katalog(B)
    K, gabeln = lade_forschung(stand) if stand != "090" else lade_forschung()   # ohne Argument: szenario_stufen.py ersetzt die Funktion
    # 16.11.2026: wie im Spiel geht die Balance Forschung.<Knoten>.AbWelle vor (z. B. Teslaspule 5 statt 6)
    for k, kn in K.items():
        kn["ab"] = int(B.get(f"Forschung.{k}.AbWelle", kn["ab"]))
    Z["Stand"] = stand
    if stand == "091":
        Z["KnotenGesamt"] = Z.get("KnotenGesamt091", len(K))
    return B, A, Z, katalog, phasen, K, gabeln, quelle


def massenlauf(stufe="Normal", laenge=50, laeufe=3, seed=7, balance_ueber=None, z_ueber=None, strategie_liste=None, stand="090"):
    B, A, Z, katalog, phasen, K, gabeln, quelle = lade_alles(balance_ueber, stand)
    if z_ueber:
        Z.update(z_ueber)
    wellen = sim.Wellen(B, katalog, phasen, A["Gegner_GrundLP"], stufe)
    wellen_je_seed = []
    for s_i in range(laeufe):
        rng = random.Random(seed * 1009 + s_i)
        wellen_je_seed.append([wellen.erzeuge(n, rng) for n in range(1, laenge + 1)])
    tw, wi = Turmwerte(B), Wirtschaft(B, Z)
    liste = strategie_liste or strategien(random.Random(seed))
    ergebnisse = []
    for i, s in enumerate(liste):
        for s_i in range(laeufe):
            r = spiele(s, wellen_je_seed[s_i], B, Z, K, gabeln, tw, wi, stufe, laenge, random.Random(seed * 31 + i * 7 + s_i))
            r.update({"Linie": s["Linie"], "Doktrin": s["Doktrin"] or "keine", "Stil": s["Stil"], "Kern": "ja" if s["Kern"] else "nein",
                      "Gabel": "/".join(v for v in s["Gabel"].values()), "Startwert": s_i, "Nr": i})
            ergebnisse.append(r)
    return ergebnisse, quelle


def zielbild(ergebnisse, Z):
    """Kennzahlen gegen das Zielbild: mehrere Wege erfolgreich, nicht alles, nicht zu leicht"""
    je_linie = collections.defaultdict(list)
    for r in ergebnisse:
        je_linie[r["Linie"]].append(r["Erfolg"])
    quote = {l: sum(v) / len(v) for l, v in je_linie.items()}
    gesamt = sum(r["Erfolg"] for r in ergebnisse) / len(ergebnisse)
    tragfaehig = [l for l, q in quote.items() if q >= Z["ZielLinieQuote"]]
    leicht = [r for r in ergebnisse if r["Erfolg"] and r["KernMin"] >= Z["ZuLeichtKern"]]
    return {"Quote": gesamt, "JeLinie": quote, "Tragfaehig": tragfaehig, "ZuLeichtAnteil": len(leicht) / len(ergebnisse),
            "Ziel_Wege": len(tragfaehig) >= Z["ZielWegeMin"], "Ziel_NichtAlles": gesamt <= Z["ZielQuoteMax"],
            "Ziel_NichtZuLeicht": len(leicht) / len(ergebnisse) <= Z["ZielZuLeichtMax"],
            # Marius 08.11.2026: Mono-Linien muessen W50 nicht tragen; "keine tote Linie" gilt fuer die Kombinationen, und es muss
            # ausser "Breit" (alle Tuerme) mindestens zwei tragfaehige Wege geben (man soll nicht alle Tuerme brauchen)
            "Ziel_KeineDominanz": max(quote.values()) <= Z["ZielLinieMax"],
            "Ziel_KeineToteLinie": min([q for l, q in quote.items() if not l.startswith("Mono")] or [1.0]) >= Z["ZielLinieMin"],
            "Ziel_NichtAlleTuerme": len([l for l in tragfaehig if l != "Breit"]) >= 2,
            "Dominant": [l for l, q in quote.items() if q > Z["ZielLinieMax"]],
            "Tot": [l for l, q in quote.items() if q < Z["ZielLinieMin"] and not l.startswith("Mono")],
            **wirtschaft_ziele(ergebnisse, Z)}


def wirtschaft_ziele(ergebnisse, Z):
    """W2-W6 aus dem Testplan (Notizen/Testplan-Balance-Kreislauf.md); W1 rechnet balance_kreislauf.py am festen Plan"""
    erfolg = [r for r in ergebnisse if r["Erfolg"]] or ergebnisse
    def median(xs):
        xs = list(xs)
        return statistics.median(xs) if xs else 0.0
    ueberschuss = median(r["EP_Rest"] / max(1.0, r["EP_Summe"]) for r in erfolg)
    fabrik = median(r.get("EP_Fabrik", 0) / max(1.0, sum(v for k, v in r.items() if k.startswith("EP_") and k not in ("EP_Rest", "EP_Summe"))) for r in erfolg)
    forschung = median(r["Forschung"] / Z.get("KnotenGesamt", 44) for r in erfolg)
    verzug = median(max(0, r["HauptAb"] - r["HauptFrei"]) for r in ergebnisse)
    # Not = in mindestens einer Welle weniger als NotVersorgung (80 %) der Munition, weil EP fehlten
    not_anteil = sum(1 for r in ergebnisse if r.get("VersorgungMin", 1.0) < Z.get("NotVersorgung", 0.8)) / len(ergebnisse)
    return {"W_Ueberschuss": ueberschuss, "W_Fabrik": fabrik, "W_Forschung": forschung, "W_Verzug": verzug, "W_Not": not_anteil,
            "Ziel_W2": ueberschuss <= Z["ZielUeberschussMax"], "Ziel_W3": Z["ZielFabrikMin"] <= fabrik <= Z["ZielFabrikMax"],
            "Ziel_W4": verzug <= Z["ZielVerzugMax"], "Ziel_W5": Z["ZielForschungMin"] <= forschung <= Z["ZielForschungMax"],
            "Ziel_W6": not_anteil <= Z["ZielNotMax"]}


def bericht(ergebnisse, quelle, stufe, laenge, Z, pfad):
    zb = zielbild(ergebnisse, Z)

    def gruppe(feld):
        g = collections.defaultdict(list)
        for r in ergebnisse:
            g[r[feld]].append(r)
        zeilen = []
        for k, rs in sorted(g.items(), key=lambda kv: -sum(r["Erfolg"] for r in kv[1]) / len(kv[1])):
            erfolg = [r for r in rs if r["Erfolg"]]
            fehl = [r for r in rs if not r["Erfolg"]]
            ursachen = collections.Counter(r["Ursache"] for r in fehl)
            zeilen.append((k, len(rs), len(erfolg) / len(rs), statistics.median([r["Welle"] for r in fehl]) if fehl else None,
                           statistics.median([r["KernMin"] for r in erfolg]) if erfolg else None,
                           ", ".join(f"{u} {c}" for u, c in ursachen.most_common(3))))
        return zeilen

    def tabelle(titel, feld):
        z = gruppe(feld)
        s = f"<h2>{titel}</h2><div class=tab><table><tr><th>{titel}</th><th class=z>Läufe</th><th class=z>Erfolg</th><th class=z>Scheitern (Median-Welle)</th><th class=z>Kern-Tiefpunkt der Erfolgreichen</th><th>Ursachen</th></tr>"
        for k, n, q, w, km, u in z:
            cls = "gut" if q >= Z["ZielLinieQuote"] else "warn" if q >= 0.2 else "schlecht"
            s += f"<tr><td>{k}</td><td class=z>{n}</td><td class=z><span class='p {cls}'>{q * 100:.0f} %</span></td><td class=z>{w if w else '–'}</td><td class=z>{f'{km:.0f} %' if km is not None else '–'}</td><td>{u}</td></tr>"
        return s + "</table></div>"

    beste = sorted({r["Nr"]: r for r in ergebnisse}.values(), key=lambda r: 0)
    je_strat = collections.defaultdict(list)
    for r in ergebnisse:
        je_strat[r["Nr"]].append(r)
    top = sorted(je_strat.values(), key=lambda rs: (-sum(r["Erfolg"] for r in rs), -statistics.mean(r["KernMin"] for r in rs)))[:15]
    ziel = [("Mehrere Wege tragen", zb["Ziel_Wege"], f"{len(zb['Tragfaehig'])} Linien mit ≥ {Z['ZielLinieQuote'] * 100:.0f} % Erfolg (Ziel ≥ {Z['ZielWegeMin']}): {', '.join(zb['Tragfaehig']) or 'keine'}"),
            ("Nicht alles funktioniert", zb["Ziel_NichtAlles"], f"Erfolg gesamt {zb['Quote'] * 100:.0f} % (Ziel ≤ {Z['ZielQuoteMax'] * 100:.0f} %)"),
            ("Keine Linie dominiert", zb["Ziel_KeineDominanz"], f"höchste Linie {max(zb['JeLinie'].values()) * 100:.0f} % (Ziel ≤ {Z['ZielLinieMax'] * 100:.0f} %): {', '.join(zb['Dominant']) or '–'}"),
            ("Keine Linie ist tot", zb["Ziel_KeineToteLinie"], f"niedrigste Linie {min(zb['JeLinie'].values()) * 100:.0f} % (Ziel ≥ {Z['ZielLinieMin'] * 100:.0f} %): {', '.join(zb['Tot']) or '–'}"),
            ("Nicht zu leicht", zb["Ziel_NichtZuLeicht"], f"{zb['ZuLeichtAnteil'] * 100:.0f} % der Läufe gewinnen mit Kern nie unter {Z['ZuLeichtKern']} % (Ziel ≤ {Z['ZielZuLeichtMax'] * 100:.0f} %)")]
    html = f"""<!doctype html><meta charset=utf-8><title>Strategie-Prüfstand {stufe}</title>
<style>body{{font:14px/1.5 'Noto Sans',system-ui,sans-serif;margin:24px auto;max-width:1150px;padding:0 16px;color:#24221f;background:#f4f2ef}}
h1,h2{{font-family:'Barlow Semi Condensed','Arial Narrow',sans-serif}} table{{border-collapse:collapse;width:100%;font-size:13px}} th,td{{padding:4px 8px;border-bottom:1px solid #d9d5ce;text-align:left;white-space:nowrap}}
.z{{text-align:right;font-variant-numeric:tabular-nums}} .tab{{overflow-x:auto;background:#fff;border:1px solid #d9d5ce;border-radius:4px;padding:6px}}
.p{{padding:1px 7px;border-radius:9px;font-weight:600;font-size:12px}} .gut{{background:#dcefdc;color:#2f7a3a}} .warn{{background:#f6ebc9;color:#8a6410}} .schlecht{{background:#f6d9d3;color:#a3392a}}
.ziel{{display:grid;gap:6px;margin:8px 0}} .hin{{color:#6f6a63;font-size:12.5px;max-width:95ch}}</style>
<h1>Strategie-Prüfstand: {stufe}, {laenge} Wellen</h1>
<p class=hin>{len(ergebnisse)} Läufe ({len(je_strat)} Strategien × {len(ergebnisse) // max(1, len(je_strat))} Startwerte). Balance: {quelle}. Annahmen: strategie_annahmen.json. Kernverlust-Modell ist eine Annahme (siehe dort).</p>
<h2>Zielbild</h2><div class=ziel>{''.join(f"<div><span class='p {'gut' if ok else 'schlecht'}'>{'erfüllt' if ok else 'verfehlt'}</span> <b>{t}</b>: {d}</div>" for t, ok, d in ziel)}</div>
{tabelle('Turmlinie', 'Linie')}{tabelle('Doktrin', 'Doktrin')}{tabelle('Ausgabestil', 'Stil')}{tabelle('Kern-Ausbau', 'Kern')}{tabelle('Gabelungen', 'Gabel')}
<h2>Beste 15 Strategien</h2><div class=tab><table><tr><th>Linie</th><th>Doktrin</th><th>Stil</th><th>Kern</th><th>Gabelungen</th><th class=z>Erfolg</th><th class=z>Kern-Tiefpunkt</th><th class=z>Türme</th><th class=z>Ø Level</th><th class=z>EP übrig</th></tr>
{''.join(f"<tr><td>{rs[0]['Linie']}</td><td>{rs[0]['Doktrin']}</td><td>{rs[0]['Stil']}</td><td>{rs[0]['Kern']}</td><td>{rs[0]['Gabel']}</td><td class=z>{sum(r['Erfolg'] for r in rs)}/{len(rs)}</td><td class=z>{statistics.mean(r['KernMin'] for r in rs):.0f} %</td><td class=z>{statistics.mean(r['Tuerme'] for r in rs):.0f}</td><td class=z>{statistics.mean(r['Level'] for r in rs):.1f}</td><td class=z>{statistics.mean(r['EP_Rest'] for r in rs):.0f}</td></tr>" for rs in top)}</table></div>
"""
    open(pfad, "w", encoding="utf-8").write(html)
    return zb


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stufe", default="Normal")
    ap.add_argument("--wellen", type=int, default=50)
    ap.add_argument("--laeufe", type=int, default=3)
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--stand", choices=["090", "091"], default="090",
                    help="090: ohne Endgame-Pakete E1-E3 (Forschungsbaum 0.9.0, bisheriges Verhalten); 091: mit E1-E3, Ausgabe *_091")
    a = ap.parse_args()
    ergebnisse, quelle = massenlauf(a.stufe, a.wellen, a.laeufe, a.seed, stand=a.stand)
    os.makedirs(os.path.join(HIER, "Ausgabe"), exist_ok=True)
    name = f"Strategien_{a.stufe}" + ("_091" if a.stand == "091" else "")
    pfad = os.path.join(HIER, "Ausgabe", f"{name}.csv")
    felder = sorted({k for r in ergebnisse for k in r})
    with open(pfad, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=felder, delimiter=";")
        w.writeheader()
        w.writerows(ergebnisse)
    Z = lade_alles(stand=a.stand)[2]
    zb = bericht(ergebnisse, quelle, a.stufe, a.wellen, Z, os.path.join(HIER, "Ausgabe", f"{name}.html"))
    print(f"{len(ergebnisse)} Läufe, Erfolg {zb['Quote'] * 100:.0f} %, tragfähige Linien: {', '.join(zb['Tragfaehig']) or 'keine'}")
    for l, q in sorted(zb["JeLinie"].items(), key=lambda kv: -kv[1]):
        print(f"  {l:16s} {q * 100:5.0f} %")


if __name__ == "__main__":
    main()
