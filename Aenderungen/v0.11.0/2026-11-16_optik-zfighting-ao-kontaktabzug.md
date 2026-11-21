---
titel: Deckungsgleiche Flächen beseitigt (Z-Fighting), AO-Sperrklinke in der Pipeline, Kontaktabzug mit Spielgebäuden
typ: behoben
modi: alle
sichtbar: ja
doku: nicht-noetig
simulation: nicht-noetig
tests: Optik
experiment:
---

**Was:**
- **Z-Fighting:** In 12 Modellen lagen sichtbare Flächen zweier Teile deckungsgleich übereinander, zum Beispiel Mündungsloch auf der Mündung oder Stufenleisten auf Paneelen. Im Spiel flimmern solche Flächen.
  - `Satisfactory\Modelle\td_deckgleich.py --beheben` hat je Paar das kleinere Teil 3 mm herausgeschoben, in drei Durchgängen.
  - Ergebnis: von 213 sichtbaren Paaren sind 8 kleine Reste übrig (≤ 0,08 m², Fasen am Schildgenerator, Himmelsturm und an der Teslaspule).
  - Unterseiten am Boden und von einem dritten Teil verdeckte Flächen zählen nicht.
  - Neu exportiert, gebacken und importiert: Bolzenturm, Gewehrturm, Nobelisk-Werfer, Flammenwerfer, Teslaspule, Himmelsturm, Singularität, Funkmast, Schildgenerator, Drohnenplatz, Lockfeuer, Tor.
- **AO-Sperrklinke:** `Tests\pruefe_ao.py` (Pipeline-Schritt „Optik AO“, Blender, rund 20 s) misst je gebackenem Teil die sichtbare Fläche mit AO < 0,1 und vergleicht sie mit `Tests\ao_basis.json`. Ein Fehler entsteht, wenn ein Teil um mehr als 3 Prozentpunkte schlechter wird oder ein neues Teil über 20 % liegt.
- **Kontaktabzug (P4):** Der Plan `Optik` baut Konstruktor, Schmelzofen und Assembler des Spiels und fotografiert sie im selben Licht. `python Tools\kontaktabzug.py` legt alle Fotos eines Laufs mit den Prüfzeilen in eine HTML-Seite neben den Testbericht.
- **Autotest (additiv):**
  - `baue` nimmt mit `"klasse"` auch Spielgebäude.
  - `stelle` findet Bauten auch über den Klassennamen (`Build_…`).

**Warum:** Offene Punkte des Optik-Durchgangs 16.11.2026 (Bericht `Satisfactory\Notizen\Optik-Durchgang-2026-11-16.md`, Abschnitt 8).

**Changelog EN:** Towers and buildings: removed flickering overlapping surfaces.

**Changelog DE:** Türme und Bauten: flimmernde, übereinanderliegende Flächen beseitigt.
