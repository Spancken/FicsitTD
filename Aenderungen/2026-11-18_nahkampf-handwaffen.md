---
titel: Xeno-Zapper und Xeno-Basher als schwache Nahkampf-Handwaffen (B22)
typ: behoben
modi: einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: Grundtest
---

**Befund (Handspiel Marius 18.11.):** „Mein Schaden in Welle 1 mit dem Taser wurde nicht gewertet.“ Log: Treffer durch
`Equip_ShockShank_C` erkannt, aber „Waffe -1“ – `UTDPionier::Waffe` kannte nur Armierpistole, Gewehr, Nobelisk. Ohne Zuordnung kein
Turmwert und kein Wachstum mit den Wellen, gegen Wellengegner fast wirkungslos (W1: 20 Schaden, 4,8 %).

**Entscheidung Marius 18.11.:** schwaecher, nur Notwehr.

**Was (`TDPionier.cpp`):** neue Waffe `Nahkampf` (3) fuer ShockShank (Xeno-Zapper) und StunSpear (Xeno-Basher): Stromschaden, Tabellenwert =
Armierungsbolzen x `Pionier.Nahkampf.Anteil` (0,35), Spielwert 10 bzw. 30, waechst mit den Wellen wie die anderen Handwaffen;
Resistenzen gelten. Durchschlag (Schuetze 3) nur noch fuer Armierpistole und Gewehr.

**Stand:** gebaut; im Spiel nicht geprueft (Log-Zeile „Spielertreffer durch Equip_ShockShank_C -> Waffe 3“ und die Rechnung je Treffer).

**Simulation:** nicht betroffen.

**Changelog EN:** Xeno-Zapper and Xeno-Basher now count as hand weapons (self-defense: electric damage at about a third of a rebar bolt, growing with the waves).

**Changelog DE:** Xeno-Zapper und Xeno-Basher zählen jetzt als Handwaffen (Notwehr: Stromschaden etwa ein Drittel eines Armierungsbolzens, wächst mit den Wellen).
