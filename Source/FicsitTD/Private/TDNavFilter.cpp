#include "TDNavFilter.h"
#include "AI/FGNavArea_Water.h"

UTDNavFilter::UTDNavFilter()
{
	FNavigationFilterArea Wasser;
	Wasser.AreaClass = UFGNavArea_Water::StaticClass();
	Wasser.bIsExcluded = true;
	Areas.Add(Wasser);
}
