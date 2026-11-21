---
titel: Turmnamen in Schadensleiste und Statistik in der Spielsprache
typ: behoben
modi: alle
sichtbar: ja
doku: nicht-noetig
simulation: nicht-noetig
tests: Schadensleiste
experiment:
---

**Was:** Die Wellenleiste („Schaden dieser Welle“) und der Statistik-Reiter im Kern-Menü (Munition, Wellen im Detail) zeigen den Anzeigenamen der Turmart statt des internen Schlüssels. Auf Englisch steht dort jetzt „Bolt Turret“ statt „Bolzenturm“, auf Deutsch „Störfeld“, „Singularitätswerfer“ und „Ölschlick-Feld“ statt „Stoerfeld“, „Singularitaet“ und „Oelschlick“. Dieselbe Korrektur gilt für den Kopf des Turmfensters und die Chat-Zeilen „Beste Türme“, „ohne Munition“, „ohne Strom“.

**Warum:** Fund nach dem Release 0.11.0 (Befund C1 im Fahrplan 0.12). Ursache: Die Statistik führt die Turmart unter dem deutschen Schlüssel `mTurmName`, und drei Stellen in `TDKernTerminal.cpp` gaben diesen Schlüssel direkt aus. Dazu gab es zwei Namenstabellen: `TDTurmAnzeige` in `TDTurretBase.cpp` kannte den Drohnenplatz nicht und ließ deutsche Schlüssel ohne Umlaut durch. Jetzt gibt es eine Tabelle (`UTDEinsatzSubsystem::Anzeige`), `TDTurmAnzeige` ruft sie auf. Nachweis über die Logzeile „Wellenleiste Schaden en: Bolt Turret (Schluessel Bolzenturm)“ im Plan `Schadensleiste` (Paket `ReleaseEn`).

**Changelog EN:** Turret names in the wave damage bar and the statistics tab now follow the game language (the English bar showed "Bolzenturm").

**Changelog DE:** Turmnamen in der Schadensleiste und im Statistik-Reiter folgen der Spielsprache (englisch stand dort „Bolzenturm“), deutsche Namen mit Umlauten.
