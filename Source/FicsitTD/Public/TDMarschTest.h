#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDMarschTest.generated.h"

class AFGCreature;
class ATDDefenseCore;
class UCommandSender;

/** Messwerte eines Marschtest-Gegners */
struct FTDMarschTeil
{
	TWeakObjectPtr<AFGCreature> Kreatur;
	TCHAR Abschnitt = 'A';
	FString Art;
	float Spawnzeit = 0.f;
	float StartRest = 0.f;
	float Rest30 = -1.f;
	bool bAmZiel = false;
	bool bAbgeschaltet = false;
	float MaxUngesehen = 0.f;
};

/**
 * /td test marsch (Weg C, Konzept-Gegnersteuerung.md Abschnitt 5): je ein Hog, Alpha-Hog, Stinger und Spitter mit Gehirn an Front 1,
 * in drei Abschnitten - A zuschauen, B wegschauen, C weit weg (>= 400 m). Je Gegner alle 5 s eine Logzeile,
 * am Ende Meter in den ersten 30 s, am Ziel, versetzt, abgeschaltet, Schaden am Kern. Danach werden die Testgegner entfernt.
 */
UCLASS()
class FICSITTD_API ATDMarschTest : public AActor
{
	GENERATED_BODY()
public:
	ATDMarschTest();
	void Starte(ATDDefenseCore* Kern, UCommandSender* Sender);

private:
	void Schritt();
	void SpawneAbschnitt(TCHAR Abschnitt);
	void Auswerten();
	void Melde(const FString& Text, bool bChat) const;
	float SpielerAbstand() const;

	TWeakObjectPtr<ATDDefenseCore> mKern;
	UPROPERTY()
	TObjectPtr<UCommandSender> mSender;
	TArray<FTDMarschTeil> mTeile;
	FVector mFront = FVector::ZeroVector;
	FString mFrontName;
	TArray<FVector> mWeg;
	FTimerHandle mTimer;
	float mZeit = 0.f;
	float mKernLPStart = 0.f;
};
