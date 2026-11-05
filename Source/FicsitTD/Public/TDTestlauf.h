#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDTestlauf.generated.h"

class AFGCreature;
class UCommandSender;

/** Messwerte eines Test-Gegners */
struct FTDTestGegner
{
	TWeakObjectPtr<AFGCreature> Kreatur;
	FString Name;
	float StartAbstand = 0.f;
	float MinAbstand = 0.f;
	float Weg = 0.f;
	FVector LetzterOrt = FVector::ZeroVector;
	TMap<FString, int32> Zustaende;
};

/**
 * Automatischer Test (/td test [karte|gegner]): prueft rund um den Abwehrkern (sonst um den Spieler)
 * 1) Karte: 16 Richtungen x 150/200/250 m - Boden, Wasser, Hoehenunterschied, Navigationsnetz, Pfad zum Ziel (Laenge, vollstaendig?)
 * 2) Gegner: jeder Gegnertyp einmal in 40 m, 25 s zum Ziel gelenkt (20-fache LP, damit Tuerme die Messung nicht beenden),
 *    gemessen: geringster Abstand, zurueckgelegter Weg, KI-Zustaende, Schaden am Kern.
 * Ergebnis zeilenweise ins Log (LogFicsitTD "TDTest"), Zusammenfassung in den Chat. Der Actor entfernt sich danach selbst.
 */
UCLASS()
class FICSITTD_API ATDTestlauf : public AActor
{
	GENERATED_BODY()
public:
	ATDTestlauf();
	void Starte(AActor* Ziel, bool bKarte, bool bGegner, UCommandSender* Sender);

private:
	void KartenTest();
	void StarteGegnerTest();
	void GegnerSchritt();
	void GegnerAuswerten();
	void Melde(const FString& Text, bool bChat) const;

	TWeakObjectPtr<AActor> mZiel;
	UPROPERTY()
	TObjectPtr<UCommandSender> mSender;
	TArray<FTDTestGegner> mGegner;
	FTimerHandle mTimer;
	float mZeit = 0.f;
	float mKernLPStart = 0.f;
};
