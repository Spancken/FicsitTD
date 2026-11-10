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
	/** 0.9.1 (Simulation 09.11.): Plasmaflamme trifft als Strom - wirkt gegen hitzefeste Wuesten- und Wasserspucker */
	ETDSchadensart Art = ETDSchadensart::Feuer;
};

/** 0.9.3 Feuer-Flipbook: eine wandernde Feuerwolke des Strahls (Sprite mit M_TD_FeuerWolke) */
struct FTDFeuerWolke
{
	float Alter = 1.f;          // 0..1, >= 1 = frei
	float Lebensdauer = 0.6f;   // s
	FVector Start = FVector::ZeroVector;
	FVector Weg = FVector::ZeroVector;    // Strecke bis zum Lebensende (Welt, cm)
	float GroesseStart = 40.f;
	float GroesseEnde = 300.f;
	float Steigen = 60.f;       // cm am Lebensende nach oben
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
 * Kegel (Reichweite, 15 Grad zur Seite) nehmen Schaden und brennen nach. Flammenstrahl ab 0.9.3 aus wandernden Feuerwolken (Flipbook aus Blender, M_TD_FeuerWolke); Rueckfall auf den Kegel SM_TD_FlameJet, falls das Material fehlt.
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

	/** 0.9.3 Feuer-Flipbook (Marius 09.11.2026): Strahl aus wandernden Feuerwolken statt Kegel-Modell */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialBillboardComponent>> mWolkenSprites;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> mWolkenMaterial;
	TArray<FTDFeuerWolke> mWolken;
	float mWolkenTakt = 0.f;
	void ErzeugeWolken();
	void BewegeWolken(float DeltaSeconds, bool bNeue, const FTDTreibstoffWerte& W, float Kegel);

	/** Anschluss musste von Hand eingehaengt werden -> Rohr selbst abpumpen */
	bool mSelbstPumpen = false;
	float mFlammeRest = 0.f;
	float mFlammenLaenge = 0.f;
	TArray<FTDBrand> mBraende;
};
