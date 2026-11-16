"""FicsitTD: Modelle fuer die Spielmodi 2 (Endlos-Wellen) und 3 (Freies Spiel mit Abwehr, Bohrung). Stand 16.11.2026.

Baut auf strategien.py auf (Wirtschaft, Forschung, Kampf, Kernverlust) und ersetzt nur, was sich je Modus unterscheidet:

Endlos (TDWaveSubsystem.cpp mEndlos, EndlosAbstand, Endlos.* in TDBalance.cpp, TDEinsatz.cpp Starte/GibEndlosCoupons):
  - Wellen auf Zeit: erste Welle nach Endlos.<Stufe>.ErsteWelle, danach Abstand ab Wellenbeginn
    A(n) = max(Endlos.AbstandMin, Endlos.<Stufe>.Abstand - Endlos.AbstandJeWelle * n).
  - Welle frueher erledigt: Restzeit als Pause, mindestens Endlos.MinPause. Nicht erledigt: naechste Welle nach
    Endlos.UeberlaufPause obendrauf, die Reste bleiben im Feld (hier: Anteil der Gegner-LP wandert in die naechste Welle).
  - Nach Bosswellen mindestens Endlos.BossPause und Bonuswahl (Coupons / Kern / Festplatten).
  - Immer eine Front, Gegnerstrom 2/s (spaetestens nach Endlos.StromAnteil des Abstands draussen).
  - Vorziehen: 1 Coupon je Endlos.VorziehSekundenJeCoupon Restzeit.
  - Keine Bausperre, aber Bauzeit ist begrenzt: jedes Kontingent (Turm, Fabrikgebaeude, Kraftwerk, Stuetzbau) kostet
    BauSekundenJeGebaeude (Annahme, aus der Bauphasen-Messung abgeleitet, siehe kalibriere_bauzeit), Level im Turmfenster
    kosten keine Bauzeit. Verfuegbar ist die Pause plus BauAnteilInWelle der Wellenzeit.
  - Kernverlust beendet den Lauf, Wertung = letzte geschaffte Welle.

Bohrung (TDBohrung.cpp, Bohrung.* in TDBalance.cpp):
  - Wellen VonWelle..BisWelle mit den Werten der Stufe Normal (StarteKernVerteidigung(..., 1)), Kern-LP Bohrung.<Stufe>.LP.
  - Der Einsatz ist nicht aktiv (SetzeAbwehrKern), also keine Coupons, keine Grundreparatur zwischen den Wellen.
  - Es wird ein fester Aufbau geprueft (N Tuerme eines Typs auf Level L mit fester Munition, Fabrik deckt die Munition).
"""
import collections
import math
import random
import statistics

import simulator as sim
import strategien as st

ENDLOS_WERTE = {"Erkundung": (600, 240), "Normal": (480, 180), "Hart": (420, 165), "Albtraum": (360, 150)}   # TDEinsatz.cpp Starte
BOHR_STUFEN = ["Leicht", "Mittel", "Schwer"]
GABEL_STANDARD = {"Bolzen": "Streubolzen", "Gewehr": "Panzerbrechend", "Nobelisk": "PulsNobelisk", "Tesla": "Kettenblitz", "Kern": "KernSchild"}


# ---------------------------------------------------------------- Endlos
class EndlosLauf(st.Lauf):
    """Lauf mit begrenzter Bauzeit: kosten() liefert 'unbezahlbar', wenn die Bauzeit fuer die Kontingente nicht reicht"""
    UNBEZAHLBAR = 10 ** 9

    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.bauzeit = float("inf")
        self.gebaut = 0
        self.zeit_fehlt = 0

    def kosten(self, fehl):
        if len(fehl) * self.Z["BauSekundenJeGebaeude"] > self.bauzeit + 1e-9:
            self.zeit_fehlt += 1
            return self.UNBEZAHLBAR
        return super().kosten(fehl)

    def kaufe(self, fehl, posten):
        super().kaufe(fehl, posten)
        self.gebaut += len(fehl)
        if self.bauzeit != float("inf"):
            self.bauzeit = max(0.0, self.bauzeit - len(fehl) * self.Z["BauSekundenJeGebaeude"])


def endlos_abstand(B, stufe, n):
    erste, abstand = ENDLOS_WERTE[stufe]
    a0 = B.get(f"Endlos.{stufe}.Abstand", abstand)
    return max(B.get("Endlos.AbstandMin", 90), a0 - B.get("Endlos.AbstandJeWelle", 1.5) * n)


def spiele_endlos(s, wellen, B, Z, K, gabeln, tw, wi, stufe, max_wellen, rng, vorziehen=False):
    """Ein Endlos-Lauf. Ergebnis: erreichte Welle (geschafft), Q je Welle, Ueberlaeufe, Bauzeit-Engpaesse"""
    L = EndlosLauf(s, B, Z, K, gabeln, tw, wi, stufe)
    erste = B.get(f"Endlos.{stufe}.ErsteWelle", ENDLOS_WERTE[stufe][0])
    anlauf, u = Z["EndlosAnlaufSekunden"], Z["EndlosFeuerAnteil"]
    rest = []            # Gegner, die beim Ueberlauf noch leben: (Name, LP)
    bauzeit = erste      # bis Welle 1
    D_prev, luecke_prev = Z["WellenGrund"], erste
    verlauf, ueberlaeufe, geschafft = [], 0, 0
    for n in range(1, max_wellen + 1):
        A = endlos_abstand(B, stufe, n)
        zyklus = (luecke_prev + D_prev) / 60 if n > 1 else (erste + D_prev) / 60
        L.bauzeit = bauzeit
        L.forsche(n, gabeln)
        L.baue(n, zyklus, D_prev)
        L.verbrenne(n, zyklus, D_prev)
        ungenutzt = L.bauzeit
        gegner, elite = wellen.erzeuge(n, rng)
        alle = gegner + rest
        q, Kd, verl = L.kampf(alle, elite, n, rng)
        # Wellendauer: Strom 2/s (grosse Wellen schneller), Abwehr braucht q*60 s Feuer bei Feueranteil u
        strom = min(len(gegner) / max(1.0, B.get("Endlos.NachschubPaket", 2) / B.get("Endlos.NachschubTakt", 1)),
                    max(10.0, A * B.get("Endlos.StromAnteil", 0.5)))
        t_klar = anlauf + max(strom, q * 60 / u)
        ueberlauf = t_klar > A
        if ueberlauf:
            ueberlaeufe += 1
            anteil = min(1.0, (t_klar - A) / max(1.0, t_klar - anlauf))
            rest = [(name, l * anteil) for name, l in alle]
            ende, pause = A, B.get("Endlos.UeberlaufPause", 5)
        else:
            rest = []
            ende, pause = t_klar, max(B.get("Endlos.MinPause", 30), A - t_klar)
        boss = n % 10 == 0
        if boss:
            pause = max(pause, B.get("Endlos.BossPause", 300))
        verlauf.append({"Welle": n, "Q": q, "Q_Nenn": L.q_nenn, "Abstand": A, "Dauer": ende, "Ueberlauf": ueberlauf,
                        "Kern": max(0.0, L.kern), "Tuerme": len(L.tuerme), "Bauzeit_ungenutzt": ungenutzt})
        if L.kern <= 0:
            break
        geschafft = n
        L.belohnung(n, len(gegner), elite, boss)
        if boss:
            # Bonuswahl (Annahme): Kern +30 %, wenn er unter EndlosBonusKernUnter steht, sonst Coupons
            if L.kern < Z["EndlosBonusKernUnter"]:
                L.kern = min(100.0, L.kern + 100 * B.get("Endlos.Bonus.Kern", 0.3))
            else:
                L.ep += B.get("Endlos.Bonus.Coupons", 80)
        if vorziehen and not boss and not ueberlauf and ungenutzt > 0:
            # Wer nichts mehr zu bauen hat, zieht die naechste Welle vor und bekommt Coupons fuer die Restzeit
            spar = min(pause - 0.0, ungenutzt)
            L.ep += math.floor(spar / B.get("Endlos.VorziehSekundenJeCoupon", 10))
            pause -= spar
        bauzeit = pause + Z["BauAnteilInWelle"] * ende
        D_prev, luecke_prev = ende, pause
    return {"Welle": geschafft, "Kappung": geschafft >= max_wellen, "Verlauf": verlauf, "Ueberlaeufe": ueberlaeufe,
            "Gebaut": L.gebaut, "Tuerme": len(L.tuerme), "Level": statistics.mean(lv for _, lv in L.tuerme)}


def endlos_strategien():
    """Je Turmlinie ein Vertreter: Doktrin auf der Hauptlinie (ausser Breit), Ausgabestil Wert, ohne Kern-Ausbau"""
    aus = []
    for linie, gew in st.LINIEN.items():
        haupt = max(gew, key=lambda t: (gew[t], -st.TURM_TYPEN.index(t)))
        aus.append({"Linie": linie, "Gewichte": gew, "Doktrin": None if len(gew) == 5 else haupt, "Stil": "Wert", "Kern": False,
                    "Gabel": dict(GABEL_STANDARD)})
    return aus


def endlos_lauf(daten, stufe, max_wellen=150, laeufe=2, seed=11):
    B, A, Z, katalog, phasen, K, gabeln = daten
    wellen = sim.Wellen(B, katalog, phasen, A["Gegner_GrundLP"], stufe, endlos=True)
    tw, wi = st.Turmwerte(B), st.Wirtschaft(B, Z)
    erg = []
    for i, s in enumerate(endlos_strategien()):
        for vor in (False, True):
            for s_i in range(laeufe):
                r = spiele_endlos(s, wellen, B, Z, K, gabeln, tw, wi, stufe, max_wellen,
                                  random.Random(seed * 1009 + i * 31 + s_i * 7 + vor), vorziehen=vor)
                r.update({"Linie": s["Linie"], "Vorziehen": vor, "Startwert": s_i})
                erg.append(r)
    return erg


def kalibriere_bauzeit(daten, laeufe=1, seed=7):
    """BauSekundenJeGebaeude aus der Messung: Im Einsatz (Bauphase ohne Zeitgrenze) wartete Marius im Median BauphaseSekunden
    (195 s) je Bauphase. Der Modell-Spieler kauft im Einsatz Normal je Bauphase im Median k Kontingente -> 195 / k s je Gebaeude.
    Das ist eine Obergrenze (die Bauphase enthaelt auch Warten, Umbauen, Versorgen) und macht Endlos eher zu schwer."""
    B, A, Z, katalog, phasen, K, gabeln = daten
    wellen = sim.Wellen(B, katalog, phasen, A["Gegner_GrundLP"], "Normal")
    tw, wi = st.Turmwerte(B), st.Wirtschaft(B, Z)
    je_phase = []
    for i, s in enumerate(st.strategien(random.Random(seed))[::4]):
        rng = random.Random(seed + i)
        L = EndlosLauf(s, B, Z, K, gabeln, tw, wi, "Normal")
        for n in range(1, 31):
            D = Z["WellenGrund"] + Z["WellenJe"] * (n - 1)
            zyklus = st.zyklus_min(Z, n, D)
            L.forsche(n, gabeln)
            vor = L.gebaut
            L.baue(n, zyklus, D)
            if n > 1:
                je_phase.append(L.gebaut - vor)
            g, e = wellen.erzeuge(n, rng)
            L.kampf(g, e, n, rng)
            if L.kern <= 0:
                break
            L.belohnung(n, len(g), e, n % 10 == 0)
    k = statistics.median(je_phase)
    return Z["BauphaseSekunden"] / max(k, 0.5), k, statistics.mean(je_phase)


# ---------------------------------------------------------------- Bohrung
def bohr_werte(B, stufe):
    std = {"Leicht": (1, 4, 8000), "Mittel": (15, 19, 12000), "Schwer": (25, 30, 16000)}[stufe]   # TDBohrStufen[]
    return (int(B.get(f"Bohrung.{stufe}.VonWelle", std[0])), int(B.get(f"Bohrung.{stufe}.BisWelle", std[1])),
            B.get(f"Bohrung.{stufe}.LP", std[2]))


def bohr_aufbau_ok(daten, stufe, typ, level, mun_name, anzahl, wellen_liste, rng_saat):
    """Ein fester Aufbau gegen alle Wellen einer Bohrung. Ergebnis: (geschafft, Kern % am Ende, Q max)"""
    B, A, Z, katalog, phasen, K, gabeln = daten
    von, bis, lp = bohr_werte(B, stufe)
    s = {"Linie": typ, "Gewichte": {typ: 1}, "Doktrin": None, "Stil": "Wert", "Kern": False, "Gabel": dict(GABEL_STANDARD)}
    L = st.Lauf(s, B, Z, K, gabeln, st.Turmwerte(B), None, "Normal")
    L.kern_lp = lp
    mun = next(m for m in st.MUNITION[typ] if m[0] == mun_name)
    L.erforscht = set()                                      # ohne Kern-Ausbau, Doktrin und Hochspannung: nur der Aufbau zaehlt
    L.mun_aktiv[typ] = mun
    L.munition = lambda t, m=mun: [m]                        # feste Munition, die Fabrik deckt sie (Versorgung 1)
    L.tuerme = [[typ, level] for _ in range(anzahl)]
    rng = random.Random(rng_saat)
    qmax = 0.0
    for n in range(von, bis + 1):
        g, e = wellen_liste[n]
        q, _, _ = L.kampf(g, e, n, rng)
        qmax = max(qmax, q)
        if L.kern <= 0:
            return False, 0.0, qmax
    return True, L.kern, qmax


BOHR_AUFBAUTEN = [("Bolzenturm", 1, "Armierungsbolzen"), ("Bolzenturm", 2, "Armierungsbolzen"), ("Bolzenturm", 3, "Durchschlagbolzen"),
                  ("Gewehrturm", 1, "Standard"), ("Gewehrturm", 2, "Standard"), ("Gewehrturm", 3, "Panzerbrechend"),
                  ("Nobelisk-Werfer", 2, "Nobelisk"), ("Teslaspule", 2, "Entladung"), ("Flammenwerfer", 2, "Treibstoff")]


def bohrung_lauf(daten, stufe, laeufe=5, seed=3, quote=0.8, max_n=60):
    """Je Aufbau die kleinste Turmzahl, mit der mindestens `quote` der Laeufe die Bohrung schaffen"""
    B, A, Z, katalog, phasen, K, gabeln = daten
    wellen = sim.Wellen(B, katalog, phasen, A["Gegner_GrundLP"], "Normal")
    von, bis, _ = bohr_werte(B, stufe)
    saetze = []
    for s_i in range(laeufe):
        rng = random.Random(seed * 101 + s_i)
        saetze.append({n: wellen.erzeuge(n, rng) for n in range(von, bis + 1)})
    aus = []
    for typ, level, mun in BOHR_AUFBAUTEN:
        def erfolg(n):
            r = [bohr_aufbau_ok(daten, stufe, typ, level, mun, n, saetze[i], seed + i) for i in range(laeufe)]
            return sum(x[0] for x in r) / laeufe, statistics.median(x[1] for x in r), statistics.median(x[2] for x in r)
        lo, hi = 1, max_n
        if erfolg(hi)[0] < quote:
            aus.append({"Typ": typ, "Level": level, "Munition": mun, "Mindestzahl": None})
            continue
        while lo < hi:                                       # Erfolg steigt mit der Turmzahl (monoton bis auf Zufall)
            mid = (lo + hi) // 2
            if erfolg(mid)[0] >= quote:
                hi = mid
            else:
                lo = mid + 1
        q, kern, qmax = erfolg(lo)
        aus.append({"Typ": typ, "Level": level, "Munition": mun, "Mindestzahl": lo, "Erfolg": q, "Kern_Ende_Median": kern, "Q_max_Median": qmax})
    return aus
