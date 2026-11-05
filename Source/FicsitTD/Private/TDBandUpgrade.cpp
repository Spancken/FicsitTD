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
#include "FGFactoryConnectionComponent.h"

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
	// 0.78.3 (Marius 05.11.: nach dem Aufruesten hingen Baender nicht mehr an Gebaeuden): vorher wurde jedes Band einzeln ersetzt und
	// das alte abgerissen - der Abriss loeste dabei die Verbindung des Nachbarn, der schon am neuen Band hing. Jetzt wie der Bau-Assistent:
	// 1. alle Verbindungen merken und loesen, 2. alle neuen Teile aufsetzen (noch nicht fertig), 3. neu verbinden, 4. fertigstellen,
	// 5. Gegenstaende uebergeben und die alten abreissen (sie haben keine Verbindungen mehr)
	struct FVerbindung { AFGBuildable* Eigner; FName Name; AFGBuildable* Partner; FName PartnerName; };
	TArray<AFGBuildable*> Alte;
	Alte.Append(Baender);
	Alte.Append(Lifte);
	TArray<FVerbindung> Verbindungen;
	for (AFGBuildable* Alt : Alte)
	{
		TInlineComponentArray<UFGFactoryConnectionComponent*> Enden(Alt);
		for (UFGFactoryConnectionComponent* E : Enden)
		{
			if (UFGFactoryConnectionComponent* G = E->GetConnection())
			{
				Verbindungen.Add({ Alt, E->GetFName(), Cast<AFGBuildable>(G->GetOwner()), G->GetFName() });
			}
		}
	}
	for (AFGBuildable* Alt : Alte)
	{
		IFGDismantleInterface::Execute_PreUpgrade(Alt);
		TInlineComponentArray<UFGFactoryConnectionComponent*> Enden(Alt);
		for (UFGFactoryConnectionComponent* E : Enden)
		{
			E->ClearConnection();
		}
	}
	TMap<AFGBuildable*, AFGBuildable*> Neue;
	for (AFGBuildableConveyorBelt* Alt : Baender)
	{
		const FTransform T = Alt->GetActorTransform();
		if (AFGBuildableConveyorBelt* Neu = Cast<AFGBuildableConveyorBelt>(Bau->BeginSpawnBuildable(BandKlasse, T)))
		{
			*Neu->GetMutableSplinePointData() = Alt->GetSplinePointData();
			Neu->SetBuiltWithRecipe(BandRezept);
			Neu->SetCustomizationData_Native(Alt->GetCustomizationData_Implementation());
			Neue.Add(Alt, Neu);
		}
	}
	for (AFGBuildableConveyorLift* Alt : Lifte)
	{
		const FTransform T = Alt->GetActorTransform();
		if (AFGBuildableConveyorLift* Neu = Cast<AFGBuildableConveyorLift>(Bau->BeginSpawnBuildable(LiftKlasse, T)))
		{
			// Hoehe, Richtung und Durchfuehrungen sind geschuetzte Felder -> ueber die Reflexion kopieren
			for (const TCHAR* Feld : { TEXT("mTopTransform"), TEXT("mIsReversed"), TEXT("mIsBeltUsingInputRotation"), TEXT("mSnappedPassthroughs") })
			{
				TDKopiereFeld(AFGBuildableConveyorLift::StaticClass(), Feld, Alt, Neu);
			}
			Neu->SetBuiltWithRecipe(LiftRezept);
			Neu->SetCustomizationData_Native(Alt->GetCustomizationData_Implementation());
			Neue.Add(Alt, Neu);
		}
	}
	auto Ende = [](AFGBuildable* Eigner, FName Name) -> UFGFactoryConnectionComponent*
	{
		TInlineComponentArray<UFGFactoryConnectionComponent*> Enden(Eigner);
		for (UFGFactoryConnectionComponent* E : Enden)
		{
			if (E->GetFName() == Name)
			{
				return E;
			}
		}
		return nullptr;
	};
	int32 Verbunden = 0, Fehlt = 0;
	for (const FVerbindung& V : Verbindungen)
	{
		AFGBuildable* const* NeuA = Neue.Find(V.Eigner);
		AFGBuildable* const* NeuB = V.Partner ? Neue.Find(V.Partner) : nullptr;
		UFGFactoryConnectionComponent* A = NeuA ? Ende(*NeuA, V.Name) : nullptr;
		UFGFactoryConnectionComponent* G = V.Partner ? Ende(NeuB ? *NeuB : V.Partner, V.PartnerName) : nullptr;
		if (!A || !G)
		{
			++Fehlt;
			continue;
		}
		if (!A->IsConnected() && !G->IsConnected())
		{
			A->SetConnection(G);
			++Verbunden;
		}
	}
	int32 B = 0, L = 0;
	for (const TPair<AFGBuildable*, AFGBuildable*>& P : Neue)
	{
		P.Value->FinishSpawning(P.Value->GetActorTransform());
	}
	for (const TPair<AFGBuildable*, AFGBuildable*>& P : Neue)
	{
		IFGDismantleInterface::Execute_Upgrade(P.Key, P.Value);
		(P.Key->IsA<AFGBuildableConveyorLift>() ? L : B)++;
		P.Key->Destroy();
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Band-Upgrade Mk%d: %d Verbindungen gemerkt, %d wiederhergestellt (je Paar einmal), %d ohne Gegenstueck"),
		Stufe, Verbindungen.Num(), Verbunden, Fehlt);
	UE_LOG(LogFicsitTD, Log, TEXT("Band-Upgrade Mk%d: %d Baender, %d Lifte aufgeruestet"), Stufe, B, L);
	return FString::Printf(TEXT("FICSIT-Logistik: %d Foerderbaender und %d Lifte automatisch auf Mk%d aufgeruestet."), B, L, Stufe);
}


int32 UTDBandUpgrade::VerbindeOffeneEnden(UWorld* World, bool bNurZaehlen, FString* OutBericht)
{
	int32 Offen = 0, Verbunden = 0;
	TArray<FString> Beispiele;
	for (TActorIterator<AFGBuildable> It(World); It; ++It)
	{
		if (!It->IsA<AFGBuildableConveyorBelt>() && !It->IsA<AFGBuildableConveyorLift>())
		{
			continue;
		}
		TInlineComponentArray<UFGFactoryConnectionComponent*> Enden(*It);
		for (UFGFactoryConnectionComponent* E : Enden)
		{
			if (E->IsConnected())
			{
				continue;
			}
			// dieselbe Suche wie beim Einrasten im Bau-Assistenten: passendes Ende (Richtung, Art) an derselben Stelle
			UFGFactoryConnectionComponent* G = UFGFactoryConnectionComponent::FindCompatibleOverlappingConnections(E, E->GetComponentLocation(), nullptr, 50.f);
			if (!G || G->IsConnected() || G->GetOwner() == *It)
			{
				continue;
			}
			++Offen;
			if (Beispiele.Num() < 5)
			{
				Beispiele.Add(FString::Printf(TEXT("%s/%s -> %s/%s bei %s"), *It->GetName(), *E->GetName(), *G->GetOwner()->GetName(), *G->GetName(), *E->GetComponentLocation().ToCompactString()));
			}
			if (!bNurZaehlen)
			{
				E->SetConnection(G);
				++Verbunden;
			}
		}
	}
	const FString Bericht = bNurZaehlen
		? FString::Printf(TEXT("%d offene Band-Enden mit Gegenstueck%s"), Offen, Beispiele.Num() ? *(TEXT(": ") + FString::Join(Beispiele, TEXT("; "))) : TEXT(""))
		: FString::Printf(TEXT("%d offene Band-Enden wieder verbunden. Jetzt speichern und den Spielstand neu laden, dann baut das Spiel die Bandketten neu auf."), Verbunden);
	UE_LOG(LogFicsitTD, Log, TEXT("Band-Verbindungen: %s"), *Bericht);
	if (OutBericht)
	{
		*OutBericht = Bericht;
	}
	return Offen;
}
