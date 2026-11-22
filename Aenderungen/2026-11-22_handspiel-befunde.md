---
titel: Befunde aus Marius' Handspiel 22.11. – Plattform, MAM nach Welle 2, Ring 60 m, Lags, Shop-Bereiche, Frontabstand
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: Sperren-1121, KZ-3-Ring, KZ-4-Wegpruefung, EinsatzPhase, Shop
experiment:
---

**Was** (Befundliste `Satisfactory/Notizen/Funde-Handspiel-2026-11-22.md`):
- **H1 Plattform:** Fundamente und Rampen der Kernplattform sind schon gesperrt, solange der Kernplatz auf das Navigationsnetz wartet. Vorher war das erst ab Einsatzstart so. Dafür gibt es `UTDKernplatz::GetWartenderKern`, `mWartetAufNetz`.
- **H2 MAM:**
  - Das MAM-Rezept ist nicht mehr im Startpaket, `FreigabeMam` gibt es nach `Einsatz.Mam.AbWelle` (2).
  - Bis dahin ist das Kontingent MAM 0, auch wenn die HUB-Stufen das Rezept liefern.
- **H3 Ring 60 m:** `Einsatz.Ring.Radius` 60 (vorher 40). Das ist die Fläche, die der Kernplatz vom Bewuchs räumt. Aufgabentexte, ADA-Begrüßung und ADA-Ansage sagen „sechzig Meter“, mit neuer Stimme.
- **H4 Lags:**
  - Die Lebensadern der Wegprüfung rechnen nicht mehr in jedem Bild, sondern eine Richtung je 0,25 s (`KernZugang.LebensaderTakt`).
  - In der Welle rechnen sie nur noch in Endlos und Bohrung.
  - Kostenzeile „Lebensadern: … ms“ je Wellenstart.
  - Noch nicht gemessen: die Kosten des Netzumbaus je gesetzter Sperrfläche (N1) und die bis zu 400 Leuchtringe der Wegvorschau.
- **H5 Shop:**
  - Sechs Bereiche als Hauptkategorien: Türme, Verteidigung, Produktion, Strom, Logistik, Pionier und Kern. Vorher gab es eine Kategorie „FICSIT-Abwehr“ mit allen 47 Posten.
  - Darin die Unterkategorien Kontingente, Ausrüstung, Kern und Festplatten (`Scripts/create_shop.py`).
  - Alte Posten gesichert in `E:\Modding\Backup\Shop_vor_1122_Bereiche`.
- **H7 Frontabstand:** Die Klippenprobe verlegt einen Frontstart nie näher als `Gegner.FrontMinAbstand` (150 m) an den Kern. Vorher zogen sechs Verlegungen Front 1 von 270 m auf 85 m.

**Warum:** Marius' Handspiel am 22.11. (Befunde H1 bis H7). H6 (Forschungsbaum) läuft als Recherche-Auftrag A6, siehe `Notizen/Auftraege-0.13/A6-Forschungsbaum.md`.

**Changelog EN:** The unstable ground around the Core is now 60 m. The MAM unlocks after wave 2, the core platform cannot be dismantled while the site is still being prepared, the shop is split into six sections, waves no longer spawn too close to the Core, and the build-time path check runs less often (smoother building).

**Changelog DE:** Der instabile Boden um den Kern reicht jetzt 60 m. Das MAM kommt nach Welle 2, die Kernplattform lässt sich auch während der Vorbereitung nicht abreißen, der Shop ist in sechs Bereiche geteilt, Wellen erscheinen nicht mehr zu nah am Kern, und die Wegprüfung beim Bauen rechnet seltener (flüssigeres Bauen).

**Nachweis (22.11. nachmittags):** Paket `Handspiel-1122` (`2026-11-22_1441`), 9 von 10 Plänen grün.
- `KZ-1` hatte einen einmaligen Ausreißer: Der Wandring mit Lücke galt als abgeriegelt. Allein lief `KZ-1` grün (`_1445`), in allen Läufen davor auch. Beobachten.
- H1 ist nicht automatisch prüfbar, weil der Selbsttest erst nach dem Einsatzstart beginnt. Marius sollte es im Handspiel prüfen.
- H2: Das MAM ist vor Welle 2 gesperrt (Kontingent), das Rezept kommt erst danach („MAM freigegeben nach Welle …“).
- H3: Konstruktor bei 59 m „Boden“, bei 61 m erlaubt, 63 Ringsignale.
- H4: Lebensadern in der Bauphase zusammen 124 ms, im Mittel 0,25 ms je Weg, längster 0,64 ms. Vorher lief jedes Bild, auch in der Welle. Der schlechteste 10-s-Abschnitt im Paket lag bei 26 Bilder/s. Ob die Lags weg sind, muss Marius im Handspiel bestätigen.
- H5: Foto `Shop_offen` zeigt sechs Reiter: Türme, Verteidigung, Produktion, Strom, Logistik, Pionier und Kern.
- H7: Welle 1 aus 236 m (vorher 85 m), nur noch eine Verlegung.
