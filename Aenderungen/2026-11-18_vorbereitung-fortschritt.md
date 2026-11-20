---
titel: Fortschrittsanzeige der Vorbereitung, Startfenster sofort, Bau gleich nach der Wahl
typ: neu
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: ja
tests: Vorbereitung
---

**Befund (Marius 18.11.):** Nach der Landung und nach der Modus-Wahl passierte lange sichtbar nichts. Log seines Spiels: 27 s von
Karte geladen bis Kernplatz-Bau (Landung, dann 6 s zu Fuss), 18 s Warten auf das Navigationsnetz nur mit einer Chatzeile; mit
Startfenster kamen nach der Wahl noch einmal 6 s dazu.

**Was:**
1. Startfenster oeffnet, sobald der Spieler nach der Landung zu Fuss ist (ohne die 6 s); nach „Los geht's“ startet der Bau sofort.
2. `UTDVorbereitung` (oben Mitte, orange umrandet): „Einsatzort wird vorbereitet“ mit vier Schritten – Einsatzort anfliegen, Gelaende
   laden, Plattform und Abwehrkern bauen, Wege der Gegner berechnen (bis zu 60 s) –, laufender Schritt orange mit Punkten und Sekunden,
   erledigte gruen, am Ende „Bereit! Dein Einsatz beginnt.“, nach 3 s zu. Steuerung `UTDKernplatz::Fortschritt`. Schalter
   `Spielmodus.Fortschritt`.

**Stand:** gebaut; Nachweis Plan `Vorbereitung` (Fotos in kurzen Abstaenden nach dem Laden) offen.

**Simulation:** nicht betroffen.

**Changelog EN:** While your operation site is being prepared, a progress panel shows each step, so you know what is happening.

**Changelog DE:** Während dein Einsatzort vorbereitet wird, zeigt eine Anzeige jeden Schritt – du siehst, was gerade passiert.
