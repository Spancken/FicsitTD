"""FicsitTD: Zahlentabellen in Spielregeln.md aus dem Code erzeugen (Roadmap M0, Bewertung: "Zahlen per Skript aus TDBalance.cpp").

Ersetzt in Satisfactory\\Spielregeln.md den Bereich zwischen <!-- ZAHLEN-ANFANG --> und <!-- ZAHLEN-ENDE -->.
Quelle sind die Standardwerte in TDBalance.cpp und die Forschungsdaten in Scripts\\create_research.py.
Laeuft automatisch in pruefe_mod.py (also vor jedem Build).
"""
import os
import re
import sys
from unittest import mock

MOD = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOK = r"D:\OneDrive\Claude\Claude Modding\Satisfactory\Spielregeln.md"
ANFANG, ENDE = "<!-- ZAHLEN-ANFANG -->", "<!-- ZAHLEN-ENDE -->"
# Gestrichen (0.9.3 bzw. frueher): Balance-Schluessel bleiben fuer alte Spielstaende, in den Tabellen nicht zeigen
GESTRICHEN = {"Reparaturstation", "Dimensionslager", "Panzersperre"}


def zahl(v):
    return f"{v:g}".replace(".", ",")


def lade():
    text = open(os.path.join(MOD, "Source", "FicsitTD", "Private", "TDBalance.cpp"), encoding="utf-8-sig").read()
    B = {k: float(v) for k, v in re.findall(r'\{\s*TEXT\("([^"]+)"\),\s*([-0-9.]+)f?\s*\}', text)}
    sys.modules["unreal"] = mock.MagicMock()
    src = open(os.path.join(MOD, "Scripts", "create_research.py"), encoding="utf-8").read()
    ns = {}
    exec(src[:src.index("def punkt(")], ns)
    return B, ns


def tabellen(B, ns):
    z = []
    w = lambda k, d="–": zahl(B[k]) if k in B else d
    # Stufen
    z += ["### Schwierigkeitsstufen", "", "| Wert | Erkundung | Normal | Hart | Albtraum |", "|---|---|---|---|---|"]
    for wert, name in [("Budget", "Gegnerbudget ×"), ("GegnerLP", "Gegner-LP ×"), ("KernLP", "Kern-LP"), ("AufbauSekunden", "Aufbau bis Welle 1 (s)"),
                       ("PauseSekunden", "Aufbau zwischen Wellen (s)"), ("FrontenStart", "Fronten zu Beginn"), ("NeueFrontAbWelle", "neue Front ab Welle"),
                       ("EliteAbWelle", "Elite ab Welle")]:
        z.append(f"| {name} | " + " | ".join(w(f"Stufen.{s}.{wert}") for s in ("Erkundung", "Normal", "Hart", "Albtraum")) + " |")
    # Wellen
    z += ["", "### Wellen", "",
          f"- Budget = Stufe × ({w('Wellen.BudgetGrund')} + {w('Wellen.BudgetJeWelle')} × N + {w('Wellen.BudgetQuadrat')} × N²)",
          f"- Gegner-LP × (1 + {w('Wellen.LPJeWelle')} × (N − 1)) bis Welle 30, danach + {w('Wellen.LPJeWelleAb31')} je Welle; Elite-LP × {w('Wellen.EliteLP')}",
          f"- Bosswellen: Gegnerbudget × {w('Wellen.BossBudget')} plus Boss",
          f"- höchstens {w('Wellen.MaxGleichzeitig')} gleichzeitig, {w('Wellen.MaxGesamt')} je Welle (Nachschub alle {w('Wellen.NachschubTakt')} s je {w('Wellen.NachschubPaket')})",
          f"- Standardlänge eines Einsatzes: {w('Einsatz.WellenStandard', '50')} Wellen", ""]
    z += ["| Gegnerart | ab Welle |", "|---|---|"]
    for k in sorted((k for k in B if k.startswith("Katalog.") and k.endswith(".AbWelle")), key=lambda k: B[k]):
        z.append(f"| {k.split('.')[1]} | {zahl(B[k])} |")
    z += ["", "| Phase | Bosse | Boss-LP × |", "|---|---|---|"]
    for p in range(1, 6):
        z.append(f"| {p} | {w(f'Phasen.{p}.BossAnzahl')} | {w(f'Phasen.{p}.BossLP')} |")
    # Belohnung
    z += ["", "### Belohnung je Welle", "",
          f"- **Coupons** = {w('Einsatz.EP.Grund')} + {w('Einsatz.EP.JeWelle')} × N, +{w('Einsatz.EP.KernVoll')} bei vollem Kern bzw. +{w('Einsatz.EP.KernAb75')} ab 75 %; Bosswelle × {w('Einsatz.EP.BossFaktor')} + {w('Einsatz.EP.BossBonus')}",
          f"- **FP** = {w('Einsatz.FP.JeWelle')}, +{w('Einsatz.FP.KernAb75')} bei Kern ab 75 %, +{w('Einsatz.FP.Boss')} im Boss",
          f"- Biomasse {w('Einsatz.Biomasse.JeWelle')}; Kern-Grundreparatur {zahl(100 * B.get('Kern.Grundreparatur', 0))} % je Welle",
          ""]
    z += ["| Beute | Normal | Elite | Boss |", "|---|---|---|---|"]
    for art in ("Kampfdaten", "EP", "Munition", "Splitter"):
        z.append(f"| {'Coupons' if art == 'EP' else art} (Chance) | " + " | ".join(w(f"Loot.{art}.{s}") for s in ("Normal", "Elite", "Boss")) + " |")
    # Laden
    z += ["", "### Laden (Kontingente)", "", "Preis = Grundpreis × (1 + Aufschlag × bisher gekauft); Paket = Gebäude je Kauf.", "",
          "| Eintrag | Grundpreis | Aufschlag | Paket | Start |", "|---|---|---|---|---|"]
    for k in sorted({k.split(".")[2] for k in B if k.startswith("Einsatz.Laden.")} - GESTRICHEN):
        z.append(f"| {k} | {w(f'Einsatz.Laden.{k}.Grundpreis')} | {w(f'Einsatz.Laden.{k}.Aufschlag')} | {w(f'Einsatz.Laden.{k}.Paket')} | {w(f'Einsatz.Start.{k}', '0')} |")
    # Tuerme
    z += ["", "### Türme", "", "| Turm | Reichweite (m) | LP |", "|---|---|---|"]
    for t in [t for t in ("Bolzenturm", "Gewehrturm", "Teslaspule", "Flammenwerfer", "Nobelisk-Werfer", "Reparaturstation") if t not in GESTRICHEN]:
        z.append(f"| {t} | {w(f'Tuerme.{t}.Reichweite')} | {w(f'Tuerme.{t}.LP')} |")
    z += ["", f"Turmlevel kosten {w('Tuerme.Level.Preis2')} / {w('Tuerme.Level.Preis3')} / {w('Tuerme.Level.Preis4')} / {w('Tuerme.Level.Preis5')} Coupons (Level 2–5). "
          f"Doktrin: Schaden +{zahl(100 * B.get('Doktrin.Schaden', 0))} %, Level −{zahl(100 * B.get('Doktrin.LevelRabatt', 0))} %.", "",
          "| Level-Wirkung | Wert |", "|---|---|"]
    for k in sorted(k for k in B if k.startswith("Tuerme.Level.") and ".L" in k):
        z.append(f"| {k.replace('Tuerme.Level.', '')} | {zahl(B[k])} |")
    # Munition
    z += ["", "### Munition", "", "| Schlüssel | Wert |", "|---|---|"]
    for k in sorted(k for k in B if k.startswith("Munition.")):
        z.append(f"| {k.replace('Munition.', '')} | {zahl(B[k])} |")
    # Resistenzen
    arten = ("Durchschlag", "Kugel", "Feuer", "Strom", "Explosion", "Gift")
    z += ["", "### Resistenzen (Schadensfaktor)", "", "| Familie | " + " | ".join(arten) + " |", "|---|" + "---|" * len(arten)]
    for f in sorted({k.split(".")[1] for k in B if k.startswith("Resistenzen.")}):
        z.append(f"| {f} | " + " | ".join(w(f"Resistenzen.{f}.{a}", "1") for a in arten) + " |")
    # Forschung
    z += ["", "### Forschung (MAM)", "", "Preis aus der Phase der effektiven Freigabe-Welle (Eltern und Voraussetzungen eingerechnet).", "",
          "| Baum | Knoten | Name | ab Welle | FP | Gabelung mit |", "|---|---|---|---|---|---|"]
    gesamt = 0
    for b_key, _, _, _, knoten in ns["BAEUME"]:
        for kk in knoten:
            fp = ns["preis"](b_key, kk[0])
            gesamt += fp
            andere = ", ".join(ns["andere_von"](b_key, kk[0]))
            z.append(f"| {b_key} | {kk[0]} | {kk[1]} | {ns['eff_welle'](b_key, kk[0])} | {fp} | {andere} |")
    z += ["", f"Alle Knoten zusammen: {gesamt} FP."]
    return z


def main():
    B, ns = lade()
    if not os.path.exists(DOK):
        print("Spielregeln.md fehlt - nichts erzeugt")
        return
    text = open(DOK, encoding="utf-8").read()
    if ANFANG not in text or ENDE not in text:
        print("Spielregeln.md: Marken fehlen - nichts erzeugt")
        return
    a = text.index(ANFANG) + len(ANFANG)
    b = text.index(ENDE)
    neu = text[:a] + "\n*Automatisch erzeugt von `Tests\\erzeuge_spielregeln.py` aus `TDBalance.cpp` und `create_research.py`. Nicht von Hand ändern.*\n\n" + "\n".join(tabellen(B, ns)) + "\n\n" + text[b:]
    if neu != text:
        open(DOK, "w", encoding="utf-8", newline="\n").write(neu)
        print("Spielregeln.md: Zahlen aktualisiert")
    else:
        print("Spielregeln.md: Zahlen unverändert")


if __name__ == "__main__":
    main()
