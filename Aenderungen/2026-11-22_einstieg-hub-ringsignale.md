---
titel: Einstieg HUB → Shop → MAM, neue ADA-Begrüßung, Ringsignale im Baumodus, Ring-Ansage, Lückenmarke
typ: neu
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: KZ-3-Ring, KZ-4-Wegpruefung, EinsatzPhase, Shop
experiment:
---

**Was:**
- **Einstieg neu** (Marius 22.11.): zu Beginn die Aufgabe „Baue die HUB“ (Strom aus den Biomasse-Brennern, Wiedereinstiegspunkt), nach Welle 1 der AWESOME-Shop (`Einsatz.Shop.AbWelle` 1, vorher 2), nach Welle 2 das MAM (`Einsatz.Mam.AbWelle` 2, vorher nach Welle 1).
  - Jede Aufgabe kommt einmal im Chat und von ADA mit Stimme (`AufgabeHub` neu, `AufgabeShop` und `AufgabeMam` umformuliert) und steht in der HUD-Zeile, Reihenfolge HUB, Shop, MAM.
  - Die Aufgabentexte nennen „mindestens 60 m vom Kern“ (ursprünglich 40 m, seit H3 60 m).
- **ADA-Begrüßung** neu, 8 Sätze DE/EN mit Stimme und Untertiteln:
  - neu: Boden 60 m (zuerst 40 m, H3), „lassen Sie den Kreaturen einen Weg“, Reihenfolge HUB, Shop, MAM, „bis Ihr Shop steht, verkauft der Kern“
  - Quelle `Scripts/Audio/saetze_begruessung.json`
- **Ringsignale:** kleine orange Leuchtsignale (Lampenkopf `SM_TD_StatusLight` vierfach vergrößert, ≈ 1 m, alle 6 m) markieren den Ring (60 m). Sie sind nur sichtbar, solange die Bau-Pistole im Baumodus ist (Einsatz, Endlos), Schalter `Einsatz.Ring.Signale*`.
- **Ring-Ansage:** Beim ersten Versuch, im Ring ein Fabrikgebäude zu bauen, sagt ADA mit Stimme, warum das nicht geht (`RingInstabil`, einmal je Kern).
- **Lückenmarke:** Ist ein Hologramm rot, weil es den letzten Weg schließen würde, zeigen grüne Signale entlang des letzten Wegs (25 m um das Hologramm, alle 3 m), wo die Gegner durch müssen. Schalter `KernZugang.LueckeMarkieren`.
- Selbsttest: Prüfung `Ringsignale` (`min`, `sichtbar`), Plan `KZ-3-Ring` (Signale, Ansage einmal), `KZ-4-Wegpruefung` (Lückenmarke), `EinsatzPhase` (HUB-Aufgabe), Paket `Einstieg-013`.

**Warum:** Marius am 22.11.:
- „HUB Welle 1 (Strom und Spawnpunkt), Shop Welle 2, direkt Kontingent zum Bauen nutzen, MAM in Welle 3, da kommen Forschungspunkte dazu, dann baut sich alles logisch auf.“
- „Ring mit kleinen leuchtenden Signalen im Baumodus“, „ADA-Ansage beim ersten Versuch im Ring zu bauen“, „letzte Lücke markieren: ja“.
- Die HUB war nie gesperrt, nur ihre Meilensteine (Entscheidung 12.11.). Damit ist T2-5 („Errichte die HUB“ als Spielziel) erledigt, denn die HUB ist jetzt die erste Aufgabe.

**Changelog EN:** New start order: ADA asks for the HUB first (power and respawn point), the AWESOME Shop after wave 1 and the MAM after wave 2. A new ADA briefing explains the order and the unstable ground. In build mode small orange lights mark the 60 m ring, and green lights show the last path when a building would close it.

**Changelog DE:** Neuer Einstieg: ADA bittet zuerst um die HUB (Strom und Wiedereinstiegspunkt), nach Welle 1 um den AWESOME-Shop, nach Welle 2 um das MAM. Eine neue ADA-Begrüßung erklärt die Reihenfolge und den instabilen Boden. Im Baumodus markieren kleine orange Leuchtsignale den 60-m-Ring, und grüne Signale zeigen den letzten Weg, wenn ein Bau ihn schließen würde.

**Nachweis (22.11. mittags):**
- Paket `Einstieg-013` (`2026-11-22_1230`): KZ-3, KZ-4, EinsatzPhase, Shop und Grundtest grün.
  - Aufgabe HUB zu Beginn einmal, ADA sagt `AufgabeHub` mit Stimme, die HUD-Zeile „AUFGABE: Baue die HUB“ ist im Foto zu sehen.
  - Shop nach Welle 1, MAM nach Welle 2.
  - Ringsignale 42 Lampen, ohne Baumodus ausgeblendet.
  - `RingInstabil` nur einmal, trotz zweitem Versuch.
  - Lückenmarke mit 16 Signalen.
  - `Sperren-1121` wurde dort nur wegen der Planreihenfolge rot (die Hook-Zeile steht vor dem Planstart), allein grün (`_1232`).
- Gesamtpaket `Kern-Zugang-K1` (`2026-11-22_1258`): alle 8 Pläne grün.
- Fotos `KZ-3-Foto` (`_1307`): Die Ringsignale sind als Lampenkranz um den Kern zu sehen. Im ersten Fotolauf (`_1302`) waren sie mit 36 cm und Leuchtkraft 20 kaum sichtbar, jetzt `Einsatz.Ring.SignalGroesse` 4 (≈ 1 m) und Leuchtkraft 80.
