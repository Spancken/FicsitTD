#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "TDNavFilter.generated.h"

/**
 * Wegsuche fuer Wellengegner: Wasserflaechen (UFGNavArea_Water) sind gesperrt.
 * Test 0.16.5: ein Weg fuehrte am Seeufer durchs flache Wasser, der Gegner blieb dort haengen.
 */
UCLASS()
class FICSITTD_API UTDNavFilter : public UNavigationQueryFilter
{
	GENERATED_BODY()
public:
	UTDNavFilter();
	/** 23.11. (Labyrinth-Selbsttest): Suchgrenze der A*-Suche aus Wege.SuchKnoten (Engine 2048). In Labyrinthen aus Mauern
	 *  reichte sie nicht, die Suche lieferte einen Teilweg bis vor die aeussere Mauer. */
	virtual void InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const override;
};
