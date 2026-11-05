"""FicsitTD: Pruefungen ohne Spiel (Konzept-Testautomatik, Stufe 1 / A1).

Laeuft vor jedem Build (Build-Mod.ps1) und bricht bei FEHLER ab. HINWEIS-Zeilen sind nur zur Info.
  python pruefe_mod.py            # Forschungsbaum, Balance-Schluessel, gespeicherte Balance.json
  python pruefe_mod.py --sim      # zusaetzlich Simulator: jede Turmlinie allein (dauert 1-2 min)
"""
import argparse
import json
import os
import re
import subprocess
import sys
from unittest import mock

MOD = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
QUELLE = os.path.join(MOD, "Source", "FicsitTD")
SKRIPTE = os.path.join(MOD, "Scripts")
SIMULATOR = r"D:\OneDrive\Claude\Claude Modding\Satisfactory\Werkzeuge\FicsitTD-Simulator\simulator.py"
BALANCE_JSON = os.path.join(os.environ.get("LOCALAPPDATA", ""), "FactoryGame", "Saved", "FicsitTD", "Balance.json")
AUSNAHMEN = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ausnahmen.json")

fehler, hinweise = [], []
def FEHLER(t): fehler.append(t)
def HINWEIS(t): hinweise.append(t)


# ------------------------------------------------------------------ 1. Forschungsbaum
def pruefe_forschung():
    sys.modules["unreal"] = mock.MagicMock()
    src = open(os.path.join(SKRIPTE, "create_research.py"), encoding="utf-8").read()
    ns = {}
    exec(src[:src.index("def punkt(")], ns)
    baeume = ns["BAEUME"]
    alle = {}
    for b_key, _, _, _, knoten in baeume:
        belegt = {kk[3]: kk[0] for kk in knoten}
        if len(belegt) != len(knoten):
            FEHLER(f"Forschung {b_key}: zwei Knoten auf demselben Platz")
        for k_key, k_name, k_text, (x, y), eltern, _, _, _, _ in knoten:
            if k_key in alle:
                FEHLER(f"Forschung: Schluessel {k_key} doppelt")
            alle[k_key] = b_key
            for name, tab in (("ORDNER", ns["ORDNER"]), ("KURZ", ns["KURZ"])):
                if k_key not in tab:
                    FEHLER(f"Forschung {b_key}/{k_key}: fehlt in {name}")
            for e in eltern:
                ek = next((kk for kk in knoten if kk[0] == e), None)
                if ek is None:
                    FEHLER(f"Forschung {b_key}/{k_key}: Elternknoten {e} nicht im selben Baum")
                    continue
                ex, ey = ek[3]
                if ey >= y:
                    FEHLER(f"Forschung {b_key}/{k_key}: steht nicht unter seinem Elternknoten {e}")
                # Linie wie im Skript: waagerecht in der Zeile des Elternknotens, dann senkrecht nach unten
                weg = [(xx, ey) for xx in range(min(ex, x) + 1, max(ex, x))] + ([(x, ey)] if x != ex else [])
                weg += [(x, yy) for yy in range(ey + 1, y)]
                for punkt in weg:
                    if punkt in belegt and belegt[punkt] not in (k_key, e):
                        FEHLER(f"Forschung {b_key}: Linie {e} -> {k_key} laeuft durch {belegt[punkt]} {punkt}")
            try:
                ns["preis"](b_key, k_key)
            except Exception as ex_:
                FEHLER(f"Forschung {b_key}/{k_key}: kein Preis ({ex_})")
    for g_b, glieder in ns["GRUPPEN"]:
        for g in glieder:
            if alle.get(g) != g_b:
                FEHLER(f"Forschung: Gabelung {glieder} - {g} steht nicht im Baum {g_b}")
    for (b, k), vor in ns["VORAUSSETZUNGEN"].items():
        for vb, vk in [(b, k)] + vor:
            if alle.get(vk) != vb:
                FEHLER(f"Forschung: Voraussetzung ({vb}, {vk}) gibt es nicht")
    for (b, k) in ns["AB_WELLE"]:
        if alle.get(k) != b:
            FEHLER(f"Forschung: AB_WELLE ({b}, {k}) gibt es nicht")
    breit = {b_key: 1 + max(kk[3][0] for kk in knoten) for b_key, _, _, _, knoten in baeume}
    HINWEIS(f"Forschung: {len(alle)} Knoten in {len(baeume)} Baeumen, Spalten je Baum {breit}")
    return ns


# ------------------------------------------------------------------ 2. Balance-Schluessel im Code
def standard_tabelle():
    text = open(os.path.join(QUELLE, "Private", "TDBalance.cpp"), encoding="utf-8-sig").read()
    return {k: float(v) for k, v in re.findall(r'\{\s*TEXT\("([^"]+)"\),\s*([-0-9.]+)f?\s*\}', text)}


def pruefe_balance_schluessel(standard, forschung_ns):
    benutzt = {}
    for wurzel, _, dateien in os.walk(QUELLE):
        for d in dateien:
            if not d.endswith((".cpp", ".h")):
                continue
            text = open(os.path.join(wurzel, d), encoding="utf-8-sig", errors="replace").read()
            for k in re.findall(r'TDBalance::(?:Wert|Ganz)\(\s*TEXT\("([^"%]+)"\)', text):
                benutzt.setdefault(k, d)
    for k, d in sorted(benutzt.items()):
        if k not in standard:
            FEHLER(f"Balance: Schluessel {k} (in {d}) fehlt in der Standardtabelle")
    # Freigabe-Wellen: TDForschung fragt Forschung.<Knoten>.AbWelle fuer jeden Knoten mit Welle > 0
    for b_key, _, _, _, knoten in forschung_ns["BAEUME"]:
        for kk in knoten:
            if forschung_ns["AB_WELLE"].get((b_key, kk[0]), 0) > 0 and f"Forschung.{kk[0]}.AbWelle" not in standard:
                FEHLER(f"Balance: Forschung.{kk[0]}.AbWelle fehlt in der Standardtabelle")
            w = forschung_ns["AB_WELLE"].get((b_key, kk[0]))
            s = standard.get(f"Forschung.{kk[0]}.AbWelle")
            if w is not None and s is not None and int(s) != int(w):
                FEHLER(f"Balance: Forschung.{kk[0]}.AbWelle {s:g} weicht vom Forschungsskript ({w}) ab")
    HINWEIS(f"Balance: {len(benutzt)} feste Schluessel im Code, {len(standard)} in der Standardtabelle")


# ------------------------------------------------------------------ 3. gespeicherte Balance.json
def pruefe_balance_json(standard):
    if not os.path.exists(BALANCE_JSON):
        HINWEIS("Balance.json: keine gespeicherte Datei")
        return
    werte = {}
    def flach(o, vorne=""):
        for k, v in o.items():
            if k.startswith("_"):
                continue
            n = f"{vorne}.{k}" if vorne else k
            if isinstance(v, dict):
                flach(v, n)
            elif isinstance(v, (int, float)):
                werte[n] = float(v)
    flach(json.load(open(BALANCE_JSON, encoding="utf-8-sig")))
    abweichend = [f"{k}={v:g} (Standard {standard[k]:g})" for k, v in sorted(werte.items()) if k in standard and abs(standard[k] - v) > 1e-4]
    unbekannt = [k for k in werte if k not in standard]
    if abweichend:
        HINWEIS(f"Balance.json ueberschreibt {len(abweichend)} Standardwerte: " + ", ".join(abweichend[:12]) + (" ..." if len(abweichend) > 12 else ""))
    if unbekannt:
        HINWEIS(f"Balance.json: {len(unbekannt)} unbekannte Schluessel (wirkungslos): " + ", ".join(sorted(unbekannt)[:8]) + (" ..." if len(unbekannt) > 8 else ""))


# ------------------------------------------------------------------ 4. Simulator: jede Linie allein
def pruefe_simulator():
    grenze = {"Mischaufbau": 1.1}
    ausnahmen = json.load(open(AUSNAHMEN, encoding="utf-8")).get("Mono", {}) if os.path.exists(AUSNAHMEN) else {}
    lauf = subprocess.run([sys.executable, SIMULATOR, "--stufe", "Normal", "--mono"], capture_output=True, text=True, timeout=900)
    if lauf.returncode != 0:
        FEHLER(f"Simulator: Abbruch ({lauf.stderr.strip()[-300:]})")
        return
    for zeile in lauf.stdout.splitlines():
        m = re.match(r"\s*Normal\s+(\S+)\s+((?:W\d+ [0-9.]+\s*)+)", zeile)
        if not m:
            continue
        linie = m.group(1)
        qs = {int(w): float(q) for w, q in re.findall(r"W(\d+) ([0-9.]+)", m.group(2))}
        erlaubt = ausnahmen.get(linie, grenze.get(linie, 1.1))
        schlimmst = max(qs.values())
        text = f"Simulator {linie}: Q max {schlimmst:.2f} (erlaubt {erlaubt:g})"
        (FEHLER if schlimmst > erlaubt else HINWEIS)(text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sim", action="store_true")
    args = ap.parse_args()
    standard = standard_tabelle()
    ns = pruefe_forschung()
    pruefe_balance_schluessel(standard, ns)
    pruefe_balance_json(standard)
    if args.sim:
        pruefe_simulator()
    # Roadmap M0: Zahlentabellen in Spielregeln.md bei jedem Build aus dem Code erneuern
    try:
        import erzeuge_spielregeln
        erzeuge_spielregeln.main()
    except Exception as e:
        HINWEIS(f"Spielregeln.md: Zahlen nicht erzeugt ({e})")
    for h in hinweise:
        print("HINWEIS", h)
    for f in fehler:
        print("FEHLER ", f)
    print(f"Pruefung: {len(fehler)} Fehler, {len(hinweise)} Hinweise")
    sys.exit(1 if fehler else 0)


if __name__ == "__main__":
    main()
