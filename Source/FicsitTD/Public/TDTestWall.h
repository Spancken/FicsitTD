#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDDamageable.h"
#include "TDTestWall.generated.h"

class ATDDecoy;
class UBoxComponent;
class UStaticMeshComponent;

/**
 * Testmauer fuer Machbarkeitstests (/mauer): 8 x 4 m Block mit eigenen Lebenspunkten, nicht durchlaufbar.
 * Traegt unsichtbare Koeder, die die Kreatur-KI als Ziel annimmt; deren Schaden landet hier. Wird nicht gespeichert.
 */
UCLASS(NotPlaceable)
class FICSITTD_API ATDTestWall : public AActor, public ITDDamageable
{
	GENERATED_BODY()
public:
	ATDTestWall();

	/** Setzt Lebenspunkte und spawnt die Koeder (nach dem Spawnen aufrufen). */
	void Init(float InMaxHealth);

	// ITDDamageable
	virtual void ApplyTDDamage(float Amount, const AActor* Causer) override;
	virtual bool IsTDIntact() const override { return mHealth > 0.f; }
	virtual float GetTDHealth() const override { return mHealth; }
	virtual float GetTDMaxHealth() const override { return mMaxHealth; }
	/** Reparatur: heilt bis zum Maximum, liefert die tatsaechlich geheilten Lebenspunkte */
	virtual float HealTD(float Amount) override
	{
		if (!IsTDIntact() || Amount <= 0.f) return 0.f;
		const float Alt = mHealth;
		mHealth = FMath::Min(GetTDMaxHealth(), mHealth + Amount);
		return mHealth - Alt;
	}

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Treffer je Verursacher-Klasse, fuer den Statusbericht */
	TMap<FString, int32> TrefferJeQuelle;

private:
	void SetupVisual();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> mCollision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> mVisual;

	UPROPERTY()
	TArray<TObjectPtr<ATDDecoy>> mDecoys;

	float mHealth = 1000.f;
	float mMaxHealth = 1000.f;
};
