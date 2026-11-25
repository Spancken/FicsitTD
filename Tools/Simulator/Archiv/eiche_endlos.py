# 23.11.2026: Endlos.<Stufe>.LPFaktor eichen, damit der haertere Einsatz (Stufen.*.GegnerLP, Wellen.LPJeWelle) den
# Endlos-Modus nicht unter seine Baender drueckt. Ziel je Stufe: Median der erreichten Welle (ohne Vorziehen, 3 Laeufe je Linie).
# Rechnet nur im Speicher. Aufruf im Simulator-Ordner: python eiche_endlos.py [Normal Hart Albtraum Erkundung]
import statistics, sys
import strategien as st, sim_modi as sm

ZIEL = {"Erkundung": 45, "Normal": 30, "Hart": 20, "Albtraum": 14}


def median_welle(daten, stufe, faktor):
    B = dict(daten[0])
    B[f"Endlos.{stufe}.LPFaktor"] = faktor
    erg = sm.endlos_lauf((B,) + tuple(daten[1:]), stufe, max_wellen=150, laeufe=3)
    return statistics.median(r["Welle"] for r in erg if not r["Vorziehen"])


def main():
    stufen = sys.argv[1:] or ["Normal", "Hart", "Albtraum"]
    B, A, Z, katalog, phasen, K, gabeln, _ = st.lade_alles(stand="091")
    daten = (B, A, Z, katalog, phasen, K, gabeln)
    for stufe in stufen:
        lo, hi = 0.3, 1.5
        print(f"{stufe}: Faktor 1.0 -> W{median_welle(daten, stufe, 1.0)} (Ziel W{ZIEL[stufe]})", flush=True)
        for _ in range(7):
            mid = (lo + hi) / 2
            w = median_welle(daten, stufe, mid)
            print(f"  Faktor {mid:.3f}: W{w}", flush=True)
            if abs(w - ZIEL[stufe]) <= 1.5:
                lo = hi = mid
                break
            if w > ZIEL[stufe]:
                lo = mid
            else:
                hi = mid
        print(f"=> Endlos.{stufe}.LPFaktor etwa {(lo + hi) / 2:.2f}", flush=True)


if __name__ == "__main__":
    main()
