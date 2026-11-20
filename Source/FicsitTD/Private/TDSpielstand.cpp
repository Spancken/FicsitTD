#include "TDSpielstand.h"
#include "TDDefenseCore.h"
#include "TDPionier.h"
#include "FicsitTDModule.h"
#include "Subsystem/SubsystemActorManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"

ATDSpielstand::ATDSpielstand()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;
}

ATDSpielstand* ATDSpielstand::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	USubsystemActorManager* Manager = World ? World->GetSubsystem<USubsystemActorManager>() : nullptr;
	return Manager ? Manager->GetSubsystemActor<ATDSpielstand>() : nullptr;
}

int32 ATDSpielstand::Gleiche()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	const int32 VorherErfolge = mBohrErfolge.Num();
	const int32 VorherXP = mPionierXP;
	// 1) Erfolge: Vereinigung aller Kerne (werden nie zurueckgenommen). XP: Hoechstwert (waechst nur).
	TArray<ATDDefenseCore*> Kerne;
	int32 MaxXP = mPionierXP;
	for (TActorIterator<ATDDefenseCore> It(World); It; ++It)
	{
		Kerne.Add(*It);
		for (const FString& E : It->mBohrErfolge)
		{
			mBohrErfolge.AddUnique(E);
		}
		MaxXP = FMath::Max(MaxXP, It->mPionierXP);
	}
	// 2) Talente und Bonuspunkte vom aktuellsten Kern (XP-Stand = Hoechstwert, bevorzugt der aktive des Pioniers).
	//    Neu verteilte Talente bleiben so erhalten; ein neu gebauter Kern (0 XP) ueberschreibt nichts.
	const UTDPionier* Pionier = UTDPionier::Get(World);
	ATDDefenseCore* Aktiv = Pionier ? Pionier->Kern() : nullptr;
	ATDDefenseCore* Quelle = Aktiv && Aktiv->mPionierXP == MaxXP ? Aktiv : nullptr;
	for (ATDDefenseCore* K : Kerne)
	{
		Quelle = Quelle ? Quelle : (K->mPionierXP == MaxXP && MaxXP > 0 ? K : nullptr);
	}
	mPionierXP = MaxXP;
	if (Quelle)
	{
		mTalente = Quelle->mTalente;
		mBonusPunkte = Quelle->mPionierBonusPunkte;
	}
	// 3) Spielstand -> alle Kerne
	for (ATDDefenseCore* K : Kerne)
	{
		for (const FString& E : mBohrErfolge)
		{
			K->mBohrErfolge.AddUnique(E);
		}
		K->mPionierXP = mPionierXP;
		K->mPionierBonusPunkte = mBonusPunkte;
		K->mTalente = mTalente;
	}
	if (mBohrErfolge.Num() != VorherErfolge || mPionierXP != VorherXP)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Spielstand: %d Bohr-Erfolge, %d Pionier-XP, %d Bonus-Talentpunkte (%d Kerne abgeglichen)"),
			mBohrErfolge.Num(), mPionierXP, mBonusPunkte, Kerne.Num());
	}
	return Kerne.Num();
}

void ATDSpielstand::PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
	Gleiche();
}
