"""Schneidet Autotest-Fotos (z. B. 5120 x 1440 auf Ultrawide) auf 16:9 fuer Handbuch und ficsit.app.

Aufruf:
  python -I schneide_screenshots.py <Quellordner mit PNG> <Zielordner> NAME[:x] [NAME[:x] ...]

x = Mitte des Ausschnitts als Anteil der Bildbreite (0..1, Standard 0.5). Ausgabe: JPG, 1920 x 1080, Qualitaet 88.
Beispiel: EN_Role_Commander_HUD:0.3 nimmt den linken Teil mit dem Rollenabzeichen.
"""
import sys
from pathlib import Path

from PIL import Image

BREITE, HOEHE = 1920, 1080


def schneide(quelle: Path, ziel: Path, mitte: float) -> tuple[int, int]:
    bild = Image.open(quelle).convert("RGB")
    b, h = bild.size
    soll_b = min(b, round(h * 16 / 9))
    soll_h = min(h, round(soll_b * 9 / 16))
    links = round(mitte * b - soll_b / 2)
    links = max(0, min(b - soll_b, links))
    oben = (h - soll_h) // 2
    teil = bild.crop((links, oben, links + soll_b, oben + soll_h)).resize((BREITE, HOEHE), Image.LANCZOS)
    teil.save(ziel, "JPEG", quality=88, optimize=True)
    return links, soll_b


def main() -> int:
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    quelle, ziel = Path(sys.argv[1]), Path(sys.argv[2])
    ziel.mkdir(parents=True, exist_ok=True)
    for eintrag in sys.argv[3:]:
        name, _, x = eintrag.partition(":")
        mitte = float(x) if x else 0.5
        datei = quelle / f"{name}.png"
        if not datei.exists():
            print(f"FEHLT {datei}")
            continue
        links, b = schneide(datei, ziel / f"{name}.jpg", mitte)
        print(f"ok {name}: Ausschnitt ab x={links}, Breite {b} -> {BREITE}x{HOEHE}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
