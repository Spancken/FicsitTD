---
titel: Gegencheck 0.13 – acht Fehler behoben (Gegenseite, Wegprüfung, Ladeprüfung, Englisch, Talente, Festplatte abgeben)
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: A6-Forschung, A6-Abwehr, A6-Abgabe-Endlos, KZ-1-Einsatz, KZ-3-Ring, KZ-4-Wegpruefung, Shop
experiment:
---

**Was** (Befunde F1–F8 aus `Notizen/Gegencheck-0.13-Bericht.md`):
- **F1 Gegenseite im Freien Spiel nicht mehr grau:** `UTDAbzweigungAbhaengigkeit::AreDependenciesMet` lässt im Abwehr-Modus alles durch, wie schon die Wellen-Abhängigkeit. Der Selbsttest-Schritt `pruefe: KnotenFrei` prüft die Abhängigkeiten so, wie das MAM sie prüft.
- **F2 Texte zur Gegenseite:**
  - `TDForschung::Zustand` meldet im Abwehr-Modus „Möglich“ ohne Wellen-Sperre.
  - Die Statuskarte zeigt Coupons und „Erforschbar: doppelter Preis“.
  - Die Gabelbeschreibung sagt nicht mehr fest „für diesen Einsatz“.
- **F3 Wegprüfung bis zur Front:**
  - Die Lebensadern beginnen am Startabstand der Fronten (`TDLAStartAbstand`) statt fest bei 120 m. Bis 300 m prüfen sie mit (`KernZugang.LebensaderMax`).
  - Das Hologramm wird an seinem Umriss geprüft (`HologrammKasten`), nicht nur an der Mitte.
  - Der Takt bleibt fest bei 0,25 s je Ader. Ein ruhigerer Takt ohne Baupistole (2 s) ließ die Adern nach einem Umbau bis 32 s veralten, KZ-4 war damit rot (Lauf `2026-11-23_0854`). Eine Wegsuche kostet gemessen im Mittel 0,2 ms, höchstens 0,7 ms.
  - Von der Front aus sind auf Grass Fields 7 von 16 Adern gültig, vorher waren es 10 von 16 ab 120 m. Die Prüfung braucht mindestens 3.
- **F4 Ladeprüfung:** 15 s nach dem Laden prüft der Mod einmal den Kern-Zugang (`KernZugang.LadePruefungNach`). Ein abgeriegelter Bestand meldet sich sofort und nicht erst beim Wellenstart.
- **F5 Englisch im Kern-Menü, Reiter Forschung:**
  - Baum, Name, Text, „Im Laden“ und „Gut gegen“ laufen über `TDForschung::Anzeige`.
  - Die EN-Texte erzeugt `Scripts/ergaenze_en_forschung.py`.
- **F6 Industrie-Liste im Freien Spiel:** `TDIndustrieKnoten` kennt jetzt auch Band Mk4/Mk5 und Miner Mk2/Mk3.
- **F7 Talente alter Spielstände:** Einmalig werden Talentstufen zurückgesetzt, die über den Bosspunkten liegen, mit Chatmeldung (`PruefeAlteTalente`).
- **F8 Festplatte abgeben ohne Coupons:**
  - Fehlt der angezeigte Mindestpreis, leiht der Shop ihn vor (`FTDShopKauf::Vorschuss`) und verrechnet ihn danach. Scheitert der Kauf, nimmt er den Vorschuss zurück.
  - Die Abgabe ist auf den Bestand an Festplatten begrenzt.
- **Dazu:**
  - Das MAM zeigt vor Welle 2 (`Einsatz.Mam.AbWelle`) im Hologramm einen eigenen Grund „MamFrueh“ (`UTDCDMamFrueh`).
  - Die Ringsignale schweigen in der freien Welt nach dem Einsatz.
  - Der Selbsttest-Schritt `gib` mit negativer Anzahl nimmt weg.
  - Selbsttest auf Englisch: `antwortEnthaeltEn` und `sucheEn` gelten bei englischer Spielsprache (Chattexte), die Pläne Shop und A6-Forschung nutzen sie.
  - Die Prüfstufen `Release` und `ReleaseEn` enthalten jetzt die 0.13-Pläne.

**Warum:** Der Gegencheck vom 23.11. hat die Befunde bestätigt. Marius: „JA“ zu allen Fixes.

**Noch offen:** Marius entscheidet die Bohrung-Regel: Prüfung nur beim Start, oder wie jetzt im Code auch vor jeder Bohrwelle und im Hologramm.

**Changelog EN:** Fixes: the opposite fork side in Free Play with Defense is no longer shown locked and shows its coupon price; path check now covers the whole way to the fronts and the full hologram outline; a blocked core is reported shortly after loading; research tab of the core menu in English; missing industry nodes in Free Play; talents of old saves are reset once to the new boss points; handing in a hard drive works without coupons.

**Changelog DE:** Fehlerbehebungen: Die Gegenseite einer Gabel im Freien Spiel mit Abwehr ist nicht mehr gesperrt und zeigt ihren Coupon-Preis. Die Wegprüfung reicht bis zu den Fronten und prüft den ganzen Umriss des Hologramms. Ein abgeriegelter Kern meldet sich kurz nach dem Laden. Der Forschungsreiter im Kern-Menü ist auf Englisch. Im Freien Spiel fehlen keine Industrie-Knoten mehr. Talente alter Spielstände werden einmal auf die neuen Bosspunkte gesetzt. Festplatten lassen sich auch ohne Coupons abgeben.

**Nachweis (23.11. vormittags, Build mit allen Fixes):**
- Paket `Release` (27 Pläne, `2026-11-23_0854`): 26 grün. KZ-4 war rot durch den ruhigeren Ader-Takt, siehe oben.
- Nach der Rücknahme sind grün:
  - KZ-4 (`_0901`): 7 von 7 Adern geschnitten, Hologramm rot
  - KZ-1 (`_0932`), KZ-2 (`_0935`), KZ-6 (`_0938`)
- Grün waren schon im Release-Lauf:
  - A6-Forschung: Vorschuss-Logzeile, danach genau 12 Coupons
  - A6-Abwehr: `KnotenFrei` und Kettenblitz „Möglich“
  - A6-Abgabe-Endlos, Shop, Einsatz-Laden, Bohrung
- Paket `ReleaseEn` (`_0915`): 7 von 9 grün. Shop und A6-Forschung suchten deutsche Chattexte, der Mod antwortete richtig englisch („Bought: Bolt Turret +1 for 4 Coupons“).
- Mit `sucheEn`/`antwortEnthaeltEn` sind grün:
  - Shop EN (`_0921`), A6-Forschung EN (`_0924`)
  - Gegenprobe DE: Shop (`_0925`), A6-Forschung (`_0928`)
- Fehlliste Übersetzung: 21 Einträge, kein deutscher mehr. Vorher standen dort die Forschungsreiter-Texte, F5 ist damit belegt.
