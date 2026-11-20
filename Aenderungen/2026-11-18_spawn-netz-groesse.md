---
titel: Spawnprobe im Netz der Kreaturgroesse - Versuch gegen festgefahrene Stinger (B16), aus
typ: intern
modi: einsatz, endlos, abwehr
sichtbar: nein
doku: ja
simulation: ja
tests: Stinger-NW
---

**Befund (B16, 9 Faelle 17./18.11., Diagnose in G3 18.11.):** grosse Stinger (Kapsel 84/128, Agent-Radius 120) am Startpunkt der Front
Nordwesten (Fels-S8) ohne Fortschritt. Diagnose: freier Platz ja (0 cm), nach Umschalten auf Walking sofort wieder `NavWalking`,
Netz `FGAlphaNavMesh`, darunter Netz. Das Spiel haelt diese Stinger im NavWalking des Alpha-Netzes.

**Ursache (Hypothese, Nachweis ueber den Testlauf):** `SpawneGruppe` prueft den Spawnpunkt (erreichbar, voller Weg zur Front) nur
im Standardnetz. Fuer den grossen Stinger liegt der Punkt auf einer Insel des Alpha-Netzes; der Weg des Gegnerhirns faellt dann aufs
Standardnetz zurueck, dem er im NavWalking nicht folgen kann.

**Was:** `SpawnNetzFuer` waehlt das Netz zur Groesse der Klasse (Kapsel des Standardobjekts, dieselben Regeln wie
`UTDGegnerHirn::BerechneWeg`: nur Netze groesser als das Standardnetz). Die Spawnprobe sucht zuerst dort, ohne Treffer wie bisher im
Standardnetz. Log: `Spawnprobe: <Klasse> xN im Netz der Groesse <Netz>: a Punkte dort, b im Standardnetz`. Schalter
`Gegner.SpawnEigenesNetz` (1).

**Nachweis 1 (12:00):** Probe griff fuer Alpha- und Klippen-Hogs, nicht fuer Stinger (Kapsel 84 -> Standardnetz); Lauf 2 wieder ein Stinger festgefahren.
**Nachweis 2 (12:07, Agenteneigenschaften der Bewegung der Klasse):** 0 Versetzungen in zwei Laeufen, aber wieder keine Spawnprobe-Zeile fuer
Stinger – die Klasse liefert vor dem Spawn nicht die echte Groesse (120 entsteht erst im Spiel). Damit nicht belegt.
**Dritter Ansatz:** nach dem Spawn mit den echten Agenteneigenschaften pruefen (`NetzFuerAgent`): voller Weg im eigenen Netz zur Front?
Sonst sofort auf einen Punkt dieses Netzes mit vollem Weg setzen. Log: `Spawnprobe nach dem Spawn: <Klasse> xN, a geprueft, b umgesetzt, c ohne vollen Weg`.
**Nachweis 3 (12:19, Bericht `Testberichte/2026-11-18_1219_Stinger-Nachweis.md`):** Die Pruefung lief fuer Stinger (242 geprueft), alle
Spawnpunkte hatten im Alpha-Netz einen vollen Weg zur Front (0 umgesetzt) – trotzdem wieder ein Stinger im NavWalking festgefahren.
**Hypothese „Insel im Alpha-Netz“ widerlegt.** Ursache weiter offen (NavWalking trotz Umschalten, Tempo voll, kein Fortschritt).

**Entscheidung nach drei Anlaeufen (Fahrplan-Regel 4):** B16 bleibt bekannter Fehler fuer 0.11. Schalter `Gegner.SpawnEigenesNetz` steht auf **0**
(Verhalten wie in G2/G3); Code und Stinger-Diagnose bleiben fuer die naechste Untersuchung. Naechste Spur: wer setzt NavWalking zurueck
(`UFGCreatureMovementComponent::OnMovementModeChanged`), und warum bewegt NavWalking mit vollem Weg nicht.

**Simulation:** nicht betroffen.
