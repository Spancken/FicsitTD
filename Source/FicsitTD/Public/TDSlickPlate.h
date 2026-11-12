#pragma once

#include "CoreMinimal.h"
#include "TDTurretBase.h"
#include "TDSlickPlate.generated.h"

/**
 * Oelschlick-Feld (P1, Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07, Konzept-Modi-Progression 7.3): 8 x 8 m Bodenplatte, per Rohr mit
 * Schweroelrueckstand versorgt (Rezept, 1 m³ je 6 s). Solange Gegner in der Naehe sind, verteilt sie den Film: Gegner auf der Platte
 * laufen um 35 % langsamer. Trifft ein Flammenwerfer einen Gegner auf der Platte, brennt sie 5 s und schaedigt alle darauf.
 * Blueprint = Kopie des Konstruktors wie die Tuerme (Strom, Rezepte), Rohr-Eingang hier in C++ wie beim Flammenwerfer.
 */
UCLASS()
class FICSITTD_API ATDSlickPlate : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDSlickPlate();
	virtual void BeginPlay() override;
	virtual void Factory_Tick(float dt) override;

	/** Film liegt (Zyklus laeuft oder kurz danach) */
	bool IstNass() const;
	/** Liegt Ort auf der Platte (2D, mit Rand)? */
	bool AufPlatte(const FVector& Ort) const;
	/** Feuer an Ort (Flammenwerfer, Brandmunition) entzuendet eine nasse Platte darunter. Zuender zaehlt die Kombo Oelbrand. */
	static void Entzuende(UWorld* Welt, const FVector& Ort, const ATDTurretBase* Zuender = nullptr);
	/** Feuer mit Radius (Feuerteppich, Lockfeuer): entzuendet jede nasse Platte, die der Kreis beruehrt */
	static void EntzuendeUmkreis(UWorld* Welt, const FVector& Ort, float Radius, const ATDTurretBase* Zuender = nullptr);

protected:
	virtual void NachMaschinenStart() override;
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	/** 0.9.6 (Gesamtreview 12.11., A6): Oel fliesst, sobald ein Gegner auf der 8 x 8 m Platte oder knapp davor ist
	 *  (Rand Tuerme.Oelschlick.Ausloserand). Vorher: Ziel in 6,5 m ab dem Ventil am Plattenrand, mit Sichtpruefung. */
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override;
	virtual FString ZusatzInfo() const override;
	virtual FString MunitionsText() const override;
	virtual bool ZusatzImPanel() const override { return true; }
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	/** Selbsttest: Entzuendet (Anzahl Braende), Brandschaden, Gebremst */
	virtual float ZusatzTestWert(const FString& Name) const override
	{
		return Name == TEXT("Entzuendet") ? float(mBraende) : Name == TEXT("Brandschaden") ? mBrandSchadenSumme : Name == TEXT("Gebremst") ? float(mGebremst)
			: Name == TEXT("Zyklen") ? float(mSchuesse) : Name == TEXT("ZielTakte") ? float(mDiagZiel) : Name == TEXT("NassTakte") ? float(mDiagNass) : Name == TEXT("NahTakte") ? float(mDiagNah) : -1.f;   // 0.9.4 Diagnose
	}

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<class UFGPipeConnectionFactory> mRohrEingang;

private:
	double Jetzt() const;

	bool mSelbstPumpen = false;
	double mNassBis = 0.0;
	double mBrandBis = 0.0;
	float mWirkTakt = 0.f;
	float mFunkenTakt = 0.f;
	int32 mGebremst = 0;
	/** 0.9.4 Diagnose (Oelschlick bremst im Test nie): Takte von 0,5 s mit Ziel, nass, Ziel hoechstens 2 m neben der Platte */
	int32 mDiagZiel = 0;
	int32 mDiagNass = 0;
	int32 mDiagNah = 0;
	float mDiagTakt = 0.f;
	int32 mBraende = 0;
	/** 0.9.1 Oelbrand: Gegner, die auf der brennenden Platte standen, brennen bis dahin nach */
	TMap<TWeakObjectPtr<AFGCreature>, double> mNachbrand;
	float mBrandSchadenSumme = 0.f;
	void Zuende(const ATDTurretBase* Zuender);
	/** 0.9.3 (Marius 09.11.2026): sichtbare Flammen ueber der ganzen Platte, solange sie brennt (Feuer-Flipbook M_TD_FeuerWolke) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialBillboardComponent>> mFlammen;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> mFlammenMat;
	TArray<float> mFlammeAlter;
	TArray<float> mFlammeLeben;
	TArray<FVector> mFlammeOrt;
	FTimerHandle mFlammenTimer;
	void FlammenTick();
};
