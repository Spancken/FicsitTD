---
titel: Wegprüfung beim Bauen – Bau, der den letzten Weg zum Kern schließt, ist rot (K4)
typ: neu
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: KZ-4-Wegpruefung
experiment:
---

**Was:**
- Solange ein Kern läuft (Einsatz, Endlos, laufende Bohrung), hält das Wellensystem 16 „Lebensadern“ bereit: volle Wege aus 16 Richtungen in 120 m (`KernZugang.LebensaderAbstand`) zum Kern. Gerechnet wird reihum eine Richtung je Bild, während eines Netzumbaus nicht.
- Ein Hologramm wird rot mit „Würde den letzten Weg zum Kern schließen. Lass eine Lücke oder sperre mit einer Abwehrmauer.“ (DE/EN, `UTDCDKernAbriegelung`), wenn zweierlei zutrifft:
  - Es ist ein Bau, der den Weg sperrt: Maschine, Turm, Lager, Fundament, Wand.
  - Sein Kasten, um 60 cm erweitert, schneidet alle gültigen Lebensadern (mindestens 3).
- Die letzten 14 m am Kern zählen nicht, dort laufen alle Wege zusammen.
- Mehrere Lücken verteilen die Adern, dann bleibt der Bau grün.
- TD-Mauer, Sperre, Tor und Elektrozaun dürfen schließen, die Gegner greifen sie an. Logistik ist frei.
- Was trotzdem durchrutscht (zwei Teile zugleich, Bau vor dem Neuaufbau der Adern), fängt die Prüfung vor der Welle ab: Die Welle wartet, nichts wird abgerissen (K1).
- Haken: `AFGHologram::ValidatePlacementAndCost` (wie die Kontingente). Schalter `KernZugang.Wegpruefung`.
- Selbsttest: Prüfung `Wegpruefung` (Rechteck bei Winkel/Abstand) und Schritt `hologramm` (echtes Hologramm aus dem Rezept, Platzprüfung wie die Bau-Pistole, Ablehnungsgründe auslesen).

**Warum:** Marius am 21.11. abends: Baut man Fabrikgebäude so, dass sie die Wege schließen würden, prüft der Mod, ob noch ein Weg frei ist, und sonst lässt sich der Bau nicht setzen. Türme zählen mit (Dialog 21.11.). Fahrplan 0.13, Etappe K4.

**Noch offen:** Die letzte Lücke wird noch nicht am Boden markiert, nur der Text im Hologramm erscheint.

**Changelog EN:** Building something that would close the last path to the Core is now blocked in the hologram ("Would close the last path to the Core"). Defense walls may still close it – enemies attack them.

**Changelog DE:** Ein Bau, der den letzten Weg zum Kern schließen würde, ist jetzt im Hologramm gesperrt („Würde den letzten Weg zum Kern schließen“). Abwehrmauern dürfen schließen – die Gegner greifen sie an.

**Nachweis (22.11. vormittags):** Plan `KZ-4-Wegpruefung` bestanden (Läufe `1007` und `1031`).
- Ohne Ring frei, mit Wandring und Lücke frei.
- Ein Bau in der Lücke schneidet 10 von 10 Lebensadern, gegenüber 0 von 10.
- Echtes Hologramm Bolzenturm in der Lücke: `TDCDKernAbriegelung`. Gegenüber kein Grund, Verteidigungsmauer in der Lücke erlaubt.
- Vorher korrigiert: Der Hologramm-Kasten nahm Reichweitenring, Freiraum-Prüfkasten und eine leere Instanzen-Komponente am Weltursprung mit (Läufe `0948` bis `1003`). Jetzt zählen nur sichtbare Modellteile am Hologramm. Diagnose-Logzeile mit `KernZugang.HologrammLog` 1.
