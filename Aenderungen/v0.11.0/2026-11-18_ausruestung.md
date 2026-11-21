---
titel: Klingenlaeufer im Startkit, Jetpack und Hover-Pack im Laden (B23)
typ: geaendert
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: ja
tests: Ausruestung
---

**Befund/Wunsch (Handspiel Marius 18.11.):** „ggf. macht es Sinn, dem Spieler von Anfang an die Hosen zu geben, um etwas schneller zu sein,
das wirkt am Anfang sehr langsam.“ – Entscheidung: ins Startkit. Dazu: „zum Kaufen spaeter die Duesentriebdinger erst, wenn Oel erforscht
ist, und dann die Stromgleiter.“

**Was (`TDEinsatz.cpp`):**
- Klingenlaeufer in `StartRessourcen` (Startkit von Einsatz und Endlos, einmal je Einsatz); im Laden bleiben sie als Ersatz.
- Laden: **Jetpack** (16 Coupons, einmal) ab dem Rezept der Raffinerie (Forschung Raffinerie; das Jetpack fliegt mit Treibstoff),
  **Hover-Pack** (24 Coupons, einmal) ab dem Fertiger (Forschung Schwerindustrie). Balance-Schluessel `Einsatz.Laden.Jetpack.*`,
  `Einsatz.Laden.Hover-Pack.*`.

**Stand:** gebaut; Plan `Ausruestung` (Kauf vor/nach der Forschung) und Startkit im Spiel offen. Pfade der Gegenstaende
(`JetPack/BP_EquipmentDescriptorJetPack`, `HoverPack/BP_EquipmentDescriptorHoverPack`) nach dem Muster der Spiel-Assets, im Spiel pruefen.

**Simulation:** nicht betroffen.

**Changelog EN:** You start with Blade Runners; the Jetpack (after Refinery research) and the Hover Pack (after Heavy Industry) are in the shop.

**Changelog DE:** Klingenläufer gibt es von Anfang an; Jetpack (nach der Forschung Raffinerie) und Hover-Pack (nach Schwerindustrie) gibt es im Laden.
