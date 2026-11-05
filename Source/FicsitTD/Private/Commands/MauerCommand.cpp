#include "Commands/MauerCommand.h"
#include "Command/CommandSender.h"
#include "Engine/World.h"
#include "FGPlayerController.h"
#include "TDTestWall.h"
#include "TDWaveSubsystem.h"

AMauerCommand::AMauerCommand()
{
	CommandName = TEXT("mauer");
	Aliases.Add(TEXT("wall"));
	Usage = NSLOCTEXT("FicsitTD", "Mauer.Usage", "/mauer [LP] | /mauer status | /mauer weg");
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer = true;
}

EExecutionStatus AMauerCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	AFGPlayerController* Player = Sender->GetPlayer();
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!Waves || !Pawn)
	{
		Sender->SendChatMessage(TEXT("Wellen-System oder Spielfigur nicht verfuegbar."), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}
	const FString Sub = Arguments.Num() > 0 ? Arguments[0].ToLower() : FString();

	if (Sub == TEXT("status"))
	{
		const TArray<FString> Lines = Waves->GetWallStatusLines();
		if (Lines.Num() == 0)
		{
			Sender->SendChatMessage(TEXT("Keine Testmauern."));
		}
		for (const FString& Line : Lines)
		{
			Sender->SendChatMessage(Line);
		}
		return EExecutionStatus::COMPLETED;
	}
	if (Sub == TEXT("weg"))
	{
		Sender->SendChatMessage(FString::Printf(TEXT("%d Testmauern entfernt."), Waves->RemoveWalls()));
		return EExecutionStatus::COMPLETED;
	}

	const float MaxHealth = Sub.IsEmpty() ? 1000.f : FMath::Clamp(FCString::Atof(*Sub), 1.f, 1000000.f);

	// 12 m vor dem Spieler, Wandflaeche zum Spieler gedreht, auf den Boden gesetzt
	const FRotator Blick(0.f, Pawn->GetActorRotation().Yaw, 0.f);
	FVector Location = Pawn->GetActorLocation() + Blick.Vector() * 1200.f;
	FHitResult Hit;
	FCollisionQueryParams Query(TEXT("FicsitTDMauer"), false, Pawn);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Location + FVector(0, 0, 3000.f), Location - FVector(0, 0, 10000.f), ECC_Visibility, Query))
	{
		Location = Hit.ImpactPoint;
	}
	// Wandlaenge (X) quer zur Blickrichtung
	const FRotator WandRotation(0.f, Blick.Yaw + 90.f, 0.f);
	ATDTestWall* Wall = Waves->SpawnWall(Location, WandRotation, MaxHealth);
	if (!Wall)
	{
		Sender->SendChatMessage(TEXT("Mauer konnte nicht gesetzt werden."), FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}
	Sender->SendChatMessage(FString::Printf(TEXT("Testmauer %s gesetzt: %.0f LP. Jetzt z. B. /welle Hog 3 40 mauer"), *Wall->GetName(), MaxHealth));
	return EExecutionStatus::COMPLETED;
}
