#pragma once

#include "CoreMinimal.h"
#include "TDTurretBase.h"
#include "TDTeslaCoil.generated.h"

/**
 * Teslaspule: keine Munition, nur Strom. Rezept "Entladung" ohne Zutat; ein Produktionszyklus = Aufladen (Strom fliesst nur
 * dabei, Verbrauch im Bauplan). Am Ende ein Blitz von der Kugel zum Ziel, der auf bis zu 3 weitere Gegner im Umkreis von
 * 8 m ueberspringt (je Sprung halber Schaden, kurze Betaeubung). Die Leuchtkanten glühen mit der Ladung auf.
 */
UCLASS()
class FICSITTD_API ATDTeslaCoil : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDTeslaCoil();
	virtual void BeginPlay() override;

protected:
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override { return Ziel != nullptr; }
	virtual bool HatMunition() const override { return true; }
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	virtual float NennDPS() const override;
	/** Grundlast ohne Aufladen (Tuerme.Teslaspule.Grundverbrauch) */
	virtual float GetIdlePowerConsumption() const override;
	/** 0.73 Akkubank: beim Aufladen nur ein Teil der Leistung aus dem Netz */
	virtual float GetProducingPowerConsumptionBase() const override;

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TArray<TObjectPtr<UStaticMeshComponent>> mBlitze;

private:
	void ZeigeBlitz(int32 Index, const FVector& Von, const FVector& Nach);

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> mBlitzMaterial;
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> mGluehMaterial;

	float mBlitzRest = 0.f;
	float mNachgluehen = 0.f;
	/** Level 5 Spannungsfeld: Sekunden bis zum naechsten Takt (alle 0,5 s) */
	float mFeldTakt = 0.f;
};
