#include "TDBandUpgrade.h"
#include "TDForschung.h"
#include "FicsitTDModule.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "FGBuildableSubsystem.h"
#include "FGRecipe.h"
#include "FGChatManager.h"
#include "FGDismantleInterface.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"

UTDBandUpgrade* UTDBandUpgrade::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTDBandUpgrade>() : nullptr;
}

void UTDBandUpgrade::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	// der Schematic-Manager steht erst nach dem Laden bereit
	InWorld.GetTimerManager().SetTimer(mTimer, this, &UTDBandUpgrade::Verbinde, 2.f, true, 3.f);
}

void UTDBandUpgrade::Verbinde()
{
	AFGSchematicManager* Schemata = AFGSchematicManager::Get(GetWorld());
	if (!Schemata || mVerbunden)
	{
		return;
	}
	Schemata->PurchasedSchematicDelegate.AddDynamic(this, &UTDBandUpgrade::Erforscht);
	mVerbunden = true;
	TDForschung::GibStilleRezepte(this);
	GetWorld()->GetTimerManager().ClearTimer(mTimer);
}

void UTDBandUpgrade::Erforscht(TSubclassOf<UFGSchematic> Schematic)
{
	const FString Pfad = Schematic ? Schematic->GetPathName() : FString();
	if (Pfad.StartsWith(TEXT("/FicsitTD/Research/")))
	{
		TDForschung::GibStilleRezepte(this);
	}
	const int32 Stufe = Pfad.Contains(TEXT("Research_TD_BandMk3")) ? 3 : Pfad.Contains(TEXT("Research_TD_BandMk2")) ? 2 : 0;
	if (Stufe == 0)
	{
		return;
	}
	// kurz warten, bis die Freischaltung (Rezepte) durch ist
	FTimerHandle Einmal;
	GetWorld()->GetTimerManager().SetTimer(Einmal, FTimerDelegate::CreateWeakLambda(this, [this, Stufe]()
	{
		const FString Bericht = RuesteUm(Stufe);
		if (AFGChatManager* Chat = AFGChatManager::Get(GetWorld()))
		{
			FChatMessageStruct N;
			N.MessageText = FText::FromString(Bericht);
			N.MessageType = EFGChatMessageType::CMT_CustomMessage;
			N.MessageSender = FText::FromString(TEXT("FICSIT"));
			N.MessageSenderColor = FLinearColor(1.f, 0.55f, 0.1f);
			Chat->BroadcastChatMessage(N);
		}
	}), 1.f, false);
}

/** Stufe aus dem Klassennamen (Build_ConveyorBeltMk2_C -> 2) */
static int32 TDBandStufe(const UClass* Klasse)
{
	const FString Name = Klasse ? Klasse->GetName() : FString();
	const int32 Mk = Name.Find(TEXT("Mk"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	return Mk != INDEX_NONE ? FCString::Atoi(*Name.Mid(Mk + 2)) : 0;
}


static void TDKopiereFeld(UClass* Klasse, const TCHAR* Feld, UObject* Von, UObject* Nach)
{
	if (FProperty* P = FindFProperty<FProperty>(Klasse, Feld))
	{
		P->CopyCompleteValue_InContainer(Nach, Von);
	}
}

FString UTDBandUpgrade::RuesteUm(int32 Stufe)
{
	UWorld* World = GetWorld();
	AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(World);
	const FString BandPfad = FString::Printf(TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk%d/Build_ConveyorBeltMk%d.Build_ConveyorBeltMk%d_C"), Stufe, Stufe, Stufe);
	const FString LiftPfad = FString::Printf(TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorLiftMk%d/Build_ConveyorLiftMk%d.Build_ConveyorLiftMk%d_C"), Stufe, Stufe, Stufe);
	UClass* BandKlasse = LoadClass<AFGBuildableConveyorBelt>(nullptr, *BandPfad);
	UClass* LiftKlasse = LoadClass<AFGBuildableConveyorLift>(nullptr, *LiftPfad);
	UClass* BandRezept = LoadClass<UFGRecipe>(nullptr, *FString::Printf(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk%d.Recipe_ConveyorBeltMk%d_C"), Stufe, Stufe));
	UClass* LiftRezept = LoadClass<UFGRecipe>(nullptr, *FString::Printf(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk%d.Recipe_ConveyorLiftMk%d_C"), Stufe, Stufe));
	if (!Bau || !BandKlasse)
	{
		return FString::Printf(TEXT("Baender Mk%d: Klasse nicht gefunden, nichts aufgeruestet."), Stufe);
	}
	TArray<AFGBuildableConveyorBelt*> Baender;
	TArray<AFGBuildableConveyorLift*> Lifte;
	for (TActorIterator<AFGBuildableConveyorBelt> It(World); It; ++It)
	{
		if (TDBandStufe(It->GetClass()) < Stufe)
		{
			Baender.Add(*It);
		}
	}
	for (TActorIterator<AFGBuildableConveyorLift> It(World); It; ++It)
	{
		if (LiftKlasse && TDBandStufe(It->GetClass()) < Stufe)
		{
			Lifte.Add(*It);
		}
	}
	int32 B = 0, L = 0;
	for (AFGBuildableConveyorBelt* Alt : Baender)
	{
		const FTransform T = Alt->GetActorTransform();
		IFGDismantleInterface::Execute_PreUpgrade(Alt);
		AFGBuildableConveyorBelt* Neu = Cast<AFGBuildableConveyorBelt>(Bau->BeginSpawnBuildable(BandKlasse, T));
		if (!Neu)
		{
			continue;
		}
		*Neu->GetMutableSplinePointData() = Alt->GetSplinePointData();
		Neu->SetBuiltWithRecipe(BandRezept);
		Neu->SetCustomizationData_Native(Alt->GetCustomizationData_Implementation());
		Neu->FinishSpawning(T);
		IFGDismantleInterface::Execute_Upgrade(Alt, Neu);
		Alt->Destroy();
		++B;
	}
	for (AFGBuildableConveyorLift* Alt : Lifte)
	{
		const FTransform T = Alt->GetActorTransform();
		IFGDismantleInterface::Execute_PreUpgrade(Alt);
		AFGBuildableConveyorLift* Neu = Cast<AFGBuildableConveyorLift>(Bau->BeginSpawnBuildable(LiftKlasse, T));
		if (!Neu)
		{
			continue;
		}
		// Hoehe, Richtung und Durchfuehrungen sind geschuetzte Felder -> ueber die Reflexion kopieren
		for (const TCHAR* Feld : { TEXT("mTopTransform"), TEXT("mIsReversed"), TEXT("mIsBeltUsingInputRotation"), TEXT("mSnappedPassthroughs") })
		{
			TDKopiereFeld(AFGBuildableConveyorLift::StaticClass(), Feld, Alt, Neu);
		}
		Neu->SetBuiltWithRecipe(LiftRezept);
		Neu->SetCustomizationData_Native(Alt->GetCustomizationData_Implementation());
		Neu->FinishSpawning(T);
		IFGDismantleInterface::Execute_Upgrade(Alt, Neu);
		Alt->Destroy();
		++L;
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Band-Upgrade Mk%d: %d Baender, %d Lifte aufgeruestet"), Stufe, B, L);
	return FString::Printf(TEXT("FICSIT-Logistik: %d Foerderbaender und %d Lifte automatisch auf Mk%d aufgeruestet."), B, L, Stufe);
}
