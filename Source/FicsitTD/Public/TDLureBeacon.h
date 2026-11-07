#pragma once

#include "CoreMinimal.h"
#include "TDTurretBase.h"
#include "TDLureBeacon.generated.h"

class ATDDecoy;

/**
 * Lockfeuer (P1, Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07, Konzept-Modi-Progression 7.3): verbrennt Kohle (Rezept) und zieht
 * waehrend eines Zyklus (15 s) alle Wellengegner in 40 m an - sie greifen das Lockfeuer an statt ihren Weg zu laufen.
 * Danach 30 s Pause. Macht selbst keinen Schaden. Klappen, Lautsprecher und Warnleuchte bewegen sich, solange es lockt.
 * Blueprint = Kopie des Konstruktors wie die Tuerme (Strom, Band-Eingang, Rezepte), Sockel 2 x 2 m.
 */
UCLASS()
class FICSITTD_API ATDLureBeacon : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDLureBeacon();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Lockt gerade (Zyklus laeuft)? */
	bool IstAktiv() const;
	/** Koeder des naechsten aktiven Lockfeuers, dessen Radius Ort erreicht; nullptr, wenn keins lockt (fuer UTDGegnerHirn::SucheZiel) */
	static ATDDecoy* LockKoeder(const UWorld* Welt, const FVector& Ort);

protected:
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override;
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mLeuchte;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mKlappe0;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mKlappe1;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mKlappe2;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mKlappe3;

private:
	float LockRadius() const;
	double Jetzt() const;

	double mPauseBis = 0.0;
	float mKlappenWinkel = 0.f;
	float mHornZeit = 0.f;
	float mLeuchteWinkel = 0.f;
	float mGlutTakt = 0.f;
	int32 mLockrufe = 0;

	static TArray<TWeakObjectPtr<ATDLureBeacon>> sAlle;
};
