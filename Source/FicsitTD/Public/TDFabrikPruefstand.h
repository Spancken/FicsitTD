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
		TArray<TWeakObjectPtr<AFGBuildable>>& OutBauten, TArray<FString>& OutBericht, int32 Ketten = 1);

	/** Zustand der gebauten Linie: Produktivitaet je Maschine, Munition je Turm (Log und Rueckgabe fuer den Bericht) */
	TArray<FString> Bericht(const TArray<TWeakObjectPtr<AFGBuildable>>& Bauten);

	// 17.11. (P11, Konzept-Fabrik-Strom-2026-11-17.md): echter Strom und Kosten wie beim Spieler
	/** Kostenmodus: "frei" (wie bisher), "kontingent" (kauft wie ein Spieler), "echt" (dazu Baumaterial aus dem Inventar) */
	void SetzeKosten(const FString& Modus);
	/** Vor jedem Bau: Kontingent pruefen und notfalls kaufen, im Modus "echt" Material abziehen. false = nicht bauen (Grund) */
	bool KostenPruefen(UWorld* World, UClass* Klasse, const FVector& Ort, FString& OutGrund);
	/** Stromquelle bauen (S0: "Biomasse": Lager mit Biomasse aus dem Inventar, Band zu den Brennern) und alle Verbraucher verkabeln */
	bool BaueStrom(UWorld* World, ATDDefenseCore* Kern, const FString& Art, int32 Anzahl,
		TArray<TWeakObjectPtr<AFGBuildable>>& InOutBauten, TArray<FString>& OutBericht);
	/** Masten und Kabel: haengt alle Pruefstand-Bauten und Tuerme ohne Netz an das Netz. Rueckgabe: neue Masten */
	int32 VerbindeNetz(UWorld* World, TArray<TWeakObjectPtr<AFGBuildable>>& InOutBauten, TArray<FString>& OutBericht);
	/** Je Takt: Sicherung pruefen und nach Test.SicherungZurueck (20 s) zuruecksetzen; Spitzenverbrauch merken */
	TArray<FString> Ueberwache(UWorld* World, const TArray<TWeakObjectPtr<AFGBuildable>>& Bauten, float Dt);
	/** Netzwerte fuer den Bericht */
	TArray<FString> StromBericht();
	/** 17.11.: Zaehler, Netz und Kostenmodus fuer einen neuen Testplan zuruecksetzen */
	void NeuerPlan();
}
