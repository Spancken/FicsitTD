"""FicsitTD Optik-Durchgang 16.11.2026 (P4): Kontaktabzug aus einem Selbsttest Optik.

Legt alle Fotos eines Laufs (Satisfactory\\Testberichte\\<Zeit>_Optik\\Optik_*.jpg) in ein Raster: je Bau eine Zeile mit
Level 1 von zwei Seiten und Level 5, oben die Spielgebaeude (Konstruktor, Schmelzofen, Assembler) im selben Licht als Vergleich
(Pruefliste satisfactory-design Punkt 8). Dazu die Zeilen der Pruefung "Optik" aus dem Testbericht.

Aufruf: python Tools\\kontaktabzug.py [<Ordner des Laufs>]   (ohne Angabe: der neueste *_Optik-Lauf)
Ergebnis: <Zeit>_Optik_Kontaktabzug.html neben dem Testbericht (Bilder relativ verlinkt, nichts wird kopiert).
"""
import glob, html, os, re, sys

BERICHTE = r"D:\OneDrive\Claude\Claude Modding\Satisfactory\Testberichte"
BAUTEN = ["Bolzenturm", "Gewehrturm", "Teslaspule", "Flammenwerfer", "Nobelisk-Werfer", "Himmelsturm", "Singularitaet", "Stoerfeld",
          "Funkmast", "Schildgenerator", "Drohnenplatz", "Elektrozaun", "Lockfeuer", "Oelschlick", "Tor"]
SPALTEN = [("Level 1, 30°", "{b}_30"), ("Level 1, 210°", "{b}_210"), ("Level 5, 30°", "{b}_L5_30")]


def main():
    if len(sys.argv) > 1:
        ordner = sys.argv[1]
    else:
        laeufe = sorted(p for p in glob.glob(os.path.join(BERICHTE, "*_Optik")) if os.path.isdir(p))
        if not laeufe:
            raise SystemExit("kein *_Optik-Lauf gefunden")
        ordner = laeufe[-1]
    name = os.path.basename(ordner)
    bilder = {os.path.splitext(os.path.basename(p))[0][len("Optik_"):]: f"{name}/{os.path.basename(p)}"
              for p in glob.glob(os.path.join(ordner, "Optik_*.jpg"))}
    bericht = os.path.join(BERICHTE, name + ".md")
    pruef = []
    if os.path.exists(bericht):
        for z in open(bericht, encoding="utf-8"):
            if re.search(r"\| Pruefung \| Level \d", z):
                teile = [t.strip() for t in z.strip().strip("|").split("|")]
                pruef.append((teile[2], teile[3], teile[4]))

    def zelle(schl, titel):
        if schl in bilder:
            return f'<figure><a href="{html.escape(bilder[schl])}"><img src="{html.escape(bilder[schl])}" loading="lazy"></a><figcaption>{titel}</figcaption></figure>'
        return f'<figure class="leer"><div>kein Foto</div><figcaption>{titel}</figcaption></figure>'

    z = ["<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>",
         f"<title>Kontaktabzug {html.escape(name)}</title><style>",
         ":root{--kopf:#181818;--koerper:#2E2E2E;--feld:#161616;--orange:#FA9549;--text:#EBEBEB;--grau:#969696;--gut:#96D77D;--fehler:#EB5A46}",
         "body{margin:0;background:var(--koerper);color:var(--text);font-family:'Noto Sans',system-ui,sans-serif}",
         "header{background:var(--kopf);padding:12px 16px}h1{margin:0;font-size:20px}h2{font-size:16px;color:var(--orange);margin:20px 16px 8px}",
         ".zeile{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:8px;padding:0 16px}",
         "figure{margin:0;background:var(--feld);padding:6px}img{width:100%;display:block}figcaption{font-size:12px;color:var(--grau);padding-top:4px}",
         ".leer div{aspect-ratio:32/9;display:grid;place-items:center;color:var(--grau)}",
         "table{margin:0 16px;border-collapse:collapse;font-size:13px}td{padding:4px 8px;background:var(--feld)}.ja{color:var(--gut)}.nein{color:var(--fehler)}",
         "</style></head><body>",
         f"<header><h1>Kontaktabzug {html.escape(name)}</h1></header>"]
    if pruef:
        z.append("<h2>Prüfung Optik</h2><table>")
        for text, ok, ist in pruef:
            z.append(f"<tr><td>{html.escape(text)}</td><td class='{ 'ja' if ok == 'ja' else 'nein'}'>{html.escape(ok)}</td><td>{html.escape(ist)}</td></tr>")
        z.append("</table>")
    originale = sorted(k for k in bilder if k.startswith("Original-"))
    if originale:
        z.append("<h2>Spielgebäude im selben Licht (Vergleich)</h2><div class='zeile'>")
        z += [zelle(k, k[len("Original-"):].replace("_", ", ") + "°") for k in originale]
        z.append("</div>")
    for b in BAUTEN:
        if not any(k.startswith(b + "_") for k in bilder):
            continue
        z.append(f"<h2>{html.escape(b)}</h2><div class='zeile'>")
        z += [zelle(m.format(b=b), t) for t, m in SPALTEN]
        z.append("</div>")
    if "Ueberblick" in bilder:
        z.append("<h2>Überblick</h2><div class='zeile'>" + zelle("Ueberblick", "über dem Kern") + "</div>")
    z.append("<p style='padding:16px;color:var(--grau);font-size:12px'>Erzeugt von Tools\\kontaktabzug.py</p></body></html>")
    ziel = os.path.join(BERICHTE, name + "_Kontaktabzug.html")
    open(ziel, "w", encoding="utf-8").write("\n".join(z))
    print(f"Kontaktabzug: {ziel} ({len(bilder)} Fotos, {len(pruef)} Pruefzeilen)")


if __name__ == "__main__":
    main()
