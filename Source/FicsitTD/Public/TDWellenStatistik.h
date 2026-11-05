#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDWellenStatistik.generated.h"

/** 0.66 (Marius 04.11.2026: Transparenz je Welle): Werte einer Turmart in einer Welle */
USTRUCT()
struct FICSITTD_API FTDTurmStat
{
	GENERATED_BODY()
	/** Turmart (Anzeigename, z. B. Bolzenturm) */
	UPROPERTY(SaveGame)
	FString Art;
	UPROPERTY(SaveGame)
	int32 Anzahl = 0;
	/** wirksamer Schaden (ohne Overkill) */
	UPROPERTY(SaveGame)
	float Schaden = 0.f;
	UPROPERTY(SaveGame)
	int32 Schuesse = 0;
	UPROPERTY(SaveGame)
	int32 Kills = 0;
	/** Munitionsvorrat in den Tuermen zu Beginn und am Ende der Welle */
	UPROPERTY(SaveGame)
	int32 VorratStart = 0;
	UPROPERTY(SaveGame)
	int32 VorratEnde = 0;
	/** Soll-Produktion der Munition dieser Turmart in der Fabrik (je Minute) */
	UPROPERTY(SaveGame)
	float ProduktionMin = 0.f;
	/** Turm-Sekunden: Ziel da, aber keine Munition / kein Strom */
	UPROPERTY(SaveGame)
	float Hungrig = 0.f;
	UPROPERTY(SaveGame)
	float OhneStrom = 0.f;
};

/** Eine abgeschlossene Welle */
USTRUCT()
struct FICSITTD_API FTDWellenStat
{
	GENERATED_BODY()
	UPROPERTY(SaveGame)
	int32 Welle = 0;
	UPROPERTY(SaveGame)
	float Dauer = 0.f;
	UPROPERTY(SaveGame)
	int32 Gegner = 0;
	/** Kern in Prozent zu Beginn und am Ende */
	UPROPERTY(SaveGame)
	float KernStart = 0.f;
	UPROPERTY(SaveGame)
	float KernEnde = 0.f;
	UPROPERTY(SaveGame)
	int32 EP = 0;
	UPROPERTY(SaveGame)
	int32 FP = 0;
	/** Strom in MW: Kapazitaet, mittlerer und hoechster Verbrauch, Sekunden ueber der Kapazitaet bzw. Sicherung raus */
	UPROPERTY(SaveGame)
	float StromKapazitaet = 0.f;
	UPROPERTY(SaveGame)
	float StromMittel = 0.f;
	UPROPERTY(SaveGame)
	float StromSpitze = 0.f;
	UPROPERTY(SaveGame)
	float StromUeberlast = 0.f;
	UPROPERTY(SaveGame)
	TArray<FTDTurmStat> Tuerme;
};

/** Sammelt die Werte waehrend einer Welle und legt sie am Wellenende im Kern ab (Spielstand) */
UCLASS()
class FICSITTD_API UTDWellenStatistik : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	static UTDWellenStatistik* Get(const UObject* WorldContext);
	void WellenStart(int32 Welle);
	void WellenEnde(int32 Welle, int32 EP, int32 FP, float KernAnteil);
	/** Laufende Welle je Turmart (Schaden, Schuesse, Kills seit Wellenbeginn), nach Schaden absteigend */
	TArray<FTDTurmStat> Live() const;
	bool LaeuftWelle() const { return mLaeuft; }
	/** Alle gespeicherten Wellen (aelteste zuerst) */
	const TArray<FTDWellenStat>& Verlauf() const;
	/** 0.70 (Szenarien): Tuerme und Soll-Produktion je Turmart jetzt, Strom-Kapazitaet jetzt */
	TMap<FString, FTDTurmStat> Momentaufnahme() const;
	float StromKapazitaetJetzt() const;
	/** Farbe je Turmart fuer Balken */
	static FLinearColor Farbe(const FString& Art);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UTDWellenStatistik, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return mLaeuft; }

private:
	/** Turmart -> (Schaden, Schuesse, Kills, Anzahl, Vorrat, Hungrig, OhneStrom) der Tuerme jetzt */
	void Sammle(TMap<FString, FTDTurmStat>& Out) const;
	/** Soll-Produktion je Turmart (Munition, die ihre Rezepte verbrauchen) */
	void Produktion(TMap<FString, FTDTurmStat>& Out) const;
	void MesseStrom(float& Kapazitaet, float& Verbrauch, bool& bUeberlast) const;

	bool mLaeuft = false;
	int32 mWelle = 0;
	double mStart = 0.0;
	float mKernStart = 0.f;
	/** Turm -> Abschuesse zu Wellenbeginn */
	TMap<TWeakObjectPtr<class ATDTurretBase>, int32> mKillsStart;
	TMap<FString, int32> mVorratStart;
	float mTakt = 0.f;
	int32 mProben = 0;
	float mStromSumme = 0.f;
	float mStromSpitze = 0.f;
	float mStromKap = 0.f;
	float mUeberlast = 0.f;
};
