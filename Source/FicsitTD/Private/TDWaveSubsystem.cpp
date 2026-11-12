#include "TDWaveSubsystem.h"
#include "Engine/StaticMeshActor.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "TDSprache.h"
#include "TDSicherheitsnetz.h"
#include "TDKernplatz.h"
#include "TDLaufProtokoll.h"
#include "FGItemPickup.h"
#include "TDBalance.h"
#include "TDForschung.h"
#include "TDWellenStatistik.h"
#include "TDEinsatz.h"
#include "Buildables/FGBuildable.h"
#include "FGMapManager.h"
#include "FGMapMarker.h"
#include "FGIconDatabaseSubsystem.h"
#include "Resources/FGItemDescriptor.h"
#include "FicsitTDModule.h"
#include "TDDamageable.h"
#include "TDDecoy.h"
#include "TDTurretBase.h"
#include "TDDefenseCore.h"
#include "FGChatManager.h"
#include "FGHealthComponent.h"
#include "FGCreatureSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/CoreDelegates.h"

/** T2h: Zahl der Speicherbereinigungen (Garbage Collection) seit Spielstart */
static int32 GTDSpeicherbereinigungen = 0;

/** T2i: Tick-Sonde, die zu Beginn ihrer Tick-Gruppe (hohe Prioritaet) einen Zeitstempel setzt */
struct FTDPhasenSonde : public FTickFunction
{
	TWeakObjectPtr<UTDWaveSubsystem> Ziel;
	int32 Phase = 0;
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override
	{
		if (UTDWaveSubsystem* Z = Ziel.Get())
		{
			Z->Stempel(Phase);
		}
	}
	virtual FString DiagnosticMessage() override { return TEXT("FicsitTD Phasensonde"); }
};

/** Phasen: 0 Bildbeginn, 1 Weltstart, 2 TG_PrePhysics, 3 TG_DuringPhysics, 4 TG_PostPhysics, 5 TG_PostUpdateWork, 6 Welt fertig, 7 Bildende */
static const TCHAR* const GTDPhasenNamen[] = {
	TEXT("Vorlauf"), TEXT("Weltstart"), TEXT("VorPhysik"), TEXT("Physik"), TEXT("NachPhysik+Timer"), TEXT("Nacharbeit"), TEXT("Zeichnen"), TEXT("Bildwechsel") };
#include "Components/SkeletalMeshComponent.h"
#include "TDLeistung.h"
#include "RenderTimer.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "DynamicRHI.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TDDefenseWall.h"
#include "TDNavFilter.h"
#include "TDGegnerHirn.h"
#include "TDEinsatz.h"
#include "FGWaterVolume.h"
#include "TDTestWall.h"
#include "AI/FGCreatureController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Creature/FGCreature.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TDKernTerminal.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

static const FName CreaturePath(TEXT("/Game/FactoryGame/Character/Creature"));

/** Immer frei: was der Einsatz von Anfang an braucht (Kern, Mauer, Bolzenturm mit Bolzen, Notrezept Rohbolzen) */
static const TCHAR* GrundRezepte[] = {
	TEXT("/FicsitTD/Buildings/DefenseCore/Recipe_TD_DefenseCore.Recipe_TD_DefenseCore_C"),
	TEXT("/FicsitTD/Buildings/DefenseWall/Recipe_TD_DefenseWall.Recipe_TD_DefenseWall_C"),
	TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_BoltTurret.Recipe_TD_BoltTurret_C"),
	TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_Shoot_SpikedRebar.Recipe_TD_Shoot_SpikedRebar_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_Rohbolzen.Recipe_TD_Rohbolzen_C"),
};

/** Sandkasten: alle uebrigen TD-Rezepte. Im Einsatz kommen sie ueber das MAM; frei per /td freischalten, /td kern start, /td einsatz frei an */
static const TCHAR* SandkastenRezepte[] = {
	TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_Shoot_Explosive.Recipe_TD_Shoot_Explosive_C"),
	TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_Shoot_Stunshot.Recipe_TD_Shoot_Stunshot_C"),
	TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_Shoot_Spreadshot.Recipe_TD_Shoot_Spreadshot_C"),
	TEXT("/FicsitTD/Buildings/NobeliskLauncher/Recipe_TD_NobeliskLauncher.Recipe_TD_NobeliskLauncher_C"),
	TEXT("/FicsitTD/Buildings/NobeliskLauncher/Recipe_TD_Nobelisk_Explosive.Recipe_TD_Nobelisk_Explosive_C"),
	TEXT("/FicsitTD/Buildings/NobeliskLauncher/Recipe_TD_Nobelisk_Gas.Recipe_TD_Nobelisk_Gas_C"),
	TEXT("/FicsitTD/Buildings/NobeliskLauncher/Recipe_TD_Nobelisk_Pulse.Recipe_TD_Nobelisk_Pulse_C"),
	TEXT("/FicsitTD/Buildings/NobeliskLauncher/Recipe_TD_Nobelisk_Cluster.Recipe_TD_Nobelisk_Cluster_C"),
	TEXT("/FicsitTD/Buildings/NobeliskLauncher/Recipe_TD_Nobelisk_Nuke.Recipe_TD_Nobelisk_Nuke_C"),
	TEXT("/FicsitTD/Buildings/Gate/Recipe_TD_Gate.Recipe_TD_Gate_C"),
	TEXT("/FicsitTD/Buildings/LureBeacon/Recipe_TD_LureBeacon.Recipe_TD_LureBeacon_C"),
	TEXT("/FicsitTD/Buildings/LureBeacon/Recipe_TD_Lure_Coal.Recipe_TD_Lure_Coal_C"),
	TEXT("/FicsitTD/Buildings/SlickPlate/Recipe_TD_SlickPlate.Recipe_TD_SlickPlate_C"),
	TEXT("/FicsitTD/Buildings/SlickPlate/Recipe_TD_Slick_HeavyOil.Recipe_TD_Slick_HeavyOil_C"),
	TEXT("/FicsitTD/Buildings/ConductorWall/Recipe_TD_ConductorWall.Recipe_TD_ConductorWall_C"),
	TEXT("/FicsitTD/Buildings/ConductorWall/Recipe_TD_Zaun_Strom.Recipe_TD_Zaun_Strom_C"),
	TEXT("/FicsitTD/Buildings/RadioMast/Recipe_TD_RadioMast.Recipe_TD_RadioMast_C"),
	TEXT("/FicsitTD/Buildings/RadioMast/Recipe_TD_Funk_Strom.Recipe_TD_Funk_Strom_C"),
	TEXT("/FicsitTD/Buildings/ShieldGenerator/Recipe_TD_ShieldGenerator.Recipe_TD_ShieldGenerator_C"),
	TEXT("/FicsitTD/Buildings/ShieldGenerator/Recipe_TD_Schild_Strom.Recipe_TD_Schild_Strom_C"),
	TEXT("/FicsitTD/Buildings/FlakTurret/Recipe_TD_FlakTurret.Recipe_TD_FlakTurret_C"),
	TEXT("/FicsitTD/Buildings/FlakTurret/Recipe_TD_Flak_Shoot.Recipe_TD_Flak_Shoot_C"),
	TEXT("/FicsitTD/Buildings/FieldProjector/Recipe_TD_FieldProjector.Recipe_TD_FieldProjector_C"),
	TEXT("/FicsitTD/Buildings/FieldProjector/Recipe_TD_Feld_SAM.Recipe_TD_Feld_SAM_C"),
	TEXT("/FicsitTD/Buildings/SingularityLauncher/Recipe_TD_SingularityLauncher.Recipe_TD_SingularityLauncher_C"),
	TEXT("/FicsitTD/Buildings/SingularityLauncher/Recipe_TD_Singularitaet_Ficsonium.Recipe_TD_Singularitaet_Ficsonium_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_FlakKanister.Recipe_TD_FlakKanister_C"),
	TEXT("/FicsitTD/Buildings/DronePort/Recipe_TD_DronePort.Recipe_TD_DronePort_C"),
	TEXT("/FicsitTD/Buildings/DronePort/Recipe_TD_Drohne_Reparatur.Recipe_TD_Drohne_Reparatur_C"),
	TEXT("/FicsitTD/Buildings/DronePort/Recipe_TD_Drohne_ReparaturVerstaerkt.Recipe_TD_Drohne_ReparaturVerstaerkt_C"),
	TEXT("/FicsitTD/Buildings/DronePort/Recipe_TD_Drohne_Kampf.Recipe_TD_Drohne_Kampf_C"),
	TEXT("/FicsitTD/Buildings/GunTurret/Recipe_TD_Gun_Panzer.Recipe_TD_Gun_Panzer_C"),
	TEXT("/FicsitTD/Buildings/GunTurret/Recipe_TD_Gun_Brand.Recipe_TD_Gun_Brand_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_PatronePanzer.Recipe_TD_PatronePanzer_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_PatroneBrand.Recipe_TD_PatroneBrand_C"),
	TEXT("/FicsitTD/Buildings/GunTurret/Recipe_TD_GunTurret.Recipe_TD_GunTurret_C"),
	TEXT("/FicsitTD/Buildings/GunTurret/Recipe_TD_Gun_Standard.Recipe_TD_Gun_Standard_C"),
	TEXT("/FicsitTD/Buildings/GunTurret/Recipe_TD_Gun_Homing.Recipe_TD_Gun_Homing_C"),
	TEXT("/FicsitTD/Buildings/GunTurret/Recipe_TD_Gun_Turbo.Recipe_TD_Gun_Turbo_C"),
	TEXT("/FicsitTD/Buildings/TeslaCoil/Recipe_TD_TeslaCoil.Recipe_TD_TeslaCoil_C"),
	TEXT("/FicsitTD/Buildings/TeslaCoil/Recipe_TD_Tesla_Discharge.Recipe_TD_Tesla_Discharge_C"),
	TEXT("/FicsitTD/Buildings/FlameTurret/Recipe_TD_FlameTurret.Recipe_TD_FlameTurret_C"),
	TEXT("/FicsitTD/Buildings/FlameTurret/Recipe_TD_Flame_Fuel.Recipe_TD_Flame_Fuel_C"),
	TEXT("/FicsitTD/Buildings/FlameTurret/Recipe_TD_Flame_Biofuel.Recipe_TD_Flame_Biofuel_C"),
	TEXT("/FicsitTD/Buildings/FlameTurret/Recipe_TD_Flame_Turbo.Recipe_TD_Flame_Turbo_C"),
	TEXT("/FicsitTD/Buildings/FlameTurret/Recipe_TD_Flame_Rocket.Recipe_TD_Flame_Rocket_C"),
	TEXT("/FicsitTD/Buildings/FlameTurret/Recipe_TD_Flame_Ionized.Recipe_TD_Flame_Ionized_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_Explosivbolzen.Recipe_TD_Explosivbolzen_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_Streubolzen.Recipe_TD_Streubolzen_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_Patrone.Recipe_TD_Patrone_C"),
	TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_Shoot_Durchschlag.Recipe_TD_Shoot_Durchschlag_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_Durchschlagbolzen.Recipe_TD_Durchschlagbolzen_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_Schockbolzen.Recipe_TD_Schockbolzen_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_Zielsuchmunition.Recipe_TD_Zielsuchmunition_C"),
	TEXT("/FicsitTD/Recipes/Recipe_TD_Turbomunition.Recipe_TD_Turbomunition_C"),
};

TArray<FSoftClassPath> UTDWaveSubsystem::FindCreatureClasses()
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(CreaturePath, Assets, true);

	TSet<FString> Seen;
	TArray<FSoftClassPath> Result;
	for (const FAssetData& Asset : Assets)
	{
		FString Name = Asset.AssetName.ToString();
		if (!Name.StartsWith(TEXT("Char_")))
		{
			continue;
		}
		Name.RemoveFromEnd(TEXT("_C"));
		const FString ClassPath = FString::Printf(TEXT("%s.%s_C"), *Asset.PackageName.ToString(), *Name);
		if (!Seen.Contains(ClassPath))
		{
			Seen.Add(ClassPath);
			Result.Add(FSoftClassPath(ClassPath));
		}
	}
	// Kurze Namen zuerst, damit "Spitter" den normalen Spitter vor Varianten findet
	Result.Sort([](const FSoftClassPath& A, const FSoftClassPath& B)
	{
		const FString NA = A.GetAssetName(), NB = B.GetAssetName();
		return NA.Len() != NB.Len() ? NA.Len() < NB.Len() : NA < NB;
	});
	return Result;
}

int32 UTDWaveSubsystem::SpawnWave(TSubclassOf<AFGCreature> CreatureClass, int32 Count, float DistanceCm, const FVector& Center, AActor* Target)
{
	UWorld* World = GetWorld();
	if (!World || !CreatureClass)
	{
		return 0;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	int32 Spawned = 0;
	const float StartAngle = FMath::FRandRange(0.f, 360.f);
	for (int32 i = 0; i < Count; ++i)
	{
		// Im Halbkreis verteilt, damit alle grob aus einer Richtung kommen
		const float Angle = FMath::DegreesToRadians(StartAngle + 180.f * i / FMath::Max(1, Count - 1));
		FVector Location = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * DistanceCm;

		FHitResult Hit;
		FCollisionQueryParams Query(TEXT("FicsitTDGround"), false);
		if (World->LineTraceSingleByChannel(Hit, Location + FVector(0, 0, 10000.f), Location - FVector(0, 0, 20000.f), ECC_Visibility, Query))
		{
			Location = Hit.ImpactPoint + FVector(0, 0, 150.f);
		}

		const FRotator Facing = (Center - Location).GetSafeNormal2D().Rotation();
		AFGCreature* Creature = World->SpawnActor<AFGCreature>(CreatureClass, Location, Facing, Params);
		if (!Creature)
		{
			UE_LOG(LogFicsitTD, Warning, TEXT("Spawn fehlgeschlagen: %s bei %s"), *CreatureClass->GetName(), *Location.ToString());
			continue;
		}
		if (!Creature->GetController())
		{
			Creature->SpawnDefaultController();
		}
		// Nicht vom Kreatur-System wegen Entfernung despawnen lassen
		Creature->SetIsPersistent(true);
		KeinLoot(Creature);

		FTDWellenGegner Gegner;
		Gegner.Kreatur = Creature;
		Gegner.Ziel = Target;
		Gegner.bMauer = Target == nullptr;
		mGegner.Add(Gegner);
		++Spawned;
	}

	UE_LOG(LogFicsitTD, Log, TEXT("Welle: %d/%d x %s, Abstand %.0f m, Ziel %s"), Spawned, Count, *CreatureClass->GetName(), DistanceCm / 100.f,
		Target ? *Target->GetName() : TEXT("Mauer-Koeder"));
	mZeitBisSteuerung = 0.f;
	return Spawned;
}

ATDTestWall* UTDWaveSubsystem::SpawnWall(const FVector& GroundLocation, const FRotator& Rotation, float MaxHealth)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATDTestWall* Wall = World->SpawnActor<ATDTestWall>(ATDTestWall::StaticClass(), GroundLocation + FVector(0, 0, 200.f), Rotation, Params);
	if (Wall)
	{
		Wall->Init(MaxHealth);
		mTestMauern.Add(Wall);
	}
	return Wall;
}

AActor* UTDWaveSubsystem::FindNearestDefense(const FVector& Location) const
{
	const ATDDecoy* Decoy = Cast<ATDDecoy>(NaechsterKoeder(Location));
	return Decoy ? Decoy->GetActiveTarget() : nullptr;
}

AActor* UTDWaveSubsystem::NaechsterKoeder(const FVector& Location) const
{
	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<ATDDecoy> It(GetWorld()); It; ++It)
	{
		ATDDecoy* Decoy = *It;
		if (IsValid(Decoy) && Decoy->GetActiveTarget())
		{
			const float Dist = FVector::DistSquared(Decoy->GetActorLocation(), Location);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = Decoy;
			}
		}
	}
	return Best;
}

AActor* UTDWaveSubsystem::KernKoeder(const FVector& Location) const
{
	const ATDDefenseCore* Kern = mKern.Get();
	AActor* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<ATDDecoy> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && Kern && It->GetActiveTarget() == Kern)
		{
			const float Dist = FVector::DistSquared(It->GetActorLocation(), Location);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = *It;
			}
		}
	}
	return Best;
}

AActor* UTDWaveSubsystem::ZielFuer(const FTDWellenGegner& Gegner) const
{
	const AFGCreature* Creature = Gegner.Kreatur.Get();
	if (!Creature)
	{
		return nullptr;
	}
	if (Gegner.bKern)
	{
		// Was im Weg steht (Mauern, Tuerme im Umkreis von 25 m) zuerst, sonst zum Kern
		AActor* Nah = NaechsterKoeder(Creature->GetActorLocation());
		if (Nah && FVector::Dist(Nah->GetActorLocation(), Creature->GetActorLocation()) < 2500.f)
		{
			return Nah;
		}
		return KernKoeder(Creature->GetActorLocation());
	}
	return Gegner.bMauer ? NaechsterKoeder(Creature->GetActorLocation()) : Gegner.Ziel.Get();
}

TArray<AFGCreature*> UTDWaveSubsystem::GetLivingWaveCreatures() const
{
	TArray<AFGCreature*> Result;
	for (const FTDWellenGegner& Gegner : mGegner)
	{
		AFGCreature* Creature = Gegner.Kreatur.Get();
		if (Creature && Creature->IsAliveAndWell())
		{
			Result.Add(Creature);
		}
	}
	return Result;
}

int32 UTDWaveSubsystem::StopAll()
{
	int32 Removed = 0;
	for (const FTDWellenGegner& Gegner : mGegner)
	{
		if (AFGCreature* Creature = Gegner.Kreatur.Get())
		{
			if (AController* Controller = Creature->GetController())
			{
				Controller->Destroy();
			}
			Creature->Destroy();
			++Removed;
		}
	}
	mGegner.Reset();
	return Removed;
}

int32 UTDWaveSubsystem::RemoveWalls()
{
	int32 Removed = 0;
	for (const TWeakObjectPtr<ATDTestWall>& Weak : mTestMauern)
	{
		if (ATDTestWall* Wall = Weak.Get())
		{
			Wall->Destroy();
			++Removed;
		}
	}
	mTestMauern.Reset();
	return Removed;
}

void UTDWaveSubsystem::SteuereKreaturen()
{
	mGegner.RemoveAll([](const FTDWellenGegner& G) { return !G.Kreatur.IsValid() || !G.Kreatur->IsAliveAndWell(); });

	TArray<APawn*> Spieler;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr)
		{
			Spieler.Add(Pawn);
		}
	}

	for (const FTDWellenGegner& Gegner : mGegner)
	{
		if (Gegner.bKern)
		{
			continue;   // Kern-Gegner denken selbst (UTDGegnerHirn)
		}
		AFGCreature* Creature = Gegner.Kreatur.Get();
		AFGCreatureController* AI = Creature ? Cast<AFGCreatureController>(Creature->GetController()) : nullptr;
		AActor* Target = ZielFuer(Gegner);
		if (!AI || !Target)
		{
			continue; // keine Mauer mehr: Gegner machen auf eigene Faust weiter
		}
		if (Gegner.bMauer)
		{
			// Spieler-Aggro unterdruecken, damit sie bei der Verteidigung bleiben
			for (APawn* Pawn : Spieler)
			{
				AI->SetAggroForTarget(Pawn, 0.f, false);
			}
		}
		AI->SetAggroForTarget(Target, FMath::Max(Creature->GetMaximumAggroLevel(), 1.f), true);
		AI->SetCurrentTarget(Target);
		if (Modus == ETDSteuerModus::Laufen && !AI->CheckCurrentState(static_cast<int32>(ECreatureState::CS_Fight)))
		{
			AI->MoveToActor(Target, 150.f);
		}
	}
}

void UTDWaveSubsystem::SchalteSandkastenFrei()
{
	AFGRecipeManager* Manager = AFGRecipeManager::Get(GetWorld());
	int32 Anzahl = 0;
	for (const TCHAR* Pfad : SandkastenRezepte)
	{
		if (UClass* Recipe = Manager ? LoadClass<UFGRecipe>(nullptr, Pfad) : nullptr)
		{
			Manager->AddAvailableRecipe(Recipe);
			++Anzahl;
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Sandkasten: %d TD-Rezepte freigeschaltet"), Anzahl);
}

void UTDWaveSubsystem::SchalteRezepteFrei()
{
	AFGRecipeManager* Manager = AFGRecipeManager::Get(GetWorld());
	if (!Manager)
	{
		return; // noch nicht da, naechster Versuch spaeter
	}
	for (const TCHAR* Pfad : GrundRezepte)
	{
		if (UClass* Recipe = LoadClass<UFGRecipe>(nullptr, Pfad))
		{
			Manager->AddAvailableRecipe(Recipe);
			UE_LOG(LogFicsitTD, Log, TEXT("Rezept freigeschaltet: %s"), *Recipe->GetName());
		}
		else
		{
			UE_LOG(LogFicsitTD, Warning, TEXT("Rezept nicht gefunden: %s"), Pfad);
		}
	}
	mRezepteFreigeschaltet = true;
}

TArray<FString> UTDWaveSubsystem::GetStatusLines() const
{
	TArray<FString> Lines;
	int32 Alive = 0, Verfolgen = 0, Nah = 0, AufMauer = 0;
	float SummeAbstand = 0.f;
	TMap<FString, int32> Zustaende;

	float SummeKern = 0.f;
	int32 ImKern = 0, Versetzt = 0;
	TMap<FString, int32> Phasen;
	for (const FTDWellenGegner& Gegner : mGegner)
	{
		const AFGCreature* Creature = Gegner.Kreatur.Get();
		if (!Creature || !Creature->IsAliveAndWell())
		{
			continue;
		}
		if (Gegner.bKern && mKern.IsValid())
		{
			SummeKern += FVector::Dist2D(Creature->GetActorLocation(), mKern->GetActorLocation()) / 100.f;
			++ImKern;
			if (const UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(Creature))
			{
				static const TCHAR* Namen[] = { TEXT("Marsch"), TEXT("Anlauf"), TEXT("Angriff"), TEXT("tot") };
				Phasen.FindOrAdd(Namen[static_cast<int32>(Hirn->GetPhase())])++;
				Versetzt += Hirn->GetVersetzt();
			}
			continue;
		}
		++Alive;
		AufMauer += Gegner.bMauer ? 1 : 0;
		const AActor* Target = ZielFuer(Gegner);
		if (const AFGCreatureController* AI = Cast<AFGCreatureController>(Creature->GetController()))
		{
			Zustaende.FindOrAdd(CreatureStateEnumToString(AI->GetCurrentCreatureState()))++;
			Verfolgen += Target && AI->GetCurrentTarget() == Target ? 1 : 0;
		}
		if (Target)
		{
			const float Abstand = FVector::Dist(Creature->GetActorLocation(), Target->GetActorLocation()) / 100.f;
			SummeAbstand += Abstand;
			Nah += Abstand < 5.f ? 1 : 0;
		}
	}
	if (ImKern > 0)
	{
		FString PhasenText;
		for (const TPair<FString, int32>& Pair : Phasen)
		{
			PhasenText += FString::Printf(TEXT(" %s %d"), *Pair.Key, Pair.Value);
		}
		Lines.Add(FString::Printf(TEXT("Kern-Gegner: %d, mittlerer Abstand zum Kern %.0f m,%s, versetzt %d"), ImKern, SummeKern / ImKern, *PhasenText, Versetzt));
	}
	if (Alive == 0 && ImKern > 0)
	{
		// keine /welle-Testgegner: nur noch der Zustand der Verteidigung
		Lines.Append(GetWallStatusLines());
		return Lines;
	}

	Lines.Add(FString::Printf(TEXT("Welle: %d am Leben (%d auf Mauern), Modus: %s"), Alive, AufMauer, Modus == ETDSteuerModus::Aggro ? TEXT("aggro") : TEXT("laufen")));
	if (Alive > 0)
	{
		Lines.Add(FString::Printf(TEXT("Verfolgen Ziel: %d, mittlerer Abstand: %.0f m, naeher als 5 m: %d"), Verfolgen, SummeAbstand / Alive, Nah));
	}
	FString ZustandText;
	for (const TPair<FString, int32>& Pair : Zustaende)
	{
		ZustandText += FString::Printf(TEXT("%s=%d "), *Pair.Key, Pair.Value);
	}
	if (!ZustandText.IsEmpty())
	{
		Lines.Add(TEXT("Zustaende: ") + ZustandText);
	}
	Lines.Append(GetWallStatusLines());
	return Lines;
}

TArray<FString> UTDWaveSubsystem::GetWallStatusLines() const
{
	TArray<FString> Lines;
	int32 Mauern = 0, Tuerme = 0, Testmauern = 0;
	float Summe = 0.f, SummeMax = 0.f;
	struct FEintrag { FString Name; float Anteil; float LP; float Max; };
	TArray<FEintrag> Beschaedigt;
	FString TurmText;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const ITDDamageable* Damageable = Cast<ITDDamageable>(*It);
		if (!Damageable || !Damageable->IsTDIntact())
		{
			continue;
		}
		FString Art = TEXT("Mauer");
		if (const ATDTurretBase* Turm = Cast<ATDTurretBase>(*It))
		{
			++Tuerme;
			Art = FString::Printf(TEXT("%s %d"), *Turm->GetTurmName(), Tuerme);
			TurmText += FString::Printf(TEXT("%s: %s, %d Schuesse, Level %d (%d Abschuesse), Strom %s; "), *Art, *Turm->GetZustandText(),
				Turm->GetShotsFired(), Turm->GetLevel(), Turm->GetKills(), *Turm->GetStromText());
		}
		else if (Cast<ATDTestWall>(*It))
		{
			++Testmauern;
			Art = FString::Printf(TEXT("Testmauer %d"), Testmauern);
		}
		else
		{
			++Mauern;
			Art = FString::Printf(TEXT("Mauer %d"), Mauern);
		}
		const float LP = Damageable->GetTDHealth(), Max = Damageable->GetTDMaxHealth();
		Summe += LP;
		SummeMax += Max;
		if (LP < Max)
		{
			Beschaedigt.Add({ Art, LP / Max, LP, Max });
		}
	}
	if (Mauern + Tuerme + Testmauern == 0)
	{
		return Lines;
	}
	Lines.Add(FString::Printf(TEXT("Verteidigung: %d Mauern, %d Tuerme%s - gesamt %.0f/%.0f LP (%.0f %%), %d beschaedigt"),
		Mauern, Tuerme, Testmauern > 0 ? *FString::Printf(TEXT(", %d Testmauern"), Testmauern) : TEXT(""),
		Summe, SummeMax, 100.f * Summe / FMath::Max(SummeMax, 1.f), Beschaedigt.Num()));
	if (Beschaedigt.Num() > 0)
	{
		Beschaedigt.Sort([](const FEintrag& A, const FEintrag& B) { return A.Anteil < B.Anteil; });
		FString Text = TEXT("Schwaechste: ");
		for (int32 i = 0; i < FMath::Min(3, Beschaedigt.Num()); ++i)
		{
			Text += FString::Printf(TEXT("%s %.0f/%.0f; "), *Beschaedigt[i].Name, Beschaedigt[i].LP, Beschaedigt[i].Max);
		}
		Lines.Add(Text);
	}
	if (!TurmText.IsEmpty())
	{
		Lines.Add(TurmText);
	}
	return Lines;
}

// ---------------------------------------------------------------------------------------------------------------
// Modus Kern-Verteidigung: Aufbau-Countdown -> Welle (bis alle Gegner tot) -> ... -> Sieg nach mWellenGesamt; Kern faellt -> Niederlage
// ---------------------------------------------------------------------------------------------------------------

/** Gegner-Katalog (Wellen-Progression, Phasen wie der Weltraumlift): Kurzname fuer die Balance, Klasse, Punkte, ab Welle,
 *  Rolle (0 Ansturm, 1 Panzer, 2 Fern, 3 Schwarm), Phase der Familie (1-5), Teil des Stresstest-Mix.
 *  Einfuehrungswelle je Art ueberschreibbar: Balance Katalog.<Kurz>.AbWelle */
struct FTDKatalogEintrag { const TCHAR* Kurz; const TCHAR* Pfad; int32 Punkte; int32 AbWelle; int32 Rolle; int32 Phase; bool bStressMix; };
static const FTDKatalogEintrag KernKatalog[] = {
	{ TEXT("Hog"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Char_Hog.Char_Hog_C"), 1, 1, 0, 1, true },
	{ TEXT("AlphaHog"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/AlphaHog/Char_AlphaHog.Char_AlphaHog_C"), 3, 5, 1, 1, true },
	{ TEXT("StingerKlein"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/SmallStinger/Char_Stinger_Child.Char_Stinger_Child_C"), 1, 11, 3, 2, true },
	{ TEXT("Stinger"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/Char_Stinger.Char_Stinger_C"), 3, 15, 3, 2, true },
	{ TEXT("Spucker"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestSmall.Char_SpitterForestSmall_C"), 2, 21, 2, 3, true },
	{ TEXT("RoterSpucker"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForestRed/Char_SpitterForestRedSmall.Char_SpitterForestRedSmall_C"), 3, 24, 2, 3, false },
	{ TEXT("AlphaSpucker"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestAlpha.Char_SpitterForestAlpha_C"), 4, 26, 2, 3, true },
	{ TEXT("RoterAlphaSpucker"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForestRed/Char_SpitterForestRedAlpha.Char_SpitterForestRedAlpha_C"), 5, 28, 2, 3, false },
	{ TEXT("KlippenHog"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/CliffHog/Char_CliffHog.Char_CliffHog_C"), 5, 31, 1, 4, true },
	{ TEXT("WuestenSpucker"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterDesert/Char_SpitterDesertSmall.Char_SpitterDesertSmall_C"), 3, 33, 2, 4, false },
	{ TEXT("WuestenAlpha"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterDesert/Char_SpitterDesertAlpha.Char_SpitterDesertAlpha_C"), 5, 35, 2, 4, false },
	{ TEXT("WasserSpucker"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterAquatic/Char_SpitterAquaticSmall.Char_SpitterAquaticSmall_C"), 3, 36, 2, 4, false },
	{ TEXT("WasserAlpha"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterAquatic/Char_SpitterAquaticAlpha.Char_SpitterAquaticAlpha_C"), 5, 37, 2, 4, false },
	{ TEXT("EliteStinger"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/BigStinger/Char_EliteStinger.Char_EliteStinger_C"), 6, 38, 3, 4, false },
	{ TEXT("NuklearHog"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/NuclearHog/Char_NuclearHog.Char_NuclearHog_C"), 8, 41, 1, 5, false },
	{ TEXT("Johnny"), TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Johnny/Char_Johnny.Char_Johnny_C"), 25, 46, 1, 5, false },
};

static int32 AbWelleVon(const FTDKatalogEintrag& E)
{
	return TDBalance::Ganz(FString::Printf(TEXT("Katalog.%s.AbWelle"), E.Kurz), E.AbWelle);
}

/** Phasen (je 10 Wellen, Boss in der letzten): Name, Ankuendigung, Vorwarnung zur Mitte der Phase davor, Boss.
 *  Boss-Anzahl und -LP ueberschreibbar: Balance Phasen.<N>.BossAnzahl / BossLP. Ab Welle 51 endlos, Bosse reihum. */
struct FTDPhasenInfo { const TCHAR* Name; const TCHAR* Text; const TCHAR* Vorwarnung; const TCHAR* BossPfad; const TCHAR* BossName; int32 BossAnzahl; float BossLP; };
static const FTDPhasenInfo KernPhasen[] = {
	{ TEXT("Die Herde"), TEXT("Hogs rammen die Mauern - Bolzen und Mauern halten sie auf."), TEXT(""),
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/AlphaHog/Char_AlphaHog.Char_AlphaHog_C"), TEXT("Panzerkoloss"), 1, 10.f },
	{ TEXT("Der Schwarm"), TEXT("Stinger kommen schnell und in Massen - Gewehrturm, Streubolzen und Tesla."), TEXT("Spaeher melden Stinger-Nester: ab Welle 13 kommt der Schwarm. Gewehrturm und Kohlekraft vorbereiten."),
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/Char_Stinger.Char_Stinger_C"), TEXT("Brutmutter"), 1, 25.f },
	{ TEXT("Saeure"), TEXT("Spucker greifen aus der Ferne an - Flammenwerfer und Nobelisk."), TEXT("Spaeher melden Saeurespucker: ab Welle 21 greifen sie aus der Ferne an. Raffinerie und Oel vorbereiten."),
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestAlpha.Char_SpitterForestAlpha_C"), TEXT("Saeurekoenigin"), 1, 15.f },
	{ TEXT("Fremde Biome"), TEXT("Klippen-Hogs, Wuesten- und Wasserspucker, Gas-Stinger - Feuer allein reicht nicht mehr."), TEXT("Spaeher melden fremde Arten aus Wueste und Wasser: ab Welle 31 hilft Feuer allein nicht mehr."),
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/BigStinger/Char_EliteStinger.Char_EliteStinger_C"), TEXT("Gasriese"), 1, 15.f },
	{ TEXT("Strahlung"), TEXT("Nuklear-Hogs und Johnny - die schwersten Gegner des Planeten."), TEXT("Spaeher melden Strahlung: ab Welle 41 kommen Nuklear-Hogs und Johnny."),
		TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Johnny/Char_Johnny.Char_Johnny_C"), TEXT("Johnny-Rotte"), 2, 2.f },
};
static constexpr int32 KernPhasenAnzahl = UE_ARRAY_COUNT(KernPhasen);

/** 0.85 (L1): Anzeigetexte der Phasen in der Spielsprache; die Tabelle oben bleibt deutsch */
static FString PhasenName(int32 Ph)
{
	switch (Ph)
	{
	case 0: return TDT(TEXT("Die Herde"), TEXT("The Herd"));
	case 1: return TDT(TEXT("Der Schwarm"), TEXT("The Swarm"));
	case 2: return TDT(TEXT("Saeure"), TEXT("Acid"));
	case 3: return TDT(TEXT("Fremde Biome"), TEXT("Foreign Biomes"));
	case 4: return TDT(TEXT("Strahlung"), TEXT("Radiation"));
	default: return KernPhasen[FMath::Clamp(Ph, 0, KernPhasenAnzahl - 1)].Name;
	}
}
static FString PhasenBeschreibung(int32 Ph)
{
	switch (Ph)
	{
	case 0: return TDT(TEXT("Hogs rammen die Mauern - Bolzen und Mauern halten sie auf."), TEXT("Hogs ram the walls - bolts and walls stop them."));
	case 1: return TDT(TEXT("Stinger kommen schnell und in Massen - Gewehrturm, Streubolzen und Tesla."), TEXT("Stingers come fast and in swarms - Rifle Turret, spread bolts and Tesla."));
	case 2: return TDT(TEXT("Spucker greifen aus der Ferne an - Flammenwerfer und Nobelisk."), TEXT("Spitters attack from range - Flamethrower and Nobelisk."));
	case 3: return TDT(TEXT("Klippen-Hogs, Wuesten- und Wasserspucker, Gas-Stinger - Feuer allein reicht nicht mehr."), TEXT("Cliff Hogs, desert and aquatic spitters, gas stingers - fire alone is no longer enough."));
	case 4: return TDT(TEXT("Nuklear-Hogs und Johnny - die schwersten Gegner des Planeten."), TEXT("Nuclear Hogs and Johnny - the toughest enemies on the planet."));
	default: return KernPhasen[FMath::Clamp(Ph, 0, KernPhasenAnzahl - 1)].Text;
	}
}
static FString PhasenVorwarnung(int32 Ph)
{
	switch (Ph)
	{
	case 1: return TDT(TEXT("Spaeher melden Stinger-Nester: ab Welle 13 kommt der Schwarm. Gewehrturm und Kohlekraft vorbereiten."), TEXT("Scouts report stinger nests: the swarm arrives from wave 13. Prepare Rifle Turrets and coal power."));
	case 2: return TDT(TEXT("Spaeher melden Saeurespucker: ab Welle 21 greifen sie aus der Ferne an. Raffinerie und Oel vorbereiten."), TEXT("Scouts report acid spitters: from wave 21 they attack from range. Prepare a Refinery and oil."));
	case 3: return TDT(TEXT("Spaeher melden fremde Arten aus Wueste und Wasser: ab Welle 31 hilft Feuer allein nicht mehr."), TEXT("Scouts report foreign species from desert and water: from wave 31 fire alone no longer helps."));
	case 4: return TDT(TEXT("Spaeher melden Strahlung: ab Welle 41 kommen Nuklear-Hogs und Johnny."), TEXT("Scouts report radiation: Nuclear Hogs and Johnny arrive from wave 41."));
	default: return KernPhasen[FMath::Clamp(Ph, 0, KernPhasenAnzahl - 1)].Vorwarnung;
	}
}
static FString PhasenBossName(int32 Ph)
{
	switch (Ph)
	{
	case 0: return TDT(TEXT("Panzerkoloss"), TEXT("Armored Colossus"));
	case 1: return TDT(TEXT("Brutmutter"), TEXT("Brood Mother"));
	case 2: return TDT(TEXT("Saeurekoenigin"), TEXT("Acid Queen"));
	case 3: return TDT(TEXT("Gasriese"), TEXT("Gas Giant"));
	case 4: return TDT(TEXT("Johnny-Rotte"), TEXT("Johnny Pack"));
	default: return KernPhasen[FMath::Clamp(Ph, 0, KernPhasenAnzahl - 1)].BossName;
	}
}
/** Wellenart fuer Ansage und Anzeige (0 Schwarm, 1 Ansturm, 2 Panzer, 3 Gemischt, 4 Fernkampf) */
static FString WellenArtName(int32 Art)
{
	switch (FMath::Clamp(Art, 0, 4))
	{
	case 0: return TDT(TEXT("Schwarm"), TEXT("Swarm"));
	case 1: return TDT(TEXT("Ansturm"), TEXT("Rush"));
	case 2: return TDT(TEXT("Panzer"), TEXT("Tank"));
	case 3: return TDT(TEXT("Gemischt"), TEXT("Mixed"));
	default: return TDT(TEXT("Fernkampf"), TEXT("Ranged"));
	}
}

/** 0-basierte Phase einer Welle; >= KernPhasenAnzahl = endlos */
static int32 PhaseVon(int32 Welle) { return FMath::Max(0, Welle - 1) / 10; }
/** Wellen-Tipp nach der Phase, nicht nach der Wellenart (Test 0.51: W10 mit nur Hogs bekam "Flammenwerfer gegen Massen") */
static FString TippFuer(int32 Welle)
{
	const int32 Ph = PhaseVon(Welle);
	return Ph < KernPhasenAnzahl ? PhasenBeschreibung(Ph) : TDT(TEXT("Endlos: alle Arten im Mix - Feuer, Strom und Explosionen mischen."), TEXT("Endless: all species mixed - combine fire, power and explosions."));
}

static constexpr int32 KernMaxGegner = 60;

/** bDeutsch: fuer Log-Zeilen immer deutsch; sonst in der Spielsprache (0.85 L1) */
static FString Himmelsrichtung(float WinkelGrad, bool bDeutsch = false)
{
	// Satisfactory-Karte: +X = Osten, +Y = Sueden (Test 0.16.2: "Norden" mit +X lief nicht nach Norden). Kompasskurs = Winkel + 90
	static const TCHAR* Namen[] = { TEXT("Norden"), TEXT("Nordosten"), TEXT("Osten"), TEXT("Suedosten"), TEXT("Sueden"), TEXT("Suedwesten"), TEXT("Westen"), TEXT("Nordwesten") };
	const int32 i = FMath::RoundToInt(FRotator::NormalizeAxis(WinkelGrad + 90.f) / 45.f + 8.f) % 8;
	if (bDeutsch)
	{
		return Namen[i];
	}
	switch (i)
	{
	case 0: return TDT(TEXT("Norden"), TEXT("North"));
	case 1: return TDT(TEXT("Nordosten"), TEXT("Northeast"));
	case 2: return TDT(TEXT("Osten"), TEXT("East"));
	case 3: return TDT(TEXT("Suedosten"), TEXT("Southeast"));
	case 4: return TDT(TEXT("Sueden"), TEXT("South"));
	case 5: return TDT(TEXT("Suedwesten"), TEXT("Southwest"));
	case 6: return TDT(TEXT("Westen"), TEXT("West"));
	default: return TDT(TEXT("Nordwesten"), TEXT("Northwest"));
	}
}

/** Anzeigename der Schwierigkeit; FTDStufe::Name bleibt deutsch (Balance-Schluessel Stufen.<Name>.*) */
static FString StufenAnzeige(int32 Stufe)
{
	switch (FMath::Clamp(Stufe, 0, 3))
	{
	case 0: return TDT(TEXT("Erkundung"), TEXT("Exploration"));
	case 1: return TDT(TEXT("Normal"), TEXT("Normal"));
	case 2: return TDT(TEXT("Hart"), TEXT("Hard"));
	default: return TDT(TEXT("Albtraum"), TEXT("Nightmare"));
	}
}

/** Frontname; bDeutsch fuer Log-Zeilen */
static FString FrontText(const TArray<FTDFront>& Fronten, int32 Index, bool bDeutsch)
{
	if (!Fronten.IsValidIndex(Index))
	{
		return TEXT("?");
	}
	const FTDFront& F = Fronten[Index];
	if (bDeutsch)
	{
		return F.Weg > 0.f ? FString::Printf(TEXT("%s (Weg %.0f m)"), *Himmelsrichtung(F.Winkel, true), F.Weg / 100.f)
			: FString::Printf(TEXT("%s (ungeprueft)"), *Himmelsrichtung(F.Winkel, true));
	}
	return F.Weg > 0.f ? TDF(TEXT("%s (Weg %.0f m)"), TEXT("%s (path %.0f m)"), *Himmelsrichtung(F.Winkel), F.Weg / 100.f)
		: TDF(TEXT("%s (ungeprueft)"), TEXT("%s (unchecked)"), *Himmelsrichtung(F.Winkel));
}

// ---- Schwierigkeit und Fronten (Level-Konzept Abschnitte 3 und 5) ----

static const FTDStufe KernStufen[] = {
	//  Name           Budget LP    Aufbau  Zwischen Start Neu  Elite  Min      Max      KernLP   Ansage
	{ TEXT("Erkundung"), 0.6f, 0.7f, 1800.f, 900.f,  1,    15,  15,    25000.f, 30000.f, 30000.f, true },
	{ TEXT("Normal"),    1.0f, 1.0f, 1200.f, 600.f,  2,    10,  10,    20000.f, 25000.f, 20000.f, true },
	{ TEXT("Hart"),      1.4f, 1.3f,  900.f, 480.f,  3,     8,   7,    15000.f, 25000.f, 15000.f, false },
	{ TEXT("Albtraum"),  2.0f, 1.7f,  600.f, 360.f,  3,     5,   5,    15000.f, 20000.f, 10000.f, false },
};
static constexpr int32 KernMaxFronten = 4;

FTDStufe UTDWaveSubsystem::GetStufe(int32 Stufe)
{
	FTDStufe S = KernStufen[FMath::Clamp(Stufe, 0, 3)];
	const FString Vorne = FString(TEXT("Stufen.")) + S.Name + TEXT(".");
	S.Budget = TDBalance::Wert(Vorne + TEXT("Budget"), S.Budget);
	S.LP = TDBalance::Wert(Vorne + TEXT("GegnerLP"), S.LP);
	S.Aufbau = TDBalance::Wert(Vorne + TEXT("AufbauSekunden"), S.Aufbau);
	S.Zwischen = TDBalance::Wert(Vorne + TEXT("PauseSekunden"), S.Zwischen);
	S.FrontenStart = TDBalance::Ganz(Vorne + TEXT("FrontenStart"), S.FrontenStart);
	S.NeueFrontAb = TDBalance::Ganz(Vorne + TEXT("NeueFrontAbWelle"), S.NeueFrontAb);
	S.EliteAb = TDBalance::Ganz(Vorne + TEXT("EliteAbWelle"), S.EliteAb);
	S.KernLP = TDBalance::Wert(Vorne + TEXT("KernLP"), S.KernLP);
	return S;
}

int32 UTDWaveSubsystem::StufeAusText(const FString& Text)
{
	const FString T = Text.ToLower();
	if (T == TEXT("erkundung") || T == TEXT("leicht")) return 0;
	if (T == TEXT("normal"))                          return 1;
	if (T == TEXT("hart") || T == TEXT("schwer"))     return 2;
	if (T == TEXT("albtraum"))                        return 3;
	return -1;
}

int32 UTDWaveSubsystem::AktiveFrontenFuer(int32 Welle) const
{
	const FTDStufe& S = GetStufe(mStufe);
	int32 N = S.FrontenStart;
	N += Welle >= S.NeueFrontAb ? 1 : 0;
	N += Welle >= 2 * S.NeueFrontAb - 1 ? 1 : 0;   // Normal: zweite ab 11, dritte ab 21
	return FMath::Clamp(N, 1, FMath::Max(1, FMath::Min(mFronten.Num(), KernMaxFronten)));
}

FString UTDWaveSubsystem::FrontName(int32 Index) const
{
	return FrontText(mFronten, Index, false);
}

static FVector FrontBoden(UWorld* World, const FVector& Ort, bool& bTreffer, bool& bWasser)
{
	FHitResult Hit;
	FCollisionQueryParams Query(TEXT("FicsitTDFront"), false);
	bTreffer = World->LineTraceSingleByChannel(Hit, Ort + FVector(0, 0, 30000.f), Ort - FVector(0, 0, 60000.f), ECC_Visibility, Query);
	bWasser = bTreffer && Hit.GetActor() && Hit.GetActor()->GetClass()->GetName().Contains(TEXT("Water"));
	return bTreffer ? Hit.ImpactPoint : Ort;
}

FString UTDWaveSubsystem::NavDiagnose(UWorld* World, const FVector& Mitte)
{
	UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Nav)
	{
		return TEXT("Netz: kein Navigationssystem");
	}
	const ANavigationData* Daten = Nav->GetDefaultNavDataInstance();
	FNavLocation Egal;
	FString Text = FString::Printf(TEXT("Netz: Invoker %s, Daten %s, Kern %s, Reichweite"),
		Nav->IsActiveTilesGenerationEnabled() ? TEXT("an") : TEXT("aus"), Daten ? *Daten->GetClass()->GetName() : TEXT("KEINE"),
		Nav->ProjectPointToNavigation(Mitte, Egal, FVector(1200.f, 1200.f, 1000.f)) ? TEXT("ja") : TEXT("nein"));
	for (const float Abstand : { 1000.f, 2500.f, 5000.f, 10000.f, 15000.f, 20000.f, 25000.f })
	{
		int32 Treffer = 0;
		for (int32 r = 0; r < 8; ++r)
		{
			bool bBoden = false, bWasser = false;
			const FVector Ort = FrontBoden(World, Mitte + FRotator(0.f, 45.f * r, 0.f).Vector() * Abstand, bBoden, bWasser);
			Treffer += bBoden && Nav->ProjectPointToNavigation(Ort, Egal, FVector(500.f, 500.f, 500.f)) ? 1 : 0;
		}
		Text += FString::Printf(TEXT(" %.0fm %d/8"), Abstand / 100.f, Treffer);
	}
	return Text;
}

bool UTDWaveSubsystem::PfadAmZiel(const UNavigationPath* Pfad, const FVector& Ziel)
{
	if (!Pfad || !Pfad->IsValid() || Pfad->PathPoints.Num() < 2)
	{
		return false;
	}
	if (!Pfad->IsPartial())
	{
		return true;
	}
	const FVector Ende = Pfad->PathPoints.Last();
	// 8 m: der Kern selbst ist nicht begehbar, aber Mauern direkt am Kern sollen als Sperre zaehlen (Test 0.18.3: mit 20 m galt ein eingemauerter Kern als frei)
	// 0.9.3a (BautenP2 09.11.: 0 von 16 Fronten gueltig, alle "teilweise"): der Kern ist 16 x 16 m, der Weg endet 8-9 m vor der Mitte.
	// Gleicher Abstand zur Kernkante wie vorher (halbe Breite 4 m + 4 m) -> halbe Breite 8,1 m + 4 m.
	return FVector::Dist2D(Ende, Ziel) <= 1210.f && FMath::Abs(Ende.Z - Ziel.Z) <= 800.f;
}

/** Steht eine Verteidigung (Turm) in 60 m? Dort sollen keine Gegner erscheinen (Test 0.16.4: Turm an der Front erschoss Welle 1 in 2 s) */
static bool NaheVerteidigung(UWorld* World, const FVector& Ort)
{
	for (TActorIterator<ATDTurretBase> It(World); It; ++It)
	{
		if (FVector::DistSquared2D(It->GetActorLocation(), Ort) < FMath::Square(6000.f))
		{
			return true;
		}
	}
	return false;
}

/** 0.78.2: liegt der Punkt (Fuesse + 0,5 m) in einem Wasservolumen? */
static bool PunktImWasser(UWorld* World, const FVector& P)
{
	for (TActorIterator<AFGWaterVolume> It(World); It; ++It)
	{
		if (It->EncompassesPoint(P + FVector(0.f, 0.f, 50.f)))
		{
			return true;
		}
	}
	return false;
}

/** Fuehrt der Weg durch ein Wasservolumen? Stichproben alle 5 m (falls das Netz Wasser nicht als eigenen Bereich kennt) */
static bool WegDurchWasser(UWorld* World, const TArray<FVector>& Punkte)
{
	TArray<AFGWaterVolume*> Wasser;
	for (TActorIterator<AFGWaterVolume> It(World); It; ++It)
	{
		Wasser.Add(*It);
	}
	for (int32 i = 1; i < Punkte.Num(); ++i)
	{
		const int32 Schritte = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Punkte[i - 1], Punkte[i]) / 500.f));
		for (int32 k = 0; k <= Schritte; ++k)
		{
			const FVector P = FMath::Lerp(Punkte[i - 1], Punkte[i], float(k) / Schritte) + FVector(0.f, 0.f, 50.f);
			for (AFGWaterVolume* W : Wasser)
			{
				if (W->EncompassesPoint(P))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void UTDWaveSubsystem::FrontenFuer(ATDDefenseCore* Kern)
{
	if (!Kern)
	{
		return;
	}
	if (mFronten.Num() == 0 || mFrontenKern.Get() != Kern || mFrontenStufe != mStufe)
	{
		SucheFronten(Kern->GetActorLocation());
		mFrontenKern = Kern;
		mFrontenStufe = mStufe;
	}
}

void UTDWaveSubsystem::SucheFronten(const FVector& Mitte)
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const FTDStufe& S = GetStufe(mStufe);
	FNavLocation MitteNav;
	const bool bMitteNav = Nav && Nav->ProjectPointToNavigation(Mitte, MitteNav, FVector(1200.f, 1200.f, 1000.f));

	// je Richtung der beste gepruefte Punkt (kleinster Umweg), sonst ein Ersatzpunkt mit Boden ohne Wasser
	TArray<FTDFront> Gueltig, Ersatz;
	int32 OhneBoden = 0, Wasser = 0, ZuHoch = 0, OhneNetz = 0, OhnePfad = 0, Teilweise = 0, Umwege = 0, Tuerme = 0, WasserWeg = 0;
	for (int32 r = 0; r < 16; ++r)
	{
		const float Winkel = 22.5f * r;
		FTDFront Beste, ErsatzPunkt;
		float BesterUmweg = TNumericLimits<float>::Max();
		bool bErsatz = false;
		for (const float Abstand : { S.MinAbstand, 0.5f * (S.MinAbstand + S.MaxAbstand), S.MaxAbstand })
		{
			bool bTreffer = false, bWasser = false;
			const FVector Ort = FrontBoden(World, Mitte + FRotator(0.f, Winkel, 0.f).Vector() * Abstand, bTreffer, bWasser);
			if (!bTreffer || bWasser || FMath::Abs(Ort.Z - Mitte.Z) > 4000.f)
			{
				(!bTreffer ? OhneBoden : bWasser ? Wasser : ZuHoch)++;
				continue;
			}
			if (NaheVerteidigung(World, Ort))
			{
				++Tuerme;
				continue;
			}
			if (!bErsatz)
			{
				bErsatz = true;
				ErsatzPunkt.Winkel = Winkel;
				ErsatzPunkt.Ort = Ort;
			}
			FNavLocation NavOrt;
			if (!bMitteNav || !Nav->ProjectPointToNavigation(Ort, NavOrt, FVector(500.f, 500.f, 500.f)))
			{
				++OhneNetz;
				continue;
			}
			UNavigationPath* Pfad = UNavigationSystemV1::FindPathToLocationSynchronously(World, NavOrt.Location, MitteNav.Location, nullptr, UTDNavFilter::StaticClass());
			if (!PfadAmZiel(Pfad, MitteNav.Location))
			{
				(Pfad && Pfad->IsValid() ? Teilweise : OhnePfad)++;
				continue;
			}
			if (WegDurchWasser(World, Pfad->PathPoints))
			{
				++WasserWeg;
				continue;
			}
			const float Laenge = Pfad->GetPathLength();
			const float Umweg = Laenge / FMath::Max(1.f, FVector::Dist(NavOrt.Location, MitteNav.Location));
			Umwege += Umweg > 1.8f ? 1 : 0;
			if (Umweg <= 1.8f && Umweg < BesterUmweg)
			{
				BesterUmweg = Umweg;
				Beste.Winkel = Winkel;
				Beste.Ort = NavOrt.Location;
				Beste.Weg = Laenge;
				Beste.Punkte = Pfad->PathPoints;
			}
		}
		if (Beste.Weg > 0.f)
		{
			Gueltig.Add(Beste);
		}
		else if (bErsatz)
		{
			Ersatz.Add(ErsatzPunkt);
		}
	}
	mGueltigeRichtungen = Gueltig.Num();

	// Reihenfolge: Start = gepruefte Richtung mit dem laengsten Weg (meiste Vorwarnzeit, 0.56), ohne gepruefte Zufall;
	// dann jeweils die Richtung mit dem groessten Abstand zu den schon gewaehlten.
	// Gepruefte Richtungen haben Vorrang; Ersatzpunkte nur, wenn es zu wenige gepruefte gibt.
	mFronten.Reset();
	TArray<FTDFront> Pool = Gueltig.Num() > 0 ? Gueltig : Ersatz;
	if (Pool.Num() > 0)
	{
		// Zufall fest am Kernort: dieselben Fronten bei jedem Abruf (Test 0.16.4: Wege wechselten bei jedem /td kern wege)
		FRandomStream Zufall(FMath::RoundToInt(Mitte.X / 100.f) * 7919 + FMath::RoundToInt(Mitte.Y / 100.f));
		int32 Start = Zufall.RandRange(0, Pool.Num() - 1);
		for (int32 i = 0; i < Pool.Num(); ++i)
		{
			if (Pool[i].Weg > Pool[Start].Weg)
			{
				Start = i;
			}
		}
		mFronten.Add(Pool[Start]);
	}
	auto Waehle = [this](TArray<FTDFront>& Liste)
	{
		while (mFronten.Num() < KernMaxFronten)
		{
			int32 BesterIndex = INDEX_NONE;
			float BesterAbstand = 0.f;
			for (int32 i = 0; i < Liste.Num(); ++i)
			{
				float MinAbstand = 360.f;
				for (const FTDFront& F : mFronten)
				{
					MinAbstand = FMath::Min(MinAbstand, FMath::Abs(FRotator::NormalizeAxis(Liste[i].Winkel - F.Winkel)));
				}
				// benachbarte Richtungen (< 45 Grad) zaehlen nicht als eigene Front
				if (MinAbstand >= 45.f && MinAbstand > BesterAbstand)
				{
					BesterAbstand = MinAbstand;
					BesterIndex = i;
				}
			}
			if (BesterIndex == INDEX_NONE)
			{
				return;
			}
			mFronten.Add(Liste[BesterIndex]);
			Liste.RemoveAt(BesterIndex);
		}
	};
	Waehle(Gueltig);
	Waehle(Ersatz);

	UE_LOG(LogFicsitTD, Log, TEXT("Fronten (%s): %d von 16 Richtungen geprueft gueltig, %d Fronten gewaehlt"), S.Name, mGueltigeRichtungen, mFronten.Num());
	UE_LOG(LogFicsitTD, Log, TEXT("Fronten verworfen: ohne Boden %d, Wasser %d, Hoehe %d, ohne Netz %d (Kern auf Netz: %s), kein Pfad %d, teilweise %d, Umweg %d, Turm in 60 m %d, Weg durch Wasser %d"),
		OhneBoden, Wasser, ZuHoch, OhneNetz, bMitteNav ? TEXT("ja") : TEXT("nein"), OhnePfad, Teilweise, Umwege, Tuerme, WasserWeg);
	UE_LOG(LogFicsitTD, Log, TEXT("Fronten %s"), *NavDiagnose(World, Mitte));
	for (int32 i = 0; i < mFronten.Num(); ++i)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Front %d: %s bei %s"), i + 1, *FrontText(mFronten, i, true),*mFronten[i].Ort.ToCompactString());
	}
}

TArray<FTDFront> UTDWaveSubsystem::PruefeFrontenAn(const FVector& Mitte, int32 Stufe, int32& OutGueltig)
{
	const TArray<FTDFront> AlteFronten = mFronten;
	const int32 AlteGueltig = mGueltigeRichtungen;
	const int32 AlteStufe = mStufe;
	mStufe = FMath::Clamp(Stufe, 0, 3);
	SucheFronten(Mitte);
	TArray<FTDFront> Ergebnis = mFronten;
	OutGueltig = mGueltigeRichtungen;
	mFronten = AlteFronten;
	mGueltigeRichtungen = AlteGueltig;
	mStufe = AlteStufe;
	return Ergebnis;
}

TArray<FString> UTDWaveSubsystem::GetFrontenText(ATDDefenseCore* Kern)
{
	TArray<FString> Zeilen;
	if (!Kern)
	{
		return Zeilen;
	}
	if (mLauf == ETDLauf::Aus || mFronten.Num() == 0)
	{
		FrontenFuer(Kern);
	}
	Zeilen.Add(TDF(TEXT("Fronten (%s): %d von 16 Richtungen begehbar."), TEXT("Fronts (%s): %d of 16 directions passable."), *StufenAnzeige(mStufe), mGueltigeRichtungen));
	const int32 Aktiv = mLauf == ETDLauf::Aus ? AktiveFrontenFuer(1) : AktiveFrontenFuer(FMath::Max(1, mWelle));
	for (int32 i = 0; i < mFronten.Num(); ++i)
	{
		FString Ab;
		if (i >= Aktiv)
		{
			// erste Welle, in der diese Front dazukommt
			for (int32 w = 1; w <= 200; ++w)
			{
				if (AktiveFrontenFuer(w) > i)
				{
					Ab = TDF(TEXT(" - ab Welle %d"), TEXT(" - from wave %d"), w);
					break;
				}
			}
			if (Ab.IsEmpty())
			{
				Ab = TDT(TEXT(" - Reserve"), TEXT(" - reserve"));
			}
		}
		Zeilen.Add(FString::Printf(TEXT("%d. %s%s"), i + 1, *FrontName(i), *Ab));
	}
	if (mGueltigeRichtungen == 0)
	{
		Zeilen.Add(TDT(TEXT("Kein Weg geprueft (Navigationsnetz fehlt dort?) - Gegner kommen aus ungeprueften Richtungen."), TEXT("No path checked (navigation mesh missing there?) - enemies come from unchecked directions.")));
		Zeilen.Add(NavDiagnose(GetWorld(), Kern->GetActorLocation()));
	}
	return Zeilen;
}

TArray<TArray<FVector>> UTDWaveSubsystem::BerechneWege(ATDDefenseCore* Kern, FString& Bericht)
{
	if (Kern)
	{
		EntschaerfeLogistik(Kern->GetActorLocation());
	}
	TArray<TArray<FVector>> Wege;
	if (!Kern)
	{
		return Wege;
	}
	const FVector Mitte = Kern->GetActorLocation();
	if (mLauf == ETDLauf::Aus || mFronten.Num() == 0)
	{
		FrontenFuer(Kern);
	}
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	FNavLocation MitteNav;
	if (!Nav || !Nav->ProjectPointToNavigation(Mitte, MitteNav, FVector(1200.f, 1200.f, 1000.f)))
	{
		Bericht = TDT(TEXT("Kein Navigationsnetz am Kern - keine Wegvorschau moeglich."), TEXT("No navigation mesh at the Core - no path preview possible."));
		return Wege;
	}
	// aktive Fronten und die naechste, die dazukommt
	const int32 Welle = FMath::Max(1, mLauf == ETDLauf::Aufbau ? mWelle + 1 : mWelle);
	const int32 Aktiv = AktiveFrontenFuer(Welle);
	const int32 Anzahl = FMath::Min(mFronten.Num(), Aktiv + 1);
	TArray<FString> Teile;
	for (int32 i = 0; i < Anzahl; ++i)
	{
		FTDFront& F = mFronten[i];
		UNavigationPath* Pfad = UNavigationSystemV1::FindPathToLocationSynchronously(World, F.Ort, MitteNav.Location, nullptr, UTDNavFilter::StaticClass());
		if (!Pfad || !Pfad->IsValid() || Pfad->PathPoints.Num() < 2)
		{
			Teile.Add(TDF(TEXT("%s: kein Weg"), TEXT("%s: no path"), *Himmelsrichtung(F.Winkel)));
			continue;
		}
		F.Punkte = Pfad->PathPoints;
		F.Weg = Pfad->GetPathLength();
		Wege.Add(F.Punkte);
		Teile.Add(FString::Printf(TEXT("%s %.0f m%s%s"), *Himmelsrichtung(F.Winkel), F.Weg / 100.f, !PfadAmZiel(Pfad, MitteNav.Location) ? *TDT(TEXT(" (TEILWEISE)"), TEXT(" (PARTIAL)")) : TEXT(""),
			i >= Aktiv ? *TDT(TEXT(" (kommt spaeter)"), TEXT(" (comes later)")) : TEXT("")));
	}
	Bericht = TDF(TEXT("Wege: %s"), TEXT("Paths: %s"), *FString::Join(Teile, TEXT("; ")));
	return Wege;
}

void UTDWaveSubsystem::PruefeVersorgung() const
{
	int32 Tuerme = 0;
	TArray<FString> OhneMunition, OhneStrom, OhneRezept;
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		++Tuerme;
		if (!It->GetCurrentRecipe())                        OhneRezept.Add(It->GetTurmAnzeige());
		else if (!It->HatMunition())                        OhneMunition.Add(It->GetTurmAnzeige());
		if (It->RunsOnPower() && !It->HasPower())           OhneStrom.Add(It->GetTurmAnzeige());
	}
	if (Tuerme == 0)
	{
		Ansage(TDT(TEXT("Versorgung: noch kein Turm gebaut!"), TEXT("Supply: no turret built yet!")));
		return;
	}
	if (OhneMunition.Num() + OhneStrom.Num() + OhneRezept.Num() == 0)
	{
		Ansage(TDF(TEXT("Versorgung: alle %d Tuerme haben Munition und Strom."), TEXT("Supply: all %d turrets have ammo and power."), Tuerme));
		return;
	}
	FString Text = TDF(TEXT("Versorgung WARNUNG (%d Tuerme):"), TEXT("Supply WARNING (%d turrets):"), Tuerme);
	if (OhneMunition.Num()) Text += TDF(TEXT(" ohne Munition: %s;"), TEXT(" no ammo: %s;"), *FString::Join(OhneMunition, TEXT(", ")));
	if (OhneStrom.Num())    Text += TDF(TEXT(" ohne Strom: %s;"), TEXT(" no power: %s;"), *FString::Join(OhneStrom, TEXT(", ")));
	if (OhneRezept.Num())   Text += TDF(TEXT(" ohne Rezept: %s;"), TEXT(" no recipe: %s;"), *FString::Join(OhneRezept, TEXT(", ")));
	Ansage(Text);
}

void UTDWaveSubsystem::Ansage(const FString& Text) const
{
	UE_LOG(LogFicsitTD, Log, TEXT("Ansage: %s"), *Text);
	if (AFGChatManager* Chat = AFGChatManager::Get(GetWorld()))
	{
		FChatMessageStruct Nachricht;
		Nachricht.MessageText = FText::FromString(Text);
		Nachricht.MessageType = EFGChatMessageType::CMT_CustomMessage;
		Nachricht.MessageSender = TDTX(TEXT("Abwehrkern"), TEXT("Defense Core"));
		Nachricht.MessageSenderColor = FLinearColor(1.f, 0.5f, 0.05f);
		Chat->BroadcastChatMessage(Nachricht);
	}
}

bool UTDWaveSubsystem::StarteKernVerteidigung(ATDDefenseCore* Kern, int32 Wellen, float ErsterCountdown, float Countdown, int32 Stufe)
{
	mPlanWelle = -1;   // T3: Wellenplan haengt an Kern und Stufe
	if (!Kern || mLauf != ETDLauf::Aus)
	{
		return false;
	}
	mStufe = FMath::Clamp(Stufe, 0, 3);
	{
		FString Bericht;
		if (const int32 Weg = RaeumeWaisen(GetWorld(), Kern->GetActorLocation(), 60000.f, Bericht))
		{
			Ansage(TDF(TEXT("%d liegengebliebene Wellengegner entfernt (%s)."), TEXT("%d leftover wave enemies removed (%s)."), Weg, *Bericht));
		}
	}
	const FTDStufe& S = GetStufe(mStufe);
	mKern = Kern;
	Kern->mLaufEnde = 0;   // 0.9.6: neuer Durchgang, Ende-Auswahl im Kern-Menue weg
	Kern->SetzeMaxLP(S.KernLP);
	Kern->Aufladen();
	mWelle = 0;
	mWellenGesamt = FMath::Clamp(Wellen, 0, 500);   // 0 = endlos
	mZwischenCountdown = FMath::Max(10.f, Countdown < 0.f ? S.Zwischen : Countdown);
	mLauf = ETDLauf::Aufbau;
	mCountdown = FMath::Max(10.f, ErsterCountdown < 0.f ? S.Aufbau : ErsterCountdown);
	FrontenFuer(Kern);   // dieselben Fronten wie in der Vorschau (bei gleicher Stufe)
	const FString StufenText = StufenAnzeige(mStufe);
	const FString WellenText = mWellenGesamt > 0 ? TDF(TEXT("%d Wellen"), TEXT("%d waves"), mWellenGesamt) : TDT(TEXT("endlos"), TEXT("endless"));
	if (mManuell)
	{
		Ansage(TDF(TEXT("Einsatz gestartet (%s): %s, Kern %.0f LP. Bauen ohne Zeitdruck - Welle starten mit /td kern weiter."),
			TEXT("Operation started (%s): %s, Core %.0f HP. Build without time pressure - start the wave with /td kern weiter."),
			*StufenText, *WellenText, S.KernLP));
	}
	else
	{
		Ansage(TDF(TEXT("Kern-Verteidigung gestartet (%s): %s, Kern %.0f LP. Erste Welle in %.0f min, danach alle %.0f min Aufbau. Munitions-Produktion aufbauen und Tuerme versorgen! (/td kern weiter = sofort)"),
			TEXT("Core defense started (%s): %s, Core %.0f HP. First wave in %.0f min, then %.0f min build time each. Set up ammo production and supply your turrets! (/td kern weiter = now)"),
			*StufenText, *WellenText, S.KernLP, mCountdown / 60.f, mZwischenCountdown / 60.f));
	}
	for (const FString& Zeile : GetFrontenText(Kern))
	{
		Ansage(Zeile);
	}
	if (mGueltigeRichtungen > 0 && mGueltigeRichtungen < S.FrontenStart)
	{
		Ansage(TDF(TEXT("Nur %d begehbare Richtung(en): leichter, aber eng. Wegvorschau: /td kern wege"), TEXT("Only %d passable direction(s): easier, but tight. Path preview: /td kern wege"), mGueltigeRichtungen));
	}
	else
	{
		Ansage(TDT(TEXT("Wegvorschau der Gegner: /td kern wege"), TEXT("Enemy path preview: /td kern wege")));
	}
	MarkiereFronten();
	if (mGueltigeRichtungen == 0)
	{
		GetWorld()->GetTimerManager().SetTimer(mFrontenNachTimer, FTimerDelegate::CreateUObject(this, &UTDWaveSubsystem::PruefeFrontenNach), 15.f, true);
	}
	SpeichereLauf();
	return true;
}

void UTDWaveSubsystem::PruefeFrontenNach()
{
	ATDDefenseCore* Kern = mKern.Get();
	if (mLauf != ETDLauf::Aufbau || !Kern || mGueltigeRichtungen > 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(mFrontenNachTimer);
		return;
	}
	SucheFronten(Kern->GetActorLocation());
	UE_LOG(LogFicsitTD, Log, TEXT("Fronten-Nachpruefung: %d Richtungen begehbar"), mGueltigeRichtungen);
	if (mGueltigeRichtungen > 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(mFrontenNachTimer);
		Ansage(TDF(TEXT("Wege geprueft: die erste Welle kommt aus %s. Markierung auf Karte und Kompass ist aktualisiert."), TEXT("Paths checked: the first wave comes from %s. The marker on map and compass is updated."), *FrontName(0)));
		SpeichereLauf();
		MarkiereFronten();
	}
}

bool UTDWaveSubsystem::NaechsteWelleJetzt()
{
	if (mLauf != ETDLauf::Aufbau)
	{
		return false;
	}
	mCountdown = 0.f;
	return true;
}

bool UTDWaveSubsystem::SpringeZuWelle(int32 N)
{
	if (mLauf != ETDLauf::Aufbau || N < 1 || (mWellenGesamt > 0 && N > mWellenGesamt))
	{
		return false;
	}
	// Test (Marius 0.44): uebersprungene Wellen zaehlen als geschafft - EP, FP und Biomasse wie nach der Welle (Kern voll)
	int32 Belohnt = 0;
	if (UTDEinsatzSubsystem* Einsatz = GetWorld() ? GetWorld()->GetSubsystem<UTDEinsatzSubsystem>() : nullptr)
	{
		if (Einsatz->IstAktiv())
		{
			for (int32 W = mWelle + 1; W < N; ++W)
			{
				Einsatz->WelleBeendet(W, 100.f);
				++Belohnt;
			}
		}
	}
	mWelle = N - 1;
	SpeichereLauf();
	const FString BelohnText = Belohnt > 0 ? TDF(TEXT(", Belohnung fuer %d uebersprungene Wellen gutgeschrieben"), TEXT(", reward for %d skipped waves credited"), Belohnt) : FString();
	Ansage(TDF(TEXT("Test: naechste Welle ist %d (%d Front(en))%s."), TEXT("Test: next wave is %d (%d front(s))%s."), N, AktiveFrontenFuer(N), *BelohnText));
	if (AktiveFrontenFuer(N) > AktiveFrontenFuer(FMath::Max(1, N - 1)))
	{
		Ansage(TDF(TEXT("ACHTUNG: Ab Welle %d greifen sie auch aus %s an! Die neue Front ist auf Karte und Kompass markiert."), TEXT("WARNING: From wave %d they also attack from %s! The new front is marked on map and compass."), N, *FrontName(AktiveFrontenFuer(N) - 1)));
	}
	if (N % 10 == 0)
	{
		Ansage(TDF(TEXT("WARNUNG: Welle %d ist ein BOSSKAMPF."), TEXT("WARNING: Wave %d is a BOSS FIGHT."), N));
	}
	MarkiereFronten();
	return true;
}

void UTDWaveSubsystem::StoppeLauf(const FString& Grund)
{
	if (mLauf == ETDLauf::Aus)
	{
		return;
	}
	mLauf = ETDLauf::Aus;
	EntferneBossBalken();
	EntferneWellenLeiste();
	mNachschub.Reset();
	mNachschubIndex = 0;
	SpeichereLauf();
	MarkiereFronten();
	for (const FTDWellenGegner& G : mGegner)
	{
		if (G.bKern && G.Kreatur.IsValid())
		{
			if (AController* C = G.Kreatur->GetController())
			{
				C->Destroy();
			}
			G.Kreatur->Destroy();
		}
	}
	mGegner.RemoveAll([](const FTDWellenGegner& G) { return G.bKern; });
	Ansage(Grund);
}

void UTDWaveSubsystem::SpeichereLauf() const
{
	if (ATDDefenseCore* Kern = mKern.Get())
	{
		Kern->mLaufAktiv = mLauf != ETDLauf::Aus;
		Kern->mLaufWelle = mWelle;
		Kern->mLaufWellenGesamt = mWellenGesamt;
		Kern->mLaufCountdown = mCountdown;
		Kern->mLaufZwischen = mZwischenCountdown;
		Kern->mLaufStufe = mStufe;
		Kern->mLaufManuell = mManuell;
		Kern->mLaufFrontOrte.Reset();
		Kern->mLaufFrontWinkel.Reset();
		Kern->mLaufFrontWeg.Reset();
		for (const FTDFront& F : mFronten)
		{
			Kern->mLaufFrontOrte.Add(F.Ort);
			Kern->mLaufFrontWinkel.Add(F.Winkel);
			Kern->mLaufFrontWeg.Add(F.Weg);
		}
	}
}

void UTDWaveSubsystem::SetzeLaufFort(ATDDefenseCore* Kern)
{
	mPlanWelle = -1;   // T3: Wellenplan haengt an Kern und Stufe
	if (!Kern || mLauf != ETDLauf::Aus)
	{
		return;
	}
	mKern = Kern;
	{
		FString Bericht;
		if (const int32 Weg = RaeumeWaisen(GetWorld(), Kern->GetActorLocation(), 60000.f, Bericht))
		{
			Ansage(TDF(TEXT("%d liegengebliebene Wellengegner aus dem Spielstand entfernt (%s)."), TEXT("%d leftover wave enemies removed from the save (%s)."), Weg, *Bericht));
		}
	}
	mWellenGesamt = Kern->mLaufWellenGesamt;
	mZwischenCountdown = Kern->mLaufZwischen;
	mStufe = FMath::Clamp(Kern->mLaufStufe, 0, 3);
	mManuell = Kern->mLaufManuell;
	mFronten.Reset();
	for (int32 i = 0; i < Kern->mLaufFrontOrte.Num(); ++i)
	{
		FTDFront F;
		F.Ort = Kern->mLaufFrontOrte[i];
		F.Winkel = Kern->mLaufFrontWinkel.IsValidIndex(i) ? Kern->mLaufFrontWinkel[i] : 0.f;
		F.Weg = Kern->mLaufFrontWeg.IsValidIndex(i) ? Kern->mLaufFrontWeg[i] : 0.f;
		mFronten.Add(F);
	}
	mGueltigeRichtungen = mFronten.FilterByPredicate([](const FTDFront& F) { return F.Weg > 0.f; }).Num();
	mFrontenKern = Kern;
	mFrontenStufe = mStufe;
	if (mFronten.Num() == 0)
	{
		SucheFronten(Kern->GetActorLocation());   // Spielstand von vor 0.16
	}
	// War gerade eine Welle unterwegs, wird sie nach kurzer Pause wiederholt (Kreaturen werden nicht mitgespeichert)
	const bool bWarWelle = Kern->mLaufCountdown <= 0.f;
	mWelle = bWarWelle ? FMath::Max(0, Kern->mLaufWelle - 1) : Kern->mLaufWelle;
	mCountdown = bWarWelle ? 60.f : FMath::Max(30.f, Kern->mLaufCountdown);
	mLauf = ETDLauf::Aufbau;
	UE_LOG(LogFicsitTD, Log, TEXT("Kern-Verteidigung fortgesetzt: Welle %d/%d, Countdown %.0f s"), mWelle + 1, mWellenGesamt, mCountdown);
	// Ansage verzoegert, damit der Chat nach dem Laden bereit ist
	FTimerHandle Handle;
	TWeakObjectPtr<UTDWaveSubsystem> Weak = this;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
	{
		if (UTDWaveSubsystem* S = Weak.Get())
		{
			S->Ansage(TDF(TEXT("Kern-Verteidigung fortgesetzt: Welle %d in %.0f s."), TEXT("Core defense resumed: wave %d in %.0f s."), S->mWelle + 1, S->mCountdown));
			S->MarkiereFronten();
		}
	}), 5.f, false);
}

void UTDWaveSubsystem::KernGefallen(ATDDefenseCore* Kern)
{
	if (mLauf != ETDLauf::Aus && Kern == mKern.Get())
	{
		// 0.9.6 (Gesamtreview 12.11.): die verlorene Welle fehlte in Messung und Statistik - wie eine beendete Welle auswerten
		if (mLauf == ETDLauf::Welle)
		{
			EntferneWellenLeiste();
			WelleAusgewertet(0.f, true);
		}
		const int32 Welle = mWelle;
		StoppeLauf(TDF(TEXT("NIEDERLAGE: Der Abwehrkern ist in Welle %d von %d gefallen. Im Kern-Menü (E am Kern): Welle wiederholen, neu starten oder Einsatz beenden."),
			TEXT("DEFEAT: The Defense Core fell in wave %d of %d. In the Core menu (E at the Core): retry the wave, restart or end the operation."), mWelle, mWellenGesamt));
		Kern->mLaufEnde = 2;
		Kern->mLaufEndWelle = Welle;
	}
}

FString UTDWaveSubsystem::GetLaufText() const
{
	switch (mLauf)
	{
	case ETDLauf::Aufbau:
	{
		const FString StufenText = StufenAnzeige(mStufe);
		const FString GesamtText = mWellenGesamt > 0 ? TDF(TEXT(" von %d"), TEXT(" of %d"), mWellenGesamt) : TDT(TEXT(" (endlos)"), TEXT(" (endless)"));
		return TDF(TEXT("%s - Aufbau: Welle %d%s in %d:%02d min"), TEXT("%s - Build: wave %d%s in %d:%02d min"), *StufenText, mWelle + 1, *GesamtText,
			FMath::FloorToInt(mCountdown / 60.f), FMath::FloorToInt(mCountdown) % 60);
	}
	case ETDLauf::Welle:
	{
		int32 Lebend = 0;
		for (const FTDWellenGegner& G : mGegner)
		{
			Lebend += G.bKern && G.Kreatur.IsValid() && G.Kreatur->IsAliveAndWell() ? 1 : 0;
		}
		const int32 Offen = FMath::Max(0, mNachschub.Num() - mNachschubIndex);
		return TDF(TEXT("Welle %d von %d: %d / %d Gegner uebrig (%d unterwegs), aus %s"), TEXT("Wave %d of %d: %d / %d enemies left (%d on the way), from %s"), mWelle, mWellenGesamt, Lebend + Offen, mWellenGroesse, Lebend, *mRichtungen);
	}
	default:
		return TDT(TEXT("Bereit - /td kern start"), TEXT("Ready - /td kern start"));
	}
}

FString UTDWaveSubsystem::OptikFamilie(const UClass* Klasse)
{
	const FString N = Klasse ? Klasse->GetName() : FString();
	if (N.Contains(TEXT("Johnny")))        return TEXT("Johnny");
	if (N.Contains(TEXT("NuclearHog")))    return TEXT("NuclearHog");
	if (N.Contains(TEXT("CliffHog")))      return TEXT("CliffHog");
	if (N.Contains(TEXT("AlphaHog")))      return TEXT("AlphaHog");
	if (N.Contains(TEXT("Hog")))           return TEXT("Hog");
	if (N.Contains(TEXT("Aquatic")))       return TEXT("WasserSpitter");
	if (N.Contains(TEXT("Spitter")))       return N.Contains(TEXT("Alpha")) ? TEXT("SpitterAlpha") : TEXT("Spitter");
	if (N.Contains(TEXT("Stinger_Child"))) return TEXT("StingerKlein");
	if (N.Contains(TEXT("EliteStinger")))  return TEXT("StingerElite");
	if (N.Contains(TEXT("Stinger")))       return TEXT("Stinger");
	return TEXT("Hog");
}

float UTDWaveSubsystem::OptikWert(const UClass* Klasse, bool bBoss, const TCHAR* Wert, float Ersatz)
{
	return TDBalance::Wert(FString::Printf(TEXT("Optik.%s.%s.%s"), *OptikFamilie(Klasse), bBoss ? TEXT("Boss") : TEXT("Elite"), Wert), Ersatz);
}

UMaterialInstanceDynamic* UTDWaveSubsystem::Huelle(const UClass* Klasse, bool bBoss)
{
	const FString Schluessel = FString::Printf(TEXT("%s.%s"), bBoss ? TEXT("Boss") : TEXT("Elite"), *OptikFamilie(Klasse));
	TObjectPtr<UMaterialInstanceDynamic>& Ziel = mHuellen.FindOrAdd(Schluessel);
	if (!Ziel)
	{
		if (UMaterialInterface* Basis = LoadObject<UMaterialInterface>(nullptr, TEXT("/FicsitTD/Effects/M_TD_Huelle.M_TD_Huelle")))
		{
			Ziel = UMaterialInstanceDynamic::Create(Basis, this);
			const FLinearColor Farbe(OptikWert(Klasse, bBoss, TEXT("R"), bBoss ? 1.f : 0.55f), OptikWert(Klasse, bBoss, TEXT("G"), bBoss ? 0.18f : 0.35f),
				OptikWert(Klasse, bBoss, TEXT("B"), bBoss ? 0.08f : 1.f));
			Ziel->SetVectorParameterValue(TEXT("Farbe"), Farbe);
			Ziel->SetScalarParameterValue(TEXT("Staerke"), OptikWert(Klasse, bBoss, TEXT("Leuchten"), 0.5f));
			Ziel->SetScalarParameterValue(TEXT("Rand"), TDBalance::Wert(TEXT("Optik.Rand"), 4.5f));
		}
		UE_LOG(LogFicsitTD, Log, TEXT("Huelle %s: %s"), *Schluessel, Ziel ? TEXT("geladen") : TEXT("M_TD_Huelle fehlt"));
	}
	return Ziel;
}

void UTDWaveSubsystem::Faerbe(AFGCreature* Kreatur, const UClass* Klasse, bool bBoss)
{
	USkeletalMeshComponent* Modell = Kreatur ? Kreatur->GetMesh() : nullptr;
	if (!Modell || !Modell->GetSkeletalMeshAsset())
	{
		return;
	}
	// Haut: zweite Kopie des Modells folgt der Pose des Originals und multipliziert es mit der Tonfarbe (keine Original-Textur geaendert)
	const float Ton = OptikWert(Klasse, bBoss, TEXT("Ton"), bBoss ? 0.5f : 0.3f);
	if (Ton > 0.01f)
	{
		const FString Schluessel = FString::Printf(TEXT("Ton.%s.%s"), bBoss ? TEXT("Boss") : TEXT("Elite"), *OptikFamilie(Klasse));
		TObjectPtr<UMaterialInstanceDynamic>& T = mHuellen.FindOrAdd(Schluessel);
		if (!T)
		{
			if (UMaterialInterface* Basis = LoadObject<UMaterialInterface>(nullptr, TEXT("/FicsitTD/Effects/M_TD_Toenung.M_TD_Toenung")))
			{
				T = UMaterialInstanceDynamic::Create(Basis, this);
				T->SetVectorParameterValue(TEXT("Farbe"), FLinearColor(OptikWert(Klasse, bBoss, TEXT("TonR"), 0.8f), OptikWert(Klasse, bBoss, TEXT("TonG"), 0.35f),
					OptikWert(Klasse, bBoss, TEXT("TonB"), 0.22f)));
				T->SetScalarParameterValue(TEXT("Staerke"), FMath::Clamp(Ton, 0.f, 1.f));
			}
			UE_LOG(LogFicsitTD, Log, TEXT("Toenung %s: %s"), *Schluessel, T ? TEXT("geladen") : TEXT("M_TD_Toenung fehlt"));
		}
		if (T)
		{
			USkeletalMeshComponent* Kopie = NewObject<USkeletalMeshComponent>(Kreatur, TEXT("TD_Toenung"));
			Kopie->SetSkeletalMeshAsset(Modell->GetSkeletalMeshAsset());
			Kopie->SetupAttachment(Modell);
			Kopie->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Kopie->SetCastShadow(false);
			Kopie->SetCanEverAffectNavigation(false);
			Kopie->bReceivesDecals = false;
			// gleiche Sichtweite wie das Original; ausserdem blendet das Gegnerhirn die Kopie aus, solange das Original nicht gezeichnet wird
			Kopie->LDMaxDrawDistance = Modell->LDMaxDrawDistance;
			Kopie->SetCachedMaxDrawDistance(Modell->CachedMaxDrawDistance);
			Kopie->RegisterComponent();
			Kopie->SetLeaderPoseComponent(Modell);
			if (UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(Kreatur))
			{
				Hirn->mToenung = Kopie;
			}
			for (int32 i = 0; i < Kopie->GetNumMaterials(); ++i)
			{
				Kopie->SetMaterial(i, T);
			}
		}
	}
	// Fell: nur Materialien mit dem Wert HairColor (Hogs), je Kreatur eine dynamische Instanz
	if (OptikWert(Klasse, bBoss, TEXT("Fell"), bBoss ? 1.f : 0.f) > 0.5f)
	{
		const FLinearColor Fell(OptikWert(Klasse, bBoss, TEXT("FellR"), 0.45f), OptikWert(Klasse, bBoss, TEXT("FellG"), 0.12f), OptikWert(Klasse, bBoss, TEXT("FellB"), 0.06f));
		for (int32 i = 0; i < Modell->GetNumMaterials(); ++i)
		{
			FLinearColor Alt;
			const UMaterialInterface* M = Modell->GetMaterial(i);
			if (M && M->GetVectorParameterValue(FHashedMaterialParameterInfo(TEXT("HairColor")), Alt))
			{
				if (UMaterialInstanceDynamic* D = Modell->CreateDynamicMaterialInstance(i))
				{
					D->SetVectorParameterValue(TEXT("HairColor"), Fell);
				}
			}
		}
	}
}

bool UTDWaveSubsystem::GetBossStatus(FString& Name, float& LP, float& MaxLP, int32& Lebend, int32& Gesamt) const
{
	LP = MaxLP = 0.f;
	Lebend = 0;
	Gesamt = mBosse.Num();
	Name = mBossName;
	for (const TWeakObjectPtr<AFGCreature>& B : mBosse)
	{
		const AFGCreature* K = B.Get();
		const UFGHealthComponent* H = K ? K->GetHealthComponent() : nullptr;
		if (H)
		{
			MaxLP += H->GetMaxHealth();
			if (K->IsAliveAndWell())
			{
				LP += H->GetCurrentHealth();
				++Lebend;
			}
		}
	}
	return Gesamt > 0;
}

FTDWellenStatus UTDWaveSubsystem::GetWellenStatus() const
{
	FTDWellenStatus S;
	if (mLauf != ETDLauf::Welle)
	{
		return S;
	}
	S.bGueltig = true;
	S.Welle = mWelle;
	S.WellenGesamt = mWellenGesamt;
	S.Art = WellenArtName(mPlanArt);
	S.Groesse = mNachschub.Num();
	// Gesamt und noch offen je Art aus der Warteschlange, lebend aus den Wellengegnern
	auto Eintrag = [&S](const UClass* K, bool bBoss) -> FTDArtStatus&
	{
		const FString Pfad = K ? K->GetPathName() : FString();
		for (FTDArtStatus& A : S.Arten)
		{
			if (A.Pfad == Pfad && A.bBoss == bBoss)
			{
				return A;
			}
		}
		FTDArtStatus& A = S.Arten.AddDefaulted_GetRef();
		A.Pfad = Pfad;
		A.bBoss = bBoss;
		return A;
	};
	for (int32 i = 0; i < mNachschub.Num(); ++i)
	{
		FTDArtStatus& A = Eintrag(mNachschub[i].Klasse, mNachschub[i].bBoss);
		++A.Gesamt;
		A.Offen += i >= mNachschubIndex ? 1 : 0;
	}
	for (const FTDWellenGegner& G : mGegner)
	{
		const AFGCreature* K = G.Kreatur.Get();
		if (G.bKern && K && K->IsAliveAndWell())
		{
			++Eintrag(K->GetClass(), K->Tags.Contains(TEXT("FicsitTDBoss"))).Lebend;
		}
	}
	for (const FTDArtStatus& A : S.Arten)
	{
		S.Lebend += A.Lebend;
		S.Offen += A.Offen;
	}
	S.Besiegt = FMath::Max(0, S.Groesse - S.Lebend - S.Offen);
	return S;
}

void UTDWaveSubsystem::ZeigeWellenLeiste()
{
	if (mWellenLeiste)
	{
		return;
	}
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		mWellenLeiste = CreateWidget<UTDWellenLeiste>(PC, UTDWellenLeiste::StaticClass());
		if (mWellenLeiste)
		{
			mWellenLeiste->AddToViewport(3);
		}
	}
}

void UTDWaveSubsystem::EntferneWellenLeiste()
{
	if (mWellenLeiste)
	{
		mWellenLeiste->RemoveFromParent();
		mWellenLeiste = nullptr;
	}
}

bool UTDWaveSubsystem::NachschubHatBoss() const
{
	for (int32 i = mNachschubIndex; i < mNachschub.Num(); ++i)
	{
		if (mNachschub[i].bBoss)
		{
			return true;
		}
	}
	return false;
}

void UTDWaveSubsystem::ZeigeBossBalken()
{
	// T4: Lebensbalken oben in der Mitte, solange ein Boss lebt (T5: erscheint mit dem ersten Boss)
	if (mBossBalken)
	{
		return;
	}
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		mBossBalken = CreateWidget<UTDBossBalken>(PC, UTDBossBalken::StaticClass());
		if (mBossBalken)
		{
			mBossBalken->AddToViewport(4);
		}
	}
}

void UTDWaveSubsystem::EntferneBossBalken()
{
	if (mBossBalken)
	{
		mBossBalken->RemoveFromParent();
		mBossBalken = nullptr;
	}
	mBosse.Reset();
}

int32 UTDWaveSubsystem::SpawneGruppe(TSubclassOf<AFGCreature> Klasse, int32 Anzahl, const FVector& Mitte, const FVector& FrontOrt, float LPFaktor, int32 Elite, bool bBoss,
	const TArray<FVector>* VorWeg)
{
	// Elite (T3: im Wellenplan festgelegt): die ersten Elite der Gruppe sind groesser und haben 2,5-fache Lebenspunkte
	UWorld* World = GetWorld();
	if (!World || !Klasse)
	{
		return 0;
	}
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	// Weg Front -> Kern einmal je Gruppe (ohne Wasser); ihm folgen die Gehirne
	const TArray<FVector> Weg = VorWeg ? *VorWeg : UTDGegnerHirn::BerechneWeg(World, FrontOrt, mKern.Get());
	int32 Gespawnt = 0;
	int32 ProbeAbgelehnt = 0, AnDerFront = 0, FrontNichtAufNetz = 0;
	for (int32 i = 0; i < Anzahl; ++i)
	{
		// Spawnfeld: quer zur Angriffsrichtung verteilt (Wellen.SpawnBreite, Marius 03.11.2026), bevorzugt ein erreichbarer Punkt des Netzes
		FVector Ort;
		FNavLocation NavOrt;
		const FVector Quer = FVector::CrossProduct((FrontOrt - Mitte).GetSafeNormal2D(), FVector::UpVector);
		const FVector Teilort = FrontOrt + Quer * FMath::FRandRange(-1.f, 1.f) * TDBalance::Wert(TEXT("Wellen.SpawnBreite"), 40.f) * 100.f;
		// 0.78.2 Ursache der "Abstuerze" (Paket Ursache 14:54): Spawnpunkte lagen im See - die Gegner schwammen, wurden versetzt
		// und sanken bis -180 m. Gesucht wird deshalb mit dem Filter der Wege (ohne Wasserbereich) und der Punkt zusaetzlich
		// gegen die Wasservolumen geprueft (das Netz kennt nicht jedes Wasser als eigenen Bereich)
		ANavigationData* NavDaten = Nav ? Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
		const FSharedConstNavQueryFilter Filter = NavDaten ? UNavigationQueryFilter::GetQueryFilter(*NavDaten, this, UTDNavFilter::StaticClass()) : nullptr;
		bool bTrocken = false;
		// 0.84.6 (Pruefung0845: oft alle 6 Proben verworfen, die 4 Versuche danach ungeprueft wieder auf der Insel): Ziel der Probe ist
		// die aufs Netz gesetzte Front (die Abfrage verlangt einen begehbaren Endpunkt), und jeder Versuch wird geprueft.
		FNavLocation FrontNav;
		const bool bFrontAufNetz = NavDaten && Nav->ProjectPointToNavigation(FrontOrt, FrontNav, FVector(800.f, 800.f, 600.f), NavDaten, Filter);
		FrontNichtAufNetz += bFrontAufNetz ? 0 : 1;
		for (int32 Versuch = 0; Versuch < 10 && Nav && !bTrocken; ++Versuch)
		{
			const FVector& Mittelpunkt = Versuch < 6 ? Teilort : FrontOrt;
			// 0.78.2 (Prueflauf 15:31: 507 Nachzuegler am Spawn, beim Versetzen im Median 62 m hoeher): der seitlich versetzte
			// Suchpunkt rastete auf eine tiefere Ebene des Netzes ein (Senke, Schlucht) - nur Punkte auf Hoehe der Front
			bTrocken = Nav->GetRandomReachablePointInRadius(Mittelpunkt, Versuch < 6 ? 1000.f : 1500.f, NavOrt, NavDaten, Filter)
				&& FMath::Abs(NavOrt.Location.Z - FrontOrt.Z) <= TDBalance::Wert(TEXT("Wellen.SpawnHoehe"), 4.f) * 100.f
				&& !PunktImWasser(World, NavOrt.Location);
			// 0.84.3 Ursache der Nachzuegler am Frontstart (Pruefung0842, Hirn-Diagnose: 85 von 479 standen am Start der Front
			// Suedosten mit "Weg VERSPERRT", Rest wenige Meter): "erreichbar" galt nur vom seitlich versetzten Suchpunkt aus. Lag der
			// auf einem abgeschnittenen Stueck Netz (Felsabsatz), erschien der Gegner dort und kam nie vom Fleck. Jetzt muss der
			// Punkt auch die Front selbst erreichen (deren Weg zum Kern ist bei der Wahl geprueft); schneller Grobtest.
			if (bTrocken && bFrontAufNetz)
			{
				// 0.84.4 (Pruefung0843: weiter 92 Nachzuegler "Weg VERSPERRT" an einer Stelle): FPathFindingQuery erlaubt von Haus aus
				// Teilpfade, der Test war damit fast immer wahr. Jetzt nur ein vollstaendiger Weg, genau statt grob.
				FPathFindingQuery Probe(this, *NavDaten, NavOrt.Location, FrontNav.Location, Filter);
				Probe.SetAllowPartialPaths(false);
				bTrocken = Nav->TestPathSync(Probe, EPathFindingMode::Regular);
				ProbeAbgelehnt += bTrocken ? 0 : 1;
			}
		}
		if (bTrocken)
		{
			Ort = NavOrt.Location + FVector(0, 0, 150.f);
		}
		else if (Nav && !PunktImWasser(World, FrontOrt))
		{
			// die Front selbst ist bei der Wahl als trocken geprueft (FrontenFuer)
			Ort = FrontOrt + FVector(0, 0, 150.f);
			++AnDerFront;
		}
		else
		{
			Ort = FrontOrt + FVector(FMath::FRandRange(-1500.f, 1500.f), FMath::FRandRange(-1500.f, 1500.f), 0.f);
			FHitResult Hit;
			FCollisionQueryParams Query(TEXT("FicsitTDGround"), false);
			if (World->LineTraceSingleByChannel(Hit, Ort + FVector(0, 0, 20000.f), Ort - FVector(0, 0, 40000.f), ECC_Visibility, Query))
			{
				Ort = Hit.ImpactPoint + FVector(0, 0, 150.f);
			}
		}
		const bool bElite = i < Elite;
		AFGCreature* Kreatur = UTDGegnerHirn::Spawne(World, Klasse, Ort, Weg, mKern.Get(), LPFaktor * (bElite ? TDBalance::Wert(TEXT("Wellen.EliteLP"), 2.5f) : 1.f));
		if (!Kreatur)
		{
			continue;
		}
		// T4: Elite groesser mit violetter Huelle, Boss deutlich groesser mit rot pulsierender Huelle
		if (bBoss)
		{
			if (USkeletalMeshComponent* Modell = Kreatur->GetMesh())
			{
				// Groesse aufs Modell, nicht auf den Actor: beim Tod schrumpfte der Koerper sonst zurueck (Test 0.49.2)
				Modell->SetRelativeScale3D(Modell->GetRelativeScale3D() * OptikWert(Klasse, true, TEXT("Groesse"), 1.8f));
			}
			Kreatur->Tags.AddUnique(TEXT("FicsitTDBoss"));
			mBosse.Add(Kreatur);
			ZeigeBossBalken();
		}
		else if (bElite)
		{
			Kreatur->Tags.AddUnique(TEXT("FicsitTDElite"));   // Zielwahl "Elite und Boss zuerst"
			if (USkeletalMeshComponent* Modell = Kreatur->GetMesh())
			{
				Modell->SetRelativeScale3D(Modell->GetRelativeScale3D() * OptikWert(Klasse, false, TEXT("Groesse"), 1.3f));
			}
		}
		if ((bBoss || bElite) && Kreatur->GetMesh())
		{
			if (UMaterialInstanceDynamic* H = Huelle(Klasse, bBoss))
			{
				Kreatur->GetMesh()->SetOverlayMaterial(H);
			}
			Faerbe(Kreatur, Klasse, bBoss);
		}
		FTDWellenGegner G;
		G.Kreatur = Kreatur;
		G.bKern = true;
		mGegner.Add(G);
		++Gespawnt;
	}
	if (ProbeAbgelehnt > 0 || AnDerFront > 0 || FrontNichtAufNetz > 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Spawnprobe: %s x%d, %d Punkte ohne vollen Weg zur Front verworfen, %d an der Front selbst, Front %d-mal nicht auf dem Netz"), *Klasse->GetName(), Anzahl, ProbeAbgelehnt, AnDerFront, FrontNichtAufNetz);
	}
	return Gespawnt;
}

TArray<FTDPlanGruppe> UTDWaveSubsystem::PlanFuer(int32 Nummer, int32& OutArt) const
{
	TArray<FTDPlanGruppe> Plan;
	// Reproduzierbar je Kern (Ort), Stufe und Welle: dieselbe Vorschau auch nach dem Laden
	const ATDDefenseCore* Kern = mKern.Get();
	const FVector Ort = Kern ? Kern->GetActorLocation() : FVector::ZeroVector;
	const uint32 Saat = HashCombine(HashCombine(GetTypeHash(FIntVector(FMath::RoundToInt(Ort.X / 100.f), FMath::RoundToInt(Ort.Y / 100.f), 0)), GetTypeHash(mStufe)), GetTypeHash(Nummer));
	// 0.84: optional je Einsatz verschieden (ATDDefenseCore::mLaufSaat, Balance Wellen.VariationJeEinsatz)
	FRandomStream Zufall(static_cast<int32>(Kern && Kern->mLaufSaat ? HashCombine(Saat, GetTypeHash(Kern->mLaufSaat)) : Saat));
	const FTDStufe& S = GetStufe(mStufe);
	int32 Budget = FMath::Max(2, FMath::RoundToInt(S.Budget * (TDBalance::Wert(TEXT("Wellen.BudgetGrund"), 4.f)
		+ TDBalance::Wert(TEXT("Wellen.BudgetJeWelle"), 2.5f) * Nummer + TDBalance::Wert(TEXT("Wellen.BudgetQuadrat"), 0.12f) * Nummer * Nummer)));
	// 0.84 (Pruefstand 08.11.: die meisten Linien fielen am ersten Boss in W10): Bosswellen mit weniger Begleitern, der Boss ist
	// der Hoehepunkt der Welle. Wellen.BossBudget = Anteil des normalen Budgets (1 = wie bis 0.83)
	if (Nummer % 10 == 0)
	{
		Budget = FMath::Max(2, FMath::RoundToInt(Budget * TDBalance::Wert(TEXT("Wellen.BossBudget"), 1.f)));
	}
	// T5: Gesamtzahl je Welle; gleichzeitig lebende begrenzt der Nachschub (Wellen.MaxGleichzeitig)
	const int32 MaxGegner = TDBalance::Ganz(TEXT("Wellen.MaxGesamt"), 2 * KernMaxGegner);
	// Wellenart: Ansturm, Panzer, Gemischt, Fernkampf, Schwarm im Wechsel
	const int32 Art = Nummer % 5;
	const int32 Rolle = Art == 1 ? 0 : Art == 2 ? 1 : Art == 4 ? 2 : Art == 0 ? 3 : -1;
	OutArt = Art;
	int32 Anzahl = 0;

	// Einfuehrungs-Regel (Gegnertakt-Recherche 3.2): eine neue Art kommt in ihrer ersten Welle als kleine eigene Gruppe
	TArray<const FTDKatalogEintrag*> Erlaubt;
	for (const FTDKatalogEintrag& E : KernKatalog)
	{
		if (AbWelleVon(E) == Nummer)
		{
			FTDPlanGruppe G;
			G.Pfad = E.Pfad;
			G.Anzahl = FMath::Clamp(FMath::RoundToInt(6.f / E.Punkte), 2, 6);
			G.bNeu = true;
			Budget -= G.Anzahl * E.Punkte;
			Anzahl += G.Anzahl;
			Plan.Add(G);
		}
		else if (AbWelleVon(E) < Nummer)
		{
			Erlaubt.Add(&E);
		}
	}
	// Familie der laufenden Phase haeufiger (Gewicht Katalog.PhasenGewicht, Standard 3), aeltere Familien bleiben im Mix; endlos alle gleich
	const int32 Phase = PhaseVon(Nummer);
	const float PhasenGewicht = Phase < KernPhasenAnzahl ? TDBalance::Wert(TEXT("Katalog.PhasenGewicht"), 3.f) : 1.f;
	auto Waehle = [&](const TArray<const FTDKatalogEintrag*>& Liste) -> const FTDKatalogEintrag*
	{
		float Summe = 0.f;
		for (const FTDKatalogEintrag* K : Liste)
		{
			Summe += K->Phase - 1 == Phase ? PhasenGewicht : 1.f;
		}
		float Wurf = Zufall.FRand() * Summe;
		for (const FTDKatalogEintrag* K : Liste)
		{
			Wurf -= K->Phase - 1 == Phase ? PhasenGewicht : 1.f;
			if (Wurf <= 0.f)
			{
				return K;
			}
		}
		return Liste.Last();
	};
	// Rest: passende Rolle bevorzugt, sonst gemischt; hoechstens MaxGegner (dann teurere Gegner)
	TMap<const FTDKatalogEintrag*, int32> Zaehler;
	int32 Versuche = 0;
	while (Budget > 0 && Anzahl < MaxGegner && Erlaubt.Num() > 0 && ++Versuche < 5000)
	{
		const FTDKatalogEintrag* Wahl = Waehle(Erlaubt);
		if (Rolle >= 0 && Zufall.FRand() < 0.65f)
		{
			TArray<const FTDKatalogEintrag*> Passend = Erlaubt.FilterByPredicate([Rolle](const FTDKatalogEintrag* E) { return E->Rolle == Rolle; });
			if (Passend.Num() > 0)
			{
				Wahl = Waehle(Passend);
			}
		}
		// spaete Wellen: billige Gegner seltener, damit die Anzahl begrenzt bleibt
		if (Wahl->Punkte * 12 < Nummer && Zufall.FRand() < 0.6f)
		{
			continue;
		}
		Zaehler.FindOrAdd(Wahl)++;
		Budget -= Wahl->Punkte;
		++Anzahl;
	}
	// Reihenfolge wie im Katalog (billig -> teuer), neue Arten dahinter - damit die Vorschau ruhig aussieht
	TArray<FTDPlanGruppe> Neue = MoveTemp(Plan);
	Plan.Reset();
	for (const FTDKatalogEintrag& E : KernKatalog)
	{
		if (const int32* N = Zaehler.Find(&E))
		{
			FTDPlanGruppe G;
			G.Pfad = E.Pfad;
			G.Anzahl = *N;
			Plan.Add(G);
		}
	}
	Plan.Append(Neue);
	// Elite ab der Elite-Welle der Stufe (Anteil steigt bis 30 %); neue Arten bleiben ohne Elite
	const float EliteChance = Nummer >= S.EliteAb ? FMath::Min(0.3f, 0.1f + 0.01f * (Nummer - S.EliteAb)) : 0.f;
	for (FTDPlanGruppe& G : Plan)
	{
		for (int32 i = 0; i < G.Anzahl && !G.bNeu; ++i)
		{
			G.Elite += Zufall.FRand() < EliteChance ? 1 : 0;
		}
	}
	// Boss am Ende jeder Phase (Wellen-Progression); endlos reihum aus allen Phasen
	if (Nummer % 10 == 0)
	{
		const int32 B = (Nummer / 10 - 1) % KernPhasenAnzahl;
		const FTDPhasenInfo& P = KernPhasen[B];
		FTDPlanGruppe G;
		G.bBoss = true;
		G.Pfad = P.BossPfad;
		G.Anzahl = FMath::Max(1, TDBalance::Ganz(FString::Printf(TEXT("Phasen.%d.BossAnzahl"), B + 1), P.BossAnzahl));
		G.BossLP = TDBalance::Wert(FString::Printf(TEXT("Phasen.%d.BossLP"), B + 1), P.BossLP);
		G.BossName = PhasenBossName(B);
		Plan.Add(G);
	}
	return Plan;
}

void UTDWaveSubsystem::PlaneWelle(int32 Nummer)
{
	mPlan = PlanFuer(Nummer, mPlanArt);
	mPlanWelle = Nummer;
	int32 Anzahl = 0;
	for (const FTDPlanGruppe& G : mPlan)
	{
		Anzahl += G.Anzahl;
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Wellenplan %d: %d Gegnerarten, %d Gegner"), Nummer, mPlan.Num(), Anzahl);
}

FTDWellenVorschau UTDWaveSubsystem::GetVorschau(int32 Welle)
{
	FTDWellenVorschau V;
	if (mLauf == ETDLauf::Aus || !mKern.IsValid())
	{
		return V;
	}
	const bool bLaeuftJetzt = mLauf == ETDLauf::Welle;
	const int32 Aktuell = bLaeuftJetzt ? mWelle : mWelle + 1;
	V.Welle = Welle > 0 ? Welle : Aktuell;
	V.bLaeuft = bLaeuftJetzt && V.Welle == mWelle;
	V.bAktuell = V.Welle == Aktuell;
	if (mWellenGesamt > 0 && V.Welle > mWellenGesamt)
	{
		return V;
	}
	int32 Art = 0;
	TArray<FTDPlanGruppe> Plan;
	if (V.bAktuell)
	{
		if (mPlanWelle != V.Welle)
		{
			PlaneWelle(V.Welle);
		}
		Plan = mPlan;
		Art = mPlanArt;
	}
	else
	{
		Plan = PlanFuer(V.Welle, Art);
	}
	V.bGueltig = true;
	V.WellenGesamt = mWellenGesamt;
	V.Art = WellenArtName(Art);
	V.Tipp = TippFuer(V.Welle);
	V.bZusammensetzung = GetStufe(mStufe).bZusammensetzung;
	V.Countdown = V.bLaeuft || mManuell || !V.bAktuell ? -1.f : mCountdown;
	V.Gruppen = Plan;
	for (int32 i = 0; i < AktiveFrontenFuer(V.Welle) && mFronten.IsValidIndex(i); ++i)
	{
		V.Fronten += (i ? TEXT(", ") : TEXT("")) + Himmelsrichtung(mFronten[i].Winkel);
	}
	for (const FTDPlanGruppe& G : Plan)
	{
		V.Gegner += G.Anzahl;
		V.Elite += G.Elite;
		V.Punkte += Steckbrief(G.Pfad).Punkte * G.Anzahl;
	}
	return V;
}

TArray<FTDZeitleistenWelle> UTDWaveSubsystem::GetZeitleiste() const
{
	TArray<FTDZeitleistenWelle> Liste;
	if (mLauf == ETDLauf::Aus)
	{
		return Liste;
	}
	const int32 Aktuell = mLauf == ETDLauf::Welle ? mWelle : mWelle + 1;
	// endlos: Fenster um die aktuelle Welle
	const int32 Von = mWellenGesamt > 0 ? 1 : FMath::Max(1, Aktuell - 5);
	const int32 Bis = mWellenGesamt > 0 ? mWellenGesamt : Aktuell + 24;
	for (int32 N = Von; N <= Bis; ++N)
	{
		FTDZeitleistenWelle Z;
		Z.Welle = N;
		Z.bBoss = N % 10 == 0;
		Z.bNeueFront = N > 1 && AktiveFrontenFuer(N) > AktiveFrontenFuer(N - 1);
		Z.Zustand = N < Aktuell ? 0 : N == Aktuell ? (mLauf == ETDLauf::Welle ? 2 : 1) : 3;
		for (const FTDKatalogEintrag& E : KernKatalog)
		{
			if (AbWelleVon(E) == N)
			{
				Z.NeueArten.Add(E.Pfad);
			}
		}
		Liste.Add(Z);
	}
	return Liste;
}

int32 UTDWaveSubsystem::RaeumeWaisen(UWorld* World, const FVector& Mitte, float Radius, FString& Bericht)
{
	if (!World)
	{
		return 0;
	}
	TSet<const UClass*> Arten;
	for (const FTDKatalogEintrag& E : KernKatalog)
	{
		if (UClass* K = LoadClass<AFGCreature>(nullptr, E.Pfad))
		{
			Arten.Add(K);
		}
	}
	for (const FTDPhasenInfo& P : KernPhasen)
	{
		if (UClass* K = LoadClass<AFGCreature>(nullptr, P.BossPfad))
		{
			Arten.Add(K);
		}
	}
	TMap<FString, int32> Je;
	int32 Weg = 0;
	for (TActorIterator<AFGCreature> It(World); It; ++It)
	{
		AFGCreature* K = *It;
		// echte Wildtiere haben einen Spawner; aktuelle Wellengegner tragen den Tag
		if (!K || K->IsActorBeingDestroyed() || K->Tags.Contains(TEXT("FicsitTD")) || K->GetSpawner() || !Arten.Contains(K->GetClass())
			|| FVector::Dist2D(K->GetActorLocation(), Mitte) > Radius)
		{
			continue;
		}
		FString Name = K->GetClass()->GetName().Replace(TEXT("Char_"), TEXT(""));
		Name.RemoveFromEnd(TEXT("_C"));   // nicht ueberall ersetzen: Stinger_Child
		Je.FindOrAdd(Name)++;
		if (AController* C = K->GetController())
		{
			C->Destroy();
		}
		K->Destroy();
		++Weg;
	}
	Bericht.Reset();
	for (const TPair<FString, int32>& P : Je)
	{
		Bericht += FString::Printf(TEXT("%s%s %d"), Bericht.IsEmpty() ? TEXT("") : TEXT(", "), *P.Key, P.Value);
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Verwaiste Wellengegner entfernt: %d (%s) im Umkreis %.0f m"), Weg, Bericht.IsEmpty() ? TEXT("-") : *Bericht, Radius / 100.f);
	return Weg;
}

FTDKreaturSteckbrief UTDWaveSubsystem::Steckbrief(const FString& Pfad)
{
	// Reihenfolge: speziellere Namen zuerst (AlphaHog vor Hog, EliteStinger vor Stinger)
	// 0.85 (L1): NameEn/TextEn = englische Anzeige (Spielsprache), gewaehlt beim Abruf
	struct FEintrag { const TCHAR* Teil; const TCHAR* Name; const TCHAR* Desc; const TCHAR* Text; const TCHAR* NameEn; const TCHAR* TextEn; };
	static const FEintrag Tabelle[] = {
		{ TEXT("Johnny"), TEXT("Riesen-Hog"), TEXT("Desc_HogAlpha"), TEXT("Ein gewaltiger Hog, der Mauern mit wenigen Stößen einreißt. Langsam, aber kaum aufzuhalten."),
			TEXT("Giant Hog"), TEXT("A huge hog that tears down walls in a few hits. Slow, but hard to stop.") },
		{ TEXT("NuclearHog"), TEXT("Nuklear-Hog"), TEXT("Desc_HogNuclear"), TEXT("Strahlender Hog mit dicker Haut. Langsamer als seine Verwandten, rammt aber hart."),
			TEXT("Nuclear Hog"), TEXT("Radiant hog with thick skin. Slower than its relatives, but rams hard.") },
		{ TEXT("CliffHog"), TEXT("Klippen-Hog"), TEXT("Desc_HogCliff"), TEXT("Schwer gepanzerter Hog aus den Felsen. Hält viel aus und rammt Mauern mit voller Wucht."),
			TEXT("Cliff Hog"), TEXT("Heavily armored hog from the rocks. Takes a lot of damage and rams walls at full force.") },
		{ TEXT("AlphaHog"), TEXT("Alpha-Hog"), TEXT("Desc_HogAlpha"), TEXT("Großer, gepanzerter Anführer der Hogs. Rammt Mauern mit doppelter Wucht."),
			TEXT("Alpha Hog"), TEXT("Large, armored leader of the hogs. Rams walls with double force.") },
		{ TEXT("Hog"), TEXT("Hog"), TEXT("Desc_HogBasic"), TEXT("Stürmt geradeaus auf die Mauern zu und rammt sie. Die Panzerhaut lässt Kugeln teilweise abprallen."),
			TEXT("Fluffy-tailed Hog"), TEXT("Charges straight at the walls and rams them. Its armored hide deflects some bullets.") },
		{ TEXT("SpitterAquaticAlpha"), TEXT("Wasser-Alpha"), TEXT("Desc_SpitterAquatic_Alpha"), TEXT("Großer Spucker aus dem Wasser. Schießt aus der Ferne, verträgt Feuer gut, Strom aber schlecht."),
			TEXT("Aquatic Alpha"), TEXT("Large spitter from the water. Attacks from range, resists fire well, but not power.") },
		{ TEXT("SpitterAquatic"), TEXT("Wasserspucker"), TEXT("Desc_SpitterAquatic_Small"), TEXT("Spucker aus dem Wasser: hält Feuer aus, Strom trifft ihn hart."),
			TEXT("Aquatic Spitter"), TEXT("Spitter from the water: withstands fire, power hits it hard.") },
		{ TEXT("SpitterDesertAlpha"), TEXT("Wüsten-Alpha"), TEXT("Desc_SpitterDesert_Alpha"), TEXT("Großer Spucker aus der Wüste. Hitze macht ihm nichts, Kugeln schon."),
			TEXT("Desert Alpha"), TEXT("Large spitter from the desert. Heat doesn't bother it, bullets do.") },
		{ TEXT("SpitterDesert"), TEXT("Wüstenspucker"), TEXT("Desc_SpitterDesert_Small"), TEXT("Spucker aus der Wüste: Flammen wirken kaum, Gewehr und Bolzen dafür gut."),
			TEXT("Desert Spitter"), TEXT("Spitter from the desert: flames barely work, rifles and bolts work well.") },
		{ TEXT("SpitterForestRedAlpha"), TEXT("Roter Alpha"), TEXT("Desc_SpitterForest_Red_Alpha"), TEXT("Rote Variante des Alpha-Spuckers: zäher und angriffslustiger."),
			TEXT("Red Alpha"), TEXT("Red variant of the Alpha Plasma Spitter: tougher and more aggressive.") },
		{ TEXT("SpitterForestRed"), TEXT("Roter Spucker"), TEXT("Desc_SpitterForest_Small_Red"), TEXT("Rote Variante des Spuckers: etwas zäher, spuckt aus der Ferne."),
			TEXT("Red Spitter"), TEXT("Red variant of the Plasma Spitter: a bit tougher, spits from range.") },
		{ TEXT("SpitterForestAlpha"), TEXT("Alpha-Spucker"), TEXT("Desc_SpitterForest_Alpha"), TEXT("Bleibt auf Abstand und beschießt Mauern und Türme aus bis zu 18 m mit Säure."),
			TEXT("Alpha Plasma Spitter"), TEXT("Keeps its distance and shoots acid at walls and turrets from up to 18 m.") },
		{ TEXT("Spitter"), TEXT("Spucker"), TEXT("Desc_SpitterForest_Small"), TEXT("Kleiner Fernkämpfer: hält Abstand und spuckt Säure auf Mauern und Türme."),
			TEXT("Plasma Spitter"), TEXT("Small ranged attacker: keeps its distance and spits acid at walls and turrets.") },
		{ TEXT("Stinger_Child"), TEXT("Baby-Stinger"), TEXT("Desc_StingerSmall"), TEXT("Klein, flink und zahlreich. Kommt im Schwarm und ist schnell an der Mauer."),
			TEXT("Baby Stinger"), TEXT("Small, quick and numerous. Comes in swarms and reaches the wall fast.") },
		{ TEXT("EliteStinger"), TEXT("Elite-Stinger"), TEXT("Desc_StingerElite"), TEXT("Großer, zäher Stinger. Langsamer als seine Brut, aber mit kräftigen Stichen."),
			TEXT("Elite Stinger"), TEXT("Large, tough stinger. Slower than its brood, but stings hard.") },
		{ TEXT("Stinger"), TEXT("Stinger"), TEXT("Desc_StingerAlpha"), TEXT("Springt in großen Sätzen auf die Mauern zu und ist schnell durch das Turmfeuer."),
			TEXT("Stinger"), TEXT("Leaps at the walls in big jumps and gets through turret fire quickly.") },
	};
	FTDKreaturSteckbrief B;
	const FString Klasse = FPaths::GetExtension(Pfad).IsEmpty() ? Pfad : FPaths::GetExtension(Pfad);
	const bool bEnglisch = TDSprache::Englisch();
	for (const FEintrag& E : Tabelle)
	{
		if (Klasse.Contains(E.Teil))
		{
			B.Name = bEnglisch ? E.NameEn : E.Name;
			B.Deskriptor = FString::Printf(TEXT("/Game/FactoryGame/Character/Creature/CreatureDescriptors/%s.%s_C"), E.Desc, E.Desc);
			B.Text = bEnglisch ? E.TextEn : E.Text;
			break;
		}
	}
	if (B.Name.IsEmpty())
	{
		B.Name = Klasse.Replace(TEXT("Char_"), TEXT("")).Replace(TEXT("_C"), TEXT(""));
	}
	for (const FTDKatalogEintrag& E : KernKatalog)
	{
		if (Pfad == E.Pfad)
		{
			B.Punkte = E.Punkte;
			B.Rolle = E.Rolle;
		}
	}
	// Kampfwerte aus dem Gegnerhirn (Balance-Datei), Wirkung je Turm aus den Resistenzen
	if (UClass* K = LoadClass<AFGCreature>(nullptr, *Pfad))
	{
		const FTDGegnerWerte W = FTDGegnerWerte::Fuer(K);
		B.Tempo = W.Tempo / 100.f;
		B.Reichweite = W.Reichweite / 100.f;
		B.Schaden = W.Schaden;
		if (const AFGCreature* Vorlage = K->GetDefaultObject<AFGCreature>())
		{
			const TPair<ETDSchadensart, FString> Arten[] = {
				{ ETDSchadensart::Durchschlag, TDT(TEXT("Bolzen"), TEXT("Bolts")) }, { ETDSchadensart::Kugel, TDT(TEXT("Gewehr"), TEXT("Rifle")) },
				{ ETDSchadensart::Feuer, TDT(TEXT("Flammen"), TEXT("Flames")) }, { ETDSchadensart::Strom, TDT(TEXT("Tesla"), TEXT("Tesla")) },
				{ ETDSchadensart::Explosion, TDT(TEXT("Nobelisk"), TEXT("Nobelisk")) } };
			for (const TPair<ETDSchadensart, FString>& A : Arten)
			{
				B.Wirkung.Add(TPair<FString, float>(A.Value, ATDTurretBase::Resistenz(A.Key, Vorlage)));
			}
		}
	}
	return B;
}

void UTDWaveSubsystem::SpawneKernWelle(int32 Nummer)
{
	const ATDDefenseCore* Kern = mKern.Get();
	if (!Kern)
	{
		return;
	}
	const FVector Mitte = Kern->GetActorLocation();
	EntschaerfeLogistik(Mitte);
	// Netz wird um den Kern erst nach und nach erzeugt: vor Welle 1 ungepruefte Fronten noch einmal suchen
	if (Nummer == 1 && mGueltigeRichtungen == 0)
	{
		SucheFronten(Mitte);
		if (mGueltigeRichtungen > 0)
		{
			Ansage(TDF(TEXT("Fronten neu bestimmt (%d Richtungen begehbar): /td kern fronten"), TEXT("Fronts redetermined (%d directions passable): /td kern fronten"), mGueltigeRichtungen));
			SpeichereLauf();
			MarkiereFronten();
		}
	}
	// Lebenspunkte wachsen mit der Welle; die Zusammensetzung steht im Wellenplan (T3, in der Bauphase festgelegt)
	const FTDStufe& S = GetStufe(mStufe);
	mElites = 0;
	// 0.9.1 (Simulation 09.11.): ab W31 waechst das Leben schneller, Phase 4-5 hatte im Modell zu viel Luft. Im Spiel nachmessen.
	const float JeWelle = TDBalance::Wert(TEXT("Wellen.LPJeWelle"), 0.08f);
	const float LPFaktor = S.LP * (1.f + JeWelle * FMath::Min(Nummer - 1, 30) + TDBalance::Wert(TEXT("Wellen.LPJeWelleAb31"), 0.1f) * FMath::Max(Nummer - 31, 0));
	if (mPlanWelle != Nummer)
	{
		PlaneWelle(Nummer);
	}
	const int32 Art = mPlanArt;

	// Feste Fronten des Laufs; die Stufe bestimmt, ab welcher Welle weitere dazukommen
	if (mFronten.Num() == 0)
	{
		SucheFronten(Mitte);
	}
	// 0.9.0 (Pruefung0846: alle 52 versperrten Nachzuegler 30 m hinter Front 2, deren Weg bei der Wahl voll war): das Netz aendert
	// sich nach der Wahl (Gelaende und Bauten werden nachgeladen). Vor jeder Welle den Weg der aktiven Fronten neu pruefen und
	// bei einem Teilweg die Fronten neu bestimmen.
	for (int32 i = 0; i < AktiveFrontenFuer(Nummer) && mFronten.IsValidIndex(i); ++i)
	{
		bool bVoll = false;
		const TArray<FVector> Probe = UTDGegnerHirn::BerechneWeg(GetWorld(), mFronten[i].Ort, mKern.Get(), &bVoll);
		if (!bVoll)
		{
			UE_LOG(LogFicsitTD, Warning, TEXT("Frontpruefung Welle %d: Front %d (%s) hat nur noch einen Teilweg (%d Punkte, Ende %s) - Fronten neu bestimmen"),
				Nummer, i + 1, *FrontText(mFronten, i, true), Probe.Num(), Probe.Num() ? *Probe.Last().ToCompactString() : TEXT("-"));
			mFronten.Reset();
			SucheFronten(Mitte);
			MarkiereFronten();
			break;
		}
	}
	TArray<FVector> Orte;
	mRichtungen.Reset();
	float Abstand = 0.f;
	for (int32 i = 0; i < AktiveFrontenFuer(Nummer) && mFronten.IsValidIndex(i); ++i)
	{
		Orte.Add(mFronten[i].Ort);
		mRichtungen += (i ? TEXT(", ") : TEXT("")) + Himmelsrichtung(mFronten[i].Winkel);
		Abstand += FVector::Dist2D(mFronten[i].Ort, Mitte) / FMath::Max(1, AktiveFrontenFuer(Nummer));
	}
	if (Orte.Num() == 0)
	{
		// nichts gefunden (kein Boden?): zufaellige Richtung wie vor 0.16
		const float W = FMath::FRandRange(0.f, 360.f);
		Abstand = S.MaxAbstand;
		Orte.Add(Mitte + FRotator(0.f, W, 0.f).Vector() * Abstand);
		mRichtungen = Himmelsrichtung(W);
	}

	// T5 Stroeme: der Plan wird zur Warteschlange. Normale Gegner gemischt und reihum auf die Fronten verteilt, neue Arten frueh
	// an der aeltesten Front, Bosse in der Mitte der Welle. LaufTick schickt sie nach und nach los, hoechstens Wellen.MaxGleichzeitig.
	FString Boss;
	EntferneBossBalken();
	mNachschub.Reset();
	mNachschubIndex = 0;
	mNachschubTakt = 0.f;
	mNachschubMitte = Mitte;
	mNachschubOrte = Orte;
	mNachschubWege.Reset();
	for (const FVector& O : Orte)
	{
		mNachschubWege.Add(UTDGegnerHirn::BerechneWeg(GetWorld(), O, mKern.Get()));
	}
	// 0.78.6 (Pruefstand 06.11.: 44 von 101 Festgefahrenen standen an Pflanzen, die das Navigationsnetz nicht kennt): faellbaren
	// Bewuchs entlang der Frontwege raeumen, wie beim Kernplatz und der Kettensaege. Wellen.WegRaeumen = halbe Breite in m, 0 = aus
	if (const float Halb = TDBalance::Wert(TEXT("Wellen.WegRaeumen"), 3.f) * 100.f; Halb > 0.f)
	{
		int32 Geraeumt = 0;
		TSet<FString> Uebrig;
		for (const TArray<FVector>& Weg : mNachschubWege)
		{
			for (int32 i = 1; i < Weg.Num(); ++i)
			{
				const int32 Schritte = FMath::Max(1, FMath::CeilToInt(FVector::Dist(Weg[i - 1], Weg[i]) / Halb));
				for (int32 k = 0; k < Schritte; ++k)
				{
					Geraeumt += TDRaeumeBewuchs(GetWorld(), FMath::Lerp(Weg[i - 1], Weg[i], float(k) / Schritte), Halb, true, &Uebrig);
				}
			}
		}
		if (Geraeumt > 0)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Frontwege: %d Pflanzen im %.0f-m-Streifen geraeumt"), Geraeumt, 2.f * Halb / 100.f);
		}
		if (Uebrig.Num() > 0)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Frontwege: nicht faellbar, bleibt stehen: %s"), *FString::Join(Uebrig.Array(), TEXT(", ")));
		}
	}
	TArray<FTDNachschub> Normal, Neue, Bosse;
	int32 Reihum = 0;
	int32 PlanElite = 0;
	for (const FTDPlanGruppe& P : mPlan)
	{
		UClass* Klasse = LoadClass<AFGCreature>(nullptr, *P.Pfad);
		if (!Klasse || P.Anzahl <= 0)
		{
			continue;
		}
		if (P.bBoss)
		{
			Boss = TDF(TEXT(" BOSS: %s!"), TEXT(" BOSS: %s!"), *P.BossName);
			mBossName = P.BossName;
		}
		for (int32 i = 0; i < P.Anzahl; ++i)
		{
			FTDNachschub N;
			N.Klasse = Klasse;
			N.LP = LPFaktor * (P.bBoss ? P.BossLP : 1.f);
			N.bElite = i < P.Elite;
			N.bBoss = P.bBoss;
			N.Front = P.bBoss || P.bNeu ? 0 : Reihum++ % Orte.Num();
			PlanElite += N.bElite ? 1 : 0;
			(P.bBoss ? Bosse : P.bNeu ? Neue : Normal).Add(N);
		}
	}
	FRandomStream Mischen(static_cast<int32>(HashCombine(GetTypeHash(Nummer), GetTypeHash(mStufe))));
	for (int32 i = Normal.Num() - 1; i > 0; --i)
	{
		Normal.Swap(i, Mischen.RandRange(0, i));
	}
	for (int32 k = 0; k < Neue.Num(); ++k)
	{
		Normal.Insert(Neue[k], FMath::Min(Normal.Num(), 3 + 4 * k));
	}
	const int32 BossStelle = Normal.Num() / 2;
	for (int32 k = 0; k < Bosse.Num(); ++k)
	{
		Normal.Insert(Bosse[k], FMath::Min(Normal.Num(), BossStelle + k));
	}
	mNachschub = MoveTemp(Normal);
	ZeigeWellenLeiste();
	const int32 Gespawnt = mNachschub.Num();
	mWellenGroesse = Gespawnt;
	mElites = PlanElite;
	const int32 Gleichzeitig = TDBalance::Ganz(TEXT("Wellen.MaxGleichzeitig"), TDBalance::Ganz(TEXT("Wellen.MaxGegner"), KernMaxGegner));
	if (Gespawnt > Gleichzeitig)
	{
		Boss += TDF(TEXT(" Sie kommen in Schueben, hoechstens %d gleichzeitig."), TEXT(" They come in batches, at most %d at once."), Gleichzeitig);
	}
	// T3: neue Arten in der Ansage nennen (Steckbrief in der Wellenvorschau am Kern)
	FString Neu;
	for (const FTDPlanGruppe& P : mPlan)
	{
		if (P.bNeu)
		{
			Neu += (Neu.IsEmpty() ? TDT(TEXT(" NEU: "), TEXT(" NEW: ")) : FString(TEXT(", "))) + Steckbrief(P.Pfad).Name;
		}
	}
	Boss += Neu;
	if (Nummer % 10 == 1)
	{
		const int32 Ph = PhaseVon(Nummer);
		if (Ph < KernPhasenAnzahl)
		{
			Ansage(FString::Printf(TEXT("PHASE %d: %s - %s"), Ph + 1, *PhasenName(Ph), *PhasenBeschreibung(Ph)));
		}
		else if (Nummer == 51 || Ph == KernPhasenAnzahl)
		{
			Ansage(TDT(TEXT("ENDLOS: Alle Arten im Mix, alle 10 Wellen ein Boss. Wie weit kommst du?"), TEXT("ENDLESS: All species mixed, a boss every 10 waves. How far can you go?")));
		}
	}
	mWellenZeit = 0.f;
	mZeitBisSteuerung = 0.f;
	const FString GesamtText = mWellenGesamt > 0 ? FString::Printf(TEXT("/%d"), mWellenGesamt) : FString();
	if (S.bZusammensetzung)
	{
		const FString ArtText = WellenArtName(Art);
		const FString EliteText = mElites > 0 ? TDF(TEXT(", davon %d Elite"), TEXT(", %d of them Elite"), mElites) : FString();
		const FString TippText = TDT(TEXT("Tipp: "), TEXT("Tip: ")) + TippFuer(Nummer);
		Ansage(TDF(TEXT("WELLE %d%s (%s): %d Gegner%s aus %s, %.0f m entfernt.%s %s"), TEXT("WAVE %d%s (%s): %d enemies%s from %s, %.0f m away.%s %s"), Nummer,
			*GesamtText, *ArtText, Gespawnt, *EliteText, *mRichtungen, Abstand / 100.f, *Boss, *TippText));
	}
	else
	{
		// Hart/Albtraum: nur Richtung und Anzahl
		Ansage(TDF(TEXT("WELLE %d%s: %d Gegner aus %s.%s"), TEXT("WAVE %d%s: %d enemies from %s.%s"), Nummer, *GesamtText, Gespawnt, *mRichtungen, *Boss));
	}
}

void UTDWaveSubsystem::KeinLoot(AFGCreature* Kreatur)
{
	// mItemToDrop ist geschuetzt: ueber die Reflexion leeren
	static FClassProperty* Beute = FindFProperty<FClassProperty>(AFGCreature::StaticClass(), TEXT("mItemToDrop"));
	if (Kreatur && Beute)
	{
		Beute->SetObjectPropertyValue_InContainer(Kreatur, nullptr);
	}
}

int32 UTDWaveSubsystem::RaeumeLoot(UWorld* World, const FVector& Mitte, float Radius)
{
	int32 Anzahl = 0;
	for (TActorIterator<AFGItemPickup> It(World); It; ++It)
	{
		const UClass* Teil = It->GetPickupItemClass();
		const FString Name = Teil ? Teil->GetName() : FString();
		if (Name.EndsWith(TEXT("Parts_C")) && (Name.Contains(TEXT("Hog")) || Name.Contains(TEXT("Spitter")) || Name.Contains(TEXT("Stinger")) || Name.Contains(TEXT("Hatcher")))
			&& FVector::DistSquared(It->GetActorLocation(), Mitte) < FMath::Square(Radius))
		{
			It->Destroy();
			++Anzahl;
		}
	}
	return Anzahl;
}

void UTDWaveSubsystem::WelleAusgewertet(float Anteil, bool bVerloren)
{
	if (const ATDDefenseCore* K = mKern.Get())
	{
		const int32 Weg = RaeumeLoot(GetWorld(), K->GetActorLocation(), 50000.f);
		if (Weg > 0)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Loot: %d liegengebliebene Kreaturteile weggeraeumt"), Weg);
		}
	}
	// Abschuesse je Turm in dieser Welle, beste drei nennen
	TArray<TPair<FString, int32>> Bilanz;
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		const int32* Vorher = mKillsBeiStart.Find(*It);
		const int32 Neu = It->GetKills() - (Vorher ? *Vorher : 0);
		if (Neu > 0)
		{
			Bilanz.Add({ It->GetTurmAnzeige(), Neu });
		}
	}
	Bilanz.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B) { return A.Value > B.Value; });
	// G2: Trefferbilanz aller Tuerme dieser Welle ins Log
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Bilanz Welle %d: %s"), mWelle, *It->BilanzText(mWellenZeit));
	}
	FString Beste;
	for (int32 i = 0; i < FMath::Min(3, Bilanz.Num()); ++i)
	{
		Beste += FString::Printf(TEXT("%s%s %d"), i ? TEXT(", ") : TEXT(""), *Bilanz[i].Key, Bilanz[i].Value);
	}
	const FString BesteText = Beste.IsEmpty() ? FString() : TDF(TEXT(" Beste Tuerme: %s."), TEXT(" Best turrets: %s."), *Beste);
	if (bVerloren)
	{
		MeldeMetrik(Anteil);
		return;
	}
	Ansage(TDF(TEXT("Welle %d abgewehrt: %d Gegner in %.0f s, Kern bei %.0f %%.%s"), TEXT("Wave %d repelled: %d enemies in %.0f s, Core at %.0f %%.%s"), mWelle, mWellenGroesse, mWellenZeit, Anteil,
		*BesteText));
	MeldeMetrik(Anteil);
}

void UTDWaveSubsystem::MesseWellenStart()
{
	if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this))
	{
		Lauf->WellenStart(mWelle);
	}
	if (UTDWellenStatistik* Statistik = UTDWellenStatistik::Get(this))
	{
		Statistik->WellenStart(mWelle);
	}
	const ATDDefenseCore* Kern = mKern.Get();
	if (!Kern)
	{
		return;
	}
	// Abwehrkapazitaet K: Nenn-DPS aller intakten Tuerme mit Rezept (Munition egal - Mangel zeigt die Auswertung)
	TArray<TPair<ETDSchadensart, float>> Tuerme;
	mMessK = 0.f;
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		const float D = It->IsTDIntact() && It->GetCurrentRecipe() ? It->NennDPS() : 0.f;
		if (D > 0.f)
		{
			Tuerme.Add({ It->GetSchadensart(), D });
			mMessK += D;
		}
	}
	mMessTuerme = Tuerme.Num();
	// Gegner-EHP: Lebenspunkte geteilt durch den DPS-gewichteten Resistenz-Faktor der vorhandenen Tuerme
	mMessLP = 0.f;
	mMessEHP = 0.f;
	for (const FTDWellenGegner& G : mGegner)
	{
		AFGCreature* Z = G.Kreatur.Get();
		const UFGHealthComponent* Leben = Z && G.bKern ? Z->GetHealthComponent() : nullptr;
		if (!Leben)
		{
			continue;
		}
		float Faktor = 1.f;
		if (mMessK > 0.f)
		{
			float Summe = 0.f;
			for (const TPair<ETDSchadensart, float>& T : Tuerme)
			{
				Summe += T.Value * ATDTurretBase::Resistenz(T.Key, Z);
			}
			Faktor = FMath::Max(Summe / mMessK, 0.05f);
		}
		mMessLP += Leben->GetMaxHealth();
		mMessEHP += Leben->GetMaxHealth() / Faktor;
	}
	// T5: noch ausstehender Nachschub zaehlt mit (Lebenspunkte aus der Klassenvorlage x LP-Faktor)
	for (int32 i = mNachschubIndex; i < mNachschub.Num(); ++i)
	{
		const FTDNachschub& N = mNachschub[i];
		const AFGCreature* Vorlage = N.Klasse ? N.Klasse->GetDefaultObject<AFGCreature>() : nullptr;
		const UFGHealthComponent* Leben = Vorlage ? Vorlage->GetHealthComponent() : nullptr;
		if (!Leben)
		{
			continue;
		}
		const float LP = Leben->GetMaxHealth() * N.LP * (N.bElite ? TDBalance::Wert(TEXT("Wellen.EliteLP"), 2.5f) : 1.f);
		float Faktor = 1.f;
		if (mMessK > 0.f)
		{
			float Summe = 0.f;
			for (const TPair<ETDSchadensart, float>& T : Tuerme)
			{
				Summe += T.Value * ATDTurretBase::Resistenz(T.Key, Vorlage);
			}
			Faktor = FMath::Max(Summe / mMessK, 0.05f);
		}
		mMessLP += LP;
		mMessEHP += LP / Faktor;
	}
	const float Referenz = TDBalance::Wert(TEXT("Metrik.Referenzzeit"), 60.f);
	mMessQ = mMessK > 0.f ? mMessEHP / (mMessK * Referenz) : 0.f;
	mMessKernStart = Kern->GetTDHealth();
	mMessEP = Kern->mEinsatzPunkte;
	mMessNaechste = BIG_NUMBER;
	mMessGueltig = true;
	StarteBildMessung();
	MeldeKreaturen(FString::Printf(TEXT("Welle %d Start"), mWelle));
	UE_LOG(LogFicsitTD, Log, TEXT("Metrik Welle %d Start: Q %.2f (EHP %.0f, LP %.0f, K %.0f DPS aus %d Tuermen, Referenz %.0f s), %d Gegner, %d Elite, EP ungenutzt %d"),
		mWelle, mMessQ, mMessEHP, mMessLP, mMessK, mMessTuerme, Referenz, mWellenGroesse, mElites, mMessEP);
}

void UTDWaveSubsystem::MeldeMetrik(float Anteil)
{
	const ATDDefenseCore* Kern = mKern.Get();
	if (!Kern || !mMessGueltig)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Metrik Welle %d: keine Startwerte (Welle nach dem Laden fortgesetzt)"), mWelle);
		return;
	}
	mMessGueltig = false;
	float Schaden = 0.f, Overkill = 0.f, Uptime = 0.f, Hungrig = 0.f, OhneStrom = 0.f;
	int32 Tuerme = 0, Neben = 0, Treffer = 0;
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		const ATDTurretBase::FBilanz& B = It->GetBilanz();
		Schaden += B.Schaden;
		Overkill += B.Overkill;
		Treffer += B.Treffer;
		Neben += B.Nebentreffer;
		Hungrig += B.ZeitHungrig;
		OhneStrom += B.ZeitOhneStrom;
		Uptime += B.Messzeit > 0.f ? B.ZeitMitZiel / B.Messzeit : 0.f;
		++Tuerme;
	}
	const float Auslastung = mMessK > 0.f && mWellenZeit > 0.f ? Schaden / (mMessK * mWellenZeit) : 0.f;
	const float Verlust = 100.f * FMath::Max(0.f, mMessKernStart - Kern->GetTDHealth()) / FMath::Max(Kern->GetTDMaxHealth(), 1.f);
	const float Naechste = mMessNaechste < BIG_NUMBER ? mMessNaechste / 100.f : -1.f;
	const float UptimeMittel = Tuerme ? 100.f * Uptime / Tuerme : 0.f;
	const float OverkillAnteil = Schaden > 0.f ? 100.f * Overkill / Schaden : 0.f;
	const float NebenAnteil = Treffer ? 100.f * Neben / Treffer : 0.f;
	const FTDBildWerte Bild = BildWerte();
	mBildMessung = false;
	MeldeZeitleiste(FString::Printf(TEXT("Welle %d"), mWelle));
	UE_LOG(LogFicsitTD, Log, TEXT("Leistung Welle %d: %d Bilder, Bildzeit Mittel %.1f ms, 95 %% unter %.1f ms, max %.1f ms, %d Bilder ueber 50 ms; eigener Code je Bild: Gegnerhirn %.2f ms, Tuerme %.2f ms; max %d Gegner gleichzeitig, %d Tode; Treffer (Spiel) %.2f ms, Geschosse %.2f ms (%d Stueck); Engine je Bild: Spiel-Thread %.1f ms (max %.0f), Render-Thread %.1f ms (max %.0f), Grafikkarte %.1f ms (max %.0f)"),
		mWelle, Bild.Bilder, Bild.Mittel, Bild.P95, Bild.Max, Bild.Ruckler, Bild.HirnMs, Bild.TurmMs, Bild.MaxLebend, Bild.Tote, Bild.SchadenMs, Bild.GeschossMs, Bild.Geschosse,
		Bild.SpielMs, Bild.SpielMax, Bild.RenderMs, Bild.RenderMax, Bild.GpuMs, Bild.GpuMax);
	UE_LOG(LogFicsitTD, Log, TEXT("Metrik Welle %d: Q %.2f, Dauer %.0f s, Kernverlust %.1f %% (Kern %.0f %%), knappste Annaeherung %.1f m, Auslastung %.0f %%, ")
		TEXT("Schaden %.0f (Overkill %.0f %%, Nebenziele %.0f %%), Uptime %.0f %% (Mittel %d Tuerme), Munitionsmangel mit Ziel %.0f s, ohne Strom %.0f s (Turm-Sekunden), EP ungenutzt zu Beginn %d"),
		mWelle, mMessQ, mWellenZeit, Verlust, Anteil, Naechste, 100.f * Auslastung, Schaden, OverkillAnteil, NebenAnteil, UptimeMittel, Tuerme, Hungrig, OhneStrom, mMessEP);

	// CSV fuer die Auswertung ausserhalb des Spiels (Simulator, Tabellen)
	const FString Datei = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FicsitTD"), TEXT("Metriken.csv"));
	FString Zeile;
	if (!IFileManager::Get().FileExists(*Datei))
	{
		Zeile = TEXT("Zeit;Stufe;Welle;Gegner;Elite;LP;EHP;K_DPS;Tuerme;Q;Dauer_s;Kernverlust_Proz;Kern_Proz;Naechste_m;Auslastung_Proz;Schaden;Overkill_Proz;Nebenziele_Proz;Uptime_Proz;Hungrig_s;OhneStrom_s;EP_ungenutzt;Bild_ms;Bild_p95_ms;Bild_max_ms;Ruckler_50ms;Hirn_ms;Turm_ms;Lebend_max\n");
	}
	Zeile += FString::Printf(TEXT("%s;%d;%d;%d;%d;%.0f;%.0f;%.1f;%d;%.3f;%.1f;%.2f;%.1f;%.1f;%.1f;%.0f;%.1f;%.1f;%.1f;%.1f;%.1f;%d;%.2f;%.2f;%.1f;%d;%.3f;%.3f;%d\n"),
		*FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")), mStufe, mWelle, mWellenGroesse, mElites, mMessLP, mMessEHP, mMessK, mMessTuerme, mMessQ,
		mWellenZeit, Verlust, Anteil, Naechste, 100.f * Auslastung, Schaden, OverkillAnteil, NebenAnteil, UptimeMittel, Hungrig, OhneStrom, mMessEP,
		Bild.Mittel, Bild.P95, Bild.Max, Bild.Ruckler, Bild.HirnMs, Bild.TurmMs, Bild.MaxLebend);
	FFileHelper::SaveStringToFile(Zeile, *Datei, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

void UTDWaveSubsystem::LaufTick(float DeltaTime)
{
	if (mLauf == ETDLauf::Aus)
	{
		return;
	}
	if (!mKern.IsValid())
	{
		StoppeLauf(TDT(TEXT("Kern-Verteidigung beendet: der Abwehrkern wurde abgebaut."), TEXT("Core defense ended: the Defense Core was dismantled.")));
		return;
	}
	if (mLauf == ETDLauf::Aufbau)
	{
		// Gegnerklassen der naechsten Welle einmal vorladen (Test 0.31: erster Spawn einer Art ruckelte durch Nachladen)
		if (mVorgeladenFuer != mWelle + 1)
		{
			mVorgeladenFuer = mWelle + 1;
			PlaneWelle(mWelle + 1);
			const double Start = FPlatformTime::Seconds();
			int32 Neu = 0;
			for (const FTDKatalogEintrag& E : KernKatalog)
			{
				if (AbWelleVon(E) <= mWelle + 1)
				{
					UClass* K = LoadClass<AFGCreature>(nullptr, E.Pfad);
					if (K && !mVorgeladen.Contains(K))
					{
						mVorgeladen.Add(K);
						++Neu;
					}
				}
			}
			if ((mWelle + 1) % 10 == 0)
			{
				UClass* K = LoadClass<AFGCreature>(nullptr, KernPhasen[((mWelle + 1) / 10 - 1) % KernPhasenAnzahl].BossPfad);
				if (K && !mVorgeladen.Contains(K))
				{
					mVorgeladen.Add(K);
					++Neu;
				}
			}
			if (Neu > 0)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("Vorgeladen fuer Welle %d: %d neue Gegnerklassen in %.0f ms"), mWelle + 1, Neu, (FPlatformTime::Seconds() - Start) * 1000.0);
			}
		}
		const float Vorher = mCountdown;
		mCountdown -= mManuell ? 0.f : DeltaTime;   // Einsatz: Countdown steht, /td kern weiter setzt ihn auf 0
		if (FMath::FloorToInt(Vorher / 10.f) != FMath::FloorToInt(mCountdown / 10.f))
		{
			SpeichereLauf();   // alle 10 s reicht fuer den Spielstand
		}
		for (const float Marke : { 600.f, 300.f, 120.f, 60.f, 30.f, 10.f })
		{
			if (Vorher > Marke && mCountdown <= Marke)
			{
				Ansage(Marke >= 120.f ? TDF(TEXT("Welle %d in %.0f Minuten."), TEXT("Wave %d in %.0f minutes."), mWelle + 1, Marke / 60.f)
					: TDF(TEXT("Welle %d in %.0f Sekunden."), TEXT("Wave %d in %.0f seconds."), mWelle + 1, Marke));
				if (Marke == 60.f)
				{
					PruefeVersorgung();
				}
			}
		}
		if (mCountdown <= 0.f)
		{
			// 0.9.6 (Gesamtreview 12.11.): im Einsatz (manuell) lief der Versorgungs-Check nie - er hing an der 60-s-Marke des Countdowns
			if (mManuell)
			{
				PruefeVersorgung();
			}
			if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this))
			{
				SpeichereLauf();
				Lauf->SpeichereSpielstand(FString::Printf(TEXT("FicsitTD_W%02d_Start"), mWelle + 1));
			}
			++mWelle;
			mLauf = ETDLauf::Welle;
			mCountdown = 0.f;
			mKillsBeiStart.Reset();
			for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
			{
				mKillsBeiStart.Add(*It, It->GetKills());
				It->StarteBilanz();
			}
			// Einsatz: zu Beginn jeder Welle alle Verteidigungsanlagen auf 100 % (kein Reparieren waehrend der Welle)
			if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
			{
				Einsatz->HeileVerteidigung();
			}
			SpawneKernWelle(mWelle);
			MesseWellenStart();
			SpeichereLauf();
		}
		return;
	}
	// Welle: vorbei, wenn alle Kern-Gegner tot sind
	mWellenZeit += DeltaTime;
	int32 Lebend = 0;
	const FVector KernOrt = mKern->GetActorLocation();
	for (int32 i = 0; i < mGegner.Num(); ++i)
	{
		FTDWellenGegner& G = mGegner[i];
		if (G.bKern && G.Kreatur.IsValid() && G.Kreatur->IsAliveAndWell())
		{
			++Lebend;
			const float Abstand = FVector::Dist2D(G.Kreatur->GetActorLocation(), KernOrt);
			mMessNaechste = FMath::Min(mMessNaechste, Abstand);
			PruefeNachzuegler(G, Abstand, DeltaTime);
		}
	}
	// T5: Nachschub - alle NachschubTakt s bis zu NachschubPaket Gegner, solange weniger als MaxGleichzeitig leben
	if (mNachschubIndex < mNachschub.Num())
	{
		mNachschubTakt -= DeltaTime;
		if (mNachschubTakt <= 0.f)
		{
			mNachschubTakt = TDBalance::Wert(TEXT("Wellen.NachschubTakt"), 0.25f);
			const int32 Gleichzeitig = TDBalance::Ganz(TEXT("Wellen.MaxGleichzeitig"), TDBalance::Ganz(TEXT("Wellen.MaxGegner"), KernMaxGegner));
			int32 Frei = FMath::Min(Gleichzeitig - Lebend, TDBalance::Ganz(TEXT("Wellen.NachschubPaket"), 5));
			while (Frei-- > 0 && mNachschubIndex < mNachschub.Num())
			{
				const FTDNachschub& N = mNachschub[mNachschubIndex++];
				const int32 F = mNachschubOrte.IsValidIndex(N.Front) ? N.Front : 0;
				if (mNachschubOrte.IsValidIndex(F))
				{
					SpawneGruppe(N.Klasse, 1, mNachschubMitte, mNachschubOrte[F], N.LP, N.bElite ? 1 : 0, N.bBoss,
						mNachschubWege.IsValidIndex(F) ? &mNachschubWege[F] : nullptr);
				}
			}
		}
		return;
	}
	if (Lebend > 0)
	{
		return;
	}
	const ATDDefenseCore* Kern = mKern.Get();
	const float Anteil = 100.f * Kern->GetTDHealth() / FMath::Max(Kern->GetTDMaxHealth(), 1.f);
	EntferneWellenLeiste();
	WelleAusgewertet(Anteil);
	if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
	{
		Einsatz->WelleBeendet(mWelle, Anteil);
	}
	if (mWellenGesamt > 0 && mWelle >= mWellenGesamt)
	{
		mLauf = ETDLauf::Aus;
		SpeichereLauf();
		if (ATDDefenseCore* SiegKern = mKern.Get())
		{
			SiegKern->mLaufEnde = 1;
			SiegKern->mLaufEndWelle = mWelle;
		}
		MarkiereFronten();
		Ansage(TDF(TEXT("SIEG! Alle %d Wellen abgewehrt, Kern bei %.0f %%. Im Kern-Menü: neu starten oder Einsatz beenden."),
			TEXT("VICTORY! All %d waves repelled, Core at %.0f %%. In the Core menu: restart or end the operation."), mWellenGesamt, Anteil));
		return;
	}
	mLauf = ETDLauf::Aufbau;
	mCountdown = mZwischenCountdown;
	SpeichereLauf();
	if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this))
	{
		Lauf->SpeichereSpielstand(FString::Printf(TEXT("FicsitTD_W%02d_Ende"), mWelle));
	}
	Ansage(mManuell ? TDT(TEXT("Bauen, so lange du willst - naechste Welle mit /td kern weiter."), TEXT("Build as long as you like - next wave with /td kern weiter."))
		: TDF(TEXT("Naechste Welle in %.0f min (/td kern weiter = sofort)."), TEXT("Next wave in %.0f min (/td kern weiter = now)."), mCountdown / 60.f));
	const int32 Jetzt = AktiveFrontenFuer(mWelle);
	const int32 Danach = AktiveFrontenFuer(mWelle + 1);
	for (int32 i = Jetzt; i < Danach; ++i)
	{
		Ansage(TDF(TEXT("ACHTUNG: Ab Welle %d greifen sie auch aus %s an! Die neue Front ist auf Karte und Kompass markiert."), TEXT("WARNING: From wave %d they also attack from %s! The new front is marked on map and compass."), mWelle + 1, *FrontName(i)));
	}
	if ((mWelle + 1) % 10 == 0)
	{
		Ansage(TDF(TEXT("WARNUNG: Welle %d ist ein BOSSKAMPF (%s). Verteidigung bündeln - danach gibt es Extra-Coupons."), TEXT("WARNING: Wave %d is a BOSS FIGHT (%s). Concentrate your defense - extra Coupons afterwards."), mWelle + 1,
			*PhasenBossName(((mWelle + 1) / 10 - 1) % KernPhasenAnzahl)));
	}
	// Wellen-Progression: zur Mitte einer Phase die naechste Familie ankuendigen, damit man Fabrik und Forschung vorbereitet
	const int32 NaechstePhase = PhaseVon(mWelle + 1) + 1;
	if ((mWelle + 1) % 10 == 5 && NaechstePhase < KernPhasenAnzahl && (mWellenGesamt <= 0 || NaechstePhase * 10 < mWellenGesamt))
	{
		Ansage(PhasenVorwarnung(NaechstePhase));
	}
	MarkiereFronten();
}

void UTDWaveSubsystem::PruefeNachzuegler(FTDWellenGegner& G, float Abstand, float DeltaTime)
{
	// Test 0.56 W14: vier Gegner standen 3 min 250 m vor dem Kern (ohne Weg kein Versetzen im Hirn), die Welle endete nie
	// 0.84 (Nachweis0831: 363 Nachzuegler im W50-Lauf, fast alle langsame Spucker und Klippen-Hogs 140-190 m vor dem Kern): der
	// Fortschritt wurde als Luftlinie zum Kern gemessen. Die Frontwege machen Umwege (325 m Weg bei 250 m Abstand); wer laenger
	// als 25 s quer zum Kern laeuft, kam nicht naeher und wurde versetzt, obwohl er lief. Gemessen wird jetzt der Restweg auf
	// dem Weg des Hirns; nur ohne Weg die Luftlinie.
	const UTDGegnerHirn* WegHirn = UTDGegnerHirn::Von(G.Kreatur.Get());
	// 0.9.5 (Verlauf der Nachzuegler: ein 2-m-Teilweg setzte den Bestwert auf 2 m, danach lief der Gegner auf vollem Weg von
	// 200 auf 62 m und galt trotzdem als Nachzuegler): nur ein voller Weg misst den Restweg, sonst die Luftlinie
	const float Mass = WegHirn && WegHirn->IstWegVoll() && WegHirn->GetRestweg() > 0.f ? WegHirn->GetRestweg() : Abstand;
	if (G.BesterAbstand < 0.f || Mass < G.BesterAbstand - 500.f)
	{
		G.BesterAbstand = Mass;
		G.Still = 0.f;
		return;
	}
	G.Still += DeltaTime;
	if (G.Still < TDBalance::Wert(TEXT("Wellen.NachzueglerSekunden"), 25.f))
	{
		return;
	}
	G.Still = 0.f;
	AFGCreature* Kreatur = G.Kreatur.Get();
	UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(Kreatur);
	// wer am Kern oder an einer Mauer kaempft, haengt nicht fest
	// 0.83.1: wer im Stau hinter einem Wellengegner wartet, ebenfalls nicht (das Hirn versetzt ihn nach Gegner.StauGeduld selbst)
	if (Abstand < 4000.f || (Hirn && (Hirn->GetPhase() != ETDHirnPhase::Marsch || Hirn->StehtImStau())))
	{
		G.BesterAbstand = Mass;
		return;
	}
	const FVector Vorher = Kreatur->GetActorLocation();
	const FString HirnZustand = Hirn ? Hirn->Diagnose() : FString();
	const FString HirnVerlauf = Hirn ? Hirn->Verlauf() : FString();   // 0.9.5: Restweg und Neuberechnungen der letzten ~40 s
	if (++G.Rettungen > TDBalance::Ganz(TEXT("Wellen.NachzueglerVersuche"), 3))
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Nachzuegler: %s bei %s (%.0f m vor dem Kern) nach %d Versuchen entfernt"), *Kreatur->GetClass()->GetName(),
			*Vorher.ToCompactString(), Abstand / 100.f, G.Rettungen - 1);
		TDNetz::Melde(TDNetz::EArt::NachzueglerEntfernt);
		UGameplayStatics::ApplyDamage(Kreatur, 1.e7f, nullptr, nullptr, UDamageType::StaticClass());
		if (Kreatur->IsAliveAndWell())
		{
			Kreatur->Destroy();
		}
		return;
	}
	// Frontweg, der am naechsten an ihm vorbeifuehrt; darauf der erste Punkt, der 30 m naeher am Kern liegt
	const FVector KernOrt = mKern->GetActorLocation();
	const TArray<FVector>* Bester = nullptr;
	float BestAbstand = TNumericLimits<float>::Max();
	for (const TArray<FVector>& Weg : mNachschubWege)
	{
		for (const FVector& P : Weg)
		{
			const float D = FVector::DistSquared2D(P, Vorher);
			if (D < BestAbstand)
			{
				BestAbstand = D;
				Bester = &Weg;
			}
		}
	}
	FVector Ziel = KernOrt;
	bool bGefunden = false;
	if (Bester)
	{
		for (int32 i = 1; i < Bester->Num(); ++i)
		{
			const FVector& A = (*Bester)[i - 1];
			const FVector& E = (*Bester)[i];
			if (FVector::Dist2D(E, KernOrt) <= Abstand - 3000.f)
			{
				// zwischen A und E genau auf den Abstand legen
				const float DA = FVector::Dist2D(A, KernOrt), DE = FVector::Dist2D(E, KernOrt);
				const float T = DA > DE ? FMath::Clamp((DA - (Abstand - 3000.f)) / (DA - DE), 0.f, 1.f) : 1.f;
				Ziel = FMath::Lerp(A, E, T);
				bGefunden = true;
				break;
			}
		}
	}
	if (!bGefunden)
	{
		// kein Weg bekannt: 30 m geradewegs Richtung Kern, auf den Boden
		Ziel = Vorher + (KernOrt - Vorher).GetSafeNormal2D() * 3000.f;
		FHitResult Hit;
		FCollisionQueryParams Query(TEXT("FicsitTDNachzuegler"), false, Kreatur);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Ziel + FVector(0.f, 0.f, 5000.f), Ziel - FVector(0.f, 0.f, 20000.f), ECC_Visibility, Query))
		{
			Ziel = Hit.ImpactPoint;
		}
	}
	const bool bVersetzt = Kreatur->TeleportTo(Ziel + FVector(0.f, 0.f, 150.f), Kreatur->GetActorRotation());
	TDNetz::Melde(TDNetz::EArt::NachzueglerVersetzt);
	if (Hirn)
	{
		Hirn->Neustart(Kreatur, Bester ? *Bester : TArray<FVector>());
	}
	// 0.84: nach dem Versetzen neu messen (Restweg, sobald das Hirn seinen Fortschritt neu bestimmt hat; bis dahin Luftlinie)
	G.BesterAbstand = -1.f;
	// 0.84.1 Ursachensuche (Kombo084: 329 Nachzuegler im W50-Lauf, 440 davon 175-200 m vor dem Kern, gehaeuft am Startpunkt der Front
	// Suedosten): Zustand des Hirns vor dem Versetzen mitschreiben (Weg, Restweg, Tempo, Bewegungsart, Ziel)
	UE_LOG(LogFicsitTD, Log, TEXT("Nachzuegler: %s hing %.0f m vor dem Kern bei %s, Versuch %d: %s nach %s (%s) | Mass %.0f m, Hirn: %s | Verlauf: %s"), *Kreatur->GetClass()->GetName(),
		Abstand / 100.f, *Vorher.ToCompactString(), G.Rettungen, bVersetzt ? TEXT("versetzt") : TEXT("NICHT versetzt"), *Ziel.ToCompactString(),
		bGefunden ? TEXT("auf dem Frontweg") : TEXT("Richtung Kern"), Mass / 100.f, HirnZustand.IsEmpty() ? TEXT("keins") : *HirnZustand, *HirnVerlauf);
}

void UTDWaveSubsystem::EntschaerfeLogistik(const FVector& Mitte)
{
	mLogistik.Reset();
	++mLogistikVersion;
	int32 Komponenten = 0;
	for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
	{
		if (!UTDEinsatzSubsystem::IstLogistik(It->GetClass()) || FVector::DistSquared2D(It->GetActorLocation(), Mitte) > FMath::Square(50000.f))
		{
			continue;
		}
		mLogistik.Add(*It);
		TArray<UPrimitiveComponent*> Teile;
		It->GetComponents<UPrimitiveComponent>(Teile);
		for (UPrimitiveComponent* Teil : Teile)
		{
			if (Teil && Teil->CanEverAffectNavigation())
			{
				Teil->SetCanEverAffectNavigation(false);
				++Komponenten;
			}
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Logistik: %d Gebaeude im Umkreis kein Hindernis (%d Teile aus dem Navigationsnetz genommen)"), mLogistik.Num(), Komponenten);
	// 0.9.4 (Mono-Tesla bis W50, Kapselprobe der Festgefahrenen: 15x Erzvorkommen, 7x Steinknoten, 5x Krabbennest, 5x Nussbusch):
	// diese Weltobjekte halten die Gegner auf, das Netz fuehrt aber ueber sie hinweg (begehbare Huegel). Einmal je
	// Kernort im Umkreis Gegner.HindernisRadius (Standard 400 m) ins Netz nehmen, damit die Wege um sie herum fuehren.
	if (TDBalance::Wert(TEXT("Gegner.HindernisseInsNetz"), 1.f) > 0.f && !mHindernisMitte.Equals(Mitte, 100.f))
	{
		mHindernisMitte = Mitte;
		static const TCHAR* Namen[] = { TEXT("ResourceDeposit"), TEXT("NodeMeshActor"), TEXT("CrabHatcher"), TEXT("NutBush"), TEXT("BerryBush") };
		const float Radius = TDBalance::Wert(TEXT("Gegner.HindernisRadius"), 400.f) * 100.f;
		int32 Objekte = 0, Gefunden = 0;
		TMap<FString, FString> Uebersprungen;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (FVector::DistSquared2D(It->GetActorLocation(), Mitte) > FMath::Square(Radius) || (It->IsA<APawn>() && !It->GetClass()->GetName().Contains(TEXT("CrabHatcher"))))
			{
				continue;
			}
			const FString Klasse = It->GetClass()->GetName();
			bool bTreffer = false;
			for (const TCHAR* N : Namen)
			{
				bTreffer |= Klasse.Contains(N);
			}
			// 0.9.5 (Mono-Tesla bis W50: 8 Festgefahrene an Knochenboegen): StaticMeshActor mit BoneStructure-Modell ebenso sperren.
			// Felspfeiler (CliffPillar) ausprobiert und verworfen: 86 Pfeiler mehr gesperrt, Wege durch Nussbusch-Felder, 77 statt 11 Netze.
			if (!bTreffer && It->IsA<AStaticMeshActor>())
			{
				static const TCHAR* Modelle[] = { TEXT("BoneStructure") };
				TArray<UStaticMeshComponent*> Netze;
				It->GetComponents<UStaticMeshComponent>(Netze);
				for (const UStaticMeshComponent* Netz : Netze)
				{
					for (const TCHAR* M : Modelle)
					{
						bTreffer |= Netz && Netz->GetStaticMesh() && Netz->GetStaticMesh()->GetName().Contains(M);
					}
				}
			}
			if (!bTreffer)
			{
				continue;
			}
			TArray<UPrimitiveComponent*> Komp;
			It->GetComponents<UPrimitiveComponent>(Komp);
			++Gefunden;
			// 0.9.5 (Diagnose): die Modelle stehen schon im Netz, gelten dort aber als begehbare Huegel. Deshalb die blockierende
			// Grundflaeche mit NavArea_Null ausschneiden; Ausloese-Boxen ohne Pawn-Block vorher aus der Rechnung nehmen.
			bool bBlockt = false;
			for (UPrimitiveComponent* Teil : Komp)
			{
				if (!Teil)
				{
					continue;
				}
				if (Teil->IsCollisionEnabled() && Teil->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block && Teil->GetCollisionEnabled() != ECollisionEnabled::QueryOnly)
				{
					bBlockt = true;
				}
				else if (Teil->CanEverAffectNavigation())
				{
					Teil->SetCanEverAffectNavigation(false);
				}
			}
			if (!bBlockt)
			{
				// 0.9.5 Diagnose: welche Klassen werden mangels blockierender Kollision uebersprungen (und wie sind sie eingestellt)?
				FString& Eintrag = Uebersprungen.FindOrAdd(Klasse);
				if (Eintrag.IsEmpty())
				{
					for (const UPrimitiveComponent* Teil : Komp)
					{
						if (Teil)
						{
							Eintrag += FString::Printf(TEXT("%s(Kollision %d, Pawn %d, Profil %s) "), *Teil->GetName(), (int32)Teil->GetCollisionEnabled(),
								(int32)Teil->GetCollisionResponseToChannel(ECC_Pawn), *Teil->GetCollisionProfileName().ToString());
						}
					}
				}
				continue;
			}
			if (It->FindComponentByClass<UNavModifierComponent>())
			{
				continue;
			}
			UNavModifierComponent* Mod = NewObject<UNavModifierComponent>(*It, TEXT("TDHindernisNetz"));
			Mod->SetAreaClass(UNavArea_Null::StaticClass());
			Mod->RegisterComponent();
			++Objekte;
		}
		UE_LOG(LogFicsitTD, Log, TEXT("Hindernisse: %d Weltobjekte (Erz, Steinknoten, Nester, Buesche, Knochen) in %.0f m als Sperrflaeche ins Navigationsnetz genommen (%d passende Objekte)"), Objekte, Radius / 100.f, Gefunden);
		for (const TPair<FString, FString>& U : Uebersprungen)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Hindernisse: %s uebersprungen (keine blockierende Kollision): %s"), *U.Key, *U.Value);
		}
	}
}

bool UTDWaveSubsystem::SperreKlippe(AActor* Klippe)
{
	ATDDefenseCore* Kern = mKern.Get();
	if (!Klippe || !Kern || TDBalance::Wert(TEXT("Gegner.KlippenSperren"), 1.f) <= 0.f || mKlippenGeprueft.Contains(Klippe))
	{
		return false;
	}
	mKlippenGeprueft.Add(Klippe);
	FString Modell = Klippe->GetClass()->GetName();
	if (const UStaticMeshComponent* Netz = Klippe->FindComponentByClass<UStaticMeshComponent>(); Netz && Netz->GetStaticMesh())
	{
		Modell = Netz->GetStaticMesh()->GetName();
	}
	FVector Mitte, Halb;
	Klippe->GetActorBounds(true, Mitte, Halb);
	if (mKlippenGesperrt >= (int32)TDBalance::Wert(TEXT("Gegner.KlippenSperrenMax"), 12.f))
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Klippensperre: %s bei %s nicht gesperrt - Hoechstzahl %d erreicht"), *Modell, *Mitte.ToCompactString(), mKlippenGesperrt);
		return false;
	}
	// Kern und Kernplatz nie aussperren
	if (FBox(Mitte - Halb, Mitte + Halb).ExpandBy(3000.f).IsInsideXY(Kern->GetActorLocation()))
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Klippensperre: %s bei %s liegt zu nah am Kern - nicht gesperrt"), *Modell, *Mitte.ToCompactString());
		return false;
	}
	UNavModifierComponent* Mod = NewObject<UNavModifierComponent>(Klippe, TEXT("TDKlippenNetz"));
	Mod->SetAreaClass(UNavArea_Null::StaticClass());
	Mod->RegisterComponent();
	++mKlippenGesperrt;
	UE_LOG(LogFicsitTD, Log, TEXT("Klippensperre: %s bei %s (%.0f x %.0f x %.0f m) als Sperrflaeche ins Netz genommen (%d.), pruefe die Frontwege nach dem Netzumbau"),
		*Modell, *Mitte.ToCompactString(), Halb.X / 50.f, Halb.Y / 50.f, Halb.Z / 50.f, mKlippenGesperrt);
	FTimerHandle Takt;
	GetWorld()->GetTimerManager().SetTimer(Takt, FTimerDelegate::CreateUObject(this, &UTDWaveSubsystem::PruefeKlippenSperre, TWeakObjectPtr<AActor>(Klippe), 0), 3.f, false);
	return true;
}

void UTDWaveSubsystem::PruefeKlippenSperre(TWeakObjectPtr<AActor> Klippe, int32 Versuch)
{
	AActor* K = Klippe.Get();
	ATDDefenseCore* Kern = mKern.Get();
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!K || !Kern || !Nav)
	{
		return;
	}
	if (Nav->IsNavigationBuildInProgress() && Versuch < 10)
	{
		FTimerHandle Takt;
		World->GetTimerManager().SetTimer(Takt, FTimerDelegate::CreateUObject(this, &UTDWaveSubsystem::PruefeKlippenSperre, Klippe, Versuch + 1), 2.f, false);
		return;
	}
	bool bVoll = true;
	TArray<FString> Teile;
	FNavLocation MitteNav;
	if (!Nav->ProjectPointToNavigation(Kern->GetActorLocation(), MitteNav, FVector(1200.f, 1200.f, 1000.f)))
	{
		bVoll = false;
		Teile.Add(TEXT("Kern nicht auf dem Netz"));
	}
	else
	{
		const int32 Anzahl = FMath::Min(mFronten.Num(), AktiveFrontenFuer(FMath::Max(1, mWelle)) + 1);
		for (int32 i = 0; i < Anzahl; ++i)
		{
			UNavigationPath* Pfad = UNavigationSystemV1::FindPathToLocationSynchronously(World, mFronten[i].Ort, MitteNav.Location, nullptr, UTDNavFilter::StaticClass());
			const bool bOk = Pfad && Pfad->IsValid() && Pfad->PathPoints.Num() >= 2 && PfadAmZiel(Pfad, MitteNav.Location);
			bVoll &= bOk;
			Teile.Add(bOk ? FString::Printf(TEXT("%s %.0f m"), *Himmelsrichtung(mFronten[i].Winkel), Pfad->GetPathLength() / 100.f)
				: FString::Printf(TEXT("%s OHNE vollen Weg"), *Himmelsrichtung(mFronten[i].Winkel)));
		}
	}
	if (bVoll)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Klippensperre bestaetigt (nach %d Warterunden): Frontwege weiter voll - %s"), Versuch, *FString::Join(Teile, TEXT(", ")));
		return;
	}
	TArray<UNavModifierComponent*> Mods;
	K->GetComponents<UNavModifierComponent>(Mods);
	for (UNavModifierComponent* M : Mods)
	{
		if (M && M->GetFName() == TEXT("TDKlippenNetz"))
		{
			M->DestroyComponent();
		}
	}
	--mKlippenGesperrt;
	++mKlippenZurueck;
	UE_LOG(LogFicsitTD, Log, TEXT("Klippensperre zurueckgenommen (%d.): ohne die Klippe fehlt ein Frontweg - %s"), mKlippenZurueck, *FString::Join(Teile, TEXT(", ")));
}

void UTDWaveSubsystem::MarkiereFronten()
{
	static const FString Kategorie = TEXT("FicsitTD Fronten");
	AFGMapManager* Karte = AFGMapManager::Get(GetWorld());
	if (!Karte || !GetWorld()->GetAuthGameMode())
	{
		return;
	}
	TArray<FMapMarker> Alle;
	Karte->GetMapMarkers(Alle);
	int32 Entfernt = 0;
	for (const FMapMarker& M : Alle)
	{
		if (M.CategoryName == Kategorie)
		{
			Karte->RemoveMapMarker(M);
			++Entfernt;
		}
	}
	if (mLauf == ETDLauf::Aus || mFronten.Num() == 0)
	{
		return;
	}
	// Symbol: Hog-Teile (Gegner); ohne Treffer bleibt das Standard-Symbol
	static int32 Symbol = INDEX_NONE;
	if (Symbol == INDEX_NONE)
	{
		AFGIconDatabaseSubsystem* Symbole = AFGIconDatabaseSubsystem::Get(GetWorld());
		UClass* Teile = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Parts/AnimalParts/Desc_HogParts.Desc_HogParts_C"));
		Symbol = Symbole && Teile ? Symbole->GetIconIDForTexture(UFGItemDescriptor::GetSmallIcon(Teile)) : INDEX_NONE;
		if (Symbol == INDEX_NONE)
		{
			Symbol = 0;
		}
	}
	const int32 Naechste = mLauf == ETDLauf::Welle ? FMath::Max(1, mWelle) : mWelle + 1;
	const int32 Aktiv = AktiveFrontenFuer(Naechste);
	int32 Gesetzt = 0;
	for (int32 i = 0; i < mFronten.Num() && i < KernMaxFronten; ++i)
	{
		// ab welcher Welle ist diese Front dabei?
		int32 Ab = 0;
		for (int32 w = Naechste; w <= Naechste + 3; ++w)
		{
			if (AktiveFrontenFuer(w) > i)
			{
				Ab = w;
				break;
			}
		}
		if (i >= Aktiv && Ab == 0)
		{
			continue;   // oeffnet erst spaeter
		}
		FMapMarker M;
		M.Location = mFronten[i].Ort;
		const FString Richtung = Himmelsrichtung(mFronten[i].Winkel);
		M.Name = i < Aktiv ? TDF(TEXT("Front %s"), TEXT("Front %s"), *Richtung) : TDF(TEXT("Front %s (ab Welle %d)"), TEXT("Front %s (from wave %d)"), *Richtung, Ab);
		M.CategoryName = Kategorie;
		M.MapMarkerType = ERepresentationType::RT_MapMarker;
		M.IconID = Symbol;
		M.Color = i < Aktiv ? FLinearColor(1.f, 0.12f, 0.05f) : FLinearColor(0.55f, 0.55f, 0.55f);
		M.Scale = 1.f;
		M.CompassViewDistance = ECompassViewDistance::CVD_Always;
		FMapMarker Neu;
		if (Karte->CanAddNewMapMarker() && Karte->AddNewMapMarker(M, Neu))
		{
			++Gesetzt;
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Fronten-Marker: %d gesetzt, %d alte entfernt (naechste Welle %d, aktiv %d, Symbol %d)"), Gesetzt, Entfernt, Naechste, Aktiv, Symbol);
}

bool UTDWaveSubsystem::bStressSchutz = false;
bool UTDWaveSubsystem::bStressTuermeAus = false;
int32 UTDWaveSubsystem::GegnerToteGesamt = 0;
bool UTDWaveSubsystem::bStressOhneGeschoss = false;
bool UTDWaveSubsystem::bStressDrehenAus = false;
bool UTDWaveSubsystem::bStressSchattenAus = false;
bool UTDWaveSubsystem::bStressEffekteAus = false;
bool UTDWaveSubsystem::bStressDfAus = false;
float UTDWaveSubsystem::StressDrehtakt = 0.f;
int32 UTDWaveSubsystem::GeschosseGesamt = 0;

void UTDWaveSubsystem::Tick(float DeltaTime)
{
	// 0.65: Statuskarten im MAM (erforschbar / gesperrt ab Welle / Voraussetzung) einmal je Sekunde nachfuehren
	mMamTakt -= DeltaTime;
	if (mMamTakt <= 0.f)
	{
		mMamTakt = 1.f;
		TDForschung::AktualisiereMamKarten(this);
	}
	// T4: Boss-Huelle pulsiert; Balken weg, wenn kein Boss mehr lebt
	if (mBosse.Num() > 0)
	{
		mBossPuls += DeltaTime;
		// Puls um die eingestellte Leuchtstaerke der jeweiligen Art (Optik.<Familie>.Boss.Leuchten), Ausschlag Optik.Puls
		const float Puls = 1.f + TDBalance::Wert(TEXT("Optik.Puls"), 0.3f) * FMath::Sin(mBossPuls * 3.f);
		for (const TPair<FString, TObjectPtr<UMaterialInstanceDynamic>>& H : mHuellen)
		{
			if (H.Value && H.Key.StartsWith(TEXT("Boss.")))
			{
				const float Basis = TDBalance::Wert(FString::Printf(TEXT("Optik.%s.Boss.Leuchten"), *H.Key.Mid(5)), 0.5f);
				H.Value->SetScalarParameterValue(TEXT("Staerke"), Basis * Puls);
			}
		}
		const bool bLebt = mBosse.ContainsByPredicate([](const TWeakObjectPtr<AFGCreature>& B) { return B.IsValid() && B->IsAliveAndWell(); });
		if (!bLebt && !NachschubHatBoss())
		{
			mBossPuls = 0.f;
			if (mBossBalken)
			{
				Ansage(TDF(TEXT("BOSS BESIEGT: %s!"), TEXT("BOSS DEFEATED: %s!"), *mBossName));
			}
			EntferneBossBalken();
		}
	}
	MesseBild();
	StressTick();
	LaufTick(DeltaTime);
	if (!mRezepteFreigeschaltet)
	{
		mZeitBisRezeptVersuch -= DeltaTime;
		if (mZeitBisRezeptVersuch <= 0.f)
		{
			mZeitBisRezeptVersuch = 2.f;
			SchalteRezepteFrei();
		}
	}
	mTestMauern.RemoveAll([](const TWeakObjectPtr<ATDTestWall>& Weak) { return !Weak.IsValid(); });
	if (mGegner.Num() == 0)
	{
		// Ohne Welle trotzdem alle 10 s den Turmstatus loggen (Diagnose: warum schiesst er nicht?)
		mZeitBisLog -= DeltaTime;
		if (mZeitBisLog <= 0.f)
		{
			mZeitBisLog = 10.f;
			for (const FString& Line : GetWallStatusLines())
			{
				if (Line.StartsWith(TEXT("Turm")))
				{
					UE_LOG(LogFicsitTD, Log, TEXT("%s"), *Line);
				}
			}
		}
		return;
	}
	mZeitBisSteuerung -= DeltaTime;
	if (mZeitBisSteuerung <= 0.f)
	{
		mZeitBisSteuerung = 1.f;
		SteuereKreaturen();
	}
	// Alle 5 s Status ins Log, damit der Verlauf ohne Chat nachvollziehbar ist
	mZeitBisLog -= DeltaTime;
	if (mZeitBisLog <= 0.f)
	{
		mZeitBisLog = 5.f;
		for (const FString& Line : GetStatusLines())
		{
			UE_LOG(LogFicsitTD, Log, TEXT("%s"), *Line);
		}
	}
}

void UTDWaveSubsystem::StarteBildMessung()
{
	if (!mPhasenAngemeldet && GetWorld() && GetWorld()->PersistentLevel)
	{
		mPhasenAngemeldet = true;
		mBeginnHandle = FCoreDelegates::OnBeginFrame.AddUObject(this, &UTDWaveSubsystem::PhasenBildBeginn);
		mEndeHandle = FCoreDelegates::OnEndFrame.AddWeakLambda(this, [this]() { Stempel(7); });
		mWeltStartHandle = FWorldDelegates::OnWorldTickStart.AddWeakLambda(this, [this](UWorld* W, ELevelTick, float) { if (W == GetWorld()) Stempel(1); });
		mWeltEndeHandle = FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this, [this](UWorld* W, ELevelTick, float) { if (W == GetWorld()) Stempel(6); });
		const ETickingGroup Gruppen[] = { TG_PrePhysics, TG_DuringPhysics, TG_PostPhysics, TG_PostUpdateWork };
		for (int32 i = 0; i < 4; ++i)
		{
			FTDPhasenSonde* S = new FTDPhasenSonde();
			S->Ziel = this;
			S->Phase = 2 + i;
			S->bCanEverTick = true;
			S->bTickEvenWhenPaused = true;
			S->bHighPriority = true;
			S->TickGroup = Gruppen[i];
			S->EndTickGroup = Gruppen[i];
			S->RegisterTickFunction(GetWorld()->PersistentLevel);
			mSonden.Add(S);
		}
	}
	static bool bGcAngemeldet = false;
	if (!bGcAngemeldet)
	{
		bGcAngemeldet = true;
		FCoreUObjectDelegates::GetPostGarbageCollect().AddLambda([]() { ++GTDSpeicherbereinigungen; });
	}
	mBildMessung = true;
	mBildZeiten.Reset();
	mLetztesBild = FPlatformTime::Seconds();
	mHirnStart = TDLeistung::HirnZyklen;
	mTurmStart = TDLeistung::TurmZyklen;
	mMaxLebend = 0;
	mSpielSumme = mRenderSumme = mGpuSumme = 0.0;
	mSpielMax = mRenderMax = mGpuMax = 0.f;
	mToteStart = GegnerToteGesamt;
	mMessBeginn = mLetztesBild;
	mAbschnittEnde = mLetztesBild + 5.0;
	SetzeAbschnittZurueck();
	mZeitleiste.Reset();
	FMemory::Memzero(mPhaseNormal, sizeof(mPhaseNormal));
	FMemory::Memzero(mPhaseRuckel, sizeof(mPhaseRuckel));
	mPhaseNormalBilder = mPhaseRuckelBilder = 0;
	FMemory::Memzero(mTeilNormal, sizeof(mTeilNormal));
	mTickNormal.Reset();
	mTickRuckel.Reset();
	FMemory::Memzero(mTeilRuckel, sizeof(mTeilRuckel));
	mSchadenStart = TDLeistung::SchadenZyklen;
	mGeschossStart = TDLeistung::GeschossZyklen;
	mGeschosseStart = GeschosseGesamt;
}

void UTDWaveSubsystem::MesseBild()
{
	if (!mBildMessung)
	{
		return;
	}
	const double Jetzt = FPlatformTime::Seconds();
	const float BildMs = static_cast<float>((Jetzt - mLetztesBild) * 1000.0);
	if (mBildZeiten.Num() < 200000)
	{
		mBildZeiten.Add(BildMs);
	}
	mAbschnittRuckler += BildMs > 50.f ? 1 : 0;
	mAbschnittMax = FMath::Max(mAbschnittMax, BildMs);
	if (Jetzt >= mAbschnittEnde)
	{
		SchliesseAbschnitt(Jetzt);
	}
	mLetztesBild = Jetzt;
	// Engine-Zeiten des letzten fertigen Bilds (gesetzt in FViewport::Draw bzw. vom RHI)
	const float Spiel = static_cast<float>(FPlatformTime::ToMilliseconds(GGameThreadTime));
	const float Render = static_cast<float>(FPlatformTime::ToMilliseconds(GRenderThreadTime));
	const float Gpu = static_cast<float>(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
	mSpielSumme += Spiel;
	mRenderSumme += Render;
	mGpuSumme += Gpu;
	mSpielMax = FMath::Max(mSpielMax, Spiel);
	mRenderMax = FMath::Max(mRenderMax, Render);
	mGpuMax = FMath::Max(mGpuMax, Gpu);
	int32 Lebend = 0, Sicht = 0;
	FVector Mitte = FVector::ZeroVector;
	const float Weltzeit = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (const FTDWellenGegner& G : mGegner)
	{
		const AFGCreature* K = G.Kreatur.Get();
		if (!K || !K->IsAliveAndWell())
		{
			continue;
		}
		++Lebend;
		Mitte += K->GetActorLocation();
		// T2g: im Hauptbild gezeichnet (nicht nur Schatten) in den letzten 0,1 s
		const USkeletalMeshComponent* Modell = K->GetMesh();
		Sicht += Modell && Weltzeit - Modell->GetLastRenderTimeOnScreen() < 0.1f ? 1 : 0;
	}
	mMaxLebend = FMath::Max(mMaxLebend, Lebend);
	++mAbschnittBilder;
	mSichtSumme += Sicht;
	mSichtMax = FMath::Max(mSichtMax, Sicht);
	if (BildMs > 50.f)
	{
		mRucklerSichtSumme += Sicht;
		++mRucklerBilder;
		mRSpiel += Spiel;
		mRRender += Render;
		mRGpu += Gpu;
		mRLaden += IsAsyncLoading() ? 1 : 0;
	}
	const APlayerCameraManager* Kamera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (Kamera && Lebend > 0)
	{
		const FVector Zu = (Mitte / Lebend - Kamera->GetCameraLocation()).GetSafeNormal2D();
		const FVector Blick = Kamera->GetCameraRotation().Vector().GetSafeNormal2D();
		if (!Zu.IsNearlyZero() && !Blick.IsNearlyZero())
		{
			mBlickSumme += FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Zu, Blick), -1.f, 1.f)));
			++mBlickBilder;
		}
	}
}

void UTDWaveSubsystem::Stempel(int32 Phase)
{
	if (Phase >= 0 && Phase < PhasenAnzahl)
	{
		mStempel[Phase] = FPlatformTime::Seconds();
		mStempelGesetzt[Phase] = true;
	}
}

void UTDWaveSubsystem::LeseTeile(double* Ziel) const
{
	Ziel[0] = FPlatformTime::ToMilliseconds64(TDLeistung::BewegungZyklen.load());
	Ziel[1] = FPlatformTime::ToMilliseconds64(TDLeistung::ControllerZyklen.load());
	Ziel[2] = FPlatformTime::ToMilliseconds64(TDLeistung::KreaturZyklen.load());
	Ziel[3] = FPlatformTime::ToMilliseconds64(TDLeistung::SkelettZyklen.load());
	Ziel[4] = TDLeistung::BewegungTicks.load();
	Ziel[5] = TDLeistung::Durchdringung.load();
	Ziel[6] = TDLeistung::Feststecken.load();
}

FString UTDWaveSubsystem::TickKlassenText() const
{
	const double N = FMath::Max(mPhaseNormalBilder, 1);
	const double R = FMath::Max(mPhaseRuckelBilder, 1);
	struct FEintrag { const UClass* Klasse; double Normal; double Ruckel; };
	TArray<FEintrag> Liste;
	double SummeN = 0.0, SummeR = 0.0;
	TSet<const UClass*> Klassen;
	for (const TPair<const UClass*, uint64>& P : mTickNormal) Klassen.Add(P.Key);
	for (const TPair<const UClass*, uint64>& P : mTickRuckel) Klassen.Add(P.Key);
	for (const UClass* K : Klassen)
	{
		const uint64* ZN = mTickNormal.Find(K);
		const uint64* ZR = mTickRuckel.Find(K);
		FEintrag E{ K, ZN ? FPlatformTime::ToMilliseconds64(*ZN) / N : 0.0, ZR ? FPlatformTime::ToMilliseconds64(*ZR) / R : 0.0 };
		SummeN += E.Normal;
		SummeR += E.Ruckel;
		Liste.Add(E);
	}
	FString Text = FString::Printf(TEXT("Summe normal %.1f / Ruckler %.1f (%d Klassen); Mehraufwand im Ruckler:"), SummeN, SummeR, Liste.Num());
	Liste.Sort([](const FEintrag& A, const FEintrag& B) { return A.Ruckel - A.Normal > B.Ruckel - B.Normal; });
	for (int32 i = 0; i < FMath::Min(12, Liste.Num()); ++i)
	{
		Text += FString::Printf(TEXT(" %s %.2f->%.2f;"), Liste[i].Klasse ? *Liste[i].Klasse->GetName() : TEXT("?"), Liste[i].Normal, Liste[i].Ruckel);
	}
	Text += TEXT(" Teuerste normal:");
	Liste.Sort([](const FEintrag& A, const FEintrag& B) { return A.Normal > B.Normal; });
	for (int32 i = 0; i < FMath::Min(8, Liste.Num()); ++i)
	{
		Text += FString::Printf(TEXT(" %s %.2f;"), Liste[i].Klasse ? *Liste[i].Klasse->GetName() : TEXT("?"), Liste[i].Normal);
	}
	return Text;
}

FString UTDWaveSubsystem::TeileText(const double* Summen, int32 Bilder) const
{
	static const TCHAR* const Namen[] = { TEXT("Bewegung"), TEXT("Controller"), TEXT("Kreatur"), TEXT("Skelett"), TEXT("Bewegungs-Ticks"), TEXT("Durchdringung"), TEXT("Feststecken") };
	FString Text;
	for (int32 i = 0; i < TeileAnzahl; ++i)
	{
		const double Wert = Bilder > 0 ? Summen[i] / Bilder : 0.0;
		Text += i < 4 ? FString::Printf(TEXT("%s%s %.2f ms"), i ? TEXT(", ") : TEXT(""), Namen[i], Wert)
			: FString::Printf(TEXT(", %s %.1f"), Namen[i], Wert);
	}
	return Text;
}

void UTDWaveSubsystem::PhasenBildBeginn()
{
	const double Jetzt = FPlatformTime::Seconds();
	double TeilJetzt[TeileAnzahl];
	LeseTeile(TeilJetzt);
	bool bVollstaendig = mStempelGesetzt[0];
	for (int32 i = 1; i < PhasenAnzahl && bVollstaendig; ++i)
	{
		bVollstaendig = mStempelGesetzt[i] && mStempel[i] >= mStempel[i - 1];
	}
	if (bVollstaendig && mBildMessung && Jetzt >= mStempel[PhasenAnzahl - 1])
	{
		const bool bRuckler = (Jetzt - mStempel[0]) * 1000.0 > 50.0;
		double* Summe = bRuckler ? mPhaseRuckel : mPhaseNormal;
		for (int32 i = 0; i < PhasenAnzahl; ++i)
		{
			const double Ende = i + 1 < PhasenAnzahl ? mStempel[i + 1] : Jetzt;
			const double Ms = (Ende - mStempel[i]) * 1000.0;
			Summe[i] += Ms;
			if (bRuckler)
			{
				mAbschnittPhaseRuckel[i] += Ms;
			}
		}
		TMap<const UClass*, uint64>& TickSumme = bRuckler ? mTickRuckel : mTickNormal;
		for (const TPair<const UClass*, uint64>& P : TDLeistung::TickJeKlasse)
		{
			TickSumme.FindOrAdd(P.Key) += P.Value;
		}
		double* TeilSumme = bRuckler ? mTeilRuckel : mTeilNormal;
		for (int32 i = 0; i < TeileAnzahl; ++i)
		{
			const double D = TeilJetzt[i] - mTeilVorher[i];
			TeilSumme[i] += D;
			if (bRuckler)
			{
				mAbschnittTeilRuckel[i] += D;
			}
		}
		++(bRuckler ? mPhaseRuckelBilder : mPhaseNormalBilder);
		mAbschnittPhaseRuckelBilder += bRuckler ? 1 : 0;
	}
	FMemory::Memcpy(mTeilVorher, TeilJetzt, sizeof(mTeilVorher));
	TDLeistung::TickJeKlasse.Reset();
	TDLeistung::bTickProfil = mBildMessung;
	FMemory::Memzero(mStempelGesetzt, sizeof(mStempelGesetzt));
	mStempel[0] = Jetzt;
	mStempelGesetzt[0] = true;
}

FString UTDWaveSubsystem::PhasenText(const double* Summen, int32 Bilder) const
{
	FString Text;
	for (int32 i = 0; i < PhasenAnzahl; ++i)
	{
		Text += FString::Printf(TEXT("%s%s %.1f"), i ? TEXT(", ") : TEXT(""), GTDPhasenNamen[i], Bilder > 0 ? Summen[i] / Bilder : 0.0);
	}
	return Text;
}

void UTDWaveSubsystem::Deinitialize()
{
	FCoreDelegates::OnBeginFrame.Remove(mBeginnHandle);
	FCoreDelegates::OnEndFrame.Remove(mEndeHandle);
	FWorldDelegates::OnWorldTickStart.Remove(mWeltStartHandle);
	FWorldDelegates::OnWorldPostActorTick.Remove(mWeltEndeHandle);
	for (FTDPhasenSonde* S : mSonden)
	{
		S->UnRegisterTickFunction();
		delete S;
	}
	mSonden.Reset();
	mPhasenAngemeldet = false;
	Super::Deinitialize();
}

void UTDWaveSubsystem::SetzeAbschnittZurueck()
{
	mAbschnittRuckler = 0;
	mAbschnittMax = 0.f;
	mAbschnittBilder = 0;
	mSichtSumme = 0.0;
	mSichtMax = 0;
	mRucklerSichtSumme = 0.0;
	mRucklerBilder = 0;
	mRSpiel = mRRender = mRGpu = 0.0;
	FMemory::Memzero(mAbschnittPhaseRuckel, sizeof(mAbschnittPhaseRuckel));
	FMemory::Memzero(mAbschnittTeilRuckel, sizeof(mAbschnittTeilRuckel));
	mAbschnittPhaseRuckelBilder = 0;
	mRLaden = 0;
	mGcStart = GTDSpeicherbereinigungen;
	mBlickSumme = 0.0;
	mBlickBilder = 0;
}

void UTDWaveSubsystem::SchliesseAbschnitt(double Jetzt)
{
	const ATDDefenseCore* Kern = mKern.Get();
	float Naechste = -1.f;
	int32 Lebend = 0, Anlauf = 0, Angriff = 0;
	for (const FTDWellenGegner& G : mGegner)
	{
		const AFGCreature* K = G.Kreatur.Get();
		if (!K || !K->IsAliveAndWell())
		{
			continue;
		}
		++Lebend;
		if (Kern)
		{
			const float D = FVector::Dist2D(K->GetActorLocation(), Kern->GetActorLocation()) / 100.f;
			Naechste = Naechste < 0.f ? D : FMath::Min(Naechste, D);
		}
		if (const UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(K))
		{
			Anlauf += Hirn->GetPhase() == ETDHirnPhase::Anlauf ? 1 : 0;
			Angriff += Hirn->GetPhase() == ETDHirnPhase::Angriff ? 1 : 0;
		}
	}
	// T2g: Tuerme im Bild, Abstand Spieler-Kern
	int32 TuermeSicht = 0;
	const float Weltzeit = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		bool bGesehen = false;
		It->ForEachComponent<UPrimitiveComponent>(false, [&](const UPrimitiveComponent* P)
		{
			bGesehen = bGesehen || Weltzeit - P->GetLastRenderTimeOnScreen() < 0.2f;
		});
		TuermeSicht += bGesehen ? 1 : 0;
	}
	const APawn* Spieler = UGameplayStatics::GetPlayerPawn(this, 0);
	float NahSpieler = -1.f;
	if (Spieler)
	{
		for (const FTDWellenGegner& G : mGegner)
		{
			const AFGCreature* K = G.Kreatur.Get();
			if (K && K->IsAliveAndWell())
			{
				const float D = FVector::Dist2D(K->GetActorLocation(), Spieler->GetActorLocation()) / 100.f;
				NahSpieler = NahSpieler < 0.f ? D : FMath::Min(NahSpieler, D);
			}
		}
	}
	const double RB = FMath::Max(mRucklerBilder, 1);
	FString Zerlegung = mRucklerBilder > 0
		? FString::Printf(TEXT(" rS%.0f/R%.0f/G%.0f lad%d"), mRSpiel / RB, mRRender / RB, mRGpu / RB, mRLaden) : FString();
	if (mAbschnittPhaseRuckelBilder > 0)
	{
		// T2i: groesste Phase in den Ruckel-Bildern dieses Abschnitts
		int32 Groesste = 0;
		for (int32 i = 1; i < PhasenAnzahl; ++i)
		{
			Groesste = mAbschnittPhaseRuckel[i] > mAbschnittPhaseRuckel[Groesste] ? i : Groesste;
		}
		const double N = mAbschnittPhaseRuckelBilder;
		Zerlegung += FString::Printf(TEXT(" phase:%s %.0fms teile:B%.0f/C%.0f/K%.0f/S%.0f dr%.0f fest%.0f"), GTDPhasenNamen[Groesste], mAbschnittPhaseRuckel[Groesste] / N,
			mAbschnittTeilRuckel[0] / N, mAbschnittTeilRuckel[1] / N, mAbschnittTeilRuckel[2] / N, mAbschnittTeilRuckel[3] / N, mAbschnittTeilRuckel[5] / N, mAbschnittTeilRuckel[6] / N);
	}
	const float SpielerKern = Spieler && Kern ? FVector::Dist2D(Spieler->GetActorLocation(), Kern->GetActorLocation()) / 100.f : -1.f;
	mZeitleiste.Add(FString::Printf(TEXT("%.0fs R%d max%.0f nah%.0fm lebt%d anl%d angr%d sicht%.0f/%d rsicht%.0f blick%.0f tuerme%d sp%.0fm nahsp%.0fm gc%d%s"),
		Jetzt - mMessBeginn, mAbschnittRuckler, mAbschnittMax, Naechste, Lebend, Anlauf, Angriff,
		mAbschnittBilder > 0 ? mSichtSumme / mAbschnittBilder : 0.0, mSichtMax,
		mRucklerBilder > 0 ? mRucklerSichtSumme / mRucklerBilder : -1.0,
		mBlickBilder > 0 ? mBlickSumme / mBlickBilder : -1.0, TuermeSicht, SpielerKern, NahSpieler, GTDSpeicherbereinigungen - mGcStart, *Zerlegung));
	SetzeAbschnittZurueck();
	mAbschnittEnde = Jetzt + 5.0;
}

void UTDWaveSubsystem::MeldeZeitleiste(const FString& Anlass)
{
	if (mAbschnittRuckler > 0 || mAbschnittMax > 0.f)
	{
		SchliesseAbschnitt(mLetztesBild);
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Bildphasen %s (ms je Bild, Mittel; Vorlauf=Eingabe/Streaming, Weltstart=bis PrePhysics, VorPhysik=TG_PrePhysics, Physik=TG_DuringPhysics+EndPhysics, NachPhysik+Timer=TG_PostPhysics bis PostUpdateWork, Nacharbeit=TG_PostUpdateWork, Zeichnen=Weltende bis Bildende, Bildwechsel=Bildende bis naechster Beginn): normal (%d Bilder) %s | Ruckler (%d Bilder) %s"),
		*Anlass, mPhaseNormalBilder, *PhasenText(mPhaseNormal, mPhaseNormalBilder), mPhaseRuckelBilder, *PhasenText(mPhaseRuckel, mPhaseRuckelBilder));
	UE_LOG(LogFicsitTD, Log, TEXT("Tick-Klassen %s (ms je Bild, nur Spiel-Thread, inklusive Warten): %s"), *Anlass, *TickKlassenText());
	UE_LOG(LogFicsitTD, Log, TEXT("Gegner-Ticks %s (je Bild, inklusive Warten, nur Wellengegner): normal %s | Ruckler %s"),
		*Anlass, *TeileText(mTeilNormal, mPhaseNormalBilder), *TeileText(mTeilRuckel, mPhaseRuckelBilder));
	UE_LOG(LogFicsitTD, Log, TEXT("Zeitleiste %s (je 5 s: Ruckler >50 ms, schlechtestes Bild ms, naechster Gegner zum Kern, lebend, im Anlauf, im Angriff, Gegner im Bild Mittel/Max, Gegner im Bild bei Rucklern, Blickwinkel zur Gegnermitte Grad, Tuerme im Bild, Spieler zum Kern, naechster Gegner zum Spieler, Speicherbereinigungen; bei Rucklern Spiel/Render/Grafik ms je Ruckel-Bild und Ruckel-Bilder waehrend Nachladen): %s"),
		*Anlass, *FString::Join(mZeitleiste, TEXT(" | ")));
}

UTDWaveSubsystem::FTDBildWerte UTDWaveSubsystem::BildWerte() const
{
	FTDBildWerte W;
	W.Bilder = mBildZeiten.Num();
	W.MaxLebend = mMaxLebend;
	if (W.Bilder == 0)
	{
		return W;
	}
	TArray<float> Sortiert = mBildZeiten;
	Sortiert.Sort();
	double Summe = 0.0;
	for (const float Z : Sortiert)
	{
		Summe += Z;
		W.Ruckler += Z > 50.f ? 1 : 0;
	}
	W.Mittel = static_cast<float>(Summe / W.Bilder);
	W.P95 = Sortiert[FMath::Min(W.Bilder - 1, FMath::FloorToInt(0.95f * W.Bilder))];
	W.Max = Sortiert.Last();
	const double MsJeZyklus = FPlatformTime::GetSecondsPerCycle64() * 1000.0 / W.Bilder;
	W.HirnMs = static_cast<float>((TDLeistung::HirnZyklen - mHirnStart) * MsJeZyklus);
	W.TurmMs = static_cast<float>((TDLeistung::TurmZyklen - mTurmStart) * MsJeZyklus);
	W.SpielMs = static_cast<float>(mSpielSumme / W.Bilder);
	W.RenderMs = static_cast<float>(mRenderSumme / W.Bilder);
	W.GpuMs = static_cast<float>(mGpuSumme / W.Bilder);
	W.SpielMax = mSpielMax;
	W.RenderMax = mRenderMax;
	W.GpuMax = mGpuMax;
	W.Tote = GegnerToteGesamt - mToteStart;
	W.SchadenMs = static_cast<float>((TDLeistung::SchadenZyklen - mSchadenStart) * MsJeZyklus);
	W.GeschossMs = static_cast<float>((TDLeistung::GeschossZyklen - mGeschossStart) * MsJeZyklus);
	W.Geschosse = GeschosseGesamt - mGeschosseStart;
	return W;
}

void UTDWaveSubsystem::MeldeKreaturen(const FString& Anlass) const
{
	int32 Lebend = 0, Tot = 0, Unsere = 0;
	for (TActorIterator<AFGCreature> It(GetWorld()); It; ++It)
	{
		(It->IsAliveAndWell() ? Lebend : Tot)++;
		Unsere += It->Tags.Contains(TEXT("FicsitTD")) ? 1 : 0;
	}
	int32 Limit = -1;
	if (AFGCreatureSubsystem* Kreaturen = AFGCreatureSubsystem::Get(GetWorld()))
	{
		if (const FIntProperty* Feld = FindFProperty<FIntProperty>(AFGCreatureSubsystem::StaticClass(), TEXT("mMaxNumCreatures")))
		{
			Limit = Feld->GetPropertyValue_InContainer(Kreaturen);
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Kreaturen (%s): %d lebend, %d tot (Koerper liegen noch), davon %d von FicsitTD; Spiel-Limit mMaxNumCreatures %d"),
		*Anlass, Lebend, Tot, Unsere, Limit);
}

bool UTDWaveSubsystem::StarteStress(ATDDefenseCore* Kern, const TArray<int32>& Stufen, float LPFaktor, float Sekunden, const FString& Art, float SterbenJeSekunde, FString& Meldung)
{
	if (!Kern)
	{
		Meldung = TDT(TEXT("Kein Abwehrkern gebaut."), TEXT("No Defense Core built."));
		return false;
	}
	if (mLauf == ETDLauf::Welle || mStressPhase != 0)
	{
		Meldung = mStressPhase != 0 ? TDT(TEXT("Stresstest laeuft schon (/td kern stress stop)."), TEXT("Stress test already running (/td kern stress stop).")) : TDT(TEXT("Erst nach der Welle (Stresstest nur in der Bauphase oder ohne Lauf)."), TEXT("Only after the wave (stress test only in the build phase or without a run)."));
		return false;
	}
	FrontenFuer(Kern);
	if (mFronten.Num() == 0)
	{
		Meldung = TDT(TEXT("Keine Front gefunden (/td kern fronten neu)."), TEXT("No front found (/td kern fronten neu)."));
		return false;
	}
	if (mLauf == ETDLauf::Aus)
	{
		mKern = Kern;   // SpawneGruppe braucht den Kern fuer Weg und Gehirn
	}
	mStressStufen = Stufen;
	mStressIndex = 0;
	mStressLP = LPFaktor;
	mStressSekunden = Sekunden;
	mStressArt = Art;
	mStressSterben = FMath::Max(0.f, SterbenJeSekunde);
	mStressSterbenRest = 0.f;
	mStressErgebnis.Reset();
	mStressPhase = 2;   // erste Stufe sofort im naechsten Tick
	mStressEnde = 0.0;
	bStressSchutz = true;
	MeldeKreaturen(TEXT("Stresstest Start"));
	{
		// T2e: welche Schatten-/Licht-Technik die Engine gerade nutzt
		FString Render;
		for (const TCHAR* Name : { TEXT("r.Shadow.Virtual.Enable"), TEXT("r.Shadow.Virtual.Cache"), TEXT("r.DynamicGlobalIlluminationMethod"), TEXT("r.ReflectionMethod"), TEXT("r.Nanite"), TEXT("r.ShadowQuality"),
			TEXT("r.GenerateMeshDistanceFields"), TEXT("r.DistanceFieldAO"), TEXT("r.DistanceFieldShadowing"), TEXT("r.AOGlobalDistanceField"), TEXT("r.DFShadowQuality"), TEXT("r.Shadow.CSMCaching") })
		{
			const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name);
			Render += FString::Printf(TEXT("%s%s=%s"), Render.IsEmpty() ? TEXT("") : TEXT(", "), Name, V ? *V->GetString() : TEXT("-"));
		}
		int32 Tuerme = 0;
		for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
		{
			++Tuerme;
		}
		UE_LOG(LogFicsitTD, Log, TEXT("Render (Stresstest Start): %s; %d Tuerme"), *Render, Tuerme);
	}
	Meldung = TDF(TEXT("Stresstest: %d Stufen je %.0f s an %s, Gegner x%.0f LP, kein Schaden am Kern/Mauern, Abschuesse zaehlen nicht. Bitte nahe am Kern bleiben und nicht bauen."),
		TEXT("Stress test: %d stages of %.0f s each at %s, enemies x%.0f HP, no damage to Core/walls, kills do not count. Please stay near the Core and do not build."),
		Stufen.Num(), Sekunden, *FrontName(0), LPFaktor);
	return true;
}

void UTDWaveSubsystem::RaeumeStress()
{
	int32 Weg = 0;
	for (const TWeakObjectPtr<AFGCreature>& G : mStressGegner)
	{
		if (AFGCreature* K = G.Get())
		{
			K->Destroy();
			++Weg;
		}
	}
	mGegner.RemoveAll([](const FTDWellenGegner& G) { return !G.Kreatur.IsValid() || G.Kreatur->IsActorBeingDestroyed(); });
	mStressGegner.Reset();
	UE_LOG(LogFicsitTD, Log, TEXT("Stresstest: %d Gegner entfernt"), Weg);
}

void UTDWaveSubsystem::StoppeStress(const FString& Grund)
{
	if (mStressPhase == 0)
	{
		return;
	}
	mBildMessung = false;
	RaeumeStress();
	mStressPhase = 0;
	bStressSchutz = false;
	bStressTuermeAus = false;
	bStressOhneGeschoss = false;
	bStressDrehenAus = false;
	bStressSchattenAus = false;
	bStressEffekteAus = false;
	bStressDfAus = false;
	StressDrehtakt = 0.f;
	UTDGegnerHirn::bAngriffAus = false;
	UTDGegnerHirn::AnimNurSichtbarErzwungen = -1;
	UTDGegnerHirn::KollisionErzwungen = -1;
	UTDGegnerHirn::RagdollErzwungen = -1;
	Ansage(Grund);
	for (const FString& Zeile : mStressErgebnis)
	{
		Ansage(Zeile);
	}
}

void UTDWaveSubsystem::StressTick()
{
	if (mStressPhase == 0)
	{
		return;
	}
	const double Jetzt = FPlatformTime::Seconds();
	if (mStressPhase == 1 && mStressSterben > 0.f)
	{
		// Tode ohne Schuesse: je Sekunde N Stress-Gegner mit einem Schlag toeten (normaler Todesablauf des Spiels)
		mStressSterbenRest += static_cast<float>(Jetzt - mStressLetzterTick) * mStressSterben;
		for (const TWeakObjectPtr<AFGCreature>& G : mStressGegner)
		{
			if (mStressSterbenRest < 1.f)
			{
				break;
			}
			if (AFGCreature* K = G.Get(); K && K->IsAliveAndWell())
			{
				UGameplayStatics::ApplyDamage(K, 1.0e7f, nullptr, nullptr, UDamageType::StaticClass());
				mStressSterbenRest -= 1.f;
			}
		}
		mStressSterbenRest = FMath::Min(mStressSterbenRest, 1.f);
	}
	mStressLetzterTick = Jetzt;
	if (Jetzt < mStressEnde)
	{
		return;
	}
	if (mStressPhase == 1)
	{
		// Stufe fertig: auswerten, Gegner entfernen, kurze Pause
		const FTDBildWerte W = BildWerte();
		mBildMessung = false;
		MeldeZeitleiste(FString::Printf(TEXT("Stresstest %d Gegner"), mStressStufen[mStressIndex]));
		int32 Lebend = 0;
		for (const TWeakObjectPtr<AFGCreature>& G : mStressGegner)
		{
			Lebend += G.IsValid() && G->IsAliveAndWell() ? 1 : 0;
		}
		const int32 N = mStressStufen[mStressIndex];
		MeldeKreaturen(FString::Printf(TEXT("Stresstest %d Gegner, Ende"), N));
		UE_LOG(LogFicsitTD, Log, TEXT("Stresstest %d Gegner [%s, Animation %s, Kollision %s, Ragdoll %s, Tuerme %s, Geschosse %s, Angriff %s, Drehen %s, Drehtakt %.0f/s, Turmschatten %s, Distanzfeld %s, Effekte %s, Toeten %.1f/s]: %d Tode; Treffer (Spiel) %.2f ms, Geschosse %.2f ms (%d Stueck); Engine je Bild: Spiel-Thread %.1f ms (max %.0f), Render-Thread %.1f ms (max %.0f), Grafikkarte %.1f ms (max %.0f); %d Bilder in %.0f s, Bildzeit Mittel %.1f ms (%.0f FPS), 95 %% unter %.1f ms, max %.1f ms, %d Bilder ueber 50 ms; eigener Code je Bild: Gegnerhirn %.2f ms, Tuerme %.2f ms; am Ende %d lebend"),
			N, *mStressArt,
			(UTDGegnerHirn::AnimNurSichtbarErzwungen >= 0 ? UTDGegnerHirn::AnimNurSichtbarErzwungen > 0 : TDBalance::Wert(TEXT("Leistung.AnimNurSichtbar"), 1.f) > 0.5f) ? TEXT("nur sichtbar") : TEXT("immer"),
			(UTDGegnerHirn::KollisionErzwungen >= 0 ? UTDGegnerHirn::KollisionErzwungen > 0 : TDBalance::Wert(TEXT("Leistung.GegnerKollision"), 1.f) > 0.5f) ? TEXT("an") : TEXT("aus"),
			(UTDGegnerHirn::RagdollErzwungen >= 0 ? UTDGegnerHirn::RagdollErzwungen > 0 : TDBalance::Wert(TEXT("Leistung.Ragdoll"), 1.f) > 0.5f) ? TEXT("an") : TEXT("aus"),
			bStressTuermeAus ? TEXT("aus") : TEXT("an"), bStressOhneGeschoss ? TEXT("aus") : TEXT("an"), UTDGegnerHirn::bAngriffAus ? TEXT("aus") : TEXT("an"),
			bStressDrehenAus ? TEXT("aus") : TEXT("an"), StressDrehtakt > 0.f ? StressDrehtakt : 30.f, bStressSchattenAus ? TEXT("aus") : TEXT("an"), bStressDfAus ? TEXT("aus") : TEXT("an"), bStressEffekteAus ? TEXT("aus") : TEXT("an"), mStressSterben, W.Tote, W.SchadenMs, W.GeschossMs, W.Geschosse,
			W.SpielMs, W.SpielMax, W.RenderMs, W.RenderMax, W.GpuMs, W.GpuMax,
			W.Bilder, mStressSekunden, W.Mittel, W.Mittel > 0.f ? 1000.f / W.Mittel : 0.f, W.P95, W.Max, W.Ruckler, W.HirnMs, W.TurmMs, Lebend);
		mStressErgebnis.Add(TDF(TEXT("%d Gegner: %.0f FPS im Mittel, schlechtestes Bild %.0f ms, %d Ruckler, %d Tode; Spiel-Thread %.1f ms (Treffer %.2f, Geschosse %.2f), Render %.1f ms, Grafik %.1f ms"),
			TEXT("%d enemies: %.0f FPS average, worst frame %.0f ms, %d stutters, %d deaths; game thread %.1f ms (hits %.2f, projectiles %.2f), render %.1f ms, GPU %.1f ms"),
			N, W.Mittel > 0.f ? 1000.f / W.Mittel : 0.f, W.Max, W.Ruckler, W.Tote, W.SpielMs, W.SchadenMs, W.GeschossMs, W.RenderMs, W.GpuMs));
		RaeumeStress();
		++mStressIndex;
		mStressPhase = 2;
		mStressEnde = FPlatformTime::Seconds() + 3.0;
		return;
	}
	// Phase 2: naechste Stufe spawnen oder fertig
	if (!mStressStufen.IsValidIndex(mStressIndex))
	{
		StoppeStress(TDT(TEXT("Stresstest fertig. Ergebnisse (auch im Log, Zeilen 'Stresstest'):"), TEXT("Stress test finished. Results (also in the log, lines 'Stresstest'):")));
		return;
	}
	const ATDDefenseCore* Kern = mKern.Get();
	if (!Kern)
	{
		StoppeStress(TDT(TEXT("Stresstest abgebrochen: kein Abwehrkern."), TEXT("Stress test aborted: no Defense Core.")));
		return;
	}
	const int32 N = mStressStufen[mStressIndex];
	const FVector Mitte = Kern->GetActorLocation();
	EntschaerfeLogistik(Mitte);
	// Arten: hog (Standard), mix = alle Katalog-Gegner bis Welle 12 reihum, sonst ein Katalog-Name (stinger_child, alphahog ...)
	TArray<UClass*> Klassen;
	TArray<FString> Namen;
	for (const FTDKatalogEintrag& E : KernKatalog)
	{
		const FString Pfad(E.Pfad);
		Namen.Add(FPaths::GetBaseFilename(Pfad).RightChop(5));
		if (mStressArt == TEXT("mix") ? E.bStressMix : Pfad.Contains(FString::Printf(TEXT("Char_%s."), *mStressArt)))
		{
			if (UClass* K = LoadClass<AFGCreature>(nullptr, E.Pfad))
			{
				Klassen.Add(K);
			}
		}
	}
	if (Klassen.Num() == 0)
	{
		StoppeStress(TDF(TEXT("Stresstest abgebrochen: Art '%s' unbekannt. Moeglich: mix, %s"), TEXT("Stress test aborted: type '%s' unknown. Possible: mix, %s"), *mStressArt, *FString::Join(Namen, TEXT(", ")).ToLower()));
		return;
	}
	const int32 Vorher = mGegner.Num();
	const double Start = FPlatformTime::Seconds();
	// in Gruppen zu 10 wie eine Welle (je Gruppe ein Weg), alles im selben Bild - misst auch den Spawn-Ruckler
	for (int32 i = 0, k = 0; i < N; i += 10, ++k)
	{
		SpawneGruppe(Klassen[k % Klassen.Num()], FMath::Min(10, N - i), Mitte, mFronten[0].Ort, mStressLP);
	}
	const double SpawnMs = (FPlatformTime::Seconds() - Start) * 1000.0;
	for (int32 i = Vorher; i < mGegner.Num(); ++i)
	{
		mStressGegner.Add(mGegner[i].Kreatur);
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Stresstest %d Gegner (%s): %d gespawnt in %.0f ms an %s"), N, *mStressArt, mStressGegner.Num(), SpawnMs, *FrontText(mFronten, 0, true));
	MeldeKreaturen(FString::Printf(TEXT("Stresstest %d Gegner, Start"), N));
	Ansage(TDF(TEXT("Stresstest Stufe %d/%d: %d Gegner (Spawn %.0f ms), Messung %.0f s ..."), TEXT("Stress test stage %d/%d: %d enemies (spawn %.0f ms), measuring %.0f s ..."), mStressIndex + 1, mStressStufen.Num(), mStressGegner.Num(), SpawnMs, mStressSekunden));
	StarteBildMessung();
	mStressPhase = 1;
	mStressEnde = FPlatformTime::Seconds() + mStressSekunden;
}

TStatId UTDWaveSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTDWaveSubsystem, STATGROUP_Tickables);
}

bool UTDWaveSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
