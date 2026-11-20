---
titel: Startfenster nach der Landung, Spielmodus standardmaessig Einsatz, Auswahl im Menue, Schutz bestehender Welten
typ: geaendert
modi: aus, einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: Grundtest
---

**Befund (Marius 18.11., Screenshots):** Im Menue „Neues Spiel“ waren „Spielmodus“ und „Stufe“ flache Zeilen mit kleiner Schrift, die
Auswahl am rechten Bildrand – „so ist es nicht transparent und schwer zu finden, auch fuer unsere Tester“. Standard war „Aus“: wer die
Einstellung nicht fand, bekam normales Satisfactory.

**Entscheidung Marius 18.11.:** beides vor dem Release.

**Was:**
1. **Standard Einsatz:** `UFGUserSetting_IntSelector::DefaultValue` des Spielmodus zur Laufzeit auf Einsatz (`TDGameInstanceModule.cpp`).
2. **Schutz bestehender Welten:** `UTDKernplatz::PruefeTDStart` – hat die geladene Welt schon Bauten des Spielers (nicht aus /FicsitTD/),
   aber keinen Abwehrkern, wird der Modus auf Aus gesetzt (`ForceSetOptionValue`), bevor ein Kernplatz entsteht; Hinweis im Chat.
   Grund: Spielstaende ohne gespeicherten Modus (z. B. Fabrikwelten, angelegt ohne Mod) bekaemen sonst den neuen Standard.
   Schalter `Spielmodus.BestehendeWeltAus` (1).
3. **Eigene Auswahl:** `UTDAuswahlKarten` (C++, `UFGOptionsValueController`) als `CustomValueSelectorWidget` von Spielmodus und Stufe:
   jede Moeglichkeit als Schaltflaeche in einer Zeile (siehe Nachtrag); Wert ueber `IFGOptionInterface`.
   Schalter `Spielmodus.EigeneAuswahl` (1).

**Nachtrag (Screenshot Marius 13:35):** Die Zeile im Menue hat eine feste Hoehe – die Liste untereinander wurde abgeschnitten und
ueberlappte „Stufe“. Jetzt alle Moeglichkeiten als Schaltflaechen **nebeneinander in einer Zeile** (Kurzname, 15 pt fett, gewaehlte orange),
der Beschreibungssatz als Tooltip.

**Startfenster (Entscheidung Marius 18.11., nach Screenshot 13:50: der Menue-Rahmen von SML bleibt ein Mini-Fenster):** `UTDStartfenster`
(C++, Stil Kern-Menue) oeffnet `UTDKernplatz::PruefeTDStart` in neuen Spielen nach der Landung, bevor der Kernplatz entsteht: vier
Modus-Karten (Einsatz, Endlos-Wellen, Freies Spiel mit Abwehr, Aus; Name 20 pt, zwei bis drei Saetze), Stufe als Schaltflaechen (nur bei
Einsatz/Endlos) mit Satz zur Stufe, „Los geht's“. Uebernahme ueber `ForceSetOptionValue` (Modus, Stufe), Merker
`ATDSpielstand::mModusGewaehlt`; Aus/Abwehr: kein Kernplatz, Hinweis im Chat. Nicht im Selbsttest (`UTDAutotest::LaeuftPlan`), Befehl
`/td startfenster [waehle <modus> <stufe>]`, Schalter `Spielmodus.Startfenster`.
**Nachweis:** Plan `Startfenster` DE und EN bestanden (Berichte `Testberichte/2026-11-18_1351_Startfenster.md`, `..._1353_...`, nach dem
Angleichen der Kartenhoehen `..._1359_...`), Fotos zeigen das Fenster, Wahl Einsatz/Normal uebernommen. Nicht geprueft: der echte Weg
nach der Landung in einem neuen Spiel (Marius beim Handspiel).

**Stand:** gebaut. Nicht geprueft: Aussehen im Menue (Marius beim Handspiel), Schutz bestehender Welten (braucht einen Stand ohne Mod).

**Simulation:** nicht betroffen.

**Changelog EN:** New games start in Operation by default; the game mode and difficulty are shown as clear choices with a short description. Existing worlds without a Defense Core stay regular Satisfactory.

**Changelog DE:** Neue Spiele starten standardmäßig als Einsatz; Spielmodus und Stufe stehen als gut lesbare Auswahl mit kurzer Beschreibung da. Bestehende Welten ohne Abwehrkern bleiben normales Satisfactory.
