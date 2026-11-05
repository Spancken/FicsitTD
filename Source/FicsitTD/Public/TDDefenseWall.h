#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildableWall.h"
#include "TDDamageable.h"
#include "TDDefenseWall.generated.h"

class ATDDecoy;

/**
 * Verteidigungsmauer: baubar wie eine normale 8x4-Mauer (gleiches Modell, Einrasten, Hologramm - kommt aus der Blueprint,
 * die von Build_Wall_8x4_01 kopiert und auf diese Klasse umgehaengt wird), aber als echtes Actor-Gebaeude statt
 * Lightweight-Instanz, damit sie Lebenspunkte, Koeder und Zerstoerung haben kann.
 */
UCLASS()
class FICSITTD_API ATDDefenseWall : public AFGBuildableWall, public ITDDamageable
{
	GENERATED_BODY()
public:
	ATDDefenseWall(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual bool ManagedByLightweightBuildableSubsystem() const override { return false; }

	// ITDDamageable
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
	/** Lebenspunkte bei voller Gesundheit */
	UPROPERTY(EditDefaultsOnly, Category = "Tower Defense")
	float mMaxHealth = 2000.f;

	/** Koeder je Wandseite */
	UPROPERTY(EditDefaultsOnly, Category = "Tower Defense")
	int32 mDecoysPerSide = 2;

	/** Erkennungsfarbe beim ersten Bau (Spieler kann danach umlackieren) */
	UPROPERTY(EditDefaultsOnly, Category = "Tower Defense")
	FLinearColor mDefensePrimaryColor = FLinearColor(0.22f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Tower Defense")
	FLinearColor mDefenseSecondaryColor = FLinearColor(0.005f, 0.005f, 0.005f);

private:
	void SetzeErkennungsfarbe();

	/** Aktuelle Lebenspunkte, im Spielstand gesichert. < 0 = noch nie gesetzt. */
	UPROPERTY(SaveGame)
	float mHealth = -1.f;

	UPROPERTY(SaveGame)
	bool mFarbeGesetzt = false;

	UPROPERTY()
	TArray<TObjectPtr<ATDDecoy>> mDecoys;

	FTimerHandle mFarbTimer;
};
