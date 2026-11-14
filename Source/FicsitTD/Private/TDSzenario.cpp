#include "TDSzenario.h"
#include "FicsitTDModule.h"
#include "TDForschung.h"
#include "TDEinsatz.h"
#include "TDLaufProtokoll.h"
#include "TDTurretBase.h"
#include "TDWaveSubsystem.h"
#include "TDWellenStatistik.h"
#include "EngineUtils.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGGameState.h"
#include "Resources/FGItemDescriptor.h"
#include "TDDefenseCore.h"

namespace
{
	struct FTurmAnf { const TCHAR* Art; int32 Anzahl; int32 Level; };
	struct FMunAnf { const TCHAR* Art; float ProMin; };
	struct FSzenario
	{
		int32 Nr;
		const TCHAR* Name;
		int32 Welle;
		const TCHAR* Prueft;
		TArray<FTurmAnf> Tuerme;
		float StromMW;
		TArray<FMunAnf> Munition;
		bool bTuermeLeer;
	};
	// Konzept-Kernplaetze.md 5.1 (Entwurf), Biom fuer T1-T5: Felsenwueste (Normal)
	const TArray<FSzenario>& Szenarien()
	{
		static const TArray<FSzenario> Liste = {
			{ 1, TEXT("Fruehphase"), 1, TEXT("Startablauf, Kontingente, erste Wellen, Statistik"), {}, 0.f, {}, false },
			{ 2, TEXT("Phase 2"), 11, TEXT("zweite Front, Elite, Munitionsfluss"),
				{ { TEXT("Bolzenturm"), 4, 2 }, { TEXT("Gewehrturm"), 2, 2 } }, 75.f,
				{ { TEXT("Bolzenturm"), 30.f }, { TEXT("Gewehrturm"), 15.f } }, false },
			{ 3, TEXT("Phase 3"), 21, TEXT("Turmmischung, Treibstoff, Strombedarf"),
				{ { TEXT("Bolzenturm"), 4, 3 }, { TEXT("Gewehrturm"), 3, 3 }, { TEXT("Teslaspule"), 2, 3 }, { TEXT("Flammenwerfer"), 2, 3 }, { TEXT("Nobelisk-Werfer"), 2, 3 } }, 300.f,
				{ { TEXT("Bolzenturm"), 45.f }, { TEXT("Gewehrturm"), 30.f }, { TEXT("Nobelisk-Werfer"), 10.f } }, false },
			{ 4, TEXT("Endphase"), 30, TEXT("Bosse, Meisterstufen, Leistung bei vielen Gegnern"),
				{ { TEXT("Bolzenturm"), 3, 5 }, { TEXT("Gewehrturm"), 3, 5 }, { TEXT("Teslaspule"), 3, 5 }, { TEXT("Flammenwerfer"), 3, 5 }, { TEXT("Nobelisk-Werfer"), 3, 5 } }, 600.f,
				{ { TEXT("Bolzenturm"), 60.f }, { TEXT("Gewehrturm"), 40.f }, { TEXT("Nobelisk-Werfer"), 15.f } }, false },
			{ 5, TEXT("Munition unter Druck"), 15, TEXT("Nachschub waehrend der Welle"),
				{ { TEXT("Bolzenturm"), 6, 2 }, { TEXT("Gewehrturm"), 4, 2 } }, 150.f,
				{ { TEXT("Bolzenturm"), 60.f }, { TEXT("Gewehrturm"), 30.f } }, true },
		};
		return Liste;
	}
	int32 GLetztes = 0;

	const FSzenario* Finde(int32 Nr)
	{
		return Szenarien().FindByPredicate([Nr](const FSzenario& S) { return S.Nr == Nr; });
	}
}

TArray<FString> TDSzenario::Liste()
{
	TArray<FString> Zeilen;
	for (const FSzenario& S : Szenarien())
	{
		TArray<FString> T;
		for (const FTurmAnf& A : S.Tuerme)
		{
			T.Add(FString::Printf(TEXT("%dx %s L%d"), A.Anzahl, A.Art, A.Level));
		}
		Zeilen.Add(FString::Printf(TEXT("T%d %s: vor Welle %d - %s%s%s%s"), S.Nr, S.Name, S.Welle, S.Prueft,
			T.Num() ? *FString::Printf(TEXT(" | Basis: %s"), *FString::Join(T, TEXT(", "))) : TEXT(" | Basis: nur Startkit"),
			S.StromMW > 0.f ? *FString::Printf(TEXT(", Strom %.0f MW"), S.StromMW) : TEXT(""),
			S.bTuermeLeer ? TEXT(", Tuerme ohne Vorrat (/td szenario leeren)") : TEXT("")));
	}
	Zeilen.Add(TEXT("T6 Biome: neues Spiel mit Tower Defense in Grasfelder, Noerdlicher Wald und Duenenwueste - kein Befehl noetig."));
	Zeilen.Add(TEXT("/td szenario <1-5> stellt den Einsatz ein, /td szenario pruefen vergleicht die Basis mit den Anforderungen."));
	return Zeilen;
}

TArray<FString> TDSzenario::Anwenden(UObject* WorldContext, int32 Nr, APawn* Spieler)
{
	TArray<FString> Zeilen;
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const FSzenario* S = Finde(Nr);
	UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	if (!S || !Einsatz || !Waves)
	{
		Zeilen.Add(TEXT("Szenario 1 bis 5 angeben (/td szenario liste)."));
		return Zeilen;
	}
	if (!Einsatz->IstAktiv())
	{
		Zeilen.Add(TEXT("Kein Einsatz aktiv - zuerst einen Tower-Defense-Spielstand starten (Kern steht)."));
		return Zeilen;
	}
	GLetztes = Nr;
	if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(WorldContext))
	{
		Lauf->Eingriff(FString::Printf(TEXT("szenario %d"), Nr));
	}
	// 1) Forschung der Phasen davor (Freigabe-Welle bis zur Einstiegswelle); bei Abzweigungen die erste Seite
	int32 Erforscht = 0, FPKosten = 0;
	if (AFGSchematicManager* Schemata = AFGSchematicManager::Get(World))
	{
		AFGCharacterPlayer* Figur = Cast<AFGCharacterPlayer>(Spieler);
		TSet<FString> Gewaehlt;
		for (const FTDForschungKnoten& K : TDForschung::Alle())
		{
			if (TDForschung::AbWelle(K) > S->Welle || TDForschung::Hat(WorldContext, *K.Key))
			{
				continue;
			}
			TArray<FString> Seiten;
			K.Andere.ParseIntoArray(Seiten, TEXT(","), true);
			if (Seiten.ContainsByPredicate([&](const FString& A) { return Gewaehlt.Contains(A) || TDForschung::Hat(WorldContext, *A); }))
			{
				continue;
			}
			if (UClass* Klasse = LoadClass<UFGSchematic>(nullptr, *K.Pfad))
			{
				Schemata->GiveAccessToSchematic(Klasse, Figur);
				Gewaehlt.Add(K.Key);
				++Erforscht;
				FPKosten += K.FP;
			}
		}
	}
	Zeilen.Add(FString::Printf(TEXT("T%d %s: %d Forschungsknoten bis Welle %d freigegeben."), S->Nr, S->Name, Erforscht, S->Welle));
	// 2) Einstiegswelle (EP, FP und Biomasse wie nach den uebersprungenen Wellen)
	if (S->Welle > 1)
	{
		Zeilen.Add(Waves->SpringeZuWelle(S->Welle) ? FString::Printf(TEXT("Naechste Welle ist %d."), S->Welle)
			: TEXT("Wellensprung nicht moeglich (nur zwischen den Wellen) - Forschung ist trotzdem freigegeben."));
	}
	// 2b) Test T2 (Marius): FP blieben uebrig, obwohl die Forschung geschenkt war -> Kosten der freigegebenen Knoten abziehen
	{
		UClass* FPKlasse = UTDEinsatzSubsystem::FPKlasse();
		AFGCharacterPlayer* Figur = Cast<AFGCharacterPlayer>(Spieler);
		UFGInventoryComponent* Inventar = Figur ? Figur->GetInventory() : nullptr;
		int32 Rest = FPKosten;
		if (FPKlasse && Inventar)
		{
			const int32 Weg = FMath::Min(Rest, Inventar->GetNumItems(FPKlasse));
			if (Weg > 0)
			{
				Inventar->Remove(FPKlasse, Weg);
				Rest -= Weg;
			}
		}
		if (ATDDefenseCore* Kern = Einsatz->GetKern())
		{
			const int32 Weg = FMath::Min(Rest, Kern->mFPOffen);
			Kern->mFPOffen -= Weg;
			Rest -= Weg;
		}
		Zeilen.Add(FString::Printf(TEXT("FP fuer die freigegebene Forschung verrechnet: %d von %d abgezogen."), FPKosten - Rest, FPKosten));
	}
	// 3) Freies und kostenloses Bauen fuer die Basis (Test T2: Freies Bauen allein half nicht, das Material fehlte)
	Einsatz->SetzeFrei(true);
	if (AFGGameState* Zustand = World->GetGameState<AFGGameState>())
	{
		Zustand->SetCheatNoCost(true);
	}
	Zeilen.Add(TEXT("Freies Bauen und Kostenloses Bauen sind AN (keine Kontingente, kein Material). Nach dem Aufbau: /td szenario fertig."));
	Zeilen.Append(Pruefen(WorldContext, Nr));
	Zeilen.Add(TEXT("Basis fertig: /td szenario pruefen, /td szenario fertig, dann speichern als 'FicsitTD_T<Nr>'."));
	return Zeilen;
}

TArray<FString> TDSzenario::Pruefen(UObject* WorldContext, int32 Nr)
{
	TArray<FString> Zeilen;
	const FSzenario* S = Finde(Nr > 0 ? Nr : GLetztes);
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!S || !World)
	{
		Zeilen.Add(TEXT("Kein Szenario gewaehlt: /td szenario pruefen <1-5>."));
		return Zeilen;
	}
	int32 Offen = 0;
	// Tuerme je Art mit Mindestlevel
	TMap<FString, int32> Anzahl, MitLevel;
	TMap<FString, int32> MinLevel;
	for (const FTurmAnf& A : S->Tuerme)
	{
		MinLevel.Add(A.Art, A.Level);
	}
	for (TActorIterator<ATDTurretBase> It(World); It; ++It)
	{
		const FString Art = It->GetTurmName();
		++Anzahl.FindOrAdd(Art);
		if (It->GetLevel() >= MinLevel.FindRef(Art))
		{
			++MitLevel.FindOrAdd(Art);
		}
	}
	for (const FTurmAnf& A : S->Tuerme)
	{
		const int32 Ist = MitLevel.FindRef(A.Art);
		const bool bOk = Ist >= A.Anzahl;
		Offen += bOk ? 0 : 1;
		Zeilen.Add(FString::Printf(TEXT("%s %s: %d/%d mit Level %d+ (%d gebaut)"), bOk ? TEXT("[OK]") : TEXT("[FEHLT]"), A.Art, Ist, A.Anzahl, A.Level, Anzahl.FindRef(A.Art)));
	}
	// Strom und Munitionsproduktion aus der Wellen-Statistik
	if (UTDWellenStatistik* Statistik = UTDWellenStatistik::Get(WorldContext))
	{
		if (S->StromMW > 0.f)
		{
			const float Kap = Statistik->StromKapazitaetJetzt();
			const bool bOk = Kap >= S->StromMW;
			Offen += bOk ? 0 : 1;
			Zeilen.Add(FString::Printf(TEXT("%s Strom: %.0f von %.0f MW Kapazitaet"), bOk ? TEXT("[OK]") : TEXT("[FEHLT]"), Kap, S->StromMW));
		}
		const TMap<FString, FTDTurmStat> Jetzt = Statistik->Momentaufnahme();
		for (const FMunAnf& M : S->Munition)
		{
			const FTDTurmStat* T = Jetzt.Find(M.Art);
			const float Ist = T ? T->ProduktionMin : 0.f;
			const bool bOk = Ist >= M.ProMin;
			Offen += bOk ? 0 : 1;
			Zeilen.Add(FString::Printf(TEXT("%s Munition %s: Fabrik %.0f von %.0f /min"), bOk ? TEXT("[OK]") : TEXT("[FEHLT]"), M.Art, Ist, M.ProMin));
		}
	}
	if (const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>())
	{
		const int32 Naechste = Waves->GetWelle() + 1;
		const bool bOk = Naechste == S->Welle;
		Zeilen.Add(FString::Printf(TEXT("%s Naechste Welle %d (Szenario: %d)"), bOk ? TEXT("[OK]") : TEXT("[HINWEIS]"), Naechste, S->Welle));
	}
	Zeilen.Insert(FString::Printf(TEXT("Szenario T%d %s: %s"), S->Nr, S->Name, Offen == 0 ? TEXT("alle Anforderungen erfuellt") : *FString::Printf(TEXT("%d Anforderung(en) offen"), Offen)), 0);
	if (S->bTuermeLeer)
	{
		Zeilen.Add(TEXT("Vor dem Speichern: /td szenario leeren (Tuerme ohne Vorrat, Nachschub nur aus der Fabrik)."));
	}
	return Zeilen;
}

FString TDSzenario::Fertig(UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr)
	{
		Einsatz->SetzeFrei(false);
	}
	if (AFGGameState* Zustand = World ? World->GetGameState<AFGGameState>() : nullptr)
	{
		Zustand->SetCheatNoCost(false);
	}
	return TEXT("Aufbau beendet: Freies Bauen und Kostenloses Bauen sind AUS. Jetzt speichern (z. B. FicsitTD_T2).");
}

FString TDSzenario::TuermeLeeren(UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	int32 Tuerme = 0;
	for (TActorIterator<ATDTurretBase> It(World); World && It; ++It)
	{
		if (UFGInventoryComponent* Eingang = It->GetInputInventory())
		{
			Eingang->Empty();
			++Tuerme;
		}
	}
	if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(WorldContext))
	{
		Lauf->Eingriff(TEXT("szenario leeren"));
	}
	return FString::Printf(TEXT("Munition aus %d Tuermen entfernt (Magazine im Turm laufen noch leer)."), Tuerme);
}
