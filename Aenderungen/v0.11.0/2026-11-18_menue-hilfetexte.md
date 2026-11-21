---
titel: Kurze Hilfetexte im Menue Neues Spiel (Einstellungen waren nur ein schmaler Streifen)
typ: behoben
modi: aus, einsatz, endlos, abwehr
sichtbar: ja
doku: ja
simulation: ja
tests: Grundtest
---

**Befund (Marius 18.11., Screenshot):** In den Sitzungseinstellungen, Kategorie FICSIT Tower Defense, nahm der Hilfetext zu „Spielmodus“
(alle vier Modi ausfuehrlich, vier Zeilen ueber die ganze Breite) fast die ganze Hoehe; die Einstellungen „Spielmodus“ und „Stufe“
waren nur noch ein schmaler Streifen.

**Was (`TDGameInstanceModule.cpp`):** Hilfetexte von „Spielmodus“ und „Stufe“ auf ein bis zwei Saetze gekuerzt, wie die Texte des Spiels.
Die Auswahl nennt jeden Modus schon mit Kurzbeschreibung, Einzelheiten stehen im Handbuch (Verweis im Text).

**Stand:** gebaut; im Menue noch nicht angesehen (Marius beim Handspiel).

**Simulation:** nicht betroffen.

**Changelog EN:** The New Game settings of FICSIT Tower Defense have short help texts, so the settings are easy to read again.

**Changelog DE:** Die Einstellungen von FICSIT Tower Defense im Menü „Neues Spiel“ haben kurze Hilfetexte und sind wieder gut lesbar.
