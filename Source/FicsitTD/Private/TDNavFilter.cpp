#include "TDNavFilter.h"
#include "AI/FGNavArea_Water.h"
#include "TDBalance.h"

UTDNavFilter::UTDNavFilter()
{
	FNavigationFilterArea Wasser;
	Wasser.AreaClass = UFGNavArea_Water::StaticClass();
	Wasser.bIsExcluded = true;
	Areas.Add(Wasser);
	// 23.11.: je Abfrage neu, damit Wege.SuchKnoten sofort wirkt (Selbsttest A/B); Kosten: eine Filterkopie je Wegsuche
	bInstantiateForQuerier = true;
}

void UTDNavFilter::InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const
{
	Super::InitializeFilter(NavData, Querier, Filter);
	const int32 Knoten = TDBalance::Ganz(TEXT("Wege.SuchKnoten"), 8192);
	if (Knoten > 0)
	{
		Filter.SetMaxSearchNodes(static_cast<uint32>(Knoten));
	}
}
