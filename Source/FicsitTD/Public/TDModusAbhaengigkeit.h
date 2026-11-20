#pragma once

#include "CoreMinimal.h"
#include "AvailabilityDependencies/FGAvailabilityDependency.h"
#include "TDModusAbhaengigkeit.generated.h"

/**
 * 0.11 (Modus Aus, Marius 15.11.: nur der Spielmodus zaehlt): Verfuegbarkeits-Bedingung fuer die vier TD-Forschungsbaeume.
 * Erfuellt, wenn FicsitTD in der Welt aktiv ist (UTDKernplatz::ModAktiv: Spielmodus "Einsatz" oder ein im Entwicklermodus
 * von Hand gestarteter Einsatz). Wird per Scripts/setze_modus_abhaengigkeit.py in mVisibilityDependencies und
 * mUnlockDependencies der Baeume eingetragen. Die Baeume bleiben registriert, damit alte Spielstaende sie behalten.
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class FICSITTD_API UTDModusAbhaengigkeit : public UFGAvailabilityDependency
{
	GENERATED_BODY()

public:
	virtual bool AreDependenciesMet(UObject* worldContext) const override;
#if WITH_EDITOR
	virtual FString ToString() const override { return TEXT("FicsitTD aktiv (Spielmodus Einsatz)"); }
#endif
};
