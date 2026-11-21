---
titel: Aufräumen nach 0.11.0, Testkultur, Diagnose Weltobjekte am Kern
typ: intern
modi: alle
sichtbar: nein
doku: nicht-noetig
simulation: ja
tests: Schadensleiste
experiment:
---

**Was:**
- Version auf 0.12.0. Zettel von 0.11.0 nach `Aenderungen\v0.11.0\` verschoben (Schritt 2 aus `ENTWICKLUNG.md` §5 war offen). `Ausgabe/` (Ausgabe der Pipeline) steht in `.gitignore`.
- Selbsttest: `-Kultur en` hält die englische Spieloberfläche jetzt selbst (`TDHalteTestKultur` in `TDAutotest.cpp`). Vorher schaltete das Spiel sechs Sekunden nach dem Start aus seiner Einstellung `mPrimaryLanguage=de` zurück auf Deutsch (Log 20.11. 11:31:14), deshalb blieben „Rezept auswählen“ und „Armierungsbolzen“ in englischen Fotos deutsch. Die Spieleinstellung wird nicht geändert.
- Diagnose für die „graue Kugel am Kern“ (Befund C2): Beim Einlesen der Welthindernisse meldet der Mod einmal je Kernort jedes fremde Objekt mit Modell im Umkreis von 35 m („Weltobjekt am Kern: …“ bzw. „Figur am Kern: …“ mit Klasse, Modell, Material, lebt/tot, höchstens 40 Zeilen). Die Fotos `FW10_C_High` und `FW6_B_TurretsFire` zeigen am Kernsockel eine graue Figur mit Hog-Körper, schon vor Welle 1 (`Fab_E_Base`). Ursache offen.
- Simulation aller Modi nachgezogen.

**Warum:** Offene Punkte aus `Stand-nach-Release-0.11.0-2026-11-20.md`, Etappe E0 im Fahrplan 0.12.
