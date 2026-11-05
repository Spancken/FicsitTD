#include "TDForschungAbhaengigkeit.h"
#include "TDBalance.h"
#include "TDWaveSubsystem.h"
#include "FGResearchManager.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "Engine/Engine.h"

bool UTDWellenAbhaengigkeit::AreDependenciesMet(UObject* worldContext) const
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	if (!Waves)
	{
		return true;   // ausserhalb einer Spielwelt (Editor, Cook) nicht sperren
	}
	const int32 Ab = TDBalance::Ganz(FString::Printf(TEXT("Forschung.%s.AbWelle"), *mSchluessel), mWelle);
	return Waves->GetWelle() + 1 >= Ab;
}

bool UTDAbzweigungAbhaengigkeit::AreDependenciesMet(UObject* worldContext) const
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UClass* Andere = mAndereSeite.TryLoadClass<UFGSchematic>();
	if (!World || !Andere)
	{
		return true;
	}
	const AFGResearchManager* Forschung = AFGResearchManager::Get(World);
	const AFGSchematicManager* Schemata = AFGSchematicManager::Get(World);
	const bool bGewaehlt = (Forschung && (Forschung->IsResearchComplete(Andere) || Forschung->IsResearchBeingConducted(Andere)))
		|| (Schemata && Schemata->IsSchematicPurchased(Andere));
	return !bGewaehlt;
}
