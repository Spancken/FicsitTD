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
	/** Vom Flammenwerfer: ein Treffer an Ort entzuendet eine nasse Platte darunter */
	static void Entzuende(UWorld* Welt, const FVector& Ort);

protected:
	virtual void NachMaschinenStart() override;
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override { return Ziel != nullptr; }
	virtual FString ZusatzInfo() const override;
	virtual FString MunitionsText() const override;
	virtual bool ZusatzImPanel() const override { return true; }
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;

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
	int32 mBraende = 0;
};
