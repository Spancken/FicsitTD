"""FicsitTD Pruefstand (Roadmap M1): Kennzahlen je Welle aus Metriken.csv gegen die Zielbaender pruefen.

Aufruf (Teste-Mod.ps1 nach jedem Lauf):  python werte_pruefstand.py <Startzeit "YYYY-MM-DD HH:MM:SS"> <Bericht.md>
Haengt an den Bericht einen Abschnitt "Pruefstand" an (Tabelle je Welle, Bandverletzungen) und legt die Zeilen als CSV daneben.
Die Baender gelten vorerst nur als Hinweis, nicht als Fehler, bis sie mit Marius' Spielgefuehl abgeglichen sind.
"""
import csv
import json
import os
import statistics
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
METRIKEN = os.path.join(os.environ.get("LOCALAPPDATA", ""), "FactoryGame", "Saved", "FicsitTD", "Metriken.csv")
STUFEN = {0: "Erkundung", 1: "Normal", 2: "Hart", 3: "Albtraum"}


def zahl(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return 0.0


def spielt_wellen(name):
    """Plaene ohne Welle (z. B. Neustart) liefern keine Metrik-Zeilen und duerfen die Zuordnung nicht verschieben"""
    try:
        plan = json.load(open(os.path.join(HIER, "Plaene", name + ".json"), encoding="utf-8"))
    except (OSError, ValueError):
        return True
    schritte = plan.get("Schritte") or plan.get("schritte") or []
    return any("wellenlauf" in s or "kern weiter" in str(s.get("befehl", "")) for s in schritte if isinstance(s, dict))


def main():
    if len(sys.argv) < 3 or not os.path.exists(METRIKEN):
        return
    start, bericht = sys.argv[1], sys.argv[2]
    baender = json.load(open(os.path.join(HIER, "zielbaender.json"), encoding="utf-8"))
    zeilen = [z for z in csv.DictReader(open(METRIKEN, encoding="utf-8"), delimiter=";") if z["Zeit"] >= start]
    if not zeilen:
        return
    namen = sys.argv[3].split(",") if len(sys.argv) > 3 else []
    namen = [n for n in namen if spielt_wellen(n)]   # 07.11.: Neustart verschob alle spaeteren Tabellen um einen Plan
    kopf = ["| Welle | Stufe | Gegner | Türme | Q | Kern % | Verlust % | Munitionsnot | ohne Strom s | Dauer s | Bild p95 | Fabrik-Takt | Pause s | Hinweis |",
            "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    aus = ["", "## Prüfstand: Kennzahlen je Welle", "",
           "Munitionsnot = Turm-Sekunden mit Ziel ohne Munition ÷ (Türme × Wellendauer). Bild p95 in ms (22 ms ≈ 45 FPS).",
           "Fabrik-Takt = Anteil der Spielzeit, die der Fabrik-Takt der Türme bekam (B20). Unter 0,95 ist die Welle **ungültig** (Zeitraffer zu hoch für die Bildrate) und zählt nicht für Kalibrierung und Bänder."]
    verletzungen = 0
    lauf, vorher = 0, 10 ** 6
    laeufe_kurz = []
    for z in zeilen:
        neu_lauf = z.get("Lauf") not in (None, "") and len(laeufe_kurz) > 0 and z.get("Lauf") != laeufe_kurz[-1][3]
        if int(zahl(z["Welle"])) <= vorher or neu_lauf:   # Wellennummer beginnt neu (oder neue Laufnummer): naechster Plan
            lauf += 1
            aus += ["", f"### {namen[lauf - 1] if lauf <= len(namen) else f'Lauf {lauf}'}", ""] + kopf
        vorher = int(zahl(z["Welle"]))
        name = z.get("Plan") if z.get("Plan") not in (None, "", "-") else (namen[lauf - 1] if lauf <= len(namen) else f"Lauf {lauf}")
        if len(laeufe_kurz) < lauf:
            laeufe_kurz.append([name, 100.0, 0, z.get("Lauf")])
        # 17.11. (P12/B20): Gueltigkeit je Welle - zu wenig Fabrik-Takt verfaelscht Nachladen und Fabrik
        takt = zahl(z["FabrikTakt_Anteil"]) if z.get("FabrikTakt_Anteil") not in (None, "") else None
        gueltig = takt is None or takt >= 0.95
        laeufe_kurz[-1][1] = min(laeufe_kurz[-1][1], zahl(z["Kern_Proz"]))
        laeufe_kurz[-1][2] = max(laeufe_kurz[-1][2], int(zahl(z["Welle"])))
        stufe = STUFEN.get(int(zahl(z["Stufe"])), z["Stufe"])
        b = baender.get(stufe, {})
        welle = int(zahl(z["Welle"]))
        q, kern, dauer, tuerme = zahl(z["Q"]), zahl(z["Kern_Proz"]), max(zahl(z["Dauer_s"]), 1.0), max(int(zahl(z["Tuerme"])), 1)
        not_ = zahl(z["Hungrig_s"]) / (tuerme * dauer)
        hinweise = []
        phase = b.get(f"Phase{(welle - 1) // 10 + 1}", {})
        qband = phase.get("Q_Boss" if welle % 10 == 0 else "Q", b.get("Q_Boss" if welle % 10 == 0 else "Q"))
        if qband and not (qband[0] <= q <= qband[1]):
            hinweise.append("Q unter Band" if q < qband[0] else "Q über Band")
        if "Kern_Proz_min" in b and kern < b["Kern_Proz_min"]:
            hinweise.append("Kern zu tief")
        if "Hungrig_Anteil_max" in b and not_ > b["Hungrig_Anteil_max"]:
            hinweise.append("Munitionsnot")
        if "Bild_p95_ms_max" in b and zahl(z["Bild_p95_ms"]) > b["Bild_p95_ms_max"]:
            hinweise.append("Bildrate")
        if not gueltig:
            hinweise = ["**ungültig** (Fabrik-Takt %.2f)" % takt]
        else:
            verletzungen += len(hinweise)
        pause = zahl(z["Pause_s"]) if z.get("Pause_s") not in (None, "") else None
        takt_text = "-" if takt is None else f"{takt:.2f}"
        pause_text = "-" if pause is None else f"{pause:.0f}"
        aus.append(f"| {welle} | {stufe} | {z['Gegner']} | {z['Tuerme']} | {q:.2f} | {kern:.0f} | {zahl(z['Kernverlust_Proz']):.1f} | {100 * not_:.0f} % | "
                   f"{zahl(z['OhneStrom_s']):.0f} | {dauer:.0f} | {zahl(z['Bild_p95_ms']):.1f} | {takt_text} | {pause_text} | {', '.join(hinweise) or 'ok'} |")
    # 0.78.2 Streuung: je Plan Laeufe, W30 erreicht, Kern-Tiefpunkt (Median, Spanne). Ziel: Spanne <= 25 Punkte
    je_plan = {}
    for name, tief, hoechste, _lauf in laeufe_kurz:
        je_plan.setdefault(name, []).append((tief, hoechste))
    if any(len(v) > 1 for v in je_plan.values()):
        aus += ["", "### Streuung je Plan", "", "| Plan | Läufe | höchste Welle je Lauf | Kern-Tiefpunkt Median | Spanne | Ziel Spanne ≤ 25 |", "|---|---|---|---|---|---|"]
        for name, v in je_plan.items():
            tiefs = sorted(t for t, _ in v)
            spanne = tiefs[-1] - tiefs[0]
            aus.append(f"| {name} | {len(v)} | {', '.join(str(h) for _, h in v)} | {statistics.median(tiefs):.0f} % | {spanne:.0f} | {'ja' if spanne <= 25 else '**nein**'} |")
    aus += ["", f"**{verletzungen} Bandverletzungen** in {len(zeilen)} Wellen (vorerst nur Hinweis, Bänder in `Tests\\zielbaender.json`)."]
    with open(bericht, "a", encoding="utf-8") as f:
        f.write("\n".join(aus) + "\n")
    with open(os.path.splitext(bericht)[0] + "_Metriken.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(zeilen[0].keys()), delimiter=";")
        w.writeheader()
        w.writerows(zeilen)
    print(f"Pruefstand: {len(zeilen)} Wellen, {verletzungen} Bandverletzungen")


if __name__ == "__main__":
    main()
