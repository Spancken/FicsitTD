---
titel: Testfenster 1 vorbereitet: Rezeptprobe, Shop-Prototyp, Messungen zum Kern-Zugang
typ: intern
modi: alle
sichtbar: nein
doku: nicht-noetig
simulation: ja
tests: Rezeptprobe, ShopPrototyp, S0-Messung
experiment: ShopPrototyp
---

**Was:**
- **Rezeptprobe (Fund F1 aus dem Querschnitt-Bericht A5):** neue Prüfung `BauteilRezepte` im Selbsttest. Sie zählt, welche Bauteil-Rezepte des Spiels (Fundamente, Rampen, Wände, Laufstege, Säulen) der Spieler hat, und schreibt die Liste ins Log („Rezeptprobe: …“). Plan `Rezeptprobe` im Einsatz, dieselbe Prüfung im Plan `Endlos-Laden`.
- **Shop-Prototyp (Etappe E1), hinter `Experiment.ShopPrototyp`, Standard 0:** ein Posten `Shop_TD_Bolzenturm` für den AWESOME-Shop (`Scripts\create_shop_prototyp.py`), Freischaltung `UTDUnlockLaden` (beliebig oft kaufbar), Sichtbarkeit über `UTDExperimentAbhaengigkeit`. Sechs Hooks auf Kauf, Kostenabfrage und Shop-Liste schreiben nur Logzeilen („ShopProto: …“) und werden erst eingerichtet, wenn der Schalter an ist. Nach einem Kauf setzt der Prototyp den nächsten Preis ins CDO (4, 5, 6, 8 nach der Laden-Formel). Er vergibt kein Kontingent und speichert nichts. Neue Schritte im Selbsttest: `shopkauf`, `oeffne: Shop`, bei `baue` das Feld `baurezept`.
- **Spike S0 Kern-Zugang, nur die Messungen:** Entwicklerbefehl `/td s0 strahl | baue | liste | blick | probe | abriss | abrissalle | nach` (`TDSpikeS0.cpp`). Misst, wem die Kollision der Verteidigungsmauer gehört und ob sie ihren Kollisionstopf mit Spielwänden teilt (a0), löst Treffer auf Lightweight-Bauteile auf (b) und entfernt einzelne Bauteile per Code mit Nachmessung nach Speichern und Laden (c). Kein Umbau der Mauer, keine Spiellogik.

**Warum:** Fahrplan 0.12, Etappe E1. Die drei Weichen (kann man im Einsatz überhaupt mit Spielbauten abriegeln, trägt der Shop wiederholte Käufe mit steigendem Preis, lassen sich fremde Bauteile sauber entfernen) werden in einem Testfenster geklärt, bevor die große Arbeit beginnt.
