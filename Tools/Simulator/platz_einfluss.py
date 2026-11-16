# 0.83.1: Einfluss des Kernplatzes auf den Kernverlust (der Selbsttest waehlt bei jedem neuen Spiel einen der drei
# Felsenwuesten-Plaetze zufaellig). Liest alle Spiel-Logs, ordnet jede Welle Plan, Version und Kernplatz zu und vergleicht
# den gemessenen Verlust mit der Verlustkurve 47.61 * Q^1.37 (ohne Mehrziel-Faktoren, nur fuer den Vergleich der Plaetze).
#   python platz_einfluss.py
import collections, glob, os, re

LOGS = os.path.expandvars(r"%LOCALAPPDATA%\FactoryGame\Saved\Logs")
A, P = 47.61, 1.37
r_ver = re.compile(r"FicsitTD: (\d+\.\d+\.\d+)$")
r_plan = re.compile(r"Autotest: Plan \d+/\d+ (\S+) mit")
r_platz = re.compile(r"Baue den Kernplatz (\S+) ")
r_welle = re.compile(r"Metrik Welle (\d+): Q ([\d.]+), Dauer \d+ s, Kernverlust ([\d.]+) %")

zeilen = []   # (Version, Plan, Platz, Welle, Q, Verlust)
for datei in sorted(glob.glob(os.path.join(LOGS, "FactoryGame*.log"))):
    ver = plan = platz = None
    for z in open(datei, encoding="utf-8", errors="ignore"):
        if (m := r_ver.search(z)): ver = m.group(1)
        elif (m := r_plan.search(z)): plan, platz = m.group(1), "T2-Spielstand"
        elif (m := r_platz.search(z)): platz = m.group(1)
        elif plan and (m := r_welle.search(z)):
            zeilen.append((ver, plan, platz, int(m.group(1)), float(m.group(2)), float(m.group(3))))

def tabelle(schluessel, titel, filter_=lambda z: True):
    g = collections.defaultdict(lambda: [0, 0.0, 0.0])
    for z in zeilen:
        if filter_(z) and z[4] > 0:
            e = g[schluessel(z)]; e[0] += 1; e[1] += z[5]; e[2] += A * z[4] ** P
    print(f"\n{titel}\n{'Gruppe':34} Wellen  Verlust gemessen  Kurve   Faktor")
    for k in sorted(g):
        n, v, m = g[k]
        print(f"{str(k):34} {n:6}  {v / n:10.1f} %  {m / n:7.1f} %  {v / m:5.2f}")

nur_strat = lambda z: z[1].startswith("Strat-")
tabelle(lambda z: z[2], "Alle Strategie-Plaene nach Kernplatz", nur_strat)
tabelle(lambda z: (z[2], z[0]), "nach Kernplatz und Version", nur_strat)
tabelle(lambda z: (z[1].replace("Strat-Mono", "").replace("-Vor-W50", "/50"), z[2]), "nach Linie und Kernplatz", nur_strat)

# je Lauf (W50-Plaene): Summe Kernverlust W1-46 und mittleres Q, damit gleiche Plaene auf verschiedenen Plaetzen vergleichbar sind
print(f"\nW50-Laeufe einzeln (Summe Kernverlust W1-46)\n{'Plan':28} {'Platz':8} {'Version':8} Summe   Q-Mittel")
laeufe, letzt = [], None
for z in zeilen:
    if not z[1].endswith("-Vor-W50"):
        continue
    if letzt is None or z[1] != letzt[0][1] or z[3] <= letzt[-1][3]:
        letzt = []
        laeufe.append(letzt)
    letzt.append(z)
for ws in laeufe:
    w46 = [w for w in ws if w[3] <= 46]
    if len(w46) >= 40:
        print(f"{ws[0][1]:28} {ws[0][2]:8} {ws[0][0]:8} {sum(w[5] for w in w46):6.0f} %  {sum(w[4] for w in w46) / len(w46):.2f}")
