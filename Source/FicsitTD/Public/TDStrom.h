#pragma once

#include "CoreMinimal.h"

/** 23.11.2026 (Marius, Stromzweig): Leistung der Kraftwerke je Brennstoff (TDStrom.cpp). Friend von AFGBuildableGenerator
 *  (Config/AccessTransformers.ini), damit mPowerProduction je Kraftwerk gesetzt werden kann. */
class FICSITTD_API FTDStrom
{
public:
	/** Faktor des Brennstoffs (Strom.<Brennstoff>.Faktor); 1, wenn FICSIT-Abwehr aus ist oder der Brennstoff unbekannt */
	static float Faktor(const UClass* Brennstoff, const UObject* WorldContext);
	/** Alle Brennstoff-Kraftwerke der Welt auf ihre Leistung setzen; Rueckgabe: Zahl der geaenderten Kraftwerke */
	static int32 Pflege(UWorld* World);
	/** Selbsttest: naechstes Brennstoff-Kraftwerk zu Ort befuellen, Brennstoff laden, Pflege laufen lassen.
	 *  Rueckgabe: Leistung (GetPowerProductionCapacity) in MW, -1 ohne Kraftwerk */
	static float TestBefuelle(UWorld* World, const FVector& Ort, UClass* Brennstoff, int32 Menge, FString& OutText);
};
