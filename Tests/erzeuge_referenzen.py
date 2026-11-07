"""FicsitTD Pruefstand (M1): Referenzplaene erzeugen. Eine Stelle fuer den Ablauf, Varianten nur ueber Parameter.

  python erzeuge_referenzen.py   -> Tests\\Plaene\\Referenz-*.json und Tests\\Pakete\\Pruefstand.json
Alle Referenzen: neues Spiel Felsenwueste, Einsatz Normal 50, Strom frei, Munition per Versorgung (ersetzt die Fabrik).
"""
import json
import os

HIER = os.path.dirname(os.path.abspath(__file__))

# Forschung, damit Tuerme und Munition wie im Spiel freigeschaltet sind (Voraussetzungen zuerst)
FORSCHUNG = {
    5: ["Assembler", "Kupfer", "Durchschlagbolzen", "Gewehrturm", "Kohlekraft", "Teslaspule", "Giesserei"],
    10: ["NobeliskWerfer", "Turmfertigung2"],
    15: ["Raffinerie", "Flammenwerfer"],   # 0.82: Knoten Treibstoff entfaellt, der Flammenwerfer bringt den Treibstoff
    20: ["Turmfertigung3"],
}

# Ausbau der Mischbasis nach Welle: (Turm, Anzahl, Level, Rezept[, Ring])
# 0.78.1: Flammenwerfer (20 m) auf den inneren Ring, auf 16 m hatten sie nur 32 % der Zeit ein Ziel (Pruefstand 05.11. 11:30)
MISCH = {
    0: [("Bolzenturm", 2, 1, "Recipe_TD_Shoot_SpikedRebar")],
    4: [("Bolzenturm", 1, 2, "Recipe_TD_Shoot_Durchschlag"), ("Gewehrturm", 1, 2, "Recipe_TD_Gun_Standard")],
    8: [("Gewehrturm", 1, 2, "Recipe_TD_Gun_Standard"), ("Teslaspule", 1, 2, "Recipe_TD_Tesla_Discharge")],
    12: [("Nobelisk-Werfer", 1, 2, "Recipe_TD_Nobelisk_Explosive"), ("Gewehrturm", 1, 3, "Recipe_TD_Gun_Standard")],
    16: [("Flammenwerfer", 2, 2, "Recipe_TD_Flame_Fuel", "innen"), ("Teslaspule", 1, 3, "Recipe_TD_Tesla_Discharge")],
    20: [("Nobelisk-Werfer", 1, 3, "Recipe_TD_Nobelisk_Explosive"), ("Gewehrturm", 2, 3, "Recipe_TD_Gun_Standard")],
    25: [("Flammenwerfer", 1, 3, "Recipe_TD_Flame_Fuel", "innen"), ("Bolzenturm", 1, 3, "Recipe_TD_Shoot_Durchschlag")],
}
# Nur Gewehrtuerme, ab W15 mit Doktrin (Spezialisierung: jede Linie soll allein tragen)
GEWEHR = {
    0: [("Bolzenturm", 2, 1, "Recipe_TD_Shoot_SpikedRebar")],
    4: [("Gewehrturm", 2, 2, "Recipe_TD_Gun_Standard")],
    8: [("Gewehrturm", 2, 2, "Recipe_TD_Gun_Standard")],
    12: [("Gewehrturm", 2, 3, "Recipe_TD_Gun_Standard")],
    16: [("Gewehrturm", 2, 3, "Recipe_TD_Gun_Standard")],
    20: [("Gewehrturm", 2, 4, "Recipe_TD_Gun_Standard")],
    25: [("Gewehrturm", 2, 4, "Recipe_TD_Gun_Standard")],
}
# 0.79 Gegenpruefung Strategie-Pruefstand (06.11.): reine Tesla- und Flammenbasis, jeweils mit Doktrin ab W15
TESLA = {
    0: [("Bolzenturm", 2, 1, "Recipe_TD_Shoot_SpikedRebar")],
    4: [("Bolzenturm", 1, 2, "Recipe_TD_Shoot_Durchschlag")],
    8: [("Teslaspule", 2, 2, "Recipe_TD_Tesla_Discharge")],
    12: [("Teslaspule", 2, 3, "Recipe_TD_Tesla_Discharge")],
    16: [("Teslaspule", 2, 3, "Recipe_TD_Tesla_Discharge")],
    20: [("Teslaspule", 2, 4, "Recipe_TD_Tesla_Discharge")],
    25: [("Teslaspule", 2, 4, "Recipe_TD_Tesla_Discharge")],
}
FLAMME = {
    0: [("Bolzenturm", 2, 1, "Recipe_TD_Shoot_SpikedRebar")],
    4: [("Bolzenturm", 1, 2, "Recipe_TD_Shoot_Durchschlag")],
    8: [("Bolzenturm", 1, 2, "Recipe_TD_Shoot_Durchschlag")],
    12: [("Bolzenturm", 1, 3, "Recipe_TD_Shoot_Durchschlag")],
    16: [("Flammenwerfer", 3, 2, "Recipe_TD_Flame_Fuel", "innen")],
    20: [("Flammenwerfer", 2, 3, "Recipe_TD_Flame_Fuel", "innen")],
    25: [("Flammenwerfer", 2, 4, "Recipe_TD_Flame_Fuel", "innen")],
}
# Vorschlag aus Notizen/Balance-Vorschlag-2026-11-06.md (Tesla-Strom ist im Pruefstand frei, deshalb nur der Schaden)
STANDARD = {"Munition.Tesla.Schaden": 300, "Munition.Treibstoff.Biotreibstoff.SchadenProSekunde": 35,
            "Munition.Treibstoff.Treibstoff.SchadenProSekunde": 80, "Munition.Treibstoff.Turbotreibstoff.SchadenProSekunde": 80}
VORSCHLAG = {"Munition.Tesla.Schaden": 150, "Munition.Treibstoff.Biotreibstoff.SchadenProSekunde": 52,
             "Munition.Treibstoff.Treibstoff.SchadenProSekunde": 120, "Munition.Treibstoff.Turbotreibstoff.SchadenProSekunde": 120}

# Munition je Turm und Minute. Satt: Fabrik deckt den Bedarf. Faul: Fabrik wird nach dem Start nicht ausgebaut.
# Pruefstand 05.11.: 10 Patronen/min reichten in Phase 3 nicht (Gewehr 738 s ohne Munition) -> Dauerbedarf: Bolzen 1/1,2 s = 50/min,
# Gewehr 15 Schuss je Patrone bei 6/s = 24/min, Nobelisk 1/4 s = 15/min, Flamme 100 je s = 6000/min
SATT = {"Bolzenturm": 50, "Gewehrturm": 25, "Nobelisk-Werfer": 15, "Flammenwerfer": 6000}
FAUL = {"Bolzenturm": 8, "Gewehrturm": 3, "Nobelisk-Werfer": 2, "Flammenwerfer": 1500}
ABSCHNITTE = [4, 8, 12, 16, 20, 25, 30]


def plan(name, hinweis, ausbau, versorgung, extra=None, bis=30):
    s = [
        {"pruefe": "Version", "soll": "{VERSION}", "text": "richtige Mod-Version geladen"},
        {"warte": 15},
        {"befehl": "td einsatz stop"}, {"befehl": "td kern stop"},
        {"befehl": "td einsatz start normal 50"}, {"befehl": "td einsatz frei an"},
        {"stromfrei": True},
        {"versorgung": versorgung},
    ]
    erledigt = 0
    for welle in [0] + [a for a in ABSCHNITTE if a <= bis]:
        if welle > 0:
            s.append({"wellenlauf": welle, "zeitraffer": 4, "max": 500})
            if welle >= bis:
                break
        for ab, knoten in FORSCHUNG.items():
            if erledigt < ab <= welle + 5:
                s += [{"erforsche": k} for k in knoten]
        erledigt = welle + 5
        for t, n, lvl, rez, *ring in ausbau.get(welle, []):
            s.append({"baue": t, "anzahl": n, "level": lvl, "rezept": rez, **({"ring": ring[0]} if ring else {})})
        for e in (extra or {}).get(welle, []):
            s.append(e)
        if welle in (0, 15):
            s += [{"ueberblick": True}, {"foto": f"{name}_W{welle:02d}"}]
    s += [{"ueberblick": True}, {"foto": f"{name}_W{bis:02d}"},
          {"pruefe": "KernLebt", "text": f"Kern hat Welle {bis} überstanden"},
          {"pruefe": "Sicherheitsnetze", "text": "kein Sicherheitsnetz ausgelöst (Versetzen, Absturz, 0 LP)"},
          {"pruefe": "KeineWarnungen", "text": "keine Warnungen oder Fehler des Mods im Log"}]
    return {"Name": name, "_Hinweis": hinweis + " Erzeugt von Tests\\erzeuge_referenzen.py.",
            "NeuesSpiel": {"Startort": "Rocky Desert"}, "ErweiterteEinstellungen": False, "WarteNachLaden": 60, "MaxMinuten": 45,
            "Schritte": s}


def main():
    plaene = [
        plan("Referenz-Misch", "Ausgewogene Basis aller Turmarten, Munition reicht.", MISCH, SATT),
        plan("Referenz-Faul", "Gleiche Basis wie Misch, aber die Fabrik wird nach dem Start nicht ausgebaut (wenig Munition). Zeigt, ob Munitionsmangel bindet.", MISCH, FAUL),
        plan("Referenz-Gewehr", "Nur Gewehrtürme (nach dem Start), ab Welle 16 mit Doktrin Gewehrturm (bis 0.79 nie ausgeführt: 15 war kein Abschnitt). Zeigt, ob eine Linie allein trägt.", GEWEHR, SATT,
             extra={16: [{"erforsche": "DoktrinGewehr"}]}),
    ]
    for name, ausbau, dok in (("Tesla", TESLA, "DoktrinTesla"), ("Flamme", FLAMME, "DoktrinFlamme")):
        plaene.append(plan(f"Gegen-{name}", f"Gegenpruefung Strategie-Pruefstand: nur {name} (Bolzen bis zur Freigabe), Doktrin ab W15, Balance-Stand.",
                           ausbau, SATT, extra={0: [{"balance": STANDARD}], 16: [{"erforsche": dok}]}))
        plaene.append(plan(f"Gegen-{name}-Vorschlag", f"Wie Gegen-{name}, mit dem Balance-Vorschlag vom 06.11. (Tesla-Schaden 150, Flamme +50 %).",
                           ausbau, SATT, extra={0: [{"balance": VORSCHLAG}], 16: [{"erforsche": dok}]}))
    # 0.79 Breite Front (Option, Standard aus): gleiche Mischbasis mit eingeschalteter Option, fuer den A/B-Vergleich
    plaene.append(plan("Referenz-Misch-Breit", "Wie Referenz-Misch, aber mit der Option Breite Front (Querversatz, Tempo-Streuung, Abstand halten).",
                       MISCH, SATT, extra={0: [{"balance": {"Wellen.BreiteFront": 1}}]}))
    for p in plaene:
        with open(os.path.join(HIER, "Plaene", p["Name"] + ".json"), "w", encoding="utf-8") as f:
            json.dump(p, f, ensure_ascii=False, indent=2)
    with open(os.path.join(HIER, "Pakete", "Pruefstand.json"), "w", encoding="utf-8") as f:
        json.dump({"Name": "Pruefstand", "_Hinweis": "Referenzläufe bis Welle 30 (M1), je Plan etwa 15 min.",
                   "Plaene": [p["Name"] for p in plaene if p["Name"].startswith("Referenz") and not p["Name"].endswith("-Breit")]}, f, ensure_ascii=False, indent=2)
    with open(os.path.join(HIER, "Pakete", "Breite.json"), "w", encoding="utf-8") as f:
        json.dump({"Name": "Breite", "_Hinweis": "A/B Breite Front: Referenz-Misch ohne und mit Option, je 2-mal, etwa 1 h.",
                   "Plaene": ["Referenz-Misch", "Referenz-Misch-Breit"] * 2}, f, ensure_ascii=False, indent=2)
    with open(os.path.join(HIER, "Pakete", "Gegenpruefung.json"), "w", encoding="utf-8") as f:
        json.dump({"Name": "Gegenpruefung", "_Hinweis": "Strategie-Pruefstand gegen das Spiel: Tesla und Flamme, Stand gegen Vorschlag, etwa 50 min.",
                   "Plaene": ["Gegen-Tesla", "Gegen-Tesla-Vorschlag", "Gegen-Flamme", "Gegen-Flamme-Vorschlag"]}, f, ensure_ascii=False, indent=2)
    # 0.78.2 Streuung: jeder Plan 5-mal (Ziel: Kern-Tiefpunkt schwankt um hoechstens 25 Punkte), etwa 3 h
    with open(os.path.join(HIER, "Pakete", "Streuung.json"), "w", encoding="utf-8") as f:
        json.dump({"Name": "Streuung", "_Hinweis": "Jeder Referenzplan 5-mal (Streuung der Wellen), etwa 3 h ohne Aufsicht.",
                   "Plaene": [p["Name"] for p in plaene if p["Name"].startswith("Referenz") and not p["Name"].endswith("-Breit")] * 5}, f, ensure_ascii=False, indent=2)
    print("Referenzen:", ", ".join(f"{p['Name']} ({len(p['Schritte'])} Schritte)" for p in plaene))


if __name__ == "__main__":
    main()
