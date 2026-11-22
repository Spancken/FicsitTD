# FicsitTD: Spielregeln

**Die einzige gültige Beschreibung des Spiels.** Ab 0.77 gilt bei Widersprüchen zu Konzepten oder älteren Notizen dieses Dokument. Konzepte erklären, *warum* etwas so ist; hier steht, *wie* es ist.

- **Stand:** Mod-Version siehe `Mods\README.md`.
- **Zahlen:** Der Abschnitt „Zahlen“ wird vor jedem Build automatisch aus dem Code erzeugt (`Tests\erzeuge_spielregeln.py`). Alle anderen Abschnitte werden von Hand gepflegt und bei jeder Regeländerung angepasst.
- **Kennzeichnung:** *(geplant)* = beschlossen, noch nicht gebaut. *(ungetestet)* = gebaut, aber nicht im Spiel oder Selbsttest belegt.

---

## 1. Leitidee

> **Die Fabrik ernährt die Verteidigung, die Verteidigung schützt die Fabrik.**

- Türme brauchen echte Munition aus echten Produktionsketten. Sie kommt über Band oder Rohr. Dazu brauchen die Türme Strom.
- Türme verbrauchen echte Munition. Das Baumaterial der Kontingent-Gebäude kommt im Einsatz beim Kauf mit (Startkit und jeder Kontingentkauf liefern es ins Inventar, Entscheidung 02.11., bestätigt 16.11. bis zur Messung mit dem Fabrik-Prüfstand); die Fabrik liefert vor allem Munition, Strom und Freies-Spiel-Material. FICSIT-Coupons (vom Abwehrkern, im Code und Log weiter „EP“) und Festplatten (Forschung, im Code weiter „FP“) sind die Währungen. **Seit 0.10.0 sind beide echte Items des Spiels:** Coupons = `Desc_ResourceSinkCoupon` im Inventar (ist es voll, warten sie am Kern und kommen beim Öffnen des Kern-Menüs nach), Festplatten = `Desc_HardDrive` statt der früheren FICSIT-Kampfdaten. Alte Spielstände werden beim Laden umgestellt. Die Festplatten-Forschung des Spiels (Alternativrezepte) ist im Einsatz gesperrt. Der AWESOME-Sink ist im Einsatz gesperrt (0.84.4).
- Die Wellen werden mit der Wellennummer stärker, nicht mit der Größe der Fabrik.
- Eine Niederlage löscht nie die Fabrik.

## 2. Modi

| Modus | Inhalt |
|---|---|
| **Einsatz** (Hauptmodus, **Standard für neue Spiele** seit 18.11.; **Startfenster** nach der Landung: Spielmodus und Stufe wählen, einmal je Spiel, `ATDSpielstand::mModusGewaehlt`, Schalter `Spielmodus.Startfenster`, Befehl `/td startfenster`) | fester Kernplatz je Biom, Kontingente, Coupons/Festplatten, Phasen mit Bossen |
| **Endlos-Wellen** *(0.11-dev, Spielmodus 2, Marius 15.11., nach Gegencheck)* | wie Einsatz (Kernplatz, Kontingente, Coupons/Festplatten, MAM), aber Wellen **auf Zeit**, **ohne Ende**, immer aus **einer** Front. **Zeitplan je Stufe** (erste Welle / Abstand): Erkundung 10 min / 240 s, Normal 8 min / 220 s, Hart 7 min / 165 s, Albtraum 6 min / 150 s (`Endlos.<Stufe>.ErsteWelle`, `.Abstand`). Der Abstand zählt **ab Wellenbeginn** und sinkt je Welle um 1,5 s bis 90 s (`Endlos.AbstandJeWelle`, `Endlos.AbstandMin`). Budget je Welle × 0,9 (`Endlos.BudgetFaktor`), ab Welle 50 je Welle 3 % mehr Gegnerleben, zinsartig (`Endlos.LPWachstum`, `Endlos.LPWachstumAbWelle`) – sonst gibt es nach W50 keinen Druck mehr (16.11., Simulation, Fahrplan P2). **Ab Welle 60** wächst zusätzlich die Gegnerzahl je Welle um 2 bis 240 (`Endlos.MehrGegnerJeWelle`, `Endlos.MaxGesamt`) und der Elite-Anteil um 1 Prozentpunkt je Welle bis 60 % (`Endlos.EliteMax`) *(17.11., Marius)*. Ist die Welle früher erledigt, bleibt die Restzeit als Pause, mindestens 30 s (`Endlos.MinPause`). Läuft die Zeit ab, kommt die nächste Welle **obendrauf** (Reste bleiben, 5 s `Endlos.UeberlaufPause`), anhalten geht nicht. **Gegnerstrom:** 2 Gegner je Sekunde statt Pakete von 5 alle 0,25 s, große Wellen schneller, damit der Nachschub nach der Hälfte des Abstands draußen ist (`Endlos.NachschubPaket`, `.NachschubTakt`, `.StromAnteil`). **Keine Bausperre** während der Welle. **Vorziehen** („Welle vorziehen“) gibt 1 Coupon je 10 s Restzeit (`Endlos.VorziehSekundenJeCoupon`). **Nach jeder Bosswelle** mindestens 5 min Pause (`Endlos.BossPause`) und eine Bonuswahl im Kern-Menü: +80 Coupons, Kern-LP dauerhaft +10 % oder +5 Festplatten (`Endlos.Bonus.*`, `Einsatz.Bonus.KernMax`); nicht abgeholte Boni bleiben offen, gewählt wird zwischen den Wellen. Eine **Bosswelle im Überlauf** (Boss lebt noch) bringt keine Pause und keinen Bonus; noch nicht losgeschickter Nachschub verfällt nicht, er kommt vor der nächsten Welle, und die Uhr läuft auch, solange Nachschub aussteht. *(Nachprüfung 16.11., Marius)* **Laden und Turmlevel** gehen auch in der Welle (keine Bausperre). **Keine Spielstände je Welle** (sonst ließe sich jede Welle über das Lademenü wiederholen). *(Nachprüfung 16.11., Marius)* **Kernverlust beendet den Lauf** mit Bestwert je Stufe (`Saved\FicsitTD\Bestwerte.json`; ohne Wertung bei Eingriffen oder erweiterten Spieleinstellungen), kein „Welle wiederholen“. Chat: eine Sammelzeile je Welle, Countdown-Marken nur 60 und 10 s. Der Modus wird am Kern gemerkt (`mLaufEndlos`); `/td modus einsatz` ändert ein Endlos-Spiel nicht. |
| **Aus** *(Spielmodus 0, ab 0.11-dev)* | **Schutz bestehender Welten (18.11.):** Ein geladener Spielstand mit Bauten des Spielers, aber ohne Abwehrkern (z. B. eine Fabrikwelt ohne gespeicherten Modus) wird auf „Aus“ gesetzt, bevor ein Kernplatz entsteht; Hinweis im Chat (`Spielmodus.BestehendeWeltAus`). Normales Satisfactory: keine TD-Rezepte, TD-Forschungsbäume gesperrt (`UTDModusAbhaengigkeit`), kein Kernplatz, kein Knopf „Einsatz starten“. Nur der Spielmodus zählt, ein gebauter Kern nicht (Marius 15.11.). Alte Stände, deren Einsatz ohne Spielmodus lief, stellen ihn mit `/td modus einsatz` um (bleibt beim Speichern erhalten, belegt im Plan `ModusAus`). |
| **Kern-Verteidigung** | freies Feld, Kern beliebig, ohne Kontingente („Sandkasten“ nur noch hierfür, Tests; ab 0.11-dev nur im Entwicklermodus von Hand) |
| **Freies Spiel mit Abwehr** *(0.12-dev, Spielmodus 3, Marius 15.11., Gegencheck Normalspiel)* | normales Satisfactory mit HUB und Meilensteinen, dazu Türme, Mauern und Abwehr-Forschung. **Leitsatz:** Die Fabrik ernährt die Verteidigung, die Verteidigung bringt Ertrag. Nichts greift an, was du nicht eingeschaltet hast. Wer verliert, verliert nur den Ertrag. **Forschung:** Ballistik, Elementar, Festung kosten Coupons statt Festplatten (10 je Festplatte, `Abwehr.CouponsJeFestplatte`); der Baum Industrie ist verborgen, seine Knoten gelten als erforscht, sobald der HUB ihre Spielrezepte freigeschaltet hat. Keine Freigabe „ab Welle“. Turmlevel mit Coupons. Kein Laden, keine Kontingente, kein Kernplatz. **Bohrung:** Abwehrkern als Tiefbohrer höchstens 30 m neben einem festen Rohstoffknoten (nur echte Knoten, keine Rohstoffbrocken *(Nachprüfung 16.11., Marius)*); vor dem Start Platzprüfung im Klartext (Knoten, begehbare Wege). *(0.13-dev, Selbsttest 22.11.)* Riegeln Wände oder Fundamente den Bohrer ab, startet die Bohrung nicht; die Meldung nennt Bau und Stelle, die Karte markiert sie. Bestehende Fabrik bleibt stehen. Stufen Leicht (Wellen 1–4, 8000 LP, 60 Erz je Welle), Mittel (15–19, 18000 LP, 120 Erz) erst nach einem Erfolg auf Leicht, Schwer (25–30 mit Boss, 24000 LP, 200 Erz) nach Mittel. Erz × Reinheit (unrein 0,5, rein 2). Nach jeder abgewehrten Bohrwelle repariert sich der Kern um 10 % (`Bohrung.Reparatur`; 16.11., Simulation, Fahrplan P3). **Erstlohn** je Rohstoff und Stufe einmal: 2/4/6 Festplatten, auf Schwer +1 Talentpunkt; wiederholbar nur Erz und Pionier-XP (60/120/200). In der Bohrung zerstörte Bauten geben ihr Baumaterial zurück, und zu Beginn jeder Bohrwelle heilen alle Verteidigungsbauten auf 100 % wie im Einsatz *(Nachprüfung 16.11., Marius)*. Wildtiere greifen die Abwehrbauten nicht an, nur die Wellengegner der Bohrung; ein zweiter Kern ist kein Ziel. Kern-Schild und Notreparatur wirken auch in der Bohrung. Bänder rüsten hier nicht automatisch auf (das macht der HUB). Die Forschung kennt hier keine Freigabe „ab Welle“, auch nicht über die Bedingung im Knoten *(Nachprüfung 16.11., Marius)*. Abbruch jederzeit ohne Strafe, bei Kernverlust endet nur der Lauf (Kern repariert sich), nach dem Laden endet eine laufende Bohrung ohne Strafe. Speichern in der Bohrungswelle gesperrt wie im Einsatz. Alte Stände: `/td modus abwehr`. Schlüssel `Bohrung.<Stufe>.*`. |
| Belagerung, Säuberung, Munitionswellen, Fabrik-Störungen | *(abgelöst 15.11. – Bedrohung aus der Fabrik passt nicht ins gemütliche Freie Spiel; aufgehoben für einen späteren eigenen harten Modus)* |
| Mehrspieler | nicht unterstützt |

**Länge:** Ein Einsatz über den Spielmodus hat fest **50 Wellen** (Marius 16.11.: ein Lauf darf bis ins Endgame dauern). Den Einsatz von Hand („Einsatz starten“ im Kern-Menü) mit wählbarer Länge (Kurz 20, Mittel 30, Standard 50, Endlos) gibt es nur im Entwicklermodus; ohne ihn nur einmal je Kern, wenn an ihm noch kein Einsatz lief. *(0.11-dev, Gegencheck Einsatz, Marius 16.11.)* Der Singularitätswerfer (ab Welle 51) ist deshalb nur mit der Länge „Endlos“ oder im Spielmodus Endlos-Wellen erreichbar.

**Schwierigkeit:** Erkundung, Normal, Hart, Albtraum. Sitzungseinstellung **„Stufe“** (`FicsitTD.StufeInt`, unter „Spielmodus“): 0 **nach Startort** (Standard: Grasfelder Erkundung, Felsenwüste Normal, Nordwald Hart, Dünen Albtraum), 1–4 feste Stufe an jedem Startort. Gilt für Einsatz und Endlos-Wellen. Hart und Albtraum sind **ungetestet** (nur Simulation, Auswahl und Startmeldung sagen es). *(0.11-dev, Gegencheck Einsatz, Marius 16.11.)*

**Spielzeit:** Im Einsatz gibt es keinen Countdown, jede Welle startet erst auf Knopfdruck (Marius 12.11.). Die Spielzeit hängt also nur davon ab, wie lange man zwischen den Wellen baut. Gemessen ist sie nicht.

## 3. Ablauf eines Einsatzes

1. **Start:** neues Spiel, Sitzungseinstellungen, Kategorie „FICSIT Tower Defense“, **Spielmodus „Einsatz“** (seit 0.10.1 Auswahl `FicsitTD.ModusInt`: 0 Aus, 1 Einsatz, ab 0.11-dev 2 Endlos-Wellen, ab 0.12-dev 3 Freies Spiel mit Abwehr; vorher Ankreuzbox `FicsitTD.TowerDefense`, wird weiter gelesen). Der Hilfetext erklärt jeden Modus auf Deutsch oder Englisch. Weitere Modi kommen als weitere Werte dazu.
   - Der Mod wählt einen von drei festen Kernplätzen im Startbiom.
   - Dort baut er eine Plattform aus Spielfundamenten (6 × 6, 1 m erhöht, Rampenring mit Ecken) und räumt den Bewuchs in 60 m Umkreis.
   - Dann setzt er den Abwehrkern, startet den Einsatz und spielt die ADA-Begrüßung (Deutsch/Englisch).
   - *(0.12-dev, Marius 16.11.)* ADA meldet sich zu Beginn der Bauphase vor einer Welle mit neuer Gegnerart oder Boss mit einem kurzen Satz (Stimme + ADA-Zeile im Chat, je Lauf einmal, ab W60 ein kurzer Satz je Bosswelle; `TDAda.cpp`, abschaltbar `ADA.Ansagen 0`). Gilt in allen Modi mit Wellen, auch in der Bohrung.
2. **Aufbauphase:** Ohne Zeitdruck. Die Welle startet mit dem Knopf „Welle starten“ im Kern-Menü (oder `/td kern weiter`); beim Start meldet der Kern im Chat Türme ohne Munition oder Strom (die Meldung hält die Welle nicht auf). Gebaut wird mit dem Startkontingent und mit gekauften Kontingenten. Meilensteine am HUB sind im Einsatz gesperrt, neue Technik gibt es nur im MAM. Blaupausen sind im Einsatz nicht erlaubt (Marius 12.11.).
3. **Welle:**
   - Die Gegner kommen aus 1–3 Fronten und laufen auf Navigationswegen zum Kern.
   - Zu Beginn heilen alle Verteidigungsbauten auf 100 %.
   - Im Verteidigungsfeld (120 m um den Kern) ist das Bauen aller Gebäude mit Kontingent gesperrt, also auch Splitter, Verbinder, Lager, Puffer und Generatoren. Bänder, Rohre und Stromleitungen darf man weiter bauen (Marius 12.11.; Fundamente hat der Spieler im Einsatz nicht, siehe unten). Der Laden ist in der Welle zu, Turmlevel und Talente gehen.
   - Während der Welle kann nicht gespeichert werden, auch der Autosave ruht. Sonst ließe sich Beute durch Laden wiederholen.
   - Die Welle endet, wenn alle Gegner tot sind. Nachzügler werden versetzt oder entfernt.
   - **Kern-Zugang (0.13-dev, Marius 21.11., Selbsttest 22.11.)**: Vor jeder Welle prüft der Mod, ob jede aktive Front den Kern erreicht.
     - Riegeln Bauten den Kern ab, startet die Welle nicht. Das gilt im Einsatz, im Endlos und zwischen Bohrwellen.
     - Erkannt werden Wände, Fundamente, Maschinen und Türme. Maschinen und Türme bekommen dafür im Umkreis von 250 m eine unsichtbare Sperrfläche im Netz der Gegner (`Gegner.BauSperren*`, 0.13-dev N1). Die Gegner laufen um sie herum statt hinein.
     - Die Sperrstelle steht auf der Karte („Kern abgeriegelt: …“) und im Chat mit Bau, Abstand und Richtung, alle 30 s wiederholt.
     - HUD und Kern-Menü zeigen „wartet: Kern abgeriegelt“. Ist wieder ein Weg frei, startet die Welle.
     - TD-Mauern, Sperren, Tore und Zäune dürfen den Weg schließen, denn die Gegner greifen sie an.
     - Abgerissen wird nie etwas.
     - Ohne erkannten Bau (Gelände) wartet der Start höchstens 20 s (`KernZugang.*`).
   - **Instabiler Boden (0.13-dev K2, Marius 21.11.):** Im Einsatz und im Endlos lassen sich in 60 m um den Kern (Marius 22.11., vorher 40 m) keine Fabrikgebäude des Spiels bauen (Maschinen, Lager, Generatoren, MAM, Shop, HUB; `Einsatz.Ring.Radius`). Das Hologramm sagt „Boden instabil“. Bauten des Mods (Türme, Mauern, Stützbauten) und Logistik gehen. Was schon steht, bleibt stehen.
   - **Wegprüfung beim Bauen (0.13-dev K4):** Solange ein Kern läuft, ist ein Bau, der den letzten Weg der Gegner zum Kern schließen würde, im Hologramm rot („Würde den letzten Weg zum Kern schließen“). Grundlage sind 16 Wege aus 16 Richtungen (120 m). Das gilt auch für Türme. Abwehrmauern, Sperren, Tore und Zäune dürfen schließen.
   - **Klippenprobe (ab 0.11-dev):** Vor jeder Welle fährt eine Gegner-Kapsel (Radius und Halbhöhe 70 cm) jeden vollen Frontweg in 2-m-Schritten ab. Klippen mit Flächen steiler als 45 Grad, an denen sie hängen bleibt, werden vorab aus dem Navigationsnetz genommen (`Gegner.KlippenProbe*`, höchstens `Gegner.KlippenSperrenMax` = 24). Nach dem Netzumbau planen marschierende Gegner ihren Weg neu. Liegt die Hängestelle am Startpunkt einer Front, wird der Startpunkt entlang des Frontwegs hinter die Sperrfläche verlegt (`Gegner.FrontVerlegen`, 16.11.).
   - **Feste Fronten je Kernplatz (ab 0.11-dev, 18.11., Marius):** Jeder der 12 festen Kernplätze hat vier feste Fronten (Richtung und Abstand, gemessen auf Normal, Tabelle `TDFesteFronten`), in der Reihenfolge, in der sie aufgehen. Auf anderen Stufen gilt der Abstand der Stufe; hat er keinen vollen Weg, nimmt die Front den nächsten Abstand derselben Richtung. Fehlt einer festen Front noch der Weg (Netz nicht geladen), sucht die Prüfung vor der Welle neu, der Start wartet bis zu 30 s (`Gegner.FrontenWarteMax`), sonst zählt das Sicherheitsnetz „Fronten ungeprüft“. Kerne ohne festen Platz (Bohrung, Entwickler) wählen wie bisher: zuerst der längste geprüfte Weg, dann größter Winkelabstand. Schalter `Gegner.FesteFronten`.
   - **Weltobjekte am Weg (ab 0.11-dev):** Erzvorkommen, Steinknoten, Krabbennester, Nuss- und Beerenbüsche bis 400 m um den Kern stehen als Sperrfläche im Navigationsnetz, die Wege führen um sie herum. Seit 18.11. laufen Wellengegner durch diese Objekte hindurch, wenn sie deren Kante streifen. Vorher blieben sie an der Kante hängen und mussten versetzt werden (`Gegner.HindernisseDurchlaufen`).
4. **Nach der Welle:**
   - Es gibt Coupons, Festplatten und Biomasse.
   - Der Kern repariert sich (Grundreparatur, mit Forschung mehr).
   - Beute liegt am Boden oder kommt nach 60 s an den Kern.
   - Der Mod speichert einen Spielstand vor und nach jeder Welle, mit dem Namen des Spiels davor (`<Sitzung>_TD_W<n>_Start/_Ende`, seit 0.11-dev; vorher `FicsitTD_W<n>_…` für alle Spiele gemeinsam), nach dem Sieg in Welle 50 vermutlich nicht. Endlos-Modus und Bohrung speichern keine Stände je Welle. Der Stand vom Wellenstart merkt sich selbst, welche Welle er wiederholt.
   - **Chat (17.11., Marius: nur das Wichtigste):** am Wellenende eine Sammelzeile (Coupons, Festplatten, Kern, zerstörte Bauten, beste Türme, dein Anteil), Details im Kern-Menü (Statistik). Der Bauhinweis kommt nur in den ersten drei Wellen. Wichtige Warnungen (Kern unter 50 %, neue Front, Boss) und ADA bleiben eigene Zeilen (`Chat.Sammelzeile`).
   - **Phasenabschluss** nach Welle 10, 20, 30, 40: *(0.11-dev, Gegencheck Einsatz, Marius 16.11.)* Kurzbericht im Chat (Gegner, tiefster Kernstand, Coupons, Festplatten, Schadensanteil je Turmart) und eine Bonuswahl im Kern-Menü wie im Endlos-Modus: +80 Coupons, Kern-LP dauerhaft +10 % für diesen Lauf oder +5 Festplatten (`Endlos.Bonus.*`, `Einsatz.Bonus.KernMax`; offene Boni sammeln sich, gewählt wird zwischen den Wellen, Schalter `Einsatz.Phasenabschluss`). Ein Frühstart-Bonus kommt bewusst nicht in den Einsatz.
5. **Niederlage:** Der Kern fällt auf 0 LP, die Fabrik bleibt. Das Kern-Menü bietet **Welle wiederholen** (lädt `<Sitzung>_TD_W<n>_Start`, nur einen Stand dieses Spiels) oder **Einsatz beenden** (keine Kontingente und Sperren mehr, Kern voll und abreißbar; die Welt bleibt frei zum Weiterbauen, in diesem Spielstand startet nie wieder ein Einsatz, auch nicht nach Abriss des Kerns – Merkmal `mEinsatzBeendet` im Spielstand *(Nachprüfung 16.11., Marius)*). Die Kernreparatur im Laden ist dann gesperrt. **„Neu starten“ gibt es für Spieler nicht mehr**: Es behielt Coupons, Turmlevel und Forschung und zahlte alle Wellen noch einmal. Ein neuer Lauf ist ein neues Spiel. *(0.11-dev, Gegencheck Einsatz, Marius 16.11.)* (Im Entwicklermodus bleibt der Knopf für Selbsttests.)
6. **Sieg:** Alle 50 Wellen sind überstanden (in der Kern-Verteidigung: alle Wellen der gewählten Länge). Danach im Kern-Menü: Einsatz beenden.
7. **Abschlussbericht und Bestwert:** *(0.11-dev, Gegencheck Einsatz, Marius 16.11.)* Nach Sieg und Niederlage kommt ein Bericht über den ganzen Lauf (wie der Phasenbericht) und ein **Bestwert je Kernplatz und Stufe** in `Saved\FicsitTD\Bestwerte.json` (`Einsatz.<Platz>.Stufe<n>`: Welle × 1000, beim Sieg plus Kern-Rest in Prozent). Ein Lauf mit Eingriffen (Testsprung, `/gib`, `/td fuellen`, Freies Bauen …, gezählt am Kern `mLaufEingriffe`) oder mit den erweiterten Spieleinstellungen des Spiels bleibt ohne Wertung *(Nachprüfung 16.11., Marius)*. Ein Sieg nach „Welle wiederholen“ zählt voll.
8. **Nur im Entwicklermodus:** *(0.11-dev, Gegencheck Einsatz, Marius 16.11.)* Testsprung „zu Welle N“ in der Wellenleiste (jede übersprungene Welle wurde voll belohnt), **Freies Bauen** (Kontingente und Sperren aus; alte Stände schalten es beim Laden ab) und „Neu starten“. Das Startkit gibt es je Kern nur einmal, auch über „Einsatz beenden“ hinweg.

## 4. Kontingente und Laden

- **Begrenzt sind Produktions- und Verteidigungsgebäude**, je Typ. Logistik ist frei: Bänder, Lifte, Rohre, Stromleitungen. Fundamente, Wände und Rampen des Grundspiels sind im Einsatz und im Endlos nicht baubar: ihre Rezepte hängen an HUB-Meilensteinen, die gesperrt sind, und der Shop verkauft sie nicht (Marius 21.11., A5 Nr. 1, Probe `Rezeptprobe` 21.11.: 0 Rezepte). Splitter (auch Smart/Programmierbar), Verbinder, Lager und Flüssigkeitspuffer haben ein Kontingent und stehen nicht im Startkontingent, nur im Laden (Marius 02.11.). Das Abwägen zwischen neuer Fertigung und neuen Türmen ist gewollt (Marius, 05.11.).
- **Laden** im Kern-Menü: Ein Kontingent kostet Coupons, und der Preis steigt mit jedem Kauf. Dazu gibt es Spielerausrüstung und Kernreparatur. Seit 0.9.4 ist der Laden nach Kategorien geordnet (Türme, Verteidigung, Produktion, Strom, Logistik, Pionier, Kern), jede zum Einklappen. Noch nicht Erforschtes ist ausgeblendet, unten steht „N weitere im MAM erforschbar“. Turmlevel kauft man nicht im Laden, sondern im Turmfenster.
- **Spielerausrüstung und Kern im Laden** (kein Kontingent, wirkt sofort; Preise in der Laden-Tabelle unten). Ein Posten verschwindet, sobald das Maximum gekauft ist.
  - Inventarplätze: +5 je Kauf, höchstens 6 Käufe.
  - Handplatz: +1 je Kauf, höchstens 3.
  - Armierpistole, Klingenläufer, Gewehr, Nobelisken-Zünder: je einmal, fester Preis. Kaufbar, sobald Bolzenturm (ab Start), Assembler, Gewehrturm bzw. Caterium freigeschaltet sind.
  - Klingenläufer hat man ab Start; der Laden bietet sie nur als Ersatz an, wenn man keine mehr hat (weder im Inventar noch angelegt) *(0.12, C9)*.
  - Nobelisken-Zünder ab der Forschung Caterium, mit 10 Nobelisken (`Einsatz.Laden.Nobelisken-Zünder.Beigabe`). Danach „Nobelisken“: 5 Stück je Kauf, jeder Kauf 20 % teurer. Damit lassen sich Felsen über Rohstoffknoten sprengen, ohne die Nobelisk-Kette *(0.12, C14)*.
  - Kernreparatur: +25 % Kern-LP je Kauf, jeder Kauf 50 % teurer. Nur sichtbar, wenn der Kern beschädigt ist.
- **AWESOME-Shop als Laden** *(0.12 E4, Marius 14. und 21.11.)*: Nach Welle 1 (`Einsatz.Shop.AbWelle`, bis 21.11. nach Welle 2) kommt das Rezept des Shops, kostenlos, Kontingent 1. Er verkauft in sechs Bereichen *(22.11., Handspiel H5: vorher eine Kategorie „FICSIT-Abwehr“)*: Türme, Verteidigung, Produktion, Strom, Logistik, Pionier und Kern, darin Kontingente, Ausrüstung bzw. Kern und Festplatten alle Posten der Ladenliste: Kontingente mit Baumaterial, Ausrüstung, Inventar- und Handplätze, Kernreparatur und die **Festplatte** (40 Coupons, Aufschlag 0,25 = jede weitere 10 teurer, nur im Shop). Preis wie am Kern: `Grund × (1 + Aufschlag × Käufe)`; bei mehreren Stück in einem Kauf zieht der Mod die Staffel nach (das Spiel rechnet Stück × aktuellen Preis). Ein Posten ist sichtbar, solange der Einsatz läuft, er erforscht ist und seine Höchstzahl nicht erreicht. Sobald ein Shop steht, verkauft der Kern nicht mehr (`Einsatz.Shop.KernBisShop`), das Kern-Menü zeigt dann einen Hinweis und die Kontingente. Wahrheit sind die Kaufzähler am Kern (`mKontingentGekauft`); alte Stände behalten ihre Preise.
- **AWESOME-Shop im Einsatz** *(0.12, Marius 21.11., A5 Nr. 1)*: Bei aktivem Einsatz oder Endlos blendet der Shop alle Artikel des Spiels aus (Fundamente, Wände, Fabrikwagen, Deko) und lehnt ihren Kauf ab (`Einsatz.Shop.SpielArtikelSperren`). Bauteile des Spiels sind im Einsatz damit weder baubar noch kaufbar. Im Freien Spiel mit Abwehr bleibt der Shop unverändert.
- **Bonusfenster** *(0.12, Marius 21.11., C11)*: Nach jeder Bosswelle öffnet sich ein Fenster mit Phasenbericht und drei Bonus-Karten (Coupons, Kern-LP, Festplatten), dazu „Später wählen“; die Wahl bleibt im Kern-Menü offen (`Einsatz.Bonus.Fenster`).
- **Einstieg** *(0.13, Marius 22.11.: „dann baut sich alles logisch auf“; vorher 0.12 E5 MAM → Shop)*: Zu Beginn Aufgabe „Baue die HUB“ (Strom aus den Biomasse-Brennern, Wiedereinstiegspunkt), nach Welle 1 „Baue den AWESOME-Shop“ (Kontingente), nach Welle 2 „Baue das MAM“ (`Einsatz.Mam.AbWelle`, Festplatten), je einmal im Chat und von ADA mit Stimme angesagt (Deutsch/Englisch, einmal je Kern), in der HUD-Zeile, solange das Gebäude fehlt.
- **HUD-Statusleiste** *(0.12, Marius 21.11., C15)*: oben links Welle, Kern, Coupons, Festplatten, Rang; zweite Zeile „Noch baubar: …“ oder die Aufgabe „Baue den AWESOME-Shop“. Der Notizblock der To-do-Liste bekommt keinen Block mehr (das Spiel liest ihn nur beim Laden); `Einsatz.ToDo.Block` = 1 stellt ihn wieder her.
- **Kernplattform** *(0.12, Marius 21.11., A5 Nr. 6)*: Fundamente und Rampen der Plattform (bis 36 m um den Kern, `Einsatz.Plattform.Radius`) lassen sich im Einsatz nicht abreißen (`Einsatz.Plattform.AbrissSperren`). Ohne Rampen erreicht kein Gegner den Kern (Testfenster 1).
- **Pionier** (Reiter im Kern-Menü; Umbau nach `Notizen\Analyse-Spieler-und-Pionier-2026-11-12.md`: P0/P1 seit 0.9.6, P2 seit 0.9.7, P3/P4 seit 0.9.8):
  - **XP je Welle (0.9.8 halbiert):** 20 + 3 × N, +10 bei vollem Kern, jede zehnte Welle doppelt (`Pionier.XPWelle`, `XPJeWelle`, `XPKernVoll`). Dazu Leistungs-XP: 2 je Prozent Anteil am Wellenschaden (`Pionier.XPJeProzent`; der Anteil zählt höchstens bis `Pionier.XPAnteilKappe`, Standard 100 = ohne Kappe *(0.12, Regler für C10)*) und 1 je 200 reparierte LP (`Pionier.XPJeReparatur`). Eigener Abschuss 3 XP, Elite 10, Boss 50, wenn der Spieler den Gegner in den letzten 5 s getroffen hat. Assist 1 XP.
  - **Rang (0.9.8):** Schwelle je Rang 5 + 20 × Rang (`Pionier.XPGrund`, `XPSchritt`): Rang 2 bei 25 XP (also nach Welle 1), 3 bei 70, 4 bei 135, 5 bei 220, höchstens Rang 16 (`Pionier.MaxRang`). **Talentpunkte (0.12, Marius 21.11., C10):** nur noch aus geschafften Bosswellen (`Pionier.PunkteJeBoss` 1, Einsatz und Endlos, mit der Bonuswahl) und aus der Bohrung, zusammen höchstens 15 (`Pionier.MaxPunkte` *(Nachprüfung 16.11.)*). Der Rang bringt nur noch die Boni; `Pionier.TalenteAusRang` 1 stellt die alte Regel (ein Punkt je Rang) wieder her. Im Einsatz gibt es so bis Welle 40 vier Punkte. Alle 3 Ränge gibt es +10 LP und +3 % Lauftempo (`Pionier.RangBonusAlle`, `RangBonus.LP`, `RangBonus.Tempo`). Rang und Talente hängen am Spielstand (seit 0.12-dev `ATDSpielstand`) und bleiben auch nach dem Einsatz. Talente neu verteilen kostet 20 Coupons.
  - **Vier Rollen-Zweige mit je 5 Stufen (0.9.8, P3/P4, Marius 12.11.):** zwei Zweige ganz oder drei zur Hälfte. Jede Stufe ist eine Fähigkeit, keine reine Prozentzahl. Der Reiter zeigt je Zweig die Wirkung der letzten Welle.
    - **Schütze** (Handwaffen): 1 Kopftreffer ×1,5 · 2 Schnellladen (Nachladen doppelt so schnell) · 3 Durchschlag (nächster Gegner dahinter bis 6 m, halber Schaden) · 4 Munition sparen (jeder vierte Treffer kostet nichts) · 5 Sprengmantel (jeder fünfte Treffer explodiert mit vollem Trefferschaden in 4 m).
    - **Kommandant** (Türme lenken): 1 Zielmarkierung · 2 markierte Gegner nehmen +15 % Turmschaden · 3 stirbt ein markierter Gegner, springt die Markierung auf den nächsten in 10 m · 4 Sammelbefehl (Türme in 30 m schießen 5 s auf dein Ziel) · 5 Übersteuern wirkt auf alle Türme in 20 m.
    - **Techniker** (Bauten halten): 1 Feldreparatur (Bauten in 8 m heilen 5 % LP je Sekunde, 300 LP je Eisenplatte) · 2 Feldreparatur doppelt so schnell · 3 Übersteuern (Knopf im Turmfenster, 10 s +50 % Takt, danach 60 s Pause) · 4 Nachschub (Türme in 20 m ohne Munition bekommen 10 Stück aus deinem Inventar) · 5 Bollwerk (einmal je Welle fällt ein Bau nicht, sondern hält mit halben LP).
    - **Jäger** (Elite und Boss): 1 Schaden an Elite und Boss ×1,5 · 2 selbst erlegte Elite oder Boss: Festplatten und Coupons fallen sicher · 3 Schwachstelle (ein getroffener Boss nimmt 5 s +25 % von allen Türmen) · 4 Boss-Balken zeigt Resistenzen und deinen Anteil · 5 Hinrichtung (Elite unter 15 % LP stirbt beim nächsten Treffer von dir).
  - Die alten Zweige Armierpistole/Gewehr/Nobelisk/Überleben/Mobilität gibt es seit 0.9.8 nicht mehr. Markierung und Jäger-Bonus wirken nur noch mit dem passenden Talent.
  - **Optik (0.10.0, Marius 14.11.):** Der Reiter Pionier zeigt je Rolle einen Kopf mit Spiel-Icon und Stufenplakette und je Stufe eine Kachel mit Icon (gelernt orange, lernbar mit orangem Rahmen, gesperrt gedämpft). Die HUD-Zeile zeigt Symbol und Namen der stärksten Rolle(n). Sichtbare Fähigkeiten an Türmen: Feldreparatur grüner Strahl, Sammelbefehl oranger Strahl zum Ziel, Übersteuern orangener Puls am Turmkopf, Bollwerk helles Aufblitzen.
  - **Handwaffen treffen wie die Türme (0.9.6):** Armierpistole, Gewehr und Nobelisken nutzen den Munitionswert der Turm-Tabelle (Armierungsbolzen 40, Gewehrmunition 12, Nobelisk 150, bessere Munition entsprechend) statt des Spielwerts. Dazu gelten die Resistenzen der Gegner und ein Zuwachs mit der Welle: die Hälfte des LP-Zuwachses der Welle (`Pionier.WellenAnteil` 0,5). Regler je Waffe: `Pionier.HandFaktor.*`. Ziel (Marius 12.11.): Ein aktiver Spieler leistet 10–20 % des Wellenschadens, etwa wie ein guter Turm; die Balance bleibt für den passiven Spieler eingestellt.
  - **Bilanz:** Spielerschaden, Anteil am Wellenschaden und Abschüsse stehen in der Meldung nach jeder Welle („Du: …“), im Log („Pionier Welle N“) und in `Metriken.csv` (`Spieler_Schaden`, `Spieler_Anteil_Proz`, seit 0.9.7 auch `Spieler_Assists`, `Spieler_Markbonus`).
  - **Zielmarkierung (0.9.7, P2; seit 0.9.8 nur mit Kommandant 1, Bonus mit Kommandant 2):** Jeder Spielertreffer markiert den Gegner 6 s (`Pionier.Markierung.Dauer`), sichtbar als blaues Licht über ihm. Alle Türme machen an markierten Gegnern +15 % Schaden (`Pionier.Markierung.Bonus` 1,15). Zielwahl „Markierte zuerst“ (neu) nimmt sie vor allen anderen, „Elite und Boss zuerst“ direkt nach dem Boss. Erledigt ein Turm einen markierten Gegner, ist das ein Assist: 1 XP (`Pionier.XPAssist`), Zähler im Wellenbericht.
  - **Jäger (0.9.7, P2; seit 0.9.8 Talent Jäger 1 und 2):** Spielerschaden an Eliten und Bossen ×1,5 (`Pionier.Jaeger.Faktor`). Erlegt der Spieler eine Elite oder einen Boss (Treffer in den letzten 5 s), fallen Festplatten und Coupons sicher (`Pionier.Jaeger.SichereBeute`).
  - Wellengegner greifen den Spieler nie an (Marius 12.11.).
- **Abreißen** gibt den Platz im Kontingent zurück. **Während einer Welle** ist Abreißen im Verteidigungsfeld gesperrt, genau wie das Bauen (*0.11-dev, Marius 17.11.*): Ein Notabriss rettete sonst Turmlevel, die bei der Zerstörung verloren gehen. Wird ein Gebäude in der Welle zerstört, ist das Kontingent danach wieder frei, und das Baumaterial kommt ins Inventar (bei vollem Inventar wartet es am Kern).

## 5. Wellen und Gegner

- **Phasen zu je 10 Wellen**, Boss in Welle 10, 20, 30, 40, 50. Bosswellen haben 60 % des normalen Gegnerbudgets plus den Boss (`Wellen.BossBudget`):
  1. Hogs, ab Welle 7 auch der Klippen-Hog (Marius 08.11.; im Gegnerkatalog und im Phasentext des Spiels steht er bei Phase 4)
  2. Stinger (die ersten in Welle 13)
  3. Spucker Wald
  4. Wüsten- und Wasserspucker
  5. Nuklear-Hog und Johnny
- Fliegende Krabben kommen erst mit der Luftabwehr *(geplant)*.
- **Fronten:** Zu Beginn gibt es eine. Weitere kommen laut Stufentabelle dazu (Normal ab Welle 11 und 21).
- **Elite:** 2,5-fache LP, größer, eingefärbt. **Boss:** groß, mit Lebensbalken.
- **Zielwahl der Gegner:** Sie greifen an, was ihnen den Weg versperrt (Mauern). Gedränge am Kern (20 m, 3 s) führt zum Angriff auf den Kern.

## 6. Türme

- **Technik:** Türme sind Maschinen. Ein Produktionszyklus ist eine Aktion: Schuss, Magazin, Entladung oder 1 s Flamme. Maschinenfenster, Rezeptwahl, Strom und Übertakten kommen vom Spiel.
- **Munition je Item *(17.11., Marius, P10)*:** Ein Stück Munition reicht für mehrere Aktionen (`Munition.SchussJeItem.<Rezept>`): Grundmunition 4 (Armierungsbolzen, Nobelisk), Stufe 2 3 (Durchschlag-, Schock-, Streubolzen, Gas-/Puls-/Streu-Nobelisk), Stufe 3 2 (Explosivbolzen, Flak-Kanister, Ficsonium); Gewehr in Magazinen je Stück 3 / 2 / 2 / 2 / 1 (Standard, Zielsuch, Panzerbrechend, Brand, Turbo); Kampfdrohne 4 Einsätze je Ladung; Treibstoff unverändert. Ein Produktionszyklus ist eine Ladung: Er dauert so lange wie die Aktionen daraus, die Aktionen verteilen sich über den Zyklus, beim Gewehr fließen die Schüsse laufend ins Magazin (Dauerfeuer, solange Munition kommt). So zeigt auch das Maschinenfenster den echten Verbrauch. Ziel: ein voller Mk1-Eisenknoten trägt 4–5 Bolzentürme mit Grundmunition, eine kleine Kette 1–2. Startmunition, Pionier-Nachschub zählen Schuss und werden durch die Aktionen je Stück geteilt. Das Turmfenster zeigt „1 Stück = N Schuss · Stück/min bei Dauerfeuer · ein Band Mk1 trägt X Türme“.
- **Munitionsleitern:** Jede Stufe braucht eine Zutat mehr und kann mehr. Welche Munition ein Turm verschießt, wählt man über das Rezept im Maschinenfenster.
- **Schadensarten:** Durchschlag (Bolzen), Kugel (Gewehr), Feuer (Flamme), Strom (Tesla, Schockbolzen), Explosion (Nobelisk, Flächenschaden des Explosivbolzens; sein Haupttreffer zählt als Durchschlag), Gift (Gas-Nobelisk). Nachbrennen des Flammenwerfers stapelt im Dauerstrahl: jede Flammensekunde plant den Nachbrand für BrennDauer × (1 + L4-Bonus) s, also bei Treibstoff und Raketentreibstoff 3-fach, Turbotreibstoff 4-fach, ionisiertem Treibstoff 5-fach, auf Level 4 jeweils × 1,5 (`TDFlameTurret.cpp`, Prüfung 12.11.). Wie gut sie gegen welche Gegner wirken, steht in der Resistenztabelle unten.
- **Turmlevel 1–5:** Bezahlt wird mit Coupons im Turmfenster. Jedes Level ist am Modell sichtbar.
  - Level 2 gibt es mit dem Turm.
  - Level 3 und 4 kommen mit Turmfertigung II und III.
  - Level 5 (Meisterfähigkeit) kommt mit Turmfertigung IV, bei der Doktrin-Linie schon mit Turmfertigung III.
  - Meisterfähigkeiten (laut MAM-Text Turmfertigung IV): Bolzenturm Doppellauf, Gewehrturm Gurttrommel (+25 % Feuerrate, seit 17.11.; vorher Dauerfeuer, das seit P10 Standard ist), Nobelisk-Werfer Salvenkopf (drei Nobelisken je Salve), Teslaspule Spannungsfeld, Flammenwerfer Feuerteppich, Himmelsturm Sperrfeuer, Singularitätswerfer Ereignishorizont.
- **Zielwahl je Turm:** nächster zum Turm, nächster zum Kern, wenigste LP, meiste LP, Elite/Boss zuerst, markierte zuerst (0.9.7), dazu eine bevorzugte Gegnerfamilie.
- **Sichtlinie:** Fundamente, Rampen, Träger und Treppen nehmen keine Sicht. Mauern und alle Maschinen des Spiels (Fabrik vor den Türmen) nehmen Sicht. Eigene Verteidigung, Kreaturen und Logistik nehmen keine Sicht. *(seit 0.77, Maschinen bestätigt 12.11.)*
- **Übertakten:** Energie-Splitter im Maschinenfenster, bis 250 % Takt. Die Munition reicht dann kürzer, der Strom steigt. Im TD-Spiel ist das immer freigeschaltet.
- **Strom:** Ohne Netz lädt kein Turm nach. Der Gewehrturm verschießt bei Stromausfall noch sein geladenes Magazin. Die Teslaspule braucht 15 MW Grundlast und 125 MW beim Aufladen (seit 0.80, `Tuerme.Teslaspule.Ladeverbrauch`, vorher 75), mit Akkubank die Hälfte. Gegen Lastspitzen gibt es den Akku im Laden (ab der Forschung Kohlekraft).
- **Kombos (Standard seit 0.84, Schalter `Kombo.Aktiv`):** sieben Wechselwirkungen, je mit Ansage beim ersten Mal und Zähler im Turmfenster. Im MAM steht jede Kombo als Karte an ihrem Knoten.
  - Blitzableiter: Tesla springt bevorzugt auf Gegner mit steckendem Bolzen.
  - Hitzeriss: brennende Gegner verlieren die Panzerung gegen Kugel und Durchschlag.
  - Verpuffung: eine Flamme entzündet die Gaswolke des Gas-Nobelisken.
  - Ölbrand *(0.9.1)*: Flammenwerfer, Brandmunition, Feuerteppich oder ein Lockfeuer in 12 m entzünden das nasse Ölschlick-Feld. Es brennt 5 s mit 20 Schaden/s, wer hindurchläuft, brennt 3 s mit 10/s nach (`Tuerme.Oelschlick.Nachbrennen`, `NachbrennDauer`, `Tuerme.Lockfeuer.Zuendradius`).
  - Engstelle *(0.9.1)*: Steht das Ziel eines Bolzenturms in 12 m um ein Tor, durchschlägt der Bolzen einen Gegner mehr (`Kombo.Engstelle.Radius`, `Ziele`).
  - Sammelpunkt *(0.9.1)*: Gegner, die das Lockfeuer festhält, nehmen 25 % mehr Explosionsschaden (`Kombo.Sammelpunkt.Faktor`).
  - Leiter *(0.9.1)*: Ein Elektrozaun in Reichweite einer Teslaspule leitet jede Entladung an bis zu 3 Gegner, die ihn berühren (4 m, 30 % Schaden; `Kombo.Leiter.*`).
- **Bremsen (0.9.1):** Tesla-Feld, Ölschlick, Störfeld und Singularität stapeln nicht mehr „letzte gewinnt“. Bis zu drei laufen nebeneinander, es wirkt die stärkste; endet sie, greift die nächste.
- **Strahlenschutz (0.11-dev, Marius 16.11.):** Das Munitionslager der Türme ist abgeschirmt, radioaktive Munition (Ficsonium im Singularitätswerfer, Uran) strahlt nur beim Transport (`Tuerme.Abgeschirmt`). Die Forschung Singularität schaltet zusätzlich Hazmat-Anzug und Iod-Filter des Spiels frei (alle Modi; im Einsatz ist der HUB gesperrt).
- **Anschlüsse (0.9.3, ungetestet):** Türme und Bauten tragen die Original-Anschlüsse des Spiels: Bandeingang (`SM_Input_01`), Rohreingang (`SM_Pipe_InPut_01`), Wand-Stromdose (`SM_PowerPoleWall_MK1`) und Statusmast (`SM_ProductionLight_01`) mit der Statuslampe oben. Sie werden per Spielpfad geladen und nicht mitgeliefert. Die Sockel von Gewehr-, Nobelisk- und Flammenturm sind 1,65 m höher, Bolzen- und Himmelsturm 0,65 m, damit der Bandeingang in die Wand passt.
- **Himmelsturm (0.9.3, ungetestet):** In den sechs Rohren stecken sichtbare Flak-Kanister. Je Schuss verschwindet einer, nach `Tuerme.Himmelsturm.Nachladen` (1,5 s) gleitet ein neuer hinein.
- **Flammenwerfer (0.9.3, ungetestet):** Der Strahl besteht aus wandernden Feuerwolken (Flipbook, `Effekte.Flamme.*`).

## 6a. Bauten (Stand 0.9.3; was davon belegt ist, steht in Abschnitt 10)

- **Takt:** Schildgenerator, Funkmast, Störfeld-Projektor, Lockfeuer und Ölschlick-Feld laufen fest mit 100 %. Untertakten würde sie fast kostenlos machen (0.9.6).

- **Abwehrkern:** 16 × 16 m, rund 31 m hoch (seit 0.9.3 doppelt so groß). 16 Köder im Ring mit 9,4 m Abstand, daran greifen die Gegner an.
- **Tor:** 8 m breit mit 4 × 4 m Öffnung. Spieler und Fahrzeuge kommen durch, Gegner nicht. **Seit 0.9.3** braucht es 2 MW (`Bauten.Tor.Strom`). **Ohne Strom bleibt es während der Welle offen**, die Sperre im Navigationsnetz fällt weg und Kreaturen laufen durch (`Bauten.Tor.OhneStromOffen`). Statuslampe: grün mit Strom, gelb ohne Strom in der Bauphase, rot ohne Strom in der Welle.
- **Lockfeuer:** Köder. Sind Gegner in 40 m, öffnen die Klappen, Horn und Leuchte laufen, und 15 s lang gehen alle Gegner in 40 m zum Lockfeuer statt zum Kern. Danach 30 s Pause. Verbraucht Kohle per Band. Kombos: Sammelpunkt (Explosionen in der Nähe +25 %), entzündet Ölschlick in 12 m. Seit 0.9.3 symmetrisch: Leuchte mittig, Horn dreht um die Mastmitte.
- **Ölschlick-Feld:** 8 × 8 m, begehbar. Solange Öl fließt, bremst es. Angezündet brennt es 5 s, seit 0.9.3 mit sichtbaren Flammen über der ganzen Fläche (`Effekte.Oelbrand.*`). Anschluss über ein Pumpenhaus am Rand mit Original-Rohrflansch, Ventil auf dem Dach.
- **Elektrozaun:** seit 0.9.3 ohne Mauer, nur Pfosten, Schaltkästen und leuchtende Drähte. Für Gegner weiter ein Hindernis; wer angreift, bekommt einen Stromschlag. Seit 0.12 nimmt jeder Zaun 2 Kabel: eine Zaunreihe lässt sich vom Mast aus von Zaun zu Zaun verketten (`Bauten.Elektrozaun.Kabel`, C13).
- **Radar (Funkmast):** Türme in 30 m +10 % Reichweite. **Seit 0.9.3:** Rohstoffvorkommen in 150 m werden wie beim Ressourcenscanner angezeigt (`Tuerme.Funkmast.RessourcenRadius`). 30 s vor jeder Welle bis zu ihrem Ende markiert er jede aktive Front auf Karte und Kompass mit Welle, Gegnerarten und Anzahl; blau mit „FLIEGER!“, wenn Flieger kommen (`Tuerme.Funkmast.Vorwarnung`). Nur mit Strom.
- **Schildgenerator:** Kuppel mit 20 m Radius, schluckt Schaden an Bauten (nicht am Kern), 3000 Schild, gebrochen 20 s Ladezeit, sonst 5 %/s nach 5 s ohne Treffer.
- **Störfeld-Projektor** (Gabelung mit dem Schildgenerator, nur einer von beiden je Spiel): bremst alle Gegner in 15 m auf 60 % Tempo, solange er läuft (1 reanimiertes SAM je 60 s, 80 MW).
- **Drohnenplatz (seit 0.9.3, ersetzt die Reparaturstation):** Gabelung in der Festung ab Welle 40, Reparaturdrohnen **oder** Kampfdrohnen. 2 Drohnen auf Level 1, 3 auf Level 2, 4 auf Level 3 (Level mit Coupons im Gebäudefenster wie bei den Türmen, 0.9.6). Ein Rezeptzyklus ist ein Einsatz: Die Drohne steigt auf, fliegt in 15 m Höhe zum Ziel in 40 m, arbeitet und landet wieder.
  - Reparaturdrohnen: 2 Eisenplatten = 840 LP, 1 verstärkte Platte = 3150 LP für den am stärksten beschädigten Bau.
  - Kampfdrohnen: 2 Gewehrmunition je Einsatz, 3 s lang alle 0,35 s 25 Schaden (`Tuerme.Drohnenplatz.*`).
- **Gestrichen (0.9.3):** Dimensionslager und Reparaturstation sind nicht mehr erforschbar und nicht im Laden. Bereits gebaute bleiben in alten Spielständen erhalten. Ihre Balance-Schlüssel gibt es noch, in den Zahlentabellen sind sie (wie die Panzersperre) ausgeblendet.

## 7. Forschung (MAM)

- **4 Bäume:** Industrie, Ballistik, Elementar, Festung, alle ab Start im MAM. Jeder Baum wächst aus mehreren Wurzeln: Industrie seit 0.13 (A6, Marius 22.11.) aus sechs Einstiegen ohne Pflichtkette, nämlich Assembler (Fertigung), Kupfer, Förderband Mk2, Kohlekraftwerk (Strom und Öl: Raffinerie und Treibstoffkraftwerk hängen daran), Schwefel und Quarz; Ballistik aus Durchschlagbolzen, Gewehrturm, Nobelisk-Werfer und Himmelsturm; Elementar aus Teslaspule und Flammenwerfer; Festung aus Kern-Panzerung, Verstärkten Mauern und Turmfertigung II. Alle anderen Knoten hängen mit einer Linie an einem Elternknoten.
- **Sichtbarkeit:** Ein Knoten zeigt sein Icon erst, wenn sein Elternknoten erforscht ist; vorher ist er ein grauer Platzhalter. Knoten mit Voraussetzung aus einem anderen Baum (z. B. Ballistik braucht den Assembler, Elementar das Kohlekraftwerk) bleiben grau, bis sie erfüllt ist.
- **Infokarten je Knoten:** „Herstellung“ (Zutaten, Maschine, Stück pro Minute, Zutaten der Zutaten, Ziel-Turm), „Bauen“ (Baukosten) und „Vorher“ (nötige Forschung und Freigabe-Welle).
- **Bezahlt wird nur mit Festplatten** (seit 0.10.0, vorher FICSIT-Kampfdaten). Der Preis richtet sich nach der Freigabe-Welle des Knotens (Preisstufen ab Welle 0, 5, 15, 25, 35). Insgesamt gibt es bewusst mehr Knoten, als FP hereinkommen.
- **Freigabe-Welle:** Ein Knoten ist erst ab einer bestimmten Welle erforschbar. Er erbt die Welle von seinen Eltern und Voraussetzungen. Der Status steht im Knotentitel.
- **Gabelungen:** Bei jedem Paar ist nur eine Seite je Einsatz erforschbar. Das ist endgültig. Gabelungen: Streubolzen/Schockbolzen, Zielsuch/Panzerbrechend (beide ab Welle 10), Gas-/Puls-Nobelisk, Kettenblitz/Überlast, Reparatur-/Kampfdrohnen, Kern-Schild/Kern-Geschütz, Doktrin. Schildgenerator und Störfeld-Projektor sind seit 0.13 keine Gabelung mehr.
  - **Freies Spiel mit Abwehr** *(0.13, A6 Entscheidung 8)*: Dort sperrt eine gewählte Seite die andere nicht. Die Gegenseite kostet das Doppelte in Coupons (`Abwehr.GabelFaktor` 2).
- **Einstieg (0.13, A6):** Die erste Forschung ist vor Welle 3 möglich. Kohlekraftwerk, Teslaspule und Durchschlagbolzen sind ab Welle 3 frei (vorher 5 bzw. 6) und kosten damit 3 Festplatten. Der Gewehrturm kommt ab Welle 5 (Marius 22.11. abends: in der Simulation bremste er ab Welle 3 die Mischlinien). Beim ersten Besuch reicht es für genau eine der billigen Linien (Bolzen oder Tesla). Kettenblitz und Überlast ab Welle 6, Caterium und Panzerbrechend ab Welle 10.
- **Alte Spielstände:** Beim Laden bekommt jeder erforschte FicsitTD-Knoten alle Rezepte, die er im heutigen Baum freischaltet (z. B. Schwarzpulver jetzt am Schwefel). Erforschtes bleibt erforscht.
- **Festplatte abgeben** *(0.13, A6 E7)*: Im AWESOME-Shop (Bereich Pionier und Kern) gibt man eine Festplatte gegen 12 Coupons ab (`Einsatz.Laden.Festplatte.Rückgabe`; Schlüssel `Einsatz.Laden.Festplatte.Rueckgabe`). Der Shop zeigt 1 Coupon als Preis, der kommt mit zurück. Nur im Einsatz und im Endlos-Modus. Kaufen (ab 40) und Abgeben (12) hintereinander lohnt nie.
- **Doktrin:** Eine Turmlinie wird zur Hauptlinie. Sie bekommt mehr Schaden, billigere Level und Level 5 früher. **Heute** ist sie eine Gabelung mit fünf Seiten und endgültig. **Beschlossen:** wechselbar im Kern-Menü, einmal je Einsatz für 50 Coupons *(geplant, ohne Termin)*.
- **Kern-Forschung (Festung):**
  - Kern-Panzerung: Kern-LP × 1,3.
  - Notreparatur: Nach jeder abgewehrten Welle repariert sich der Kern um 10 % seiner LP.
  - Gabelung Kern-Schild oder Kern-Geschütz. Der Schild fängt in jeder Welle die ersten 2000 Schaden am Kern ab und lädt sich zum Start der nächsten Welle neu. Das Geschütz schießt alle 1,5 s einen Bolzen mit 60 Schaden auf den nächsten Gegner in 30 m.
- **Belohnungen** im MAM zeigen nur echte Freischaltungen. Die Schuss-Rezepte der Türme kommen still mit dem Knoten.
- **Bänder und Lifte** rüsten nach der Forschung Band Mk2/Mk3/Mk4/Mk5 automatisch auf (Mk4 ab W25 mit Schwerindustrie, Mk5 ab W30; Mk6 gibt es nicht, er braucht Ficsit). **Miner Mk2** (ab W25, nach Band Mk4; der Schlüssel `Forschung.MinerMk2.AbWelle` steht seit 0.13 auch auf 25) und **Miner Mk3** (ab W35, mit Mischtechnik) im Industrie-Baum; gekauft wird weiter das Kontingent Miner, das Baumaterial kommt passend zur höchsten Miner-Stufe mit *(17.11., Marius, P10)*.

## 8. Beute

- **Heute:**
  - Festplatten (bis 0.9.8 Kampfdaten), Coupons und Munition mit einer Chance je Gegnerstufe.
  - Energie-Splitter: 2 je Boss, Eliten mit 5 % Chance einen *(ungetestet)*.
- **Beschlossen** *(geplant, ohne Termin)*:
  - Eliten lassen **Energieschnecken** fallen, der Spieler stellt die Splitter selbst her.
  - Phasen-Bosse lassen einen **Somersloop** fallen, für einen Turm mit Doppelschuss.
  - Festplatten als Beute sollen wegfallen (heute noch normal 2 %, Elite 15 %, Boss 100 %, `Loot.Kampfdaten.*`) – noch nicht umgesetzt.

## 9. Kennzahlen

- **Druckquote Q** = effektive Gegner-LP (EHP) / (Abwehrkapazität K × 60 s).
  - EHP ist die Summe der Gegner-LP, geteilt durch die mittlere Schadenswirkung der aufgestellten Türme gegen den jeweiligen Gegner (Resistenzen).
  - K ist die Summe der Nenn-DPS aller Türme.
  - Q = 1 heißt: Die Türme bräuchten bei vollem Feuer 60 s für die ganze Welle.
  - Zielbänder: Normal 0,55–0,75, Boss bis etwa 1,0. Die Bänder werden im Prüfstand neu festgelegt; Methode offen (Fahrplan P12).
- **Weitere Kennzahlen im Log** (`Metriken.csv`, `Wirtschaft.csv`): Kernverlust, knappste Annäherung, Uptime, Overkill, Munitionsmangel mit Ziel, Zeit ohne Strom, EP und FP je Welle.

## 10. Belegung je System

Stand 16.11.2026 (Mod 0.11-dev; Berichte `Testberichte6-11-16_*`). Die langen Läufe sind Selbsttests im Prüfstand: Der Plan baut die Türme selbst, Strom und Munition werden gestellt, gebaut wird frei. Sie belegen Kampf und Balance, nicht die Fabrik dahinter. Berichte in `Testberichte\`, Zusammenfassung in `Mods\README.md`.

| System | Stand |
|---|---|
| Handspiel Einsatz (Bolzen, Gewehr) | im Spiel bis Welle 11 bestätigt (Marius) |
| Neues Spiel mit Kernplatz, Speichern, Laden | im Spiel bestätigt; Selbsttest „Neustart“ seit 0.77 |
| Forschung 2.0, Gabelungen, Doktrin, stille Rezepte, Übertakten | Selbsttest bestätigt |
| Turm-Paare Gewehr + Tesla, Gewehr + Flamme, Gewehr + Nobelisk bis Welle 30 | Selbsttest (Release090, 08.11.): alle drei bis W30, Kern-Tiefstand W10–16 38–68 %. Balance084 (11.11., Pläne R7): Gewehr-Tesla, Gewehr-Flamme und Mono-Nobelisk bestehen; Gewehr-Nobelisk fällt in W16, Mono-Bolzen in W22, Mono-Tesla R7 in W15 |
| Mono-Tesla bis Welle 50 (alle fünf Phasen und Bosse) | Selbsttest mehrfach 09.–11.11.: erreicht W50 (einziger Fehler meist die Prüfung „kein Sicherheitsnetz“), Kern am Ende je nach Lauf 30–100 % |
| Kombos | alle sieben im Selbsttest ausgelöst: Blitzableiter, Hitzeriss, Verpuffung (0.84.6), Ölbrand, Engstelle, Sammelpunkt (Pruefung091b), Leiter (LeiterE3, 48-mal) |
| Bauten | Selbsttest: Ölschlick bremst und brennt, Störfeld bremst, Drohnenplatz (2 Drohnen, 30 Einsätze), Sink-Sperre (`Bausperre`). Elektrozaun gebaut, aber 0 Schläge. Tor mit Strom, Radar, Anschlüsse, Himmelsturm-Kanister *(ungetestet)* |
| Englisch | Selbsttest Release090en bis W30; die 0.9.4-Texte im englischen Spiel nicht angesehen |
| Statistik, Pionier, Spielerausrüstung, ADA-Anlässe außer Begrüßung | kein Nachweis gefunden |
| Offen | Stufen außer Normal (Hart und Albtraum 16.11. nur per Simulation eingestellt, im Spiel als „ungetestet“ markiert), 11 von 12 Kernplätzen, Laden mitten in der Welle, Phasenabschluss und Bestwert im Einsatz, Flieger *(geplant)* |

---


## Sprache und Entwicklermodus (0.9.0)

- **Sprache:** Alle Spieltexte folgen der Spielsprache: Deutsch bei deutscher Spielsprache, sonst Englisch. C++-Texte stehen zweisprachig am Aufruf (`TDT`/`TDF`, `TDSprache.h`), Asset-Texte ersetzt der Mod beim Laden der Welt über `Scripts/uebersetzung_en.json`. Logs, CSV und Balance-Schlüssel bleiben deutsch. Testschalter: `-TDSprache=en|de`.
- **Entwicklermodus:** Test- und Diagnosebefehle (`/td` außer `ada`, `kern status|weiter`, `baender`; `/welle`, `/mauer`) nur mit `%LOCALAPPDATA%\FactoryGame\Saved\FicsitTD\Entwickler.txt` oder `-TDEntwickler`. Ebenso die reinen Mess-Hooks für die Ruckel-Analyse (Tick-Zeiten von Bewegung, Skelett, Kreatur und jedem Actor-Tick, *ab 0.10.2*): ohne Entwicklermodus nicht angemeldet, das Log meldet „Mess-Hooks (Tick-Zeiten): aus“.
- **Balance-Datei:** Der Mod legt keine `Balance.json` mehr an, damit Updates der Standardwerte beim Spieler ankommen. Vorlage: `Balance-Standard.json`.

## Zahlen

<!-- ZAHLEN-ANFANG -->
*Automatisch erzeugt von `Tests\erzeuge_spielregeln.py` aus `TDBalance.cpp` und `create_research.py`. Nicht von Hand ändern.*

### Schwierigkeitsstufen

| Wert | Erkundung | Normal | Hart | Albtraum |
|---|---|---|---|---|
| Gegnerbudget × | 0,6 | 1 | 1,1 | 1,1 |
| Gegner-LP × | 0,7 | 1 | 1 | 1,1 |
| Kern-LP | 30000 | 20000 | 15000 | 15000 |
| Aufbau bis Welle 1 (s, nur Kern-Verteidigung; Einsatz ohne Uhr) | 1800 | 1200 | 900 | 600 |
| Aufbau zwischen Wellen (s, nur Kern-Verteidigung) | 900 | 600 | 480 | 360 |
| Fronten zu Beginn | 1 | 1 | 1 | 1 |
| neue Front ab Welle | 16 | 11 | 8 | 6 |
| Elite ab Welle | 18 | 16 | 7 | 5 |

### Wellen

- Budget = Stufe × (12 + 5 × N + 0,15 × N²)
- Gegner-LP × (1 + 0,25 × (N − 1)) bis Welle 31, danach + 0,1 je weitere Welle; Elite-LP × 2,5
- Bosswellen: Gegnerbudget × 0,6 plus Boss
- höchstens 60 gleichzeitig, 120 je Welle (Nachschub alle 0,25 s je 5)
- Standardlänge eines Einsatzes: 50 Wellen

| Gegnerart | ab Welle |
|---|---|
| Hog | 0 |
| AlphaHog | 5 |
| KlippenHog | 7 |
| StingerKlein | 13 |
| Stinger | 15 |
| Spucker | 21 |
| RoterSpucker | 25 |
| AlphaSpucker | 28 |
| WuestenSpucker | 33 |
| RoterAlphaSpucker | 34 |
| WuestenAlpha | 35 |
| WasserSpucker | 36 |
| WasserAlpha | 37 |
| EliteStinger | 38 |
| NuklearHog | 41 |
| Johnny | 46 |

| Phase | Bosse | Boss-LP × |
|---|---|---|
| 1 | 1 | 8 |
| 2 | 1 | 25 |
| 3 | 1 | 15 |
| 4 | 1 | 15 |
| 5 | 2 | 2 |

### Belohnung je Welle

- **Coupons** = 8 + 2 × N, +5 bei vollem Kern bzw. +2 ab 75 %; Bosswelle × 2 + 20
- **Festplatten** (Forschung, seit 0.10.0 die Festplatte des Spiels) = 2, +1 bei Kern ab 75 %, +3 im Boss
- Biomasse 100; Kern-Grundreparatur 5 % je Welle

| Beute | Normal | Elite | Boss |
|---|---|---|---|
| Festplatten (Chance) | 0,02 | 0,15 | 1 |
| Coupons (Chance) | 0,03 | 0,5 | 1 |
| Munition (Chance) | 0,08 | 0,4 | 0 |
| Splitter (Chance) | 0 | 0,05 | 1 |

### Laden (Kontingente)

Preis = Grundpreis × (1 + Aufschlag × bisher gekauft); Paket = Gebäude je Kauf.

| Eintrag | Grundpreis | Aufschlag | Paket | Start |
|---|---|---|---|---|
| Abfueller | 8 | 0,4 | 1 | 0 |
| Akku | 10 | 0,4 | 1 | 0 |
| Armierpistole | 6 | 0 | 1 | 0 |
| Assembler | 8 | 0,4 | 1 | 0 |
| Biomassegenerator | 6 | 0,4 | 1 | 1 |
| Bolzenturm | 4 | 0,3 | 1 | 1 |
| Drohnenplatz | 14 | 0,5 | 1 | 0 |
| Elektrozaun | 5 | 0,3 | 1 | 0 |
| Fertiger | 14 | 0,5 | 1 | 0 |
| Festplatte | 40 | 0,25 | 1 | 0 |
| Festplattenabgabe | 0 | 0 | 1 | 0 |
| Flammenturm | 8 | 0,3 | 1 | 0 |
| Fluessigkeitspuffer | 3 | 0,3 | 1 | 0 |
| Foerderanlage | 6 | 0,4 | 3 | 0 |
| Funkmast | 8 | 0,5 | 1 | 0 |
| Gewehr | 10 | 0 | 1 | 0 |
| Gewehrturm | 6 | 0,3 | 1 | 0 |
| Giesserei | 8 | 0,4 | 1 | 0 |
| Handplatz | 8 | 0,5 | 1 | 0 |
| Himmelsturm | 10 | 0,4 | 1 | 0 |
| Hover-Pack | 24 | 0 | 1 | 0 |
| Inventarplätze | 6 | 0,5 | 5 | 0 |
| Jetpack | 16 | 0 | 1 | 0 |
| Kernreparatur | 15 | 0,5 | 25 | 0 |
| Klingenläufer | 8 | 0 | 1 | 0 |
| Kohlekraftwerk | 14 | 0,6 | 1 | 0 |
| Konstruktor | 4 | 0,3 | 1 | 1 |
| Lager | 4 | 0,5 | 1 | 0 |
| Lockfeuer | 4 | 0,4 | 1 | 0 |
| Mauer | 3 | 0,2 | 5 | 5 |
| Miner | 6 | 0,5 | 1 | 1 |
| Mischer | 10 | 0,5 | 1 | 0 |
| Nobelisk-Werfer | 10 | 0,3 | 1 | 0 |
| Nobelisken | 5 | 0,2 | 5 | 0 |
| Nobelisken-Zünder | 12 | 0 | 1 | 0 |
| Oelpumpe | 6 | 0,4 | 1 | 0 |
| Oelschlick | 4 | 0,3 | 1 | 0 |
| Raffinerie | 8 | 0,4 | 1 | 0 |
| Schildgenerator | 12 | 0,5 | 1 | 0 |
| Schmelzofen | 4 | 0,3 | 1 | 1 |
| Singularitaet | 20 | 0,6 | 1 | 0 |
| Splitter | 2 | 0,2 | 2 | 0 |
| Stoerfeld | 14 | 0,5 | 1 | 0 |
| Teslaspule | 10 | 0,4 | 1 | 0 |
| Tor | 4 | 0,3 | 1 | 0 |
| Treibstoffkraftwerk | 8 | 0,4 | 1 | 0 |
| Verbinder | 2 | 0,2 | 2 | 0 |
| Wasserpumpe | 4 | 0,3 | 1 | 0 |

### Türme

| Turm | Reichweite (m) | LP |
|---|---|---|
| Bolzenturm | 45 | 3000 |
| Gewehrturm | 42 | 3000 |
| Teslaspule | 30 | 3000 |
| Flammenwerfer | 20 | 3000 |
| Nobelisk-Werfer | 60 | 3000 |

Turmlevel kosten 8 / 15 / 25 / 40 Coupons (Level 2–5). Doktrin: Schaden +25 %, Level −25 %.

| Level-Wirkung | Wert |
|---|---|
| Bolzenturm.L2.Schaden | 0,2 |
| Bolzenturm.L3.Takt | 0,25 |
| Bolzenturm.L4.Durchschlag | 1 |
| Bolzenturm.L4.Reichweite | 0,15 |
| Flammenwerfer.L2.Schaden | 0,2 |
| Flammenwerfer.L3.Kegel | 0,5 |
| Flammenwerfer.L4.Nachbrennen | 0,5 |
| Flammenwerfer.L4.Reichweite | 0,15 |
| Gewehrturm.L2.Schaden | 0,2 |
| Gewehrturm.L3.Takt | 0,3 |
| Gewehrturm.L4.Magazin | 1 |
| Gewehrturm.L4.Reichweite | 0,15 |
| Gewehrturm.L5.Takt | 0,25 |
| Himmelsturm.L3.Takt | 0,25 |
| Himmelsturm.L4.Reichweite | 0,15 |
| Nobelisk-Werfer.L2.Schaden | 0,2 |
| Nobelisk-Werfer.L3.Takt | 0,25 |
| Nobelisk-Werfer.L4.Radius | 0,2 |
| Nobelisk-Werfer.L4.Reichweite | 0,15 |
| Preis2 | 8 |
| Preis3 | 15 |
| Preis4 | 25 |
| Preis5 | 40 |
| Singularitaet.L3.Takt | 0,2 |
| Singularitaet.L4.Reichweite | 0,15 |
| Teslaspule.L2.Reichweite | 0,1 |
| Teslaspule.L3.Schaden | 0,2 |
| Teslaspule.L3.Spruenge | 1 |
| Teslaspule.L4.Takt | 0,3 |

### Munition

| Schlüssel | Wert |
|---|---|
| Bolzen.Armierungsbolzen.Schaden | 40 |
| Bolzen.Betaeubungsbolzen.Betaeubung | 3 |
| Bolzen.Betaeubungsbolzen.Schaden | 59 |
| Bolzen.Durchschlagbolzen.Folgeschaden | 0,5 |
| Bolzen.Durchschlagbolzen.Schaden | 32 |
| Bolzen.Durchschlagbolzen.WeitereZiele | 2 |
| Bolzen.Explosivbolzen.Flaechenschaden | 87 |
| Bolzen.Explosivbolzen.Radius | 4,5 |
| Bolzen.Explosivbolzen.Schaden | 30 |
| Bolzen.Streubolzen.Schaden | 43 |
| Bolzen.Streubolzen.Splitter | 3 |
| Bolzen.Streubolzen.Splitterradius | 6 |
| Bolzen.Streubolzen.Splitterschaden | 43 |
| Nobelisk.Gas.GasDauer | 10 |
| Nobelisk.Gas.GasSchaden | 20 |
| Nobelisk.Gas.Radius | 6 |
| Nobelisk.Gas.Schaden | 20 |
| Nobelisk.Nobelisk.Radius | 6 |
| Nobelisk.Nobelisk.Schaden | 150 |
| Nobelisk.Nuke.Radius | 25 |
| Nobelisk.Nuke.Schaden | 2000 |
| Nobelisk.Puls.Betaeubung | 2 |
| Nobelisk.Puls.Radius | 8 |
| Nobelisk.Puls.Schaden | 90 |
| Nobelisk.Streu.Radius | 4 |
| Nobelisk.Streu.Schaden | 225 |
| Nobelisk.Streu.Streu | 5 |
| Patronen.Brand.BrandSchaden | 9 |
| Patronen.Brand.Magazin | 15 |
| Patronen.Brand.Schaden | 10 |
| Patronen.Brand.SchussProSekunde | 6 |
| Patronen.Panzerbrechend.Magazin | 15 |
| Patronen.Panzerbrechend.Schaden | 13 |
| Patronen.Panzerbrechend.SchussProSekunde | 6 |
| Patronen.Standard.Magazin | 15 |
| Patronen.Standard.Schaden | 12 |
| Patronen.Standard.SchussProSekunde | 6 |
| Patronen.Turbo.Magazin | 75 |
| Patronen.Turbo.Schaden | 15 |
| Patronen.Turbo.SchussProSekunde | 12 |
| Patronen.Zielsuch.Magazin | 20 |
| Patronen.Zielsuch.Schaden | 20 |
| Patronen.Zielsuch.SchussProSekunde | 5 |
| Reparatur.Eisenplatte | 800 |
| Reparatur.Stahltraeger | 2000 |
| Reparatur.VerstaerktePlatte | 3000 |
| SchussJeItem.Drohne_Kampf | 4 |
| SchussJeItem.Drohne_Reparatur | 1 |
| SchussJeItem.Drohne_ReparaturVerstaerkt | 1 |
| SchussJeItem.Feld_SAM | 1 |
| SchussJeItem.Flak_Shoot | 2 |
| SchussJeItem.Flame_Fuel | 1 |
| SchussJeItem.Funk_Strom | 1 |
| SchussJeItem.Gun_Brand | 2 |
| SchussJeItem.Gun_Homing | 2 |
| SchussJeItem.Gun_Panzer | 2 |
| SchussJeItem.Gun_Standard | 3 |
| SchussJeItem.Gun_Turbo | 1 |
| SchussJeItem.Nobelisk_Cluster | 3 |
| SchussJeItem.Nobelisk_Explosive | 4 |
| SchussJeItem.Nobelisk_Gas | 3 |
| SchussJeItem.Nobelisk_Nuke | 1 |
| SchussJeItem.Nobelisk_Pulse | 3 |
| SchussJeItem.Schild_Strom | 1 |
| SchussJeItem.Shoot_Durchschlag | 3 |
| SchussJeItem.Shoot_Explosive | 2 |
| SchussJeItem.Shoot_SpikedRebar | 4 |
| SchussJeItem.Shoot_Spreadshot | 3 |
| SchussJeItem.Shoot_Stunshot | 3 |
| SchussJeItem.Singularitaet_Ficsonium | 2 |
| SchussJeItem.Tesla_Discharge | 1 |
| SchussJeItem.Zaun_Strom | 1 |
| Tesla.Schaden | 300 |
| Tesla.Spruenge | 3 |
| Tesla.SprungFaktor | 0,35 |
| Tesla.Sprungweite | 8 |
| Treibstoff.Biotreibstoff.Betaeubung | 0 |
| Treibstoff.Biotreibstoff.Kegel | 1,4 |
| Treibstoff.Biotreibstoff.NachbrennDauer | 2 |
| Treibstoff.Biotreibstoff.Nachbrennen | 8 |
| Treibstoff.Biotreibstoff.SchadenProSekunde | 35 |
| Treibstoff.Ionisiert.AlsStrom | 1 |
| Treibstoff.Ionisiert.Betaeubung | 0,3 |
| Treibstoff.Ionisiert.Kegel | 1 |
| Treibstoff.Ionisiert.NachbrennDauer | 5 |
| Treibstoff.Ionisiert.Nachbrennen | 25 |
| Treibstoff.Ionisiert.SchadenProSekunde | 160 |
| Treibstoff.Raketentreibstoff.Betaeubung | 0 |
| Treibstoff.Raketentreibstoff.Kegel | 0,75 |
| Treibstoff.Raketentreibstoff.NachbrennDauer | 3 |
| Treibstoff.Raketentreibstoff.Nachbrennen | 20 |
| Treibstoff.Raketentreibstoff.SchadenProSekunde | 140 |
| Treibstoff.Treibstoff.Betaeubung | 0 |
| Treibstoff.Treibstoff.Kegel | 1 |
| Treibstoff.Treibstoff.NachbrennDauer | 3 |
| Treibstoff.Treibstoff.Nachbrennen | 10 |
| Treibstoff.Treibstoff.SchadenProSekunde | 80 |
| Treibstoff.Turbotreibstoff.Betaeubung | 0 |
| Treibstoff.Turbotreibstoff.Kegel | 0,9 |
| Treibstoff.Turbotreibstoff.NachbrennDauer | 4 |
| Treibstoff.Turbotreibstoff.Nachbrennen | 15 |
| Treibstoff.Turbotreibstoff.SchadenProSekunde | 120 |

### Resistenzen (Schadensfaktor)

| Familie | Durchschlag | Kugel | Feuer | Strom | Explosion | Gift |
|---|---|---|---|---|---|---|
| Hog | 1 | 0,75 | 1 | 1 | 1,25 | 1 |
| Krabbe | 0,8 | 1,3 | 1 | 1,3 | 1 | 1 |
| Spitter | 1 | 1 | 1,3 | 0,85 | 1 | 1 |
| Stinger | 0,8 | 1,25 | 1,2 | 1,2 | 0,85 | 1 |
| WasserSpitter | 1 | 1 | 0,8 | 1,3 | 1 | 1 |
| WuestenSpitter | 1 | 1,2 | 0,75 | 1 | 1 | 1 |

### Forschung (MAM)

Preis aus der Phase der effektiven Freigabe-Welle (Eltern und Voraussetzungen eingerechnet).

| Baum | Knoten | Name | ab Welle | Festplatten | Gabelung mit |
|---|---|---|---|---|---|
| Industrie | Kohlekraft | Kohlekraftwerk | 3 | 3 |  |
| Industrie | Assembler | Assembler und Eisenteile | 0 | 3 |  |
| Industrie | Kupfer | Kupferverarbeitung | 0 | 3 |  |
| Industrie | BandMk2 | Förderband Mk2 | 0 | 3 |  |
| Industrie | Munitionspuffer | Munitionspuffer | 0 | 3 |  |
| Industrie | Giesserei | Gießerei und Stahl | 0 | 3 |  |
| Industrie | BandMk3 | Förderband Mk3 | 10 | 4 |  |
| Industrie | Raffinerie | Raffinerie und Öl | 15 | 6 |  |
| Industrie | Treibstoffkraft | Treibstoffkraftwerk | 15 | 6 |  |
| Industrie | Schwefel | Schwefel | 5 | 4 |  |
| Industrie | Quarz | Quarzverarbeitung | 10 | 4 |  |
| Industrie | Caterium | Caterium | 10 | 4 |  |
| Industrie | Bauxit | Bauxit und Aluminium | 25 | 8 |  |
| Industrie | Schwerindustrie | Schwerindustrie | 25 | 8 |  |
| Industrie | Mischtechnik | Mischtechnik | 35 | 10 |  |
| Industrie | BandMk4 | Förderband Mk4 | 25 | 8 |  |
| Industrie | BandMk5 | Förderband Mk5 | 30 | 8 |  |
| Industrie | MinerMk2 | Miner Mk2 | 25 | 8 |  |
| Industrie | MinerMk3 | Miner Mk3 | 35 | 10 |  |
| Ballistik | Durchschlagbolzen | Durchschlagbolzen | 3 | 3 |  |
| Ballistik | Gewehrturm | Gewehrturm | 5 | 4 |  |
| Ballistik | Explosivbolzen | Explosivbolzen | 25 | 8 |  |
| Ballistik | Streubolzen | Streubolzen | 10 | 4 | Schockbolzen |
| Ballistik | Zielsuchmunition | Zielsuchmunition | 10 | 4 | Panzerbrechend |
| Ballistik | Panzerbrechend | Panzerbrechende Munition | 10 | 4 | Zielsuchmunition |
| Ballistik | NobeliskWerfer | Nobelisk-Werfer | 10 | 4 |  |
| Ballistik | Schockbolzen | Schockbolzen | 10 | 4 | Streubolzen |
| Ballistik | Brandmunition | Brandmunition | 15 | 6 |  |
| Ballistik | GasNobelisk | Gas-Nobelisk | 15 | 6 | PulsNobelisk |
| Ballistik | PulsNobelisk | Puls-Nobelisk | 15 | 6 | GasNobelisk |
| Ballistik | Turbomunition | Turbomunition | 25 | 8 |  |
| Ballistik | StreuNobelisk | Streu-Nobelisk | 25 | 8 |  |
| Ballistik | Himmelsturm | Himmelsturm | 35 | 10 |  |
| Ballistik | Singularitaet | Singularitätswerfer | 51 | 10 |  |
| Elementar | Teslaspule | Teslaspule | 3 | 3 |  |
| Elementar | Kettenblitz | Kettenblitz | 6 | 4 | Ueberlast |
| Elementar | Ueberlast | Überlast | 6 | 4 | Kettenblitz |
| Elementar | Akkubank | Akkubank | 15 | 6 |  |
| Elementar | Hochspannung | Hochspannung | 25 | 8 |  |
| Elementar | Flammenwerfer | Flammenwerfer | 15 | 6 |  |
| Elementar | Turbotreibstoff | Turbotreibstoff | 25 | 8 |  |
| Elementar | Raketentreibstoff | Raketentreibstoff | 35 | 10 |  |
| Elementar | Plasmaflamme | Plasmaflamme | 38 | 10 |  |
| Elementar | Oelschlick | Ölschlick-Feld | 15 | 6 |  |
| Elementar | Elektrozaun | Elektrozaun | 12 | 4 |  |
| Festung | KernPanzer | Kern-Panzerung | 0 | 3 |  |
| Festung | KernNotrep | Notreparatur | 0 | 3 |  |
| Festung | MauerMk2 | Verstärkte Mauern | 6 | 4 |  |
| Festung | Reparaturdrohnen | Reparaturdrohnen | 40 | 10 | Kampfdrohnen |
| Festung | Kampfdrohnen | Kampfdrohnen | 40 | 10 | Reparaturdrohnen |
| Festung | KernSchild | Kern-Schild | 8 | 4 | KernGeschuetz |
| Festung | KernGeschuetz | Kern-Geschütz | 10 | 4 | KernSchild |
| Festung | Turmfertigung2 | Turmfertigung II | 10 | 4 |  |
| Festung | Stachelmauer | Stachelmauern | 12 | 4 |  |
| Festung | Turmfertigung3 | Turmfertigung III | 20 | 6 |  |
| Festung | DoktrinBolzen | Doktrin: Bolzenturm | 15 | 6 | DoktrinGewehr, DoktrinNobelisk, DoktrinTesla, DoktrinFlamme |
| Festung | DoktrinGewehr | Doktrin: Gewehrturm | 15 | 6 | DoktrinBolzen, DoktrinNobelisk, DoktrinTesla, DoktrinFlamme |
| Festung | DoktrinNobelisk | Doktrin: Nobelisk-Werfer | 15 | 6 | DoktrinBolzen, DoktrinGewehr, DoktrinTesla, DoktrinFlamme |
| Festung | DoktrinTesla | Doktrin: Teslaspule | 15 | 6 | DoktrinBolzen, DoktrinGewehr, DoktrinNobelisk, DoktrinFlamme |
| Festung | DoktrinFlamme | Doktrin: Flammenwerfer | 15 | 6 | DoktrinBolzen, DoktrinGewehr, DoktrinNobelisk, DoktrinTesla |
| Festung | Turmfertigung4 | Turmfertigung IV: Meisterstufe | 30 | 8 |  |
| Festung | Tor | Tor | 6 | 4 |  |
| Festung | Lockfeuer | Lockfeuer | 8 | 4 |  |
| Festung | Funkmast | Funkmast | 12 | 4 |  |
| Festung | Schildgenerator | Schildgenerator | 25 | 8 |  |
| Festung | Stoerfeld | Störfeld-Projektor | 35 | 10 |  |

Alle Knoten zusammen: 384 FP.

<!-- ZAHLEN-ENDE -->
