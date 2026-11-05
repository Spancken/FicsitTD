#pragma once

#include "CoreMinimal.h"
#include "FGCharacterBase.h"
#include "TDDecoy.generated.h"

/**
 * Unsichtbarer, unbeweglicher Koeder-Charakter. Die Kreatur-KI behandelt ihn (per Hook) wie einen Spieler.
 * Er stirbt nie selbst - jeder Schaden wird an sein Ziel (ITDDamageable) weitergereicht. Wird nicht gespeichert.
 */
UCLASS(NotPlaceable)
class FICSITTD_API ATDDecoy : public AFGCharacterBase
{
	GENERATED_BODY()
public:
	ATDDecoy(const FObjectInitializer& ObjectInitializer);

	/** Spawnt PerSide Koeder je Wandseite fuer ein quaderfoermiges Ziel (HalfExtent in Lokalkoordinaten, Ursprung = Mitte unten + HalfExtent.Z). */
	static void SpawnForBox(AActor* Owner, const FVector& LocalCenter, const FVector& HalfExtent, int32 PerSide, TArray<TObjectPtr<ATDDecoy>>& OutDecoys);
	/** Spawnt Anzahl Koeder im Kreis (Radius um die Mitte, Hoehe ueber dem Boden) - fuer runde Ziele wie den Kern. */
	static void SpawnRing(AActor* Owner, float Radius, float Hoehe, int32 Anzahl, TArray<TObjectPtr<ATDDecoy>>& OutDecoys);
	static void DestroyAll(TArray<TObjectPtr<ATDDecoy>>& Decoys);

	void SetTarget(AActor* InTarget) { mTarget = InTarget; }
	/** Ziel, solange es noch steht - sonst nullptr. */
	AActor* GetActiveTarget() const;

	virtual void BeginPlay() override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual bool ShouldSave_Implementation() const override { return false; }

private:
	/** Falls Schaden direkt ueber die Health-Komponente kommt (nicht ueber TakeDamage): weiterleiten und wieder auffuellen. */
	UFUNCTION()
	void OnHealthDamage(AActor* DamagedActor, float DamageAmount, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	void Weiterleiten(float DamageAmount, const AActor* DamageCauser);

	UPROPERTY()
	TWeakObjectPtr<AActor> mTarget;
};
