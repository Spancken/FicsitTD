#pragma once

#include "CoreMinimal.h"

class UWorld;
class AFGBuildable;
class ATDDefenseCore;

/**
 * Fabrik-Pruefstand (Konzept Notizen/Konzept-Fabrik-Pruefstand-2026-11-16.md, Stufe F1; Marius 16.11.: "kompletten autonomen
 * Testlauf bauen"). Baut eine echte Munitionslinie mit Spielgebaeuden: Miner auf dem naechsten freien Knoten, Schmelzofen,
 * Konstrukteure, Splitter, Foerderbaender (UFGTestBlueprintFunctionLibrary::SpawnSplineBuildable) und Tuerme, die ihre
 * Munition nur ueber das Band bekommen. Strom kommt in F1 noch aus "stromfrei" (Generatoren = Baustein B5, spaeter).
 */
namespace TDFabrik
{
	/** Linie "Bolzen": Eisenerz -> Barren -> Stangen -> Armierungsbolzen -> Tuerme. Rueckgabe true, wenn alles gebaut und verbunden ist. */
	bool BaueLinie(UWorld* World, ATDDefenseCore* Kern, const FString& Linie, int32 Tuerme, int32 Level, int32 BandMk,
		TArray<TWeakObjectPtr<AFGBuildable>>& OutBauten, TArray<FString>& OutBericht);

	/** Zustand der gebauten Linie: Produktivitaet je Maschine, Munition je Turm (Log und Rueckgabe fuer den Bericht) */
	TArray<FString> Bericht(const TArray<TWeakObjectPtr<AFGBuildable>>& Bauten);
}
