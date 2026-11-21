---
titel: Diagnose fuer Stinger, die im NavWalking festhaengen
typ: intern
modi: einsatz, endlos, abwehr
sichtbar: nein
doku: ja
simulation: ja
tests: P12-E-R3-Gewehr-A
---

**Befund (17./18.11., 9 Faelle in 5 Laeufen):** immer `Char_Stinger_C` (Kapsel Radius 84, Halbhoehe 128), 20-30 m hinter dem
Startpunkt der Front Nordwesten auf Fels-S8, seit dem Erscheinen ohne Fortschritt: `MOVE_NavWalking`, Tempo 850/850, Boden nicht
begehbar, Kapsel steckt nicht. Die Pflege im Gegnerhirn schaltet jede Sekunde auf Walking, trotzdem stehen sie im NavWalking.

**Hypothese (nicht belegt):** Die Engine verlaesst NavWalking nur, wenn `FindTeleportSpot` einen freien Platz findet
(`TryToLeaveNavWalking`), sonst schaltet sie zurueck. Alternativ fehlt unter ihnen das Netz fuer ihre Groesse.

**Was:** Diagnose beim Festfahren im NavWalking: freier Platz ja/nein, Modus nach dem Umschalten, Agent-Groesse, zustaendiges
Netz, Netz darunter. Kein Eingriff ins Verhalten.

**Simulation:** nicht betroffen.

**Stand:** gebaut, Daten aus dem naechsten Lauf mit Stingern an der Front Nordwesten offen.
