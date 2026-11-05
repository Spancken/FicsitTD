#pragma once

#include "CoreMinimal.h"
#include "TDTurretBase.h"
#include "TDFlameTurret.generated.h"

/** Wirkung eines Treibstoffs */
struct FTDTreibstoffWerte
{
	float SchadenProSekunde = 50.f;
	float BrennDauer = 3.f;          // s Nachbrennen
	float BrennSchaden = 10.f;       // pro Sekunde Nachbrennen
	/** 0.64 Munition sichtbar: Kegelbreite (Faktor), Betaeubung je Sekunde Flamme, Flammenfarbe Duese/Ende, Flackertempo */
	float Kegel = 1.f;
	float Betaeubung = 0.f;
	FLinearColor Hell = FLinearColor(1.f, 0.75f, 0.35f);
	FLinearColor Rot = FLinearColor(1.f, 0.18f, 0.02f);
	float Flackern = 5.f;
};

/** Brennender Boden (Level 5 Feuerteppich) */
struct FTDBrand
{
	FVector Ort;
	float Rest = 6.f;
	float Takt = 1.f;
	float Funken = 0.f;
	float Schaden = 0.f;
	FLinearColor Hell;
};

/**
 * Flammenwerfer: Treibstoff per Rohr (Anschluss hinten), Rezept je Treibstoff. Ein Zyklus = 1 s Flamme: alle Kreaturen im
 * Kegel (Reichweite, 15 Grad zur Seite) nehmen Schaden und brennen nach. Flammenstrahl als flackernder Kegel (SM_TD_FlameJet).
 */
UCLASS()
class FICSITTD_API ATDFlameTurret : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDFlameTurret();
	virtual void BeginPlay() override;
	virtual void Factory_Tick(float dt) override;
	virtual int32 GetAmmo() const override;

	static FTDTreibstoffWerte WerteFuer(const UClass* Treibstoff);

protected:
	virtual void NachMaschinenStart() override;
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual FString ZusatzInfo() const override;
	virtual FString MunitionsText() const override { return FString::Printf(TEXT("%.1f m³ %s"), GetAmmo() / 1000.f, *GetMunitionsName()); }
	virtual bool ZusatzImPanel() const override { return false; }
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	virtual float NennDPS() const override;

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<class UFGPipeConnectionFactory> mRohrEingang;

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mFlamme;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> mFlammenMaterial;

	/** Anschluss musste von Hand eingehaengt werden -> Rohr selbst abpumpen */
	bool mSelbstPumpen = false;
	float mFlammeRest = 0.f;
	float mFlammenLaenge = 0.f;
	TArray<FTDBrand> mBraende;
};
