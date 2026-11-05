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
#include "Buildables/FGBuildableConveyorBase.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPlayerState.h"
#include "FGRecipeShortcut.h"
#include "FGRecipeManager.h"
#include "GameFramework/GameStateBase.h"

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
	// 0.78.4: alte Spielstaende - Schnellleiste auf die hoechste freigeschaltete Bandstufe (Mk2 oder Mk3)
	if (AFGRecipeManager* Rezepte = AFGRecipeManager::Get(GetWorld()))
	{
		for (int32 Stufe = 3; Stufe >= 2; --Stufe)
		{
			UClass* Rezept = LoadClass<UFGRecipe>(nullptr, *FString::Printf(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk%d.Recipe_ConveyorBeltMk%d_C"), Stufe, Stufe));
			if (Rezept && Rezepte->IsRecipeAvailable(Rezept))
			{
				SchnellleisteAufStufe(GetWorld(), Stufe);
				break;
			}
		}
	}
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
	// Selbsttest 19:53: an noch nicht fertigen Teilen gesetzte Verbindungen gingen verloren (54 offene Enden) -> erst fertigstellen
	for (const TPair<AFGBuildable*, AFGBuildable*>& P : Neue)
	{
		P.Value->FinishSpawning(P.Value->GetActorTransform());
	}
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
	// Bandketten neu bilden: das Spiel fasst verbundene Baender beim Anmelden zu Ketten zusammen - die neuen Teile wurden
	// ohne Verbindungen angemeldet, also ab- und mit Verbindungen wieder anmelden
	for (const TPair<AFGBuildable*, AFGBuildable*>& P : Neue)
	{
		if (AFGBuildableConveyorBase* Foerderer = Cast<AFGBuildableConveyorBase>(P.Value))
		{
			Bau->RemoveConveyor(Foerderer);
			Bau->AddConveyor(Foerderer);
		}
	}
	int32 B = 0, L = 0;
	for (const TPair<AFGBuildable*, AFGBuildable*>& P : Neue)
	{
		IFGDismantleInterface::Execute_Upgrade(P.Key, P.Value);
		(P.Key->IsA<AFGBuildableConveyorLift>() ? L : B)++;
		P.Key->Destroy();
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Band-Upgrade Mk%d: %d Verbindungen gemerkt, %d wiederhergestellt (je Paar einmal), %d ohne Gegenstueck"),
		Stufe, Verbindungen.Num(), Verbunden, Fehlt);
	const int32 Plaetze = SchnellleisteAufStufe(World, Stufe);
	UE_LOG(LogFicsitTD, Log, TEXT("Band-Upgrade Mk%d: %d Baender, %d Lifte aufgeruestet, %d Plaetze der Schnellleiste umgestellt"), Stufe, B, L, Plaetze);
	return FString::Printf(TEXT("FICSIT-Logistik: %d Foerderbaender und %d Lifte automatisch auf Mk%d aufgeruestet."), B, L, Stufe);
}


/** 0.78.4: wirkliche Richtung eines Endes. Band- und Lift-Enden heissen "beliebig" (FCD_ANY), sind aber gerichtet:
 *  Ende 0 ist der Eingang, Ende 1 der Ausgang (FGBuildableConveyorBase.h). Die Reparatur 0.78.3 nahm "beliebig" woertlich
 *  und haengte an Splittern Eingang an Eingang (Marius 05.11. abends). */
static EFactoryConnectionDirection TDRichtung(const UFGFactoryConnectionComponent* E)
{
	if (!E)
	{
		return EFactoryConnectionDirection::FCD_MAX;
	}
	if (const AFGBuildableConveyorBase* F = Cast<AFGBuildableConveyorBase>(E->GetOwner()))
	{
		if (E == F->GetConnection0())
		{
			return EFactoryConnectionDirection::FCD_INPUT;
		}
		if (E == F->GetConnection1())
		{
			return EFactoryConnectionDirection::FCD_OUTPUT;
		}
	}
	return E->GetDirection();
}

/** passen zwei Enden zusammen? Eingang an Ausgang (oder an ein wirklich beliebiges Ende eines Gebaeudes), nie an Pfeiler */
static bool TDPasst(const UFGFactoryConnectionComponent* A, const UFGFactoryConnectionComponent* G)
{
	const EFactoryConnectionDirection RA = TDRichtung(A), RG = TDRichtung(G);
	if (RA == EFactoryConnectionDirection::FCD_SNAP_ONLY || RG == EFactoryConnectionDirection::FCD_SNAP_ONLY)
	{
		return false;
	}
	if (RA == EFactoryConnectionDirection::FCD_ANY || RG == EFactoryConnectionDirection::FCD_ANY)
	{
		return true;
	}
	return (RA == EFactoryConnectionDirection::FCD_INPUT && RG == EFactoryConnectionDirection::FCD_OUTPUT)
		|| (RA == EFactoryConnectionDirection::FCD_OUTPUT && RG == EFactoryConnectionDirection::FCD_INPUT);
}

int32 UTDBandUpgrade::VerbindeOffeneEnden(UWorld* World, bool bNurZaehlen, FString* OutBericht)
{
	int32 Falsch = 0, Geloest = 0, Offen = 0, Verbunden = 0;
	TArray<FString> Beispiele;
	auto IstFoerderer = [](const AActor* A) { return A && (A->IsA<AFGBuildableConveyorBelt>() || A->IsA<AFGBuildableConveyorLift>()); };
	// 1. falsch herum verbundene Enden (Eingang an Eingang, Ausgang an Ausgang, Pfeiler) finden und loesen
	for (TActorIterator<AFGBuildable> It(World); It; ++It)
	{
		if (!IstFoerderer(*It))
		{
			continue;
		}
		TInlineComponentArray<UFGFactoryConnectionComponent*> Enden(*It);
		for (UFGFactoryConnectionComponent* E : Enden)
		{
			UFGFactoryConnectionComponent* G = E->GetConnection();
			if (!G || TDPasst(E, G))
			{
				continue;
			}
			++Falsch;
			if (Beispiele.Num() < 5)
			{
				Beispiele.Add(FString::Printf(TEXT("FALSCH %s/%s an %s/%s bei %s"), *It->GetName(), *E->GetName(), *G->GetOwner()->GetName(), *G->GetName(), *E->GetComponentLocation().ToCompactString()));
			}
			if (!bNurZaehlen)
			{
				E->ClearConnection();
				++Geloest;
			}
		}
	}
	// 2. offene Enden mit dem naechsten passenden, zugewandten Gegenstueck verbinden (wie das Einrasten beim Bauen)
	for (TActorIterator<AFGBuildable> It(World); It; ++It)
	{
		if (!IstFoerderer(*It))
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
			TArray<UFGFactoryConnectionComponent*> Kandidaten;
			UFGFactoryConnectionComponent::FindAllOverlappingConnections(Kandidaten, World, E->GetComponentLocation(), 50.f, EFactoryConnectionDirection::FCD_ANY);
			UFGFactoryConnectionComponent* G = nullptr;
			float Bester = TNumericLimits<float>::Max();
			for (UFGFactoryConnectionComponent* K : Kandidaten)
			{
				// zugewandt: die Enden zeigen gegeneinander (Bauvorgaben des Spiels: Anschluesse stehen sich gegenueber)
				if (!K || K == E || K->GetOwner() == *It || K->IsConnected() || !TDPasst(E, K)
					|| FVector::DotProduct(E->GetForwardVector(), K->GetForwardVector()) > -0.7f)
				{
					continue;
				}
				const float D = FVector::DistSquared(E->GetComponentLocation(), K->GetComponentLocation());
				if (D < Bester)
				{
					Bester = D;
					G = K;
				}
			}
			if (!G)
			{
				continue;
			}
			++Offen;
			if (Beispiele.Num() < 5)
			{
				Beispiele.Add(FString::Printf(TEXT("offen %s/%s -> %s/%s bei %s"), *It->GetName(), *E->GetName(), *G->GetOwner()->GetName(), *G->GetName(), *E->GetComponentLocation().ToCompactString()));
			}
			if (!bNurZaehlen)
			{
				E->SetConnection(G);
				++Verbunden;
			}
		}
	}
	const FString Bericht = bNurZaehlen
		? FString::Printf(TEXT("%d falsch herum verbundene und %d offene Band-Enden mit Gegenstueck%s"), Falsch, Offen,
			Beispiele.Num() ? *(TEXT(": ") + FString::Join(Beispiele, TEXT("; "))) : TEXT(""))
		: FString::Printf(TEXT("%d falsch herum verbundene Enden geloest, %d offene Enden verbunden. Jetzt speichern und den Spielstand neu laden, dann baut das Spiel die Bandketten neu auf."), Geloest, Verbunden);
	UE_LOG(LogFicsitTD, Log, TEXT("Band-Verbindungen: %s"), *Bericht);
	if (OutBericht)
	{
		*OutBericht = Bericht;
	}
	return Falsch + Offen;
}

int32 UTDBandUpgrade::SchnellleisteAufStufe(UWorld* World, int32 Stufe)
{
	UClass* Neu[2] = {
		LoadClass<UFGRecipe>(nullptr, *FString::Printf(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk%d.Recipe_ConveyorBeltMk%d_C"), Stufe, Stufe)),
		LoadClass<UFGRecipe>(nullptr, *FString::Printf(TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk%d.Recipe_ConveyorLiftMk%d_C"), Stufe, Stufe)) };
	const AGameStateBase* Zustand = World ? World->GetGameState() : nullptr;
	if (!Zustand)
	{
		return 0;
	}
	int32 Umgestellt = 0;
	for (APlayerState* PS : Zustand->PlayerArray)
	{
		AFGPlayerState* Spieler = Cast<AFGPlayerState>(PS);
		for (int32 h = 0; Spieler && h < Spieler->GetNumHotbars(); ++h)
		{
			UFGPlayerHotbar* Leiste = Spieler->GetHotbarAtIndex(h);
			for (int32 i = 0; Leiste && i < Leiste->GetNumShortcuts(); ++i)
			{
				const UFGRecipeShortcut* Platz = Cast<UFGRecipeShortcut>(Leiste->GetShortcutAtIndex(i));
				const UClass* Rezept = Platz ? Platz->GetRecipe().Get() : nullptr;
				const FString Name = Rezept ? Rezept->GetName() : FString();
				const bool bBand = Name.StartsWith(TEXT("Recipe_ConveyorBeltMk")), bLift = Name.StartsWith(TEXT("Recipe_ConveyorLiftMk"));
				if ((!bBand && !bLift) || TDBandStufe(Rezept) >= Stufe || !Neu[bBand ? 0 : 1])
				{
					continue;
				}
				Leiste->SetRecipeShortcutAtIndex(i, Neu[bBand ? 0 : 1], true);
				++Umgestellt;
			}
		}
	}
	if (Umgestellt)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Schnellleiste: %d Band-/Lift-Plaetze auf Mk%d umgestellt"), Umgestellt, Stufe);
	}
	return Umgestellt;
}
