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
};
