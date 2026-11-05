#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDBandUpgrade.generated.h"

/** 0.71 (Marius 04.11.2026: schnellere Baender im MAM -> alle Baender automatisch aufruesten, "das ist nur Fleissarbeit"):
 *  Nach Forschung "Foerderband Mk2/Mk3" werden alle langsameren Foerderbaender und Lifte gegen die neue Stufe getauscht,
 *  wie beim Aufruesten im Spiel (neues Teil mit gleichem Verlauf, altes uebergibt Verbindungen und Gegenstaende). */
UCLASS()
class FICSITTD_API UTDBandUpgrade : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UTDBandUpgrade* Get(const UObject* WorldContext);
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	/** Alle Baender und Lifte unter Stufe Mk<Stufe> aufruesten; Rueckgabe: Bericht */
	FString RuesteUm(int32 Stufe);

private:
	void Verbinde();
	UFUNCTION()
	void Erforscht(TSubclassOf<class UFGSchematic> Schematic);
	FTimerHandle mTimer;
	bool mVerbunden = false;
};
