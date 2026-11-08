#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "FGClearanceData.h"
#include "TDDamageable.h"
#include "TDWellenStatistik.h"
#include "TDDefenseCore.generated.h"

class ATDDecoy;

/**
 * Abwehrkern (Spielkonzept S1): das Ziel der Wellen im Modus Kern-Verteidigung. Gebaeude mit Lebenspunkten und Koedern
 * (Blueprint = Kopie des Aussichtsturms, reines AFGBuildable). Faellt er auf 0, ist der Lauf verloren - das Gebaeude bleibt
 * stehen und wird beim naechsten Start wieder aufgeladen. Der Energiekristall dreht sich und pulsiert; bei Schaden flackert er.
 * Steuerung vorerst per Chat: /td kern start|weiter|stop|status.
 */
UCLASS()
class FICSITTD_API ATDDefenseCore : public AFGBuildable, public ITDDamageable
{
	GENERATED_BODY()
public:
	ATDDefenseCore();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetClearanceData_Implementation(TArray<FFGClearanceData>& out_data) const override;
	virtual FText GetLookAtDecription_Implementation(class AFGCharacterPlayer* byCharacter, const FUseState& state) const override;
	/** Im Einsatz oder laufender Kern-Verteidigung nicht abreissbar (Test 0.55) */
	virtual bool CanDismantle_Implementation() const override { return !mEinsatzAktiv && !mLaufAktiv && Super::CanDismantle_Implementation(); }

	// ITDDamageable
	virtual void ApplyTDDamage(float Amount, const AActor* Causer) override;
	virtual bool IsTDIntact() const override { return mHealth > 0.f; }
	virtual float GetTDHealth() const override { return mHealth; }
	/** mit Forschung Kern-Panzerung x1,3 */
	virtual float GetTDMaxHealth() const override;
	/** Reparatur: heilt bis zum Maximum, liefert die tatsaechlich geheilten Lebenspunkte */
	virtual float HealTD(float Amount) override
	{
		if (!IsTDIntact() || Amount <= 0.f) return 0.f;
		const float Alt = mHealth;
		mHealth = FMath::Min(GetTDMaxHealth(), mHealth + Amount);
		return mHealth - Alt;
	}

	/** Volle Lebenspunkte (Start eines Laufs) */
	void Aufladen();

	/** Gespeicherter Stand eines laufenden Durchgangs (das Wellen-System schreibt ihn laufend, nach dem Laden setzt es fort) */
	UPROPERTY(SaveGame)
	bool mLaufAktiv = false;
	UPROPERTY(SaveGame)
	int32 mLaufWelle = 0;
	UPROPERTY(SaveGame)
	int32 mLaufWellenGesamt = 30;
	UPROPERTY(SaveGame)
	float mLaufCountdown = 0.f;
	UPROPERTY(SaveGame)
	float mLaufZwischen = 600.f;
	UPROPERTY(SaveGame)
	int32 mLaufStufe = 1;
	/** Wellen nur auf Befehl (Einsatz: Planung ohne Zeitdruck) */
	UPROPERTY(SaveGame)
	bool mLaufManuell = false;
	/** Einsatz-Modus (TDEinsatz.h): aktiv, freies Bauen (Test), Startkit schon ausgegeben */
	UPROPERTY(SaveGame)
	bool mEinsatzAktiv = false;
	UPROPERTY(SaveGame)
	bool mEinsatzFrei = false;
	UPROPERTY(SaveGame)
	bool mEinsatzStartkit = false;
	/** 0.84 (Marius 08.11.: gleiche Kernplaetze ergeben gleiche Wellen): Zufallszahl je Einsatz fuer den Wellenplan,
	 *  0 = keine Variation (Standard, Balance Wellen.VariationJeEinsatz = 0) */
	UPROPERTY(SaveGame)
	int32 mLaufSaat = 0;
	/** Einsatzpunkte (Konto) und gekaufte Kontingente je Schluessel (Anzahl Kaeufe) */
	UPROPERTY(SaveGame)
	int32 mEinsatzPunkte = 0;
	UPROPERTY(SaveGame)
	TMap<FString, int32> mKontingentGekauft;
	/** Forschungspunkte (FICSIT-Kampfdaten), die nicht ins volle Inventar passten - werden spaeter nachgereicht */
	UPROPERTY(SaveGame)
	int32 mFPOffen = 0;
	/** 0.66: Statistik je abgeschlossener Welle (Kern-Menue, Reiter Statistik) */
	UPROPERTY(SaveGame)
	TArray<FTDWellenStat> mStatistik;
	/** Baumaterial aus Startkit/Kaeufen, das nicht ins volle Inventar passte (Item-Klassenpfad -> Anzahl) */
	UPROPERTY(SaveGame)
	TMap<FString, int32> mMaterialOffen;
	/** Balancing-Lauf (/td lauf balancing): Testsprung aus, Eingriffe werden vermerkt (UTDLaufProtokoll) */
	UPROPERTY(SaveGame)
	bool mBalancingLauf = false;
	/** Feste Fronten des Laufs (Ort, Richtung, Weglaenge; 0 = ungeprueft) */
	UPROPERTY(SaveGame)
	TArray<FVector> mLaufFrontOrte;
	UPROPERTY(SaveGame)
	TArray<float> mLaufFrontWinkel;
	UPROPERTY(SaveGame)
	TArray<float> mLaufFrontWeg;
	/** Erbe abgerissener Tuerme "Typ|Abschuesse|L<Level>" (UTDEinsatzSubsystem::MerkeTurmErbe, alt "Typ|K|R|F|S|Sp") */
	UPROPERTY(SaveGame)
	TArray<FString> mTurmErbe;
	/** Pionier-Rang (UTDPionier): Erfahrung und gelernte Talente je Zweig */
	UPROPERTY(SaveGame)
	int32 mPionierXP = 0;
	UPROPERTY(SaveGame)
	TMap<FString, int32> mTalente;
	/** Kern-Schild (Forschung KernSchild): Rest in dieser Welle, wird zum Wellenstart geladen */
	float mSchild = 0.f;
	void LadeSchild();

	/** Maximale LP je nach Schwierigkeit (wird mit dem Spielstand gesichert) */
	void SetzeMaxLP(float LP) { mMaxHealth = FMath::Max(1000.f, LP); }

	/** Wegvorschau ein-/ausschalten: Leuchtringe entlang der Wege von den Fronten zum Kern */
	void ZeigeWege(const TArray<TArray<FVector>>& Wege);
	void VersteckeWege();
	bool ZeigtWege() const { return mWegMarker.Num() > 0; }

protected:
	UPROPERTY(EditDefaultsOnly, SaveGame, Category = "Tower Defense")
	float mMaxHealth = 20000.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> mWegMarker;

	/** Feste Aufbauten (Saeulen, Kaefig, Mast) */
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mAufbau;

	/** Energiekristall (dreht sich) - seit 0.61 ohne Modell (Tiefbohrer) */
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mKristall;

	/** Tiefbohrer (0.61, Satisfactory\Modelle\TD_DefenseCore_Bohrer.blend): alle Teile im Ursprung des Gebaeudes modelliert */
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mBohrer;      // Stange und Schnecke: dreht, faehrt auf und ab
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mHub;         // Querhaupt und Kolben: faehrt mit, dreht nicht
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mZahnkranz;   // dreht gegenlaeufig
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mLeuchtring;  // Zustand: Farbe nach Kern-LP

private:
	void Animieren();
	/** Kern-Geschuetz (Forschung KernGeschuetz): alle 1,5 s ein Bolzen auf den naechsten Gegner */
	void Geschuetz();
	FTimerHandle mGeschuetzTimer;

	UPROPERTY(SaveGame)
	float mHealth = -1.f;

	UPROPERTY()
	TArray<TObjectPtr<ATDDecoy>> mDecoys;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> mGluehen;

	FTimerHandle mAnimTimer;
	float mZeit = 0.f;
	float mTrefferBlitz = 0.f;
	/** Hoehe des Bohrers ueber der Bohrstellung (cm): in der Bauphase oben, in der Welle unten */
	float mBohrHoehe = 150.f;
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> mRingMaterial;
};
