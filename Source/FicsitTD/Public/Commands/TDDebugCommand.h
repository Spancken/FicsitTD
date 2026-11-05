#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "TDDebugCommand.generated.h"

/**
 * /td umgebung [Radius_m] - listet alle Actors um den Spieler mit Klasse, Abstand und sichtbaren Bauteilen
 * (in Chat und Log), um unerklaerliche Objekte wie den "schwarzen Schatten" zu identifizieren.
 */
UCLASS()
class FICSITTD_API ATDDebugCommand : public AChatCommandInstance
{
	GENERATED_BODY()
public:
	ATDDebugCommand();
	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};
