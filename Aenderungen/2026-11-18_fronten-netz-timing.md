---
titel: Fronten haengen nicht mehr am Timing des Netzaufbaus (Abgleich vor Welle 1)
typ: behoben
modi: einsatz, endlos
sichtbar: nein
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

**Nachtrag P12-E5 (18.11., Bericht `Testberichte/2026-11-18_0752_P12-E5.md`): erster Ansatz reicht nicht.** Der Ernstfall trat ein
(0 von 16 beim Start). Die Neusuche lief wie geplant vor der Klippenprobe und fand 13 Richtungen, waehlte aber Norden 357 m statt
Nordwesten 343 m. R3 fiel in W27. Die Klippensperren waren also nicht die Ursache: welche Wege gueltig sind (je Richtung nah,
mittel oder fern), haengt am Ladestand des Netzes. Die Kandidatenpunkte selbst sind fest.

**Zweiter Ansatz:** Abgleich vor Welle 1 eines neuen Laufs: ab `Gegner.FrontPruefungVorStart` (20 s) vor dem Start alle 2 s neu
suchen, bis zwei Suchen nacheinander dieselben Fronten liefern (Richtungen und Zahl gueltiger Richtungen), hoechstens
`Gegner.FrontenWarteMax` (30 s); erst dann Klippenprobe und Start. Nicht stabil nach 30 s: Sicherheitsnetz „Fronten ungeprueft“.
Fortgesetzte Spielstaende behalten ihre Fronten. Jede Suche schreibt alle 16 Richtungen ins Log (`Fronten-Richtungen`: je Abstand
Weg in m oder Grund), damit der naechste Ernstfall zeigt, was sich zwischen den Suchen aendert.

**Simulation:** nicht betroffen (Fronten sind im Simulator fest).

**Stand (P12-E4, 18.11., Bericht `Testberichte/2026-11-18_0709_P12-E4.md`):** gebaut, Reihenfolge wie am 17.11. (Leiste, dann R3).
Beide Plaene fanden 13 von 16 Richtungen, R3 startet mit Nordwesten 343 m wie alle gueltigen Laeufe, W30 Kern 87 % (vorher 84 %) –
der Wert ist wiederholbar. Der Ernstfall (Netz beim Start nicht fertig) trat **nicht** auf, Warten und Neusuche sind im Spiel also
noch nicht gelaufen. Kein Sicherheitsnetz „Fronten ungeprueft“.
Nebenbefund, andere Ursache: 2 Stinger (Radius 84) am Startpunkt Nordwesten festgefahren, `NavWalking`, „Boden begehbar NEIN“, 10 s nach dem Erscheinen.

**Nachtrag P12-E5 zweiter Lauf (18.11., Bericht `Testberichte/2026-11-18_0830_P12-E5.md`): Abgleich verschlechtert es.** Das neue
Richtungsprotokoll zeigt zwei stabile Zustaende: frueh (Einsatzstart) Nordwesten fern 343 m / Norden nah 339 m -> Nordwesten zuerst;
etwa 55 s spaeter Nordwesten 282-315 m / Norden nah 357 m -> Norden zuerst (Fronten Norden, Sueden, Westen, Osten). Der Abgleich
schaltete R6 von Nordwesten auf Norden; R3 und R6 fielen beide in W27. Abgleich deshalb aus (`Gegner.FrontenAbgleich` 0, Standard).
Was bleibt aktiv: Neusuche vor der Klippenprobe bei 0 gueltigen Richtungen, Warten bis 30 s, Sicherheitsnetz „Fronten ungeprueft“,
Richtungsprotokoll im Log. Die Fronten haengen damit weiter vom Ladestand ab – Entscheidung Marius offen (feste Fronten je Kernplatz?).
Nicht im Changelog, solange es nicht geloest ist.
