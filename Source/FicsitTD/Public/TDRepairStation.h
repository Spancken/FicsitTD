#pragma once

#include "CoreMinimal.h"
#include "TDTurretBase.h"
#include "TDRepairStation.generated.h"

/**
 * Reparaturstation (Spielkonzept S6, aktive Unterstuetzung): verbraucht Strom und Material (Rezept, z. B. Eisenplatten) und
 * repariert mit jedem Zyklus die am staerksten beschaedigten Verteidigungsgebaeude im Umkreis (Mauern, Sperren, Tuerme, Kern).
 * Laeuft nur, wenn es etwas zu reparieren gibt. Greift keine Kreaturen an. Reparaturstrahl = Blitz-Modell der Teslaspule.
 */
UCLASS()
class FICSITTD_API ATDRepairStation : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDRepairStation();
	virtual void BeginPlay() override;

	/** Lebenspunkte je Zyklus nach Material */
	static float ReparaturFuer(const UClass* Material);

protected:
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override { return mGibtArbeit; }
	virtual bool ZielErlaubt(const AFGCreature* Ziel, float Abstand) const override { return false; }
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mStrahl;

private:
	/** Beschaedigte Gebaeude im Umkreis, schwaechste zuerst */
	TArray<AActor*> SucheBeschaedigte() const;

	bool mGibtArbeit = false;
	float mSuchTakt = 0.f;
	float mStrahlRest = 0.f;
	float mGeheilt = 0.f;
	TWeakObjectPtr<AActor> mStrahlZiel;
};
