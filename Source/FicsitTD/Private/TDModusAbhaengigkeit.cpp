#include "TDModusAbhaengigkeit.h"
#include "TDKernplatz.h"

bool UTDModusAbhaengigkeit::AreDependenciesMet(UObject* worldContext) const
{
	// ohne Welt (z. B. im Editor beim Pruefen der Assets) nicht sperren
	if (!worldContext || !worldContext->GetWorld())
	{
		return true;
	}
	if (!UTDKernplatz::ModAktiv(worldContext))
	{
		return false;
	}
	// 0.12-dev (Gegencheck Normalspiel, Marius 15.11.): im Freien Spiel mit Abwehr vergibt der HUB die Fabrik - der Baum
	// "Industrie" (Assembler, Stahl, Raffinerie, Baender ...) bleibt dort verborgen. Die Bedingung steckt im Baum-Asset (Outer).
	if (UTDKernplatz::IstAbwehr(worldContext))
	{
		for (const UObject* O = GetOuter(); O; O = O->GetOuter())
		{
			// 23.11. (Stromzweig): der Baum Energie (Kraftwerke) folgt dort ebenso dem HUB
			if (O->GetName().Contains(TEXT("TD_Industrie")) || O->GetName().Contains(TEXT("TD_Energie")))
			{
				return false;
			}
		}
	}
	return true;
}
