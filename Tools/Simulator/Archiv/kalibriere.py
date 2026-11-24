"""FicsitTD-Simulator kalibrieren (Roadmap M1): die Referenzbasen des Pruefstands nachrechnen und mit den Messwerten vergleichen.

  python kalibriere.py [Messung.csv]
Ohne Argument wird die neueste `*_Pruefstand_Metriken.csv` aus Satisfactory\\Testberichte genommen.
Verglichen werden je Welle getrennt: Gegner-LP (Wellenmodell), Abwehr-DPS K (Turmmodell) und Q. Aus den Verhaeltnissen
Messung / Simulator entstehen Korrekturfaktoren, die in `annahmen.json` unter "Kalibrierung" eingetragen werden koennen.
Ausgabe: Ausgabe\\Kalibrierung.md
"""
import csv
import glob
import json
import os
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # Repo-Wurzel (Tools/Simulator/..)
import random
import statistics
import sys

import simulator as sim

HIER = os.path.dirname(os.path.abspath(__file__))
BERICHTE = r"D:\OneDrive\Claude\Claude Modding\Satisfactory\Testberichte"
TESTS = os.path.join(REPO, "Tests")
MUNITION = {"Recipe_TD_Shoot_SpikedRebar": "Bolzen.Armierungsbolzen", "Recipe_TD_Shoot_Durchschlag": "Bolzen.Durchschlagbolzen",
            "Recipe_TD_Gun_Standard": "Patronen.Standard", "Recipe_TD_Tesla_Discharge": "Tesla",
            "Recipe_TD_Nobelisk_Explosive": "Nobelisk.Nobelisk", "Recipe_TD_Flame_Fuel": "Treibstoff.Treibstoff"}
ART = {"Bolzenturm": "Durchschlag", "Gewehrturm": "Kugel", "Teslaspule": "Strom", "Flammenwerfer": "Feuer", "Nobelisk-Werfer": "Explosion"}
ZYKLUS = {"Bolzenturm": 1.2, "Teslaspule": 4.0, "Nobelisk-Werfer": 4.0}


def level_bonus(B, turm, level, wert):
    return sum(B.get(f"Tuerme.Level.{turm}.L{k}.{wert}", 0.0) for k in range(2, level + 1))


def nenn_dps(B, A, turm, level, rezept):
    """wie ATD*::NennDPS (ohne Doktrin): Schaden x Level-Bonus x Takt"""
    k = "Munition." + MUNITION[rezept]
    sb = 1 + level_bonus(B, turm, level, "Schaden")
    tb = 1 + level_bonus(B, turm, level, "Takt")
    if turm == "Gewehrturm":
        return B[k + ".Schaden"] * sb * B[k + ".SchussProSekunde"] * tb
    if turm == "Flammenwerfer":
        return B[k + ".SchadenProSekunde"] * sb
    return B[k + ".Schaden"] * sb * tb / ZYKLUS[turm]


def basis_je_welle(plan):
    """Tuerme, die zu Beginn jeder Welle stehen: (Turm, Level, Rezept)"""
    stehen, welle, je = [], 0, {}
    for s in plan["Schritte"]:
        if "baue" in s:
            stehen += [(s["baue"], s.get("level", 1), s.get("rezept"))] * s.get("anzahl", 1)
        if "wellenlauf" in s:
            for n in range(welle + 1, s["wellenlauf"] + 1):
                je[n] = list(stehen)
            welle = s["wellenlauf"]
    return je


def main():
    B, quelle = sim.lade_balance(os.path.join(sim.SAVED, "FicsitTD", "Balance.json"))
    A = json.load(open(os.path.join(HIER, "annahmen.json"), encoding="utf-8"))
    katalog, phasen = sim.lade_katalog(B)
    datei = sys.argv[1] if len(sys.argv) > 1 else max(glob.glob(os.path.join(BERICHTE, "*Pruefstand_Metriken.csv")), key=os.path.getmtime)
    zeilen = list(csv.DictReader(open(datei, encoding="utf-8"), delimiter=";"))
    # 17.11. (P12): seit den Spalten Plan/Lauf ordnet die CSV selbst zu; aeltere Dateien wie bisher ueber das Paket Pruefstand
    if zeilen and zeilen[0].get("Plan") not in (None, "", "-"):
        namen, laeufe, kennung = [], [], None
        for z in zeilen:
            if (z.get("Plan"), z.get("Lauf")) != kennung:
                kennung = (z.get("Plan"), z.get("Lauf"))
                namen.append(z["Plan"])
                laeufe.append([])
            # ungueltige Wellen (B20: Fabrik-Takt unter 0,95) nicht kalibrieren
            if z.get("FabrikTakt_Anteil") in (None, "") or float(z["FabrikTakt_Anteil"].replace(",", ".")) >= 0.95:
                laeufe[-1].append(z)
    else:
        namen = json.load(open(os.path.join(TESTS, "Pakete", "Pruefstand.json"), encoding="utf-8"))["Plaene"]
        laeufe, vorher = [], 10 ** 6
        for z in zeilen:
            if int(z["Welle"]) <= vorher:
                laeufe.append([])
            laeufe[-1].append(z)
            vorher = int(z["Welle"])
    wellen = sim.Wellen(B, katalog, phasen, A["Gegner_GrundLP"], "Normal")
    aus = [f"# Kalibrierung Simulator gegen Prüfstand", "", f"Messung: `{os.path.basename(datei)}`; Balance: {quelle}", ""]
    alle_lp, alle_k, alle_q = [], [], []
    for name, lauf in zip(namen, laeufe):
        plan = json.load(open(os.path.join(TESTS, "Plaene", name + ".json"), encoding="utf-8"))
        basis = basis_je_welle(plan)
        aus += [f"## {name}", "", "| Welle | LP Spiel | LP Sim | K Spiel | K Sim | Q Spiel | Q Sim |", "|---|---|---|---|---|---|---|"]
        for z in lauf:
            n = int(z["Welle"])
            tuerme = basis.get(n, [])
            k_sim = sum(nenn_dps(B, A, t, l, r) for t, l, r in tuerme if r in MUNITION)
            lp_l, q_l = [], []
            for seed in range(60):
                gegner, _ = wellen.erzeuge(n, random.Random(seed * 7919 + n))
                lp = sum(l for _, l in gegner)
                ehp = sum(l / max(sum(nenn_dps(B, A, t, lv, r) * sim.resistenz(B, ART[t], g) for t, lv, r in tuerme if r in MUNITION) / max(k_sim, 1e-6), 0.05)
                          for g, l in gegner)
                lp_l.append(lp)
                q_l.append(ehp / (k_sim * A["Q"]["Referenzzeit"]) if k_sim else 0)
            lp_sim, q_sim = statistics.mean(lp_l), statistics.mean(q_l)
            lp_m, k_m, q_m = float(z["LP"]), float(z["K_DPS"]), float(z["Q"])
            aus.append(f"| {n} | {lp_m:.0f} | {lp_sim:.0f} | {k_m:.0f} | {k_sim:.0f} | {q_m:.2f} | {q_sim:.2f} |")
            if lp_sim > 0 and k_sim > 0 and q_sim > 0:
                alle_lp.append(lp_m / lp_sim)
                alle_k.append(k_m / k_sim)
                alle_q.append(q_m / q_sim)
        aus.append("")
    if alle_q:
        aus += ["## Korrekturfaktoren (Median Spiel ÷ Simulator)", "",
                f"- Gegner-LP: **{statistics.median(alle_lp):.2f}** (Streuung {min(alle_lp):.2f}–{max(alle_lp):.2f})",
                f"- Abwehr-DPS K: **{statistics.median(alle_k):.2f}** ({min(alle_k):.2f}–{max(alle_k):.2f})",
                f"- Q: **{statistics.median(alle_q):.2f}** ({min(alle_q):.2f}–{max(alle_q):.2f})", "",
                "Faktoren nahe 1 heißen: das Modell trifft das Spiel. Abweichungen beim LP liegen am Wellenmodell oder an `Gegner_GrundLP`, "
                "beim K an den Turmformeln (Level, Munition, Doktrin)."]
    os.makedirs(os.path.join(HIER, "Ausgabe"), exist_ok=True)
    ziel = os.path.join(HIER, "Ausgabe", "Kalibrierung.md")
    open(ziel, "w", encoding="utf-8").write("\n".join(aus) + "\n")
    print("\n".join(aus[-6:]))
    print("Ausgabe:", ziel)


if __name__ == "__main__":
    main()
