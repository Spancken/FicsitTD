"""FicsitTD P12 (Bericht-P12-Balancing-Methode-2026-11-17.md, Abschnitt 5): Kalibrierplaene K1-K6 und Paket P12-K erzeugen.

  python erzeuge_p12k.py   -> Tests\\Plaene\\P12-K*.json und Tests\\Pakete\\P12-K.json

Alle Laeufe mit gedeckeltem Zeitraffer (Selbsttest, Test.MaxSchritt), Wellen mit Gueltigkeit je Welle (FabrikTakt_Anteil).
Entscheidungen Marius 17.11.: Referenzpause 195 s, alle Stufen testen.
"""
import copy
import json
import os

import erzeuge_referenzen as er

HIER = os.path.dirname(os.path.abspath(__file__))
PLAENE = os.path.join(HIER, "Plaene")
PAUSE = 195

# K4 Linien-Probe: je 6 Tuerme L2 einer Linie gegen dieselben Wellen 15-19 (Mehrziel-Faktoren, Befund B9 Flamme)
LINIEN = {
    "Bolzen": ("Bolzenturm", "Recipe_TD_Shoot_SpikedRebar", None),
    "Gewehr": ("Gewehrturm", "Recipe_TD_Gun_Standard", None),
    "Nobelisk": ("Nobelisk-Werfer", "Recipe_TD_Nobelisk_Explosive", None),
    "Tesla": ("Teslaspule", "Recipe_TD_Tesla_Discharge", None),
    "Flamme": ("Flammenwerfer", "Recipe_TD_Flame_Fuel", "innen"),
}

# K5: Mischbasis wie Referenz-Misch, dazu Ausbau fuer Phase 4-5 (Vorschlag Hauptsession 17.11., Level 4 mit Turmfertigung III)
MISCH_SPAET = {
    30: [("Gewehrturm", 2, 4, "Recipe_TD_Gun_Standard"), ("Teslaspule", 1, 4, "Recipe_TD_Tesla_Discharge")],
    35: [("Nobelisk-Werfer", 1, 4, "Recipe_TD_Nobelisk_Explosive"), ("Bolzenturm", 1, 4, "Recipe_TD_Shoot_Durchschlag")],
    40: [("Flammenwerfer", 1, 4, "Recipe_TD_Flame_Fuel", "innen"), ("Gewehrturm", 1, 4, "Recipe_TD_Gun_Standard")],
    45: [("Teslaspule", 1, 4, "Recipe_TD_Tesla_Discharge"), ("Nobelisk-Werfer", 1, 4, "Recipe_TD_Nobelisk_Explosive")],
}


def kopf(name, hinweis, minuten):
    return {"Name": name, "_Hinweis": hinweis + " Erzeugt von Tests\\erzeuge_p12k.py.",
            "NeuesSpiel": {"Startort": "Rocky Desert", "Modus": 1, "Stufe": 2}, "ErweiterteEinstellungen": False,
            "WarteNachLaden": 60, "MaxMinuten": minuten}


def start_normal():
    return [{"pruefe": "Version", "soll": "{VERSION}", "text": "richtige Mod-Version geladen"}, {"warte": 15},
            {"befehl": "td einsatz stop"}, {"befehl": "td kern stop"}, {"befehl": "td einsatz start normal 50"},
            {"befehl": "td einsatz frei an"}, {"stromfrei": True}, {"versorgung": er.SATT}]


def alle_forschung():
    return [{"erforsche": k} for ab in sorted(er.FORSCHUNG) for k in er.FORSCHUNG[ab]]


def k1(name, pause):
    p = json.load(open(os.path.join(PLAENE, "Fabrik-P10-Abnahme.json"), encoding="utf-8"))
    s = [x for x in p["Schritte"] if "wellenlauf" not in x and x.get("pruefe") not in ("LogEnthaelt",)]
    i = next(k for k, x in enumerate(s) if x.get("foto"))
    lauf = {"wellenlauf": 10, "zeitraffer": 4, "max": 900}
    if pause:
        lauf["pause"] = pause
    s = s[:i + 1] + [lauf, {"fabrikbericht": True}] + s[i + 1:]
    p.update(kopf(name, ("K1" if pause else "K2") + " (P12): voller Knoten, 4 Bolzentuerme L2, echter Strom, echte Kosten, bis W10"
                  + (f", Pause {pause} s zwischen den Wellen." if pause else ", ohne Pause (Grenzfall Fabrikdurchsatz)."), 40))
    p["NeuesSpiel"] = {"Startort": "Grass Fields", "Modus": 1, "Stufe": 2}
    p["Schritte"] = s
    return p


def k4(linie):
    turm, rezept, ring = LINIEN[linie]
    s = start_normal() + alle_forschung()
    s.append({"baue": turm, "anzahl": 6, "level": 2, "rezept": rezept, **({"ring": ring} if ring else {})})
    s += [{"befehl": "td kern springe 15"}, {"wellenlauf": 19, "zeitraffer": 4, "max": 500},
          {"ueberblick": True},
          {"pruefe": "KeineWarnungen", "text": "keine Warnungen oder Fehler des Mods im Log"}]
    p = kopf(f"P12-K4-{linie}", f"K4 Linien-Probe (P12): 6 x {turm} L2 gegen die Wellen 15-19, Munition satt, Strom frei.", 20)
    p["Schritte"] = s
    return p


def k5():
    ausbau = dict(er.MISCH)
    ausbau.update(MISCH_SPAET)
    s = start_normal() + alle_forschung()
    for welle in sorted(ausbau):
        if welle <= 30:
            for t, n, lvl, rez, *ring in ausbau[welle]:
                s.append({"baue": t, "anzahl": n, "level": lvl, "rezept": rez, **({"ring": ring[0]} if ring else {})})
    s += [{"ueberblick": True}, {"foto": "P12-K5_W30"}, {"befehl": "td kern springe 31"}]
    for bis in (35, 40, 45, 50):
        s.append({"wellenlauf": bis, "zeitraffer": 4, "max": 900})
        for t, n, lvl, rez, *ring in ausbau.get(bis, []):
            s.append({"baue": t, "anzahl": n, "level": lvl, "rezept": rez, **({"ring": ring[0]} if ring else {})})
    s += [{"ueberblick": True}, {"foto": "P12-K5_W50"},
          {"pruefe": "KeineWarnungen", "text": "keine Warnungen oder Fehler des Mods im Log"}]
    p = kopf("P12-K5-Misch-W31-50", "K5 (P12): Mischbasis wie Referenz-Misch mit Ausbau fuer Phase 4-5 (Level 4), Stand W30 sofort gebaut, "
             "Sprung auf W31, Wellen 31-50. Prueft das kalibrierte Modell, wird nicht gefittet.", 60)
    p["Schritte"] = s
    return p


def k6(stufe):
    p = json.load(open(os.path.join(PLAENE, f"Strat-Gewehr-Tesla-{stufe}.json"), encoding="utf-8"))
    s = []
    for x in p["Schritte"]:
        if "wellenlauf" in x and x["wellenlauf"] >= 15:
            y = dict(x)
            y["wellenlauf"] = 15
            s.append(y)
            break
        s.append(x)
    s.append({"pruefe": "KeineWarnungen", "text": "keine Warnungen oder Fehler des Mods im Log"})
    q = copy.deepcopy(p)
    q["Name"] = f"P12-K6-Gewehr-Tesla-{stufe}"
    q["_Hinweis"] = f"K6 (P12): Strat-Gewehr-Tesla auf {stufe}, Stufe wirklich gesetzt, Wellen 1-15. Erzeugt von Tests\\erzeuge_p12k.py."
    q["MaxMinuten"] = 30
    q["Schritte"] = s
    return q


def main():
    plaene = [k1("P12-K1-Bolzen", PAUSE), k1("P12-K2-Bolzen-ohnePause", 0)] + [k4(l) for l in LINIEN] + [k5(), k6("Hart"), k6("Albtraum")]
    for p in plaene:
        json.dump(p, open(os.path.join(PLAENE, p["Name"] + ".json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    reihenfolge = ["P12-K1-Bolzen", "P12-K1-Bolzen", "Strat-Gewehr-Tesla"] + [f"P12-K4-{l}" for l in LINIEN] + \
                  ["P12-K5-Misch-W31-50", "P12-K2-Bolzen-ohnePause", "P12-K6-Gewehr-Tesla-Hart", "P12-K6-Gewehr-Tesla-Albtraum"]
    json.dump({"Name": "P12-K", "_Hinweis": "Kalibrierfenster P12 (Bericht Abschnitt 5/8): K1 zweimal, K3, K4, K5, danach K2 und K6. "
               "Keine Balance-Aenderung im Fenster. Erzeugt von Tests\\erzeuge_p12k.py.", "Plaene": reihenfolge},
              open(os.path.join(HIER, "Pakete", "P12-K.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    print(f"{len(plaene)} Plaene, Paket P12-K mit {len(reihenfolge)} Laeufen")


if __name__ == "__main__":
    main()
