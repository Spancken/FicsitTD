#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "FGClearanceData.h"
#include "TDDamageable.h"
#include "FGPowerConnectionComponent.h"
#include "TDGate.generated.h"

class ATDDecoy;
class UBoxComponent;
class UFGPowerInfoComponent;

/** 0.9.3 Stromanschluss des Tors (eigene Klasse, weil die Zahl der Kabel nur in Unterklassen gesetzt werden kann) */
UCLASS()
class FICSITTD_API UTDTorStromAnschluss : public UFGPowerConnectionComponent
{
	GENERATED_BODY()
public:
	UTDTorStromAnschluss() { mMaxNumConnectionLinks = 2; }
};

/**
 * Tor (P1, Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07): 8 m breites Mauerstueck mit 4 x 4 m Oeffnung. Spieler und Fahrzeuge kommen
 * durch (Fluegel fahren auf, wenn jemand in 6 m ist), Gegner nie: die Oeffnung ist im Navigationsnetz gesperrt (NavArea_Null), und
 * waehrend Gegner in 15 m sind, bleibt das Tor zu. Fuer die Gegner ist es eine Mauer (Koeder, Regel 2/3, Lebenspunkte).
 * Blueprint = Kopie des Aussichtsturms (reines AFGBuildable), Sockel-Modell SM_TD_Gate_Base, bewegliche Teile hier.
 */
UCLASS()
class FICSITTD_API ATDGate : public AFGBuildable, public ITDDamageable
{
	GENERATED_BODY()
public:
	ATDGate();

	virtual void BeginPlay() override;
	/** 0.9.3 Gegencheck: Strominfo vor BeginPlay an den Anschluss, damit geladene Spielstaende mit Kabel sofort Strom haben */
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetClearanceData_Implementation(TArray<FFGClearanceData>& out_data) const override;
	virtual FText GetLookAtDecription_Implementation(class AFGCharacterPlayer* byCharacter, const FUseState& state) const override;

	// ITDDamageable
	virtual void ApplyTDDamage(float Amount, const AActor* Causer) override;
	virtual bool IsTDIntact() const override { return mHealth > 0.f; }
	virtual float GetTDHealth() const override { return mHealth; }
	/** Bauten.Tor.LP, mit Forschung Verstaerkte Mauern x2 */
	virtual float GetTDMaxHealth() const override;
	virtual float HealTD(float Amount) override
	{
		if (!IsTDIntact() || Amount <= 0.f) return 0.f;
		const float Alt = mHealth;
		mHealth = FMath::Min(GetTDMaxHealth(), mHealth + Amount);
		return mHealth - Alt;
	}

	/** 0 = zu, 1 = offen */
	float GetOffen() const { return mOffen; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mFluegelL;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mFluegelR;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mLeuchteL;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mLeuchteR;
	/** Sperre fuer das Navigationsnetz in der Oeffnung: Gegner planen nie hindurch, Spieler laufen (ueberlappend) durch */
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UBoxComponent> mNavSperre;
	/** 0.9.3 (Marius 09.11.2026): ohne Strom bleibt das Tor in der Welle offen und Kreaturen kommen durch */
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UTDTorStromAnschluss> mStrom;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UFGPowerInfoComponent> mStromInfo;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> mStatusLampe;
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> mStromDose;
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> mLampenMaterial;
	bool mOhneStromOffen = false;
	int32 mLampe = -1;

private:
	void Schritt();
	void Animieren();

	UPROPERTY(SaveGame)
	float mHealth = -1.f;

	UPROPERTY()
	TArray<TObjectPtr<ATDDecoy>> mDecoys;

	float mOffen = 0.f;
	float mSoll = 0.f;
	bool mGegnerNah = false;
	float mLeuchteWinkel = 0.f;
	FTimerHandle mSchrittTimer;
	FTimerHandle mAnimTimer;
};
