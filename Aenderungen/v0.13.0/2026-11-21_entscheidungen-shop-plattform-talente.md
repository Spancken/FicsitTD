---
titel: Spiel-Artikel im Shop gesperrt, Kernplattform nicht abreißbar, Talentpunkte je Bosswelle
typ: geaendert
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: Sperren-1121, EinsatzPhase
experiment:
---

**Was:**
- **Shop (A5 Nr. 1):** Bei aktivem Einsatz oder Endlos blendet der AWESOME-Shop alle Artikel des Spiels aus und lehnt ihren Kauf ab. Drei Hooks: `GetVisibleSchematicCategoryData`, `GetAllVisibleSchematicsOfType`, `PurchaseResourceSinkSchematics` (`TDShop::RichteSperreEin`). Schalter `Einsatz.Shop.SpielArtikelSperren`.
- **Kernplattform (A5 Nr. 6):** Fundamente und Rampen bis 36 m um den Kern lassen sich im Einsatz nicht abreißen. Hook auf die private Prüfung der Bau-Pistole (`UFGBuildGunStateDismantle::CanDismantleActor`, Friend in `Config/AccessTransformers.ini`) und Hinweis „Die Kernplattform lässt sich im Einsatz nicht abreißen“. Schalter `Einsatz.Plattform.AbrissSperren`, Radius `Einsatz.Plattform.Radius`.
- **Talentpunkte (C10):** Jede geschaffte Bosswelle gibt einen Talentpunkt (`Pionier.PunkteJeBoss`), dazu die Bohrung; der Rang gibt nur noch die Boni. `Pionier.TalenteAusRang` = 1 stellt die alte Regel her. Bereits gelernte Talente bleiben, neue Punkte kommen erst wieder, wenn die Bosspunkte die vergebenen übersteigen.

**Warum:** Entscheidungen Marius 21.11. (Memory `ficsittd-entscheidungen-2026-11-21`). Fenster 1 hatte gezeigt: Der Shop verkauft im Einsatz Fundamente und Wände (T1-3), ohne Rampen ist der Kern unerreichbar (T1-2), und Talentpunkte kamen durch den hohen Schadensanteil in frühen Wellen sehr schnell (C10).

**Nachweis:** Plan `Sperren-1121` (Hooks eingerichtet, Prüfung `PlattformAbriss`, Foto des offenen Shops ohne Spiel-Artikel, Logzeile „Artikel des Spiels im Einsatz ausgeblendet“), Plan `EinsatzPhase` (Logzeile „Pionier: Bosswelle 10 geschafft, +1 Talentpunkt“). Von Hand offen: Abriss einer Rampe mit der Bau-Pistole versuchen.

**Changelog EN:** During operations the AWESOME Shop no longer sells the base game's items (foundations, walls etc.), the Core platform can't be dismantled, and Talent Points now come from cleared boss waves (one each) and drilling instead of every rank.

**Changelog DE:** Im Einsatz verkauft der AWESOME-Shop keine Artikel des Grundspiels mehr (Fundamente, Wände usw.), die Kernplattform lässt sich nicht abreißen, und Talentpunkte gibt es für jede geschaffte Bosswelle (je einen) und die Bohrung statt für jeden Rang.
