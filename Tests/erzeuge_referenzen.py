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
    15: ["Raffinerie", "Flammenwerfer", "Treibstoff"],
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
        plan("Referenz-Gewehr", "Nur Gewehrtürme (nach dem Start), ab Welle 15 mit Doktrin Gewehrturm. Zeigt, ob eine Linie allein trägt.", GEWEHR, SATT,
             extra={15: [{"erforsche": "DoktrinGewehr"}]}),
    ]
    # 0.78.2 Ursachen-Versuch Absturz: gleiche Mischbasis ohne Ladequelle am Kern (Zustand bis 0.78.1) gegen mit Ladequelle
    plaene.append(plan("Ursache-Ladequelle-aus", "Wie Referenz-Misch, aber ohne Ladequelle am Kern (A/B-Versuch: fallen die Gegner nur ohne sie durch?).",
                       MISCH, SATT, extra={0: [{"balance": {"Kern.Ladequelle": 0}}]}))
    for p in plaene:
        with open(os.path.join(HIER, "Plaene", p["Name"] + ".json"), "w", encoding="utf-8") as f:
            json.dump(p, f, ensure_ascii=False, indent=2)
    with open(os.path.join(HIER, "Pakete", "Pruefstand.json"), "w", encoding="utf-8") as f:
        json.dump({"Name": "Pruefstand", "_Hinweis": "Referenzläufe bis Welle 30 (M1), je Plan etwa 15 min.",
                   "Plaene": [p["Name"] for p in plaene if p["Name"].startswith("Referenz")]}, f, ensure_ascii=False, indent=2)
    with open(os.path.join(HIER, "Pakete", "Ursache.json"), "w", encoding="utf-8") as f:
        json.dump({"Name": "Ursache", "_Hinweis": "A/B-Versuch Absturz: Mischbasis ohne und mit Ladequelle am Kern, etwa 30 min.",
                   "Plaene": ["Ursache-Ladequelle-aus", "Referenz-Misch"]}, f, ensure_ascii=False, indent=2)
    # 0.78.2 Streuung: jeder Plan 5-mal (Ziel: Kern-Tiefpunkt schwankt um hoechstens 25 Punkte), etwa 3 h
    with open(os.path.join(HIER, "Pakete", "Streuung.json"), "w", encoding="utf-8") as f:
        json.dump({"Name": "Streuung", "_Hinweis": "Jeder Referenzplan 5-mal (Streuung der Wellen), etwa 3 h ohne Aufsicht.",
                   "Plaene": [p["Name"] for p in plaene if p["Name"].startswith("Referenz")] * 5}, f, ensure_ascii=False, indent=2)
    print("Referenzen:", ", ".join(f"{p['Name']} ({len(p['Schritte'])} Schritte)" for p in plaene))


if __name__ == "__main__":
    main()
