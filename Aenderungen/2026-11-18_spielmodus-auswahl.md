---
titel: Spielmodus standardmaessig Einsatz, eigene gut lesbare Auswahl, Schutz bestehender Welten
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
   jede Moeglichkeit eine Zeile mit Namen (17 pt fett) und einem Satz Beschreibung, die gewaehlte orange; Wert ueber `IFGOptionInterface`.
   Schalter `Spielmodus.EigeneAuswahl` (1).

**Stand:** gebaut. Nicht geprueft: Aussehen im Menue (Marius beim Handspiel), Schutz bestehender Welten (braucht einen Stand ohne Mod).

**Simulation:** nicht betroffen.

**Changelog EN:** New games start in Operation by default; the game mode and difficulty are shown as clear choices with a short description. Existing worlds without a Defense Core stay regular Satisfactory.

**Changelog DE:** Neue Spiele starten standardmäßig als Einsatz; Spielmodus und Stufe stehen als gut lesbare Auswahl mit kurzer Beschreibung da. Bestehende Welten ohne Abwehrkern bleiben normales Satisfactory.
