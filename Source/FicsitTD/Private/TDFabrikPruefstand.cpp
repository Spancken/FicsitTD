#include "TDFabrikPruefstand.h"
#include "FicsitTDModule.h"
#include "TDDefenseCore.h"
#include "TDTurretBase.h"
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

	AFGBuildable* Setze(UWorld* World, UClass* Klasse, const FTransform& T, TFunctionRef<void(AFGBuildable*)> VorFertig)
	{
		AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(World);
		if (!Bau || !Klasse)
		{
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
	TArray<TWeakObjectPtr<AFGBuildable>>& OutBauten, TArray<FString>& OutBericht)
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
		OutBericht.Add(TEXT("kein freier Eisenerz-Knoten bis 150 m"));
		return false;
	}
	const FVector Richtung = (FVector(Knoten->GetActorLocation().X, Knoten->GetActorLocation().Y, KernOrt.Z) - KernOrt).GetSafeNormal2D();
	const FVector Quer = FVector(-Richtung.Y, Richtung.X, 0.f);
	const FRotator Dreh = Richtung.Rotation();
	OutBericht.Add(FString::Printf(TEXT("Knoten %s %.0f m vom Kern"), *Knoten->GetName(), Bester / 100.f));

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
		AFGBuildable* B = UFGTestBlueprintFunctionLibrary::SpawnSplineBuildable(Band, Aus[AusIndex], Ein[0], nullptr);
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

	// B2: Miner auf dem Knoten
	AFGBuildable* MinerBau = Setze(World, MinerKlasse, FTransform(Dreh, Knoten->GetActorLocation()), [Knoten](AFGBuildable* B)
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

	// B3: Maschinen auf einer Linie zwischen Kern und Knoten, 30 m vom Kern an, je 14 m
	AActor* Vorher = MinerBau;
	const float Abstand = FMath::Min(Bester - 2500.f, 3000.f + 1400.f * UE_ARRAY_COUNT(Kette));
	for (int32 i = 0; i < UE_ARRAY_COUNT(Kette); ++i)
	{
		const FVector Ort = Boden(World, KernOrt + Richtung * (Abstand - 1400.f * i));
		UClass* Klasse = LoadClass<AFGBuildable>(nullptr, Kette[i].Maschine);
		UClass* Rezept = LoadClass<UFGRecipe>(nullptr, Kette[i].Rezept);
		AFGBuildable* M = Setze(World, Klasse, FTransform(Dreh + FRotator(0.f, 180.f, 0.f), Ort), [](AFGBuildable*) {});
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
		Verbinde(Vorher, M);
		Vorher = M;
	}

	// Tuerme im Halbkreis 20 m um den Kern auf der Fabrikseite, Splitter-Kette 26 m vom Kern davor
	TArray<AActor*> TurmListe;
	for (int32 k = 0; k < Tuerme; ++k)
	{
		const float Winkel = (Tuerme > 1 ? -60.f + 120.f * k / (Tuerme - 1) : 0.f);
		const FVector Ort = Boden(World, KernOrt + Richtung.RotateAngleAxis(Winkel, FVector::UpVector) * 2000.f);
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
	if (TurmListe.Num() == 1)
	{
		Verbinde(Vorher, TurmListe[0]);
	}
	else
	{
		// Splitter-Kette: Splitter k gibt links an Turm k, in der Mitte weiter; der letzte gibt links und rechts an die letzten zwei
		AActor* Zufluss = Vorher;
		const int32 Anzahl = TurmListe.Num() - 1;
		for (int32 k = 0; k < Anzahl; ++k)
		{
			const FVector Ort = Boden(World, KernOrt + Richtung * 2600.f + Quer * ((k - (Anzahl - 1) * 0.5f) * 600.f)) + FVector(0.f, 0.f, 100.f);
			AFGBuildable* S = Setze(World, SplitterKlasse, FTransform(FRotator(0.f, (Quer).Rotation().Yaw, 0.f), Ort), [](AFGBuildable*) {});
			if (!S)
			{
				OutBericht.Add(TEXT("Splitter nicht gebaut"));
				return false;
			}
			OutBauten.Add(S);
			Verbinde(Zufluss, S, 0);   // vom Konstrukteur sein Ausgang, vom vorigen Splitter der erste freie (Output2, die Mitte)
			Verbinde(S, TurmListe[k], 0);                       // links (Output1)
			if (k == Anzahl - 1)
			{
				// letzter Splitter: die Mitte ist noch frei (Index 0 nach dem Verbinden von Output1), rechts bleibt Reserve
				Verbinde(S, TurmListe[k + 1], 0);
			}
			Zufluss = S;
		}
	}
	OutBericht.Add(FString::Printf(TEXT("Linie Bolzen: Miner, %d Maschinen, %d Tuerme, %d Baender Mk%d, %d Bandfehler"),
		UE_ARRAY_COUNT(Kette), TurmListe.Num(), Baender, BandMk, BandFehler));
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
	for (const FString& Z : Zeilen)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Fabrik-Bericht: %s"), *Z);
	}
	return Zeilen;
}
