#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "WelleCommand.generated.h"

/**
 * /welle - Machbarkeitstest fuer Gegnerwellen.
 *   /welle liste [Filter]                    verfuegbare Kreaturen
 *   /welle <Typ> [Anzahl] [Abstand_m] [ziel]  Welle spawnen; "ziel" = angeschautes Objekt statt Spieler
 *   /welle status | stop | modus aggro|laufen
 */
UCLASS()
class FICSITTD_API AWelleCommand : public AChatCommandInstance
{
	GENERATED_BODY()
public:
	AWelleCommand();
	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};
