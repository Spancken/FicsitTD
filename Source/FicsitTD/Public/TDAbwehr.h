#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDAbwehr.generated.h"

struct FItemAmount;

/**
 * 0.12-dev Spielmodus 3 "Freies Spiel mit Abwehr" (Gegencheck Normalspiel, Marius 15.11.2026):
 * - Forschung der FICSIT-Abwehr kostet Coupons statt Festplatten (Abwehr.CouponsJeFestplatte je Festplatte des Knotens).
 *   Die Kosten werden zur Laufzeit am Schematic-CDO getauscht und beim Verlassen der Welt zurueckgestellt.
 * - Der Baum "Industrie" ist verborgen (UTDModusAbhaengigkeit), seine Knoten folgen dem HUB: sobald alle Spielrezepte eines
 *   Industrie-Knotens verfuegbar sind, gilt er als erforscht. So bleiben die Voraussetzungen der anderen Baeume erfuellbar.
 */
UCLASS()
class FICSITTD_API UTDAbwehrSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UTDAbwehrSubsystem* Get(const UObject* WorldContext);
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	/** Wie viele Industrie-Knoten bisher dem HUB gefolgt sind (Log, Test) */
	int32 GetNachgezogen() const { return mNachgezogen; }
	/** A6 Entscheidung 8 (Marius 22.11.): Gegenseite einer Gabelung zum Faktor Abwehr.GabelFaktor (2) des Coupon-Preises.
	 *  Setzt die Kosten am Schematic-CDO aus mOriginalKosten neu; ohne Tausch (kein Freies Spiel mit Abwehr) wirkungslos. */
	void SetzeGabelPreise();
	/** Wie viele Knoten zuletzt den Gabel-Aufschlag trugen (Log, Test) */
	int32 GetGabelAufschlag() const { return mGabelAufschlag; }

private:
	void Pruefe();
	void TauscheKosten();
	void StelleKostenWiederHer();
	void ZieheIndustrieNach();
	/** 16.11.: Hazmat-Anzug und Iod-Filter mit der Forschung Singularitaet (alle Modi) */
	void SchalteStrahlenschutzFrei();
	bool mStrahlenschutz = false;

	FTimerHandle mTimer;
	bool mGetauscht = false;
	int32 mNachgezogen = 0;
	int32 mGabelAufschlag = -1;
	TMap<TWeakObjectPtr<UClass>, TArray<FItemAmount>> mOriginalKosten;
};
