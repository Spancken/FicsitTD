#include "Commands/WelleCommand.h"
#include "Command/CommandSender.h"
#include "Creature/FGCreature.h"
#include "Engine/World.h"
#include "FGPlayerController.h"
#include "TDTestWall.h"
#include "TDWaveSubsystem.h"

AWelleCommand::AWelleCommand()
{
	CommandName = TEXT("welle");
	Aliases.Add(TEXT("wave"));
	Usage = NSLOCTEXT("FicsitTD", "Welle.Usage", "/welle liste [Filter] | /welle <Typ> [Anzahl] [Abstand_m] [ziel|mauer] | /welle status | stop | modus aggro|laufen");
	MinNumberOfArguments = 1;
	bOnlyUsableByPlayer = true;
}

/** Kurzname fuer die Anzeige: Char_Hog_C -> Hog */
static FString KurzName(const FSoftClassPath& Path)
{
	FString Name = Path.GetAssetName();
	Name.RemoveFromStart(TEXT("Char_"));
	Name.RemoveFromEnd(TEXT("_C"));
	return Name;
}

/** Objekt, auf das der Spieler schaut (bis 200 m), ausser Landschaft. */
static AActor* AngeschautesObjekt(AFGPlayerController* Player)
{
	FVector Origin;
	FRotator Direction;
	Player->GetPlayerViewPoint(Origin, Direction);
	FHitResult Hit;
	FCollisionQueryParams Query(TEXT("FicsitTDZiel"), true, Player->GetPawn());
	if (Player->GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction.Vector() * 20000.f, ECC_Visibility, Query))
	{
		return Hit.GetActor();
	}
	return nullptr;
}

EExecutionStatus AWelleCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	AFGPlayerController* Player = Sender->GetPlayer();
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Waves || !Pawn)
	{
		Sender->SendChatMessage(TEXT("Wellen-System oder Spielfigur nicht verfuegbar."), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}
	const FString Sub = Arguments[0].ToLower();

	if (Sub == TEXT("status"))
	{
		for (const FString& Line : Waves->GetStatusLines())
		{
			Sender->SendChatMessage(Line);
		}
		return EExecutionStatus::COMPLETED;
	}
	if (Sub == TEXT("stop"))
	{
		Sender->SendChatMessage(FString::Printf(TEXT("%d Gegner entfernt."), Waves->StopAll()));
		return EExecutionStatus::COMPLETED;
	}
	if (Sub == TEXT("modus"))
	{
		const bool bLaufen = Arguments.Num() > 1 && Arguments[1].Equals(TEXT("laufen"), ESearchCase::IgnoreCase);
		Waves->Modus = bLaufen ? ETDSteuerModus::Laufen : ETDSteuerModus::Aggro;
		Sender->SendChatMessage(FString::Printf(TEXT("Steuermodus: %s"), bLaufen ? TEXT("laufen (Aggro + MoveTo)") : TEXT("aggro (nur KI-Ziel)")));
		return EExecutionStatus::COMPLETED;
	}

	const TArray<FSoftClassPath> Classes = UTDWaveSubsystem::FindCreatureClasses();
	if (Sub == TEXT("liste"))
	{
		const FString Filter = Arguments.Num() > 1 ? Arguments[1] : FString();
		TArray<FString> Names;
		for (const FSoftClassPath& Path : Classes)
		{
			const FString Name = KurzName(Path);
			if (Filter.IsEmpty() || Name.Contains(Filter))
			{
				Names.Add(Name);
			}
		}
		Sender->SendChatMessage(FString::Printf(TEXT("%d Kreaturen: %s"), Names.Num(), *FString::Join(Names, TEXT(", "))));
		return EExecutionStatus::COMPLETED;
	}

	// Typ: exakter Kurzname zuerst, sonst erster Treffer, der den Text enthaelt
	const FSoftClassPath* Found = Classes.FindByPredicate([&](const FSoftClassPath& P) { return KurzName(P).Equals(Arguments[0], ESearchCase::IgnoreCase); });
	if (!Found)
	{
		Found = Classes.FindByPredicate([&](const FSoftClassPath& P) { return KurzName(P).Contains(Arguments[0]); });
	}
	if (!Found)
	{
		Sender->SendChatMessage(FString::Printf(TEXT("Keine Kreatur \"%s\" gefunden. /welle liste zeigt alle."), *Arguments[0]), FLinearColor::Red);
		return EExecutionStatus::BAD_ARGUMENTS;
	}
	UClass* Loaded = Found->TryLoadClass<AFGCreature>();
	if (!Loaded)
	{
		Sender->SendChatMessage(FString::Printf(TEXT("Klasse %s liess sich nicht laden."), *Found->ToString()), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}

	const int32 Count = Arguments.Num() > 1 ? FMath::Clamp(FCString::Atoi(*Arguments[1]), 1, 50) : 5;
	const float DistanceM = Arguments.Num() > 2 ? FMath::Clamp(FCString::Atof(*Arguments[2]), 5.f, 500.f) : 40.f;

	if (Arguments.Contains(TEXT("mauer")))
	{
		const AActor* Wall = Waves->FindNearestDefense(Pawn->GetActorLocation());
		if (!Wall)
		{
			Sender->SendChatMessage(TEXT("Keine Mauer vorhanden. Verteidigungsmauer bauen oder /mauer setzen."), FLinearColor::Red);
			return EExecutionStatus::BAD_ARGUMENTS;
		}
		const int32 Spawned = Waves->SpawnWave(Loaded, Count, DistanceM * 100.f, Wall->GetActorLocation(), nullptr);
		Sender->SendChatMessage(FString::Printf(TEXT("%d x %s gespawnt, %.0f m von der Mauer, Ziel: Mauer-Koeder. /mauer status zeigt die Lebenspunkte."),
			Spawned, *KurzName(*Found), DistanceM));
		return Spawned > 0 ? EExecutionStatus::COMPLETED : EExecutionStatus::UNCOMPLETED;
	}

	AActor* Target = Pawn;
	if (Arguments.Contains(TEXT("ziel")))
	{
		Target = AngeschautesObjekt(Player);
		if (!Target)
		{
			Sender->SendChatMessage(TEXT("Kein Objekt im Blick (max. 200 m). Schau auf ein Gebaeude und versuch es nochmal."), FLinearColor::Red);
			return EExecutionStatus::BAD_ARGUMENTS;
		}
	}

	const int32 Spawned = Waves->SpawnWave(Loaded, Count, DistanceM * 100.f, Target->GetActorLocation(), Target);
	Sender->SendChatMessage(FString::Printf(TEXT("%d x %s gespawnt, %.0f m entfernt, Ziel: %s. /welle status zeigt den Verlauf."),
		Spawned, *KurzName(*Found), DistanceM, Target == Pawn ? TEXT("du") : *Target->GetName()));
	return Spawned > 0 ? EExecutionStatus::COMPLETED : EExecutionStatus::UNCOMPLETED;
}
