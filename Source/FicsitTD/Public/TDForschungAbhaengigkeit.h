#pragma once

#include "CoreMinimal.h"
#include "AvailabilityDependencies/FGAvailabilityDependency.h"
#include "TDForschungAbhaengigkeit.generated.h"

/**
 * MAM-Knoten erst ab einer Welle (Wellen-Progression 7, Test 0.53: gesperrte Knoten sollen auch gesperrt aussehen).
 * Haengt in mSchematicDependencies des Knotens (create_research.py); der Hook auf CanResearchBeInitiated bleibt als Sicherung.
 */
UCLASS()
class FICSITTD_API UTDWellenAbhaengigkeit : public UFGAvailabilityDependency
{
	GENERATED_BODY()
public:
	virtual bool AreDependenciesMet(UObject* worldContext) const override;

	/** Schluessel fuer Forschung.<Schluessel>.AbWelle in Balance.json */
	UPROPERTY(EditAnywhere)
	FString mSchluessel;
	UPROPERTY(EditAnywhere)
	int32 mWelle = 0;
};

/** Abzweigung im MAM: Knoten nur, solange die andere Seite weder erforscht noch in Arbeit ist */
UCLASS()
class FICSITTD_API UTDAbzweigungAbhaengigkeit : public UFGAvailabilityDependency
{
	GENERATED_BODY()
public:
	virtual bool AreDependenciesMet(UObject* worldContext) const override;

	UPROPERTY(EditAnywhere)
	FSoftClassPath mAndereSeite;
};
