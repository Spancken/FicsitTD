#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDPionier.generated.h"

class AFGCreature;
class ATDDefenseCore;

/** Ein Zweig des Talentbaums "Pionier": 5 Stufen, Waffen-Zweige brauchen die Waffe aus dem Laden */
struct FTDTalentZweig
{
	const TCHAR* Key;
	const TCHAR* Name;
	/** Laden-Posten, der gekauft sein muss (nullptr = keiner) */
	const TCHAR* Braucht;
	const TCHAR* Stufen[5];
};

/**
 * Pionier-Rang und Talente (Konzept-Forschung-und-Pionier.md, 0.58): XP aus abgewehrten Wellen und eigenen Abschuessen,
 * je Rang ein Talentpunkt. Gilt fuer den laufenden Einsatz, gespeichert am Kern (mPionierXP, mTalente).
 * Wirkt auf den Spieler (LP, Regeneration, Tempo, Nachladen) und ueber die Schadens-Hooks im Modul auf Waffenschaden und Ruestung.
 */
UCLASS()
class FICSITTD_API UTDPionier : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UTDPionier* Get(const UObject* WorldContext);
	static const TArray<FTDTalentZweig>& Zweige();
	static constexpr int32 MaxStufe = 5;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	int32 GetXP() const;
	int32 GetRang() const;
	/** XP, ab der ein Rang erreicht ist (Rang 1 = 0) */
	static int32 XPFuerRang(int32 Rang);
	static int32 MaxRang();
	int32 GetStufe(int32 Zweig) const;
	int32 FreiePunkte() const;
	/** Fehlt die Waffe fuer den Zweig? Dann der Ladenname, sonst leer */
	FString FehltFuer(int32 Zweig) const;

	/** Talent lernen bzw. alles neu verteilen; Rueckgabe = Meldung */
	FString Lerne(int32 Zweig);
	FString NeuVerteilen();
	static int32 NeuVerteilenPreis();

	/** Abgewehrte Welle (aus der EP-Abrechnung) und Wellenbeginn (Zweite Chance wieder bereit) */
	void WelleBeendet(int32 Welle, float KernAnteil);
	void WellenStart();
	/** Tod eines Wellengegners: XP, wenn der Spieler ihn zuletzt getroffen hat */
	void Abschuss(const AFGCreature* Kreatur);

	/** Aus den Schadens-Hooks: Faktor auf den Schaden. Merkt Spielertreffer (XP) und loest die Abschluss-Talente aus. */
	static float Schadensfaktor(AActor* Getroffen, AController* Ausloeser, AActor* Verursacher, float Menge, bool bWellenKreatur, bool bVonKreatur);

	/** 0.9.6 P0 (Analyse-Spieler-und-Pionier 12.11.): was der Spieler in der laufenden Welle bewirkt hat */
	struct FSpielerBilanz
	{
		float Schaden = 0.f;
		int32 Treffer = 0;
		int32 Abschuesse = 0;
		int32 Eliten = 0;
		int32 Bosse = 0;
		int32 XP = 0;
	};
	const FSpielerBilanz& GetWellenBilanz() const { return mBilanz; }
	/** Zeile fuer die Wellenmeldung ("Du: 1240 Schaden (11 %), 9 Abschuesse ..."); leer, wenn der Spieler nichts getroffen hat */
	FString WellenZeile(float TurmSchaden) const;

private:
	/** 0.9.6 P1: Handwaffen nutzen die Munitionswerte der Tuerme - Faktor = Tabellenwert x HandFaktor x Wellenzuwachs x Resistenz / Spielwert */
	float HandFaktor(int32 W, const AActor* Verursacher, const AActor* Getroffen, FString& OutMunition, float& OutSkalierung, float& OutResistenz) const;
	FSpielerBilanz mBilanz;
	TSet<FString> mGemeldeteTreffer;
	ATDDefenseCore* Kern() const;
	void GibXP(int32 XP, const FString& Grund);
	void Melde(const FString& Text) const;
	/** alle 0,5 s: Spieler-Werte setzen */
	void Wirke();
	/** Waffe des Verursachers: 0 Armierpistole, 1 Gewehr, 2 Nobelisk, -1 keine */
	int32 Waffe(const AActor* Verursacher);
	float SpielerTreffer(AActor* Getroffen, AActor* Verursacher, float Menge);

	FTimerHandle mTimer;
	TMap<TWeakObjectPtr<const AActor>, double> mSpielerTreffer;
	TSet<FString> mGemeldeteKlassen;
	int32 mGewehrTreffer = 0;
	bool mZweiteChanceGenutzt = false;
};
