---
titel: Ring „instabiler Boden“ – keine Fabrikgebäude in 40 m um den Kern (K2)
typ: neu
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: KZ-3-Ring
experiment:
---

**Was:**
- Im Einsatz und im Endlos lassen sich in 40 m um den Abwehrkern (`Einsatz.Ring.Radius`) keine Fabrikgebäude des Spiels bauen: Maschinen, Lager, Generatoren, MAM, Shop, HUB.
- Das Hologramm wird rot mit „Boden instabil: Das Bohrbeben des Abwehrkerns trägt hier keine Fabrik. Fabrikgebäude erst ab 40 m vom Kern, Abwehrbauten gehen.“ (DE/EN, Disqualifier `UTDCDBodenInstabil`).
- Erlaubt bleiben alle Bauten des Mods (Türme, Mauern, Stützbauten; Lore: Tiefanker) und die Logistik (Bänder, Rohre, Strom).
- Der Bestand bleibt stehen, gesperrt ist nur der Neubau.
- Umgesetzt in `UTDEinsatzSubsystem::SperrGrund` (Grund „Boden“) und `IstFabrikgebaeude`, Schalter `Einsatz.Ring.Aktiv`.
- Der Selbsttest-Schritt `pruefe: Bausperre` kennt dafür `abstand` (m).

**Warum:** Marius am 21.11. abends: Zubauen des Kerns ohne Zerstörung verhindern. Lore: Der Boden um den Bohrer ist nicht stabil genug für Fabriken (Fahrplan 0.13, Etappe K2).

**Noch offen:** Der Ring ist am Boden noch nicht sichtbar (Decal oder Markierung, Skill `satisfactory-design`). Die ADA-Ansage mit Stimme fehlt noch.

**Changelog EN:** Unstable ground: factory buildings can no longer be placed within 40 m of the Defense Core during an operation; defenses and logistics still can.

**Changelog DE:** Instabiler Boden: Fabrikgebäude lassen sich im Einsatz nicht mehr näher als 40 m am Abwehrkern bauen; Abwehrbauten und Logistik schon.

**Nachweis (22.11. vormittags):** Plan `KZ-3-Ring` bestanden (Läufe `0855`, `0921`, `0948`, `1031`).
- Konstruktor bei 20 und 39 m „Boden“, bei 41 m und fern erlaubt.
- MAM und Lager bei 20 m „Boden“.
- Bolzenturm, Verteidigungsmauer, Band und Strommast bei 20 m erlaubt.
