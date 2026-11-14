#include "TDBalance.h"
#include "FicsitTDModule.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace TDBalanceIntern
{
	/** Standardwerte. Einheiten: Strecken in m, Zeiten in s, Tempo in m/s, Faktoren als Dezimalzahl. */
	struct FTDStandard
	{
		const TCHAR* Schluessel;
		float Wert;
	};
	const FTDStandard Standards[] = {
		// ---- Schwierigkeitsstufen (Level-Konzept Abschnitt 5); Hart nach Test G2 (02.11.) verschaerft ----
		// Fronten (Marius 02.11.): eine Richtung zu Beginn; zweite ab NeueFrontAbWelle, dritte ab 2 x NeueFrontAbWelle - 1 (Normal: 11 / 21)
		{ TEXT("Stufen.Erkundung.Budget"), 0.6f }, { TEXT("Stufen.Erkundung.GegnerLP"), 0.7f }, { TEXT("Stufen.Erkundung.AufbauSekunden"), 1800.f },
		{ TEXT("Stufen.Erkundung.PauseSekunden"), 900.f }, { TEXT("Stufen.Erkundung.FrontenStart"), 1.f }, { TEXT("Stufen.Erkundung.NeueFrontAbWelle"), 16.f },
		{ TEXT("Stufen.Erkundung.EliteAbWelle"), 15.f }, { TEXT("Stufen.Erkundung.KernLP"), 30000.f },
		{ TEXT("Stufen.Normal.Budget"), 1.0f }, { TEXT("Stufen.Normal.GegnerLP"), 1.0f }, { TEXT("Stufen.Normal.AufbauSekunden"), 1200.f },
		{ TEXT("Stufen.Normal.PauseSekunden"), 600.f }, { TEXT("Stufen.Normal.FrontenStart"), 1.f }, { TEXT("Stufen.Normal.NeueFrontAbWelle"), 11.f },
		{ TEXT("Stufen.Normal.EliteAbWelle"), 13.f }, { TEXT("Stufen.Normal.KernLP"), 20000.f },
		{ TEXT("Stufen.Hart.Budget"), 2.0f }, { TEXT("Stufen.Hart.GegnerLP"), 1.6f }, { TEXT("Stufen.Hart.AufbauSekunden"), 900.f },
		{ TEXT("Stufen.Hart.PauseSekunden"), 480.f }, { TEXT("Stufen.Hart.FrontenStart"), 1.f }, { TEXT("Stufen.Hart.NeueFrontAbWelle"), 8.f },
		{ TEXT("Stufen.Hart.EliteAbWelle"), 7.f }, { TEXT("Stufen.Hart.KernLP"), 15000.f },
		{ TEXT("Stufen.Albtraum.Budget"), 2.0f }, { TEXT("Stufen.Albtraum.GegnerLP"), 1.7f }, { TEXT("Stufen.Albtraum.AufbauSekunden"), 600.f },
		{ TEXT("Stufen.Albtraum.PauseSekunden"), 360.f }, { TEXT("Stufen.Albtraum.FrontenStart"), 1.f }, { TEXT("Stufen.Albtraum.NeueFrontAbWelle"), 6.f },
		{ TEXT("Stufen.Albtraum.EliteAbWelle"), 5.f }, { TEXT("Stufen.Albtraum.KernLP"), 10000.f },

		// 0.77 (Marius 05.11.2026): EliteAbWelle 15/10/7/5 wie in seiner Balance.json, mit denen alle Testlaeufe liefen
		// ---- Wellen: Budget = Stufe.Budget x (Grund + JeWelle x N + Quadrat x N^2); Gegner-LP x (1 + LPJeWelle x (N-1)) ----
		// 0.56 (Lauf 03.11.: Q 0,06 statt 0,2-0,3, Wellen viel zu leicht): Simulation Q W5 0,12 / W10 0,28 bei Standard-Turmzahl
		{ TEXT("Wellen.BudgetGrund"), 12.f }, { TEXT("Wellen.BudgetJeWelle"), 5.f }, { TEXT("Wellen.BudgetQuadrat"), 0.15f },
		{ TEXT("Wellen.LPJeWelle"), 0.25f }, { TEXT("Wellen.EliteLP"), 2.5f }, { TEXT("Wellen.MaxGegner"), 60.f },
		// T5 Stroeme: Gesamtzahl je Welle, gleichzeitig lebende hoechstens, Nachschub alle NachschubTakt s je NachschubPaket Gegner
		{ TEXT("Wellen.MaxGesamt"), 120.f }, { TEXT("Wellen.MaxGleichzeitig"), 60.f }, { TEXT("Wellen.NachschubTakt"), 0.25f }, { TEXT("Wellen.NachschubPaket"), 5.f },
		{ TEXT("Wellen.BossAlphaAnzahl"), 3.f }, { TEXT("Wellen.BossAlphaLP"), 4.f }, { TEXT("Wellen.BossJohnnyLP"), 2.f },

		// ---- Wellen-Progression (Wellen-Progression.md): Einfuehrungswelle je Art, Gewicht der Familie der laufenden Phase, Bosse je Phase ----
		// 0.84.4 Buendel A (Review 08.11., Pruefung0843 je 2 Laeufe: Kernverlust -13 bis -29 %): W1 glatter, W11 entzerrt, Boss W10 leichter, Klippen-Hog W7 (Marius)
		{ TEXT("Katalog.Hog.AbWelle"), 0.f },
		{ TEXT("Katalog.AlphaHog.AbWelle"), 5.f },
		{ TEXT("Katalog.StingerKlein.AbWelle"), 13.f },
		{ TEXT("Katalog.Stinger.AbWelle"), 15.f },
		{ TEXT("Katalog.Spucker.AbWelle"), 21.f },
		{ TEXT("Katalog.RoterSpucker.AbWelle"), 25.f },
		{ TEXT("Katalog.AlphaSpucker.AbWelle"), 28.f },
		{ TEXT("Katalog.RoterAlphaSpucker.AbWelle"), 34.f },
		{ TEXT("Katalog.KlippenHog.AbWelle"), 7.f },
		{ TEXT("Katalog.WuestenSpucker.AbWelle"), 33.f },
		{ TEXT("Katalog.WuestenAlpha.AbWelle"), 35.f },
		{ TEXT("Katalog.WasserSpucker.AbWelle"), 36.f },
		{ TEXT("Katalog.WasserAlpha.AbWelle"), 37.f },
		{ TEXT("Katalog.EliteStinger.AbWelle"), 38.f },
		{ TEXT("Katalog.NuklearHog.AbWelle"), 41.f },
		{ TEXT("Katalog.Johnny.AbWelle"), 46.f },
		{ TEXT("Katalog.PhasenGewicht"), 3.f },
		{ TEXT("Phasen.1.BossAnzahl"), 1.f }, { TEXT("Phasen.1.BossLP"), 8.0f }, { TEXT("Phasen.2.BossAnzahl"), 1.f }, { TEXT("Phasen.2.BossLP"), 25.0f }, { TEXT("Phasen.3.BossAnzahl"), 1.f }, { TEXT("Phasen.3.BossLP"), 15.0f }, { TEXT("Phasen.4.BossAnzahl"), 1.f }, { TEXT("Phasen.4.BossLP"), 15.0f }, { TEXT("Phasen.5.BossAnzahl"), 2.f }, { TEXT("Phasen.5.BossLP"), 2.0f },

		// ---- Gegner (Gehirn): Schaden je Angriff, Sekunden zwischen Angriffen, Lauftempo m/s, Angriffsreichweite m ----
		{ TEXT("Gegner.Hog.Schaden"), 30.f }, { TEXT("Gegner.Hog.Takt"), 1.5f }, { TEXT("Gegner.Hog.Tempo"), 7.5f }, { TEXT("Gegner.Hog.Reichweite"), 3.5f },
		{ TEXT("Gegner.NuclearHog.Schaden"), 40.f }, { TEXT("Gegner.NuclearHog.Takt"), 1.5f }, { TEXT("Gegner.NuclearHog.Tempo"), 6.5f }, { TEXT("Gegner.NuclearHog.Reichweite"), 3.5f },
		{ TEXT("Gegner.AlphaHog.Schaden"), 60.f }, { TEXT("Gegner.AlphaHog.Takt"), 1.8f }, { TEXT("Gegner.AlphaHog.Tempo"), 6.5f }, { TEXT("Gegner.AlphaHog.Reichweite"), 4.f },
		{ TEXT("Gegner.Johnny.Schaden"), 300.f }, { TEXT("Gegner.Johnny.Takt"), 3.f }, { TEXT("Gegner.Johnny.Tempo"), 5.f }, { TEXT("Gegner.Johnny.Reichweite"), 4.5f },
		{ TEXT("Gegner.Spitter.Schaden"), 15.f }, { TEXT("Gegner.Spitter.Takt"), 2.f }, { TEXT("Gegner.Spitter.Tempo"), 4.5f }, { TEXT("Gegner.Spitter.Reichweite"), 15.f },
		{ TEXT("Gegner.SpitterAlpha.Schaden"), 25.f }, { TEXT("Gegner.SpitterAlpha.Takt"), 2.f }, { TEXT("Gegner.SpitterAlpha.Tempo"), 4.5f }, { TEXT("Gegner.SpitterAlpha.Reichweite"), 18.f },
		{ TEXT("Gegner.StingerKlein.Schaden"), 10.f }, { TEXT("Gegner.StingerKlein.Takt"), 1.f }, { TEXT("Gegner.StingerKlein.Tempo"), 9.f }, { TEXT("Gegner.StingerKlein.Reichweite"), 2.5f },
		{ TEXT("Gegner.Stinger.Schaden"), 20.f }, { TEXT("Gegner.Stinger.Takt"), 1.f }, { TEXT("Gegner.Stinger.Tempo"), 8.5f }, { TEXT("Gegner.Stinger.Reichweite"), 3.f },
		{ TEXT("Gegner.StingerElite.Schaden"), 35.f }, { TEXT("Gegner.StingerElite.Takt"), 1.f }, { TEXT("Gegner.StingerElite.Tempo"), 6.f }, { TEXT("Gegner.StingerElite.Reichweite"), 3.f },
		{ TEXT("Gegner.Sonstige.Schaden"), 25.f }, { TEXT("Gegner.Sonstige.Takt"), 1.5f }, { TEXT("Gegner.Sonstige.Tempo"), 7.f }, { TEXT("Gegner.Sonstige.Reichweite"), 3.5f },

		// ---- Tuerme: Reichweite m (Stufe 0), Lebenspunkte (Stufe 0); Flammenwerfer nach Test G2 20 m ----
		{ TEXT("Tuerme.Bolzenturm.Reichweite"), 45.f }, { TEXT("Tuerme.Bolzenturm.LP"), 3000.f },
		{ TEXT("Tuerme.Gewehrturm.Reichweite"), 42.f }, { TEXT("Tuerme.Gewehrturm.LP"), 3000.f },
		{ TEXT("Tuerme.Flammenwerfer.Reichweite"), 20.f }, { TEXT("Tuerme.Flammenwerfer.LP"), 3000.f },
		{ TEXT("Tuerme.Teslaspule.Reichweite"), 30.f }, { TEXT("Tuerme.Teslaspule.LP"), 3000.f },
		{ TEXT("Tuerme.Nobelisk-Werfer.Reichweite"), 60.f }, { TEXT("Tuerme.Nobelisk-Werfer.LP"), 3000.f },
		{ TEXT("Tuerme.Reparaturstation.Reichweite"), 25.f }, { TEXT("Tuerme.Reparaturstation.LP"), 3000.f },
		// 0.81 passive Bauten P1: Lockfeuer (lockt Gegner in Radius m, Pause s nach jedem Lockruf), Oelschlick (Bremse = Tempofaktor)
		{ TEXT("Tuerme.Lockfeuer.Reichweite"), 40.f }, { TEXT("Tuerme.Lockfeuer.LP"), 2000.f }, { TEXT("Tuerme.Lockfeuer.Radius"), 40.f }, { TEXT("Tuerme.Lockfeuer.Pause"), 30.f },
		{ TEXT("Tuerme.Oelschlick.Reichweite"), 6.5f }, { TEXT("Tuerme.Oelschlick.LP"), 3000.f }, { TEXT("Tuerme.Oelschlick.Bremse"), 0.65f }, { TEXT("Tuerme.Oelschlick.Ausloserand"), 2.f },
		{ TEXT("Tuerme.Oelschlick.BrandSchaden"), 20.f }, { TEXT("Tuerme.Oelschlick.BrandDauer"), 5.f }, { TEXT("Tuerme.Oelschlick.Nachlauf"), 3.f },
		// 0.9.1 Oelbrand als Kombo (Review 09.11.): wer durch die brennende Platte laeuft, brennt danach weiter; Lockfeuer zuendet Platten im Umkreis
		{ TEXT("Tuerme.Oelschlick.Nachbrennen"), 10.f }, { TEXT("Tuerme.Oelschlick.NachbrennDauer"), 3.f }, { TEXT("Tuerme.Lockfeuer.Zuendradius"), 12.f },
		// 0.83 Bauten P2/P3: Reichweite m, LP; Wirkung je Bau (Faktoren: Bremse = Tempofaktor, Bonus = Anteil)
		{ TEXT("Tuerme.Elektrozaun.Reichweite"), 15.f }, { TEXT("Tuerme.Elektrozaun.LP"), 4000.f }, { TEXT("Tuerme.Elektrozaun.Schaden"), 15.f }, { TEXT("Tuerme.Elektrozaun.Betaeubung"), 1.f },
		{ TEXT("Tuerme.Funkmast.Reichweite"), 60.f }, { TEXT("Tuerme.Funkmast.LP"), 2500.f }, { TEXT("Tuerme.Funkmast.Radius"), 30.f }, { TEXT("Tuerme.Funkmast.Bonus"), 0.1f },
		{ TEXT("Tuerme.Schildgenerator.Reichweite"), 20.f }, { TEXT("Tuerme.Schildgenerator.LP"), 3500.f }, { TEXT("Tuerme.Schildgenerator.Schild"), 3000.f },
		{ TEXT("Tuerme.Schildgenerator.Radius"), 20.f }, { TEXT("Tuerme.Schildgenerator.Ladezeit"), 20.f },
		{ TEXT("Tuerme.Himmelsturm.Reichweite"), 60.f }, { TEXT("Tuerme.Himmelsturm.LP"), 3000.f }, { TEXT("Tuerme.Himmelsturm.Schaden"), 200.f }, { TEXT("Tuerme.Himmelsturm.Radius"), 6.f },
		{ TEXT("Tuerme.Stoerfeld.Reichweite"), 15.f }, { TEXT("Tuerme.Stoerfeld.LP"), 3500.f }, { TEXT("Tuerme.Stoerfeld.Bremse"), 0.6f },
		{ TEXT("Tuerme.Singularitaet.Reichweite"), 50.f }, { TEXT("Tuerme.Singularitaet.LP"), 5000.f }, { TEXT("Tuerme.Singularitaet.Schaden"), 1500.f },
		{ TEXT("Tuerme.Singularitaet.Radius"), 12.f }, { TEXT("Tuerme.Singularitaet.Sogzeit"), 1.5f }, { TEXT("Tuerme.Singularitaet.Bremse"), 0.05f },
		{ TEXT("Bauten.Dimensionslager.Radius"), 30.f }, { TEXT("Bauten.Dimensionslager.Takt"), 1.f }, { TEXT("Bauten.Dimensionslager.JeTurm"), 10.f },
		{ TEXT("Bauten.Tor.LP"), 3000.f }, { TEXT("Bauten.Tor.Oeffnen"), 6.f }, { TEXT("Bauten.Tor.GegnerSperre"), 15.f },
		// Turmlevel (0.63): Preis je Level in EP; was jedes Level bringt (Anteil 0.2 = +20 %, Durchschlag/Spruenge Anzahl, Magazin 1 = x2)
		{ TEXT("Tuerme.Level.Preis2"), 8.f }, { TEXT("Tuerme.Level.Preis3"), 15.f }, { TEXT("Tuerme.Level.Preis4"), 25.f }, { TEXT("Tuerme.Level.Preis5"), 40.f },
		{ TEXT("Tuerme.Level.Gewehrturm.L2.Schaden"), 0.2f }, { TEXT("Tuerme.Level.Gewehrturm.L3.Takt"), 0.3f }, { TEXT("Tuerme.Level.Gewehrturm.L4.Magazin"), 1.0f }, { TEXT("Tuerme.Level.Gewehrturm.L4.Reichweite"), 0.15f },
		{ TEXT("Tuerme.Level.Bolzenturm.L2.Schaden"), 0.2f }, { TEXT("Tuerme.Level.Bolzenturm.L3.Takt"), 0.25f }, { TEXT("Tuerme.Level.Bolzenturm.L4.Reichweite"), 0.15f }, { TEXT("Tuerme.Level.Bolzenturm.L4.Durchschlag"), 1.0f },
		{ TEXT("Tuerme.Level.Nobelisk-Werfer.L2.Schaden"), 0.2f }, { TEXT("Tuerme.Level.Nobelisk-Werfer.L3.Takt"), 0.25f }, { TEXT("Tuerme.Level.Nobelisk-Werfer.L4.Reichweite"), 0.15f }, { TEXT("Tuerme.Level.Nobelisk-Werfer.L4.Radius"), 0.2f },
		{ TEXT("Tuerme.Level.Teslaspule.L2.Reichweite"), 0.1f }, { TEXT("Tuerme.Level.Teslaspule.L3.Spruenge"), 1.0f }, { TEXT("Tuerme.Level.Teslaspule.L3.Schaden"), 0.2f }, { TEXT("Tuerme.Level.Teslaspule.L4.Takt"), 0.3f },
		{ TEXT("Tuerme.Level.Flammenwerfer.L2.Schaden"), 0.2f }, { TEXT("Tuerme.Level.Flammenwerfer.L3.Kegel"), 0.5f }, { TEXT("Tuerme.Level.Flammenwerfer.L4.Reichweite"), 0.15f }, { TEXT("Tuerme.Level.Flammenwerfer.L4.Nachbrennen"), 0.5f },
		{ TEXT("Tuerme.Bolzenturm.Startmunition"), 20.f }, { TEXT("Tuerme.Gewehrturm.Startmunition"), 20.f },
		// 0.78 (Pruefstand: "Schluessel fehlt"): Startmunition der uebrigen Tuerme, Flammen-Eigenschaften des Treibstoffs
		{ TEXT("Tuerme.Teslaspule.Startmunition"), 0.f }, { TEXT("Tuerme.Nobelisk-Werfer.Startmunition"), 0.f }, { TEXT("Tuerme.Flammenwerfer.Startmunition"), 0.f },
		// 0.83 (Selbsttest BautenP1 0.82: "Schluessel fehlt"): Startmunition der Bauten, Pruefung jetzt in pruefe_mod.py
		{ TEXT("Tuerme.Reparaturstation.Startmunition"), 0.f }, { TEXT("Tuerme.Lockfeuer.Startmunition"), 0.f }, { TEXT("Tuerme.Oelschlick.Startmunition"), 0.f }, { TEXT("Tuerme.Elektrozaun.Startmunition"), 0.f }, { TEXT("Tuerme.Funkmast.Startmunition"), 0.f }, { TEXT("Tuerme.Schildgenerator.Startmunition"), 0.f }, { TEXT("Tuerme.Himmelsturm.Startmunition"), 0.f }, { TEXT("Tuerme.Stoerfeld.Startmunition"), 0.f }, { TEXT("Tuerme.Singularitaet.Startmunition"), 0.f },
		{ TEXT("Munition.Treibstoff.Treibstoff.Kegel"), 1.f }, { TEXT("Munition.Treibstoff.Treibstoff.Betaeubung"), 0.f },   // 0.65: Gewehrturm hatte keine
		// 0.83: bisherige Code-Standardwerte als Tabelleneintrag (Warnung Gesamt082), keine Balance-Aenderung
		{ TEXT("Munition.Treibstoff.Biotreibstoff.Kegel"), 1.4f }, { TEXT("Munition.Treibstoff.Biotreibstoff.Betaeubung"), 0.f },
		{ TEXT("Munition.Treibstoff.Turbotreibstoff.Kegel"), 0.9f }, { TEXT("Munition.Treibstoff.Turbotreibstoff.Betaeubung"), 0.f },
		{ TEXT("Munition.Treibstoff.Raketentreibstoff.Kegel"), 0.75f }, { TEXT("Munition.Treibstoff.Raketentreibstoff.Betaeubung"), 0.f },
		{ TEXT("Munition.Treibstoff.Ionisiert.Kegel"), 1.f }, { TEXT("Munition.Treibstoff.Ionisiert.Betaeubung"), 0.3f },

		// ---- Munition ----
		{ TEXT("Munition.Bolzen.Armierungsbolzen.Schaden"), 40.f },
		// Test 0.56: Durchschlag machte die Wellen wieder leicht - jeder weitere Gegner bekommt nur noch Folgeschaden x Schaden des vorigen
		{ TEXT("Munition.Bolzen.Durchschlagbolzen.Schaden"), 32.f }, { TEXT("Munition.Bolzen.Durchschlagbolzen.WeitereZiele"), 2.f },
		{ TEXT("Munition.Bolzen.Durchschlagbolzen.Folgeschaden"), 0.5f },
		{ TEXT("Munition.Bolzen.Explosivbolzen.Schaden"), 30.f }, { TEXT("Munition.Bolzen.Explosivbolzen.Flaechenschaden"), 87.f }, { TEXT("Munition.Bolzen.Explosivbolzen.Radius"), 4.5f },
		{ TEXT("Munition.Bolzen.Betaeubungsbolzen.Schaden"), 59.f }, { TEXT("Munition.Bolzen.Betaeubungsbolzen.Betaeubung"), 3.f },
		{ TEXT("Munition.Bolzen.Streubolzen.Schaden"), 43.f }, { TEXT("Munition.Bolzen.Streubolzen.Splitter"), 3.f },
		{ TEXT("Munition.Bolzen.Streubolzen.Splitterschaden"), 43.f }, { TEXT("Munition.Bolzen.Streubolzen.Splitterradius"), 6.f },
		{ TEXT("Munition.Patronen.Standard.Schaden"), 12.f }, { TEXT("Munition.Patronen.Standard.SchussProSekunde"), 6.f }, { TEXT("Munition.Patronen.Standard.Magazin"), 15.f },
		{ TEXT("Munition.Patronen.Zielsuch.Schaden"), 20.f }, { TEXT("Munition.Patronen.Zielsuch.SchussProSekunde"), 5.f }, { TEXT("Munition.Patronen.Zielsuch.Magazin"), 20.f },
		{ TEXT("Munition.Patronen.Turbo.Schaden"), 15.f }, { TEXT("Munition.Patronen.Turbo.SchussProSekunde"), 12.f }, { TEXT("Munition.Patronen.Turbo.Magazin"), 75.f },
		{ TEXT("Munition.Treibstoff.Treibstoff.SchadenProSekunde"), 80.f }, { TEXT("Munition.Treibstoff.Treibstoff.Nachbrennen"), 10.f }, { TEXT("Munition.Treibstoff.Treibstoff.NachbrennDauer"), 3.f },
		{ TEXT("Munition.Treibstoff.Biotreibstoff.SchadenProSekunde"), 35.f }, { TEXT("Munition.Treibstoff.Biotreibstoff.Nachbrennen"), 8.f }, { TEXT("Munition.Treibstoff.Biotreibstoff.NachbrennDauer"), 2.f },
		{ TEXT("Munition.Treibstoff.Turbotreibstoff.SchadenProSekunde"), 120.f }, { TEXT("Munition.Treibstoff.Turbotreibstoff.Nachbrennen"), 15.f }, { TEXT("Munition.Treibstoff.Turbotreibstoff.NachbrennDauer"), 4.f },
		{ TEXT("Munition.Treibstoff.Raketentreibstoff.SchadenProSekunde"), 140.f }, { TEXT("Munition.Treibstoff.Raketentreibstoff.Nachbrennen"), 20.f }, { TEXT("Munition.Treibstoff.Raketentreibstoff.NachbrennDauer"), 3.f },
		{ TEXT("Munition.Treibstoff.Ionisiert.SchadenProSekunde"), 160.f }, { TEXT("Munition.Treibstoff.Ionisiert.Nachbrennen"), 25.f }, { TEXT("Munition.Treibstoff.Ionisiert.NachbrennDauer"), 5.f },
		{ TEXT("Munition.Nobelisk.Nobelisk.Schaden"), 150.f }, { TEXT("Munition.Nobelisk.Nobelisk.Radius"), 6.f },
		{ TEXT("Munition.Nobelisk.Gas.Schaden"), 20.f }, { TEXT("Munition.Nobelisk.Gas.Radius"), 6.f }, { TEXT("Munition.Nobelisk.Gas.GasSchaden"), 20.f }, { TEXT("Munition.Nobelisk.Gas.GasDauer"), 10.f },
		{ TEXT("Munition.Nobelisk.Puls.Schaden"), 90.f }, { TEXT("Munition.Nobelisk.Puls.Radius"), 8.f }, { TEXT("Munition.Nobelisk.Puls.Betaeubung"), 2.f },
		{ TEXT("Munition.Nobelisk.Streu.Schaden"), 225.f }, { TEXT("Munition.Nobelisk.Streu.Radius"), 4.f }, { TEXT("Munition.Nobelisk.Streu.Streu"), 5.f },
		{ TEXT("Munition.Nobelisk.Nuke.Schaden"), 2000.f }, { TEXT("Munition.Nobelisk.Nuke.Radius"), 25.f },
		{ TEXT("Munition.Tesla.Schaden"), 300.f }, { TEXT("Munition.Tesla.Spruenge"), 3.f }, { TEXT("Munition.Tesla.Sprungweite"), 8.f }, { TEXT("Munition.Tesla.SprungFaktor"), 0.35f },
		{ TEXT("Munition.Reparatur.Eisenplatte"), 800.f }, { TEXT("Munition.Reparatur.Stahltraeger"), 2000.f }, { TEXT("Munition.Reparatur.VerstaerktePlatte"), 3000.f },

		// ---- T4 Huelle allgemein: Rand = Fresnel-Exponent (groesser = schmalerer Leuchtrand), Puls = Ausschlag des Boss-Pulsierens (0..1) ----
		{ TEXT("Optik.Rand"), 4.5f }, { TEXT("Optik.Puls"), 0.3f },

		// ---- T4 Optik je Gegnerart: Elite und Boss je Groesse, Leuchtstaerke der Huelle und Farbe (R/G/B 0..1) ----
		{ TEXT("Optik.Hog.Elite.Groesse"), 1.3f }, { TEXT("Optik.Hog.Elite.Leuchten"), 0.5f }, { TEXT("Optik.Hog.Elite.R"), 0.55f }, { TEXT("Optik.Hog.Elite.G"), 0.35f }, { TEXT("Optik.Hog.Elite.B"), 1.0f },
		{ TEXT("Optik.Hog.Boss.Groesse"), 1.8f }, { TEXT("Optik.Hog.Boss.Leuchten"), 0.5f }, { TEXT("Optik.Hog.Boss.R"), 1.0f }, { TEXT("Optik.Hog.Boss.G"), 0.18f }, { TEXT("Optik.Hog.Boss.B"), 0.08f },
		{ TEXT("Optik.AlphaHog.Elite.Groesse"), 1.3f }, { TEXT("Optik.AlphaHog.Elite.Leuchten"), 0.5f }, { TEXT("Optik.AlphaHog.Elite.R"), 0.55f }, { TEXT("Optik.AlphaHog.Elite.G"), 0.35f }, { TEXT("Optik.AlphaHog.Elite.B"), 1.0f },
		{ TEXT("Optik.AlphaHog.Boss.Groesse"), 1.8f }, { TEXT("Optik.AlphaHog.Boss.Leuchten"), 0.5f }, { TEXT("Optik.AlphaHog.Boss.R"), 1.0f }, { TEXT("Optik.AlphaHog.Boss.G"), 0.18f }, { TEXT("Optik.AlphaHog.Boss.B"), 0.08f },
		{ TEXT("Optik.CliffHog.Elite.Groesse"), 1.25f }, { TEXT("Optik.CliffHog.Elite.Leuchten"), 0.5f }, { TEXT("Optik.CliffHog.Elite.R"), 0.55f }, { TEXT("Optik.CliffHog.Elite.G"), 0.35f }, { TEXT("Optik.CliffHog.Elite.B"), 1.0f },
		{ TEXT("Optik.CliffHog.Boss.Groesse"), 1.7f }, { TEXT("Optik.CliffHog.Boss.Leuchten"), 0.5f }, { TEXT("Optik.CliffHog.Boss.R"), 1.0f }, { TEXT("Optik.CliffHog.Boss.G"), 0.18f }, { TEXT("Optik.CliffHog.Boss.B"), 0.08f },
		{ TEXT("Optik.NuclearHog.Elite.Groesse"), 1.25f }, { TEXT("Optik.NuclearHog.Elite.Leuchten"), 0.5f }, { TEXT("Optik.NuclearHog.Elite.R"), 0.55f }, { TEXT("Optik.NuclearHog.Elite.G"), 0.35f }, { TEXT("Optik.NuclearHog.Elite.B"), 1.0f },
		{ TEXT("Optik.NuclearHog.Boss.Groesse"), 1.7f }, { TEXT("Optik.NuclearHog.Boss.Leuchten"), 0.5f }, { TEXT("Optik.NuclearHog.Boss.R"), 1.0f }, { TEXT("Optik.NuclearHog.Boss.G"), 0.18f }, { TEXT("Optik.NuclearHog.Boss.B"), 0.08f },
		{ TEXT("Optik.Johnny.Elite.Groesse"), 1.0f }, { TEXT("Optik.Johnny.Elite.Leuchten"), 0.5f }, { TEXT("Optik.Johnny.Elite.R"), 0.55f }, { TEXT("Optik.Johnny.Elite.G"), 0.35f }, { TEXT("Optik.Johnny.Elite.B"), 1.0f },
		{ TEXT("Optik.Johnny.Boss.Groesse"), 1.3f }, { TEXT("Optik.Johnny.Boss.Leuchten"), 0.5f }, { TEXT("Optik.Johnny.Boss.R"), 1.0f }, { TEXT("Optik.Johnny.Boss.G"), 0.45f }, { TEXT("Optik.Johnny.Boss.B"), 0.05f },
		{ TEXT("Optik.Spitter.Elite.Groesse"), 1.3f }, { TEXT("Optik.Spitter.Elite.Leuchten"), 0.5f }, { TEXT("Optik.Spitter.Elite.R"), 0.55f }, { TEXT("Optik.Spitter.Elite.G"), 0.35f }, { TEXT("Optik.Spitter.Elite.B"), 1.0f },
		{ TEXT("Optik.Spitter.Boss.Groesse"), 1.8f }, { TEXT("Optik.Spitter.Boss.Leuchten"), 0.5f }, { TEXT("Optik.Spitter.Boss.R"), 1.0f }, { TEXT("Optik.Spitter.Boss.G"), 0.18f }, { TEXT("Optik.Spitter.Boss.B"), 0.08f },
		{ TEXT("Optik.SpitterAlpha.Elite.Groesse"), 1.25f }, { TEXT("Optik.SpitterAlpha.Elite.Leuchten"), 0.5f }, { TEXT("Optik.SpitterAlpha.Elite.R"), 0.55f }, { TEXT("Optik.SpitterAlpha.Elite.G"), 0.35f }, { TEXT("Optik.SpitterAlpha.Elite.B"), 1.0f },
		{ TEXT("Optik.SpitterAlpha.Boss.Groesse"), 1.7f }, { TEXT("Optik.SpitterAlpha.Boss.Leuchten"), 0.5f }, { TEXT("Optik.SpitterAlpha.Boss.R"), 1.0f }, { TEXT("Optik.SpitterAlpha.Boss.G"), 0.18f }, { TEXT("Optik.SpitterAlpha.Boss.B"), 0.08f },
		{ TEXT("Optik.WasserSpitter.Elite.Groesse"), 1.25f }, { TEXT("Optik.WasserSpitter.Elite.Leuchten"), 0.5f }, { TEXT("Optik.WasserSpitter.Elite.R"), 0.55f }, { TEXT("Optik.WasserSpitter.Elite.G"), 0.35f }, { TEXT("Optik.WasserSpitter.Elite.B"), 1.0f },
		{ TEXT("Optik.WasserSpitter.Boss.Groesse"), 1.7f }, { TEXT("Optik.WasserSpitter.Boss.Leuchten"), 0.5f }, { TEXT("Optik.WasserSpitter.Boss.R"), 1.0f }, { TEXT("Optik.WasserSpitter.Boss.G"), 0.18f }, { TEXT("Optik.WasserSpitter.Boss.B"), 0.08f },
		{ TEXT("Optik.StingerKlein.Elite.Groesse"), 1.4f }, { TEXT("Optik.StingerKlein.Elite.Leuchten"), 0.5f }, { TEXT("Optik.StingerKlein.Elite.R"), 0.55f }, { TEXT("Optik.StingerKlein.Elite.G"), 0.35f }, { TEXT("Optik.StingerKlein.Elite.B"), 1.0f },
		{ TEXT("Optik.StingerKlein.Boss.Groesse"), 2.0f }, { TEXT("Optik.StingerKlein.Boss.Leuchten"), 0.5f }, { TEXT("Optik.StingerKlein.Boss.R"), 1.0f }, { TEXT("Optik.StingerKlein.Boss.G"), 0.18f }, { TEXT("Optik.StingerKlein.Boss.B"), 0.08f },
		{ TEXT("Optik.Stinger.Elite.Groesse"), 1.3f }, { TEXT("Optik.Stinger.Elite.Leuchten"), 0.5f }, { TEXT("Optik.Stinger.Elite.R"), 0.55f }, { TEXT("Optik.Stinger.Elite.G"), 0.35f }, { TEXT("Optik.Stinger.Elite.B"), 1.0f },
		{ TEXT("Optik.Stinger.Boss.Groesse"), 1.8f }, { TEXT("Optik.Stinger.Boss.Leuchten"), 0.5f }, { TEXT("Optik.Stinger.Boss.R"), 1.0f }, { TEXT("Optik.Stinger.Boss.G"), 0.18f }, { TEXT("Optik.Stinger.Boss.B"), 0.08f },
		{ TEXT("Optik.StingerElite.Elite.Groesse"), 1.2f }, { TEXT("Optik.StingerElite.Elite.Leuchten"), 0.5f }, { TEXT("Optik.StingerElite.Elite.R"), 0.55f }, { TEXT("Optik.StingerElite.Elite.G"), 0.35f }, { TEXT("Optik.StingerElite.Elite.B"), 1.0f },
		{ TEXT("Optik.StingerElite.Boss.Groesse"), 1.6f }, { TEXT("Optik.StingerElite.Boss.Leuchten"), 0.5f }, { TEXT("Optik.StingerElite.Boss.R"), 1.0f }, { TEXT("Optik.StingerElite.Boss.G"), 0.18f }, { TEXT("Optik.StingerElite.Boss.B"), 0.08f },

		// ---- T4c Faerbung je Gegnerart: Ton = Staerke der Toenungsschicht 0..1 (0 = aus) mit Farbe TonR/G/B; Fell = 1 faerbt das Fell (Materialwert HairColor) in FellR/G/B ----
		{ TEXT("Optik.Hog.Elite.Ton"), 0.25f }, { TEXT("Optik.Hog.Elite.TonR"), 0.7f }, { TEXT("Optik.Hog.Elite.TonG"), 0.65f }, { TEXT("Optik.Hog.Elite.TonB"), 1.0f }, { TEXT("Optik.Hog.Elite.Fell"), 0.f }, { TEXT("Optik.Hog.Elite.FellR"), 0.6f }, { TEXT("Optik.Hog.Elite.FellG"), 0.55f }, { TEXT("Optik.Hog.Elite.FellB"), 0.9f },
		{ TEXT("Optik.Hog.Boss.Ton"), 0.35f }, { TEXT("Optik.Hog.Boss.TonR"), 0.8f }, { TEXT("Optik.Hog.Boss.TonG"), 0.35f }, { TEXT("Optik.Hog.Boss.TonB"), 0.22f }, { TEXT("Optik.Hog.Boss.Fell"), 1.f }, { TEXT("Optik.Hog.Boss.FellR"), 0.45f }, { TEXT("Optik.Hog.Boss.FellG"), 0.12f }, { TEXT("Optik.Hog.Boss.FellB"), 0.06f },
		{ TEXT("Optik.AlphaHog.Elite.Ton"), 0.25f }, { TEXT("Optik.AlphaHog.Elite.TonR"), 0.7f }, { TEXT("Optik.AlphaHog.Elite.TonG"), 0.65f }, { TEXT("Optik.AlphaHog.Elite.TonB"), 1.0f }, { TEXT("Optik.AlphaHog.Elite.Fell"), 0.f }, { TEXT("Optik.AlphaHog.Elite.FellR"), 0.6f }, { TEXT("Optik.AlphaHog.Elite.FellG"), 0.55f }, { TEXT("Optik.AlphaHog.Elite.FellB"), 0.9f },
		{ TEXT("Optik.AlphaHog.Boss.Ton"), 0.35f }, { TEXT("Optik.AlphaHog.Boss.TonR"), 0.8f }, { TEXT("Optik.AlphaHog.Boss.TonG"), 0.35f }, { TEXT("Optik.AlphaHog.Boss.TonB"), 0.22f }, { TEXT("Optik.AlphaHog.Boss.Fell"), 1.f }, { TEXT("Optik.AlphaHog.Boss.FellR"), 0.45f }, { TEXT("Optik.AlphaHog.Boss.FellG"), 0.12f }, { TEXT("Optik.AlphaHog.Boss.FellB"), 0.06f },
		{ TEXT("Optik.CliffHog.Elite.Ton"), 0.25f }, { TEXT("Optik.CliffHog.Elite.TonR"), 0.7f }, { TEXT("Optik.CliffHog.Elite.TonG"), 0.65f }, { TEXT("Optik.CliffHog.Elite.TonB"), 1.0f }, { TEXT("Optik.CliffHog.Elite.Fell"), 0.f }, { TEXT("Optik.CliffHog.Elite.FellR"), 0.6f }, { TEXT("Optik.CliffHog.Elite.FellG"), 0.55f }, { TEXT("Optik.CliffHog.Elite.FellB"), 0.9f },
		{ TEXT("Optik.CliffHog.Boss.Ton"), 0.35f }, { TEXT("Optik.CliffHog.Boss.TonR"), 0.8f }, { TEXT("Optik.CliffHog.Boss.TonG"), 0.35f }, { TEXT("Optik.CliffHog.Boss.TonB"), 0.22f }, { TEXT("Optik.CliffHog.Boss.Fell"), 1.f }, { TEXT("Optik.CliffHog.Boss.FellR"), 0.45f }, { TEXT("Optik.CliffHog.Boss.FellG"), 0.12f }, { TEXT("Optik.CliffHog.Boss.FellB"), 0.06f },
		{ TEXT("Optik.NuclearHog.Elite.Ton"), 0.25f }, { TEXT("Optik.NuclearHog.Elite.TonR"), 0.7f }, { TEXT("Optik.NuclearHog.Elite.TonG"), 0.65f }, { TEXT("Optik.NuclearHog.Elite.TonB"), 1.0f }, { TEXT("Optik.NuclearHog.Elite.Fell"), 0.f }, { TEXT("Optik.NuclearHog.Elite.FellR"), 0.6f }, { TEXT("Optik.NuclearHog.Elite.FellG"), 0.55f }, { TEXT("Optik.NuclearHog.Elite.FellB"), 0.9f },
		{ TEXT("Optik.NuclearHog.Boss.Ton"), 0.35f }, { TEXT("Optik.NuclearHog.Boss.TonR"), 0.8f }, { TEXT("Optik.NuclearHog.Boss.TonG"), 0.35f }, { TEXT("Optik.NuclearHog.Boss.TonB"), 0.22f }, { TEXT("Optik.NuclearHog.Boss.Fell"), 1.f }, { TEXT("Optik.NuclearHog.Boss.FellR"), 0.45f }, { TEXT("Optik.NuclearHog.Boss.FellG"), 0.12f }, { TEXT("Optik.NuclearHog.Boss.FellB"), 0.06f },
		{ TEXT("Optik.Johnny.Elite.Ton"), 0.25f }, { TEXT("Optik.Johnny.Elite.TonR"), 0.7f }, { TEXT("Optik.Johnny.Elite.TonG"), 0.65f }, { TEXT("Optik.Johnny.Elite.TonB"), 1.0f }, { TEXT("Optik.Johnny.Elite.Fell"), 0.f }, { TEXT("Optik.Johnny.Elite.FellR"), 0.6f }, { TEXT("Optik.Johnny.Elite.FellG"), 0.55f }, { TEXT("Optik.Johnny.Elite.FellB"), 0.9f },
		{ TEXT("Optik.Johnny.Boss.Ton"), 0.25f }, { TEXT("Optik.Johnny.Boss.TonR"), 0.85f }, { TEXT("Optik.Johnny.Boss.TonG"), 0.55f }, { TEXT("Optik.Johnny.Boss.TonB"), 0.3f }, { TEXT("Optik.Johnny.Boss.Fell"), 1.f }, { TEXT("Optik.Johnny.Boss.FellR"), 0.5f }, { TEXT("Optik.Johnny.Boss.FellG"), 0.2f }, { TEXT("Optik.Johnny.Boss.FellB"), 0.05f },
		{ TEXT("Optik.Spitter.Elite.Ton"), 0.25f }, { TEXT("Optik.Spitter.Elite.TonR"), 0.7f }, { TEXT("Optik.Spitter.Elite.TonG"), 0.65f }, { TEXT("Optik.Spitter.Elite.TonB"), 1.0f }, { TEXT("Optik.Spitter.Elite.Fell"), 0.f }, { TEXT("Optik.Spitter.Elite.FellR"), 0.6f }, { TEXT("Optik.Spitter.Elite.FellG"), 0.55f }, { TEXT("Optik.Spitter.Elite.FellB"), 0.9f },
		{ TEXT("Optik.Spitter.Boss.Ton"), 0.35f }, { TEXT("Optik.Spitter.Boss.TonR"), 0.8f }, { TEXT("Optik.Spitter.Boss.TonG"), 0.35f }, { TEXT("Optik.Spitter.Boss.TonB"), 0.22f }, { TEXT("Optik.Spitter.Boss.Fell"), 1.f }, { TEXT("Optik.Spitter.Boss.FellR"), 0.45f }, { TEXT("Optik.Spitter.Boss.FellG"), 0.12f }, { TEXT("Optik.Spitter.Boss.FellB"), 0.06f },
		{ TEXT("Optik.SpitterAlpha.Elite.Ton"), 0.25f }, { TEXT("Optik.SpitterAlpha.Elite.TonR"), 0.7f }, { TEXT("Optik.SpitterAlpha.Elite.TonG"), 0.65f }, { TEXT("Optik.SpitterAlpha.Elite.TonB"), 1.0f }, { TEXT("Optik.SpitterAlpha.Elite.Fell"), 0.f }, { TEXT("Optik.SpitterAlpha.Elite.FellR"), 0.6f }, { TEXT("Optik.SpitterAlpha.Elite.FellG"), 0.55f }, { TEXT("Optik.SpitterAlpha.Elite.FellB"), 0.9f },
		{ TEXT("Optik.SpitterAlpha.Boss.Ton"), 0.35f }, { TEXT("Optik.SpitterAlpha.Boss.TonR"), 0.8f }, { TEXT("Optik.SpitterAlpha.Boss.TonG"), 0.35f }, { TEXT("Optik.SpitterAlpha.Boss.TonB"), 0.22f }, { TEXT("Optik.SpitterAlpha.Boss.Fell"), 1.f }, { TEXT("Optik.SpitterAlpha.Boss.FellR"), 0.45f }, { TEXT("Optik.SpitterAlpha.Boss.FellG"), 0.12f }, { TEXT("Optik.SpitterAlpha.Boss.FellB"), 0.06f },
		{ TEXT("Optik.WasserSpitter.Elite.Ton"), 0.25f }, { TEXT("Optik.WasserSpitter.Elite.TonR"), 0.7f }, { TEXT("Optik.WasserSpitter.Elite.TonG"), 0.65f }, { TEXT("Optik.WasserSpitter.Elite.TonB"), 1.0f }, { TEXT("Optik.WasserSpitter.Elite.Fell"), 0.f }, { TEXT("Optik.WasserSpitter.Elite.FellR"), 0.6f }, { TEXT("Optik.WasserSpitter.Elite.FellG"), 0.55f }, { TEXT("Optik.WasserSpitter.Elite.FellB"), 0.9f },
		{ TEXT("Optik.WasserSpitter.Boss.Ton"), 0.35f }, { TEXT("Optik.WasserSpitter.Boss.TonR"), 0.8f }, { TEXT("Optik.WasserSpitter.Boss.TonG"), 0.35f }, { TEXT("Optik.WasserSpitter.Boss.TonB"), 0.22f }, { TEXT("Optik.WasserSpitter.Boss.Fell"), 1.f }, { TEXT("Optik.WasserSpitter.Boss.FellR"), 0.45f }, { TEXT("Optik.WasserSpitter.Boss.FellG"), 0.12f }, { TEXT("Optik.WasserSpitter.Boss.FellB"), 0.06f },
		{ TEXT("Optik.StingerKlein.Elite.Ton"), 0.25f }, { TEXT("Optik.StingerKlein.Elite.TonR"), 0.7f }, { TEXT("Optik.StingerKlein.Elite.TonG"), 0.65f }, { TEXT("Optik.StingerKlein.Elite.TonB"), 1.0f }, { TEXT("Optik.StingerKlein.Elite.Fell"), 0.f }, { TEXT("Optik.StingerKlein.Elite.FellR"), 0.6f }, { TEXT("Optik.StingerKlein.Elite.FellG"), 0.55f }, { TEXT("Optik.StingerKlein.Elite.FellB"), 0.9f },
		{ TEXT("Optik.StingerKlein.Boss.Ton"), 0.35f }, { TEXT("Optik.StingerKlein.Boss.TonR"), 0.8f }, { TEXT("Optik.StingerKlein.Boss.TonG"), 0.35f }, { TEXT("Optik.StingerKlein.Boss.TonB"), 0.22f }, { TEXT("Optik.StingerKlein.Boss.Fell"), 1.f }, { TEXT("Optik.StingerKlein.Boss.FellR"), 0.45f }, { TEXT("Optik.StingerKlein.Boss.FellG"), 0.12f }, { TEXT("Optik.StingerKlein.Boss.FellB"), 0.06f },
		{ TEXT("Optik.Stinger.Elite.Ton"), 0.25f }, { TEXT("Optik.Stinger.Elite.TonR"), 0.7f }, { TEXT("Optik.Stinger.Elite.TonG"), 0.65f }, { TEXT("Optik.Stinger.Elite.TonB"), 1.0f }, { TEXT("Optik.Stinger.Elite.Fell"), 0.f }, { TEXT("Optik.Stinger.Elite.FellR"), 0.6f }, { TEXT("Optik.Stinger.Elite.FellG"), 0.55f }, { TEXT("Optik.Stinger.Elite.FellB"), 0.9f },
		{ TEXT("Optik.Stinger.Boss.Ton"), 0.35f }, { TEXT("Optik.Stinger.Boss.TonR"), 0.8f }, { TEXT("Optik.Stinger.Boss.TonG"), 0.35f }, { TEXT("Optik.Stinger.Boss.TonB"), 0.22f }, { TEXT("Optik.Stinger.Boss.Fell"), 1.f }, { TEXT("Optik.Stinger.Boss.FellR"), 0.45f }, { TEXT("Optik.Stinger.Boss.FellG"), 0.12f }, { TEXT("Optik.Stinger.Boss.FellB"), 0.06f },
		{ TEXT("Optik.StingerElite.Elite.Ton"), 0.25f }, { TEXT("Optik.StingerElite.Elite.TonR"), 0.7f }, { TEXT("Optik.StingerElite.Elite.TonG"), 0.65f }, { TEXT("Optik.StingerElite.Elite.TonB"), 1.0f }, { TEXT("Optik.StingerElite.Elite.Fell"), 0.f }, { TEXT("Optik.StingerElite.Elite.FellR"), 0.6f }, { TEXT("Optik.StingerElite.Elite.FellG"), 0.55f }, { TEXT("Optik.StingerElite.Elite.FellB"), 0.9f },
		{ TEXT("Optik.StingerElite.Boss.Ton"), 0.35f }, { TEXT("Optik.StingerElite.Boss.TonR"), 0.8f }, { TEXT("Optik.StingerElite.Boss.TonG"), 0.35f }, { TEXT("Optik.StingerElite.Boss.TonB"), 0.22f }, { TEXT("Optik.StingerElite.Boss.Fell"), 1.f }, { TEXT("Optik.StingerElite.Boss.FellR"), 0.45f }, { TEXT("Optik.StingerElite.Boss.FellG"), 0.12f }, { TEXT("Optik.StingerElite.Boss.FellB"), 0.06f },

		// ---- Resistenzen: Faktor je Gegnerfamilie und Schadensart ----
		// 0.73 Spezialisierung (Konzept-Spezialisierung 5): weiche Resistenzen 0,75-1,3, damit jede Linie allein bestehen kann; Gift neu (immer 1)
		{ TEXT("Resistenzen.Hog.Durchschlag"), 1.0f }, { TEXT("Resistenzen.Hog.Kugel"), 0.75f }, { TEXT("Resistenzen.Hog.Feuer"), 1.0f }, { TEXT("Resistenzen.Hog.Strom"), 1.0f }, { TEXT("Resistenzen.Hog.Explosion"), 1.25f }, { TEXT("Resistenzen.Hog.Gift"), 1.0f },
		{ TEXT("Resistenzen.Spitter.Durchschlag"), 1.0f }, { TEXT("Resistenzen.Spitter.Kugel"), 1.0f }, { TEXT("Resistenzen.Spitter.Feuer"), 1.3f }, { TEXT("Resistenzen.Spitter.Strom"), 0.85f }, { TEXT("Resistenzen.Spitter.Explosion"), 1.0f }, { TEXT("Resistenzen.Spitter.Gift"), 1.0f },
		{ TEXT("Resistenzen.WasserSpitter.Durchschlag"), 1.0f }, { TEXT("Resistenzen.WasserSpitter.Kugel"), 1.0f }, { TEXT("Resistenzen.WasserSpitter.Feuer"), 0.8f }, { TEXT("Resistenzen.WasserSpitter.Strom"), 1.3f }, { TEXT("Resistenzen.WasserSpitter.Explosion"), 1.0f }, { TEXT("Resistenzen.WasserSpitter.Gift"), 1.0f },
		{ TEXT("Resistenzen.WuestenSpitter.Durchschlag"), 1.0f }, { TEXT("Resistenzen.WuestenSpitter.Kugel"), 1.2f }, { TEXT("Resistenzen.WuestenSpitter.Feuer"), 0.75f }, { TEXT("Resistenzen.WuestenSpitter.Strom"), 1.0f }, { TEXT("Resistenzen.WuestenSpitter.Explosion"), 1.0f }, { TEXT("Resistenzen.WuestenSpitter.Gift"), 1.0f },
		{ TEXT("Resistenzen.Stinger.Durchschlag"), 0.8f }, { TEXT("Resistenzen.Stinger.Kugel"), 1.25f }, { TEXT("Resistenzen.Stinger.Feuer"), 1.2f }, { TEXT("Resistenzen.Stinger.Strom"), 1.2f }, { TEXT("Resistenzen.Stinger.Explosion"), 0.85f }, { TEXT("Resistenzen.Stinger.Gift"), 1.0f },
		{ TEXT("Resistenzen.Krabbe.Durchschlag"), 0.8f }, { TEXT("Resistenzen.Krabbe.Kugel"), 1.3f }, { TEXT("Resistenzen.Krabbe.Feuer"), 1.0f }, { TEXT("Resistenzen.Krabbe.Strom"), 1.3f }, { TEXT("Resistenzen.Krabbe.Explosion"), 1.0f }, { TEXT("Resistenzen.Krabbe.Gift"), 1.0f },
		// 0.73 Spezialisierung S2-S4: neue Munition, Tesla-Gabelung, Akkubank, Doktrin
		{ TEXT("Munition.Patronen.Panzerbrechend.Schaden"), 13.f }, { TEXT("Munition.Patronen.Panzerbrechend.SchussProSekunde"), 6.f }, { TEXT("Munition.Patronen.Panzerbrechend.Magazin"), 15.f },
		{ TEXT("Munition.Patronen.Brand.Schaden"), 10.f }, { TEXT("Munition.Patronen.Brand.SchussProSekunde"), 6.f }, { TEXT("Munition.Patronen.Brand.Magazin"), 15.f }, { TEXT("Munition.Patronen.Brand.BrandSchaden"), 9.f },
		{ TEXT("Forschung.Kettenblitz.Spruenge"), 3.f }, { TEXT("Forschung.Ueberlast.Faktor"), 2.5f }, { TEXT("Forschung.Akkubank.Faktor"), 0.5f },
		{ TEXT("Doktrin.Schaden"), 0.25f }, { TEXT("Doktrin.LevelRabatt"), 0.25f },
		// 0.74 (pruefe_mod.py fand sie): bisher ohne Standardwert, im Log als "Schluessel fehlt"
		{ TEXT("Metrik.Referenzzeit"), 60.f }, { TEXT("Test.WellenSprung"), 1.f },
		{ TEXT("Einsatz.WellenStandard"), 50.f },   // 0.77 Marius: 50 Wellen sind Standard
		{ TEXT("Forschung.Brandmunition.AbWelle"), 15.f }, { TEXT("Forschung.Akkubank.AbWelle"), 15.f },
		{ TEXT("Forschung.DoktrinBolzen.AbWelle"), 15.f }, { TEXT("Forschung.DoktrinGewehr.AbWelle"), 15.f }, { TEXT("Forschung.DoktrinNobelisk.AbWelle"), 15.f },
		{ TEXT("Forschung.DoktrinTesla.AbWelle"), 15.f }, { TEXT("Forschung.DoktrinFlamme.AbWelle"), 15.f },
		{ TEXT("Loot.Splitter.Normal"), 0.f }, { TEXT("Loot.Splitter.Elite"), 0.05f }, { TEXT("Loot.Splitter.Boss"), 1.f },
		{ TEXT("Loot.SplitterMenge.Normal"), 1.f }, { TEXT("Loot.SplitterMenge.Elite"), 1.f }, { TEXT("Loot.SplitterMenge.Boss"), 2.f },

		// ---- Einsatz: Einsatzpunkte je Welle = Grund + JeWelle x N (+ Kernbonus), Bosswelle x BossFaktor ----
		{ TEXT("Einsatz.EP.Grund"), 8.f }, { TEXT("Einsatz.EP.JeWelle"), 2.f }, { TEXT("Einsatz.EP.KernVoll"), 5.f }, { TEXT("Einsatz.EP.KernAb75"), 2.f }, { TEXT("Einsatz.EP.BossFaktor"), 2.f }, { TEXT("Einsatz.EP.BossBonus"), 20.f },
		// Forschungspunkte (Item FICSIT-Kampfdaten) je Welle: Grund + Bonus bei Kern ab 75 % + Bosswelle
		{ TEXT("Einsatz.Biomasse.JeWelle"), 100.f },
		{ TEXT("Einsatz.FP.JeWelle"), 2.f }, { TEXT("Einsatz.FP.KernAb75"), 1.f }, { TEXT("Einsatz.FP.Boss"), 3.f },
		// Laden: Preis = Grundpreis x (1 + Aufschlag x bisher gekauft); Paket = Gebaeude je Kauf
		{ TEXT("Einsatz.Laden.Miner.Grundpreis"), 6.f }, { TEXT("Einsatz.Laden.Miner.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Miner.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Schmelzofen.Grundpreis"), 4.f }, { TEXT("Einsatz.Laden.Schmelzofen.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Schmelzofen.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Konstruktor.Grundpreis"), 4.f }, { TEXT("Einsatz.Laden.Konstruktor.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Konstruktor.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Assembler.Grundpreis"), 8.f }, { TEXT("Einsatz.Laden.Assembler.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Assembler.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Biomassegenerator.Grundpreis"), 6.f }, { TEXT("Einsatz.Laden.Biomassegenerator.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Biomassegenerator.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Kohlekraftwerk.Grundpreis"), 14.f }, { TEXT("Einsatz.Laden.Kohlekraftwerk.Aufschlag"), 0.6f }, { TEXT("Einsatz.Laden.Kohlekraftwerk.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Wasserpumpe.Grundpreis"), 4.f }, { TEXT("Einsatz.Laden.Wasserpumpe.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Wasserpumpe.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Lager.Grundpreis"), 4.f }, { TEXT("Einsatz.Laden.Lager.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Lager.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Bolzenturm.Grundpreis"), 4.f }, { TEXT("Einsatz.Laden.Bolzenturm.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Bolzenturm.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Gewehrturm.Grundpreis"), 6.f }, { TEXT("Einsatz.Laden.Gewehrturm.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Gewehrturm.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Teslaspule.Grundpreis"), 10.f }, { TEXT("Einsatz.Laden.Teslaspule.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Teslaspule.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Mauer.Grundpreis"), 3.f }, { TEXT("Einsatz.Laden.Mauer.Aufschlag"), 0.2f }, { TEXT("Einsatz.Laden.Mauer.Paket"), 5.f },
		{ TEXT("Einsatz.Laden.Panzersperre.Grundpreis"), 3.f }, { TEXT("Einsatz.Laden.Panzersperre.Aufschlag"), 0.2f }, { TEXT("Einsatz.Laden.Panzersperre.Paket"), 5.f },
		{ TEXT("Einsatz.Laden.Reparaturstation.Grundpreis"), 8.f }, { TEXT("Einsatz.Laden.Reparaturstation.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Reparaturstation.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Tor.Grundpreis"), 4.f }, { TEXT("Einsatz.Laden.Tor.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Tor.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Lockfeuer.Grundpreis"), 4.f }, { TEXT("Einsatz.Laden.Lockfeuer.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Lockfeuer.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Oelschlick.Grundpreis"), 4.f }, { TEXT("Einsatz.Laden.Oelschlick.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Oelschlick.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Elektrozaun.Grundpreis"), 5.f }, { TEXT("Einsatz.Laden.Elektrozaun.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Elektrozaun.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Funkmast.Grundpreis"), 8.f }, { TEXT("Einsatz.Laden.Funkmast.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Funkmast.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Schildgenerator.Grundpreis"), 12.f }, { TEXT("Einsatz.Laden.Schildgenerator.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Schildgenerator.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Himmelsturm.Grundpreis"), 10.f }, { TEXT("Einsatz.Laden.Himmelsturm.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Himmelsturm.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Stoerfeld.Grundpreis"), 14.f }, { TEXT("Einsatz.Laden.Stoerfeld.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Stoerfeld.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Singularitaet.Grundpreis"), 20.f }, { TEXT("Einsatz.Laden.Singularitaet.Aufschlag"), 0.6f }, { TEXT("Einsatz.Laden.Singularitaet.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Dimensionslager.Grundpreis"), 12.f }, { TEXT("Einsatz.Laden.Dimensionslager.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Dimensionslager.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Giesserei.Grundpreis"), 8.f }, { TEXT("Einsatz.Laden.Giesserei.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Giesserei.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Splitter.Grundpreis"), 2.f }, { TEXT("Einsatz.Laden.Splitter.Aufschlag"), 0.2f }, { TEXT("Einsatz.Laden.Splitter.Paket"), 2.f },
		{ TEXT("Einsatz.Laden.Verbinder.Grundpreis"), 2.f }, { TEXT("Einsatz.Laden.Verbinder.Aufschlag"), 0.2f }, { TEXT("Einsatz.Laden.Verbinder.Paket"), 2.f },
		{ TEXT("Einsatz.Laden.Flammenturm.Grundpreis"), 8.f }, { TEXT("Einsatz.Laden.Flammenturm.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Flammenturm.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Nobelisk-Werfer.Grundpreis"), 10.f }, { TEXT("Einsatz.Laden.Nobelisk-Werfer.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Nobelisk-Werfer.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Oelpumpe.Grundpreis"), 6.f }, { TEXT("Einsatz.Laden.Oelpumpe.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Oelpumpe.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Raffinerie.Grundpreis"), 8.f }, { TEXT("Einsatz.Laden.Raffinerie.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Raffinerie.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Inventarplätze.Grundpreis"), 6.f }, { TEXT("Einsatz.Laden.Inventarplätze.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Inventarplätze.Paket"), 5.f }, { TEXT("Einsatz.Laden.Inventarplätze.Max"), 6.f },
		{ TEXT("Einsatz.Laden.Handplatz.Grundpreis"), 8.f }, { TEXT("Einsatz.Laden.Handplatz.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Handplatz.Paket"), 1.f }, { TEXT("Einsatz.Laden.Handplatz.Max"), 3.f },
		{ TEXT("Einsatz.Laden.Armierpistole.Grundpreis"), 6.f }, { TEXT("Einsatz.Laden.Armierpistole.Aufschlag"), 0.f }, { TEXT("Einsatz.Laden.Armierpistole.Paket"), 1.f }, { TEXT("Einsatz.Laden.Armierpistole.Max"), 1.f },
		{ TEXT("Einsatz.Laden.Klingenläufer.Grundpreis"), 8.f }, { TEXT("Einsatz.Laden.Klingenläufer.Aufschlag"), 0.f }, { TEXT("Einsatz.Laden.Klingenläufer.Paket"), 1.f }, { TEXT("Einsatz.Laden.Klingenläufer.Max"), 1.f },
		{ TEXT("Einsatz.Laden.Gewehr.Grundpreis"), 10.f }, { TEXT("Einsatz.Laden.Gewehr.Aufschlag"), 0.f }, { TEXT("Einsatz.Laden.Gewehr.Paket"), 1.f }, { TEXT("Einsatz.Laden.Gewehr.Max"), 1.f },
		{ TEXT("Einsatz.Laden.Nobelisken-Zünder.Grundpreis"), 12.f }, { TEXT("Einsatz.Laden.Nobelisken-Zünder.Aufschlag"), 0.f }, { TEXT("Einsatz.Laden.Nobelisken-Zünder.Paket"), 1.f }, { TEXT("Einsatz.Laden.Nobelisken-Zünder.Max"), 1.f }, { TEXT("Einsatz.Laden.Kernreparatur.Max"), 0.f },
		{ TEXT("Einsatz.Laden.Treibstoffkraftwerk.Grundpreis"), 8.f }, { TEXT("Einsatz.Laden.Treibstoffkraftwerk.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Treibstoffkraftwerk.Paket"), 1.f },
		// 0.9.1 Endgame-Konzept: spaete Maschinen (Schwerindustrie W25, Mischtechnik W35), Fluessigkeitspuffer mit der Raffinerie
		{ TEXT("Einsatz.Laden.Fluessigkeitspuffer.Grundpreis"), 3.f }, { TEXT("Einsatz.Laden.Fluessigkeitspuffer.Aufschlag"), 0.3f }, { TEXT("Einsatz.Laden.Fluessigkeitspuffer.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Fertiger.Grundpreis"), 14.f }, { TEXT("Einsatz.Laden.Fertiger.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Fertiger.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Mischer.Grundpreis"), 10.f }, { TEXT("Einsatz.Laden.Mischer.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Mischer.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Abfueller.Grundpreis"), 8.f }, { TEXT("Einsatz.Laden.Abfueller.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Abfueller.Paket"), 1.f },
		{ TEXT("Einsatz.Laden.Foerderanlage.Grundpreis"), 6.f }, { TEXT("Einsatz.Geschenk.Raketentreibstoff.Foerderanlage"), 3.f }, { TEXT("Wellen.LPJeWelleAb31"), 0.1f }, { TEXT("Einsatz.Laden.Foerderanlage.Aufschlag"), 0.4f }, { TEXT("Einsatz.Laden.Foerderanlage.Paket"), 3.f },
		// Beute der Kreaturen (UTDLaufProtokoll, 0.55): Chance je Art und Gegnerstufe, Boss dreifache Menge; Rest nach N s an den Kern
		{ TEXT("Loot.An"), 1.f }, { TEXT("Loot.EinsammelnNachSekunden"), 60.f },
		{ TEXT("Loot.Kampfdaten.Normal"), 0.02f }, { TEXT("Loot.Kampfdaten.Elite"), 0.15f }, { TEXT("Loot.Kampfdaten.Boss"), 1.f },
		{ TEXT("Loot.EP.Normal"), 0.03f }, { TEXT("Loot.EP.Elite"), 0.5f }, { TEXT("Loot.EP.Boss"), 1.f },
		{ TEXT("Loot.EPMenge.Normal"), 1.f }, { TEXT("Loot.EPMenge.Elite"), 2.f }, { TEXT("Loot.EPMenge.Boss"), 10.f },
		{ TEXT("Loot.Munition.Normal"), 0.08f }, { TEXT("Loot.Munition.Elite"), 0.4f }, { TEXT("Loot.Munition.Boss"), 0.f },   // 0.73: Boss gibt Energiesplitter statt Munition
		// Teslaspule: Grundlast in MW, auch ohne Ziel (Marius 03.11.2026)
		{ TEXT("Tuerme.Teslaspule.Grundverbrauch"), 15.f },
		// 0.80 (Marius 06.11.2026, Balance-Kreislauf 3.7/3.8): Ladestrom als Balance-Wert statt fest im Asset (vorher 75 MW); Bolzen 5->4, Gewehr 8->6 EP
		{ TEXT("Tuerme.Teslaspule.Ladeverbrauch"), 125.f },
		// Freigabe je Phase (create_research.py, AB_WELLE)
		{ TEXT("Forschung.Durchschlagbolzen.AbWelle"), 5.f },
		{ TEXT("Einsatz.Laden.Kernreparatur.Grundpreis"), 15.f }, { TEXT("Einsatz.Laden.Kernreparatur.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Kernreparatur.Paket"), 25.f },
		// Nachzuegler (Test 0.56 W14: Gegner hingen 3 min fest, die Welle endete nie): s ohne 5 m Annaeherung, dann versetzen; nach x Versuchen entfernen
		{ TEXT("Wellen.NachzueglerSekunden"), 25.f }, { TEXT("Wellen.NachzueglerVersuche"), 3.f },
		// Spawnfeld quer zur Angriffsrichtung (m), damit nicht alle aus einem Punkt kommen
		{ TEXT("Wellen.SpawnBreite"), 40.f },
		// Spielstand vor und nach jeder Welle (FicsitTD_W05_Start / _Ende), 0 = aus
		{ TEXT("Lauf.Spielstaende"), 1.f },
		// Abstand von der Kernmitte (m), ab dem ein Gegner zusaetzlich zur eigenen Reichweite den Kern angreift (Tiefbohrer 0.61: Sockel 4,1 m)
		{ TEXT("Kern.AngriffsAbstand"), 10.f }, { TEXT("Gegner.AbsturzGrenze"), 150.f }, { TEXT("Wellen.SpawnHoehe"), 4.f }, { TEXT("Gegner.Abgrund"), 6.f }, { TEXT("Gegner.StauGeduld"), 40.f }, { TEXT("Gegner.Ausweichen"), 1.f }, { TEXT("Gegner.AusweichRadius"), 5.f }, { TEXT("Gegner.HindernisseInsNetz"), 1.f }, { TEXT("Gegner.HindernisRadius"), 400.f }, { TEXT("Gegner.StartSuche"), 2.f }, { TEXT("Gegner.EigenesNetz"), 1.f }, { TEXT("Gegner.KoerperDurchlaessig"), 1.f }, { TEXT("Gegner.KlippenSperren"), 1.f }, { TEXT("Gegner.KlippenSperrenMax"), 12.f }, { TEXT("Einsatz.HubGesperrt"), 1.f }, { TEXT("Einsatz.SpeichernInWelle"), 0.f }, { TEXT("Pionier.HandFaktor.Armierpistole"), 1.f }, { TEXT("Pionier.HandFaktor.Gewehr"), 1.f }, { TEXT("Pionier.HandFaktor.Nobelisk"), 1.f }, { TEXT("Pionier.WellenAnteil"), 0.5f }, { TEXT("Pionier.Spielwert.Armierpistole"), 15.f }, { TEXT("Pionier.Spielwert.Gewehr"), 5.f }, { TEXT("Pionier.Spielwert.Nobelisk"), 50.f }, { TEXT("Pionier.Sprengmantel"), 120.f }, { TEXT("Wellen.VariationJeEinsatz"), 0.f }, { TEXT("Wellen.BossBudget"), 0.6f },
		{ TEXT("Kombo.Aktiv"), 1.f }, { TEXT("Kombo.Verpuffung.Faktor"), 1.5f }, { TEXT("Kombo.Blitzableiter.Dauer"), 5.f },
		{ TEXT("Kombo.Blitzableiter.Reichweite"), 1.5f }, { TEXT("Kombo.Hitzeriss.Dauer"), 3.f }, { TEXT("Kombo.Engstelle.Radius"), 12.f }, { TEXT("Kombo.Engstelle.Ziele"), 1.f }, { TEXT("Kombo.Sammelpunkt.Faktor"), 1.25f }, { TEXT("Kombo.Leiter.Radius"), 4.f }, { TEXT("Kombo.Leiter.Anteil"), 0.3f }, { TEXT("Kombo.Leiter.MaxZiele"), 3.f }, { TEXT("Tuerme.Teslaspule.Hochspannung"), 1.3f }, { TEXT("Forschung.Hochspannung.AbWelle"), 25.f }, { TEXT("Forschung.Raketentreibstoff.AbWelle"), 35.f }, { TEXT("Forschung.Plasmaflamme.AbWelle"), 38.f }, { TEXT("Gegner.BewuchsRaeumen"), 4.f }, { TEXT("Wellen.WegRaeumen"), 3.f }, { TEXT("Treffer.Rundungsrest"), 1.f },
		// 0.79 Breite Front (Option, Standard aus - Marius 06.11.: technisch vorhanden, jetzige Spielversion unveraendert)
		{ TEXT("Wellen.BreiteFront"), 0.f }, { TEXT("Wellen.BreiteVersatz"), 12.f }, { TEXT("Wellen.BreiteTempo"), 0.12f },
		{ TEXT("Wellen.BreiteKern"), 40.f }, { TEXT("Wellen.BreiteAbstand"), 4.f }, { TEXT("Wellen.BreiteAbstandGewicht"), 0.6f }, { TEXT("Gegner.AngriffHoehe"), 12.f }, { TEXT("Kern.Gedraenge"), 20.f }, { TEXT("Kern.GedraengeSekunden"), 3.f },
		// 0.65 (Lauf 04.11.: Kern 82 % -> 10 % bis W10): Grundreparatur nach jeder Welle, Warnung mit Hinweis auf die Kernreparatur im Laden
		{ TEXT("Kern.Grundreparatur"), 0.05f }, { TEXT("Kern.Warnung"), 50.f },
		// 0.58 Forschung: Kern-Ausbau, Logistik/Befestigung, Spezialisierung
		{ TEXT("Forschung.KernPanzer.Faktor"), 1.3f }, { TEXT("Forschung.KernNotrep.Anteil"), 0.1f }, { TEXT("Forschung.KernSchild.Menge"), 2000.f },
		{ TEXT("Forschung.KernGeschuetz.Schaden"), 60.f }, { TEXT("Forschung.KernGeschuetz.Reichweite"), 30.f },
		{ TEXT("Forschung.Munitionspuffer.Faktor"), 2.f }, { TEXT("Forschung.MauerMk2.Faktor"), 2.f },
		{ TEXT("Forschung.Stachelmauer.Anteil"), 0.25f }, { TEXT("Forschung.Stachelmauer.Minimum"), 15.f },
		{ TEXT("Forschung.KernSchild.AbWelle"), 8.f }, { TEXT("Forschung.KernGeschuetz.AbWelle"), 10.f }, { TEXT("Forschung.MauerMk2.AbWelle"), 6.f },
		{ TEXT("Forschung.BandMk3.AbWelle"), 10.f }, { TEXT("Forschung.Stachelmauer.AbWelle"), 12.f },
		{ TEXT("Forschung.Turmfertigung2.AbWelle"), 10.f }, { TEXT("Forschung.Turmfertigung3.AbWelle"), 20.f }, { TEXT("Forschung.Turmfertigung4.AbWelle"), 30.f },
		// Level 5 Meisterstufen (0.64)
		{ TEXT("Tuerme.Nobelisk-Werfer.StreuAbstand"), 4.5f }, { TEXT("Tuerme.Teslaspule.FeldRadius"), 10.f }, { TEXT("Tuerme.Teslaspule.FeldTempo"), 0.6f },
		{ TEXT("Tuerme.Teslaspule.FeldSchaden"), 15.f }, { TEXT("Tuerme.Flammenwerfer.TeppichDauer"), 6.f }, { TEXT("Tuerme.Flammenwerfer.TeppichAnteil"), 0.4f },
		{ TEXT("Tuerme.Flammenwerfer.TeppichRadius"), 3.f },
		// 0.9.1 Plasmaflamme (Ionisiert) trifft als Strom (1 = an)
		{ TEXT("Munition.Treibstoff.Ionisiert.AlsStrom"), 1.f },
		// 0.9.3 Feuer-Flipbook: Anzahl Feuerwolken je Flammenwerfer, Lebensdauer einer Wolke (s), Leuchtkraft
		{ TEXT("Tuerme.Himmelsturm.Nachladen"), 1.5f },   // 0.9.3 sichtbare Kanister: Sekunden bis ein Rohr wieder geladen ist
		{ TEXT("Tuerme.Funkmast.RessourcenRadius"), 150.f }, { TEXT("Tuerme.Funkmast.Vorwarnung"), 30.f },   // 0.9.3 Radar
		{ TEXT("Tuerme.Drohnenplatz.Tempo"), 1500.f }, { TEXT("Tuerme.Drohnenplatz.Flughoehe"), 15.f }, { TEXT("Tuerme.Drohnenplatz.KampfSchaden"), 25.f },
		{ TEXT("Tuerme.Drohnenplatz.KampfDauer"), 3.f }, { TEXT("Tuerme.Drohnenplatz.ReparaturDauer"), 2.f }, { TEXT("Tuerme.Drohnenplatz.DrohnenGroesse"), 0.2f },
		{ TEXT("Tuerme.Drohnenplatz.LP"), 4000.f }, { TEXT("Tuerme.Drohnenplatz.Reichweite"), 40.f }, { TEXT("Tuerme.Drohnenplatz.Startmunition"), 0.f }, { TEXT("Tuerme.Drohnenplatz.LevelPreis2"), 8.f }, { TEXT("Tuerme.Drohnenplatz.LevelPreis3"), 15.f },
		{ TEXT("Einsatz.Laden.Drohnenplatz.Grundpreis"), 14.f }, { TEXT("Einsatz.Laden.Drohnenplatz.Aufschlag"), 0.5f }, { TEXT("Einsatz.Laden.Drohnenplatz.Paket"), 1.f },
		{ TEXT("Forschung.Reparaturdrohnen.AbWelle"), 40.f }, { TEXT("Forschung.Kampfdrohnen.AbWelle"), 40.f },
		{ TEXT("Bauten.Tor.Strom"), 2.f }, { TEXT("Bauten.Tor.OhneStromOffen"), 1.f },   // 0.9.3 Tor: MW, ohne Strom in der Welle offen (1 = an)
		{ TEXT("Effekte.Oelbrand.Flammen"), 36.f }, { TEXT("Effekte.Oelbrand.Lebensdauer"), 0.9f }, { TEXT("Effekte.Oelbrand.Staerke"), 7.f },   // 0.9.3 Flammen beim Oelbrand
		{ TEXT("Effekte.Flamme.Wolken"), 28.f }, { TEXT("Effekte.Flamme.Lebensdauer"), 0.6f }, { TEXT("Effekte.Flamme.Staerke"), 8.f },
		// 0.58 Pionier-Rang: XP je Welle (Grund + je Welle, Kern voll, Boss x2), je eigenem Abschuss; Rang-Schwelle Grund + Schritt x Rang
		{ TEXT("Pionier.XPWelle"), 20.f }, { TEXT("Pionier.XPJeWelle"), 3.f }, { TEXT("Pionier.XPKernVoll"), 10.f },
		{ TEXT("Pionier.XPAbschuss"), 3.f }, { TEXT("Pionier.XPAbschussElite"), 10.f }, { TEXT("Pionier.XPAbschussBoss"), 50.f },
		{ TEXT("Pionier.XPGrund"), 5.f }, { TEXT("Pionier.XPSchritt"), 20.f }, { TEXT("Pionier.MaxRang"), 16.f }, { TEXT("Pionier.NeuVerteilenPreis"), 20.f },
		// 0.9.7 P2 (Analyse-Spieler-und-Pionier 3.2): Zielmarkierung (Dauer s, Turmschaden-Faktor, Licht), Jaeger (Spielerschaden an Elite/Boss, sichere Beute), Assist-XP
		{ TEXT("Pionier.Markierung.An"), 1.f }, { TEXT("Pionier.Markierung.Dauer"), 6.f }, { TEXT("Pionier.Markierung.Bonus"), 1.15f }, { TEXT("Pionier.Markierung.Licht"), 1.f },
		{ TEXT("Pionier.Jaeger.Faktor"), 1.5f }, { TEXT("Pionier.Jaeger.SichereBeute"), 1.f }, { TEXT("Pionier.XPAssist"), 1.f },
		// 0.9.8 P3/P4 (Analyse 3.3, Marius 12.11.): Talentbaum Schuetze/Kommandant/Techniker/Jaeger, Rangboni, XP fuer Leistung (je Prozent Schadensanteil, je reparierte LP)
		{ TEXT("Pionier.RangBonusAlle"), 3.f }, { TEXT("Pionier.RangBonus.LP"), 10.f }, { TEXT("Pionier.RangBonus.Tempo"), 0.03f },
		{ TEXT("Pionier.XPJeProzent"), 2.f }, { TEXT("Pionier.XPJeReparatur"), 200.f },
		{ TEXT("Pionier.Kopftreffer"), 1.5f }, { TEXT("Pionier.Schnellladen"), 0.5f }, { TEXT("Pionier.Munition.Anteil"), 0.25f }, { TEXT("Pionier.Sprengmantel.Radius"), 4.f },
		{ TEXT("Pionier.Markierung.Sprung"), 10.f }, { TEXT("Pionier.Sammelbefehl.Radius"), 30.f }, { TEXT("Pionier.Sammelbefehl.Dauer"), 5.f },
		{ TEXT("Pionier.Uebersteuern.Radius"), 20.f }, { TEXT("Pionier.Uebersteuern.Dauer"), 10.f }, { TEXT("Pionier.Uebersteuern.Pause"), 60.f }, { TEXT("Pionier.Uebersteuern.Takt"), 1.5f },
		{ TEXT("Pionier.Reparatur.Rate"), 0.05f }, { TEXT("Pionier.Reparatur.Radius"), 8.f }, { TEXT("Pionier.Reparatur.LPJePlatte"), 300.f },
		{ TEXT("Pionier.Nachschub.Radius"), 20.f }, { TEXT("Pionier.Nachschub.Menge"), 10.f },
		{ TEXT("Pionier.Jaeger.Schwachstelle"), 1.25f }, { TEXT("Pionier.Jaeger.SchwachDauer"), 5.f }, { TEXT("Pionier.Jaeger.Hinrichtung"), 0.15f },
		{ TEXT("Tuerme.Bolzenturm.GeschossGroesse"), 3.f }, { TEXT("Forschung.Kohlekraft.AbWelle"), 6.f }, { TEXT("Forschung.Tor.AbWelle"), 6.f }, { TEXT("Forschung.Lockfeuer.AbWelle"), 8.f }, { TEXT("Forschung.Oelschlick.AbWelle"), 15.f }, { TEXT("Forschung.UpgBolzen.AbWelle"), 3.f },
		// 0.83 Bauten P2/P3
		{ TEXT("Forschung.Elektrozaun.AbWelle"), 12.f }, { TEXT("Forschung.Funkmast.AbWelle"), 12.f }, { TEXT("Forschung.Schildgenerator.AbWelle"), 25.f }, { TEXT("Forschung.Himmelsturm.AbWelle"), 35.f }, { TEXT("Forschung.Stoerfeld.AbWelle"), 35.f }, { TEXT("Forschung.Singularitaet.AbWelle"), 51.f }, { TEXT("Forschung.Dimensionslager.AbWelle"), 51.f },
		{ TEXT("Forschung.Gewehrturm.AbWelle"), 5.f }, { TEXT("Forschung.Teslaspule.AbWelle"), 5.f }, { TEXT("Forschung.Raffinerie.AbWelle"), 15.f },
		{ TEXT("Forschung.Treibstoffkraft.AbWelle"), 15.f }, { TEXT("Forschung.Flammenwerfer.AbWelle"), 15.f }, { TEXT("Forschung.Schwefel.AbWelle"), 5.f }, { TEXT("Forschung.Quarz.AbWelle"), 10.f }, { TEXT("Forschung.Caterium.AbWelle"), 12.f }, { TEXT("Forschung.Bauxit.AbWelle"), 25.f }, { TEXT("Forschung.Schwerindustrie.AbWelle"), 25.f }, { TEXT("Forschung.Mischtechnik.AbWelle"), 35.f }, { TEXT("Forschung.Explosivbolzen.AbWelle"), 25.f },
		{ TEXT("Forschung.NobeliskWerfer.AbWelle"), 10.f }, { TEXT("Forschung.GasNobelisk.AbWelle"), 15.f }, { TEXT("Forschung.PulsNobelisk.AbWelle"), 15.f },
		{ TEXT("Forschung.Turbomunition.AbWelle"), 25.f }, { TEXT("Forschung.Turbotreibstoff.AbWelle"), 25.f }, { TEXT("Forschung.StreuNobelisk.AbWelle"), 25.f },
		// Startkontingent je Einsatz
		{ TEXT("Einsatz.Start.Abwehrkern"), 1.f }, { TEXT("Einsatz.Start.MAM"), 1.f }, { TEXT("Einsatz.Start.Miner"), 1.f }, { TEXT("Einsatz.Start.Schmelzofen"), 1.f },
		{ TEXT("Einsatz.Start.Konstruktor"), 1.f }, { TEXT("Einsatz.Start.Biomassegenerator"), 1.f }, { TEXT("Einsatz.Start.Lager"), 0.f },
		{ TEXT("Einsatz.Start.Bolzenturm"), 1.f }, { TEXT("Einsatz.Start.Mauer"), 5.f }, { TEXT("Einsatz.Start.Splitter"), 0.f }, { TEXT("Einsatz.Start.Verbinder"), 0.f },

		// ---- Leistung (T2, Gegnertakt-Recherche 5): Animation nur sichtbar (1) oder immer (0), Update Rate Optimization,
		//      tote Gegner nach Sekunden entfernen (0 = liegen lassen), Gegner kollidieren untereinander (1) oder laufen durcheinander (0) ----
		{ TEXT("Leistung.AnimNurSichtbar"), 1.f }, { TEXT("Leistung.URO"), 1.f }, { TEXT("Leistung.LeicheSekunden"), 3.f }, { TEXT("Leistung.GegnerKollision"), 1.f },
		// T2b: Ragdoll beim Tod (1) oder Koerper sofort einfrieren und nach 0,5 s entfernen (0)
		{ TEXT("Leistung.Ragdoll"), 1.f },
		// T2c: sichtbare Spiel-Geschosse der Bolzentuerme (1) oder nur Schaden ohne Geschoss (0)
		{ TEXT("Leistung.Geschosse"), 1.f },
	};

	TMap<FString, float> GStandard;
	TMap<FString, float> GDatei;
	bool GGeladen = false;

	void FuelleStandard()
	{
		if (GStandard.Num() == 0)
		{
			for (const FTDStandard& S : Standards)
			{
				GStandard.Add(S.Schluessel, S.Wert);
			}
		}
	}

	/** "A.B.C" = 1 -> {"A":{"B":{"C":1}}}, Reihenfolge wie in der Tabelle */
	TSharedRef<FJsonObject> AlsJson(const TMap<FString, float>& Werte)
	{
		TSharedRef<FJsonObject> Wurzel = MakeShared<FJsonObject>();
		Wurzel->SetStringField(TEXT("_Hinweis"), TEXT("FicsitTD-Balance. Werte aendern, speichern, im Spiel /td balance neu. Strecken in m, Zeiten in s, Tempo in m/s. Fehlende Werte = Standard (Balance-Standard.json)."));
		for (const FTDStandard& S : Standards)
		{
			const float* W = Werte.Find(S.Schluessel);
			TArray<FString> Teile;
			FString(S.Schluessel).ParseIntoArray(Teile, TEXT("."));
			TSharedRef<FJsonObject> Ebene = Wurzel;
			for (int32 i = 0; i < Teile.Num() - 1; ++i)
			{
				const TSharedPtr<FJsonObject>* Kind = nullptr;
				if (!Ebene->TryGetObjectField(Teile[i], Kind))
				{
					Ebene->SetObjectField(Teile[i], MakeShared<FJsonObject>());
					Ebene->TryGetObjectField(Teile[i], Kind);
				}
				Ebene = Kind->ToSharedRef();
			}
			Ebene->SetNumberField(Teile.Last(), W ? *W : S.Wert);
		}
		return Wurzel;
	}

	void Flach(const TSharedPtr<FJsonObject>& Objekt, const FString& Vorne, TMap<FString, float>& Aus)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Feld : Objekt->Values)
		{
			if (Feld.Key.StartsWith(TEXT("_")) || !Feld.Value.IsValid())
			{
				continue;
			}
			const FString Name = Vorne.IsEmpty() ? Feld.Key : Vorne + TEXT(".") + Feld.Key;
			if (Feld.Value->Type == EJson::Object)
			{
				Flach(Feld.Value->AsObject(), Name, Aus);
			}
			else if (Feld.Value->Type == EJson::Number)
			{
				Aus.Add(Name, static_cast<float>(Feld.Value->AsNumber()));
			}
		}
	}

	bool Schreibe(const FString& Datei, const TMap<FString, float>& Werte)
	{
		FString Text;
		const TSharedRef<TJsonWriter<>> Schreiber = TJsonWriterFactory<>::Create(&Text);
		FJsonSerializer::Serialize(AlsJson(Werte), Schreiber);
		return FFileHelper::SaveStringToFile(Text, *Datei, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

using namespace TDBalanceIntern;

FString TDBalance::Pfad()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FicsitTD"), TEXT("Balance.json"));
}

FString TDBalance::Lade()
{
	FuelleStandard();
	GGeladen = true;
	GDatei.Reset();
	const FString Datei = Pfad();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Datei), true);
	Schreibe(FPaths::Combine(FPaths::GetPath(Datei), TEXT("Balance-Standard.json")), GStandard);
	if (!FPaths::FileExists(Datei))
	{
		// 0.9.0: keine Balance.json mehr anlegen - sonst froren die Werte beim Spieler ein und spaetere Balance-Updates wirkten nicht.
		// Wer etwas aendern will, kopiert Balance-Standard.json nach Balance.json (oder legt nur die geaenderten Schluessel an).
		const FString Meldung = FString::Printf(TEXT("Balance: keine %s - es gelten die %d Standardwerte (Vorlage Balance-Standard.json)"), *Datei, GStandard.Num());
		UE_LOG(LogFicsitTD, Log, TEXT("%s"), *Meldung);
		return Meldung;
	}
	FString Text;
	TSharedPtr<FJsonObject> Wurzel;
	if (!FFileHelper::LoadFileToString(Text, *Datei) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Wurzel) || !Wurzel.IsValid())
	{
		const FString Meldung = FString::Printf(TEXT("Balance: %s ist kein gueltiges JSON - es gelten die Standardwerte"), *Datei);
		UE_LOG(LogFicsitTD, Warning, TEXT("%s"), *Meldung);
		return Meldung;
	}
	TMap<FString, float> Gelesen;
	Flach(Wurzel, FString(), Gelesen);
	TArray<FString> Geaendert, Unbekannt;
	for (const TPair<FString, float>& W : Gelesen)
	{
		const float* Standard = GStandard.Find(W.Key);
		if (!Standard)
		{
			Unbekannt.Add(W.Key);
			continue;
		}
		GDatei.Add(W.Key, W.Value);
		if (!FMath::IsNearlyEqual(*Standard, W.Value))
		{
			Geaendert.Add(FString::Printf(TEXT("%s=%g (Standard %g)"), *W.Key, W.Value, *Standard));
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Balance: %d Werte gelesen, %d vom Standard abweichend%s"), Gelesen.Num(), Geaendert.Num(),
		Geaendert.Num() ? *(TEXT(": ") + FString::Join(Geaendert, TEXT(", "))) : TEXT(""));
	if (Unbekannt.Num())
	{
		UE_LOG(LogFicsitTD, Warning, TEXT("Balance: unbekannte Schluessel (Tippfehler?): %s"), *FString::Join(Unbekannt, TEXT(", ")));
	}
	return FString::Printf(TEXT("Balance geladen: %d Werte, %d geaendert%s%s"), Gelesen.Num(), Geaendert.Num(),
		Unbekannt.Num() ? *FString::Printf(TEXT(", %d unbekannt (siehe Log)"), Unbekannt.Num()) : TEXT(""),
		Geaendert.Num() ? *(TEXT(" - ") + FString::Join(Geaendert, TEXT(", "))) : TEXT(""));
}

bool TDBalance::SetzeTest(const FString& Schluessel, float Neu)
{
	if (!GGeladen)
	{
		Lade();
	}
	if (!GStandard.Contains(Schluessel))
	{
		return false;
	}
	GDatei.Add(Schluessel, Neu);
	UE_LOG(LogFicsitTD, Log, TEXT("Balance (Selbsttest): %s = %g"), *Schluessel, Neu);
	return true;
}

bool TDBalance::Hat(const FString& Schluessel)
{
	if (!GGeladen)
	{
		Lade();
	}
	return GDatei.Contains(Schluessel) || GStandard.Contains(Schluessel);
}

float TDBalance::Wert(const FString& Schluessel, float Ersatz)
{
	if (!GGeladen)
	{
		Lade();
	}
	if (const float* W = GDatei.Find(Schluessel))
	{
		return *W;
	}
	if (const float* W = GStandard.Find(Schluessel))
	{
		return *W;
	}
	static TSet<FString> Gemeldet;
	if (!Gemeldet.Contains(Schluessel))
	{
		Gemeldet.Add(Schluessel);
		UE_LOG(LogFicsitTD, Warning, TEXT("Balance: Schluessel %s fehlt in der Tabelle, nehme %g"), *Schluessel, Ersatz);
	}
	return Ersatz;
}
