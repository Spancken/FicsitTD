#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDFlieger.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class ATDDefenseCore;

/** 25.11.2026 (Marius: "die Krabben sind keine Fluggeschoepfe, das soll was Realistisches sein"): Luftgegner aus Spiel-Assets.
 *  Kein AFGCreature - Bodentuerme suchen sie nicht. Der Himmelsturm (ATDFlakTurret) und die Spielerwaffen treffen sie ueber
 *  TakeDamage. Gesteuert im Tick, ohne Netz, ueber Mauern. Greifen nie den Spieler und nie die Fabrik an, nur den Kern. */
UCLASS(Abstract, NotBlueprintable)
class FICSITTD_API ATDFlieger : public AActor
{
	GENERATED_BODY()
public:
	ATDFlieger();
	/** nach dem Spawnen: Ziel, Lebenspunkte, Schaden je Treffer am Kern */
	void Starte(ATDDefenseCore* Kern, float LP, float KernSchaden);
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	float GetLP() const { return mLP; }
	float GetMaxLP() const { return mMaxLP; }
	bool IstTot() const { return mLP <= 0.f; }
	/** Vogel am Kern: das Subsystem richtet den Schaden an und entfernt ihn */
	bool AmKern() const { return mAmKern; }
	float GetKernSchaden() const { return mKernSchaden; }
	/** Anzeigename fuer Log und Ansage */
	virtual FString Art() const { return TEXT("Flieger"); }

	/** Zaehler der Sitzung (Selbsttest): abgeschossen, Saeuretreffer am Kern */
	static int32 sAbgeschossen;
	static int32 sSaeureTreffer;

protected:
	virtual void BeginPlay() override;
	/** je Art: Bewegung und Angriff */
	virtual void Fliege(float DeltaSeconds) {}
	virtual void Sterben();
	/** Hoehe ueber dem Boden halten (Strahl nach unten, gekuerzt) */
	float BodenZ(const FVector& Ort);
	void Ausrichten(const FVector& Richtung, float DeltaSeconds);

	UPROPERTY()
	USphereComponent* mKoerper = nullptr;

	TWeakObjectPtr<ATDDefenseCore> mKern;
	float mLP = 1.f;
	float mMaxLP = 1.f;
	float mKernSchaden = 0.f;
	bool mAmKern = false;
	float mZeit = 0.f;
	float mPhase = 0.f;
	float mBodenTakt = 0.f;
	float mBodenZ = 0.f;
};

/** Kleiner, schneller Flieger im Schwarm: der rote Vogel des Spiels (Fluegelschlag im VAT-Material). Fliegt zum Kern und trifft ihn. */
UCLASS()
class FICSITTD_API ATDFliegerVogel : public ATDFlieger
{
	GENERATED_BODY()
public:
	ATDFliegerVogel();
	virtual FString Art() const override { return TEXT("Vogel"); }
protected:
	virtual void BeginPlay() override;
	virtual void Fliege(float DeltaSeconds) override;
	UPROPERTY()
	UStaticMeshComponent* mMesh = nullptr;
};

/** Grosser, zaeher Flieger: der Manta des Spiels. Kreist in Abstand um den Kern und spuckt Saeure auf ihn. */
UCLASS()
class FICSITTD_API ATDFliegerManta : public ATDFlieger
{
	GENERATED_BODY()
public:
	ATDFliegerManta();
	virtual FString Art() const override { return TEXT("Manta"); }
protected:
	virtual void BeginPlay() override;
	virtual void Fliege(float DeltaSeconds) override;
	virtual void Sterben() override;
	void Schiesse();
	void PflegeSaeure(float DeltaSeconds);

	UPROPERTY()
	USkeletalMeshComponent* mMesh = nullptr;

	struct FSaeure
	{
		TWeakObjectPtr<UStaticMeshComponent> Kugel;
		FVector Ort;
	};
	TArray<FSaeure> mSaeure;
	float mSchuss = 0.f;
	float mWinkel = 0.f;
	bool mKreist = false;
	/** abgeschossen: faellt und verschwindet */
	float mFall = -1.f;
	FVector mFallTempo = FVector::ZeroVector;
};
