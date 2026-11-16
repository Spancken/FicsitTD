#pragma once

#include "CoreMinimal.h"
#include "Subsystem/ModSubsystem.h"
#include "FGSaveInterface.h"
#include "TDSpielstand.generated.h"

class ATDDefenseCore;

/**
 * 0.12-dev (Marius 15.11.: Pionier-Rang je Spielstand; 16.11.: Erstlohn darf nicht am Abriss eines Kerns haengen):
 * Fortschritt im Freien Spiel mit Abwehr, der zum Spielstand gehoert und nicht zu einem Kern - erste Bohr-Erfolge,
 * Pionier-XP, Talente, Bonus-Talentpunkte. Gespeichert mit dem Spielstand (IFGSaveInterface).
 * Abgleich alle 3 s (UTDAbwehrSubsystem): Vereinigung aller Kerne hierher, dann zurueck in jeden Kern. So bleibt der
 * Spielcode unveraendert (liest weiter am Kern), und ein abgerissener oder neuer Kern verliert bzw. erschleicht nichts.
 */
UCLASS()
class FICSITTD_API ATDSpielstand : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()
public:
	ATDSpielstand();
	static ATDSpielstand* Get(const UObject* WorldContext);

	/** Kerne -> Spielstand (Vereinigung), danach Spielstand -> alle Kerne. Rueckgabe: Zahl der Kerne */
	int32 Gleiche();

	// IFGSaveInterface
	virtual void PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
	virtual void PostSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PreLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void GatherDependencies_Implementation(TArray<UObject*>& OutDependentObjects) override {}
	virtual bool NeedTransform_Implementation() override { return false; }
	virtual bool ShouldSave_Implementation() const override { return true; }

	UPROPERTY(SaveGame)
	TArray<FString> mBohrErfolge;
	UPROPERTY(SaveGame)
	int32 mPionierXP = 0;
	UPROPERTY(SaveGame)
	TMap<FString, int32> mTalente;
	UPROPERTY(SaveGame)
	int32 mBonusPunkte = 0;
};
