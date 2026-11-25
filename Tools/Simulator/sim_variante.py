# Gegenversuch zur A6-Simulation: alle Modi mit veraenderten Freigabewellen einzelner Knoten (nur im Speicher, Code bleibt).
# Aufruf im Simulator-Ordner: python sim_variante.py <ausgabeordner> Knoten=Welle [Knoten=Welle ...]
# (22.11.2026 Analyse-A6-Simulation; 23.11. ins Repo, Gegencheck 0.13. Achtung: sim_alle_modi.main() schreibt sim_stand.json neu -
#  nach einem Gegenversuch mit `git checkout -- Tools/Simulator/sim_stand.json` zuruecksetzen.)
import os, sys
sys.path.insert(0, os.getcwd())
import strategien as st
import sim_alle_modi as sam

aus = sys.argv[1]
aenderung = dict((a.split("=")[0], int(a.split("=")[1])) for a in sys.argv[2:])
PREIS_WELLE, PREIS = [0, 5, 15, 25, 35], [3, 4, 6, 8, 10]
original = st.lade_alles


def lade_alles(*a, **k):
    B, A, Z, katalog, phasen, K, gabeln, quelle = original(*a, **k)
    for name, w in aenderung.items():
        K[name]["ab"] = w
    # Freigabe erbt von Voraussetzungen, Preis aus der Phase (wie create_research.eff_welle/preis)
    for _ in range(5):
        for name, kn in K.items():
            eff = max([kn["ab"]] + [K[v]["ab"] for v in kn["vor"] if v in K])
            kn["ab"] = eff
            kn["fp"] = PREIS[max(i for i, ab in enumerate(PREIS_WELLE) if eff >= ab)]
    return B, A, Z, katalog, phasen, K, gabeln, quelle


st.lade_alles = lade_alles
sam.pruefe_modell = lambda *a, **k: None
sys.argv = ["sim_alle_modi.py", "--aus", aus]
sam.main()
