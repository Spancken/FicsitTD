#pragma once

#include "CoreMinimal.h"
#include "TDTurretBase.h"
#include "TDNobeliskLauncher.generated.h"

/** Wirkung einer Nobelisk-Art */
struct FTDNobeliskWerte
{
	float Schaden = 150.f;
	float Radius = 600.f;          // cm
	float Betaeubung = 0.f;        // s
	float GasSchaden = 0.f;        // pro Sekunde
	float GasDauer = 0.f;          // s
	int32 Streu = 0;               // Zusatz-Explosionen
	float ExplosionsGroesse = 1.f; // Optik
};

/** Liegende Gaswolke nach einem Gas-Nobelisk: trifft jede Sekunde alle Gegner darin, auch nachkommende */
struct FTDGasWolke
{
	FVector Ort;
	float Radius = 600.f;
	float Schaden = 20.f;
	float Rest = 10.f;
	float Takt = 0.f;
};

/** Ein Geschoss im Flug (rein optisch, der Schaden faellt beim Einschlag) */
struct FTDNobeliskFlug
{
	TWeakObjectPtr<UStaticMeshComponent> Modell;
	FVector Start;
	FVector Ziel;
	float Dauer = 1.f;
	float Zeit = 0.f;
	float Bogen = 500.f;
	bool bAktiv = false;
	/** Verfolgter Gegner: der Einschlagpunkt wird im Flug nachgefuehrt */
	TWeakObjectPtr<class AFGCreature> Gegner;
};

/**
 * Nobelisk-Werfer: Rezept je Nobelisk-Art, ein Zyklus = ein Schuss im Bogen (Mindestabstand 10 m, Reichweite 60 m).
 * Flaechenschaden beim Einschlag: Nobelisk, Gas (Wolke mit Schaden ueber Zeit), Puls (Betaeubung), Streu (Zusatz-Explosionen),
 * Nuke (riesig). Das Rohr steht steil (Neigung 35-70 Grad); ausgerichtet wird nach der Seite.
 */
UCLASS()
class FICSITTD_API ATDNobeliskLauncher : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDNobeliskLauncher();
	virtual void BeginPlay() override;

	static FTDNobeliskWerte WerteFuer(const UClass* Munition);
	/** 0.84 Verpuffung: eine Flamme am Ort entzuendet jede Gaswolke, die ihn einschliesst (Stichflamme, Explosion, Wolke weg).
	 *  Gibt den Schaden zurueck (0 = keine Wolke oder Kombination nicht freigeschaltet). */
	static float EntzuendeWolke(UWorld* Welt, const FVector& Ort, const class ATDTurretBase* Zuender);

protected:
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	virtual float NennDPS() const override;
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual bool ZielErlaubt(const AFGCreature* Ziel, float Abstand) const override { return Abstand >= 1000.f; }

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TArray<TObjectPtr<UStaticMeshComponent>> mGeschosse;

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TArray<TObjectPtr<UStaticMeshComponent>> mExplosionen;

private:
	void Einschlag(const FVector& Ort, const FTDNobeliskWerte& W, float Faktor);
	/** Ein Nobelisk auf den Weg bringen; Versatz != 0 = Nebenziel des Streuschusses (Level 5) */
	void StarteFlug(const FVector& Start, AFGCreature* Ziel, const FVector& Versatz);
	void ZeigeExplosion(const FVector& Ort, float Groesse);
	/** Explosion des Spiel-Nobelisks (Effekt und Klang, ohne Schaden); false, wenn es nicht ging */
	bool SpielExplosion(const FVector& Ort) const;
	TArray<FTDGasWolke> mWolken;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> mExplosionsMaterial;

	TArray<FTDNobeliskFlug> mFluege;
	TArray<float> mExplosionRest;
	float mRueckstoss = 0.f;
};
