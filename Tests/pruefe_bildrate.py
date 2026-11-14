"""FicsitTD Selbsttest: War der Rechner waehrend des Laufs frei genug, damit das Ergebnis zaehlt?

Anlass 14.11.2026: Ein Lauf meldete 2 Fehler ("Front 1 hat nur noch einen Teilweg"), weil der Rechner ausgelastet war.
Das Spiel lief schon im Hauptmenue mit 1-4 Bildern/s, das Navigationsnetz war beim Wellenstart unfertig.
Normal sind 60-80 Bilder/s. Ein solcher Lauf ist weder bestanden noch durchgefallen, sondern ungueltig.

Aufruf (Teste-Mod.ps1 nach jedem Lauf):
    python pruefe_bildrate.py <Start UTC "YYYY-MM-DD HH:MM:SS"> <Bericht.md> [Log]
Misst die mittlere Bildrate ab dem Start aus dem Bildzaehler im Spiel-Log ([Zeit][Bild%1000]),
haengt einen Abschnitt an den Bericht und endet mit 2, wenn sie unter MINDEST_BILDER liegt (sonst 0).
"""
import os
import re
import sys
from datetime import datetime

MINDEST_BILDER = 15.0   # Bilder/s im Mittel ueber den Lauf; gute Laeufe 60-80, der Fehllauf 14.11. lag bei 3-4
LOG = os.path.join(os.environ.get("LOCALAPPDATA", ""), "FactoryGame", "Saved", "Logs", "FactoryGame.log")
ZEILE = re.compile(r"^\[(\d{4}\.\d\d\.\d\d-\d\d\.\d\d\.\d\d:\d{3})\]\[\s*(\d+)\]")


def messe(log, start):
    """Mittlere Bilder/s und schlechtestes 10-s-Fenster ab start (UTC, wie die Zeitstempel im Log)"""
    # Der Zaehler steht im Log nur modulo 1000. Liegen zwischen zwei Zeilen mehr als 1000 Bilder (z. B. 15 s ohne Log bei
    # 70 Bilder/s), ist der Sprung mehrdeutig. Deshalb die Anzahl Ueberlaeufe aus der vergangenen Zeit und der zuletzt
    # gemessenen Bildrate schaetzen (14.11.: ohne das ergab ein guter Lauf ein Fenster mit -17 Bilder/s).
    punkte, gesamt, letzte, letzte_zeit, rate = [], 0, None, None, 60.0
    for zeile in open(log, encoding="utf-8", errors="replace"):
        m = ZEILE.match(zeile)
        if not m:
            continue
        zeit = datetime.strptime(m.group(1), "%Y.%m.%d-%H.%M.%S:%f")
        bild = int(m.group(2))
        if letzte is not None:
            dt = max((zeit - letzte_zeit).total_seconds(), 0.0)
            schritt = (bild - letzte) % 1000
            schritt += 1000 * max(0, round((rate * dt - schritt) / 1000))
            gesamt += schritt
            if dt >= 1.0:
                rate = 0.7 * rate + 0.3 * (schritt / dt)
        letzte, letzte_zeit = bild, zeit
        if zeit >= start and bild > 0:
            punkte.append((zeit, gesamt))
    if len(punkte) < 2:
        return None
    dauer = (punkte[-1][0] - punkte[0][0]).total_seconds()
    if dauer < 20:
        return None
    mittel = (punkte[-1][1] - punkte[0][1]) / dauer
    fenster, i = [], 0
    for j in range(len(punkte)):
        while (punkte[j][0] - punkte[i][0]).total_seconds() > 10:
            i += 1
        sek = (punkte[j][0] - punkte[i][0]).total_seconds()
        if sek >= 8:
            fenster.append((punkte[j][1] - punkte[i][1]) / sek)
    return mittel, (min(fenster) if fenster else mittel), dauer


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 0
    start = datetime.strptime(sys.argv[1], "%Y-%m-%d %H:%M:%S")
    bericht = sys.argv[2]
    log = sys.argv[3] if len(sys.argv) > 3 else LOG
    if not os.path.exists(log):
        print("Bildrate: kein Spiel-Log gefunden")
        return 0
    wert = messe(log, start)
    if wert is None:
        print("Bildrate: zu wenig Log-Zeilen fuer eine Messung")
        return 0
    mittel, schlechtest, dauer = wert
    gueltig = mittel >= MINDEST_BILDER
    text = (f"Bildrate ueber den Lauf: im Mittel {mittel:.0f} Bilder/s, schlechtestes 10-s-Fenster {schlechtest:.0f} Bilder/s "
            f"({dauer / 60:.0f} min, Grenze {MINDEST_BILDER:.0f} Bilder/s).")
    aus = ["", "## Gültigkeit", "", text]
    if not gueltig:
        aus += ["", "**LAUF UNGÜLTIG:** Der Rechner war ausgelastet. Fehler und Kennzahlen dieses Laufs nicht werten, "
                "andere Last beenden und den Lauf wiederholen."]
    with open(bericht, "a", encoding="utf-8") as f:
        f.write("\n".join(aus) + "\n")
    print(("Bildrate ok: " if gueltig else "LAUF UNGUELTIG: ") + text)
    return 0 if gueltig else 2


if __name__ == "__main__":
    sys.exit(main())
