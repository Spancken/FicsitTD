"""Auswertung eines Balancing-Laufs: Metriken.csv, Wirtschaft.csv und Notizen.csv aus Saved/FicsitTD zu einer Seite.

Aufruf:  python auswertung_lauf.py [--ab "2026-11-04 18:00"] [--aus ORDNER]
Ausgabe: Ausgabe/Lauf.html (Kurven, Tabelle je Welle mit Noten und Eingriffen), dazu die Simulation (Normal) als Vergleich
"""
import argparse
import csv
import datetime
import json
import os

import simulator as sim

SAVED = os.path.join(os.environ.get("LOCALAPPDATA", ""), "FactoryGame", "Saved", "FicsitTD")


def lies(name, ab):
    p = os.path.join(SAVED, name)
    if not os.path.exists(p):
        return []
    with open(p, encoding="utf-8") as f:
        zeilen = list(csv.DictReader(f, delimiter=";"))
    return [z for z in zeilen if not ab or z.get("Zeit", "") >= ab]


def zahl(x, std=0.0):
    try:
        return float(x)
    except (TypeError, ValueError):
        return std


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ab", help="nur Zeilen ab diesem Zeitpunkt (JJJJ-MM-TT HH:MM), Standard: letzter Lauf ab Welle 1")
    ap.add_argument("--aus", default=os.path.join(sim.HIER, "Ausgabe"))
    a = ap.parse_args()

    wirtschaft = lies("Wirtschaft.csv", a.ab)
    if not a.ab and wirtschaft:
        # letzter Lauf: ab der letzten Zeile mit Welle 1
        starts = [i for i, z in enumerate(wirtschaft) if z["Welle"] == "1"]
        ab = wirtschaft[starts[-1]]["Zeit"] if starts else wirtschaft[0]["Zeit"]
    else:
        ab = a.ab or ""
    wirtschaft = [z for z in wirtschaft if z["Zeit"] >= ab]
    metriken = {int(z["Welle"]): z for z in lies("Metriken.csv", ab)}
    notizen = {}
    for z in lies("Notizen.csv", ab):
        notizen.setdefault(int(z["Welle"]), []).append(z)
    if not wirtschaft:
        raise SystemExit("Keine Zeilen in Wirtschaft.csv - wurde mit 0.55 gespielt?")

    wellen = [int(z["Welle"]) for z in wirtschaft]
    q = [(w, zahl(metriken[w]["Q"])) for w in wellen if w in metriken]
    verlust = [(w, zahl(metriken[w]["Kernverlust_Proz"]) + 0.01) for w in wellen if w in metriken]

    # Simulation (Normal) zum Vergleich
    B, quelle = sim.lade_balance(os.path.join(SAVED, "Balance.json"))
    A = json.load(open(os.path.join(sim.HIER, "annahmen.json"), encoding="utf-8"))
    A["Tuerme"] = {k: v for k, v in A["Tuerme"].items() if not k.startswith("_")}
    A["Munition"] = {k: v for k, v in A["Munition"].items() if not k.startswith("_")}
    katalog, phasen = sim.lade_katalog(B)
    ab_welle = sim.lade_forschung_ab_welle(B)
    A["Simulation"]["Durchlaeufe"] = 100
    simz = sim.zusammenfassen(sim.simuliere(B, A, katalog, phasen, ab_welle, "Normal", 100, A["Simulation"]["Seed"]))
    sim_q = [(z["Welle"], z["Q"]) for z in simz if z["Welle"] <= max(wellen)]

    q_svg = sim.svg_linien([("Spiel", "#B23A48", q, False), ("Simulation Normal", "#3A6EA5", sim_q, True)],
                           "Druckquote Q je Welle", "Q")
    ep_svg = sim.svg_linien([
        ("EP verdient", "#2f7d4f", [(int(z["Welle"]), zahl(z["EP_verdient"])) for z in wirtschaft], False),
        ("EP ausgegeben", "#D08A2E", [(int(z["Welle"]), max(zahl(z["EP_ausgegeben"]), 0.01)) for z in wirtschaft], False),
        ("EP Konto", "#6b6b70", [(int(z["Welle"]), max(zahl(z["EP_Konto"]), 0.01)) for z in wirtschaft], True),
    ], "Einsatzpunkte je Welle", "EP")
    strom_svg = sim.svg_linien([
        ("Kapazität", "#3A6EA5", [(int(z["Welle"]), max(zahl(z["Strom_Kapazitaet_MW"]), 0.01)) for z in wirtschaft], False),
        ("Spitze", "#B23A48", [(int(z["Welle"]), max(zahl(z["Strom_Spitze_MW"]), 0.01)) for z in wirtschaft], False),
    ], "Strom am Wellenende", "MW")
    bau_svg = sim.svg_linien([
        ("Bauphase", "#4C9A6A", [(int(z["Welle"]), max(zahl(z["Bauphase_s"]) / 60, 0.01)) for z in wirtschaft if zahl(z["Bauphase_s"], -1) >= 0], False),
    ], "Länge der Bauphase vor der Welle", "min")

    noten_text = {1: "zu leicht", 2: "eher leicht", 3: "passend", 4: "eher schwer", 5: "zu schwer"}
    zeilen = []
    for z in wirtschaft:
        w = int(z["Welle"])
        m = metriken.get(w, {})
        n = notizen.get(w, [])
        note = ", ".join(f'{x["Note"]} {x["Text"]}' for x in n)
        eingriff = z.get("Eingriffe", "")
        zeilen.append(
            f'<tr class="{"eingriff" if eingriff else ""}"><td>{w}</td><td>{zahl(z["Bauphase_s"]) / 60:.1f}</td>'
            f'<td>{m.get("Gegner", "")}</td><td>{zahl(m.get("Q")):.2f}</td><td>{zahl(m.get("Kernverlust_Proz")):.1f}</td>'
            f'<td>{zahl(m.get("Naechste_m")):.0f}</td><td>{zahl(m.get("Hungrig_s")):.0f}</td><td>{zahl(m.get("OhneStrom_s")):.0f}</td>'
            f'<td>{z["EP_verdient"]}</td><td>{z["EP_ausgegeben"]}</td><td>{z["EP_Konto"]}</td><td class="l">{z["Kaeufe_EP"]}</td>'
            f'<td>{z["FP_verdient"]}/{z["FP_ausgegeben"]}</td><td class="l">{z["Erforscht"]}</td><td class="l">{z["Beute"]}</td>'
            f'<td class="l">{z["Munition_Schuss_Vorrat"]}</td><td>{zahl(z["Strom_Spitze_MW"]):.0f}/{zahl(z["Strom_Kapazitaet_MW"]):.0f}</td>'
            f'<td class="l">{note}</td><td class="l">{eingriff}</td></tr>')
    alle_noten = [int(x["Note"]) for v in notizen.values() for x in v]
    schnitt = f"{sum(alle_noten) / len(alle_noten):.1f}" if alle_noten else "–"
    html = f"""<!doctype html><html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>FicsitTD Balancing-Lauf</title><style>
:root {{ --bg:#fbfaf7; --fg:#1d1d1f; --mut:#6b6b70; --rand:#e2e0da; --warn:#fbe9d8; }}
@media (prefers-color-scheme: dark) {{ :root {{ --bg:#18181a; --fg:#ececee; --mut:#a0a0a8; --rand:#34343a; --warn:#3a2a1a; }} }}
body {{ background:var(--bg); color:var(--fg); font:14px/1.5 system-ui,Segoe UI,sans-serif; margin:0 auto; max-width:1200px; padding:24px 16px; }}
h1 {{ font-size:22px; margin:0 0 4px; }} h2 {{ font-size:17px; margin:24px 0 8px; }} .m {{ color:var(--mut); font-size:13px; }}
svg {{ width:100%; height:auto; display:block; }} svg .g {{ stroke:var(--rand); }} svg .a {{ fill:var(--mut); font-size:11px; }} svg .t {{ fill:var(--fg); font-size:13px; font-weight:600; }}
.zwei {{ display:grid; grid-template-columns:1fr 1fr; gap:12px; }} @media (max-width:800px) {{ .zwei {{ grid-template-columns:1fr; }} }}
table {{ border-collapse:collapse; width:100%; font-size:12px; }} th,td {{ border-bottom:1px solid var(--rand); padding:3px 5px; text-align:right; vertical-align:top; }}
td.l, th.l {{ text-align:left; }} tr.eingriff td {{ background:var(--warn); }} .scroll {{ overflow-x:auto; }}
</style></head><body>
<h1>FicsitTD Balancing-Lauf</h1>
<p class="m">Ab {ab} · {len(wirtschaft)} Wellen (bis Welle {max(wellen)}) · Noten im Mittel {schnitt} (1 zu leicht, 3 passend, 5 zu schwer) · Simulation: {quelle} · erzeugt {datetime.datetime.now():%d.%m.%Y %H:%M}</p>
{q_svg}
<div class="zwei">{ep_svg}{strom_svg}</div>
{bau_svg}
<h2>Je Welle</h2>
<p class="m">Hellorange: Welle mit Eingriff (/gib, /td fuellen, Testsprung …), für das Balancing nur bedingt aussagekräftig. Munition: Turmtyp Schüsse/Vorrat am Wellenende. Strom: Spitze/Kapazität in MW.</p>
<div class="scroll"><table><thead><tr><th>Welle</th><th>Bau min</th><th>Gegner</th><th>Q</th><th>Kern −%</th><th>Nächste m</th><th>ohne Mun. s</th><th>ohne Strom s</th>
<th>EP +</th><th>EP −</th><th>Konto</th><th class="l">Käufe (EP)</th><th>FP +/−</th><th class="l">erforscht</th><th class="l">Beute</th><th class="l">Munition</th><th>Strom</th><th class="l">Notiz</th><th class="l">Eingriffe</th></tr></thead>
<tbody>{"".join(zeilen)}</tbody></table></div>
</body></html>"""
    os.makedirs(a.aus, exist_ok=True)
    ziel = os.path.join(a.aus, "Lauf.html")
    open(ziel, "w", encoding="utf-8").write(html)
    print(f"{len(wirtschaft)} Wellen ab {ab}, Noten {len(alle_noten)}, Ausgabe: {ziel}")


if __name__ == "__main__":
    main()
