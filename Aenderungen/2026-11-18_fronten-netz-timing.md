---
titel: Fronten haengen nicht mehr am Timing des Netzaufbaus (Welle 1 wartet auf gepruefte Fronten)
typ: behoben
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: ja
tests: P12-E-R3-Gewehr-A
---

**Befund (P12-E2 R3 vom 17.11. abends gegen alle anderen Laeufe auf Fels-S8):** Die Frontsuche beim Einsatzstart fand 0 von 16
Richtungen (36 Wege „teilweise“, Netz noch nicht fertig) und waehlte ungepruefte Ersatzpunkte (Norden, Sueden, Westen, Osten).
Der Plan startete Welle 1 nach 12 s, die Nachpruefung laeuft nur alle 15 s. Vor Welle 1 sperrte die Klippenprobe zuerst fuenf
Klippen am ungeprueften Nordweg, erst danach suchte `SpawneKernWelle` die Fronten neu – mit veraenderten Weglaengen. Ergebnis:
Norden zuerst (spaeter 118 m Weg) statt Nordwesten mit 343 m wie in allen anderen Laeufen. R3 fiel auf 32 % statt 84 %; der Lauf
ist als Balancewert ungueltig.

**Ursache:** Die Wahl der ersten Front (laengster Weg) haengt davon ab, wie weit Netz und Klippensperren bei der Suche sind.

**Was:**
1. `PruefeFrontenVorWelle` sucht ungepruefte Fronten zuerst neu, vor der Klippenprobe. So sieht die Neuwahl dasselbe Netz wie ein
   rechtzeitiger Start.
2. Der Wellenstart haelt bei 0 gepruften Richtungen an und sucht alle 2 s neu, hoechstens `Gegner.FrontenWarteMax` (30 s).
3. Startet eine Welle trotzdem mit ungeprueften Fronten, zaehlt das neue Sicherheitsnetz „Fronten ungeprueft“ und es gibt eine
   Warnung im Log – der Pruefstand markiert den Lauf damit als Fehler.
Die Bohrung (Abwehr-Modus) ist ausgenommen, sie hat eigene Regeln fuer Ersatzpunkte.

**Simulation:** nicht betroffen (Fronten sind im Simulator fest).

**Stand (P12-E4, 18.11., Bericht `Testberichte/2026-11-18_0709_P12-E4.md`):** gebaut, Reihenfolge wie am 17.11. (Leiste, dann R3).
Beide Plaene fanden 13 von 16 Richtungen, R3 startet mit Nordwesten 343 m wie alle gueltigen Laeufe, W30 Kern 87 % (vorher 84 %) –
der Wert ist wiederholbar. Der Ernstfall (Netz beim Start nicht fertig) trat **nicht** auf, Warten und Neusuche sind im Spiel also
noch nicht gelaufen. Kein Sicherheitsnetz „Fronten ungeprueft“.
Nebenbefund, andere Ursache: 2 Stinger (Radius 84) am Startpunkt Nordwesten festgefahren, `NavWalking`, „Boden begehbar NEIN“, 10 s nach dem Erscheinen.

**Changelog EN:** Enemy fronts no longer depend on how fast the navigation mesh loads: the first wave waits up to 30 seconds for checked paths, so the same core site always gets the same fronts.

**Changelog DE:** Die Fronten hängen nicht mehr davon ab, wie schnell das Navigationsnetz lädt: Die erste Welle wartet bis zu 30 Sekunden auf geprüfte Wege, derselbe Kernplatz bekommt immer dieselben Fronten.
