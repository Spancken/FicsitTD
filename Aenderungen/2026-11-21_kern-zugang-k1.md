---
titel: Kern-Zugang K1 – Prüfung „Kern erreichbar“, Wellenstart wartet bei abgeriegeltem Kern
typ: neu
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: KZ-1-Einsatz, KZ-6-Bohrung
experiment:
---

**Was:**
- Vor jeder Welle prüft der Mod, ob der Abwehrkern von jeder aktiven Front aus erreichbar ist. Dafür gibt es `UTDWaveSubsystem::PruefeKernZugang`, neue Datei `TDKernZugang.cpp`. Es gibt vier Zustände:
  - `Frei`
  - `Durchbrechbar`: Den Weg sperren nur TD-Mauern, Sperren, Tore oder Zäune, die die Gegner angreifen.
  - `NetzNichtBereit`
  - `Unpassierbar`
- Bei einem Teilweg sucht die Prüfung die Sperrstelle:
  - fünf Kapselfahrten vom Wegende Richtung Kern, ±20° und ±40°, je 12 m
  - Treffer werden über den Instanz-Manager und Lightweight-Instanzen in den Bau aufgelöst
  - Brückentest hinter dem Bau
- Riegelt ein Bau des Spielers den Kern ab, also eine Maschine, ein Turm oder ein Spielbau, startet die Welle nicht. Das gilt im Einsatz, im Endlos und zwischen Bohrwellen.
  - Die Sperrstelle wird markiert: Kartenmarke „Kern abgeriegelt: <Bau>“, Chatzeile mit Bau, Abstand und Richtung, alle 30 s wiederholt.
  - Die HUD-Zeile zeigt „Welle N wartet: Kern abgeriegelt“, der Knopf im Kern-Menü „Kern abgeriegelt (Karte)“.
  - Sobald wieder ein Weg frei ist, startet die Welle.
- Ohne erkannten Bau (Gelände, Loch im Netz) wartet der Start höchstens 20 s (`KernZugang.WarteOhneBau`), damit ein Netzfehler den Einsatz nicht sperrt.
- Die Frontprüfung vor der Welle sucht die Fronten nicht mehr neu, wenn ein Bau den Teilweg verursacht. Vorher fand die Neuwahl dann 0 gültige Richtungen.
- Die Bohrung startet nicht, wenn ein Bau den Bohrer abriegelt (Ringprobe, 8 Richtungen in 60 und 120 m). Die Meldung nennt Bau und Stelle, dazu kommt die Kartenmarke.
- Nichts wird abgerissen.
- Neuer Sicherheitsnetz-Zähler `Kern abgeriegelt` (`TDNetz::EArt::Abriegelung`), je Wartefall einmal. Ziel 0, sobald die Wegprüfung beim Bauen (K4) steht.
- Selbsttest:
  - Schritte `spielbau` (Ring aus „Wand“, „Fundament“ oder „Konstruktor“, `ring` in m, `luecke`) und `entferne`.
  - Prüfungen `KernZugang` (`soll`, `bau`, `sperre`) und `WartetAufKernZugang`.
  - Pläne `KZ-1-Einsatz`, `KZ-6-Bohrung`, Paket `Kern-Zugang-K1`.
- Balance-Schlüssel `KernZugang.*`: Aktiv, StartWarten, Haltezeit, PruefTakt, Wiederholung, WarteOhneBau, StrahlLaenge, MaxSuchen, RingAbstand1/2.

**Warum:** Fahrplan 0.13, Etappe K1, Marius am 21.11. abends:
- Fabrikgebäude werden nie zerstört.
- Wer den Kern abriegelt, bekommt keine Welle, die Sperre wird angezeigt.
- In der Bohrung gibt es nur die Wegprüfung beim Start.
- Grundlage für K2 (Ring „instabiler Boden“) und K4 (Wegprüfung beim Bauen).

**Nicht enthalten:**
- Nachzügler ohne Versetzen über Sperren (A1 Nr. 6) und Belagerung (A1 Nr. 5). Beides hängt an der offenen Entscheidung „Endlos in der Welle“.
- Logistik-Nachzug in der Welle (A1 Nr. 8).
- Prüfung im Netz der großen Gegner. K1 prüft nur das Standardnetz.
- `Durchbrechbar` macht eine Bohrung mit geschlossenem TD-Mauerring noch nicht startbar, die Fronten gelten dort weiter als ungültig. Das kommt mit K3 (A1 F8).

**Changelog EN:** A sealed-off Core no longer wins waves: if your buildings block every path to the Core, the wave (or the drilling) will not start until a gap is open, and the blocking spot is marked on the map. Nothing is ever demolished.

**Changelog DE:** Ein abgeriegelter Kern gewinnt keine Wellen mehr: Sperren deine Bauten jeden Weg zum Kern, startet die Welle (oder die Bohrung) erst, wenn wieder eine Lücke frei ist; die Sperrstelle steht auf der Karte. Abgerissen wird nie etwas.

**Nachweis (22.11. morgens):**
- Paket `Kern-Zugang-K1`, Lauf `2026-11-22_0727`: `KZ-1-Einsatz`, Grundtest und EinsatzPhase bestanden.
- `KZ-6-Bohrung` nach der Textkorrektur bestanden, Lauf `2026-11-22_0733`.
- Belegt:
  - Wandring 20 m mit Lücke gilt als frei.
  - Ein geschlossener Ring wird erkannt („abgeriegelt durch Standardwand (4 m)“, 2 Wegsuchen, 0,5 ms).
  - Der Wellenstart wartet auch nach 40 s noch, Kartenmarke gesetzt, Fronten bleiben.
  - Nach dem Entfernen startet die Welle.
  - Bohrung: Start abgelehnt mit Bau und Stelle, nach dem Öffnen startet sie.

**Befund aus den Messläufen** (`2026-11-22_0654/0703_Kern-Zugang-Messung`, Plan `KZ-0-Messung`):
- **Spielwände, Fundamente und TD-Mauern** werden aus dem Navigationsnetz der Gegner ausgespart. Wenn sie über das Hologramm gebaut sind, baut das Netz von selbst um.
- **Maschinen (Konstruktor) und Türme (Bolzenturm)** wirken **nicht** aufs Netz. Unter allen 41 Konstruktoren und 57 Türmen liegt Netz, auch nach einer Netzauffrischung.
  - Die Gegner planen also durch Maschinen und Türme hindurch und bleiben dann an ihnen hängen.
  - Ein Ring aus Maschinen oder Türmen ist für K1 unsichtbar.
  - Ausgerechnet diesen Fall gibt es im Einsatz, wo Spielwände gesperrt sind.
  - Ohne Lösung dafür ist K1 im Einsatz kaum wirksam. Entscheidung offen (Fahrplan 0.13).
- Per Code gesetzte Bauten (`BeginSpawnBuildable`) bauen das Netz nicht um. Die Selbsttests bauen deshalb über das Hologramm (`spielbau` mit `echt`) oder rufen `netzauffrischen` auf.

**Korrekturen nach den Läufen:**
- Während eines Netzumbaus urteilt die Prüfung nicht. Der Start hält dann höchstens `KernZugang.WarteNetz` = 10 s.
- Eine Abriegelung wird erst gemeldet (Ansage, Marke, Zähler), wenn eine zweite Prüfung sie bestätigt. Vorher meldete die Rücknahme einer Klippensperre im selben Bild „Unpassierbar“.
- Ringprobe: Punkte ohne Netz zählen nicht.
- Bohrung: Ist der Zugang frei, sucht sie Fronten ohne gültige Richtung neu.
- Der Meldetext nennt den geprüften Kern.
