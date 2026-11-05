#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "FGClearanceData.h"
#include "TDDamageable.h"
#include "TDBarrier.generated.h"

class ATDDecoy;

/**
 * Panzersperre: passive Verteidigung (Spielkonzept: nur aktive Tuerme machen Schaden). Haelt Kreaturen auf, die sie
 * im Weg haben (Koeder: im Umkreis von 25 m greifen Wellen-Gegner zuerst sie an), und faellt bei 0 Lebenspunkten.
 * Blueprint = Kopie des Aussichtsturms (reines AFGBuildable), Sockel 2 x 2 m.
 */
UCLASS()
class FICSITTD_API ATDBarrier : public AFGBuildable, public ITDDamageable
{
	GENERATED_BODY()
public:
	ATDBarrier();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetClearanceData_Implementation(TArray<FFGClearanceData>& out_data) const override;
	virtual FText GetLookAtDecription_Implementation(class AFGCharacterPlayer* byCharacter, const FUseState& state) const override;

	virtual void ApplyTDDamage(float Amount, const AActor* Causer) override;
	virtual bool IsTDIntact() const override { return mHealth > 0.f; }
	virtual float GetTDHealth() const override { return mHealth; }
	/** mit Forschung Verstaerkte Mauern x2 */
	virtual float GetTDMaxHealth() const override;
	/** Reparatur: heilt bis zum Maximum, liefert die tatsaechlich geheilten Lebenspunkte */
	virtual float HealTD(float Amount) override
	{
		if (!IsTDIntact() || Amount <= 0.f) return 0.f;
		const float Alt = mHealth;
		mHealth = FMath::Min(GetTDMaxHealth(), mHealth + Amount);
		return mHealth - Alt;
	}

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Tower Defense")
	float mMaxHealth = 4000.f;

private:
	UPROPERTY(SaveGame)
	float mHealth = -1.f;

	UPROPERTY()
	TArray<TObjectPtr<ATDDecoy>> mDecoys;
};
