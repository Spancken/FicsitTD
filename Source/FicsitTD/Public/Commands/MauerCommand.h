#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "MauerCommand.generated.h"

/**
 * /mauer - Testmauer mit Lebenspunkten und Koedern.
 *   /mauer [LP]      Mauer 12 m vor dir setzen (Standard 1000 LP)
 *   /mauer status    Lebenspunkte und Treffer je Gegnertyp
 *   /mauer weg       alle Testmauern entfernen
 */
UCLASS()
class FICSITTD_API AMauerCommand : public AChatCommandInstance
{
	GENERATED_BODY()
public:
	AMauerCommand();
	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};
