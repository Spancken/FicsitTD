#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDLaufProtokoll.generated.h"

class AFGCreature;
class AFGItemPickup_Spawnable;

/**
 * Balancing-Lauf (Marius 03.11.2026): Wirtschaftsprotokoll je Welle (Saved/FicsitTD/Wirtschaft.csv), Noten des Spielers
 * (/td notiz), Eingriffe wie /gib oder Testsprung, Beute der Kreaturen und der Lauf-Modus "Balancing".
 * Die Kampfwerte je Welle stehen weiter in Metriken.csv (UTDWaveSubsystem::MeldeMetrik).
 */
UCLASS()
class FICSITTD_API UTDLaufProtokoll : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UTDLaufProtokoll* Get(const UObject* WorldContext);

	/** Lauf-Modus Balancing: Testsprung aus, Eingriffe werden laut gemeldet (am Kern gespeichert, hier gespiegelt) */
	static bool bBalancing;
	bool IstBalancing() const;
	void SetzeBalancing(bool bAn);

	/** Wellenstart: Laenge der Bauphase seit dem letzten Wellenende */
	void WellenStart(int32 Welle);
	/** Eigener Spielstand mit Namen (Lauf.Spielstaende = 0 schaltet die Wellen-Spielstaende ab) */
	void SpeichereSpielstand(const FString& Name) const;
	/** Ende einer Welle nach den Belohnungen (UTDEinsatzSubsystem::WelleBeendet): Zeile in Wirtschaft.csv */
	void WellenEnde(int32 Welle, int32 EPVerdient, int32 FPVerdient, float KernAnteil);
	/** Ladenkauf (EP je Posten fuer die Auswertung) */
	void Kauf(const FString& Key, int32 EP);
	/** Eingriff, der den Lauf verfaelscht (/gib, /td fuellen, Testsprung ...) */
	void Eingriff(const FString& Was);
	/** /td notiz: Note 1 (zu leicht) bis 5 (zu schwer) und Stichwort zur laufenden bzw. letzten Welle */
	FString Notiz(int32 Note, const FString& Text);

	/** Beute beim Tod eines Wellengegners (aus dem Gegnerhirn) */
	static void Beute(AFGCreature* Kreatur);

protected:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
private:
	void WirfBeute(AFGCreature* Kreatur);
	/** Beute, die nach der Welle noch liegt, an den Kern bringen */
	void SammleBeute();
	int32 FPImInventar() const;
	void SchreibeZeile(const FString& Datei, const FString& Kopf, const FString& Zeile) const;
	void Melde(const FString& Text) const;

	double mWellenEndeZeit = -1.0;
	float mBauphase = -1.f;
	int32 mKontoNachLetzter = -1;
	int32 mFPNachLetzter = -1;
	TMap<FString, int32> mKaeufe;
	TArray<FString> mEingriffe;
	TSet<FString> mErforscht;
	bool mErforschtGelesen = false;
	TMap<FString, int32> mBeuteWelle;
	TArray<TWeakObjectPtr<AFGItemPickup_Spawnable>> mBeuteLiegt;
	FTimerHandle mSammelTimer;
	int32 mLetzteWelle = 0;
	bool mWelleLaeuft = false;
};
