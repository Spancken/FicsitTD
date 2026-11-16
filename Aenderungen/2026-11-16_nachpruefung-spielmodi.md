---
titel: Nachpruefung der Spielmodi - Welt nach Einsatz-Ende frei, Wertung, Kern-Bonus, Endlos-Ueberlauf, Freies Spiel
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: EinsatzPhase, Endlos, Bohrung
---

**Was (Nachpruefung 16.11., Bericht `Notizen\Nachpruefung-Spielmodi-2026-11-16.md`, Entscheidungen Marius 16.11.):**

Freies Spiel mit Abwehr:
- **F1:** Die Freigabe „ab Welle“ in den Forschungsknoten (`UTDWellenAbhaengigkeit`) gilt im Modus 3 nicht mehr. Vorher zaehlte die Welle der Bohrung, Turmfertigung II–IV und spaete Knoten (Himmelsturm, Stoerfeld, Drohnen, Singularitaet …) waren kaum oder nie erreichbar.
- **F2:** In der Bohrung zerstoerte Bauten geben ihr Material zurueck, zu Beginn jeder Bohrwelle heilen alle Verteidigungsbauten (wie im Einsatz).
- **F4:** Das Kern-Menue holt wartende Festplatten, Coupons und Material auch ohne Einsatz ab (nach dem Laden war der Einsatz-Kern leer).
- **F5:** Bohrung nur an echten Rohstoffknoten, nicht an Rohstoffbrocken (`AFGResourceDeposit`).
- **F8:** Talentpunkte aus Rang und Bohrung zusammen hoechstens 15 (`Pionier.MaxPunkte`).

Einsatz:
- **E1:** Nach „Einsatz beenden“ bleibt die Welt frei: Merkmal `mEinsatzBeendet` im Spielstand (`ATDSpielstand`), danach kein neuer Kernplatz (auch nach Abriss des Kerns) und ohne Entwicklermodus kein Start von Hand.
- **E3:** Erweiterte Spieleinstellungen des Spiels (Kreativ-Modus) = Lauf ohne Wertung, im Einsatz und im Endlos-Modus; Endlos beachtet jetzt auch Eingriffe (E8).
- **E4/E5:** Bonus „Kern“ = Kern-LP dauerhaft +10 % fuer den Lauf (`Einsatz.Bonus.KernMax`, Zaehler `mKernBonusStufen`) statt Heilung um 30 %. Offene Boni sammeln sich (`mBonusOffen`), gewaehlt wird zwischen den Wellen.

Endlos-Wellen:
- **N1:** Die Uhr laeuft auch, solange Nachschub aussteht; nicht losgeschickter Nachschub verfaellt beim Ueberlauf nicht, er kommt vor der naechsten Welle.
- **N2:** Bosswelle im Ueberlauf: keine Bosspause, keine Bonuswahl.
- **N3:** Keine Spielstaende je Welle mehr (Wiederholen ueber das Lademenue entfaellt, keine Hunderte Staende).
- Laden und Turmlevel auch in der Welle (es gibt keine Bausperre).

**Simulation:** Bonuswahl ist im Modell nicht enthalten; Freigabe „ab Welle“ im Modus 3 betrifft die Bohrungs-Simulation nicht (Tuerme fest vorgegeben). Simulation neu gerechnet, Stand in `sim_stand.json`.

**Offen (Fahrplan B13):** E2 Wiederholen zaehlt voll (so entschieden), E6 Chatzeilen an der Phasengrenze, E7 Stufe in der Bau-Meldung des Kernplatzes, N4 Ueberlauf heisst im Chat „abgewehrt“, N5 Vorziehen vor W1 (bleibt), N6–N8, F3 Wildtiere und Abwehrbauten, F6 Band-Upgrade, F7 Kern-Forschung ohne Wirkung im Modus 3, F9 zweiter Kern, F10 Einsatz-Texte im Modus 3, Plan „Spielersicht“ ohne Entwicklermodus. Im Spiel nicht nachgewiesen.

**Changelog EN:** Free play with defense: research no longer waits for wave numbers, destroyed buildings refund their materials during drilling and defenses heal each drilling wave, drilling only works at real resource nodes, talent points are capped at 15. Operation: after ending an operation the world stays free to build, advanced game settings make a run unranked, the Core bonus now permanently adds 10 % Core HP and unclaimed bonuses are kept. Endless waves: the timer keeps running while enemies are still queued, an unfinished boss wave gives no break or bonus, the shop is open during waves, no saves per wave.

**Changelog DE:** Freies Spiel mit Abwehr: Forschung wartet nicht mehr auf Wellennummern, in der Bohrung zerstörte Bauten geben ihr Material zurück und die Abwehr heilt zu jeder Bohrwelle, gebohrt wird nur an echten Rohstoffknoten, Talentpunkte höchstens 15. Einsatz: nach dem Beenden bleibt die Welt frei zum Weiterbauen, erweiterte Spieleinstellungen machen den Lauf wertungsfrei, der Kern-Bonus gibt dauerhaft 10 % Kern-LP und offene Boni bleiben erhalten. Endlos-Wellen: die Uhr läuft weiter, solange Gegner nachkommen, eine nicht geschaffte Bosswelle gibt keine Pause und keinen Bonus, der Laden ist in der Welle offen, keine Spielstände je Welle.
