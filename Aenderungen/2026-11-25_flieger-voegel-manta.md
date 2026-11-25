---
titel: Vogelschwarm und Mantas statt fliegender Krabben
typ: neu
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: ja
tests: Flieger, Flieger-Fotos
experiment:
---

**Was:**
- **Neue Luftgegner** (`ATDFlieger`, `TDFlieger.h/.cpp`): Modelle und Animationen kommen aus dem Spiel und werden zur Laufzeit geladen. Der Mod liefert davon nichts mit.
  - **Vogelschwarm** ab Welle 43 (`Flieger.AbWelle`), die roten Vögel des Spiels (`BirdFlying`, Flügelschlag im VAT-Material).
    - Anzahl wie bisher bei den Krabben: 6 + 0,5 je Welle, × Budget der Stufe, höchstens 24.
    - Sie fliegen im Schwarm mit Schlingern (14 m/s) und halten mindestens 5 m über Grund. Mauern überfliegen sie.
    - Am Kern richten sie 15 × Wellenwachstum Schaden an und verschwinden. Ein Treffer tötet einen Vogel (`Flieger.Vogel.LP` 1).
  - **Mantas** ab Welle 45 (`Flieger.Manta.AbWelle`), das Original-Modell des Spiels mit Fluganimation, auf 35 % verkleinert.
    - Anzahl: 1 + 0,25 je Welle, höchstens 6.
    - Leben: 1500 × LP der Stufe, wächst ab W45 wie bei den Bodengegnern.
    - Sie fliegen in 18 m Höhe an, kreisen in 30 m Abstand um den Kern und spucken alle 3 s Säure auf ihn (Spucker-Geschoss des Spiels, 10 × Wellenwachstum Schaden).
    - Abgeschossen trudeln sie zu Boden.
- **Die fliegenden Krabben (`BP_FlyingBabyCrab`) sind raus.**
- **Himmelsturm:** Er sucht und trifft jetzt Vögel und Mantas (`ATDFlieger`), Bodentürme weiterhin nicht. Spielerwaffen treffen über `TakeDamage`, das Kollisionsprofil blockt alles, beeinflusst aber kein Netz.
- **Ansagen:** „FLIEGER!“ für den Vogelschwarm, „MANTAS!“ beim ersten Manta, DE/EN.
- **Wellenmarker:** Die Kartenmarkierung zeigt „FLIEGER!“ ab der ersten Fliegerwelle.
- **Kernschaden-Messung:** Die Familien „Vogel“ und „Manta“ werden getrennt gezählt.
- **Zielwahl der Türme:** Den Eintrag „Krabben“ gibt es nicht mehr, Bodentürme zielen nie auf Flieger.
- **Simulator** (`strategien.py`): Er rechnet die Mantas mit. Der gute Spieler stellt je Manta einen Himmelsturm. Die Säure wirkt, bis die Himmelstürme die Mantas nacheinander abschießen. Das ist eine Annahme und im Spiel noch nicht nachgemessen.
- **Selbsttest** `Flieger`: Sprung auf W45, Fotos von Vogel, Manta und Kern, Prüfung der Abschüsse beider Arten. Autotest-Kamera mit den Punkten `vogel` und `manta`.

**Warum:** Marius am 25.11.: „Die Krabben würde ich eigentlich ersetzen, das sind ja keine Fluggeschöpfe, das soll was Realistisches sein.“ Auf die Frage „Was machen die Manta?“ hat er „Schießt auf den Kern“ gewählt, als Ersatz für die Krabben den Vogelschwarm.
Es gilt weiter: Gegner greifen nie den Spieler an und zerstören nie die Fabrik.

**Changelog EN:** Flyers are now real creatures: a flock of birds from wave 43 and, from wave 45, Mantas that circle the Core and spit acid at it. Ground turrets can't hit them – the Sky Tower and your hand weapons can. The flying crabs are gone.

**Changelog DE:** Flieger sind jetzt echte Flugtiere: ein Vogelschwarm ab Welle 43 und ab Welle 45 Mantas, die über dem Kern kreisen und Säure spucken. Bodentürme treffen sie nicht, der Himmelsturm und deine Handwaffen schon. Die fliegenden Krabben sind raus.

**Nachweis:** offen (Selbsttest `Flieger` nach dem Build).
