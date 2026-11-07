#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildableStorage.h"
#include "TDDimensionalDepot.generated.h"

/**
 * Dimensionslager (P3, 0.83): Lager (Kopie der Lagerkiste Mk1) mit Mercer-Kugel. Verteilt jede Sekunde seinen Inhalt an die
 * Tuerme in 30 m, deren Rezept genau diesen Gegenstand verbraucht - Munition ohne Band. Werte unter Bauten.Dimensionslager.*.
 */
UCLASS()
class FICSITTD_API ATDDimensionalDepot : public AFGBuildableStorage
{
	GENERATED_BODY()
public:
	ATDDimensionalDepot();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	int32 GetVerteilt() const { return mVerteilt; }
protected:
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mKugel;
private:
	void Verteile();
	FTimerHandle mTimer;
	int32 mVerteilt = 0;
	int32 mLetzteTuerme = 0;
};
