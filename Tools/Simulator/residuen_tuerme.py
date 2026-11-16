# 0.83.1: Restfehler der Verlustkurve nach Turmzahl (Frage: unterschaetzt das Modell Wellen mit sehr wenigen Tuermen?)
#   python residuen_tuerme.py
import collections
import strategien as st
import verlust_fit as vf

zeilen, B, Z = vf.lade_wellen(vf.STANDARD)
an = vf.Anpassung(zeilen, B.get("Wellen.MaxGleichzeitig", 60), Z.get("VerlustStreckung", 1.5))
f = {t: Z.get(f"Mehrziel_{t}", 1.0) for t in vf.FREI}
v = [Z.get("VerlustGrund", 0) + x for x in an.vorhersage(Z["VerlustQ"], Z.get("VerlustP", 1.0), f)]
gruppen = collections.defaultdict(list)
for z, p in zip(zeilen, v):
    n = z[7]
    g = "1" if n <= 1 else "2-3" if n <= 3 else "4-6" if n <= 6 else "7-12" if n <= 12 else "13+"
    gruppen[g].append((z[4], p, z[3]))
print("Tuerme | Wellen | Verlust gemessen (Mittel) | Modell (Mittel) | Q (Mittel)")
for g in ("1", "2-3", "4-6", "7-12", "13+"):
    w = gruppen.get(g, [])
    if w:
        print(f"{g:>6} | {len(w):6} | {sum(x[0] for x in w) / len(w):9.1f} % | {sum(x[1] for x in w) / len(w):9.1f} % | {sum(x[2] for x in w) / len(w):.2f}")
