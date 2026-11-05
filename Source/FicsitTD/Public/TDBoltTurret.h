#pragma once

#include "CoreMinimal.h"
#include "TDTurretBase.h"
#include "TDBoltTurret.generated.h"

/** Wirkung einer Bolzenart */
struct FTDBolzenWerte
{
	float Schaden = 40.f;
	float FlaechenRadius = 0.f;     // cm, > 0 = Explosion
	float FlaechenSchaden = 0.f;
	float Betaeubung = 0.f;         // Sekunden
	int32 Splitter = 0;             // zusaetzliche Ziele
	float SplitterRadius = 600.f;
	float SplitterSchaden = 0.f;
	int32 Durchschlag = 0;          // weitere Gegner in der Schusslinie (Durchschlagbolzen)
	float Folgeschaden = 1.f;       // Schaden je weiterem Gegner relativ zum vorigen
};

/**
 * Bolzenturm: jede Bolzenart ist ein Rezept (1 Bolzen -> 1 Bolzenschuss, wird weggeworfen), ein Zyklus = ein Schuss.
 * Sichtbares Geschoss = Projektil des Armierungsgewehrs; Schaden nach Flugzeit.
 */
UCLASS()
class FICSITTD_API ATDBoltTurret : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDBoltTurret();

	/** Wirkung je Bolzenart (nach Klassenname des Munitions-Items) */
	static FTDBolzenWerte WerteFuer(const UClass* MunitionsKlasse);

protected:
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	virtual float NennDPS() const override;

	/** Geschossgeschwindigkeit in cm/s (fuer Flugzeit und sichtbares Geschoss) */
	UPROPERTY(EditDefaultsOnly, Category = "Tower Defense")
	float mProjectileSpeed = 6000.f;

private:
	void SpawneGeschoss(const FVector& Von, const FVector& Nach) const;
	/** Ein Bolzen aus einer Muendung (Level 5: zwei je Zyklus) */
	void Schuss(AFGCreature* Ziel, const FVector& Muendung, const FTDBolzenWerte& W, float Faktor);
};
