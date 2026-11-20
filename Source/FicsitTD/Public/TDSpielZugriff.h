// 19.11.2026 (Spielupdate 1.2.5.1): AFGGameState ist im Spiel 128 Byte groesser als in den Headern (CL517554).
// Inline-Getter wie IsCreativeModeEnabled()/GetSessionName() lesen dann falsche Felder. Diese Helfer lesen
// ueber die Reflexion des laufenden Spiels und sind damit vom Header-Layout unabhaengig.
#pragma once

#include "CoreMinimal.h"
#include "FGGameState.h"
#include "UObject/UnrealType.h"

inline bool TDKreativModusAn(const AFGGameState* Spiel)
{
	static FBoolProperty* P = FindFProperty<FBoolProperty>(AFGGameState::StaticClass(), TEXT("mIsCreativeModeEnabled"));
	return Spiel && P && P->GetPropertyValue_InContainer(Spiel);
}

inline FString TDSitzungsName(const AFGGameState* Spiel)
{
	static FStrProperty* P = FindFProperty<FStrProperty>(AFGGameState::StaticClass(), TEXT("mReplicatedSessionName"));
	return (Spiel && P) ? P->GetPropertyValue_InContainer(Spiel) : FString();
}
