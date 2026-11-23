---
titel: Öl-Hinweis beim Einsatzstart (Entfernung und Richtung zum nächsten Rohöl)
typ: neu
modi: einsatz, endlos
sichtbar: ja
doku: ja
simulation: nicht-noetig
tests: Oel
experiment:
---

**Was:**
- Beim Einsatzstart meldet der Mod im Chat einmal Entfernung und Richtung zum nächsten Rohöl-Vorkommen. Liegt eine Ölquelle näher, nennt er sie dazu (braucht den Druckregler).
- Bei mehr als 1 km kommt der Rat dazu: „plane eine lange Rohrleitung oder setze zuerst auf Kohle“.
- Code: `UTDKernplatz::OelHinweis`, aufgerufen in `UTDEinsatzSubsystem::Starte`.
- Das Handbuch nennt die Entfernungen je Biom: Wald 0,8–1,1 km, Felsenwüste 1,2–1,9 km, Dünenwüste 1,3–1,9 km, Grasfelder 2,2–2,4 km. Der Wald ist der Ölplatz.

**Warum:** Messung 23.11. (`Notizen/Oel-Entfernung-2026-11-23.md`): An 9 von 12 Kernplätzen liegt Öl weiter als 1 km. Marius am 23.11. abends: „A wirkt künstlich. dann bleibt es bei D“. Also keine Ölkanister im Shop und kein Ölknoten am Kern, sondern ehrlich sagen, wo Öl ist.

**Changelog EN:** Where is the oil? When an operation starts, the chat tells you how far away the nearest crude oil is and in which direction.

**Changelog DE:** Wo ist Öl? Beim Einsatzstart sagt der Chat Entfernung und Richtung zum nächsten Rohöl.

**Nachweis:** Build 23.11. abends. Selbsttest `Oel` grün auf Deutsch (`2026-11-23_1848`: „Das nächste Rohöl-Vorkommen liegt 2,3 km Osten …“) und auf Englisch (`_1851`: „The nearest crude oil node is 2.3 km east …“).
