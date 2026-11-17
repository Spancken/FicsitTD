#include "TDFabrikPruefstand.h"
#include "FicsitTDModule.h"
#include "TDDefenseCore.h"
#include "TDTurretBase.h"
#include "TDWaveSubsystem.h"
#include "FGBuildableSubsystem.h"
#include "FGFactoryConnectionComponent.h"
#include "FGRecipe.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableResourceExtractorBase.h"
#include "Resources/FGResourceNode.h"
#include "Resources/FGResourceDeposit.h"
#include "Resources/FGResourceDescriptor.h"
#include "Resources/FGExtractableResourceInterface.h"
#include "Tests/FGTestBlueprintFunctionLibrary.h"
#include "Tests/FGBuildableSpawnStrategy_Spline.h"
#include "TDEinsatz.h"
#include "TDBalance.h"
#include "FGGameState.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "FGInventoryComponent.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "FGPowerCircuit.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableStorage.h"
#include "Resources/FGItemDescriptor.h"
#include "FGPipeConnectionComponent.h"
#include "FGWaterVolume.h"
#include "Resources/FGResourceNodeFrackingSatellite.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* Miner = TEXT("/Game/FactoryGame/Buildable/Factory/MinerMK1/Build_MinerMk1.Build_MinerMk1_C");
	const TCHAR* Schmelze = TEXT("/Game/FactoryGame/Buildable/Factory/SmelterMk1/Build_SmelterMk1.Build_SmelterMk1_C");
	const TCHAR* Konstrukteur = TEXT("/Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1.Build_ConstructorMk1_C");
	const TCHAR* Splitter = TEXT("/Game/FactoryGame/Buildable/Factory/CA_Splitter/Build_ConveyorAttachmentSplitter.Build_ConveyorAttachmentSplitter_C");

	struct FTDFabrikStufe { const TCHAR* Maschine; const TCHAR* Rezept; };

	/** Bodenpunkt unter/ueber P (Sichtstrahl), sonst P */
	FVector Boden(UWorld* World, const FVector& P)
	{
		FHitResult Treffer;
		FCollisionQueryParams Abfrage(TEXT("TDFabrik"), false);
		if (World->LineTraceSingleByChannel(Treffer, P + FVector(0.f, 0.f, 5000.f), P - FVector(0.f, 0.f, 5000.f), ECC_Visibility, Abfrage))
		{
			return Treffer.ImpactPoint;
		}
		return P;
	}

	/** Anschluesse eines Baus in einer Richtung, nach Namen sortiert (Splitter: Output1 links, Output2 Mitte, Output3 rechts) */
	TArray<UFGFactoryConnectionComponent*> Anschluesse(AActor* Bau, EFactoryConnectionDirection Richtung)
	{
		TArray<UFGFactoryConnectionComponent*> Alle, Treffer;
		if (Bau)
		{
			Bau->GetComponents<UFGFactoryConnectionComponent>(Alle);
		}
		for (UFGFactoryConnectionComponent* C : Alle)
		{
			if (C && C->GetDirection() == Richtung && !C->IsConnected())
			{
				Treffer.Add(C);
			}
		}
		Treffer.Sort([](const UFGFactoryConnectionComponent& A, const UFGFactoryConnectionComponent& B) { return A.GetName() < B.GetName(); });
		return Treffer;
	}

	/**
	 * 17.11. (Test Fenster1117: Lager, Splitter-Ketten und Brenner bekamen nichts): die Standardroute der Testbibliothek ("Auto",
	 * gerade Strecke mit Boegen) liefert bei kurzen Abstaenden und quer stehenden Anschluessen keine Spline-Punkte - das Spiel
	 * fuellt Ersatzwerte ein ("not enough spline points '0'") und das Band foerdert nicht. Die Tangenten-Route ergibt immer eine
	 * Kurve wie ein vom Spieler gezogenes Band. Gilt fuer Baender und Rohre.
	 */
	AFGBuildable* SplineBau(UClass* Klasse, UFGConnectionComponent* Von, UFGConnectionComponent* Nach)
	{
		UFGBuildableSpawnStrategy_Spline* Weg = NewObject<UFGBuildableSpawnStrategy_Spline>(GetTransientPackage());
		Weg->mSplineRouteStrategy = EFGSplineBuildableRouteStrategy::TangentBasedSpline3D;
		return UFGTestBlueprintFunctionLibrary::SpawnSplineBuildable(Klasse, Von, Nach, Weg);
	}

	AFGBuildable* Setze(UWorld* World, UClass* Klasse, const FTransform& T, TFunctionRef<void(AFGBuildable*)> VorFertig)
	{
		AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(World);
		if (!Bau || !Klasse)
		{
			return nullptr;
		}
		// 17.11. (P11/B6): Kosten wie beim Spieler (Modus aus dem Plan, Standard "frei")
		FString Grund;
		if (!TDFabrik::KostenPruefen(World, Klasse, T.GetLocation(), Grund))
		{
			UE_LOG(LogFicsitTD, Warning, TEXT("Fabrik: %s nicht gebaut - %s"), *GetNameSafe(Klasse), *Grund);
			return nullptr;
		}
		AFGBuildable* Neu = Bau->BeginSpawnBuildable(Klasse, T);
		if (Neu)
		{
			VorFertig(Neu);
			Neu->FinishSpawning(T);
		}
		return Neu;
	}
}

bool TDFabrik::BaueLinie(UWorld* World, ATDDefenseCore* Kern, const FString& Linie, int32 Tuerme, int32 Level, int32 BandMk,
	TArray<TWeakObjectPtr<AFGBuildable>>& OutBauten, TArray<FString>& OutBericht, int32 Ketten, int32 Ausbau)
{
	if (!World || !Kern)
	{
		OutBericht.Add(TEXT("kein Kern"));
		return false;
	}
	if (Linie != TEXT("Bolzen"))
	{
		OutBericht.Add(FString::Printf(TEXT("Linie %s gibt es noch nicht (F1: Bolzen)"), *Linie));
		return false;
	}
	static const FTDFabrikStufe Kette[] = {
		{ Schmelze, TEXT("/Game/FactoryGame/Recipes/Smelter/Recipe_IngotIron.Recipe_IngotIron_C") },
		{ Konstrukteur, TEXT("/Game/FactoryGame/Recipes/Constructor/Recipe_IronRod.Recipe_IronRod_C") },
		{ Konstrukteur, TEXT("/Game/FactoryGame/Recipes/Equipment/Recipe_SpikedRebar.Recipe_SpikedRebar_C") },
	};
	const FString ErzName = TEXT("Desc_OreIron");
	UClass* Band = LoadClass<AFGBuildable>(nullptr, *FString::Printf(TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk%d/Build_ConveyorBeltMk%d.Build_ConveyorBeltMk%d_C"),
		FMath::Clamp(BandMk, 1, 6), FMath::Clamp(BandMk, 1, 6), FMath::Clamp(BandMk, 1, 6)));
	UClass* MinerKlasse = LoadClass<AFGBuildable>(nullptr, Miner);
	UClass* SplitterKlasse = LoadClass<AFGBuildable>(nullptr, Splitter);
	UClass* TurmKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/FicsitTD/Buildings/BoltTurret/Build_TD_BoltTurret.Build_TD_BoltTurret_C"));
	UClass* TurmRezept = LoadClass<UFGRecipe>(nullptr, TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_Shoot_SpikedRebar.Recipe_TD_Shoot_SpikedRebar_C"));
	if (!Band || !MinerKlasse || !SplitterKlasse || !TurmKlasse)
	{
		OutBericht.Add(FString::Printf(TEXT("Klassen fehlen: Band %d, Miner %d, Splitter %d, Turm %d"), Band != nullptr, MinerKlasse != nullptr, SplitterKlasse != nullptr, TurmKlasse != nullptr));
		return false;
	}
	const FVector KernOrt = Kern->GetActorLocation();

	// 16.11. abends (Befund B18): bis zu drei parallele Ketten, je eigener Miner auf dem naechsten freien Eisenknoten,
	// zusammengefuehrt ueber einen Verbinder - misst, wie viel Fabrik eine Turmgruppe braucht
	int32 Baender = 0, BandFehler = 0;
	auto Verbinde = [&](AActor* Von, AActor* Nach, int32 AusIndex = 0) -> bool
	{
		const TArray<UFGFactoryConnectionComponent*> Aus = Anschluesse(Von, EFactoryConnectionDirection::FCD_OUTPUT);
		const TArray<UFGFactoryConnectionComponent*> Ein = Anschluesse(Nach, EFactoryConnectionDirection::FCD_INPUT);
		if (!Aus.IsValidIndex(AusIndex) || Ein.Num() == 0)
		{
			++BandFehler;
			UE_LOG(LogFicsitTD, Warning, TEXT("Fabrik: kein freier Anschluss %s -> %s (Ausgaenge %d, Eingaenge %d)"), *GetNameSafe(Von), *GetNameSafe(Nach), Aus.Num(), Ein.Num());
			return false;
		}
		AFGBuildable* B = SplineBau(Band, Aus[AusIndex], Ein[0]);
		if (!B)
		{
			++BandFehler;
			UE_LOG(LogFicsitTD, Warning, TEXT("Fabrik: Band %s -> %s nicht gebaut"), *GetNameSafe(Von), *GetNameSafe(Nach));
			return false;
		}
		++Baender;
		OutBauten.Add(B);
		return true;
	};
	const int32 KettenZahl = FMath::Clamp(Ketten, 1, 3);
	TArray<AActor*> Enden;
	FVector Richtung = FVector::ForwardVector;
	for (int32 c = 0; c < KettenZahl; ++c)
	{
		// B1: naechster freier fester Eisenerz-Knoten (keine Brocken) bis 150 m
		AFGResourceNode* Knoten = nullptr;
		float Bester = 15000.f;
		for (TActorIterator<AFGResourceNode> It(World); It; ++It)
		{
			const TSubclassOf<UFGResourceDescriptor> Art = It->GetResourceClass();
			if (!Art || It->IsA<AFGResourceDeposit>() || It->IsOccupied() || !Art->GetName().Contains(ErzName))
			{
				continue;
			}
			const float D = FVector::Dist2D(It->GetActorLocation(), KernOrt);
			if (D < Bester)
			{
				Bester = D;
				Knoten = *It;
			}
		}
		if (!Knoten)
		{
			OutBericht.Add(FString::Printf(TEXT("Kette %d: kein freier Eisenerz-Knoten bis 150 m"), c + 1));
			break;
		}
		const FVector R = (FVector(Knoten->GetActorLocation().X, Knoten->GetActorLocation().Y, KernOrt.Z) - KernOrt).GetSafeNormal2D();
		// Ketten zum selben Knoten-Gebiet nebeneinander (je 12 m seitlich je Maschine einer Stufe), damit sich nichts ueberlagert
		const int32 Voll = FMath::Clamp(Ausbau, 1, 4);
		const FVector QuerK(-R.Y, R.X, 0.f);
		const FVector Seite = QuerK * (1200.f * Voll * c);
		if (c == 0)
		{
			Richtung = R;
		}
		const FRotator DrehK = R.Rotation();
		OutBericht.Add(FString::Printf(TEXT("Kette %d: Knoten %s %.0f m vom Kern"), c + 1, *Knoten->GetName(), Bester / 100.f));
		// B2: Miner auf dem Knoten
		AFGBuildable* MinerBau = Setze(World, MinerKlasse, FTransform(DrehK, Knoten->GetActorLocation()), [Knoten](AFGBuildable* B)
		{
			if (AFGBuildableResourceExtractorBase* X = Cast<AFGBuildableResourceExtractorBase>(B))
			{
				X->SetExtractableResource(TScriptInterface<IFGExtractableResourceInterface>(Knoten));
			}
		});
		if (!MinerBau)
		{
			OutBericht.Add(TEXT("Miner nicht gebaut"));
			return false;
		}
		OutBauten.Add(MinerBau);
		// B3: Maschinen auf einer Linie zwischen Kern und Knoten.
		// 17.11. (P10-Abnahme "voller Knoten", Marius: bauen wie ein guter Spieler): "ausbau" N (1-4) nutzt den Miner aus -
		// N Stangen- und N Bolzen-Konstrukteure, ceil(N/2) Schmelzoefen, gleichmaessig aufgeteilt (Splitter je 2) und am Ende
		// verbunden. N = 4: 60 Erz/min (Miner Mk1, normaler Knoten) -> 60 Armierungsbolzen/min. N = 1 wie bisher (15/min).
		AActor* VorherK = MinerBau;
		if (Voll == 1)
		{
			const float Abstand = FMath::Min(Bester - 2500.f, 3000.f + 1400.f * UE_ARRAY_COUNT(Kette));
			for (int32 i = 0; i < UE_ARRAY_COUNT(Kette); ++i)
			{
				const FVector Ort = Boden(World, KernOrt + R * (Abstand - 1400.f * i) + Seite);
				UClass* Klasse = LoadClass<AFGBuildable>(nullptr, Kette[i].Maschine);
				UClass* Rezept = LoadClass<UFGRecipe>(nullptr, Kette[i].Rezept);
				AFGBuildable* M = Setze(World, Klasse, FTransform(DrehK + FRotator(0.f, 180.f, 0.f), Ort), [](AFGBuildable*) {});
				if (!M || !Rezept)
				{
					OutBericht.Add(FString::Printf(TEXT("Stufe %d nicht gebaut (Maschine %d, Rezept %d)"), i + 1, M != nullptr, Rezept != nullptr));
					return false;
				}
				if (AFGBuildableManufacturer* Fab = Cast<AFGBuildableManufacturer>(M))
				{
					Fab->SetRecipe(Rezept);
				}
				OutBauten.Add(M);
				Verbinde(VorherK, M);
				VorherK = M;
			}
		}
		else
		{
			UClass* VerbKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/CA_Merger/Build_ConveyorAttachmentMerger.Build_ConveyorAttachmentMerger_C"));
			// Stufen vom Miner nach innen je 13 m, Bolzen-Konstrukteure nicht naeher als 33 m am Kern (Tuerme 20 m, Verteilbaum 26 m)
			const float DBolzen = FMath::Max(3300.f, Bester - 3900.f);
			const float DStangen = DBolzen + 1300.f;
			const float DSchmelze = DStangen + 1300.f;
			auto Platz = [&](float D, float Quer) { return Boden(World, KernOrt + R * D + Seite + QuerK * Quer); };
			auto Maschine = [&](int32 Stufe, const FVector& Ort) -> AFGBuildable*
			{
				UClass* Klasse = LoadClass<AFGBuildable>(nullptr, Kette[Stufe].Maschine);
				UClass* Rezept = LoadClass<UFGRecipe>(nullptr, Kette[Stufe].Rezept);
				AFGBuildable* M = Setze(World, Klasse, FTransform(DrehK + FRotator(0.f, 180.f, 0.f), Ort), [](AFGBuildable*) {});
				if (M && Rezept)
				{
					if (AFGBuildableManufacturer* Fab = Cast<AFGBuildableManufacturer>(M))
					{
						Fab->SetRecipe(Rezept);
					}
					OutBauten.Add(M);
				}
				return M && Rezept ? M : nullptr;
			};
			auto Teiler = [&](UClass* Klasse, const FVector& Ort) -> AFGBuildable*
			{
				AFGBuildable* T = Setze(World, Klasse, FTransform(DrehK + FRotator(0.f, 180.f, 0.f), Ort + FVector(0.f, 0.f, 100.f)), [](AFGBuildable*) {});
				if (T)
				{
					OutBauten.Add(T);
				}
				return T;
			};
			const int32 Oefen = (Voll + 1) / 2;
			auto QuerVon = [](int32 k, int32 n) { return (k - (n - 1) * 0.5f) * 1200.f; };
			// Erz: Miner -> (Splitter) -> Schmelzoefen
			TArray<AActor*> OfenListe;
			for (int32 o = 0; o < Oefen; ++o)
			{
				AFGBuildable* Ofen = Maschine(0, Platz(DSchmelze, QuerVon(o, Oefen) * 2.f));
				if (!Ofen)
				{
					OutBericht.Add(TEXT("Schmelzofen nicht gebaut"));
					return false;
				}
				OfenListe.Add(Ofen);
			}
			if (Oefen == 1)
			{
				Verbinde(MinerBau, OfenListe[0]);
			}
			else if (AFGBuildable* T = Teiler(SplitterKlasse, Platz(DSchmelze + 700.f, 0.f)))
			{
				Verbinde(MinerBau, T);
				for (AActor* Ofen : OfenListe)
				{
					Verbinde(T, Ofen);
				}
			}
			// Barren: Ofen o -> (Splitter) -> Stangen 2o, 2o+1 ; Stange i -> Bolzen i
			TArray<AActor*> BolzenListe;
			for (int32 o = 0; o < Oefen; ++o)
			{
				const int32 Erste = 2 * o;
				const int32 Zahl = FMath::Min(2, Voll - Erste);
				TArray<AActor*> Stangen;
				for (int32 k = 0; k < Zahl; ++k)
				{
					AFGBuildable* St = Maschine(1, Platz(DStangen, QuerVon(Erste + k, Voll)));
					AFGBuildable* Bo = St ? Maschine(2, Platz(DBolzen, QuerVon(Erste + k, Voll))) : nullptr;
					if (!St || !Bo)
					{
						OutBericht.Add(TEXT("Konstrukteur nicht gebaut"));
						return false;
					}
					Verbinde(St, Bo);
					Stangen.Add(St);
					BolzenListe.Add(Bo);
				}
				if (Zahl == 1)
				{
					Verbinde(OfenListe[o], Stangen[0]);
				}
				else if (AFGBuildable* T = Teiler(SplitterKlasse, Platz(DStangen + 650.f, (QuerVon(Erste, Voll) + QuerVon(Erste + 1, Voll)) * 0.5f)))
				{
					Verbinde(OfenListe[o], T);
					Verbinde(T, Stangen[0]);
					Verbinde(T, Stangen[1]);
				}
			}
			// Bolzen zusammenfuehren: bis 3 je Verbinder, ein vierter ueber einen zweiten Verbinder
			AFGBuildable* V1 = Teiler(VerbKlasse, Platz(DBolzen - 600.f, -600.f));
			if (!V1)
			{
				OutBericht.Add(TEXT("Verbinder nicht gebaut"));
				return false;
			}
			for (int32 k = 0; k < FMath::Min(3, BolzenListe.Num()); ++k)
			{
				Verbinde(BolzenListe[k], V1);
			}
			VorherK = V1;
			if (BolzenListe.Num() == 4)
			{
				AFGBuildable* V2 = Teiler(VerbKlasse, Platz(DBolzen - 600.f, 800.f));
				if (V2)
				{
					Verbinde(V1, V2);
					Verbinde(BolzenListe[3], V2);
					VorherK = V2;
				}
			}
			OutBericht.Add(FString::Printf(TEXT("Kette %d: Ausbau %d (%d Schmelzoefen, %d + %d Konstrukteure) = %d Bolzen/min"),
				c + 1, Voll, Oefen, Voll, Voll, 15 * Voll));
		}
		Enden.Add(VorherK);
	}
	if (Enden.Num() == 0)
	{
		return false;
	}
	const FVector Quer = FVector(-Richtung.Y, Richtung.X, 0.f);
	const FRotator Dreh = Richtung.Rotation();
	AActor* Vorher = Enden[0];
	if (Enden.Num() > 1)
	{
		UClass* VerbinderKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/CA_Merger/Build_ConveyorAttachmentMerger.Build_ConveyorAttachmentMerger_C"));
		AFGBuildable* V = Setze(World, VerbinderKlasse, FTransform(Dreh + FRotator(0.f, 180.f, 0.f), Boden(World, KernOrt + Richtung * 2900.f) + FVector(0.f, 0.f, 100.f)), [](AFGBuildable*) {});
		if (!V)
		{
			OutBericht.Add(TEXT("Verbinder nicht gebaut"));
			return false;
		}
		OutBauten.Add(V);
		for (AActor* E : Enden)
		{
			Verbinde(E, V);
		}
		Vorher = V;
	}

	// Tuerme 20 m um den Kern. Fenster B2 16.11.: im Halbkreis auf der Fabrikseite fiel der Kern in W3 (die Front lag gegenueber) -
	// jetzt gleichmaessig im Kreis, beginnend bei der ersten Front. Splitter-Kette 26 m vom Kern Richtung Fabrik.
	FVector FrontRichtung = Richtung;
	if (const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>())
	{
		for (const FTDFront& F : Waves->GetFronten())
		{
			const FVector R = (F.Ort - KernOrt).GetSafeNormal2D();
			if (!R.IsNearlyZero())
			{
				FrontRichtung = R;
				break;
			}
		}
	}
	TArray<AActor*> TurmListe;
	for (int32 k = 0; k < Tuerme; ++k)
	{
		const float Winkel = 360.f * k / FMath::Max(1, Tuerme);
		const FVector Ort = Boden(World, KernOrt + FrontRichtung.RotateAngleAxis(Winkel, FVector::UpVector) * 2000.f);
		AFGBuildable* T = Setze(World, TurmKlasse, FTransform(Dreh, Ort), [](AFGBuildable*) {});
		if (ATDTurretBase* Turm = Cast<ATDTurretBase>(T))
		{
			if (TurmRezept)
			{
				Turm->SetRecipe(TurmRezept);
			}
			Turm->SetzeLevelTest(Level);
			TurmListe.Add(T);
			OutBauten.Add(T);
		}
	}
	// 17.11. (Marius nach Test Fenster1117b: "will ich die Tuerme gleichmaessig bedienen, muss ich auch die Aufteilung gleichmaessig
	// machen" - die alte Splitter-Kette gab 1/2, 1/4, 1/8, 1/8 und liess die letzten Tuerme hungern; das ist Spiellogik, kein
	// Fehler). Verteilbaum wie ein guter Spieler: jeder Splitter teilt gleichmaessig auf 2 oder 3 Straenge (4 Tuerme: einer teilt
	// in zwei, die naechsten zwei geben je 50 % an zwei Tuerme). Exakt gleich fuer 2^a * 3^b Tuerme, sonst annaehernd.
	int32 Verteiler = 0;
	bool bGleich = true;
	TFunction<void(AActor*, const TArray<AActor*>&, int32)> Verteile;
	Verteile = [&](AActor* Zufluss, const TArray<AActor*>& Gruppe, int32 Tiefe)
	{
		if (Gruppe.Num() == 1)
		{
			Verbinde(Zufluss, Gruppe[0], 0);
			return;
		}
		const int32 Teile = Gruppe.Num() % 3 == 0 ? 3 : 2;
		if (Gruppe.Num() % Teile != 0)
		{
			bGleich = false;
		}
		// Ort: Wurzel 26 m Richtung Fabrik, darunter je Ebene 5 m weiter aussen in Richtung der versorgten Tuerme
		FVector R = Richtung;
		if (Tiefe > 0)
		{
			FVector Mitte = FVector::ZeroVector;
			for (const AActor* T : Gruppe)
			{
				Mitte += (T->GetActorLocation() - KernOrt).GetSafeNormal2D();
			}
			R = Mitte.IsNearlyZero() ? (Gruppe[0]->GetActorLocation() - KernOrt).GetSafeNormal2D() : Mitte.GetSafeNormal2D();
		}
		const FVector Ort = Boden(World, KernOrt + R * (2600.f + 500.f * Tiefe)) + FVector(0.f, 0.f, 100.f);
		AFGBuildable* S = Setze(World, SplitterKlasse, FTransform(R.Rotation(), Ort), [](AFGBuildable*) {});
		if (!S)
		{
			OutBericht.Add(TEXT("Splitter nicht gebaut"));
			++BandFehler;
			return;
		}
		OutBauten.Add(S);
		++Verteiler;
		Verbinde(Zufluss, S, 0);
		int32 Start = 0;
		for (int32 t = 0; t < Teile; ++t)
		{
			const int32 Groesse = Gruppe.Num() / Teile + (t < Gruppe.Num() % Teile ? 1 : 0);
			TArray<AActor*> Teil;
			for (int32 k = 0; k < Groesse; ++k)
			{
				Teil.Add(Gruppe[Start + k]);
			}
			Start += Groesse;
			Verteile(S, Teil, Tiefe + 1);
		}
	};
	if (TurmListe.Num() > 0)
	{
		Verteile(Vorher, TurmListe, 0);
	}
	OutBericht.Add(FString::Printf(TEXT("Verteilung: %d Splitter im Baum, %s"), Verteiler,
		bGleich ? TEXT("gleichmaessig") : TEXT("annaehernd gleichmaessig (Turmzahl nicht 2^a*3^b)")));
	OutBericht.Add(FString::Printf(TEXT("Linie Bolzen: %d Ketten (je Miner + %d Maschinen), %d Tuerme, %d Baender Mk%d, %d Bandfehler"),
		Enden.Num(), UE_ARRAY_COUNT(Kette), TurmListe.Num(), Baender, BandMk, BandFehler));
	for (const FString& Z : OutBericht)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Fabrik: %s"), *Z);
	}
	return BandFehler == 0 && TurmListe.Num() == Tuerme;
}

TArray<FString> TDFabrik::Bericht(const TArray<TWeakObjectPtr<AFGBuildable>>& Bauten)
{
	TArray<FString> Zeilen;
	TArray<FString> Maschinen, Tuerme;
	for (const TWeakObjectPtr<AFGBuildable>& W : Bauten)
	{
		const AFGBuildable* B = W.Get();
		if (const ATDTurretBase* T = Cast<ATDTurretBase>(B))
		{
			Tuerme.Add(FString::Printf(TEXT("%d"), T->GetAmmo()));
		}
		else if (const AFGBuildableFactory* F = Cast<AFGBuildableFactory>(B))
		{
			Maschinen.Add(FString::Printf(TEXT("%s %.0f %%"), *F->GetClass()->GetName().Replace(TEXT("Build_"), TEXT("")).Replace(TEXT("_C"), TEXT("")),
				100.f * F->GetProductivity()));
		}
	}
	Zeilen.Add(FString::Printf(TEXT("Maschinen: %s"), *FString::Join(Maschinen, TEXT(", "))));
	Zeilen.Add(FString::Printf(TEXT("Munition je Turm: %s"), *FString::Join(Tuerme, TEXT(" / "))));
	// 17.11. (Test Fenster1117: Netz 0 MW, Brenner 0 %): Brennstoff, Lagerinhalt und Netz-Nummer je Bau, damit die Ursache im Log steht
	TArray<FString> Strom;
	TMap<int32, int32> Netze;
	for (const TWeakObjectPtr<AFGBuildable>& W : Bauten)
	{
		AFGBuildable* B = W.Get();
		if (!B || B->IsA<AFGBuildableWire>())
		{
			continue;
		}
		const UFGPowerConnectionComponent* C = B->FindComponentByClass<UFGPowerConnectionComponent>();
		const int32 Netz = C ? C->GetCircuitID() : -2;
		if (C)
		{
			Netze.FindOrAdd(Netz)++;
		}
		if (const AFGBuildableGeneratorFuel* G = Cast<AFGBuildableGeneratorFuel>(B))
		{
			const UFGInventoryComponent* Brennstoff = G->GetFuelInventory();
			int32 Menge = 0;
			if (Brennstoff)
			{
				TArray<FInventoryStack> Stapel;
				Brennstoff->GetInventoryStacks(Stapel);
				for (const FInventoryStack& St : Stapel)
				{
					Menge += St.NumItems;
				}
			}
			Strom.Add(FString::Printf(TEXT("%s: Brennstoff %d, HatBrennstoff %d, KannStarten %d, Netz %d, Verbindungen %d"),
				*G->GetName(), Menge, G->HasFuel() ? 1 : 0, G->CanStartPowerProduction() ? 1 : 0, Netz, C ? C->GetNumConnections() : -1));
		}
		else if (AFGBuildableStorage* L = Cast<AFGBuildableStorage>(B))
		{
			int32 Menge = 0;
			if (const UFGInventoryComponent* Inhalt = L->GetStorageInventory())
			{
				TArray<FInventoryStack> Stapel;
				Inhalt->GetInventoryStacks(Stapel);
				for (const FInventoryStack& St : Stapel)
				{
					Menge += St.NumItems;
				}
			}
			Strom.Add(FString::Printf(TEXT("%s: %d Teile im Lager"), *L->GetName(), Menge));
		}
	}
	UWorld* Welt = nullptr;
	for (const TWeakObjectPtr<AFGBuildable>& W : Bauten)
	{
		if (W.IsValid())
		{
			Welt = W->GetWorld();
			break;
		}
	}
	if (Welt)
	{
		for (TActorIterator<ATDTurretBase> It(Welt); It; ++It)
		{
			const UFGPowerConnectionComponent* C = It->FindComponentByClass<UFGPowerConnectionComponent>();
			Strom.Add(FString::Printf(TEXT("%s: Netz %d, Verbindungen %d"), *It->GetName(), C ? C->GetCircuitID() : -2, C ? C->GetNumConnections() : -1));
		}
	}
	// Band-Weg der Strom-Bauten: Anschluesse von Lager, Splittern, Brennern und die Enden jedes Bandes
	auto Ende = [](const UFGFactoryConnectionComponent* C) -> FString
	{
		if (!C)
		{
			return TEXT("-");
		}
		const UFGFactoryConnectionComponent* G = C->GetConnection();
		TArray<FInventoryItem> Blick;
		const bool bBlick = C->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT && C->Factory_PeekOutput(Blick);
		return FString::Printf(TEXT("%s(%s)->%s [Inv %s, Index %d, Peek %d/%d]"), *C->GetName(), C->GetDirection() == EFactoryConnectionDirection::FCD_INPUT ? TEXT("ein") : TEXT("aus"),
			G ? *GetNameSafe(G->GetOwner()) : TEXT("frei"), C->GetInventory() ? TEXT("ja") : TEXT("nein"), C->GetInventoryAccessIndex(), bBlick ? 1 : 0, Blick.Num());
	};
	for (const TWeakObjectPtr<AFGBuildable>& W : Bauten)
	{
		AFGBuildable* B = W.Get();
		if (!B || !(B->IsA<AFGBuildableStorage>() || B->IsA<AFGBuildableGeneratorFuel>() || B->GetClass()->GetName().Contains(TEXT("Splitter"))))
		{
			if (AFGBuildableConveyorBase* Band = Cast<AFGBuildableConveyorBase>(B))
			{
				const AActor* Von = Band->GetConnection0() && Band->GetConnection0()->GetConnection() ? Band->GetConnection0()->GetConnection()->GetOwner() : nullptr;
				const AActor* Nach = Band->GetConnection1() && Band->GetConnection1()->GetConnection() ? Band->GetConnection1()->GetConnection()->GetOwner() : nullptr;
				if ((Von && (Von->IsA<AFGBuildableStorage>() || Von->GetClass()->GetName().Contains(TEXT("Splitter")))) || (Nach && Nach->IsA<AFGBuildableGeneratorFuel>()))
				{
					TArray<FConveyorBeltItem*> Teile;
					Band->GetConveyorBeltItems(Teile);
					Strom.Add(FString::Printf(TEXT("Band %s: %s -> %s, steht %d, Kette %s, Teile %d"), *Band->GetName(), *GetNameSafe(Von), *GetNameSafe(Nach),
						Band->IsStalled() ? 1 : 0, Band->GetConveyorChainActor() ? TEXT("ja") : TEXT("nein"), Teile.Num()));
				}
			}
			continue;
		}
		TArray<UFGFactoryConnectionComponent*> Alle;
		B->GetComponents<UFGFactoryConnectionComponent>(Alle);
		TArray<FString> Teile;
		for (const UFGFactoryConnectionComponent* C : Alle)
		{
			Teile.Add(Ende(C));
		}
		Strom.Add(FString::Printf(TEXT("%s Anschluesse: %s"), *B->GetName(), *FString::Join(Teile, TEXT(", "))));
	}
	TArray<FString> NetzListe;
	for (const TPair<int32, int32>& N : Netze)
	{
		NetzListe.Add(FString::Printf(TEXT("%d:%d"), N.Key, N.Value));
	}
	Strom.Add(FString::Printf(TEXT("Netze (Nummer:Bauten) %s"), *FString::Join(NetzListe, TEXT(" "))));
	for (const FString& Z : Strom)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Fabrik-Strom: %s"), *Z);
	}
	for (const FString& Z : Zeilen)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Fabrik-Bericht: %s"), *Z);
	}
	return Zeilen;
}

// ---------------------------------------------------------------------------------------------------------------
// 17.11. (P11, Konzept-Fabrik-Strom-2026-11-17.md, Entscheidungen Marius): echter Strom und Kosten wie beim Spieler

namespace
{
	FString GKosten = TEXT("frei");
	TWeakObjectPtr<UFGPowerCircuit> GNetz;
	float GSicherungUhr = -1.f;
	int32 GSicherungen = 0;
	int32 GUnsichtbar = 0;
	float GSpitze = 0.f;

	const TCHAR* Brenner = TEXT("/Game/FactoryGame/Buildable/Factory/GeneratorBiomass/Build_GeneratorBiomass_Automated.Build_GeneratorBiomass_Automated_C");
	const TCHAR* Lager = TEXT("/Game/FactoryGame/Buildable/Factory/StorageContainerMk1/Build_StorageContainerMk1.Build_StorageContainerMk1_C");
	const TCHAR* Mast = TEXT("/Game/FactoryGame/Buildable/Factory/PowerPoleMk1/Build_PowerPoleMk1.Build_PowerPoleMk1_C");
	const TCHAR* Kabel = TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C");
	const TCHAR* Biomasse = TEXT("/Game/FactoryGame/Resource/Parts/GenericBiomass/Desc_GenericBiomass.Desc_GenericBiomass_C");

	UFGInventoryComponent* SpielerLager(UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const AFGCharacterPlayer* Figur = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
		return Figur ? Figur->GetInventory() : nullptr;
	}

	UFGPowerConnectionComponent* Stromanschluss(AActor* Bau)
	{
		if (AFGBuildablePowerPole* Pfahl = Cast<AFGBuildablePowerPole>(Bau))
		{
			return Pfahl->GetPowerConnections().Num() > 0 ? Pfahl->GetPowerConnections()[0] : nullptr;
		}
		return Bau ? Bau->FindComponentByClass<UFGPowerConnectionComponent>() : nullptr;
	}

	/** Kabel zwischen zwei Anschluessen: echtes Kabel, sonst die versteckte Verbindung der Testbibliothek (gezaehlt, Ziel 0) */
	bool Verkable(UWorld* World, UFGPowerConnectionComponent* A, UFGPowerConnectionComponent* B, TArray<TWeakObjectPtr<AFGBuildable>>& OutBauten)
	{
		if (!A || !B)
		{
			return false;
		}
		AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(World);
		UClass* KabelKlasse = LoadClass<AFGBuildable>(nullptr, Kabel);
		if (Bau && KabelKlasse)
		{
			const FTransform T(A->GetComponentLocation());
			if (AFGBuildableWire* Draht = Cast<AFGBuildableWire>(Bau->BeginSpawnBuildable(KabelKlasse, T)))
			{
				Draht->FinishSpawning(T);
				if (Draht->Connect(A, B))
				{
					OutBauten.Add(Draht);
					return true;
				}
				Draht->Destroy();
			}
		}
		UFGTestBlueprintFunctionLibrary::MakeConnectionBetweenComponents(A, B);
		++GUnsichtbar;
		UE_LOG(LogFicsitTD, Warning, TEXT("Fabrik: Kabel %s -> %s nur unsichtbar verbunden"), *GetNameSafe(A->GetOwner()), *GetNameSafe(B->GetOwner()));
		return true;
	}
}

void TDFabrik::NeuerPlan()
{
	GKosten = TEXT("frei");
	GNetz.Reset();
	GSicherungUhr = -1.f;
	GSicherungen = 0;
	GUnsichtbar = 0;
	GSpitze = 0.f;
}

void TDFabrik::SetzeKosten(const FString& Modus)
{
	GKosten = Modus;
	UE_LOG(LogFicsitTD, Log, TEXT("Fabrik: Kostenmodus %s"), *Modus);
}

bool TDFabrik::KostenPruefen(UWorld* World, UClass* Klasse, const FVector& Ort, FString& OutGrund)
{
	if (GKosten == TEXT("frei"))
	{
		return true;
	}
	UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	const FString Key = UTDEinsatzSubsystem::Schluessel(Klasse);
	if (!Einsatz || Key.IsEmpty())
	{
		return true;   // Logistik (Masten, Kabel, Baender, Fundamente) wie beim Spieler ohne Kontingent
	}
	FString Grund = Einsatz->SperrGrund(Klasse, Ort, 1);
	if (Grund == TEXT("Kontingent"))
	{
		const int32 Vorher = Einsatz->Konto();
		const FString Antwort = Einsatz->Kaufe(Key, 1);
		Grund = Einsatz->SperrGrund(Klasse, Ort, 1);
		UE_LOG(LogFicsitTD, Log, TEXT("Fabrik: Kauf %s fuer den Bau - %s (Konto %d -> %d)"), *Key, *Antwort, Vorher, Einsatz->Konto());
		if (!Grund.IsEmpty())
		{
			OutGrund = FString::Printf(TEXT("fehlt: %s - %s"), *Key, *Antwort);
			return false;
		}
	}
	if (!Grund.IsEmpty())
	{
		OutGrund = FString::Printf(TEXT("gesperrt: %s (%s)"), *Key, *Grund);
		return false;
	}
	// "echt" (Marius 17.11.: wie in echt): das Baumaterial wird aus dem Inventar genommen - der Kauf hat es dorthin gelegt
	if (GKosten == TEXT("echt"))
	{
		TMap<UClass*, int32> Material;
		Einsatz->SammleBaumaterial(Key, 1, Material);
		UFGInventoryComponent* Inventar = SpielerLager(World);
		for (const TPair<UClass*, int32>& M : Material)
		{
			if (!Inventar || Inventar->GetNumItems(M.Key) < M.Value)
			{
				OutGrund = FString::Printf(TEXT("Material fehlt: %s"), *GetNameSafe(M.Key));
				return false;
			}
		}
		for (const TPair<UClass*, int32>& M : Material)
		{
			Inventar->Remove(M.Key, M.Value);
		}
	}
	return true;
}

namespace
{
	/** Freie Rohranschluesse eines Baus */
	TArray<UFGPipeConnectionComponent*> Rohranschluesse(AActor* Bau)
	{
		TArray<UFGPipeConnectionComponent*> Alle, Frei;
		if (Bau)
		{
			Bau->GetComponents<UFGPipeConnectionComponent>(Alle);
		}
		for (UFGPipeConnectionComponent* C : Alle)
		{
			if (C && !C->IsConnected())
			{
				Frei.Add(C);
			}
		}
		return Frei;
	}

	/** Rohr zwischen zwei Bauten: naechstes freies Paar, Fluss A -> B (kein Verbraucher-Anschluss an A, kein Erzeuger an B) */
	AFGBuildable* Rohrverbindung(UClass* RohrKlasse, AActor* A, AActor* B)
	{
		UFGPipeConnectionComponent* BestA = nullptr;
		UFGPipeConnectionComponent* BestB = nullptr;
		for (UFGPipeConnectionComponent* CA : Rohranschluesse(A))
		{
			if (CA->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER || CA->GetPipeConnectionType() == EPipeConnectionType::PCT_SNAP_ONLY)
			{
				continue;
			}
			for (UFGPipeConnectionComponent* CB : Rohranschluesse(B))
			{
				if (CB->GetPipeConnectionType() == EPipeConnectionType::PCT_PRODUCER || CB->GetPipeConnectionType() == EPipeConnectionType::PCT_SNAP_ONLY)
				{
					continue;
				}
				if (!BestA || FVector::DistSquared(CA->GetComponentLocation(), CB->GetComponentLocation()) < FVector::DistSquared(BestA->GetComponentLocation(), BestB->GetComponentLocation()))
				{
					BestA = CA;
					BestB = CB;
				}
			}
		}
		return BestA ? SplineBau(RohrKlasse, BestA, BestB) : nullptr;
	}

	/** Naechster freier fester Knoten einer Ressource (Name enthaelt Erz) bis MaxDist um Ort */
	AFGResourceNode* FreierKnoten(UWorld* World, const FString& Erz, const FVector& Ort, float MaxDist, float& OutDist)
	{
		AFGResourceNode* Knoten = nullptr;
		OutDist = MaxDist;
		for (TActorIterator<AFGResourceNode> It(World); It; ++It)
		{
			const TSubclassOf<UFGResourceDescriptor> Art = It->GetResourceClass();
			if (!Art || It->IsA<AFGResourceDeposit>() || It->IsA<AFGResourceNodeFrackingSatellite>() || It->IsOccupied() || !Art->GetName().Contains(Erz))
			{
				continue;
			}
			const float D = FVector::Dist2D(It->GetActorLocation(), Ort);
			if (D < OutDist)
			{
				OutDist = D;
				Knoten = *It;
			}
		}
		return Knoten;
	}

	/**
	 * 17.11. (P11, Marius: Kohlekraftwerk am Wasser, Kohle ueber Foerderband, laengere Wege wie im echten Spiel).
	 * Naechstes Gewaesser bis 400 m: Wasserpumpen am Ufer, Rohr ueber Kreuzungen zu den Kraftwerken an Land, Kohle-Miner
	 * auf dem naechsten Kohleknoten, Band (mit Zwischenstuecken alle 45 m) ueber eine Splitter-Kette. Anlaufstrom muss wie
	 * im Spiel aus einem vorhandenen Netz kommen (erst "strom": "Biomasse") - die Pumpen brauchen Strom.
	 */
	bool BaueKohle(UWorld* World, ATDDefenseCore* Kern, int32 Anzahl, TArray<TWeakObjectPtr<AFGBuildable>>& InOutBauten, TArray<FString>& OutBericht)
	{
		const FVector KernOrt = Kern->GetActorLocation();
		UClass* GenKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/GeneratorCoal/Build_GeneratorCoal.Build_GeneratorCoal_C"));
		UClass* PumpKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/WaterPump/Build_WaterPump.Build_WaterPump_C"));
		UClass* RohrKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline.Build_Pipeline_C"));
		UClass* KreuzKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/PipeJunction/Build_PipelineJunction_Cross.Build_PipelineJunction_Cross_C"));
		UClass* MinerKlasse = LoadClass<AFGBuildable>(nullptr, Miner);
		UClass* SplitterKlasse = LoadClass<AFGBuildable>(nullptr, Splitter);
		UClass* VerbinderKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/CA_Merger/Build_ConveyorAttachmentMerger.Build_ConveyorAttachmentMerger_C"));
		UClass* Band = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk1/Build_ConveyorBeltMk1.Build_ConveyorBeltMk1_C"));
		if (!GenKlasse || !PumpKlasse || !RohrKlasse || !KreuzKlasse || !MinerKlasse || !SplitterKlasse || !VerbinderKlasse || !Band)
		{
			OutBericht.Add(TEXT("Strom Kohle: Klassen fehlen"));
			return false;
		}
		// Gewaesser: entlang Kern -> Mitte jedes Wasservolumens den ersten Wasserpunkt suchen, das naechste gewinnt
		AFGWaterVolume* Wasser = nullptr;
		FVector Ufer = FVector::ZeroVector, Richtung = FVector::ForwardVector;
		float Oben = 0.f, Bester = 40000.f;
		for (TActorIterator<AFGWaterVolume> It(World); It; ++It)
		{
			FVector Mitte, Ausdehnung;
			It->GetActorBounds(false, Mitte, Ausdehnung);
			const float Flaeche = Mitte.Z + Ausdehnung.Z;
			const FVector R = (FVector(Mitte.X, Mitte.Y, 0.f) - FVector(KernOrt.X, KernOrt.Y, 0.f)).GetSafeNormal2D();
			const float Laenge = FMath::Min(FVector::Dist2D(Mitte, KernOrt), Bester);
			for (float Weg = 0.f; Weg <= Laenge; Weg += 500.f)
			{
				const FVector P(KernOrt.X + R.X * Weg, KernOrt.Y + R.Y * Weg, Flaeche - 100.f);
				if (It->EncompassesPoint(P))
				{
					if (Weg < Bester)
					{
						Bester = Weg;
						Wasser = *It;
						Ufer = FVector(P.X, P.Y, Flaeche);
						Richtung = R;
						Oben = Flaeche;
					}
					break;
				}
			}
		}
		if (!Wasser)
		{
			OutBericht.Add(TEXT("Strom Kohle: kein Gewaesser bis 400 m vom Kern"));
			return false;
		}
		const FVector Quer(-Richtung.Y, Richtung.X, 0.f);
		const FRotator Dreh = Richtung.Rotation();
		const int32 Kraftwerke = FMath::Clamp(Anzahl, 1, 8);
		// Wasser wie im Spiel: 45 m3/min je Kraftwerk, 120 m3/min je Pumpe
		const int32 Pumpen = FMath::DivideAndRoundUp(Kraftwerke * 45, 120);
		TArray<AActor*> PumpListe, GenListe, Kreuze;
		for (int32 k = 0; k < Pumpen; ++k)
		{
			const FVector Ort = Ufer + Richtung * 1500.f + Quer * ((k - (Pumpen - 1) * 0.5f) * 2200.f);
			if (!Wasser->EncompassesPoint(FVector(Ort.X, Ort.Y, Oben - 100.f)))
			{
				OutBericht.Add(FString::Printf(TEXT("Pumpe %d: kein Wasser am Platz"), k + 1));
				continue;
			}
			AFGBuildable* P = Setze(World, PumpKlasse, FTransform(Dreh, FVector(Ort.X, Ort.Y, Oben)), [Wasser](AFGBuildable* B)
			{
				if (AFGBuildableResourceExtractorBase* X = Cast<AFGBuildableResourceExtractorBase>(B))
				{
					X->SetExtractableResource(TScriptInterface<IFGExtractableResourceInterface>(Wasser));
				}
			});
			if (P)
			{
				PumpListe.Add(P);
				InOutBauten.Add(P);
			}
		}
		// Kraftwerke an Land, 25 m vor dem Ufer, quer nebeneinander; je eine Rohrkreuzung davor
		for (int32 k = 0; k < Kraftwerke; ++k)
		{
			const FVector Seite = Quer * ((k - (Kraftwerke - 1) * 0.5f) * 1300.f);
			AFGBuildable* G = Setze(World, GenKlasse, FTransform(Dreh, Boden(World, Ufer - Richtung * 2500.f + Seite)), [](AFGBuildable*) {});
			if (!G)
			{
				OutBericht.Add(FString::Printf(TEXT("Kohlekraftwerk %d nicht gebaut (Kosten/Sperre, siehe Log)"), k + 1));
				break;
			}
			GenListe.Add(G);
			InOutBauten.Add(G);
			AFGBuildable* K = Setze(World, KreuzKlasse, FTransform(Dreh, Boden(World, Ufer - Richtung * 800.f + Seite) + FVector(0.f, 0.f, 150.f)), [](AFGBuildable*) {});
			if (K)
			{
				Kreuze.Add(K);
				InOutBauten.Add(K);
			}
		}
		// Rohre: naechstes freies Anschlusspaar zweier Bauten
		int32 Rohre = 0, RohrFehler = 0;
		auto Rohr = [&](AActor* A, AActor* B)
		{
			AFGBuildable* Neu = Rohrverbindung(RohrKlasse, A, B);
			if (Neu)
			{
				InOutBauten.Add(Neu);
				++Rohre;
			}
			else
			{
				++RohrFehler;
				UE_LOG(LogFicsitTD, Warning, TEXT("Fabrik: Rohr %s -> %s nicht gebaut"), *GetNameSafe(A), *GetNameSafe(B));
			}
		};
		for (int32 k = 0; k < Kreuze.Num(); ++k)
		{
			if (GenListe.IsValidIndex(k))
			{
				Rohr(Kreuze[k], GenListe[k]);
			}
			if (k + 1 < Kreuze.Num())
			{
				Rohr(Kreuze[k], Kreuze[k + 1]);
			}
		}
		for (int32 k = 0; k < PumpListe.Num() && Kreuze.Num() > 0; ++k)
		{
			Rohr(PumpListe[k], Kreuze[FMath::Min(Kreuze.Num() - 1, k * Kreuze.Num() / FMath::Max(1, PumpListe.Num()))]);
		}
		// Kohle: Miner auf dem naechsten Kohleknoten (vom Kraftwerk aus), Band mit Zwischenstuecken, Splitter-Kette
		int32 Baender = 0, Zwischen = 0;
		auto Band1 = [&](AActor* Von, AActor* Nach) -> bool
		{
			const TArray<UFGFactoryConnectionComponent*> Aus = Anschluesse(Von, EFactoryConnectionDirection::FCD_OUTPUT);
			const TArray<UFGFactoryConnectionComponent*> Ein = Anschluesse(Nach, EFactoryConnectionDirection::FCD_INPUT);
			AFGBuildable* B = (Aus.Num() > 0 && Ein.Num() > 0) ? SplineBau(Band, Aus[0], Ein[0]) : nullptr;
			if (!B)
			{
				UE_LOG(LogFicsitTD, Warning, TEXT("Fabrik: Kohleband %s -> %s nicht gebaut"), *GetNameSafe(Von), *GetNameSafe(Nach));
				return false;
			}
			InOutBauten.Add(B);
			++Baender;
			return true;
		};
		float KohleWeg = 0.f;
		const FVector Werk = Ufer - Richtung * 2500.f;
		AFGResourceNode* Knoten = GenListe.Num() > 0 ? FreierKnoten(World, TEXT("Desc_Coal"), Werk, 150000.f, KohleWeg)   /* Test Fenster1117: Grass Fields keine Kohle bis 500 m */ : nullptr;
		AFGBuildable* KohleMiner = nullptr;
		if (Knoten)
		{
			const FRotator DrehM = (Werk - Knoten->GetActorLocation()).GetSafeNormal2D().Rotation();
			KohleMiner = Setze(World, MinerKlasse, FTransform(DrehM, Knoten->GetActorLocation()), [Knoten](AFGBuildable* B)
			{
				if (AFGBuildableResourceExtractorBase* X = Cast<AFGBuildableResourceExtractorBase>(B))
				{
					X->SetExtractableResource(TScriptInterface<IFGExtractableResourceInterface>(Knoten));
				}
			});
		}
		if (KohleMiner)
		{
			InOutBauten.Add(KohleMiner);
			// Splitter-Kette 12 m hinter den Kraftwerken (vom Wasser weg)
			TArray<AActor*> Verteiler;
			for (int32 k = 0; k + 1 < GenListe.Num(); ++k)
			{
				const FVector Ort = Boden(World, Werk - Richtung * 1600.f + Quer * ((k - (GenListe.Num() - 1) * 0.5f) * 1300.f)) + FVector(0.f, 0.f, 100.f);
				if (AFGBuildable* S = Setze(World, SplitterKlasse, FTransform(FRotator(0.f, Quer.Rotation().Yaw, 0.f), Ort), [](AFGBuildable*) {}))
				{
					Verteiler.Add(S);
					InOutBauten.Add(S);
				}
			}
			// Weg vom Miner zum ersten Verteiler (oder Kraftwerk): alle 45 m ein Zwischenstueck (Verbinder) wie ein Spieler mit Bandstuetzen
			AActor* Ziel = Verteiler.Num() > 0 ? Verteiler[0] : GenListe[0];
			AActor* Zufluss = KohleMiner;
			const FVector Start = KohleMiner->GetActorLocation();
			const float Weg = FVector::Dist2D(Start, Ziel->GetActorLocation());
			const int32 Stuecke = FMath::Max(0, FMath::CeilToInt(Weg / 4500.f) - 1);
			for (int32 i = 1; i <= Stuecke; ++i)
			{
				const FVector Ort = Boden(World, FMath::Lerp(Start, Ziel->GetActorLocation(), float(i) / (Stuecke + 1))) + FVector(0.f, 0.f, 100.f);
				const FRotator DrehZ = (Ziel->GetActorLocation() - Start).GetSafeNormal2D().Rotation();
				AFGBuildable* V = Setze(World, VerbinderKlasse, FTransform(DrehZ, Ort), [](AFGBuildable*) {});
				if (!V)
				{
					break;
				}
				InOutBauten.Add(V);
				++Zwischen;
				Band1(Zufluss, V);
				Zufluss = V;
			}
			Band1(Zufluss, Ziel);
			for (int32 k = 0; k < Verteiler.Num(); ++k)
			{
				Band1(Verteiler[k], GenListe[k]);
				Band1(Verteiler[k], k + 1 < Verteiler.Num() ? Verteiler[k + 1] : GenListe.Last());
			}
		}
		else if (GenListe.Num() > 0)
		{
			OutBericht.Add(TEXT("Strom Kohle: kein freier Kohleknoten bis 1,5 km oder Miner nicht gebaut"));
		}
		const int32 Masten = TDFabrik::VerbindeNetz(World, InOutBauten, OutBericht);
		float Kapazitaet = 0.f;
		for (AActor* G : GenListe)
		{
			Kapazitaet += Cast<AFGBuildableGenerator>(G)->GetPowerProductionCapacity();
		}
		OutBericht.Add(FString::Printf(TEXT("Strom Kohle: %d Kraftwerke, %d Pumpen, Gewaesser %.0f m vom Kern, %d Rohre (%d Fehler), Kohleknoten %.0f m vom Werk, %d Baender, %d Zwischenstuecke, %d Masten, %d unsichtbare Kabel, Kapazitaet beim Bau %.0f MW"),
			GenListe.Num(), PumpListe.Num(), Bester / 100.f, Rohre, RohrFehler, KohleWeg / 100.f, Baender, Zwischen, Masten, GUnsichtbar, Kapazitaet));
		for (const FString& Z : OutBericht)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Fabrik: %s"), *Z);
		}
		return GenListe.Num() > 0 && PumpListe.Num() > 0 && KohleMiner && RohrFehler == 0;
	}
}

namespace
{
	/**
	 * 17.11. (P11, Marius: Treibstoff-Strom mit einbauen, laengere Wege wie im echten Spiel). Wie ein Spieler am Oelfeld:
	 * Oelpumpe auf dem naechsten freien Oelknoten (bis 5 km), Rohr ueber eine Kreuzung zu den Raffinerien (Rezept Treibstoff,
	 * 60 Oel -> 40 Treibstoff + 30 Harz), Harz per Band in je ein Lager, Treibstoff ueber Kreuzungen zu den Kraftwerken
	 * (20 m3/min je Kraftwerk), lange Leitung mit Zwischenmasten ans vorhandene Netz (Anlaufstrom fuer Pumpe und Raffinerie).
	 */
	bool BaueTreibstoff(UWorld* World, ATDDefenseCore* Kern, int32 Anzahl, TArray<TWeakObjectPtr<AFGBuildable>>& InOutBauten, TArray<FString>& OutBericht)
	{
		const FVector KernOrt = Kern->GetActorLocation();
		UClass* PumpKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/OilPump/Build_OilPump.Build_OilPump_C"));
		UClass* RaffKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/OilRefinery/Build_OilRefinery.Build_OilRefinery_C"));
		UClass* GenKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/GeneratorFuel/Build_GeneratorFuel.Build_GeneratorFuel_C"));
		UClass* RohrKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline.Build_Pipeline_C"));
		UClass* KreuzKlasse = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/PipeJunction/Build_PipelineJunction_Cross.Build_PipelineJunction_Cross_C"));
		UClass* LagerKlasse = LoadClass<AFGBuildable>(nullptr, Lager);
		UClass* Band = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk1/Build_ConveyorBeltMk1.Build_ConveyorBeltMk1_C"));
		UClass* Rezept = LoadClass<UFGRecipe>(nullptr, TEXT("/Game/FactoryGame/Recipes/OilRefinery/Recipe_LiquidFuel.Recipe_LiquidFuel_C"));
		if (!PumpKlasse || !RaffKlasse || !GenKlasse || !RohrKlasse || !KreuzKlasse || !LagerKlasse || !Band || !Rezept)
		{
			OutBericht.Add(TEXT("Strom Treibstoff: Klassen fehlen"));
			return false;
		}
		float Weg = 0.f;
		AFGResourceNode* Knoten = FreierKnoten(World, TEXT("Desc_LiquidOil"), KernOrt, 500000.f, Weg);   // Test Fenster1117: auf Grass Fields kein Oel bis 2 km
		if (!Knoten)
		{
			OutBericht.Add(TEXT("Strom Treibstoff: kein freier Oelknoten bis 5 km"));
			return false;
		}
		const FVector Feld = Knoten->GetActorLocation();
		const FVector Richtung = (FVector(KernOrt.X, KernOrt.Y, 0.f) - FVector(Feld.X, Feld.Y, 0.f)).GetSafeNormal2D();
		const FVector Quer(-Richtung.Y, Richtung.X, 0.f);
		const FRotator Dreh = Richtung.Rotation();
		const int32 Kraftwerke = FMath::Clamp(Anzahl, 1, 8);
		// eine Oelpumpe Mk1 auf normalem Knoten = 120 Oel/min = 2 Raffinerien = 80 Treibstoff/min = 4 Kraftwerke
		const int32 Raffinerien = FMath::Min(2, FMath::DivideAndRoundUp(Kraftwerke, 2));
		if (Kraftwerke > 4)
		{
			OutBericht.Add(FString::Printf(TEXT("Strom Treibstoff: ein Oelknoten reicht fuer 4 Kraftwerke, %d bestellt"), Kraftwerke));
		}
		auto Reihe = [&](float Vor, int32 k, int32 n, float Abstand)
		{
			return Boden(World, Feld + Richtung * Vor + Quer * ((k - (n - 1) * 0.5f) * Abstand));
		};
		AFGBuildable* Pumpe = Setze(World, PumpKlasse, FTransform(Dreh, Feld), [Knoten](AFGBuildable* B)
		{
			if (AFGBuildableResourceExtractorBase* X = Cast<AFGBuildableResourceExtractorBase>(B))
			{
				X->SetExtractableResource(TScriptInterface<IFGExtractableResourceInterface>(Knoten));
			}
		});
		if (!Pumpe)
		{
			OutBericht.Add(TEXT("Strom Treibstoff: Oelpumpe nicht gebaut (Kosten/Sperre, siehe Log)"));
			return false;
		}
		InOutBauten.Add(Pumpe);
		int32 Rohre = 0, RohrFehler = 0, Baender = 0;
		auto Rohr = [&](AActor* A, AActor* B)
		{
			if (AFGBuildable* Neu = (A && B) ? Rohrverbindung(RohrKlasse, A, B) : nullptr)
			{
				InOutBauten.Add(Neu);
				++Rohre;
				return;
			}
			++RohrFehler;
			UE_LOG(LogFicsitTD, Warning, TEXT("Fabrik: Rohr %s -> %s nicht gebaut"), *GetNameSafe(A), *GetNameSafe(B));
		};
		auto Kreuz = [&](const FVector& Ort) -> AActor*
		{
			AFGBuildable* K = Setze(World, KreuzKlasse, FTransform(Dreh, Ort + FVector(0.f, 0.f, 150.f)), [](AFGBuildable*) {});
			if (K)
			{
				InOutBauten.Add(K);
			}
			return K;
		};
		// Reihen vom Knoten zum Kern: Pumpe 0, Oelkreuzung 14 m, Raffinerien 26 m, Harzlager 40 m, Treibstoffkreuzungen 50 m, Kraftwerke 64 m
		AActor* OelKreuz = Kreuz(Reihe(1400.f, 0, 1, 0.f));
		Rohr(Pumpe, OelKreuz);
		TArray<AActor*> RaffListe, GenListe, Kreuze;
		int32 LagerZahl = 0;
		for (int32 k = 0; k < Raffinerien; ++k)
		{
			AFGBuildable* R = Setze(World, RaffKlasse, FTransform(Dreh, Reihe(2600.f, k, Raffinerien, 1400.f)), [](AFGBuildable*) {});
			if (!R)
			{
				OutBericht.Add(FString::Printf(TEXT("Raffinerie %d nicht gebaut (Kosten/Sperre, siehe Log)"), k + 1));
				break;
			}
			if (AFGBuildableManufacturer* Fab = Cast<AFGBuildableManufacturer>(R))
			{
				Fab->SetRecipe(Rezept);
			}
			RaffListe.Add(R);
			InOutBauten.Add(R);
			Rohr(OelKreuz, R);
			// Harz wie beim Spieler ins Lager (sonst laeuft die Raffinerie voll und steht)
			if (AFGBuildable* L = Setze(World, LagerKlasse, FTransform(Dreh, Reihe(4000.f, k, Raffinerien, 1400.f)), [](AFGBuildable*) {}))
			{
				InOutBauten.Add(L);
				++LagerZahl;
				const TArray<UFGFactoryConnectionComponent*> Aus = Anschluesse(R, EFactoryConnectionDirection::FCD_OUTPUT);
				const TArray<UFGFactoryConnectionComponent*> Ein = Anschluesse(L, EFactoryConnectionDirection::FCD_INPUT);
				if (AFGBuildable* B = (Aus.Num() > 0 && Ein.Num() > 0) ? SplineBau(Band, Aus[0], Ein[0]) : nullptr)
				{
					InOutBauten.Add(B);
					++Baender;
				}
				else
				{
					UE_LOG(LogFicsitTD, Warning, TEXT("Fabrik: Harzband %s -> %s nicht gebaut"), *GetNameSafe(R), *GetNameSafe(L));
				}
			}
		}
		for (int32 k = 0; k < Kraftwerke && RaffListe.Num() > 0; ++k)
		{
			AFGBuildable* G = Setze(World, GenKlasse, FTransform(Dreh, Reihe(6400.f, k, Kraftwerke, 2200.f)), [](AFGBuildable*) {});
			if (!G)
			{
				OutBericht.Add(FString::Printf(TEXT("Treibstoffkraftwerk %d nicht gebaut (Kosten/Sperre, siehe Log)"), k + 1));
				break;
			}
			GenListe.Add(G);
			InOutBauten.Add(G);
			if (AActor* K = Kreuz(Reihe(5000.f, k, Kraftwerke, 2200.f)))
			{
				Kreuze.Add(K);
			}
		}
		for (int32 k = 0; k < Kreuze.Num(); ++k)
		{
			if (GenListe.IsValidIndex(k))
			{
				Rohr(Kreuze[k], GenListe[k]);
			}
			if (k + 1 < Kreuze.Num())
			{
				Rohr(Kreuze[k], Kreuze[k + 1]);
			}
		}
		for (int32 k = 0; k < RaffListe.Num() && Kreuze.Num() > 0; ++k)
		{
			Rohr(RaffListe[k], Kreuze[FMath::Min(Kreuze.Num() - 1, k * Kreuze.Num() / RaffListe.Num())]);
		}
		const int32 Masten = TDFabrik::VerbindeNetz(World, InOutBauten, OutBericht);
		float Kapazitaet = 0.f;
		for (AActor* G : GenListe)
		{
			Kapazitaet += Cast<AFGBuildableGenerator>(G)->GetPowerProductionCapacity();
		}
		OutBericht.Add(FString::Printf(TEXT("Strom Treibstoff: Oelknoten %.0f m vom Kern, 1 Oelpumpe, %d Raffinerien, %d Harzlager, %d Kraftwerke, %d Rohre (%d Fehler), %d Baender, %d Masten, %d unsichtbare Kabel, Kapazitaet beim Bau %.0f MW"),
			Weg / 100.f, RaffListe.Num(), LagerZahl, GenListe.Num(), Rohre, RohrFehler, Baender, Masten, GUnsichtbar, Kapazitaet));
		for (const FString& Z : OutBericht)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Fabrik: %s"), *Z);
		}
		return RaffListe.Num() > 0 && GenListe.Num() > 0 && RohrFehler == 0;
	}
}

bool TDFabrik::BaueStrom(UWorld* World, ATDDefenseCore* Kern, const FString& Art, int32 Anzahl,
	TArray<TWeakObjectPtr<AFGBuildable>>& InOutBauten, TArray<FString>& OutBericht)
{
	if (!World || !Kern)
	{
		return false;
	}
	if (Art == TEXT("Kohle"))
	{
		if (AFGGameState* Zustand = World->GetGameState<AFGGameState>())
		{
			Zustand->SetCheatNoPower(false);
		}
		return BaueKohle(World, Kern, Anzahl, InOutBauten, OutBericht);
	}
	if (Art == TEXT("Treibstoff"))
	{
		if (AFGGameState* Zustand = World->GetGameState<AFGGameState>())
		{
			Zustand->SetCheatNoPower(false);
		}
		return BaueTreibstoff(World, Kern, Anzahl, InOutBauten, OutBericht);
	}
	if (Art != TEXT("Biomasse"))
	{
		OutBericht.Add(FString::Printf(TEXT("Strom %s gibt es noch nicht (Biomasse, Kohle, Treibstoff)"), *Art));
		return false;
	}
	// Cheat aus: ein vergessener Schritt "stromfrei" verfaelschte sonst still alles
	if (AFGGameState* Zustand = World->GetGameState<AFGGameState>())
	{
		Zustand->SetCheatNoPower(false);
	}
	const FVector KernOrt = Kern->GetActorLocation();
	// Kraftwerksplatz 30 m vom Kern, gegenueber der Fabrik (die laeuft zum Eisenknoten)
	FVector Richtung = FVector::ForwardVector;
	for (const TWeakObjectPtr<AFGBuildable>& W : InOutBauten)
	{
		if (W.IsValid() && W->IsA<AFGBuildableResourceExtractorBase>())
		{
			Richtung = -(W->GetActorLocation() - KernOrt).GetSafeNormal2D();
			break;
		}
	}
	const FVector Quer(-Richtung.Y, Richtung.X, 0.f);
	const FRotator Dreh = Richtung.Rotation();
	FString Grund;
	UClass* LagerKlasse = LoadClass<AFGBuildable>(nullptr, Lager);
	UClass* BrennerKlasse = LoadClass<AFGBuildable>(nullptr, Brenner);
	UClass* SplitterKlasse = LoadClass<AFGBuildable>(nullptr, Splitter);
	UClass* Band = LoadClass<AFGBuildable>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk1/Build_ConveyorBeltMk1.Build_ConveyorBeltMk1_C"));
	UClass* MastKlasse = LoadClass<AFGBuildable>(nullptr, Mast);
	if (!LagerKlasse || !BrennerKlasse || !SplitterKlasse || !Band || !MastKlasse)
	{
		OutBericht.Add(TEXT("Strom: Klassen fehlen"));
		return false;
	}
	// Lager mit der Biomasse aus dem Inventar (Marius: Lager mit Band)
	const FVector LagerOrt = Boden(World, KernOrt + Richtung * 3600.f);
	AFGBuildable* LagerBau = Setze(World, LagerKlasse, FTransform(Dreh, LagerOrt), [](AFGBuildable*) {});
	if (!LagerBau)
	{
		OutBericht.Add(TEXT("Lager nicht gebaut (Kosten, siehe Log)"));
		return false;
	}
	InOutBauten.Add(LagerBau);
	int32 Eingelagert = 0;
	if (AFGBuildableStorage* Speicher = Cast<AFGBuildableStorage>(LagerBau))
	{
		UClass* Bio = LoadClass<UFGItemDescriptor>(nullptr, Biomasse);
		UFGInventoryComponent* Inventar = SpielerLager(World);
		if (Bio && Inventar && Speicher->GetStorageInventory())
		{
			const int32 Hat = Inventar->GetNumItems(Bio);
			Eingelagert = Hat > 0 ? Speicher->GetStorageInventory()->AddStack(FInventoryStack(Hat, Bio), true) : 0;
			if (Eingelagert > 0)
			{
				Inventar->Remove(Bio, Eingelagert);
			}
		}
	}
	// Brenner in einer Reihe, Splitter-Kette davor (wie bei den Tuermen)
	TArray<AActor*> BrennerListe;
	for (int32 k = 0; k < FMath::Max(1, Anzahl); ++k)
	{
		const FVector Ort = Boden(World, KernOrt + Richtung * 4600.f + Quer * ((k - (Anzahl - 1) * 0.5f) * 900.f));
		AFGBuildable* B = Setze(World, BrennerKlasse, FTransform(Dreh, Ort), [](AFGBuildable*) {});
		if (!B)
		{
			OutBericht.Add(FString::Printf(TEXT("Brenner %d nicht gebaut (Kosten, siehe Log)"), k + 1));
			break;
		}
		BrennerListe.Add(B);
		InOutBauten.Add(B);
	}
	int32 Baender = 0;
	auto VerbindeBand = [&](AActor* Von, AActor* Nach) -> bool
	{
		const TArray<UFGFactoryConnectionComponent*> Aus = Anschluesse(Von, EFactoryConnectionDirection::FCD_OUTPUT);
		const TArray<UFGFactoryConnectionComponent*> Ein = Anschluesse(Nach, EFactoryConnectionDirection::FCD_INPUT);
		if (Aus.Num() == 0 || Ein.Num() == 0)
		{
			return false;
		}
		if (AFGBuildable* B = SplineBau(Band, Aus[0], Ein[0]))
		{
			InOutBauten.Add(B);
			++Baender;
			return true;
		}
		return false;
	};
	// 17.11. (Marius: gleichmaessig aufteilen wie ein guter Spieler): Verteilbaum statt Kette - jeder Splitter teilt auf 2 oder 3
	// Straenge, Ort zwischen Lager (36 m) und Brennern (46 m) quer bei der Mitte der versorgten Brenner
	TFunction<void(AActor*, const TArray<AActor*>&, int32)> Verteile;
	Verteile = [&](AActor* Zufluss, const TArray<AActor*>& Gruppe, int32 Tiefe)
	{
		if (Gruppe.Num() == 1)
		{
			VerbindeBand(Zufluss, Gruppe[0]);
			return;
		}
		const int32 Teile = Gruppe.Num() % 3 == 0 ? 3 : 2;
		float QuerMitte = 0.f;
		for (const AActor* G : Gruppe)
		{
			QuerMitte += FVector::DotProduct(G->GetActorLocation() - KernOrt, Quer) / Gruppe.Num();
		}
		const FVector Ort = Boden(World, KernOrt + Richtung * (3900.f + 250.f * Tiefe) + Quer * QuerMitte) + FVector(0.f, 0.f, 100.f);
		AFGBuildable* S = Setze(World, SplitterKlasse, FTransform(Dreh, Ort), [](AFGBuildable*) {});
		if (!S)
		{
			return;
		}
		InOutBauten.Add(S);
		VerbindeBand(Zufluss, S);
		int32 Start = 0;
		for (int32 t = 0; t < Teile; ++t)
		{
			const int32 Groesse = Gruppe.Num() / Teile + (t < Gruppe.Num() % Teile ? 1 : 0);
			TArray<AActor*> Teil;
			for (int32 k = 0; k < Groesse; ++k)
			{
				Teil.Add(Gruppe[Start + k]);
			}
			Start += Groesse;
			Verteile(S, Teil, Tiefe + 1);
		}
	};
	if (BrennerListe.Num() > 0)
	{
		Verteile(LagerBau, BrennerListe, 0);
	}
	const int32 Masten = VerbindeNetz(World, InOutBauten, OutBericht);
	float Kapazitaet = 0.f;
	for (AActor* B : BrennerListe)
	{
		if (const AFGBuildableGenerator* G = Cast<AFGBuildableGenerator>(B))
		{
			Kapazitaet += G->GetPowerProductionCapacity();
		}
	}
	OutBericht.Add(FString::Printf(TEXT("Strom Biomasse: %d Brenner, Lager mit %d Biomasse, %d Baender, %d Masten, %d unsichtbare Kabel, Kapazitaet beim Bau %.0f MW"),
		BrennerListe.Num(), Eingelagert, Baender, Masten, GUnsichtbar, Kapazitaet));
	for (const FString& Z : OutBericht)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Fabrik: %s"), *Z);
	}
	return BrennerListe.Num() > 0;
}

int32 TDFabrik::VerbindeNetz(UWorld* World, TArray<TWeakObjectPtr<AFGBuildable>>& InOutBauten, TArray<FString>& OutBericht)
{
	UClass* MastKlasse = LoadClass<AFGBuildable>(nullptr, Mast);
	if (!World || !MastKlasse)
	{
		return 0;
	}
	// alle Pruefstand-Bauten mit Stromanschluss, die noch kein Netz mit Erzeuger haben (Generatoren zuerst)
	TArray<AFGBuildable*> Offen;
	TArray<AFGBuildable*> Erzeuger;
	for (const TWeakObjectPtr<AFGBuildable>& W : InOutBauten)
	{
		AFGBuildable* B = W.Get();
		UFGPowerConnectionComponent* C = Stromanschluss(B);
		if (!B || !C || B->IsA<AFGBuildablePowerPole>() || B->IsA<AFGBuildableWire>() || C->GetNumConnections() > 0)
		{
			continue;
		}
		(B->IsA<AFGBuildableGenerator>() ? Erzeuger : Offen).Add(B);
	}
	// Turmgruppen: auch Tuerme ausserhalb der Fabrik-Liste (Schritt "baue"), die noch an keinem Netz haengen
	for (TActorIterator<ATDTurretBase> It(World); It; ++It)
	{
		UFGPowerConnectionComponent* C = Stromanschluss(*It);
		if (C && C->GetNumConnections() == 0 && !Offen.Contains(*It))
		{
			Offen.Add(*It);
		}
	}
	const float Reichweite = FMath::Max(1000.f, GetDefault<AFGBuildableWire>(LoadClass<AFGBuildable>(nullptr, Kabel))->mMaxLength * 0.9f);
	TArray<AFGBuildablePowerPole*> Masten;
	for (const TWeakObjectPtr<AFGBuildable>& W : InOutBauten)
	{
		if (AFGBuildablePowerPole* P = Cast<AFGBuildablePowerPole>(W.Get()))
		{
			Masten.Add(P);
		}
	}
	const int32 MastenVorher = Masten.Num();
	auto NeuerMast = [&](const FVector& Ort) -> AFGBuildablePowerPole*
	{
		AFGBuildablePowerPole* P = Cast<AFGBuildablePowerPole>(Setze(World, MastKlasse, FTransform(Boden(World, Ort)), [](AFGBuildable*) {}));
		if (P)
		{
			Masten.Add(P);
			InOutBauten.Add(P);
		}
		return P;
	};
	auto Naechster = [&](const FVector& Ort) -> AFGBuildablePowerPole*
	{
		AFGBuildablePowerPole* Best = nullptr;
		for (AFGBuildablePowerPole* P : Masten)
		{
			if (!Best || FVector::DistSquared(P->GetActorLocation(), Ort) < FVector::DistSquared(Best->GetActorLocation(), Ort))
			{
				Best = P;
			}
		}
		return Best;
	};
	// Stammleitung: jeder neue Mast an den naechsten vorhandenen, bei zu grosser Entfernung Zwischenmasten
	auto AnsNetz = [&](AFGBuildablePowerPole* Neu)
	{
		AFGBuildablePowerPole* Ziel = nullptr;
		for (AFGBuildablePowerPole* P : Masten)
		{
			if (P != Neu && (!Ziel || FVector::DistSquared(P->GetActorLocation(), Neu->GetActorLocation()) < FVector::DistSquared(Ziel->GetActorLocation(), Neu->GetActorLocation())))
			{
				Ziel = P;
			}
		}
		AFGBuildablePowerPole* Von = Neu;
		while (Ziel && FVector::Dist(Von->GetActorLocation(), Ziel->GetActorLocation()) > Reichweite)
		{
			const FVector Schritt = Von->GetActorLocation() + (Ziel->GetActorLocation() - Von->GetActorLocation()).GetSafeNormal() * Reichweite * 0.95f;
			AFGBuildablePowerPole* Zwischen = NeuerMast(Schritt);
			if (!Zwischen)
			{
				break;
			}
			Verkable(World, Stromanschluss(Von), Stromanschluss(Zwischen), InOutBauten);
			Von = Zwischen;
		}
		if (Ziel)
		{
			Verkable(World, Stromanschluss(Von), Stromanschluss(Ziel), InOutBauten);
		}
	};
	auto Haenge = [&](AFGBuildable* B)
	{
		AFGBuildablePowerPole* P = Naechster(B->GetActorLocation());
		UFGPowerConnectionComponent* PC = P ? Stromanschluss(P) : nullptr;
		if (!P || FVector::Dist(P->GetActorLocation(), B->GetActorLocation()) > 2500.f || !PC || PC->GetNumFreeConnections() <= 1)
		{
			P = NeuerMast(B->GetActorLocation() + FVector(400.f, 400.f, 0.f));
			if (!P)
			{
				return;
			}
			AnsNetz(P);
		}
		Verkable(World, Stromanschluss(B), Stromanschluss(P), InOutBauten);
	};
	for (AFGBuildable* B : Erzeuger)
	{
		Haenge(B);
	}
	for (AFGBuildable* B : Offen)
	{
		Haenge(B);
	}
	return Masten.Num() - MastenVorher;
}

TArray<FString> TDFabrik::Ueberwache(UWorld* World, const TArray<TWeakObjectPtr<AFGBuildable>>& Bauten, float Dt)
{
	TArray<FString> Meldungen;
	UFGPowerCircuit* Netz = GNetz.Get();
	if (!Netz)
	{
		for (const TWeakObjectPtr<AFGBuildable>& W : Bauten)
		{
			const AFGBuildableGenerator* G = Cast<AFGBuildableGenerator>(W.Get());
			if (G && G->GetPowerInfo() && G->GetPowerInfo()->GetPowerCircuit())
			{
				Netz = G->GetPowerInfo()->GetPowerCircuit();
				GNetz = Netz;
				break;
			}
		}
	}
	if (!Netz)
	{
		return Meldungen;
	}
	FPowerCircuitStats Stand;
	Netz->GetStats(Stand);
	GSpitze = FMath::Max(GSpitze, Stand.PowerConsumed);
	if (Netz->IsFuseTriggered())
	{
		if (GSicherungUhr < 0.f)
		{
			GSicherungUhr = 0.f;
			++GSicherungen;
			Meldungen.Add(FString::Printf(TEXT("Sicherung ausgeloest (%d.), Verbrauch %.0f / Kapazitaet %.0f MW"), GSicherungen, Stand.PowerConsumed, Stand.PowerProductionCapacity));
			UE_LOG(LogFicsitTD, Warning, TEXT("Fabrik: %s"), *Meldungen.Last());
		}
		GSicherungUhr += Dt;
		// Marius 17.11.: nach 20 s zuruecksetzen wie ein Spieler, der zum Schalter laeuft
		if (GSicherungUhr >= TDBalance::Wert(TEXT("Test.SicherungZurueck"), 20.f))
		{
			APlayerController* PC = World->GetFirstPlayerController();
			Netz->ResetFuse(Cast<AFGPlayerController>(PC));
			GSicherungUhr = -1.f;
			UE_LOG(LogFicsitTD, Log, TEXT("Fabrik: Sicherung zurueckgesetzt"));
		}
	}
	return Meldungen;
}

TArray<FString> TDFabrik::StromBericht()
{
	TArray<FString> Zeilen;
	if (UFGPowerCircuit* Netz = GNetz.Get())
	{
		FPowerCircuitStats Stand;
		Netz->GetStats(Stand);
		Zeilen.Add(FString::Printf(TEXT("Netz: Kapazitaet %.0f MW, Verbrauch %.0f MW, Spitze %.0f MW, Sicherung %d-mal, unsichtbare Kabel %d"),
			Stand.PowerProductionCapacity, Stand.PowerConsumed, GSpitze, GSicherungen, GUnsichtbar));
	}
	return Zeilen;
}
