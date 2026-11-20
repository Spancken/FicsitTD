"""0.84 (08.11.2026): Ausgangslage des Strategie-Prüfstands bis W50 mit Zielbild, Fall-Wellen und Ursachen je Linie.
    python basis_lauf.py [--laeufe 3] [--balance '{"Schluessel": Wert}']"""
import argparse, collections, json, statistics
import strategien as st

ap = argparse.ArgumentParser()
ap.add_argument("--laeufe", type=int, default=3)
ap.add_argument("--wellen", type=int, default=50)
ap.add_argument("--balance", default="{}")
ap.add_argument("--regler", default="{}")
a = ap.parse_args()
bal, reg = json.loads(a.balance), json.loads(a.regler)
Z0 = st.lade_alles(bal)[2]
erg, _ = st.massenlauf("Normal", a.wellen, a.laeufe, 7, balance_ueber=bal, z_ueber=reg)
zb = st.zielbild(erg, {**Z0, **reg})
krit = ["Ziel_Wege", "Ziel_NichtAlles", "Ziel_NichtZuLeicht", "Ziel_KeineDominanz", "Ziel_KeineToteLinie", "Ziel_W1", "Ziel_W2", "Ziel_W3", "Ziel_W4", "Ziel_W5", "Ziel_W6"]
print("Ziele:", sum(bool(zb.get(k)) for k in krit), "/", len(krit), {k[5:]: bool(zb.get(k)) for k in krit})
print("Erfolg gesamt", round(zb["Quote"] * 100), "%  je Linie:", {l: round(q * 100) for l, q in sorted(zb["JeLinie"].items())})
fall = collections.defaultdict(list); urs = collections.defaultdict(collections.Counter)
for e in erg:
    if not e["Erfolg"]:
        fall[e["Linie"]].append(e["Welle"]); urs[e["Linie"]][e["Ursache"]] += 1
for l in sorted(fall):
    print(f"  {l:16} fällt median W{statistics.median(fall[l]):.0f}  Ursachen {dict(urs[l])}")
print({k: v for k, v in zb.items() if not k.startswith("Ziel_") and k not in ("JeLinie",) and not isinstance(v, (list, dict))})
