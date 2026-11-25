---
titel: ADA spricht die Einstiegs-Aufgaben MAM und AWESOME-Shop
typ: neu
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: EinsatzPhase, Shop
experiment:
---

**Was:** Die beiden Einstiegs-Aufgaben (nach Welle 1 das MAM, nach Welle 2 den AWESOME-Shop bauen) sagt ADA zusätzlich mit Stimme an, Deutsch und Englisch, über dieselbe Warteschlange wie die Gegner-Ansagen (`TDAda`, Schlüssel `AufgabeMam`, `AufgabeShop`). Jede Ansage einmal je Kern (`mAdaGesagt`, im Spielstand), Schalter `ADA.Ansagen` wie bisher.

**Warum:** Fahrplan 0.12 E5: Einstieg Kern → MAM → Shop mit ADA-Sätzen und Ton (Marius 14.11.). Bisher nur Chat und HUD-Zeile.

**Nachweis:** Logzeilen „ADA-Ansage AufgabeMam (Stimme …)“ und „ADA-Ansage AufgabeShop (Stimme …)“ in den Plänen `EinsatzPhase` bzw. `Shop`. Ton von Hand: Marius.

**Changelog EN:** ADA now voices the two starter tasks (build the MAM, build the AWESOME Shop).

**Changelog DE:** ADA spricht die beiden Einstiegs-Aufgaben (MAM bauen, AWESOME-Shop bauen).
