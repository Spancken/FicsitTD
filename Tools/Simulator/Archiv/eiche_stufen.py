# 23.11.2026 (Marius: "Es sollte schon schwerer sein, Normal und vor allem Hart und Albtraum"; Gegencheck 0.13):
# Gegnerleben je Stufe so eichen, dass der Prüfstand (guter Spieler, Spielerwahl) die Siegquote im Einsatz in die Mitte
# des Zielbands bringt (sim_baender.json). Rechnet nur im Speicher (massenlauf mit balance_ueber), schreibt nichts.
# Aufruf im Simulator-Ordner: python eiche_stufen.py [Normal Hart Albtraum] [Schluessel=Wert ...]
import random, sys, statistics
import strategien as st

ZIEL = {"Normal": 0.50, "Hart": 0.35, "Albtraum": 0.20}


ZUSATZ = {}   # weitere Balance-Werte fuer alle Stufen, z. B. Wellen.LPJeWelle=0.35 auf der Kommandozeile


def quote(stufe, lp, liste):
    ueber = dict(ZUSATZ)
    ueber[f"Stufen.{stufe}.GegnerLP"] = lp
    erg, _ = st.massenlauf(stufe, 50, 1, seed=7, balance_ueber=ueber, strategie_liste=liste, stand="091")
    erfolg = sum(r["Erfolg"] for r in erg) / len(erg)
    kern = statistics.median([r["KernMin"] for r in erg if r["Erfolg"]] or [0])
    return erfolg, kern


def main():
    stufen = [a for a in sys.argv[1:] if "=" not in a] or list(ZIEL)
    for a in sys.argv[1:]:
        if "=" in a:
            ZUSATZ[a.split("=")[0]] = float(a.split("=")[1])
    B = st.lade_alles(stand="091")[0]
    liste = st.strategien(random.Random(7))[::2]          # wie sim_alle_modi
    for stufe in stufen:
        lo, hi = B[f"Stufen.{stufe}.GegnerLP"] * 0.6, B[f"Stufen.{stufe}.GegnerLP"] * 2
        heute = B[f"Stufen.{stufe}.GegnerLP"]
        e_h, _ = quote(stufe, heute, liste)
        print(f"{stufe}: LP {heute:.2f} {ZUSATZ or ''} -> Siege {e_h:.0%} (Ziel {ZIEL[stufe]:.0%})", flush=True)
        # Bisektion auf dem LP-Faktor (Siegquote faellt mit mehr Leben)
        for _ in range(7):
            mid = (lo + hi) / 2
            e, k = quote(stufe, mid, liste)
            print(f"  LP {mid:.3f}: Siege {e:.0%}, Kern-Tief (Median der Siege) {k:.0f} %", flush=True)
            if abs(e - ZIEL[stufe]) <= 0.03:
                lo = hi = mid
                break
            if e > ZIEL[stufe]:
                lo = mid
            else:
                hi = mid
        print(f"=> {stufe}: GegnerLP etwa {(lo + hi) / 2:.2f}", flush=True)


if __name__ == "__main__":
    main()
