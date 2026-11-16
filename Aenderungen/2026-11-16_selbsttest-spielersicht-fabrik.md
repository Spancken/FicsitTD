---
titel: Selbsttests Spielersicht und Fabrik-Pruefstand F1, Meldungen Ueberlauf und Stufe
typ: intern
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: ja
tests: Spielersicht, Fabrik-F1
---

**Was (16.11., Nachpruefung B14 und Konzept Fabrik-Pruefstand, Marius: „kompletten autonomen Testlauf bauen“):**
- **Spielersicht:** `UTDKernplatz::SetzeSpielersicht` schaltet den Entwicklermodus zur Laufzeit ab (eine Quelle fuer alle Pruefungen, auch `/td`-Befehle). Autotest: Schritt `"spielersicht": true|false`, Pruefung `"TestKnoepfe"` (sichtbare Entwickler-Knoepfe im Kern-Menue), `"befehl"` mit `"antwortEnthaelt"`. Jeder Plan beginnt wieder mit Entwicklermodus. Plan `Spielersicht`.
- **Fabrik-Pruefstand F1** (`TDFabrikPruefstand.cpp`): Schritt `"fabrik": "Bolzen"` baut Miner Mk1 auf dem naechsten freien Eisenknoten, Schmelzofen, Konstrukteur Stangen, Konstrukteur Armierungsbolzen, Splitter-Kette und Tuerme; Baender ueber `UFGTestBlueprintFunctionLibrary::SpawnSplineBuildable` des Spiels. Munition nur ueber das Band. `"fabrikbericht"`: Produktivitaet je Maschine, Munition je Turm. Plan `Fabrik-F1`. Strom noch ueber `stromfrei` (Baustein B5 folgt), Kontingent mit `td einsatz frei` umgangen (B6 folgt).
- Meldungen: Ueberlauf im Endlos-Modus heisst nicht mehr „abgewehrt“ (N4); die Bau-Meldung des Kernplatzes nennt die gewaehlte Stufe (E7).

**Offen:** Im Spiel noch nicht gelaufen (Fenster B Teil 2).

**Changelog EN:** Endless waves: an overflowing wave is no longer announced as repelled. The core site message names the chosen difficulty.

**Changelog DE:** Endlos-Wellen: Eine überlaufende Welle heißt nicht mehr „abgewehrt“. Die Meldung beim Bau des Kernplatzes nennt die gewählte Stufe.
