---
titel: Kleinfunde P16 – Welle wiederholen, Spielstandsnamen je Spiel, kein Abriss in der Welle, Freigabe ab Welle
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: Kleinfunde-P16
---

**Was (Fahrplan P16, Konzept `Notizen\Konzept-Kernzugang-Kleinfunde-2026-11-17.md` Teil B, Entscheidungen Marius 17.11.):**
- **A9 Welle wiederholen:** Der Kern merkt sich vor dem Speichern am Wellenstart, welche Welle der Stand wiederholt (`mLaufWiederholWelle`). Beim Laden gilt dieser Wert; alte Stände rechnen wie bisher. Damit haengt das Ergebnis nicht mehr davon ab, dass das Spiel erst am Bildende speichert.
- **A10 Spielstandsnamen:** `<Sitzung>_TD_W<n>_Start/_Ende` statt `FicsitTD_W<n>_…` fuer alle Spiele. „Welle wiederholen“ laedt nur einen Stand, dessen Kopf zur laufenden Sitzung passt. Die Bohrung schreibt wie Endlos keine Staende je Welle mehr. Alte `FicsitTD_W*`-Dateien bleiben liegen (nicht geloescht).
- **A12 kein Abriss waehrend der Welle** (Marius): Tuerme und TD-Bauten (Mauer, Panzersperre, Tor, alle Turm-Bauten) lassen sich im Verteidigungsfeld waehrend einer Welle nicht abreissen, mit Grund in der Bau-Pistole. Gleiche Regel wie die Bausperre (Einsatz, nicht Endlos). Spielgebaeude (Maschinen, Baender) bleiben abreissbar.
- **A13 Freigabe „ab Welle“:** eine Funktion `GetFreigabeWelle()` (Bauphase: naechste Welle, in der Welle: laufende) fuer MAM-Sperre, Knotenstatus, Schematic-Abhaengigkeit und Forschungsreiter. Forschung „ab 15“ ist in Welle 14 nicht mehr offen.

**Test:** Plan `Kleinfunde-P16` mit neuen Pruefungen `"pruefe": "Abriss"` und `"pruefe": "FreigabeWelle"` (vor, in und nach der Welle) und Logpruefung des neuen Namens.

**Simulation:** nachgezogen, keine Wirkung (betrifft nur Speichern, Abriss und Freigabezeitpunkt).

**Offen:** nichts im Spiel gelaufen. Laden eines Standes nach Wellenstart und zwei Sitzungen nacheinander sind noch nicht automatisch geprueft. B17 (Bohrplatz mit Teilweg) wartet auf Marius' Antwort, Statistik bleibt wie entschieden unveraendert.

**Changelog EN:** Wave saves now carry the session name, and "Retry wave" only loads a save of the current game. Turrets and defenses can no longer be dismantled near the Core during a wave. Research unlocked "from wave N" opens after the previous wave ends, not during it.

**Changelog DE:** Wellen-Spielstände tragen jetzt den Namen des Spiels, „Welle wiederholen“ lädt nur Stände dieses Spiels. Türme und Abwehrbauten lassen sich während einer Welle am Kern nicht mehr abreißen. Forschung „ab Welle N“ öffnet erst nach dem Ende der Welle davor.
